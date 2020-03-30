/*
 * Copyright © 2018 Intel Corporation
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

#include "nir_builder.h"

/**
 * Some ALU operations may not be supported in hardware in specific bit-sizes.
 * This pass allows implementations to selectively lower such operations to
 * a bit-size that is supported natively and then converts the result back to
 * the original bit-size.
 *
 * When lowering integer arithmetic, this pass uses undef_extend32 and keeps
 * track of whether the result of a lowered instruction is sign or zero extended
 * to avoid creating conversion code.
 */

typedef struct {
   union {
      struct {
         /* old bit size minus 1. */
         uint8_t old_size:5;

         /* Whether the result of this instruction is zero/sign extended from
          * old_size to new_size after lowering.
          */
         bool zext:1;
         bool sext:1;

         bool is_output_float:1;
         bool is_output_unsized:1;

         /* new bit size minus 1. */
         uint8_t new_size:6;
      };
      uint16_t v;
   };
} pass_flags;

/* Returns whether this source needs the upper bits to be valid for the lower
 * bits to be valid when lowering.
 */
static bool
care_about_upper_bits(const nir_alu_instr *alu, unsigned src)
{
   /* Upcasts of constants are free, so don't undef_extend32 them. */
   if (alu->src[src].src.ssa->parent_instr->type == nir_instr_type_load_const)
      return true;

   switch (alu->op) {
   case nir_op_iadd:
   case nir_op_isub:
   case nir_op_ineg:
   case nir_op_iand:
   case nir_op_ior:
   case nir_op_ixor:
   case nir_op_inot:
   case nir_op_ishl:
   case nir_op_bcsel:
   case nir_op_b8csel:
   case nir_op_b16csel:
   case nir_op_b32csel:
   case nir_op_imul:
   case nir_op_extract_u8:
   case nir_op_extract_i8:
   case nir_op_extract_u16:
   case nir_op_extract_i16:
   case nir_op_bitfield_select:
      return false;
   default:
      return true;
   }
}

static void
propagate_upper_bits_instr(nir_alu_instr *alu)
{
   if (!alu->instr.pass_flags)
      return;

   pass_flags flags = {.v=alu->instr.pass_flags};

   bool src_zext[4] = { false, false, false, false };
   bool src_sext[4] = { false, false, false, false };
   for (unsigned i = 0; i < nir_op_infos[alu->op].num_inputs; i++) {
      nir_ssa_def *src = alu->src[i].src.ssa;
      nir_alu_type type = nir_op_infos[alu->op].input_types[i];
      if (nir_alu_type_get_type_size(type))
         continue;

      if (care_about_upper_bits(alu, i)) {
         /* We're going to have to sign/zero-extend this source if it's not
          * already.
          */
         nir_alu_type base = nir_alu_type_get_base_type(type);
         src_zext[i] = base == nir_type_uint;
         src_sext[i] = base == nir_type_int;
      } else if (src->parent_instr->pass_flags) {
         /* update_uses() will pass the lowered result to this instruction
          * without conversion.
          */
         pass_flags op_flags = {.v=src->parent_instr->pass_flags};
         if (op_flags.new_size == flags.new_size &&
             op_flags.old_size == flags.old_size &&
             op_flags.is_output_unsized) {
            src_zext[i] = op_flags.zext;
            src_sext[i] = op_flags.sext;
         }
      }
   }

   switch (alu->op) {
   case nir_op_iand:
      flags.zext = src_zext[0] || src_zext[1];
      flags.sext = src_sext[0] && src_sext[1];
      break;
   case nir_op_ior:
   case nir_op_ixor:
      flags.zext = src_zext[0] && src_zext[1];
      flags.sext = src_sext[0] && src_sext[1];
      break;
   case nir_op_bcsel:
   case nir_op_b8csel:
   case nir_op_b16csel:
   case nir_op_b32csel:
      flags.zext = src_zext[1] && src_zext[2];
      flags.sext = src_sext[1] && src_sext[2];
      break;
   case nir_op_ushr:
   case nir_op_umul_high:
   case nir_op_extract_u8:
   case nir_op_extract_u16:
   case nir_op_udiv:
   case nir_op_umod:
      flags.zext = true;
      break;
   case nir_op_ishr:
   case nir_op_imul_high:
   case nir_op_extract_i8:
   case nir_op_extract_i16:
   case nir_op_idiv:
   case nir_op_imod:
   case nir_op_irem:
      flags.sext = true;
      break;
   case nir_op_iadd:
   case nir_op_isub:
   case nir_op_imul:
      flags.zext = alu->no_unsigned_wrap && src_zext[0] && src_zext[1];
      flags.sext = alu->no_signed_wrap && src_sext[0] && src_sext[1];
      break;
   case nir_op_ishl:
      flags.zext = alu->no_unsigned_wrap && src_zext[0];
      flags.sext = alu->no_signed_wrap && src_sext[0];
      break;
   case nir_op_ineg:
      flags.sext = alu->no_signed_wrap && src_sext[0];
      break;
   default:
      break;
   }

   alu->instr.pass_flags = flags.v;
}

static bool
propagate_upper_bits_phi(nir_phi_instr *phi)
{
   /* Lower phis if all sources are lowered in the same way. This helps
    * propagate zext/sext information and eliminate later upcasts.
    */

   if (list_is_empty(&phi->dest.ssa.uses))
      return false;

   unsigned new_bit_size = 0;
   bool is_float = 0;
   bool all_zext = true;
   bool all_sext = true;
   nir_foreach_phi_src(src, phi) {
      nir_instr *instr = src->src.ssa->parent_instr;
      if (!instr->pass_flags)
         return false;

      pass_flags flags = {.v=instr->pass_flags};
      if ((new_bit_size && (flags.new_size != new_bit_size ||
                            flags.is_output_float != is_float)) ||
          flags.is_output_unsized)
         return false;

      all_zext = all_zext && flags.zext;
      all_sext = all_sext && flags.sext;
      new_bit_size = flags.new_size;
      is_float = flags.is_output_float;
   }

   pass_flags flags;
   flags.zext = all_zext;
   flags.sext = all_sext;
   flags.new_size = new_bit_size;
   flags.old_size = phi->dest.ssa.bit_size - 1;
   flags.is_output_unsized = true;
   flags.is_output_float = is_float;
   bool progress = flags.v != phi->instr.pass_flags;
   phi->instr.pass_flags = flags.v;

   return progress;
}

static bool
propagate_upper_bits(struct exec_list *list)
{
   bool header_phis_changed = false;
   bool first_block = true;
   foreach_list_typed(nir_cf_node, cf_node, node, list) {
      switch (cf_node->type) {
      case nir_cf_node_block: {
         nir_block *block = nir_cf_node_as_block(cf_node);
         nir_foreach_instr(instr, block) {
            if (instr->type == nir_instr_type_alu) {
               propagate_upper_bits_instr(nir_instr_as_alu(instr));
            } else if (instr->type == nir_instr_type_phi) {
               bool phi_changed = propagate_upper_bits_phi(nir_instr_as_phi(instr));
               header_phis_changed |= phi_changed && first_block;
            }
         }
         first_block = false;
         break;
      }
      case nir_cf_node_if: {
         nir_if *nif = nir_cf_node_as_if(cf_node);
         propagate_upper_bits(&nif->then_list);
         propagate_upper_bits(&nif->else_list);
         break;
      }
      case nir_cf_node_loop: {
         nir_loop *loop = nir_cf_node_as_loop(cf_node);
         while (propagate_upper_bits(&loop->body)) ;
         break;
      }
      case nir_cf_node_function:
         unreachable("Invalid cf type");
      }
   }
   return header_phis_changed;
}

static void
update_uses(nir_builder *bld, nir_ssa_def *old, nir_ssa_def *new, pass_flags flags)
{
   unsigned old_size = flags.old_size + 1;
   unsigned new_size = flags.new_size + 1;
   nir_alu_type type = flags.is_output_float ? nir_type_float : nir_type_uint;

   nir_op cvt_unsigned = nir_type_conversion_op(
      nir_type_uint | old_size, nir_type_uint | new_size, nir_rounding_mode_undef);
   nir_op cvt_signed = nir_type_conversion_op(
      nir_type_int | old_size, nir_type_int | new_size, nir_rounding_mode_undef);

   nir_foreach_use_safe(src, old) {
      nir_ssa_def *def = NULL;

      /* We can convert with fewer instructions in lower_alu_instr(). */
      if (src->parent_instr->pass_flags && !flags.is_output_float)
         def = new;

      if (!def)
         def = nir_convert_to_bit_size(bld, new, type, old_size);

      if (def != old)
         nir_instr_rewrite_src(src->parent_instr, src, nir_src_for_ssa(def));
   }

   nir_foreach_if_use_safe(src, old) {
      nir_ssa_def *converted = nir_convert_to_bit_size(bld, new, type, old_size);
      nir_if_rewrite_condition(src->parent_if, nir_src_for_ssa(converted));
   }
}

static void
lower_alu_instr(nir_builder *bld, nir_instr *instr, bool allow_undef_extend32)
{
   nir_alu_instr *alu = nir_instr_as_alu(instr);
   nir_op op = alu->op;
   pass_flags flags = {.v=instr->pass_flags};
   unsigned old_bit_size = flags.old_size + 1;
   unsigned new_bit_size = flags.new_size + 1;
   nir_alu_type output_type = nir_op_infos[op].output_type;

   bld->cursor = nir_before_instr(&alu->instr);

   /* Convert sources to the correct bit size */
   nir_ssa_def *srcs[4] = { NULL, NULL, NULL, NULL };
   for (unsigned i = 0; i < nir_op_infos[op].num_inputs; i++) {
      nir_ssa_def *src = nir_ssa_for_alu_src(bld, alu, i);
      nir_alu_type type = nir_op_infos[op].input_types[i];
      bool is_sized = nir_alu_type_get_type_size(type);
      unsigned op_old_bit_size = is_sized ? nir_alu_type_get_type_size(type) : old_bit_size;
      unsigned op_new_bit_size = is_sized ? nir_alu_type_get_type_size(type) : new_bit_size;
      bool care = is_sized || care_about_upper_bits(alu, i);

      /* Downcast if the upper bits might be invalid. */
      if (src->parent_instr->pass_flags) {
         pass_flags op_flags = {.v=src->parent_instr->pass_flags};
         nir_alu_type base = nir_alu_type_get_base_type(type);
         bool need_zext = care && base == nir_type_uint;
         bool need_sext = care && base == nir_type_int;

         /* update_uses() should ensure these are true. */
         assert(op_flags.is_output_unsized);
         assert(op_flags.new_size == flags.new_size);
         assert(!op_flags.is_output_float);

         if (is_sized || (need_zext && !op_flags.zext) || (need_sext && !op_flags.sext))
            src = nir_u2uN(bld, src, op_old_bit_size);
      }

      /* Convert if needed. */
      if (src->bit_size != op_new_bit_size) {
         if (!care && src->bit_size < 32 && op_new_bit_size == 32 && allow_undef_extend32)
            src = nir_undef_extend32(bld, src);
         else
            src = nir_convert_to_bit_size(bld, src, type, op_new_bit_size);
      }

      srcs[i] = src;
   }

   /* Emit the lowered ALU instruction. */
   nir_ssa_def *lowered_dst = NULL;
   switch (op) {
   case nir_op_imul_high:
   case nir_op_umul_high: {
      lowered_dst = nir_imul(bld, srcs[0], srcs[1]);
      nir_ssa_def *tmp = nir_imm_int(bld, old_bit_size);
      if (output_type & nir_type_uint)
         lowered_dst = nir_ubitfield_extract(bld, lowered_dst, tmp, tmp);
      else
         lowered_dst = nir_ibitfield_extract(bld, lowered_dst, tmp, tmp);
      break;
   }
   case nir_op_ishl:
   case nir_op_ishr:
   case nir_op_ushr:
      lowered_dst = nir_build_alu(
         bld, op, srcs[0], nir_iand(bld, srcs[1], nir_imm_int(bld, old_bit_size - 1)), NULL, NULL);
      break;
   default:
      lowered_dst = nir_build_alu(bld, op, srcs[0], srcs[1], srcs[2], srcs[3]);
      break;
   }
   lowered_dst->parent_instr->pass_flags = instr->pass_flags;

   /* Convert result back to the original bit-size if needed and update uses. */
   if (flags.is_output_unsized) {
      update_uses(bld, &alu->dest.dest.ssa, lowered_dst, flags);
   } else {
      nir_ssa_def_rewrite_uses(&alu->dest.dest.ssa,
                               nir_src_for_ssa(lowered_dst));
   }

   nir_instr_remove(&alu->instr);
}

static void
lower_phi_instr(nir_builder *bld, nir_instr *instr)
{
   nir_phi_instr *phi = nir_instr_as_phi(instr);
   pass_flags flags = {.v=instr->pass_flags};

   bld->cursor = nir_after_phis(instr->block);

   update_uses(bld, &phi->dest.ssa, &phi->dest.ssa, flags);
   phi->dest.ssa.bit_size = flags.new_size + 1;
}

static unsigned
get_alu_bit_size(const nir_alu_instr *alu)
{
   if (nir_alu_type_get_type_size(nir_op_infos[alu->op].output_type)) {
      for (unsigned i = 0; i < nir_op_infos[alu->op].num_inputs; i++) {
         if (!nir_alu_type_get_type_size(nir_op_infos[alu->op].input_types[i]))
            return alu->src[i].src.ssa->bit_size;
      }
      return 0;
   } else {
      return alu->dest.dest.ssa.bit_size;
   }
}

static bool
mark_for_lowering(nir_function_impl *impl, nir_lower_bit_size_callback callback,
                  void *callback_data)
{
   bool progress = false;

   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         instr->pass_flags = 0;

         if (instr->type != nir_instr_type_alu)
            continue;

         nir_alu_instr *alu = nir_instr_as_alu(instr);
         assert(alu->dest.dest.is_ssa);

         unsigned new_bit_size = callback(alu, callback_data);
         if (!new_bit_size)
            continue;

         unsigned old_bit_size = get_alu_bit_size(alu);
         nir_alu_type output_type = nir_op_infos[alu->op].output_type;

         assert(old_bit_size && old_bit_size != new_bit_size);

         pass_flags flags;
         flags.zext = false;
         flags.sext = false;
         flags.is_output_float = nir_alu_type_get_base_type(output_type) == nir_type_float;
         flags.is_output_unsized = !nir_alu_type_get_type_size(output_type);
         flags.new_size = new_bit_size - 1;
         flags.old_size = old_bit_size - 1;

         instr->pass_flags = flags.v;
         progress = true;
      }
   }

   return progress;
}

bool
nir_lower_bit_size(nir_shader *shader,
                   nir_lower_bit_size_callback callback,
                   void *callback_data,
                   bool allow_undef_extend32)
{
   STATIC_ASSERT(sizeof(pass_flags) == 2);

   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      if (!mark_for_lowering(function->impl, callback, callback_data)) {
         nir_metadata_preserve(function->impl, nir_metadata_all);
         continue;
      }

      propagate_upper_bits(&function->impl->body);

      nir_builder b;
      nir_builder_init(&b, function->impl);

      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (!instr->pass_flags)
               continue;

            if (instr->type == nir_instr_type_phi)
               lower_phi_instr(&b, instr);
            else if (instr->type == nir_instr_type_alu)
               lower_alu_instr(&b, instr, allow_undef_extend32);
         }
      }

      nir_metadata_preserve(function->impl, nir_metadata_block_index |
                                            nir_metadata_dominance);

      progress |= true;
   }

   return progress;
}
