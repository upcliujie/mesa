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

/* swap fddx and fddy operations for fragment shader if swap_xy extension is
 * enabled.
 */

#include "nir/nir.h"
#include "nir/nir_builder.h"

bool
nir_lower_fs_swap_xy(nir_shader *shader, bool swap_xy)
{
   bool progress = false;
   /* No need to do anything here if swap_xy is not set. */
   if (swap_xy == false)
      return progress;
   
   assert(shader->info.stage == MESA_SHADER_FRAGMENT);
   nir_foreach_function(func, shader) {
       if (!func->impl)
          continue;
       
      nir_builder b;
      nir_builder_init(&b, func->impl);
      nir_foreach_block(block, func->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type == nir_instr_type_alu) {
               nir_alu_instr *alu = nir_instr_as_alu(instr);
               switch (alu->op) {
                  case nir_op_fddx:
                     alu->op = nir_op_fddy;
                     progress = true;
                     break;
                  case nir_op_fddx_fine:
                     alu->op = nir_op_fddy_fine;
                     progress = true;
                     break;
                  case nir_op_fddx_coarse:
                     alu->op = nir_op_fddy_coarse;
                     progress = true;
                     break;
                  case nir_op_fddy:
                     alu->op = nir_op_fddx;
                     progress = true;
                     break;
                  case nir_op_fddy_fine:
                     alu->op = nir_op_fddx_fine;
                     progress = true;
                     break;
                  case nir_op_fddy_coarse:
                     alu->op = nir_op_fddx_coarse;
                     progress = true;
                     break;
                  default:
                     break;
               }
            }
         }
      }
      if (progress) {
         nir_metadata_preserve(func->impl, nir_metadata_block_index |
                               nir_metadata_dominance);
      }
   }
   return progress;
}