/*
 * Copyright (C) 2020 Google
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

/* swap x and y coordinates for gl_Position inputs if swap_xy extension is
 * enabled.
 */

#include "nir/nir.h"
#include "nir/nir_builder.h"

bool
nir_lower_vs_swap_xy(nir_shader *shader, bool swap_xy)
{
   bool progress = false;
   /* No need to do anything here if swap_xy is not set. */
   if (!swap_xy)
      return progress;
   
   assert(shader->info.stage == MESA_SHADER_VERTEX);
   nir_foreach_function(func, shader) {
      nir_foreach_block(block, func->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
            if (intr->intrinsic != nir_intrinsic_store_deref)
               continue;

            nir_variable *var = nir_intrinsic_get_var(intr, 0);
            if (var->data.mode != nir_var_shader_out ||
                var->data.location != VARYING_SLOT_POS)
               continue;

            nir_builder b;
            nir_builder_init(&b, func->impl);
            b.cursor = nir_before_instr(instr);

            /* Grab the source of world_space. */
            nir_ssa_def *input_point = nir_ssa_for_src(&b, intr->src[1], 4);    

            /* Swap the x and y of world_space. */
            nir_ssa_def *world_space = nir_vec4(&b,
                             nir_channel(&b, input_point, 1),
                             nir_channel(&b, input_point, 0),
                             nir_channel(&b, input_point, 2),
                             nir_channel(&b, input_point, 3));

            nir_instr_rewrite_src(instr, &intr->src[1],
                                  nir_src_for_ssa(world_space));
            progress = true;
         }
      }
      if (progress) {
         nir_metadata_preserve(func->impl, nir_metadata_block_index |
                               nir_metadata_dominance);
      }
   }
   return progress;
}
