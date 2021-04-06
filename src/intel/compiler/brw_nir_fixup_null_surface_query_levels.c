/*
 * Copyright © 2021 Intel Corporation
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

#include "compiler/nir/nir_builtin_builder.h"
#include "brw_nir.h"

/**
 * Wa_1940217:
 *
 * When a surface of type SURFTYPE_NULL is accessed by resinfo, the MIPCount
 * returned is undefined instead of 0.
 *
 * This NIR pass works around this by replacing the obained MIPCount with
 * 0 for all 0 width textures.
 */

bool
brw_nir_fixup_null_surface_query_levels(nir_shader *shader)
{
   bool progress = false;
   nir_builder b;

   nir_foreach_function(func, shader) {
      bool function_progress = false;

      if (!func->impl)
         continue;

      nir_builder_init(&b, func->impl);

      nir_foreach_block(block, func->impl) {
         nir_foreach_instr_safe(instr, block) {
            nir_ssa_def *mip_count = NULL;
            nir_tex_instr *tex_instr;

            switch (instr->type) {
            case nir_instr_type_tex: {
               tex_instr = nir_instr_as_tex(instr);
               if (tex_instr->op != nir_texop_query_levels)
                  break;

               mip_count = &tex_instr->dest.ssa;
               break;
            }

            default:
               break;
            }

            if (!mip_count)
               continue;

            nir_ssa_def *image_size = nir_get_texture_size(&b, tex_instr);

            b.cursor = nir_after_instr(instr);

            nir_ssa_def *zero = nir_imm_int(&b, 0);
            nir_ssa_def *mip_count_fixed =
               nir_bcsel(&b,
                         nir_ieq(&b, nir_channel(&b, image_size, 0), zero),
                         zero,
                         mip_count);

            nir_ssa_def_rewrite_uses_after(mip_count,
                                           mip_count_fixed,
                                           mip_count_fixed->parent_instr);

            function_progress = true;
         }
      }

      if (function_progress) {
         nir_metadata_preserve(func->impl, nir_metadata_block_index |
                                           nir_metadata_dominance);
         progress = function_progress;
      } else {
         nir_metadata_preserve(func->impl, nir_metadata_all);
      }
   }

   return progress;
}
