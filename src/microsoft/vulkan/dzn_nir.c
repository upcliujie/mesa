/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <directx/d3d12.h>

#include "spirv_to_dxil.h"
#include "nir_to_dxil.h"
#include "nir_builder.h"

#include "dzn_nir.h"

nir_shader *
dzn_nir_indirect_draw_shader(bool indexed)
{
   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
                                     dxil_get_nir_compiler_options(),
                                     "dzn_meta_indirect_%sdraw()",
                                     indexed ? "indexed_" : "");
   b.shader->info.internal = true;

   nir_variable *uniforms_var =
      nir_variable_create(b.shader, nir_var_mem_ubo, glsl_uint_type(), "uniforms");
   uniforms_var->data.driver_location = uniforms_var->data.binding = 0;
   b.shader->info.num_ubos++;
   nir_variable *draw_buf_var =
      nir_variable_create(b.shader, nir_var_mem_ssbo, glsl_uint_type(), "draw_buf");
   draw_buf_var->data.access = ACCESS_NON_WRITEABLE;
   draw_buf_var->data.driver_location = draw_buf_var->data.binding = 1;
   nir_variable *exec_buf_var =
      nir_variable_create(b.shader, nir_var_mem_ssbo, glsl_uint_type(), "exec_buf");
   exec_buf_var->data.access = ACCESS_NON_READABLE;
   exec_buf_var->data.driver_location = exec_buf_var->data.binding = 2;

   nir_ssa_def *draw_stride =
      nir_load_ubo(&b, 1, 32, nir_imm_int(&b, uniforms_var->data.binding), nir_imm_int(&b, 0),
                   .align_mul = 4, .align_offset = 0, .range_base = 0, .range = ~0);
   nir_ssa_def *exec_stride = nir_imm_int(&b, sizeof(struct dzn_indirect_draw_exec_params));
   nir_ssa_def *index =
      nir_channel(&b, nir_load_global_invocation_id(&b, 32), 0);

   nir_ssa_def *draw_offset = nir_imul(&b, draw_stride, index);
   nir_ssa_def *exec_offset = nir_imul(&b, exec_stride, index);

   nir_ssa_def *draw_info1 =
      nir_load_ssbo(&b, 4, 32, nir_imm_int(&b, draw_buf_var->data.binding),
                    draw_offset, .align_mul = 4);
   nir_ssa_def *draw_info2 =
      indexed ?
      nir_load_ssbo(&b, 1, 32, nir_imm_int(&b, draw_buf_var->data.binding),
                    nir_iadd_imm(&b, draw_offset, 16), .align_mul = 4) :
      nir_imm_int(&b, 0);

   nir_ssa_def *first_vertex = nir_channel(&b, draw_info1, indexed ? 3 : 2);
   nir_ssa_def *base_instance =
      indexed ? draw_info2 : nir_channel(&b, draw_info1, 3);

   nir_ssa_def *exec_vals[] = {
      first_vertex,
      base_instance,
      nir_channel(&b, draw_info1, 0),
      nir_channel(&b, draw_info1, 1),
      nir_channel(&b, draw_info1, 2),
      nir_channel(&b, draw_info1, 3),
      draw_info2,
   };

   assert(sizeof(struct dzn_indirect_draw_exec_params) == (ARRAY_SIZE(exec_vals) * 4));

   nir_store_ssbo(&b, nir_vec(&b, exec_vals, 4),
                  nir_imm_int(&b, exec_buf_var->data.binding), exec_offset,
                  .write_mask = 0xf, .access = ACCESS_NON_READABLE, .align_mul = 4);
   nir_store_ssbo(&b, nir_vec(&b, &exec_vals[4], 3),
                  nir_imm_int(&b, exec_buf_var->data.binding),
                  nir_iadd_imm(&b, exec_offset, 16),
                  .write_mask = 7, .access = ACCESS_NON_READABLE, .align_mul = 4);

   return b.shader;
}
