/*
 * Copyright © 2021 Google, Inc.
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

#include "nir.h"
#include "nir_builder.h"

/*
 * This pass tries to move narrowing precision conversion of phi def to
 * phi srcs, when all the uses of the phi are equivalent narrowing
 * instructions.  In other words, convert:
 *
 *    vec1 32 ssa_124 = load_const (0x00000000)
 *    ...
 *    loop {
 *        ...
 *        vec1 32 ssa_155 = phi block_0: ssa_124, block_4: ssa_53
 *        vec1 16 ssa_8 = i2imp ssa_155
 *        ...
 *        vec1 32 ssa_53 = i2i32 ssa_52
 *    }
 *
 * into:
 *
 *    vec1 32 ssa_124 = load_const (0x00000000)
 *    vec1 16 ssa_156 = i2i16 ssa_124
 *    ...
 *    loop {
 *        ...
 *        vec1 16 ssa_8 = phi block_0: ssa_156, block_4: ssa_157
 *        ...
 *        vec1 32 ssa_53 = i2i32 ssa_52
 *        vec1 16 ssa_157 = i2i16 ssa_53
 *    }
 *
 * Or failing that, tries to push widening conversion of phi srcs to
 * the phi def.  In this case, since load_const is frequently one
 * of the phi sources this pass checks if can be narrowed without a
 * loss of precision:
 *
 *    vec1 32 ssa_0 = load_const (0x00000000)
 *    ...
 *    loop {
 *        ...
 *        vec1 32 ssa_8 = phi block_0: ssa_0, block_4: ssa_19
 *        ...
 *        vec1 16 ssa_18 = iadd ssa_21, ssa_3
 *        vec1 32 ssa_19 = i2i32 ssa_18
 *    }
 *
 * into:
 *
 *    vec1 32 ssa_0 = load_const (0x00000000)
 *    vec1 16 ssa_22 = i2i16 ssa_0
 *    ...
 *    loop {
 *        ...
 *        vec1 16 ssa_8 = phi block_0: ssa_22, block_4: ssa_18
 *        vec1 32 ssa_23 = i2i32 ssa_8
 *        ...
 *        vec1 16 ssa_18 = iadd ssa_21, ssa_3
 *    }
 *
 * Note that either transformations can convert x2ymp  into x2y16, which
 * is normally done later in nir_opt_algebraic_late(), losing the option
 * to fold away sequences like (i2i32 (i2imp (x))), but algebraic opts
 * cannot see through phis.
 */

#define INVALID_OP nir_num_opcodes

/**
 * Get the corresponding exact conversion for a x2ymp conversion
 */
static nir_op
concrete_conversion(nir_op op)
{
   switch (op) {
   case nir_op_i2imp: return nir_op_i2i16;
   case nir_op_i2fmp: return nir_op_i2f16;
   case nir_op_u2fmp: return nir_op_u2f16;
   case nir_op_f2fmp: return nir_op_f2f16;
   case nir_op_f2imp: return nir_op_f2i16;
   case nir_op_f2ump: return nir_op_f2u16;
   default:           return op;
   }
}

static nir_op
narrowing_conversion_op(nir_instr *instr, nir_op current_op)
{
   if (instr->type != nir_instr_type_alu)
      return INVALID_OP;

   nir_op op = nir_instr_as_alu(instr)->op;
   switch (op) {
   case nir_op_i2imp:
   case nir_op_i2i16:
   case nir_op_i2fmp:
   case nir_op_i2f16:
   case nir_op_u2fmp:
   case nir_op_u2f16:
   case nir_op_f2fmp:
   case nir_op_f2f16:
   case nir_op_f2imp:
   case nir_op_f2i16:
   case nir_op_f2ump:
   case nir_op_f2u16:
   case nir_op_f2f16_rtne:
   case nir_op_f2f16_rtz:
      break;
   default:
      return INVALID_OP;
   }

   /* If we've already picked a conversion op from a previous phi use,
    * make sure it is compatible with the current use
    */
   if (current_op != INVALID_OP) {
      if (current_op != op) {
         /* If we have different conversions, but one can be converted
          * to the other, then let's do that:
          */
         if (concrete_conversion(current_op) == concrete_conversion(op)) {
            op = concrete_conversion(op);
         } else {
            return INVALID_OP;
         }
      }
   }

   return op;
}

static nir_op
widening_conversion_op(nir_instr *instr)
{
   if (instr->type != nir_instr_type_alu)
      return INVALID_OP;

   nir_alu_instr *alu = nir_instr_as_alu(instr);
   switch (alu->op) {
   case nir_op_i2i32:
   case nir_op_i2f32:
   case nir_op_u2f32:
   case nir_op_f2f32:
   case nir_op_f2i32:
   case nir_op_f2u32:
      break;
   default:
      return INVALID_OP;
   }

   /* We also need to check that the conversion's dest was actually
    * wider:
    */
   if (nir_dest_bit_size(alu->dest.dest) <=
         nir_src_bit_size(alu->src[0].src))
      return INVALID_OP;

   return alu->op;
}

static nir_alu_type
op_to_type(nir_op op)
{
   switch (op) {
   case nir_op_i2imp:
   case nir_op_i2i16:
   case nir_op_f2imp:
   case nir_op_i2i32:
   case nir_op_f2i32:
      return nir_type_int;
   case nir_op_f2u16:
   case nir_op_f2ump:
   case nir_op_f2u32:
      return nir_type_uint;
   case nir_op_i2fmp:
   case nir_op_i2f16:
   case nir_op_u2fmp:
   case nir_op_u2f16:
   case nir_op_f2fmp:
   case nir_op_f2f16:
   case nir_op_f2i16:
   case nir_op_f2f16_rtne:
   case nir_op_f2f16_rtz:
   case nir_op_i2f32:
   case nir_op_u2f32:
   case nir_op_f2f32:
      return nir_type_float;
   default:
      unreachable("bad op");
      return nir_type_invalid;
   }
}

static bool
try_move_narrowing_dst(nir_builder *b, nir_phi_instr *phi)
{
   nir_op op = INVALID_OP;

   assert(phi->dest.is_ssa);

   /* If the phi has already been narrowed, nothing more to do: */
   if (phi->dest.ssa.bit_size != 32)
      return false;

   /* Are the only uses of the phi conversion instructions, and
    * are they all the same conversion?
    */
   nir_foreach_use (use, &phi->dest.ssa) {
      op = narrowing_conversion_op(use->parent_instr, op);

      /* Not a (compatible) narrowing conversion: */
      if (op == INVALID_OP)
         return false;
   }

   /* If the phi has no uses, then nothing to do: */
   if (op == INVALID_OP)
      return false;

   /* construct replacement phi instruction: */
   nir_phi_instr *new_phi = nir_phi_instr_create(b->shader);
   nir_ssa_dest_init(&new_phi->instr, &new_phi->dest,
                     phi->dest.ssa.num_components,
                     phi->dest.ssa.bit_size, NULL);

   /* Push the conversion into the new phi sources: */
   nir_foreach_phi_src (src, phi) {
      assert(src->src.is_ssa);

      /* insert new conversion instr in block of original phi src: */
      b->cursor = nir_after_instr_and_phis(src->src.ssa->parent_instr);
      nir_ssa_def *old_src = nir_ssa_for_src(
            b, src->src, nir_src_num_components(src->src));
      nir_ssa_def *new_src = nir_build_alu(b, op, old_src, NULL, NULL, NULL);

      new_phi->dest.ssa.bit_size = new_src->bit_size;

      /* and add corresponding phi_src to the new_phi: */
      nir_phi_src *phi_src = ralloc(new_phi, nir_phi_src);
      phi_src->pred = src->pred;
      phi_src->src = nir_src_for_ssa(new_src);
      exec_list_push_tail(&new_phi->srcs, &phi_src->node);
   }

   /* And finally rewrite the original uses of the original phi uses to
    * directly use the new phi, skipping the conversion out of the orig
    * phi
    */
   nir_foreach_use (use, &phi->dest.ssa) {
      /* We've previously established that all the uses were alu
       * conversion ops:
       */
      nir_alu_instr *alu = nir_instr_as_alu(use->parent_instr);

      assert(alu->dest.dest.is_ssa);

      nir_ssa_def_rewrite_uses(&alu->dest.dest.ssa, &new_phi->dest.ssa);
   }

   /* And finally insert the new phi after all sources are in place: */
   b->cursor = nir_after_instr(&phi->instr);
   nir_builder_instr_insert(b, &new_phi->instr);

   return true;
}

/* Check all the phi sources to see if they are the same widening op, in
 * which case we can push the widening op to the other side of the phi
 */
static nir_op
find_widening_op(nir_phi_instr *phi, bool *has_load_const)
{
   nir_op op = INVALID_OP;

   *has_load_const = false;

   nir_foreach_phi_src (src, phi) {
      assert(src->src.is_ssa);

      nir_instr *instr = src->src.ssa->parent_instr;
      if (instr->type == nir_instr_type_load_const) {
         *has_load_const = true;
         continue;
      }

      nir_op src_op = widening_conversion_op(instr);

      /* Not a widening conversion: */
      if (src_op == INVALID_OP)
         return INVALID_OP;

      /* If it is a widening conversion, it needs to be the same op as
       * other phi sources:
       */
      if ((op != INVALID_OP) && (op != src_op))
         return INVALID_OP;

      op = src_op;
   }

   return op;
}

static bool
can_convert_load_const(nir_load_const_instr *lc, nir_op op)
{
   nir_alu_type type = op_to_type(op);

   /* Note that we only handle phi's with bit_size == 32: */
   assert(lc->def.bit_size == 32);

   for (unsigned i = 0; i < lc->def.num_components; i++) {
      switch (type) {
      case nir_type_int:
         if (lc->value[i].i32 != (int32_t)(int16_t)lc->value[i].i32)
            return false;
         break;
      case nir_type_uint:
         if (lc->value[i].u32 != (uint32_t)(uint16_t)lc->value[i].u32)
            return false;
         break;
      case nir_type_float:
         if (lc->value[i].f32 != _mesa_half_to_float(
               _mesa_float_to_half(lc->value[i].f32)))
            return false;
         break;
      default:
         unreachable("bad type");
         return false;
      }
   }

   return true;
}

static bool
try_convert_load_consts(nir_builder *b, nir_phi_instr *phi, nir_op op)
{
   /* First check that we can convert all load_const sources: */
   nir_foreach_phi_src (src, phi) {
      assert(src->src.is_ssa);

      nir_instr *instr = src->src.ssa->parent_instr;
      if (instr->type != nir_instr_type_load_const)
         continue;

      if (!can_convert_load_const(nir_instr_as_load_const(instr), op))
         return false;
   }

   /* If we get this far, we can convert all the load_const sources: */
   nir_foreach_phi_src (src, phi) {
      assert(src->src.is_ssa);

      nir_instr *instr = src->src.ssa->parent_instr;
      if (instr->type != nir_instr_type_load_const)
         continue;

      nir_load_const_instr *lc = nir_instr_as_load_const(instr);

      b->cursor = nir_after_instr(instr);

      nir_ssa_def *def;

      switch (op_to_type(op)) {
      case nir_type_int:
      case nir_type_uint:
         def = nir_i2imp(b, &lc->def);
         break;
      case nir_type_float:
         def = nir_f2fmp(b, &lc->def);
         break;
      default:
         unreachable("bad type");
         return false;
      }

      nir_ssa_def *new_src = nir_build_alu(b, op, def, NULL, NULL, NULL);

      nir_instr_rewrite_src_ssa(&phi->instr, &src->src, new_src);
   }

   return true;
}

static bool
try_move_widening_src(nir_builder *b, nir_phi_instr *phi)
{
   assert(phi->dest.is_ssa);

   /* If the phi has already been narrowed, nothing more to do: */
   if (phi->dest.ssa.bit_size != 32)
      return false;

   bool has_load_const;
   nir_op op = find_widening_op(phi, &has_load_const);

   if (op == INVALID_OP)
      return false;

   /* If we could otherwise move widening sources, but load_const
    * is one of the phi sources (and does not have a widening
    * conversion, but could have a narrowing->widening sequence
    * inserted without loss of precision), insert that narrowing->
    * widening sequence now to make the rest of the transformation
    * possible:
    */
   if (has_load_const) {
      if (!try_convert_load_consts(b, phi, op))
         return false;

      /* At this point, since we've already transformed the IR, we
       * need to return progress==true, so it would be bad if we
       * were in a state where that wasn't true:
       */
      assert((find_widening_op(phi, &has_load_const) != INVALID_OP) &&
             !has_load_const);
   }

   /* construct replacement phi instruction: */
   nir_phi_instr *new_phi = nir_phi_instr_create(b->shader);
   nir_ssa_dest_init(&new_phi->instr, &new_phi->dest,
                     phi->dest.ssa.num_components,
                     phi->dest.ssa.bit_size, NULL);

   /* Remove the widening conversions from the phi sources: */
   nir_foreach_phi_src (src, phi) {
      /* at this point we know the sources source is a conversion: */
      nir_alu_instr *alu = nir_instr_as_alu(src->src.ssa->parent_instr);
      b->cursor = nir_after_instr(&alu->instr);

      /* The conversion we are stripping off could have had a swizzle,
       * so replace it with a mov if necessary:
       */
      nir_ssa_def *new_src =
            nir_mov_alu(b, alu->src[0], nir_dest_num_components(alu->dest.dest));

      new_phi->dest.ssa.bit_size = new_src->bit_size;

      /* add corresponding phi_src to the new_phi: */
      nir_phi_src *phi_src = ralloc(new_phi, nir_phi_src);
      phi_src->pred = src->pred;
      phi_src->src = nir_src_for_ssa(new_src);
      exec_list_push_tail(&new_phi->srcs, &phi_src->node);
   }

   /* And insert the new phi after all sources are in place: */
   b->cursor = nir_after_instr(&phi->instr);
   nir_builder_instr_insert(b, &new_phi->instr);

   /* And finally add back the widening conversion after the phi,
    * and re-write the original phi's uses
    */
   b->cursor = nir_after_instr_and_phis(&new_phi->instr);
   nir_ssa_def *def = nir_build_alu(b, op, &new_phi->dest.ssa, NULL, NULL, NULL);

   nir_ssa_def_rewrite_uses(&phi->dest.ssa, def);

   return true;
}

static bool
lower_phi(nir_builder *b, nir_phi_instr *phi)
{
   bool progress = try_move_narrowing_dst(b, phi);
   if (!progress)
      progress = try_move_widening_src(b, phi);
   return progress;
}

bool
nir_opt_phi_precision(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      nir_builder b;
      nir_builder_init(&b, function->impl);

      nir_foreach_block (block, function->impl) {
         nir_foreach_instr_safe (instr, block) {
            if (instr->type != nir_instr_type_phi)
               break;

            progress |= lower_phi(&b, nir_instr_as_phi(instr));
         }
      }

      if (progress) {
         nir_metadata_preserve(function->impl,
                               nir_metadata_block_index |
                               nir_metadata_dominance);
      } else {
         nir_metadata_preserve(function->impl, nir_metadata_all);
      }
   }

   return progress;
}

