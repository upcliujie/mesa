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

/* This pass works on nir_variables of tesselation shaders to ensure their
 * input array variables are limited to the number of input vertices
 * (VkPipelineTessellationStateCreateInfo::patchControlPoints).
 */

#include "anv_nir.h"

bool
anv_nir_clamp_per_vertex_input(nir_shader *shader,
                               unsigned input_vertices)
{
   bool progress = false;

   nir_foreach_variable_in_shader(var, shader) {
      if (!nir_is_per_vertex_io(var, shader->info.stage))
         continue;

      const struct glsl_type *type = var->type;
      if (!glsl_type_is_array(type))
         continue;

      int array_size = glsl_array_size(type);
      if (array_size <= input_vertices)
         continue;

      const struct glsl_type *elem_type =
         glsl_get_array_element(type);

      var->type = glsl_array_type(elem_type, input_vertices, 0);

      progress = true;

      nir_foreach_function(function, shader) {
         if (!function->impl)
            continue;

         nir_foreach_block(block, function->impl) {
            nir_foreach_instr(instr, block) {
               if (instr->type != nir_instr_type_deref)
                  continue;

               nir_deref_instr *deref = nir_instr_as_deref(instr);
               if (deref->var != var)
                  continue;

               deref->type = var->type;
            }
         }
      }
   }

   return progress;
}
