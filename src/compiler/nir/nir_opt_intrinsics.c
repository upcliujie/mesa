/*
 * Copyright © 2017 Intel Corporation
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

/**
 * \file nir_opt_intrinsics.c
 */

static bool
src_is_single_use_shuffle(nir_src src, nir_ssa_def **data, nir_ssa_def **index)
{
   nir_intrinsic_instr *shuffle = nir_src_as_intrinsic(src);
   if (shuffle == NULL || shuffle->intrinsic != nir_intrinsic_shuffle)
      return false;

   /* This is only called when src is part of an ALU op so requiring no if
    * uses is reasonable.  If we ever want to use this from an if statement,
    * we can change it then.
    */
   if (!list_is_empty(&shuffle->dest.ssa.if_uses) ||
       !list_is_singular(&shuffle->dest.ssa.uses))
      return false;

   assert(shuffle->src[0].is_ssa);
   assert(shuffle->src[1].is_ssa);

   *data = shuffle->src[0].ssa;
   *index = shuffle->src[1].ssa;

   return true;
}

static nir_ssa_def *
try_opt_bcsel_of_shuffle(nir_builder *b, nir_alu_instr *alu,
                         bool block_has_discard)
{
   assert(alu->op == nir_op_bcsel);

   /* If we've seen a discard in this block, don't do the optimization.  We
    * could try to do something fancy where we check if the shuffle is on our
    * side of the discard or not but this is good enough for correctness for
    * now and subgroup ops in the presence of discard aren't common.
    */
   if (block_has_discard)
      return false;

   if (!nir_alu_src_is_trivial_ssa(alu, 0))
      return NULL;

   nir_ssa_def *data1, *index1;
   if (!nir_alu_src_is_trivial_ssa(alu, 1) ||
       alu->src[1].src.ssa->parent_instr->block != alu->instr.block ||
       !src_is_single_use_shuffle(alu->src[1].src, &data1, &index1))
      return NULL;

   nir_ssa_def *data2, *index2;
   if (!nir_alu_src_is_trivial_ssa(alu, 2) ||
       alu->src[2].src.ssa->parent_instr->block != alu->instr.block ||
       !src_is_single_use_shuffle(alu->src[2].src, &data2, &index2))
      return NULL;

   if (data1 != data2)
      return NULL;

   nir_ssa_def *index = nir_bcsel(b, alu->src[0].src.ssa, index1, index2);
   nir_ssa_def *shuffle = nir_shuffle(b, data1, index);

   return shuffle;
}

static bool
opt_intrinsics_alu(nir_builder *b, nir_alu_instr *alu,
                   bool block_has_discard)
{
   nir_ssa_def *replacement = NULL;

   switch (alu->op) {
   case nir_op_bcsel:
      replacement = try_opt_bcsel_of_shuffle(b, alu, block_has_discard);
      break;

   default:
      break;
   }

   if (replacement) {
      nir_ssa_def_rewrite_uses(&alu->dest.dest.ssa,
                               replacement);
      nir_instr_remove(&alu->instr);
      return true;
   } else {
      return false;
   }
}

struct fragdepth_optim {
   bool wrote_once;
   nir_intrinsic_instr *store_intrin;
};

static bool
opt_intrinsics_intrin(nir_builder *b, nir_intrinsic_instr *intrin,
                      const struct nir_shader_compiler_options *options,
                      struct fragdepth_optim *fragdepth_store_opt)
{
   switch (intrin->intrinsic) {
   case nir_intrinsic_load_sample_mask_in: {
      /* Transform:
       *   gl_SampleMaskIn == 0 ---> gl_HelperInvocation
       *   gl_SampleMaskIn != 0 ---> !gl_HelperInvocation
       */
      if (!options->optimize_sample_mask_in)
         return false;

      bool progress = false;
      nir_foreach_use_safe(use_src, &intrin->dest.ssa) {
         if (use_src->parent_instr->type == nir_instr_type_alu) {
            nir_alu_instr *alu = nir_instr_as_alu(use_src->parent_instr);

            if (alu->op == nir_op_ieq ||
                alu->op == nir_op_ine) {
               /* Check for 0 in either operand. */
               nir_const_value *const_val =
                   nir_src_as_const_value(alu->src[0].src);
               if (!const_val)
                  const_val = nir_src_as_const_value(alu->src[1].src);
               if (!const_val || const_val->i32 != 0)
                  continue;

               nir_ssa_def *new_expr = nir_load_helper_invocation(b, 1);

               if (alu->op == nir_op_ine)
                  new_expr = nir_inot(b, new_expr);

               nir_ssa_def_rewrite_uses(&alu->dest.dest.ssa,
                                        new_expr);
               nir_instr_remove(&alu->instr);
               progress = true;
            }
         }
      }
      return progress;
   }

   case nir_intrinsic_store_deref: {
      if (b->shader->info.stage != MESA_SHADER_FRAGMENT)
         return false;

      nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);
      nir_variable *var = nir_deref_instr_get_variable(deref);

      if (var->data.mode == nir_var_shader_out &&
          var->data.location == FRAG_RESULT_DEPTH) {
         /* We found a write to gl_FragDepth */
         if (fragdepth_store_opt->wrote_once) {
            /* This isn't the only write: give up on this optimisation */
            fragdepth_store_opt->store_intrin = NULL;
         } else {
            fragdepth_store_opt->wrote_once = true;
            nir_ssa_scalar scalar = nir_ssa_scalar_resolved(intrin->src[1].ssa, 0);
            if (scalar.comp == 2 &&
                scalar.def->parent_instr->type == nir_instr_type_intrinsic &&
                nir_instr_as_intrinsic(scalar.def->parent_instr)->intrinsic == nir_intrinsic_load_frag_coord) {
               /* We're storing gl_FragCoord.z: remember 'intrin' so we can try to remove it later. */
               fragdepth_store_opt->store_intrin = intrin;
            }
         }
      }
      return false;
   }

   default:
      return false;
   }
}

static bool
opt_intrinsics_impl(nir_function_impl *impl,
                    const struct nir_shader_compiler_options *options,
                    struct fragdepth_optim *fragdepth_store_opt)
{
   nir_builder b;
   nir_builder_init(&b, impl);
   bool progress = false;

   nir_foreach_block(block, impl) {
      bool block_has_discard = false;

      nir_foreach_instr_safe(instr, block) {
         b.cursor = nir_before_instr(instr);

         switch (instr->type) {
         case nir_instr_type_alu:
            if (opt_intrinsics_alu(&b, nir_instr_as_alu(instr),
                                   block_has_discard))
               progress = true;
            break;

         case nir_instr_type_intrinsic: {
            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            if (intrin->intrinsic == nir_intrinsic_discard ||
                intrin->intrinsic == nir_intrinsic_discard_if ||
                intrin->intrinsic == nir_intrinsic_demote ||
                intrin->intrinsic == nir_intrinsic_demote_if ||
                intrin->intrinsic == nir_intrinsic_terminate ||
                intrin->intrinsic == nir_intrinsic_terminate_if)
               block_has_discard = true;

            if (opt_intrinsics_intrin(&b, intrin, options,
                                      fragdepth_store_opt))
               progress = true;
            break;
         }

         default:
            break;
         }
      }
   }

   return progress;
}

bool
nir_opt_intrinsics(nir_shader *shader)
{
   bool progress = false;
   struct fragdepth_optim fd_opt = {0};

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      if (opt_intrinsics_impl(function->impl, shader->options, &fd_opt)) {
         progress = true;
         nir_metadata_preserve(function->impl, nir_metadata_block_index |
                                               nir_metadata_dominance);
      } else {
         nir_metadata_preserve(function->impl, nir_metadata_all);
      }
   }

   if (fd_opt.wrote_once && fd_opt.store_intrin) {
      /* Found a single store to gl_FragDepth, and it writes gl_FragCoord.z to it.
       * Remove it since that's the implicit value of gl_FragDepth.
       */
      nir_instr_remove(&fd_opt.store_intrin->instr);
      progress = true;
   }

   return progress;
}
