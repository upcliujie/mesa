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
dzn_nir_indirect_draw_shader(enum dzn_indirect_draw_type type)
{
   const char *type_str[] = {
      "draw",
      "indexed_draw",
      "draw_triangle_fan",
      "indexed_draw_triangle_fan",
   };

   assert(type < ARRAY_SIZE(type_str));

   bool indexed = type == DZN_INDIRECT_INDEXED_DRAW ||
                  type == DZN_INDIRECT_INDEXED_DRAW_TRIANGLE_FAN;
   bool triangle_fan = type == DZN_INDIRECT_DRAW_TRIANGLE_FAN ||
                       type == DZN_INDIRECT_INDEXED_DRAW_TRIANGLE_FAN;
   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
                                     dxil_get_nir_compiler_options(),
                                     "dzn_meta_indirect_%s()",
                                     type_str[type]);
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

   unsigned params_size =
      triangle_fan ?
      sizeof(struct dzn_indirect_draw_triangle_fan_rewrite_params) :
      sizeof(struct dzn_indirect_draw_rewrite_params);

   nir_ssa_def *params =
      nir_load_ubo(&b, params_size / 4, 32,
                   nir_imm_int(&b, uniforms_var->data.binding), nir_imm_int(&b, 0),
                   .align_mul = 4, .align_offset = 0, .range_base = 0, .range = ~0);

   nir_ssa_def *draw_stride = nir_channel(&b, params, 0);
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

   nir_ssa_def *exec_vals[7] = {
      first_vertex,
      base_instance,
   };

   if (triangle_fan) {
      /* Patch {vertex,index}_count and first_index */
      nir_ssa_def *triangle_count =
         nir_usub_sat(&b, nir_channel(&b, draw_info1, 0), nir_imm_int(&b, 2));
      exec_vals[2] = nir_imul_imm(&b, triangle_count, 3);
      exec_vals[3] = nir_channel(&b, draw_info1, 1);
      exec_vals[4] = nir_imm_int(&b, 0);
      exec_vals[5] = indexed ? nir_channel(&b, draw_info1, 3) : nir_imm_int(&b, 0);
      exec_vals[6] = indexed ? draw_info2 : nir_channel(&b, draw_info1, 3);

      nir_variable *triangle_fan_exec_buf_var =
         nir_variable_create(b.shader, nir_var_mem_ssbo, glsl_uint_type(), "triangle_fan_exec_buf");
      triangle_fan_exec_buf_var->data.access = ACCESS_NON_READABLE;
      triangle_fan_exec_buf_var->data.driver_location = triangle_fan_exec_buf_var->data.binding = 3;
      nir_ssa_def *triangle_fan_index_buf_stride = nir_channel(&b, params, 1);
      nir_ssa_def *triangle_fan_index_buf_addr_lo =
         nir_iadd(&b, nir_channel(&b, params, 2),
                  nir_imul(&b, triangle_fan_index_buf_stride, index));
      nir_ssa_def *addr_lo_overflow =
         nir_ult(&b, triangle_fan_index_buf_addr_lo, nir_channel(&b, params, 2));
      nir_ssa_def *triangle_fan_index_buf_addr_hi =
         nir_iadd(&b, nir_channel(&b, params, 3),
                  nir_bcsel(&b, addr_lo_overflow, nir_imm_int(&b, 1), nir_imm_int(&b, 0)));

      nir_ssa_def *triangle_fan_exec_vals[] = {
         triangle_fan_index_buf_addr_lo,
         triangle_fan_index_buf_addr_hi,
         nir_channel(&b, draw_info1, 2),
         triangle_count,
	 nir_imm_int(&b, 1),
	 nir_imm_int(&b, 1),
      };

      assert(sizeof(struct dzn_indirect_triangle_fan_rewrite_index_exec_params) == (ARRAY_SIZE(triangle_fan_exec_vals) * 4));

      nir_ssa_def *triangle_fan_exec_stride =
         nir_imm_int(&b, sizeof(struct dzn_indirect_triangle_fan_rewrite_index_exec_params));
      nir_ssa_def *triangle_fan_exec_offset =
         nir_imul(&b, triangle_fan_exec_stride, index);

      nir_store_ssbo(&b, nir_vec(&b, &triangle_fan_exec_vals[0], 4),
                     nir_imm_int(&b, triangle_fan_exec_buf_var->data.binding),
                     triangle_fan_exec_offset,
                     .write_mask = 0xf, .access = ACCESS_NON_READABLE, .align_mul = 4);
      nir_store_ssbo(&b, nir_vec(&b, &triangle_fan_exec_vals[4], 2),
                     nir_imm_int(&b, triangle_fan_exec_buf_var->data.binding),
                     nir_iadd_imm(&b, triangle_fan_exec_offset, 16),
                     .write_mask = 0x3, .access = ACCESS_NON_READABLE, .align_mul = 4);

      nir_ssa_def *ibview_vals[] = {
         triangle_fan_index_buf_addr_lo,
         triangle_fan_index_buf_addr_hi,
         triangle_fan_index_buf_stride,
         nir_imm_int(&b, DXGI_FORMAT_R32_UINT),
      };

      nir_store_ssbo(&b, nir_vec(&b, ibview_vals, ARRAY_SIZE(ibview_vals)),
                     nir_imm_int(&b, exec_buf_var->data.binding), exec_offset,
                     .write_mask = 0x3, .access = ACCESS_NON_READABLE, .align_mul = 4);
      exec_offset = nir_iadd_imm(&b, exec_offset, ARRAY_SIZE(ibview_vals) * 4);
   } else {
      exec_vals[2] = nir_channel(&b, draw_info1, 0);
      exec_vals[3] = nir_channel(&b, draw_info1, 1);
      exec_vals[4] = nir_channel(&b, draw_info1, 2);
      exec_vals[5] = nir_channel(&b, draw_info1, 3);
      exec_vals[6] = draw_info2;
   }

   nir_store_ssbo(&b, nir_vec(&b, exec_vals, 4),
                  nir_imm_int(&b, exec_buf_var->data.binding), exec_offset,
                  .write_mask = 0xf, .access = ACCESS_NON_READABLE, .align_mul = 4);
   nir_store_ssbo(&b, nir_vec(&b, &exec_vals[4], 3),
                  nir_imm_int(&b, exec_buf_var->data.binding),
                  nir_iadd_imm(&b, exec_offset, 16),
                  .write_mask = 7, .access = ACCESS_NON_READABLE, .align_mul = 4);


   return b.shader;
}

nir_shader *
dzn_nir_triangle_fan_rewrite_index_shader(uint8_t old_index_size)
{
   assert(old_index_size == 0 || old_index_size == 2 || old_index_size == 4);

   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
                                     dxil_get_nir_compiler_options(),
                                     "dzn_meta_triangle_rewrite_index(old_index_size=%d)",
                                     old_index_size);
   b.shader->info.internal = true;

   nir_variable *params_var =
      nir_variable_create(b.shader, nir_var_mem_ubo, glsl_uint_type(), "params");
   params_var->data.driver_location = params_var->data.binding = 0;
   b.shader->info.num_ubos++;
   nir_variable *new_index_buf_var =
      nir_variable_create(b.shader, nir_var_mem_ssbo, glsl_uint_type(), "new_index_buf");
   new_index_buf_var->data.access = ACCESS_NON_READABLE;
   new_index_buf_var->data.driver_location = new_index_buf_var->data.binding = 1;

   nir_variable *old_index_buf_var = NULL;
   if (old_index_size > 0) {
      old_index_buf_var =
         nir_variable_create(b.shader, nir_var_mem_ssbo, glsl_uint_type(), "old_index_buf");
      old_index_buf_var->data.access = ACCESS_NON_WRITEABLE;
      old_index_buf_var->data.driver_location = old_index_buf_var->data.binding = 2;
   }

   nir_ssa_def *params =
      nir_load_ubo(&b, sizeof(struct dzn_triangle_fan_rewrite_index_params) / 4, 32,
                   nir_imm_int(&b, params_var->data.binding), nir_imm_int(&b, 0),
                   .align_mul = 4, .align_offset = 0, .range_base = 0, .range = ~0);

   nir_ssa_def *triangle = nir_channel(&b, nir_load_global_invocation_id(&b, 32), 0);
   nir_ssa_def *new_indices;

   if (old_index_size > 0) {
      nir_ssa_def *old_first_index = nir_channel(&b, params, 0);
      nir_ssa_def *old_index0_offset =
         nir_imul_imm(&b, old_first_index, old_index_size);
      nir_ssa_def *old_index1_offset =
         nir_imul_imm(&b, nir_iadd(&b, nir_iadd_imm(&b, triangle, 1), old_first_index),
                      old_index_size);

      nir_ssa_def *old_index0 =
         nir_load_ssbo(&b, 1, 32,
                       nir_imm_int(&b, old_index_buf_var->data.binding),
                       old_index_size == 2 ? nir_iand_imm(&b, old_index0_offset, ~3ULL) : old_index0_offset,
                       .align_mul = 4);

      if (old_index_size == 2) {
         old_index0 =
            nir_bcsel(&b,
                      nir_ieq_imm(&b, nir_iand_imm(&b, old_index0_offset, 0x2), 0),
                      nir_iand_imm(&b, old_index0, 0xffff),
                      nir_ushr_imm(&b, old_index0, 16));
      }

      nir_ssa_def *old_index12 =
         nir_load_ssbo(&b, 2, 32,
                       nir_imm_int(&b, old_index_buf_var->data.binding),
                       old_index_size == 2 ? nir_iand_imm(&b, old_index1_offset, ~3ULL) : old_index1_offset,
                       .align_mul = 4);
      if (old_index_size == 2) {
         nir_ssa_def *indices[] = {
            nir_iand_imm(&b, nir_channel(&b, old_index12, 0), 0xffff),
            nir_ushr_imm(&b, nir_channel(&b, old_index12, 0), 16),
            nir_iand_imm(&b, nir_channel(&b, old_index12, 1), 0xffff),
         };

         old_index12 =
            nir_bcsel(&b,
                      nir_ieq_imm(&b, nir_iand_imm(&b, old_index1_offset, 0x2), 0),
                      nir_vec2(&b, indices[0], indices[1]),
                      nir_vec2(&b, indices[1], indices[2]));
      }

      /* TODO: VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT */
      new_indices =
         nir_vec3(&b, nir_channel(&b, old_index12, 0),
                  nir_channel(&b, old_index12, 1), old_index0);
   } else {
      nir_ssa_def *first_vertex = nir_channel(&b, params, 0);

      new_indices = 
         nir_vec3(&b,
                  nir_iadd(&b, nir_iadd_imm(&b, triangle, 1), first_vertex),
                  nir_iadd(&b, nir_iadd_imm(&b, triangle, 2), first_vertex),
                  first_vertex);
   }

   nir_ssa_def *new_index_offset =
      nir_imul_imm(&b, triangle, 4 * 3);

   nir_store_ssbo(&b, new_indices,
                  nir_imm_int(&b, new_index_buf_var->data.binding),
                  new_index_offset,
                  .write_mask = 7, .access = ACCESS_NON_READABLE, .align_mul = 4);

   return b.shader;
}

nir_shader *
dzn_nir_blit_vs(void)
{
   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_VERTEX,
                                     dxil_get_nir_compiler_options(),
                                     "dzn_meta_blit_vs()");
   b.shader->info.internal = true;

   nir_variable *params_var =
      nir_variable_create(b.shader, nir_var_mem_ubo, glsl_float_type(), "params");
   params_var->data.driver_location = params_var->data.binding = 0;
   b.shader->info.num_ubos++;

   nir_variable *out_pos =
      nir_variable_create(b.shader, nir_var_shader_out, glsl_vec4_type(),
                          "gl_Position");
   out_pos->data.location = VARYING_SLOT_POS;
   out_pos->data.driver_location = 0;

   nir_variable *out_coords =
      nir_variable_create(b.shader, nir_var_shader_out, glsl_vec_type(3),
                          "coords");
   out_coords->data.location = VARYING_SLOT_TEX0;
   out_coords->data.driver_location = 1;

   nir_ssa_def *vertex = nir_load_vertex_id(&b);
   nir_ssa_def *base = nir_imul_imm(&b, vertex, 4 * sizeof(float));
   nir_ssa_def *coords =
      nir_load_ubo(&b, 4, 32, nir_imm_int(&b, 0), base,
                   .align_mul = 16, .align_offset = 0, .range_base = 0, .range = ~0);
   nir_ssa_def *pos =
      nir_vec4(&b, nir_channel(&b, coords, 0), nir_channel(&b, coords, 1),
               nir_imm_float(&b, 0.0), nir_imm_float(&b, 1.0));
   nir_ssa_def *z_coord =
      nir_load_ubo(&b, 1, 32, nir_imm_int(&b, 0), nir_imm_int(&b, 4 * 4 * sizeof(float)),
                   .align_mul = 64, .align_offset = 0, .range_base = 0, .range = ~0);
   coords = nir_vec3(&b, nir_channel(&b, coords, 2), nir_channel(&b, coords, 3), z_coord);

   nir_store_var(&b, out_pos, pos, 0xf);
   nir_store_var(&b, out_coords, coords, 0x7);
   return b.shader;
}

nir_shader *
dzn_nir_blit_fs(const struct dzn_nir_blit_info *info)
{
   bool ms = info->src_samples > 1;
   nir_alu_type nir_out_type =
      nir_get_nir_type_for_glsl_base_type(info->out_type);
   uint32_t coord_comps =
      glsl_get_sampler_dim_coordinate_components(info->sampler_dim) +
      info->src_is_array;

   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,
                                     dxil_get_nir_compiler_options(),
                                     "dzn_meta_blit_fs()");
   b.shader->info.internal = true;

   const struct glsl_type *tex_type =
      glsl_texture_type(info->sampler_dim, info->src_is_array, info->out_type);
   nir_variable *tex_var =
      nir_variable_create(b.shader, nir_var_uniform, tex_type, "texture");

   nir_variable *pos_var =
      nir_variable_create(b.shader, nir_var_shader_in,
                          glsl_vector_type(GLSL_TYPE_FLOAT, 4),
                          "gl_FragCoord");
   pos_var->data.location = VARYING_SLOT_POS;
   pos_var->data.driver_location = 0;

   nir_variable *coord_var =
      nir_variable_create(b.shader, nir_var_shader_in,
                          glsl_vector_type(GLSL_TYPE_FLOAT, 3),
                          "coord");
   coord_var->data.location = VARYING_SLOT_TEX0;
   coord_var->data.driver_location = 1;
   nir_ssa_def *coord =
      nir_channels(&b, nir_load_var(&b, coord_var), (1 << coord_comps) - 1);

   uint32_t out_comps =
      (info->loc == FRAG_RESULT_DEPTH || info->loc == FRAG_RESULT_STENCIL) ? 1 : 4;
   nir_variable *out =
      nir_variable_create(b.shader, nir_var_shader_out,
                          glsl_vector_type(info->out_type, out_comps),
                          "out");
   out->data.location = info->loc;

   nir_ssa_def *res = NULL;

   if (info->resolve) {
      /* When resolving a float type, we need to calculate the average of all
       * samples. For integer resolve, Vulkan says that one sample should be
       * chosen without telling which. Let's just pick the first one in that
       * case.
       */

      unsigned nsamples = info->out_type == GLSL_TYPE_FLOAT ?
                          info->src_samples : 1;
      for (unsigned s = 0; s < nsamples; s++) {
         nir_tex_instr *tex = nir_tex_instr_create(b.shader, 3);

	 tex->op = nir_texop_txf_ms;
	 tex->dest_type = nir_out_type;
	 tex->texture_index = 0;
	 tex->is_array = info->src_is_array;
	 tex->sampler_dim = info->sampler_dim;

         tex->src[0].src_type = nir_tex_src_coord;
         tex->src[0].src = nir_src_for_ssa(nir_f2i32(&b, coord));
         tex->coord_components = coord_comps;

         tex->src[1].src_type = nir_tex_src_ms_index;
         tex->src[1].src = nir_src_for_ssa(nir_imm_int(&b, s));

         tex->src[2].src_type = nir_tex_src_lod;
         tex->src[2].src = nir_src_for_ssa(nir_imm_int(&b, 0));
         nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, NULL);

         nir_builder_instr_insert(&b, &tex->instr);
         res = res ? nir_fadd(&b, res, &tex->dest.ssa) : &tex->dest.ssa;
      }

      if (nsamples > 1) {
         unsigned type_sz = nir_alu_type_get_type_size(nir_out_type);
         res = nir_fmul(&b, res, nir_imm_floatN_t(&b, 1.0f / nsamples, type_sz));
      }
   } else {
      nir_tex_instr *tex =
         nir_tex_instr_create(b.shader, ms ? 3 : 1);

      tex->dest_type = nir_out_type;
      tex->texture_index = 0;
      tex->is_array = info->src_is_array;
      tex->sampler_dim = info->sampler_dim;

      if (ms) {
         tex->op = nir_texop_txf_ms;

         tex->src[0].src_type = nir_tex_src_coord;
         tex->src[0].src = nir_src_for_ssa(nir_f2i32(&b, coord));
         tex->coord_components = coord_comps;

         tex->src[1].src_type = nir_tex_src_ms_index;
         tex->src[1].src = nir_src_for_ssa(nir_load_sample_id(&b));

         tex->src[2].src_type = nir_tex_src_lod;
         tex->src[2].src = nir_src_for_ssa(nir_imm_int(&b, 0));
      } else {
         nir_variable *sampler_var =
            nir_variable_create(b.shader, nir_var_uniform, glsl_bare_sampler_type(), "sampler");

         tex->op = nir_texop_tex;
         tex->sampler_index = 0;

         tex->src[0].src_type = nir_tex_src_coord;
         tex->src[0].src = nir_src_for_ssa(coord);
         tex->coord_components = coord_comps;
      }

      nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, NULL);
      nir_builder_instr_insert(&b, &tex->instr);
      res = &tex->dest.ssa;
   }

   nir_store_var(&b, out, nir_channels(&b, res, (1 << out_comps) - 1), 0xf);

   return b.shader;
}
