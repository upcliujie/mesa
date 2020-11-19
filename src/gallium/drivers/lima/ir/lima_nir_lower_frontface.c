/*
 * Copyright © 2020 Andreas Baierl <ichgeh@imkreisrum.de>
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

#include "lima_ir.h"

static bool
lima_nir_lower_frontface_impl(nir_function_impl *impl)
{
   bool progress = false;
   nir_builder b;
   nir_builder_init(&b, impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
         if (intrin->intrinsic != nir_intrinsic_load_front_face)
            continue;

         intrin->dest.ssa.bit_size = 32;

         nir_foreach_use_safe(src, &intrin->dest.ssa) {
            b.cursor = nir_after_instr(&intrin->instr);
            nir_ssa_def *new = nir_ine(&b, &intrin->dest.ssa,
                                       nir_imm_int(&b, 0));

            nir_ssa_def_rewrite_uses_after(&intrin->dest.ssa,
                                           nir_src_for_ssa(new),
                                           new->parent_instr);
         }

         progress = true;
      }
   }

   return progress;
}

bool
lima_nir_lower_frontface(nir_shader *shader)
{
   assert(shader->info.stage == MESA_SHADER_FRAGMENT);

   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress |= lima_nir_lower_frontface_impl(function->impl);
   }

   return progress;
}
