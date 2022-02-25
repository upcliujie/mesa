/*
 * Copyright © 2022 Intel Corporation
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

#include "anv_private.h"

#include "compiler/brw_compiler.h"
#include "compiler/brw_nir.h"
#include "dev/intel_debug.h"
#include "util/macros.h"

/**
 * This file contains a shader meant to overwrite existing 3DPRIMITIVE
 * instruction with parameters loaded from an indirect buffer.
 */

static nir_shader *
anv_nir_generated_indirect_draws(const struct brw_compiler *compiler,
                                 void *mem_ctx)
{
   const nir_shader_compiler_options *nir_options =
      compiler->nir_options[MESA_SHADER_COMPUTE];

   STATIC_ASSERT(sizeof(struct anv_generated_indirect_draw_params) <= 32);

   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_VERTEX,
                                                  nir_options,
                                                  "Indirect draw generate");
   ralloc_steal(mem_ctx, b.shader);

   nir_variable *indirect_data_var0 =
      nir_variable_create(b.shader,
                          nir_var_shader_in,
                          glsl_uvec4_type(),
                          "indirect_data0");
   nir_variable *indirect_data_var1 =
      nir_variable_create(b.shader,
                          nir_var_shader_in,
                          glsl_uint_type(),
                          "indirect_data1");
   indirect_data_var1->data.location = 1;
   nir_ssa_def *indirect_data0 =
      nir_load_var(&b, indirect_data_var0);
   nir_ssa_def *indirect_data1 =
      nir_load_var(&b, indirect_data_var1);

   nir_ssa_def *base_generated_cmds_addr =
      nir_load_uniform(&b, 1, 64, nir_imm_int(&b, 0),
                       .base = offsetof(struct anv_generated_indirect_draw_params,
                                        generated_cmd_addr),
                       .range = sizeof(uint64_t),
                       .dest_type = nir_type_uint64);
   nir_ssa_def *generated_cmd_stride =
      nir_load_uniform(&b, 1, 32, nir_imm_int(&b, 0),
                       .base = offsetof(struct anv_generated_indirect_draw_params,
                                        generated_cmd_stride),
                       .range = sizeof(uint32_t),
                       .dest_type = nir_type_uint32);
   nir_ssa_def *indexed =
      nir_ieq_imm(
         &b,
         nir_load_uniform(&b, 1, 32, nir_imm_int(&b, 0),
                          .base = offsetof(struct anv_generated_indirect_draw_params,
                                           indexed),
                          .range = sizeof(uint32_t),
                          .dest_type = nir_type_uint32),
         true);
   nir_ssa_def *multiview_multiplier =
         nir_load_uniform(&b, 1, 32, nir_imm_int(&b, 0),
                          .base = offsetof(struct anv_generated_indirect_draw_params,
                                           multiview_multiplier),
                          .range = sizeof(uint32_t),
                          .dest_type = nir_type_uint32);

   nir_ssa_def *index = nir_load_vertex_id(&b);

   nir_ssa_def *generated_cmd_addr =
      nir_iadd(&b,
               base_generated_cmds_addr,
               nir_iadd_imm(&b,
                            nir_imul(&b,
                                     nir_i2i64(&b, generated_cmd_stride),
                                     nir_i2i64(&b, index)),
                            2 * 4 /* dword 2 */));

   nir_ssa_def *instance_count =
      nir_imul(&b, nir_channel(&b, indirect_data0, 1), multiview_multiplier);

   nir_push_if(&b, nir_inot(&b, indexed));
   {
      /* Build the 2 vec4 replacing dwords [2, 5] & [6, 9] in the 3DPRIMTIVE
       * instruction.
       *
       * The indirect input data:
       *
       * typedef struct VkDrawIndirectCommand {
       *   uint32_t    vertexCount;      -> indirect_data_var0
       *   uint32_t    instanceCount;    -> indirect_data_var0
       *   uint32_t    firstVertex;      -> indirect_data_var0
       *   uint32_t    firstInstance;    -> indirect_data_var0
       * } VkDrawIndirectCommand;
       *
       */
      nir_ssa_def *dword_2_5_primitive_instr =
         nir_vec4(&b,
                  nir_channel(&b, indirect_data0, 0), /* Vertex Count Per Instance */
                  nir_channel(&b, indirect_data0, 2), /* Start Vertex Location */
                  instance_count,                     /* Instance Count */
                  nir_channel(&b, indirect_data0, 3)  /* Start Instance Location */);
      nir_ssa_def *dword_6_9_primitive_instr =
         nir_vec4(&b,
                  nir_imm_int(&b, 0),                 /* Base Vertex Location */
                  nir_channel(&b, indirect_data0, 2), /* Extended Parameter 0 / gl_BaseVertex */
                  nir_channel(&b, indirect_data0, 3), /* Extended Parameter 1 / gl_BaseInstance */
                  index);                             /* Extended Parameter 2 / gl_DrawID */

      /* Write the 3DPRIMITIVE instruction in dwords [2, 5] */
      nir_store_global(&b, generated_cmd_addr, 4,
                       dword_2_5_primitive_instr, 0xf /* write_mask */);

      /* Write the 3DPRIMITIVE instruction in dwords [6, 9] */
      nir_store_global(&b, nir_iadd_imm(&b, generated_cmd_addr, 4 * 4), 4,
                       dword_6_9_primitive_instr, 0xf /* write_mask */);
   }
   nir_push_else(&b, NULL);
   {
      /* Build the 2 vec4 replacing dwords [2, 5] & [6, 9] in the 3DPRIMTIVE
       * instruction.
       *
       * The indirect input data:
       *
       * typedef struct VkDrawIndexedIndirectCommand {
       *   uint32_t    indexCount;       -> indirect_data_var0
       *   uint32_t    instanceCount;    -> indirect_data_var0
       *   uint32_t    firstIndex;       -> indirect_data_var0
       *   int32_t     vertexOffset;     -> indirect_data_var0
       *   uint32_t    firstInstance;    -> indirect_data_var1
       * } VkDrawIndexedIndirectCommand;
       */
      nir_ssa_def *dword_2_5_primitive_instr =
         nir_vec4(&b,
                  nir_channel(&b, indirect_data0, 0),  /* Vertex Count Per Instance */
                  nir_channel(&b, indirect_data0, 2),  /* Start Vertex Location */
                  instance_count,                      /* Instance Count */
                  nir_channel(&b, indirect_data1, 0)); /* Start Instance Location */
      nir_ssa_def *dword_6_9_primitive_instr =
         nir_vec4(&b,
                  nir_channel(&b, indirect_data0, 3),  /* Base Vertex Location */
                  nir_channel(&b, indirect_data0, 3),  /* Extended Parameter 0 / gl_BaseVertex */
                  nir_channel(&b, indirect_data1, 0),  /* Extended Parameter 1 / gl_BaseInstance */
                  index);                              /* Extended Parameter 2 / gl_DrawID */

      /* Write the 3DPRIMITIVE instruction in dwords [2, 5] */
      nir_store_global(&b, generated_cmd_addr, 4,
                       dword_2_5_primitive_instr, 0xf /* write_mask */);

      /* Write the 3DPRIMITIVE instruction in dwords [6, 9] */
      nir_store_global(&b, nir_iadd_imm(&b, generated_cmd_addr, 4 * 4), 4,
                       dword_6_9_primitive_instr, 0xf /* write_mask */);
   }
   nir_pop_if(&b, NULL);

   nir_shader *nir = b.shader;
   nir->info.name = ralloc_strdup(nir, "Indirect draw generate");
   nir_validate_shader(nir, "in anv_nir_generated_indirect_draws");
   nir->num_uniforms = sizeof(struct anv_generated_indirect_draw_params);

   return nir;
}

void
anv_device_init_generated_indirect_draws(struct anv_device *device)
{
   if (device->info.ver < 11)
      return;

   struct {
      char name[40];
   } indirect_draws_key = {
      .name = "anv-generated-indirect-draws",
   };

   device->generated_draw_kernel =
      anv_pipeline_cache_search(&device->default_pipeline_cache,
                                &indirect_draws_key,
                                sizeof(indirect_draws_key));
   if (device->generated_draw_kernel != NULL)
      return;

   void *mem_ctx = ralloc_context(NULL);

   nir_shader *nir = anv_nir_generated_indirect_draws(
      device->physical->compiler, mem_ctx);
   assert(nir);

   nir->info.internal = true;
   struct brw_vs_prog_key vs_key;
   memset(&vs_key, 0, sizeof(vs_key));

   struct brw_vs_prog_data vs_prog_data = {
      .base.base.nr_params = nir->num_uniforms / 4,
   };

   struct brw_compiler *compiler = device->physical->compiler;
   brw_preprocess_nir(compiler, nir, NULL);
   nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));

   struct brw_compile_vs_params params = {
      .nir = nir,
      .key = &vs_key,
      .prog_data = &vs_prog_data,
      .log_data = device,
      .debug_flag = DEBUG_VS,
   };
   const unsigned *program = brw_compile_vs(compiler, mem_ctx, &params);

   struct anv_pipeline_bind_map bind_map;
   memset(&bind_map, 0, sizeof(bind_map));

   device->generated_draw_kernel =
      anv_pipeline_cache_upload_kernel(&device->default_pipeline_cache,
                                       nir->info.stage,
                                       &indirect_draws_key,
                                       sizeof(indirect_draws_key),
                                       program,
                                       vs_prog_data.base.base.program_size,
                                       &vs_prog_data.base.base, sizeof(vs_prog_data),
                                       NULL, 0, NULL, &bind_map);

   ralloc_free(mem_ctx);

   const struct intel_l3_weights w =
      intel_get_default_l3_weights(&device->info,
                                   true /* wants_dc_cache */,
                                   false /* needs_slm */);
   device->generated_draw_l3_config = intel_get_l3_config(&device->info, w);
}

void
anv_device_finish_generated_indirect_draws(struct anv_device *device)
{
   if (device->info.ver < 11)
      return;

   if (device->generated_draw_kernel)
      anv_shader_bin_unref(device, device->generated_draw_kernel);
}
