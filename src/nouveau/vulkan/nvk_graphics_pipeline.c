#include "nvk_pipeline.h"

#include "nvk_device.h"
#include "nvk_physical_device.h"
#include "nvk_shader.h"
#include "nv_push.h"
#include "vk_nir.h"
#include "vk_pipeline.h"

#include "nouveau_context.h"

#include "compiler/spirv/nir_spirv.h"

#include "nvk_cl9097.h"
#include "nvk_clb197.h"
#include "nvk_clc397.h"

static void
nvk_populate_fs_key(struct nvk_fs_key *key,
                    const struct vk_multisample_state *ms)
{
   memset(key, 0, sizeof(*key));
   if (ms == NULL || ms->rasterization_samples <= 1)
      return;

   key->msaa = ms->rasterization_samples;
   if (ms->sample_shading_enable &&
       (ms->rasterization_samples * ms->min_sample_shading) > 1.0)
      key->force_per_sample = true;
}

static float
calculate_min_sample_shading(const struct vk_multisample_state *ms,
                             bool force_max_samples)
{
   const float min_sample_shading = force_max_samples ? 1 :
      (ms->sample_shading_enable ? CLAMP(ms->min_sample_shading, 0, 1) : 0);
   return min_sample_shading;
}

static void
emit_pipeline_xfb_state(struct nv_push *p,
                        const struct nvk_transform_feedback_state *xfb)
{
   const uint8_t max_buffers = 4;
   for (uint8_t b = 0; b < max_buffers; ++b) {
      const uint32_t var_count = xfb->varying_count[b];
      P_MTHD(p, NV9097, SET_STREAM_OUT_CONTROL_STREAM(b));
      P_NV9097_SET_STREAM_OUT_CONTROL_STREAM(p, b, xfb->stream[b]);
      P_NV9097_SET_STREAM_OUT_CONTROL_COMPONENT_COUNT(p, b, var_count);
      P_NV9097_SET_STREAM_OUT_CONTROL_STRIDE(p, b, xfb->stride[b]);

      /* upload packed varying indices in multiples of 4 bytes */
      const uint32_t n = (var_count + 3) / 4;
      if (n > 0) {
         P_MTHD(p, NV9097, SET_STREAM_OUT_LAYOUT_SELECT(b, 0));
         P_INLINE_ARRAY(p, (const uint32_t*)xfb->varying_index[b], n);
      }
   }
}

static const uint32_t mesa_to_nv9097_shader_type[] = {
   [MESA_SHADER_VERTEX]    = NV9097_SET_PIPELINE_SHADER_TYPE_VERTEX,
   [MESA_SHADER_TESS_CTRL] = NV9097_SET_PIPELINE_SHADER_TYPE_TESSELLATION_INIT,
   [MESA_SHADER_TESS_EVAL] = NV9097_SET_PIPELINE_SHADER_TYPE_TESSELLATION,
   [MESA_SHADER_GEOMETRY]  = NV9097_SET_PIPELINE_SHADER_TYPE_GEOMETRY,
   [MESA_SHADER_FRAGMENT]  = NV9097_SET_PIPELINE_SHADER_TYPE_PIXEL,
};

static void
emit_tessellation_paramaters(struct nv_push *p,
                             const struct nvk_shader *shader,
                             const struct vk_tessellation_state *state)
{
   const uint32_t cw = NV9097_SET_TESSELLATION_PARAMETERS_OUTPUT_PRIMITIVES_TRIANGLES_CW;
   const uint32_t ccw = NV9097_SET_TESSELLATION_PARAMETERS_OUTPUT_PRIMITIVES_TRIANGLES_CCW;
   uint32_t output_prims = shader->tes.output_prims;
   /* When the origin is lower-left, we have to flip the winding order */
   if (state->domain_origin == VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT) {
      if (output_prims == cw) {
         output_prims = ccw;
      } else if (output_prims == ccw) {
         output_prims = cw;
      }
   }
   P_MTHD(p, NV9097, SET_TESSELLATION_PARAMETERS);
   P_NV9097_SET_TESSELLATION_PARAMETERS(p, {
      shader->tes.domain_type,
      shader->tes.spacing,
      output_prims
   });
}

static void
merge_tess_info(struct shader_info *tes_info, struct shader_info *tcs_info)
{
   /* The Vulkan 1.0.38 spec, section 21.1 Tessellator says:
    *
    *    "PointMode. Controls generation of points rather than triangles
    *     or lines. This functionality defaults to disabled, and is
    *     enabled if either shader stage includes the execution mode.
    *
    * and about Triangles, Quads, IsoLines, VertexOrderCw, VertexOrderCcw,
    * PointMode, SpacingEqual, SpacingFractionalEven, SpacingFractionalOdd,
    * and OutputVertices, it says:
    *
    *    "One mode must be set in at least one of the tessellation
    *     shader stages."
    *
    * So, the fields can be set in either the TCS or TES, but they must
    * agree if set in both.  Our backend looks at TES, so bitwise-or in
    * the values from the TCS.
    */
   assert(tcs_info->tess.tcs_vertices_out == 0 || tes_info->tess.tcs_vertices_out == 0 ||
          tcs_info->tess.tcs_vertices_out == tes_info->tess.tcs_vertices_out);
   tes_info->tess.tcs_vertices_out |= tcs_info->tess.tcs_vertices_out;

   assert(tcs_info->tess.spacing == TESS_SPACING_UNSPECIFIED ||
          tes_info->tess.spacing == TESS_SPACING_UNSPECIFIED ||
          tcs_info->tess.spacing == tes_info->tess.spacing);
   tes_info->tess.spacing |= tcs_info->tess.spacing;

   assert(tcs_info->tess._primitive_mode == TESS_PRIMITIVE_UNSPECIFIED ||
          tes_info->tess._primitive_mode == TESS_PRIMITIVE_UNSPECIFIED ||
          tcs_info->tess._primitive_mode == tes_info->tess._primitive_mode);
   tes_info->tess._primitive_mode |= tcs_info->tess._primitive_mode;
   tes_info->tess.ccw |= tcs_info->tess.ccw;
   tes_info->tess.point_mode |= tcs_info->tess.point_mode;

   /* Copy the merged info back to the TCS */
   tcs_info->tess.tcs_vertices_out = tes_info->tess.tcs_vertices_out;
   tcs_info->tess.spacing = tes_info->tess.spacing;
   tcs_info->tess._primitive_mode = tes_info->tess._primitive_mode;
   tcs_info->tess.ccw = tes_info->tess.ccw;
   tcs_info->tess.point_mode = tes_info->tess.point_mode;
}

VkResult
nvk_graphics_pipeline_create(struct nvk_device *dev,
                             struct vk_pipeline_cache *cache,
                             const VkGraphicsPipelineCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator,
                             VkPipeline *pPipeline)
{
   VK_FROM_HANDLE(vk_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   struct nvk_graphics_pipeline *pipeline;
   VkResult result = VK_SUCCESS;

   pipeline = vk_object_zalloc(&dev->vk, pAllocator, sizeof(*pipeline),
                               VK_OBJECT_TYPE_PIPELINE);
   if (pipeline == NULL)
      return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);

   pipeline->base.type = NVK_PIPELINE_GRAPHICS;

   struct vk_graphics_pipeline_all_state all;
   struct vk_graphics_pipeline_state state = {};
   result = vk_graphics_pipeline_state_fill(&dev->vk, &state, pCreateInfo,
                                            NULL, &all, NULL, 0, NULL);
   assert(result == VK_SUCCESS);

   nir_shader *nir[MESA_SHADER_STAGES] = {};
   struct vk_pipeline_robustness_state robustness[MESA_SHADER_STAGES];

   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      const VkPipelineShaderStageCreateInfo *sinfo = &pCreateInfo->pStages[i];
      gl_shader_stage stage = vk_to_mesa_shader_stage(sinfo->stage);

      vk_pipeline_robustness_state_fill(&dev->vk, &robustness[stage],
                                        pCreateInfo->pNext, sinfo->pNext);

      const nir_shader_compiler_options *nir_options =
         nvk_physical_device_nir_options(pdev, stage);
      const struct spirv_to_nir_options spirv_options =
         nvk_physical_device_spirv_options(pdev, &robustness[stage]);

      result = vk_pipeline_shader_stage_to_nir(&dev->vk, sinfo,
                                               &spirv_options, nir_options,
                                               NULL, &nir[stage]);
      if (result != VK_SUCCESS)
         goto fail;
   }

   if (nir[MESA_SHADER_TESS_CTRL] && nir[MESA_SHADER_TESS_EVAL]) {
      nir_lower_patch_vertices(nir[MESA_SHADER_TESS_EVAL], nir[MESA_SHADER_TESS_CTRL]->info.tess.tcs_vertices_out, NULL);
      merge_tess_info(&nir[MESA_SHADER_TESS_EVAL]->info, &nir[MESA_SHADER_TESS_CTRL]->info);
   }

   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      const VkPipelineShaderStageCreateInfo *sinfo = &pCreateInfo->pStages[i];
      gl_shader_stage stage = vk_to_mesa_shader_stage(sinfo->stage);
      nvk_lower_nir(dev, nir[stage], &robustness[stage],
                    state.rp->view_mask != 0, pipeline_layout->set_layouts,
                    pipeline_layout->set_count);
   }

   for (gl_shader_stage stage = 0; stage < MESA_SHADER_STAGES; stage++) {
      if (nir[stage] == NULL)
         continue;

      struct nvk_fs_key fs_key_tmp, *fs_key = NULL;
      if (stage == MESA_SHADER_FRAGMENT) {
         nvk_populate_fs_key(&fs_key_tmp, state.ms);
         fs_key = &fs_key_tmp;
      }

      result = nvk_compile_nir(pdev, nir[stage], fs_key,
                               &pipeline->base.shaders[stage]);
      ralloc_free(nir[stage]);
      if (result != VK_SUCCESS)
         goto fail;

      result = nvk_shader_upload(dev, &pipeline->base.shaders[stage]);
      if (result != VK_SUCCESS)
         goto fail;
   }

   struct nv_push push;
   nv_push_init(&push, pipeline->push_data, ARRAY_SIZE(pipeline->push_data));
   struct nv_push *p = &push;

   bool force_max_samples = false;

   struct nvk_shader *last_geom = NULL;
   for (gl_shader_stage stage = 0; stage <= MESA_SHADER_FRAGMENT; stage++) {
      struct nvk_shader *shader = &pipeline->base.shaders[stage];
      uint32_t idx = mesa_to_nv9097_shader_type[stage];

      P_IMMD(p, NV9097, SET_PIPELINE_SHADER(idx), {
         .enable  = shader->upload_size > 0,
         .type    = mesa_to_nv9097_shader_type[stage],
      });

      if (shader->upload_size == 0)
         continue;

      if (stage != MESA_SHADER_FRAGMENT)
         last_geom = shader;

      uint64_t addr = nvk_shader_address(shader);
      if (dev->pdev->info.cls_eng3d >= VOLTA_A) {
         P_MTHD(p, NVC397, SET_PIPELINE_PROGRAM_ADDRESS_A(idx));
         P_NVC397_SET_PIPELINE_PROGRAM_ADDRESS_A(p, idx, addr >> 32);
         P_NVC397_SET_PIPELINE_PROGRAM_ADDRESS_B(p, idx, addr);
      } else {
         assert(addr < 0xffffffff);
         P_IMMD(p, NV9097, SET_PIPELINE_PROGRAM(idx), addr);
      }

      P_IMMD(p, NV9097, SET_PIPELINE_REGISTER_COUNT(idx), shader->num_gprs);

      switch (stage) {
      case MESA_SHADER_VERTEX:
      case MESA_SHADER_GEOMETRY:
      case MESA_SHADER_TESS_CTRL:
         break;

      case MESA_SHADER_FRAGMENT:
         P_IMMD(p, NV9097, SET_SUBTILING_PERF_KNOB_A, {
            .fraction_of_spm_register_file_per_subtile         = 0x10,
            .fraction_of_spm_pixel_output_buffer_per_subtile   = 0x40,
            .fraction_of_spm_triangle_ram_per_subtile          = 0x16,
            .fraction_of_max_quads_per_subtile                 = 0x20,
         });
         P_NV9097_SET_SUBTILING_PERF_KNOB_B(p, 0x20);

         P_IMMD(p, NV9097, SET_API_MANDATED_EARLY_Z, shader->fs.early_z);

         if (dev->pdev->info.cls_eng3d >= MAXWELL_B) {
            P_IMMD(p, NVB197, SET_POST_Z_PS_IMASK,
                   shader->fs.post_depth_coverage);
         } else {
            assert(!shader->fs.post_depth_coverage);
         }

         P_MTHD(p, NV9097, SET_ZCULL_BOUNDS);
         P_INLINE_DATA(p, shader->flags[0]);

         /* If we're using the incoming sample mask and doing sample shading,
          * we have to do sample shading "to the max", otherwise there's no
          * way to tell which sets of samples are covered by the current
          * invocation.
          */
         force_max_samples = shader->fs.sample_mask_in ||
                             shader->fs.uses_sample_shading;
         break;

      case MESA_SHADER_TESS_EVAL:
         emit_tessellation_paramaters(p, shader, state.ts);
         break;

      default:
         unreachable("Unsupported shader stage");
      }
   }

   const uint8_t clip_cull = last_geom->vs.clip_enable |
                             last_geom->vs.cull_enable;
   if (clip_cull) {
      P_IMMD(p, NV9097, SET_USER_CLIP_ENABLE, {
         .plane0 = (clip_cull >> 0) & 1,
         .plane1 = (clip_cull >> 1) & 1,
         .plane2 = (clip_cull >> 2) & 1,
         .plane3 = (clip_cull >> 3) & 1,
         .plane4 = (clip_cull >> 4) & 1,
         .plane5 = (clip_cull >> 5) & 1,
         .plane6 = (clip_cull >> 6) & 1,
         .plane7 = (clip_cull >> 7) & 1,
      });
      P_IMMD(p, NV9097, SET_USER_CLIP_OP, {
         .plane0 = (last_geom->vs.cull_enable >> 0) & 1,
         .plane1 = (last_geom->vs.cull_enable >> 1) & 1,
         .plane2 = (last_geom->vs.cull_enable >> 2) & 1,
         .plane3 = (last_geom->vs.cull_enable >> 3) & 1,
         .plane4 = (last_geom->vs.cull_enable >> 4) & 1,
         .plane5 = (last_geom->vs.cull_enable >> 5) & 1,
         .plane6 = (last_geom->vs.cull_enable >> 6) & 1,
         .plane7 = (last_geom->vs.cull_enable >> 7) & 1,
      });
   }

   /* TODO: prog_selects_layer */
   P_IMMD(p, NV9097, SET_RT_LAYER, {
      .v       = 0,
      .control = (last_geom->hdr[13] & (1 << 9)) ?
                 CONTROL_GEOMETRY_SHADER_SELECTS_LAYER :
                 CONTROL_V_SELECTS_LAYER,
   });

   if (last_geom->xfb) {
      emit_pipeline_xfb_state(&push, last_geom->xfb);
   }

   if (state.ms) {
      const float s = calculate_min_sample_shading(state.ms, force_max_samples);
      pipeline->min_sample_shading = s;
   } else {
      pipeline->min_sample_shading = 0.0f;
   }

   pipeline->push_dw_count = nv_push_dw_count(&push);

   pipeline->dynamic.vi = &pipeline->_dynamic_vi;
   pipeline->dynamic.ms.sample_locations = &pipeline->_dynamic_sl;
   vk_dynamic_graphics_state_fill(&pipeline->dynamic, &state);

   *pPipeline = nvk_pipeline_to_handle(&pipeline->base);

   return VK_SUCCESS;

fail:
   vk_object_free(&dev->vk, pAllocator, pipeline);
   return result;
}
