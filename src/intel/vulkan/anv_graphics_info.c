/*
 * Copyright © 2015 Intel Corporation
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

#include <assert.h>

#include "anv_private.h"

#include "vk_deepcopy.h"

/**
 * This file implements gathering information from a
 * VkGraphicsPipelineCreateInfo for graphics pipeline libraries.
 *
 * It copies structure using ralloc in a given mem_ctx and discards anything
 * that should be ignored due do dynamic states or other conditions as
 * described in the Vulkan specification.
 */

static void
anv_graphics_pipeline_clone_rp_info(struct anv_graphics_pipeline_base *pipeline,
                                    struct anv_graphics_pipeline_info *info,
                                    void *mem_ctx,
                                    const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   /* We'll use these as defaults if we don't have pipeline rendering or
    * self-dependency structs. Saves us some NULL checks.
    */
   info->_rsd = (VkRenderingSelfDependencyInfoMESA) {
      .sType = VK_STRUCTURE_TYPE_RENDERING_SELF_DEPENDENCY_INFO_MESA,
   };
   info->_ri = (VkPipelineRenderingCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = &info->_rsd,
   };

   info->ri = vk_get_pipeline_rendering_create_info(pCreateInfo);
   if (info->ri)
      info->ri = vk_PipelineRenderingCreateInfo_deepcopy(mem_ctx, info->ri);
   else
      info->ri = &info->_ri;

   info->rsd =
      vk_find_struct_const(info->ri->pNext,
                           RENDERING_SELF_DEPENDENCY_INFO_MESA);
   if (info->rsd)
      info->_rsd = *info->rsd;
   info->rsd = &info->_rsd;

   pipeline->view_mask = info->ri->viewMask;
}

static bool
is_depth_stencil_attachment_used(struct anv_graphics_pipeline_info *info)
{
   return info->ri->depthAttachmentFormat != VK_FORMAT_UNDEFINED ||
          info->ri->stencilAttachmentFormat != VK_FORMAT_UNDEFINED;
}

static bool
is_color_attachment_used(struct anv_graphics_pipeline_info *info)
{
   for (unsigned i = 0; i < info->ri->colorAttachmentCount; i++) {
      if (info->ri->pColorAttachmentFormats[i] != VK_FORMAT_UNDEFINED)
         return true;
   }
   return false;
}

static void
deep_clone_vertex_input_info(struct anv_graphics_pipeline_base *pipeline,
                             struct anv_graphics_pipeline_info *info,
                             void *mem_ctx,
                             const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   /* VkGraphicsPipelineCreateInfo::pVertexInputState:
    * VkGraphicsPipelineCreateInfo::pInputAssemblyState:
    *
    *  "It is ignored if the pipeline includes a mesh shader stage."
    */
   if (pipeline->active_stages & VK_SHADER_STAGE_MESH_BIT_NV)
      return;

   /* TODO: if we support VK_EXT_vertex_input_dynamic_state
    *
    *  "It is ignored if the pipeline is created with the
    *   VK_DYNAMIC_STATE_VERTEX_INPUT_EXT dynamic state set"
    */
   info->vi =
      vk_PipelineVertexInputStateCreateInfo_deepcopy(mem_ctx,
                                                     pCreateInfo->pVertexInputState);

   info->ia =
      vk_PipelineInputAssemblyStateCreateInfo_deepcopy(mem_ctx,
                                                       pCreateInfo->pInputAssemblyState);
}

static VkPipelineViewportStateCreateInfo *
deep_clone_PipelineViewportStateCreateInfo(struct anv_graphics_pipeline_base *pipeline,
                                           void *mem_ctx,
                                           const VkPipelineViewportStateCreateInfo *in)
{
   VkPipelineViewportStateCreateInfo *out = rzalloc_size(mem_ctx, sizeof(*in));

   out->sType = in->sType;
   out->flags = in->flags;

   /* VkGraphicsPipelineCreateInfo::pViewportState:
    *
    *  "If the pipeline is being created with pre-rasterization shader state,
    *   and no element of the pDynamicStates member of pDynamicState is
    *   VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, the
    *   pViewports member of pViewportState must be a valid pointer to an
    *   array of pViewportState->viewportCount valid VkViewport structures"
    */
   if (!(pipeline->dynamic_states & ANV_CMD_DIRTY_DYNAMIC_VIEWPORT)) {
      out->viewportCount = in->viewportCount;
      out->pViewports = vk_Viewport_copy_array(mem_ctx, in->pViewports, in->viewportCount);
   }

   /* VkGraphicsPipelineCreateInfo::pViewportState:
    *
    *  "If the pipeline is being created with pre-rasterization shader state,
    *   and no element of the pDynamicStates member of pDynamicState is
    *   VK_DYNAMIC_STATE_SCISSOR or VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT, the
    *   pScissors member of pViewportState must be a valid pointer to an array
    *   of pViewportState->scissorCount VkRect2D structures"
    */
   if (!(pipeline->dynamic_states & ANV_CMD_DIRTY_DYNAMIC_SCISSOR)) {
      out->scissorCount = in->scissorCount;
      out->pScissors = vk_Rect2D_copy_array(mem_ctx, in->pScissors, in->scissorCount);
   }

   vk_foreach_struct_const(iter, in->pNext) {
      if (iter->sType == VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT) {
         void *copy = vk_PipelineViewportDepthClipControlCreateInfoEXT_deepcopy(mem_ctx, (const VkPipelineViewportDepthClipControlCreateInfoEXT *) iter);
         __vk_append_struct(out, copy);
      }
   }


   return out;
}

static void
deep_clone_pre_raster_info(struct anv_graphics_pipeline_base *pipeline,
                           struct anv_graphics_pipeline_info *info,
                           void *mem_ctx,
                           const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineFragmentShadingRateStateCreateInfoKHR *fsr_info =
      vk_find_struct_const(pCreateInfo->pNext,
                           PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR);
   const VkShaderStageFlagBits tess_stages =
      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

   anv_graphics_pipeline_clone_rp_info(pipeline, info, mem_ctx, pCreateInfo);

   /* VkGraphicsPipelineCreateInfo::pTessellationState:
    *
    *  "pTessellationState is a pointer to a
    *   VkPipelineTessellationStateCreateInfo structure, and is ignored if the
    *   pipeline does not include a tessellation control shader stage and
    *   tessellation evaluation shader stage."
    */
   if ((pipeline->active_stages & tess_stages) == tess_stages) {
      info->ts =
         vk_PipelineTessellationStateCreateInfo_deepcopy(mem_ctx,
                                                         pCreateInfo->pTessellationState);
   }

   /* VkGraphicsPipelineCreateInfo::pRasterizationState:
    *
    *  "pRasterizationState must be a valid pointer to a valid
    *   VkPipelineRasterizationStateCreateInfo structure"
    */
   assert(pCreateInfo->pRasterizationState);
   info->rs =
      vk_PipelineRasterizationStateCreateInfo_deepcopy(mem_ctx,
                                                       pCreateInfo->pRasterizationState);

   /* VkGraphicsPipelineCreateInfo::pViewportState
    *
    *  "pViewportState is a pointer to a VkPipelineViewportStateCreateInfo
    *   structure, and is ignored if the pipeline has rasterization disabled."
    *
    *  "If the pipeline is being created with pre-rasterization shader state,
    *   and the graphics pipeline state was created with the
    *   VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE dynamic state enabled,
    *   pViewportState must be a valid pointer to a valid
    *   VkPipelineViewportStateCreateInfo structure"
    */
   if (!info->rs->rasterizerDiscardEnable ||
       (pipeline->dynamic_states & ANV_CMD_DIRTY_DYNAMIC_RASTERIZER_DISCARD_ENABLE)) {
      assert(pCreateInfo->pViewportState);
      info->vp = deep_clone_PipelineViewportStateCreateInfo(pipeline, mem_ctx,
                                                            pCreateInfo->pViewportState);
   }

   /* VkPipelineFragmentShadingRateStateCreateInfoKHR is not required to be
    * present but we should ignore it if it's a dynamic state.
    */
   if ((pipeline->dynamic_states & ANV_CMD_DIRTY_DYNAMIC_SHADING_RATE) != 0 &&
       fsr_info) {
      info->fsr =
         vk_PipelineFragmentShadingRateStateCreateInfoKHR_deepcopy(mem_ctx,
                                                                   fsr_info);
   }
}

static bool
is_rasterization_enabled(const struct anv_graphics_pipeline_base *pipeline,
                         const struct anv_graphics_pipeline_info *info)
{
   /* It's dynamic, so we have to assume rasterization can be enabled. */
   if (pipeline->dynamic_states & ANV_CMD_DIRTY_DYNAMIC_RASTERIZER_DISCARD_ENABLE)
      return true;

   /* It's not specified because we're dealing with a pipeline library that
    * doesn't have the pre-raster part.
    */
   if (!info->rs)
      return true;

   return !info->rs->rasterizerDiscardEnable;
}

static bool
has_fragment_shader_per_sample_variable(const struct anv_graphics_pipeline_base *pipeline)
{
   if (!pipeline->shaders[MESA_SHADER_FRAGMENT])
      return false;

   const struct brw_wm_prog_data *wm_prog_data =
      get_wm_prog_data((const struct anv_graphics_pipeline *)pipeline);

   return wm_prog_data->sample_shading;
}

static void
deep_clone_fragment_info(struct anv_graphics_pipeline_base *pipeline,
                         struct anv_graphics_pipeline_info *info,
                         void *mem_ctx,
                         const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineFragmentShadingRateStateCreateInfoKHR *fsr_info =
      vk_find_struct_const(pCreateInfo->pNext,
                           PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR);

   anv_graphics_pipeline_clone_rp_info(pipeline, info, mem_ctx, pCreateInfo);

   /* VkGraphicsPipelineCreateInfo::pMultisampleState:
    *
    *  "pMultisampleState is a pointer to a
    *   VkPipelineMultisampleStateCreateInfo structure, and is ignored if the
    *   pipeline has rasterization disabled."
    *
    *  "If the pipeline is being created with fragment shader state with a
    *   fragment shader that either enables sample shading or decorates any
    *   variable in the code:Input storage class with code:Sample, then
    *   pname:pMultisampleState must: not be `NULL`"
    */
   if (!is_rasterization_enabled(pipeline, info) &&
       !has_fragment_shader_per_sample_variable(pipeline)) {
      assert(pCreateInfo->pMultisampleState);
      info->ms =
         vk_PipelineMultisampleStateCreateInfo_deepcopy(mem_ctx,
                                                        pCreateInfo->pMultisampleState);
   }

   /* VkGraphicsPipelineCreateInfo::pDepthStencilState:
    *
    *  "pDepthStencilState is a pointer to a
    *   VkPipelineDepthStencilStateCreateInfo structure, and is ignored if the
    *   pipeline has rasterization disabled or if no depth/stencil attachment
    *   is used."
    */
   if (is_rasterization_enabled(pipeline, info) &&
       is_depth_stencil_attachment_used(info)) {
      assert(pCreateInfo->pDepthStencilState);
      info->ds =
         vk_PipelineDepthStencilStateCreateInfo_deepcopy(mem_ctx,
                                                         pCreateInfo->pDepthStencilState);
   }

   /* VkPipelineFragmentShadingRateStateCreateInfoKHR is not required to be
    * present but we should ignore it if it's a dynamic state.
    *
    * Also don't copy again if already specified by another part of the
    * pipeline.
    */
   if (!(pipeline->dynamic_states & ANV_CMD_DIRTY_DYNAMIC_SHADING_RATE) &&
       fsr_info &&
       !info->fsr) {
      info->fsr =
         vk_PipelineFragmentShadingRateStateCreateInfoKHR_deepcopy(mem_ctx,
                                                                   fsr_info);
   }
}

static void
deep_clone_output_info(struct anv_graphics_pipeline_base *pipeline,
                       struct anv_graphics_pipeline_info *info,
                       void *mem_ctx,
                       const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   anv_graphics_pipeline_clone_rp_info(pipeline, info, mem_ctx, pCreateInfo);

   /* VkGraphicsPipelineCreateInfo::pMultisampleState:
    *
    *  "pMultisampleState is a pointer to a
    *   VkPipelineMultisampleStateCreateInfo structure, and is ignored if the
    *   pipeline has rasterization disabled."
    *
    *  "If the pipeline is being created with fragment shader state,
    *   pMultisampleState must be a valid pointer to a valid
    *   VkPipelineMultisampleStateCreateInfo structure"
    */
   if (is_rasterization_enabled(pipeline, info) ||
       (pipeline->active_stages & VK_SHADER_STAGE_FRAGMENT_BIT)) {
      assert(pCreateInfo->pMultisampleState);
      info->ms =
         vk_PipelineMultisampleStateCreateInfo_deepcopy(mem_ctx,
                                                        pCreateInfo->pMultisampleState);
   }

   /* VkGraphicsPipelineCreateInfo::pColorBlendState:
    *
    *  "pColorBlendState is a pointer to a VkPipelineColorBlendStateCreateInfo
    *   structure, and is ignored if the pipeline has rasterization disabled or
    *   if no color attachments are used."
    */
   if (is_rasterization_enabled(pipeline, info) &&
       is_color_attachment_used(info)) {
      assert(pCreateInfo->pColorBlendState);
      info->cb =
         vk_PipelineColorBlendStateCreateInfo_deepcopy(mem_ctx,
                                                       pCreateInfo->pColorBlendState);
   }
}

static VkGraphicsPipelineLibraryFlagBitsEXT
shader_stage_to_pipeline_library_flags(VkShaderStageFlagBits stage)
{
   assert(util_bitcount(stage) == 1);
   switch (stage) {
   case VK_SHADER_STAGE_VERTEX_BIT:
   case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
   case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
   case VK_SHADER_STAGE_GEOMETRY_BIT:
      return VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

   case VK_SHADER_STAGE_FRAGMENT_BIT:
      return VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

   default:
      unreachable("Invalid shader stage");
   }
}

/* For a given dynamic state, tells you what graphics pipeline library block
 * is impacted.
 */
static VkGraphicsPipelineLibraryFlagBitsEXT
anv_dynamic_state_graphics_library_flags(VkDynamicState state)
{
   switch (state) {
   case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE:
   case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE:
   case VK_DYNAMIC_STATE_VERTEX_INPUT_EXT:
      return VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

   case VK_DYNAMIC_STATE_VIEWPORT:
   case VK_DYNAMIC_STATE_SCISSOR:
   case VK_DYNAMIC_STATE_LINE_WIDTH:
   case VK_DYNAMIC_STATE_DEPTH_BIAS:
   case VK_DYNAMIC_STATE_CULL_MODE:
   case VK_DYNAMIC_STATE_FRONT_FACE:
   case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:
   case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT:
   case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:
   case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:
   case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE:
   case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:
   case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE:
   case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:
   case VK_DYNAMIC_STATE_STENCIL_OP:
   case VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE:
   case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE:
   case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV:
   case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT:
   case VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV:
   case VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV:
   case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV:
   case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
   case VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT:
      return VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

   case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
   case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
   case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
   case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
      return VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

   case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
   case VK_DYNAMIC_STATE_LOGIC_OP_EXT:
   case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:
      return VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

   case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
   case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
      return VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
             VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

   case VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR:
      return 0;

   default:
      unreachable("Missing case");
   }
}

void
anv_graphics_pipeline_import_info(struct anv_graphics_pipeline_base *pipeline,
                                  struct anv_graphics_pipeline_info *info,
                                  void *mem_ctx,
                                  const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                  VkGraphicsPipelineLibraryFlagBitsEXT lib_flags)
{

   pipeline->lib_flags |= lib_flags;

   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      if (shader_stage_to_pipeline_library_flags(pCreateInfo->pStages[i].stage) & lib_flags)
         pipeline->active_stages |= pCreateInfo->pStages[i].stage;
   }
   if (pipeline->active_stages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
      pipeline->active_stages |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

   if (pCreateInfo->pDynamicState) {
      /* Remove all of the states that are marked as dynamic */
      uint32_t count = pCreateInfo->pDynamicState->dynamicStateCount;
      for (uint32_t s = 0; s < count; s++) {
         VkDynamicState state = pCreateInfo->pDynamicState->pDynamicStates[s];

         /* Discard states we don't care about for this pipeline. */
         if ((anv_dynamic_state_graphics_library_flags(state) & lib_flags) == 0)
            continue;

         pipeline->dynamic_states |= anv_cmd_dirty_bit_for_vk_dynamic_state(
            pCreateInfo->pDynamicState->pDynamicStates[s]);
      }
   }

   if (lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
      deep_clone_vertex_input_info(pipeline, info, mem_ctx, pCreateInfo);

   if (lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
      deep_clone_pre_raster_info(pipeline, info, mem_ctx, pCreateInfo);

   if (lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
      deep_clone_fragment_info(pipeline, info, mem_ctx, pCreateInfo);

   if (lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
      deep_clone_output_info(pipeline, info, mem_ctx, pCreateInfo);

   ANV_FROM_HANDLE(anv_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   struct anv_pipeline_sets_layout *layout =
      pipeline_layout ? &pipeline_layout->sets_layout : NULL;

   if (lib_flags == ALL_GRAPHICS_LIB_FLAGS) {
      anv_pipeline_sets_layout_fini(&pipeline->base.layout);
      anv_pipeline_sets_layout_init(&pipeline->base.layout,
                                    pipeline->base.device,
                                    false /* independent_sets */);
   }

   if (layout) {
      /* As explained in the specification, the application can provide a non
       * compatible pipeline layout when doing optimized linking :
       *
       *    "However, in the specific case that a final link is being
       *     performed between stages and
       *     `VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT` is specified,
       *     the application can override the pipeline layout with one that is
       *     compatible with that union but does not have the
       *     `VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT` flag set,
       *     allowing a more optimal pipeline layout to be used when
       *     generating the final pipeline."
       *
       * In that case discard whatever was imported before.
       */
      if (pCreateInfo->flags & VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT &&
          !layout->independent_sets) {
         anv_pipeline_sets_layout_fini(&pipeline->base.layout);
         anv_pipeline_sets_layout_init(&pipeline->base.layout,
                                       pipeline->base.device,
                                       false /* independent_sets */);
      } else {
         /* Otherwise if we include a layout that had independent_sets,
          * propagate that property.
          */
         pipeline->base.layout.independent_sets |= layout->independent_sets;
      }

      for (uint32_t s = 0; s < layout->num_sets; s++) {
         if (layout->set[s].layout == NULL)
            continue;

         anv_pipeline_sets_layout_add(&pipeline->base.layout, s,
                                      layout->set[s].layout);
      }
   }
}

static void
import_render_pass(struct anv_graphics_pipeline_info *info,
                   struct anv_graphics_lib_pipeline *lib)
{
   /* Already imported */
   if (info->ri)
      return;

   if (!lib->info.ri)
      return;

   info->ri = lib->info.ri;
   info->rsd = lib->info.rsd;
}

void
anv_graphics_pipeline_import_lib(struct anv_graphics_pipeline_base *pipeline,
                                 struct anv_graphics_pipeline_info *info,
                                 void *mem_ctx,
                                 struct anv_graphics_lib_pipeline *lib,
                                 bool link_optimize)
{
   /* There should be no common blocks between a lib we import and the current
    * pipeline we're building.
    */
   assert((pipeline->lib_flags & lib->base.lib_flags) == 0);
   assert((pipeline->active_stages & lib->base.active_stages) == 0);

   /* VK_EXT_graphics_pipeline_library:
    *
    *    "To perform link time optimizations,
    *     VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT must
    *     be specified on all pipeline libraries that are being linked
    *     together. Implementations should retain any additional information
    *     needed to perform optimizations at the final link step when this bit
    *     is present."
    */
   assert(!link_optimize || lib->base.retain_shaders);

   pipeline->lib_flags |= lib->base.lib_flags;
   pipeline->dynamic_states |= lib->base.dynamic_states;
   pipeline->active_stages |= lib->base.active_stages;

   if (lib->base.lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) {
      assert(info->vi == NULL && info->ia == NULL);

      info->vi =
         vk_PipelineVertexInputStateCreateInfo_deepcopy(mem_ctx, lib->info.vi);
      info->ia =
         vk_PipelineInputAssemblyStateCreateInfo_deepcopy(mem_ctx, lib->info.ia);
   }

   if (lib->base.lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) {
      if (lib->info.ts) {
         info->ts =
            vk_PipelineTessellationStateCreateInfo_deepcopy(mem_ctx, lib->info.ts);
      }

      assert(lib->info.rs != NULL);
      info->rs =
         vk_PipelineRasterizationStateCreateInfo_deepcopy(mem_ctx, lib->info.rs);

      if (lib->info.vp) {
         info->vp =
            vk_PipelineViewportStateCreateInfo_deepcopy(mem_ctx, lib->info.vp);
      }

      if (lib->info.fsr) {
         info->fsr =
            vk_PipelineFragmentShadingRateStateCreateInfoKHR_deepcopy(mem_ctx,
                                                                      lib->info.fsr);
      }

      import_render_pass(info, lib);
   }

   if (lib->base.lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) {
      if (lib->info.ms) {
         info->ms =
            vk_PipelineMultisampleStateCreateInfo_deepcopy(mem_ctx, lib->info.ms);
      }
      if (lib->info.ds) {
         info->ds =
            vk_PipelineDepthStencilStateCreateInfo_deepcopy(mem_ctx, lib->info.ds);
      }

      if (!info->fsr && lib->info.fsr) {
         info->fsr =
            vk_PipelineFragmentShadingRateStateCreateInfoKHR_deepcopy(mem_ctx,
                                                                      lib->info.fsr);
      }

      import_render_pass(info, lib);
   }

   if (lib->base.lib_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) {
      if (!info->ms && lib->info.ms) {
         info->ms =
            vk_PipelineMultisampleStateCreateInfo_deepcopy(mem_ctx, lib->info.ms);
      }
      if (lib->info.cb) {
         info->cb =
            vk_PipelineColorBlendStateCreateInfo_deepcopy(mem_ctx, lib->info.cb);
      }

      import_render_pass(info, lib);
   }

   /* If the library we import was able to use primitivie replication, this
    * pipeline will be able too.
    *
    * Note that currently primitive replication can only be enabled when both
    * VS & FS shaders are given together to create a pipeline (see
    * anv_check_for_primitive_replication).
    */
   if (lib->base.use_primitive_replication)
      pipeline->use_primitive_replication = true;

   if (lib->base.view_mask)
      pipeline->view_mask = lib->base.view_mask;

   /* Carry on the dynamic fragment information of the library */
   if (lib->base.fragment_dynamic && !link_optimize)
      pipeline->fragment_dynamic = true;

   /* Import the shaders but skip the binaries if we're doing link
    * optimization. In that case we're likely to lookup the cache with a
    * slightly different shader key.
    */
   for (uint32_t s = 0; s < ARRAY_SIZE(lib->base.shaders); s++) {
      if (!lib->base.shaders[s])
         continue;

      pipeline->retained_shaders[s] = lib->base.retained_shaders[s];
      if (!link_optimize)
         pipeline->shaders[s] = anv_shader_bin_ref(lib->base.shaders[s]);
   }

   struct anv_pipeline_sets_layout *lib_layout =
      &lib->base.base.layout;

   pipeline->base.layout.independent_sets |= lib_layout->independent_sets;
   for (uint32_t s = 0; s < lib_layout->num_sets; s++) {
      if (lib_layout->set[s].layout == NULL)
         continue;

      anv_pipeline_sets_layout_add(&pipeline->base.layout, s,
                                   lib_layout->set[s].layout);
   }
}
