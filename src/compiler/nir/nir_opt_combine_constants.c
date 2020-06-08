/*
 * Copyright © 2020 Intel Corporation
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

/**
 * \file nir_opt_combine_constants.c
 * Attempts to reduce the number of constants by "CSE" of negations.
 *
 * Do not run constant folding after this pass!
 *
 * This pass is fairly specific to some of the quirks of the Intel GPU
 * architecture.  Intel GPUs have some limitations with the use of immediate
 * values in some instructions.  The major limitations are:
 *
 * - Previous to Gen11, 3-source instructions (e.g., multiply-and-accumulate)
 *   cannot have any immediate sources.  On Gen11 and later, it is sometimes
 *   possible to use an immediate value for first or last source, but there
 *   are still limitations.
 *
 * - Two-source instructions instructions can have only one immediate source,
 *   and that source must be the second source.  Many two-source instructions
 *   are commutative, but shifts, rotates, POW, and FDIV are not.  Division is
 *   always lowered to multiplication with the reciprocal, so FDIV is ignored.
 *   Even selection is effectively commutative due to an "inverted" condition
 *   flag.  As a result, bcsel with two immediate sources and ishl, ishr,
 *   ushr, uror, and fpow with the first source immediate are problematic.
 *   Implementing a NIR sequence like (bcsel, (flt, a, b), 46.0, 5.0) requires
 *   three instructions: a compare, a move to load one of the immediate
 *   values, and a SEL instruction to pick the desired value.
 *
 * As a result, generated shaders can have a lot of instructions that just
 * load immediate values into registers.  To add to the problem, it is
 * possible for a value and its negation to both be loaded into registers.
 * This pass attempts to aleviate this part of the problem.  The negation may
 * not always be obvious.
 *
 * This optimization pass uses \c util_combine_constants to reduce the number
 * of \c load_const instructions.
 */

#include <math.h> /* for fabsf() */
#include "util/combine_constants.h"
#include "nir.h"
#include "nir_builder.h"

static nir_const_value
as_nir_const_value(constant_value v)
{
   nir_const_value ncv;

   ncv.u64 = v.u64;
   return ncv;
}

static unsigned
add_candidate_constant(struct value **candidates, unsigned num_candidates,
                       nir_alu_instr *alu, unsigned src)
{
   assert(nir_src_is_const(alu->src[src].src));

   const nir_load_const_instr *const load =
      nir_instr_as_load_const(alu->src[src].src.ssa->parent_instr);

   if (load->def.num_components != 1)
      return num_candidates;

   if (load->def.bit_size < 8)
      return num_candidates;

   struct value *updated_candidates =
      realloc(*candidates, (num_candidates + 1) * sizeof(struct value));

   memset(&updated_candidates[num_candidates], 0, sizeof(struct value));
   updated_candidates[num_candidates].value.u64 = load->value[0].u64;
   updated_candidates[num_candidates].instr = (struct abstract_instruction *) alu;
   updated_candidates[num_candidates].bit_size = load->def.bit_size;
   updated_candidates[num_candidates].src = src;
   updated_candidates[num_candidates].no_negations = false;

   if (alu->op == nir_op_bcsel || alu->op == nir_op_b32csel) {
      updated_candidates[num_candidates].allow_one_constant = true;
      updated_candidates[num_candidates].type = either_type;
   } else {
      updated_candidates[num_candidates].allow_one_constant = false;

      const nir_alu_type base_type =
         nir_alu_type_get_base_type(nir_op_infos[alu->op].input_types[src]);

      updated_candidates[num_candidates].type =
         base_type == nir_type_float ? float_only : integer_only;
   }

   *candidates = updated_candidates;

   return num_candidates + 1;
}

static unsigned
collect_candidate_constants(nir_function_impl *impl, struct value **candidates)
{
   unsigned num_candidates = 0;

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_alu)
            continue;

         nir_alu_instr *const alu = nir_instr_as_alu(instr);

         switch (alu->op) {
         case nir_op_ffma:
         case nir_op_flrp:
            for (unsigned i = 0; i < 3; i++) {
               if (nir_src_is_const(alu->src[i].src)) {
                  num_candidates = add_candidate_constant(candidates,
                                                          num_candidates,
                                                          alu,
                                                          i);
               }
            }

            break;

         case nir_op_bcsel:
         case nir_op_b32csel:
            if (nir_src_is_const(alu->src[1].src) &&
                nir_src_is_const(alu->src[2].src)) {
               /* Many shaders contain code like 'gl_FrontFacing ? 1.0 :
                * -1.0'.  This sequence is so common that at least some
                * drivers (e.g., i965 and Iris) have special optimizations for
                * it.  Don't include bcsel instruction sources that match that
                * pattern.
                */
               if (fabs(nir_src_as_float(alu->src[1].src)) == 1.0 &&
                   fabs(nir_src_as_float(alu->src[2].src)) == 1.0) {
                  const nir_intrinsic_instr *const intrin =
                     nir_src_as_intrinsic(alu->src[0].src);

                  if (intrin != NULL &&
                      intrin->intrinsic == nir_intrinsic_load_front_face)
                     break;
               }

               num_candidates = add_candidate_constant(candidates,
                                                       num_candidates,
                                                       alu,
                                                       1);
               num_candidates = add_candidate_constant(candidates,
                                                       num_candidates,
                                                       alu,
                                                       2);
            }

            break;


         case nir_op_ishl:
         case nir_op_ishr:
         case nir_op_ushr:
         case nir_op_uror:
         case nir_op_fpow:
            if (nir_src_is_const(alu->src[0].src)) {
               num_candidates = add_candidate_constant(candidates,
                                                       num_candidates,
                                                       alu,
                                                       0);
            }

            break;

         default:
            break;
         }
      }
   }

   return num_candidates;
}

static void
replace_constants(nir_function_impl *impl,
                  const struct combine_constants_result *result,
                  const struct value *candidates)
{
   /* Emit the load_const instructions for the optimal route, and update the
    * users to use the new constants.
    */
   nir_block *const start_block = nir_start_block(impl);
   nir_builder b;
   nir_builder_init(&b, impl);

   for (unsigned i = 0; i < result->num_values_to_emit; i++) {
      /* To ensure the def dominates all the uses, insert the new constants at
       * the beginning of the function.
       */
      b.cursor = nir_before_block(start_block);

      const nir_const_value v = as_nir_const_value(result->values_to_emit[i].value);
      nir_ssa_def *imm = nir_build_imm(&b, 1,
                                       result->values_to_emit[i].bit_size, &v);

      for (unsigned j = 0; j < result->values_to_emit[i].num_users; j++) {
         const unsigned map_idx = j + result->values_to_emit[i].first_user;
         const unsigned idx = result->user_map[map_idx].index;
         const unsigned src = candidates[idx].src;
         nir_alu_instr *instr = (nir_alu_instr *) candidates[idx].instr;

         if (!result->user_map[map_idx].negate) {
            nir_instr_rewrite_src(&instr->instr,
                                  &instr->src[src].src,
                                  nir_src_for_ssa(imm));
         } else {
            b.cursor = nir_before_instr(&instr->instr);

            assert(result->user_map[map_idx].type != either_type);

            const nir_op neg_op = result->user_map[map_idx].type == float_only
               ? nir_op_fneg : nir_op_ineg;

            nir_ssa_def *const negation =
               nir_build_alu(&b, neg_op, imm, NULL, NULL, NULL);

            nir_instr_rewrite_src(&instr->instr,
                                  &instr->src[src].src,
                                  nir_src_for_ssa(negation));
         }
      }
   }
}

static bool
nir_opt_combine_constants_impl(nir_function_impl *impl)
{
   bool progress = false;
   struct value *candidates = NULL;

   /* Collect the set of candidate constants. */
   const unsigned num_candidates = collect_candidate_constants(impl,
                                                               &candidates);

   if (num_candidates != 0) {
      struct combine_constants_result *result =
         util_combine_constants(candidates, num_candidates);

      replace_constants(impl, result, candidates);

      util_combine_constants_result_dtor(result);

      progress = true;
   }


   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance);
   } else {
#ifndef NDEBUG
      impl->valid_metadata &= ~nir_metadata_not_properly_reset;
#endif
   }

   return progress;
}

bool
nir_opt_combine_constants(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress |= nir_opt_combine_constants_impl(function->impl);
   }

   return progress;
}
