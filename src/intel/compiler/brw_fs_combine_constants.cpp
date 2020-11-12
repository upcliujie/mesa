/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** @file brw_fs_combine_constants.cpp
 *
 * This file contains the opt_combine_constants() pass that runs after the
 * regular optimization loop. It passes over the instruction list and
 * selectively promotes immediate values to registers by emitting a mov(1)
 * instruction.
 *
 * This is useful on Gen 7 particularly, because a few instructions can be
 * coissued (i.e., issued in the same cycle as another thread on the same EU
 * issues an instruction) under some circumstances, one of which is that they
 * cannot use immediate values.
 */

#include "brw_fs.h"
#include "brw_cfg.h"
#include "util/half_float.h"
#include "util/combine_constants.h"

using namespace brw;

static const bool debug = false;

/* Returns whether an instruction could co-issue if its immediate source were
 * replaced with a GRF source.
 */
static bool
could_coissue(const struct gen_device_info *devinfo, const fs_inst *inst)
{
   assert(inst->opcode == BRW_OPCODE_MOV ||
          inst->opcode == BRW_OPCODE_CMP ||
          inst->opcode == BRW_OPCODE_ADD ||
          inst->opcode == BRW_OPCODE_MUL);

   if (devinfo->gen != 7)
      return false;

   /* Only float instructions can coissue.  We don't have a great
    * understanding of whether or not something like float(int(a) + int(b))
    * would be considered float (based on the destination type) or integer
    * (based on the source types), so we take the conservative choice of
    * only promoting when both destination and source are float.
    */
      return inst->dst.type == BRW_REGISTER_TYPE_F &&
             inst->src[0].type == BRW_REGISTER_TYPE_F;
}

/**
 * Box for storing fs_inst and some other necessary data
 *
 * The \c fs_inst_box is used as the \c abstract_instruction for
 * \c util_combine_constants.
 *
 * \sa box_instruction
 */
struct fs_inst_box {
   fs_inst *inst;
   unsigned ip;
   bblock_t *block;
   bool must_promote;
};

/** A box for putting fs_regs in a linked list. */
struct reg_link {
   DECLARE_RALLOC_CXX_OPERATORS(reg_link)

   reg_link(fs_inst *inst, unsigned src, bool negate, enum interpreted_type type)
   : inst(inst), src(src), negate(negate), type(type) {}

   struct exec_node link;
   fs_inst *inst;
   uint8_t src;
   bool negate;
   enum interpreted_type type;
};

static struct exec_node *
link(void *mem_ctx, fs_inst *inst, unsigned src, bool negate,
     enum interpreted_type type)
{
   reg_link *l = new(mem_ctx) reg_link(inst, src, negate, type);
   return &l->link;
}

/**
 * Information about an immediate value.
 */
struct imm {
   /** The common ancestor of all blocks using this immediate value. */
   bblock_t *block;

   /**
    * The instruction generating the immediate value, if all uses are contained
    * within a single basic block. Otherwise, NULL.
    */
   fs_inst *inst;

   /**
    * A list of fs_regs that refer to this immediate.  If we promote it, we'll
    * have to patch these up to refer to the new GRF.
    */
   exec_list *uses;

   /** The immediate value */
   union {
      char bytes[8];
      double df;
      int64_t d64;
      float f;
      int32_t d;
      int16_t w;
   };
   uint8_t size;

   /** When promoting half-float we need to account for certain restrictions */
   bool is_half_float;

   /**
    * The GRF register and subregister number where we've decided to store the
    * constant value.
    */
   uint8_t subreg_offset;
   uint16_t nr;

   /** The number of coissuable instructions using this immediate. */
   uint16_t uses_by_coissue;

   /**
    * Whether this constant is used by an instruction that can't handle an
    * immediate source (and already has to be promoted to a GRF).
    */
   bool must_promote;

   uint16_t first_use_ip;
   uint16_t last_use_ip;
};

/** The working set of information about immediates. */
struct table {
   struct value *values;
   int size;
   int num_values;

   struct imm *imm;
   int len;

   struct fs_inst_box *boxes;
   unsigned num_boxes;
   unsigned size_boxes;
};

static struct value *
new_value(struct table *table, void *mem_ctx)
{
   if (table->num_values == table->size) {
      table->size *= 2;
      table->values = reralloc(mem_ctx, table->values, struct value, table->size);
   }
   return &table->values[table->num_values++];
}

/**
 * Store an instruction with some other data in a table.
 *
 * \returns the index into the dynamic array of boxes for the instruction.
 */
static unsigned
box_instruction(struct table *table, void *mem_ctx, fs_inst *inst,
                unsigned ip, bblock_t *block, bool must_promote)
{
   /* It is common for box_instruction to be called consecutively for each
    * source of an instruction.  As a result, the most common case for finding
    * an instruction in the table is when that instruction was the last one
    * added.  Search the list back to front.
    */
   for (unsigned i = table->num_boxes; i > 0; /* empty */) {
      i--;

      if (table->boxes[i].inst == inst)
         return i;
   }

   if (table->num_boxes == table->size_boxes) {
      table->size_boxes *= 2;
      table->boxes = reralloc(mem_ctx, table->boxes, fs_inst_box,
                              table->size_boxes);
   }

   assert(table->num_boxes < table->size_boxes);

   const unsigned idx = table->num_boxes++;
   fs_inst_box *ib =  &table->boxes[idx];

   ib->inst = inst;
   ib->block = block;
   ib->ip = ip;
   ib->must_promote = must_promote;

   return idx;
}

/**
 * Comparator used for sorting an array of imm structures.
 *
 * We sort by basic block number, then last use IP, then first use IP (least
 * to greatest). This sorting causes immediates live in the same area to be
 * allocated to the same register in the hopes that all values will be dead
 * about the same time and the register can be reused.
 */
static int
compare(const void *_a, const void *_b)
{
   const struct imm *a = (const struct imm *)_a,
                    *b = (const struct imm *)_b;

   int block_diff = a->block->num - b->block->num;
   if (block_diff)
      return block_diff;

   int end_diff = a->last_use_ip - b->last_use_ip;
   if (end_diff)
      return end_diff;

   return a->first_use_ip - b->first_use_ip;
}

static struct brw_reg
build_imm_reg_for_copy(struct imm *imm)
{
   switch (imm->size) {
   case 8:
      return brw_imm_d(imm->d64);
   case 4:
      return brw_imm_d(imm->d);
   case 2:
      return brw_imm_w(imm->w);
   default:
      unreachable("not implemented");
   }
}

static inline uint32_t
get_alignment_for_imm(const struct imm *imm)
{
   if (imm->is_half_float)
      return 4; /* At least MAD seems to require this */
   else
      return imm->size;
}

static bool
representable_as_hf(float f, uint16_t *hf)
{
   union fi u;
   uint16_t h = _mesa_float_to_half(f);
   u.f = _mesa_half_to_float(h);

   if (u.f == f) {
      *hf = h;
      return true;
   }

   return false;
}

static bool
represent_src_as_imm(const struct gen_device_info *devinfo,
                     fs_reg *src)
{
   /* TODO : consider specific platforms also */
   if (devinfo->gen == 12) {
      uint16_t hf;
      if (representable_as_hf(src->f, &hf)) {
         *src = retype(brw_imm_uw(hf), BRW_REGISTER_TYPE_HF);
         return true;
      }
   }
   return false;
}

static void
add_candidate_immediate(struct table *table, fs_inst *inst, unsigned ip,
                        unsigned i,
                        bool must_promote,
                        bool allow_one_constant,
                        bblock_t *block,
                        const struct gen_device_info *devinfo,
                        void *const_ctx)
{
   struct value *v = new_value(table, const_ctx);

   unsigned box_idx = box_instruction(table, const_ctx, inst, ip, block,
                                      must_promote);

   v->value.u64 = inst->src[i].d64;
   v->bit_size = 8 * type_sz(inst->src[i].type);
   v->instr = (struct abstract_instruction *)(uintptr_t) box_idx;
   v->src = i;
   v->allow_one_constant = allow_one_constant;

   /* Right-shift instructions are special.  They can have source modifiers,
    * but changing the type can change the semantic of the instruction.  Only
    * allow negations on a right shift if the source type is already signed.
    */
   v->no_negations = !inst->can_do_source_mods(devinfo) ||
                     ((inst->opcode == BRW_OPCODE_SHR ||
                       inst->opcode == BRW_OPCODE_ASR) &&
                      type_is_unsigned_int(inst->src[i].type));

   switch (inst->src[i].type) {
   case BRW_REGISTER_TYPE_DF:
   case BRW_REGISTER_TYPE_NF:
   case BRW_REGISTER_TYPE_F:
   case BRW_REGISTER_TYPE_HF:
      v->type = float_only;
      break;

   case BRW_REGISTER_TYPE_UQ:
   case BRW_REGISTER_TYPE_Q:
   case BRW_REGISTER_TYPE_UD:
   case BRW_REGISTER_TYPE_D:
   case BRW_REGISTER_TYPE_UW:
   case BRW_REGISTER_TYPE_W:
      v->type = integer_only;
      break;

   case BRW_REGISTER_TYPE_VF:
   case BRW_REGISTER_TYPE_UV:
   case BRW_REGISTER_TYPE_V:
   case BRW_REGISTER_TYPE_UB:
   case BRW_REGISTER_TYPE_B:
   default:
      unreachable("not reached");
   }

   /* It is safe to change the type of the operands of a select instruction
    * that has no conditional modifier, no source modifiers, and no saturate
    * modifer.
    */
   if (inst->opcode == BRW_OPCODE_SEL &&
       inst->conditional_mod == BRW_CONDITIONAL_NONE &&
       !inst->src[0].negate && !inst->src[0].abs &&
       !inst->src[1].negate && !inst->src[1].abs &&
       !inst->saturate) {
      v->type = either_type;
   }
}

bool
fs_visitor::opt_combine_constants()
{
   void *const_ctx = ralloc_context(NULL);

   struct table table;

   /* For each of the dynamic arrays in the table, allocate about a page of
    * memory.  On LP64 systems, this gives 126 value objects 169 fs_inst_box
    * objects.  Even larger shaders that have been obverved rarely need more
    * than 20 or 30 values.  Most smaller shaders, which is most shaders, need
    * at most a couple dozen fs_inst_box.
    */
   table.size = (4096 - (5 * sizeof(void *))) / sizeof(struct value);
   table.num_values = 0;
   table.values = ralloc_array(const_ctx, struct value, table.size);

   table.size_boxes = (4096 - (5 * sizeof(void *))) / sizeof(struct fs_inst_box);
   table.num_boxes = 0;
   table.boxes = ralloc_array(const_ctx, fs_inst_box, table.size_boxes);

   const brw::idom_tree &idom = idom_analysis.require();
   unsigned ip = -1;

   /* Make a pass through all instructions and count the number of times each
    * constant is used by coissueable instructions or instructions that cannot
    * take immediate arguments.
    */
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      ip++;

      switch (inst->opcode) {
      case SHADER_OPCODE_INT_QUOTIENT:
      case SHADER_OPCODE_INT_REMAINDER:
      case SHADER_OPCODE_POW:
         if (inst->src[0].file == IMM) {
            assert(inst->opcode != SHADER_OPCODE_POW);

            add_candidate_immediate(&table, inst, ip, 0, true, false, block,
                                    devinfo, const_ctx);
         }

         if (inst->src[1].file == IMM && devinfo->gen < 8) {
            add_candidate_immediate(&table, inst, ip, 1, true, false, block,
                                    devinfo, const_ctx);
         }

         break;

      case BRW_OPCODE_MAD: {
         bool represented_as_imm = false;
         for (int i = 0; i < inst->sources; i++) {
            if (inst->src[i].file != IMM)
               continue;

            if (!represented_as_imm && i == 0 &&
                represent_src_as_imm(devinfo, &inst->src[i])) {
               represented_as_imm = true;
               continue;
            }

            add_candidate_immediate(&table, inst, ip, i, true, false, block,
                                    devinfo, const_ctx);
         }

         break;
      }

      case BRW_OPCODE_BFE:
      case BRW_OPCODE_BFI2:
      case BRW_OPCODE_LRP:
         for (int i = 0; i < inst->sources; i++) {
            if (inst->src[i].file != IMM)
               continue;

            add_candidate_immediate(&table, inst, ip, i, true, false, block,
                                    devinfo, const_ctx);
         }

         break;

      case BRW_OPCODE_SEL:
         if (inst->src[0].file == IMM) {
            /* It is possible to have src0 be immediate but src1 not be
             * immediate for the non-commutative conditional modifiers (e.g.,
             * G).
             */
            if (inst->conditional_mod == BRW_CONDITIONAL_NONE ||
                /* Only GE and L are commutative. */
                inst->conditional_mod == BRW_CONDITIONAL_GE ||
                inst->conditional_mod == BRW_CONDITIONAL_L) {
               assert(inst->src[1].file == IMM);

               add_candidate_immediate(&table, inst, ip, 0, true, true, block,
                                       devinfo, const_ctx);
               add_candidate_immediate(&table, inst, ip, 1, true, true, block,
                                       devinfo, const_ctx);
            } else {
               add_candidate_immediate(&table, inst, ip, 0, true, false, block,
                                       devinfo, const_ctx);
            }
         }
         break;

      case BRW_OPCODE_ASR:
      case BRW_OPCODE_BFI1:
      case BRW_OPCODE_ROL:
      case BRW_OPCODE_ROR:
      case BRW_OPCODE_SHL:
      case BRW_OPCODE_SHR:
         if (inst->src[0].file == IMM) {
            add_candidate_immediate(&table, inst, ip, 0, true, false, block,
                                    devinfo, const_ctx);
         }
         break;

      case BRW_OPCODE_MOV:
         if (could_coissue(devinfo, inst) && inst->src[0].file == IMM) {
            add_candidate_immediate(&table, inst, ip, 0, false, false, block,
                                    devinfo, const_ctx);
         }
         break;

      case BRW_OPCODE_CMP:
      case BRW_OPCODE_ADD:
      case BRW_OPCODE_MUL:
         assert(inst->src[0].file != IMM);

         if (could_coissue(devinfo, inst) && inst->src[1].file == IMM) {
            add_candidate_immediate(&table, inst, ip, 1, false, false, block,
                                    devinfo, const_ctx);
         }
         break;

      default:
         break;
      }
   }

   if (table.num_values == 0) {
      ralloc_free(const_ctx);
      return false;
   }

   struct combine_constants_result *result =
      util_combine_constants(table.values, table.num_values);

   table.imm = ralloc_array(const_ctx, struct imm, result->num_values_to_emit);
   table.len = 0;

   for (unsigned i = 0; i < result->num_values_to_emit; i++) {
      struct imm *imm = &table.imm[table.len];

      imm->block = NULL;
      imm->inst = NULL;
      imm->d64 = result->values_to_emit[i].value.u64;
      imm->size = result->values_to_emit[i].bit_size / 8;

      imm->uses_by_coissue = 0;
      imm->must_promote = false;
      imm->is_half_float = false;

      imm->first_use_ip = UINT16_MAX;
      imm->last_use_ip = 0;

      imm->uses = new(const_ctx) exec_list;

      const unsigned first_user = result->values_to_emit[i].first_user;
      const unsigned last_user = first_user +
         result->values_to_emit[i].num_users;

      for (unsigned j = first_user; j < last_user; j++) {
         const unsigned idx =
            (unsigned)(uintptr_t) table.values[result->user_map[j].index].instr;
         fs_inst_box *const ib = &table.boxes[idx];

         const unsigned src = table.values[result->user_map[j].index].src;

         imm->uses->push_tail(link(const_ctx, ib->inst, src,
                                   result->user_map[j].negate,
                                   result->user_map[j].type));

         if (ib->must_promote)
            imm->must_promote = true;
         else
            imm->uses_by_coissue++;

         if (imm->block == NULL) {
            /* Block should only be NULL on the first pass.  On the first
             * pass, inst should also be NULL.
             */
            assert(imm->inst == NULL);

            imm->inst = ib->inst;
            imm->block = ib->block;
            imm->first_use_ip = ib->ip;
            imm->last_use_ip = ib->ip;
         } else {
            bblock_t *intersection = idom.intersect(ib->block,
                                                    imm->block);

            if (imm->first_use_ip > ib->ip) {
               imm->first_use_ip = ib->ip;

               /* If the first-use instruction is to be tracked, block must be
                * the block that contains it.  The old block was read in the
                * idom.intersect call above, so it is safe to overwrite it
                * here.
                */
               imm->inst = ib->inst;
               imm->block = ib->block;
            }

            if (imm->last_use_ip < ib->ip)
               imm->last_use_ip = ib->ip;

            /* The common dominator is not the block that contains the
             * first-use instruction, so don't track that instruction.  The
             * load instruction will be added in the common dominator block
             * instead of before the first-use instruction.
             */
            if (intersection != imm->block)
               imm->inst = NULL;

            imm->block = intersection;
         }

         if (ib->inst->src[src].type == BRW_REGISTER_TYPE_HF)
            imm->is_half_float = true;
      }

      /* Remove constants from the table that don't have enough uses to make
       * them profitable to store in a register.
       */
      if (imm->must_promote || imm->uses_by_coissue >= 4)
         table.len++;
   }

   util_combine_constants_result_dtor(result);

   if (table.len == 0) {
      ralloc_free(const_ctx);
      return false;
   }
   if (cfg->num_blocks != 1)
      qsort(table.imm, table.len, sizeof(struct imm), compare);

   /* Insert MOVs to load the constant values into GRFs. */
   fs_reg reg(VGRF, alloc.allocate(1));
   reg.stride = 0;
   for (int i = 0; i < table.len; i++) {
      struct imm *imm = &table.imm[i];
      /* Insert it either before the instruction that generated the immediate
       * or after the last non-control flow instruction of the common ancestor.
       */
      exec_node *n = (imm->inst ? imm->inst :
                      imm->block->last_non_control_flow_inst()->next);

      /* From the BDW and CHV PRM, 3D Media GPGPU, Special Restrictions:
       *
       *   "In Align16 mode, the channel selects and channel enables apply to a
       *    pair of half-floats, because these parameters are defined for DWord
       *    elements ONLY. This is applicable when both source and destination
       *    are half-floats."
       *
       * This means that Align16 instructions that use promoted HF immediates
       * and use a <0,1,0>:HF region would read 2 HF slots instead of
       * replicating the single one we want. To avoid this, we always populate
       * both HF slots within a DWord with the constant.
       */
      const uint32_t width = devinfo->gen == 8 && imm->is_half_float ? 2 : 1;
      const fs_builder ibld = bld.at(imm->block, n).exec_all().group(width, 0);

      /* Put the immediate in an offset aligned to its size. Some instructions
       * seem to have additional alignment requirements, so account for that
       * too.
       */
      reg.offset = ALIGN(reg.offset, get_alignment_for_imm(imm));

      /* Ensure we have enough space in the register to copy the immediate */
      struct brw_reg imm_reg = build_imm_reg_for_copy(imm);
      if (reg.offset + type_sz(imm_reg.type) * width > REG_SIZE) {
         reg.nr = alloc.allocate(1);
         reg.offset = 0;
      }

      ibld.MOV(retype(reg, imm_reg.type), imm_reg);
      imm->nr = reg.nr;
      imm->subreg_offset = reg.offset;

      reg.offset += imm->size * width;
   }
   shader_stats.promoted_constants = table.len;

   /* Rewrite the immediate sources to refer to the new GRFs. */
   for (int i = 0; i < table.len; i++) {
      foreach_list_typed(reg_link, link, link, table.imm[i].uses) {
         fs_reg *reg = &link->inst->src[link->src];

         if (link->inst->opcode == BRW_OPCODE_SEL) {
            if (link->type == either_type) {
               /* Do not change the register type. */
            } else if (link->type == integer_only) {
               reg->type = brw_int_type(type_sz(reg->type), true);
            } else {
               assert(link->type == float_only);

               switch (type_sz(reg->type)) {
               case 2:
                  reg->type = BRW_REGISTER_TYPE_HF;
                  break;
               case 4:
                  reg->type = BRW_REGISTER_TYPE_F;
                  break;
               case 8:
                  reg->type = BRW_REGISTER_TYPE_DF;
                  break;
               default:
                  unreachable("Bad type size");
               }
            }
         } else if ((link->inst->opcode == BRW_OPCODE_SHL ||
                     link->inst->opcode == BRW_OPCODE_ASR) &&
                    link->negate) {
            reg->type = brw_int_type(type_sz(reg->type), true);
         }

#ifdef DEBUG
         switch (reg->type) {
         case BRW_REGISTER_TYPE_DF:
            assert((isnan(reg->df) && isnan(table.imm[i].df)) ||
                   (fabs(reg->df) == fabs(table.imm[i].df)));
            break;
         case BRW_REGISTER_TYPE_F:
            assert((isnan(reg->f) && isnan(table.imm[i].f)) ||
                   (fabsf(reg->f) == fabsf(table.imm[i].f)));
            break;
         case BRW_REGISTER_TYPE_HF:
            assert((isnan(_mesa_half_to_float(reg->d & 0xffffu)) &&
                    isnan(_mesa_half_to_float(table.imm[i].w))) ||
                   (fabsf(_mesa_half_to_float(reg->d & 0xffffu)) ==
                    fabsf(_mesa_half_to_float(table.imm[i].w))));
            break;
         case BRW_REGISTER_TYPE_Q:
            assert(abs(reg->d64) == abs(table.imm[i].d64));
            break;
         case BRW_REGISTER_TYPE_UQ:
            assert(!link->negate);
            assert(reg->d64 == table.imm[i].d64);
            break;
         case BRW_REGISTER_TYPE_D:
            assert(abs(reg->d) == abs(table.imm[i].d));
            break;
         case BRW_REGISTER_TYPE_UD:
            assert(!link->negate);
            assert(reg->d == table.imm[i].d);
            break;
         case BRW_REGISTER_TYPE_W:
            assert(abs((int16_t) (reg->d & 0xffff)) == table.imm[i].w);
            break;
         case BRW_REGISTER_TYPE_UW:
            assert(!link->negate);
            assert((reg->ud & 0xffffu) == (uint16_t) table.imm[i].w);
            break;
         default:
            break;
         }
#endif

         assert(link->inst->can_do_source_mods(devinfo) || !link->negate);

         reg->file = VGRF;
         reg->offset = table.imm[i].subreg_offset;
         reg->stride = 0;
         reg->negate = link->negate;
         reg->nr = table.imm[i].nr;
      }
   }

   /* Fixup any SEL instructions that have src0 still as an immediate.  Fixup
    * the types of any SEL instruction that have a negation on one of the
    * sources.  Adding the negation may have changed the type of that source,
    * so the other source (and destination) must be changed to match.
    */
   for (unsigned i = 0; i < table.num_boxes; i++) {
      fs_inst *inst = table.boxes[i].inst;

      if (inst->opcode != BRW_OPCODE_SEL)
         continue;

      /* If both sources have negation, the types had better be the same! */
      assert(!inst->src[0].negate || !inst->src[1].negate ||
             inst->src[0].type == inst->src[1].type);

      /* If either source has a negation, force the type of the other source
       * and the type of the result to be the same.
       */
      if (inst->src[0].negate) {
         inst->src[1].type = inst->src[0].type;
         inst->dst.type = inst->src[0].type;
      }

      if (inst->src[1].negate) {
         inst->src[0].type = inst->src[1].type;
         inst->dst.type = inst->src[1].type;
      }

      if (inst->src[0].file != IMM)
         continue;

      assert(inst->src[1].file != IMM);
      assert(inst->conditional_mod == BRW_CONDITIONAL_NONE ||
             inst->conditional_mod == BRW_CONDITIONAL_GE ||
             inst->conditional_mod == BRW_CONDITIONAL_L);

      fs_reg temp = inst->src[0];
      inst->src[0] = inst->src[1];
      inst->src[1] = temp;

      /* If this was predicated, flipping operands means we also need to flip
       * the predicate.
       */
      if (inst->conditional_mod == BRW_CONDITIONAL_NONE)
         inst->predicate_inverse = !inst->predicate_inverse;
   }

   if (debug) {
      for (int i = 0; i < table.len; i++) {
         struct imm *imm = &table.imm[i];

         fprintf(stderr,
                 "0x%016" PRIx64 " - block %3d, reg %3d sub %2d, "
                 "Uses: (%2d, %2d), IP: %4d to %4d, length %4d\n",
                 (uint64_t)(imm->d & BITFIELD64_MASK(imm->size * 8)),
                 imm->block->num,
                 imm->nr,
                 imm->subreg_offset,
                 imm->must_promote,
                 imm->uses_by_coissue,
                 imm->first_use_ip,
                 imm->last_use_ip,
                 imm->last_use_ip - imm->first_use_ip);
      }
   }

   ralloc_free(const_ctx);
   invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return true;
}
