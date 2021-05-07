/*
 * Copyright © 2021 Advanced Micro Devices, Inc.
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
 * This pass removes no-op assignment to gl_FragDepth.
 *
 * gl_FragDepth implicit value is gl_FragCoord.z, so if a shader only assign
 * this value to gl_FragDepth, the store instruction is removed.
 */

struct fragdepth_optim {
   bool wrote_once;
   nir_intrinsic_instr *store_intrin;
};

static bool
opt_fragdepth_pass(struct nir_builder *b, nir_instr *instr, void *data)
{
   struct fragdepth_optim *fragdepth_store_opt = data;

   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   if (intrin->intrinsic != nir_intrinsic_store_deref)
      return false;

   nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);
   nir_variable *var = nir_deref_instr_get_variable(deref);

   if (!var ||
       var->data.mode != nir_var_shader_out ||
       var->data.location != FRAG_RESULT_DEPTH)
      return false;

   /* We found a write to gl_FragDepth */
   if (fragdepth_store_opt->wrote_once) {
      /* This isn't the only write: give up on this optimization */
      fragdepth_store_opt->store_intrin = NULL;
   } else {
      fragdepth_store_opt->wrote_once = true;
      nir_ssa_scalar scalar = nir_ssa_scalar_resolved(intrin->src[1].ssa, 0);
      nir_instr *scalar_instr = scalar.def->parent_instr;
      if (scalar.comp == 2 &&
          scalar_instr->type == nir_instr_type_intrinsic &&
          nir_instr_as_intrinsic(scalar_instr)->intrinsic == nir_intrinsic_load_frag_coord) {
         /* We're storing gl_FragCoord.z: remember intrin so we can try to remove it later */
         fragdepth_store_opt->store_intrin = intrin;
      }
   }

   return false;
}

bool
nir_opt_fragdepth(nir_shader *shader)
{
   struct fragdepth_optim fd_opt = {0};

   if (shader->info.stage != MESA_SHADER_FRAGMENT)
      return false;

   nir_shader_instructions_pass(shader, opt_fragdepth_pass, nir_metadata_all, &fd_opt);

   if (fd_opt.wrote_once && fd_opt.store_intrin) {
      /* Found a single store to gl_FragDepth, and it writes gl_FragCoord.z to it.
       * Remove it since that's the implicit value of gl_FragDepth.
       */
      nir_instr_remove(&fd_opt.store_intrin->instr);

      nir_metadata preserved = nir_metadata_block_index |
                               nir_metadata_dominance |
                               nir_metadata_loop_analysis |
                               nir_metadata_instr_index;
      nir_foreach_function(function, shader) {
         if (function->impl)
            nir_metadata_preserve(function->impl, preserved);
      }

      return true;
   }

   return false;
}
