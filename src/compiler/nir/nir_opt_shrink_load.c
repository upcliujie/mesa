/*
 * Copyright © 2018 Valve Corporation
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

static nir_variable_mode
load_intrin_to_mode(nir_intrinsic_op op)
{
   switch (op) {
   case nir_intrinsic_load_ubo:
      return nir_var_mem_ubo;
   case nir_intrinsic_load_ssbo:
      return nir_var_mem_ssbo;
   case nir_intrinsic_load_shared:
      return nir_var_mem_shared;
   case nir_intrinsic_load_global:
      return nir_var_mem_global;
   case nir_intrinsic_load_push_constant:
      return nir_var_mem_push_const;
   default:
      return 0;
   }
}

static bool
opt_shrink_load_impl(nir_function_impl *impl, nir_variable_mode modes)
{
   bool progress = false;

   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
         if (!(modes & load_intrin_to_mode(intrin->intrinsic)))
            continue;

         unsigned mask = nir_ssa_def_components_read(&intrin->dest.ssa);

         if (intrin->num_components > util_last_bit(mask)) {
            intrin->num_components = util_last_bit(mask);
            intrin->dest.ssa.num_components = intrin->num_components;
            progress = true;
         }
      }
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
nir_opt_shrink_load(nir_shader *shader, nir_variable_mode modes)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl && opt_shrink_load_impl(function->impl, modes))
         progress = true;
   }

   return progress;
}
