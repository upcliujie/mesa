/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from tu_cmd_buffer.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "panvk_private.h"
#include "panfrost-quirks.h"

#include "pan_encoder.h"

#include "util/rounding.h"
#include "util/u_pack_color.h"
#include "vk_format.h"

static VkResult
panvk_reset_cmdbuf(struct panvk_cmd_buffer *cmdbuf)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;

   cmdbuf->record_result = VK_SUCCESS;

   list_for_each_entry_safe(struct panvk_batch, batch, &cmdbuf->batches, node) {
      list_del(&batch->node);
      util_dynarray_fini(&batch->jobs);
      if (!pan_is_bifrost(pdev))
         panfrost_bo_unreference(batch->tiler.ctx.midgard.polygon_list);

      util_dynarray_fini(&batch->event_ops);

      vk_free(&cmdbuf->pool->alloc, batch);
   }

   panvk_pool_reset(&cmdbuf->desc_pool);
   panvk_pool_reset(&cmdbuf->tls_pool);
   panvk_pool_reset(&cmdbuf->varying_pool);
   cmdbuf->status = PANVK_CMD_BUFFER_STATUS_INITIAL;

   for (unsigned i = 0; i < MAX_BIND_POINTS; i++)
      memset(&cmdbuf->descriptors[i].sets, 0, sizeof(cmdbuf->descriptors[i].sets));

   return cmdbuf->record_result;
}

static VkResult
panvk_create_cmdbuf(struct panvk_device *device,
                    struct panvk_cmd_pool *pool,
                    VkCommandBufferLevel level,
                    struct panvk_cmd_buffer **cmdbuf_out)
{
   struct panvk_cmd_buffer *cmdbuf;

   cmdbuf = vk_object_zalloc(&device->vk, NULL, sizeof(*cmdbuf),
                             VK_OBJECT_TYPE_COMMAND_BUFFER);
   if (!cmdbuf)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   cmdbuf->device = device;
   cmdbuf->level = level;
   cmdbuf->pool = pool;

   if (pool) {
      list_addtail(&cmdbuf->pool_link, &pool->active_cmd_buffers);
      cmdbuf->queue_family_index = pool->queue_family_index;
   } else {
      /* Init the pool_link so we can safely call list_del when we destroy
       * the command buffer
       */
      list_inithead(&cmdbuf->pool_link);
      cmdbuf->queue_family_index = PANVK_QUEUE_GENERAL;
   }

   panvk_pool_init(&cmdbuf->desc_pool, &device->physical_device->pdev,
                   pool ? &pool->desc_bo_pool : NULL, 0, 64 * 1024,
                   "Command buffer descriptor pool", true);
   panvk_pool_init(&cmdbuf->tls_pool, &device->physical_device->pdev,
                   pool ? &pool->tls_bo_pool : NULL,
                   PAN_BO_INVISIBLE, 64 * 1024, "TLS pool", false);
   panvk_pool_init(&cmdbuf->varying_pool, &device->physical_device->pdev,
                   pool ? &pool->varying_bo_pool : NULL,
                   PAN_BO_INVISIBLE, 64 * 1024, "Varyings pool", false);
   list_inithead(&cmdbuf->batches);
   cmdbuf->status = PANVK_CMD_BUFFER_STATUS_INITIAL;
   *cmdbuf_out = cmdbuf;
   return VK_SUCCESS;
}

static void
panvk_destroy_cmdbuf(struct panvk_cmd_buffer *cmdbuf)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;
   struct panvk_device *device = cmdbuf->device;

   list_del(&cmdbuf->pool_link);

   list_for_each_entry_safe(struct panvk_batch, batch, &cmdbuf->batches, node) {
      list_del(&batch->node);
      util_dynarray_fini(&batch->jobs);
      if (!pan_is_bifrost(pdev))
         panfrost_bo_unreference(batch->tiler.ctx.midgard.polygon_list);

      util_dynarray_fini(&batch->event_ops);

      vk_free(&cmdbuf->pool->alloc, batch);
   }

   panvk_pool_cleanup(&cmdbuf->desc_pool);
   panvk_pool_cleanup(&cmdbuf->tls_pool);
   panvk_pool_cleanup(&cmdbuf->varying_pool);
   vk_object_free(&device->vk, NULL, cmdbuf);
}

VkResult
panvk_AllocateCommandBuffers(VkDevice _device,
                             const VkCommandBufferAllocateInfo *pAllocateInfo,
                             VkCommandBuffer *pCommandBuffers)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_cmd_pool, pool, pAllocateInfo->commandPool);

   VkResult result = VK_SUCCESS;
   unsigned i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {
      struct panvk_cmd_buffer *cmdbuf = NULL;

      if (!list_is_empty(&pool->free_cmd_buffers)) {
         cmdbuf = list_first_entry(
            &pool->free_cmd_buffers, struct panvk_cmd_buffer, pool_link);

         list_del(&cmdbuf->pool_link);
         list_addtail(&cmdbuf->pool_link, &pool->active_cmd_buffers);

         cmdbuf->level = pAllocateInfo->level;
         vk_object_base_reset(&cmdbuf->base);
      } else {
         result = panvk_create_cmdbuf(device, pool, pAllocateInfo->level, &cmdbuf);
      }

      if (result != VK_SUCCESS)
         goto err_free_cmd_bufs;

      pCommandBuffers[i] = panvk_cmd_buffer_to_handle(cmdbuf);
   }

   return VK_SUCCESS;

err_free_cmd_bufs:
   panvk_FreeCommandBuffers(_device, pAllocateInfo->commandPool, i,
                            pCommandBuffers);
   for (unsigned j = 0; j < i; j++)
      pCommandBuffers[j] = VK_NULL_HANDLE;

   return result;
}

void
panvk_FreeCommandBuffers(VkDevice device,
                         VkCommandPool commandPool,
                         uint32_t commandBufferCount,
                         const VkCommandBuffer *pCommandBuffers)
{
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, pCommandBuffers[i]);

      if (cmdbuf) {
         if (cmdbuf->pool) {
            list_del(&cmdbuf->pool_link);
            panvk_reset_cmdbuf(cmdbuf);
            list_addtail(&cmdbuf->pool_link,
                         &cmdbuf->pool->free_cmd_buffers);
         } else
            panvk_destroy_cmdbuf(cmdbuf);
      }
   }
}

VkResult
panvk_ResetCommandBuffer(VkCommandBuffer commandBuffer,
                         VkCommandBufferResetFlags flags)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   return panvk_reset_cmdbuf(cmdbuf);
}

VkResult
panvk_BeginCommandBuffer(VkCommandBuffer commandBuffer,
                       const VkCommandBufferBeginInfo *pBeginInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VkResult result = VK_SUCCESS;

   if (cmdbuf->status != PANVK_CMD_BUFFER_STATUS_INITIAL) {
      /* If the command buffer has already been reset with
       * vkResetCommandBuffer, no need to do it again.
       */
      result = panvk_reset_cmdbuf(cmdbuf);
      if (result != VK_SUCCESS)
         return result;
   }

   memset(&cmdbuf->state, 0, sizeof(cmdbuf->state));

   cmdbuf->status = PANVK_CMD_BUFFER_STATUS_RECORDING;

   return VK_SUCCESS;
}

void
panvk_CmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                           uint32_t firstBinding,
                           uint32_t bindingCount,
                           const VkBuffer *pBuffers,
                           const VkDeviceSize *pOffsets)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   assert(firstBinding + bindingCount <= MAX_VBS);

   for (uint32_t i = 0; i < bindingCount; i++) {
      struct panvk_buffer *buf = panvk_buffer_from_handle(pBuffers[i]);

      cmdbuf->state.vb.bufs[firstBinding + i].address = buf->bo->ptr.gpu + pOffsets[i];
      cmdbuf->state.vb.bufs[firstBinding + i].size = buf->size - pOffsets[i];
   }
   cmdbuf->state.vb.count = MAX2(cmdbuf->state.vb.count, firstBinding + bindingCount);
   cmdbuf->state.vb.attrib_bufs = cmdbuf->state.vb.attribs = 0;
}

void
panvk_CmdBindIndexBuffer(VkCommandBuffer commandBuffer,
                         VkBuffer buffer,
                         VkDeviceSize offset,
                         VkIndexType indexType)
{
   panvk_stub();
}

void
panvk_CmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                            VkPipelineBindPoint pipelineBindPoint,
                            VkPipelineLayout _layout,
                            uint32_t firstSet,
                            uint32_t descriptorSetCount,
                            const VkDescriptorSet *pDescriptorSets,
                            uint32_t dynamicOffsetCount,
                            const uint32_t *pDynamicOffsets)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_pipeline_layout, layout, _layout);

   struct panvk_descriptor_state *descriptors_state =
      &cmdbuf->descriptors[pipelineBindPoint];

   for (unsigned i = 0; i < descriptorSetCount; ++i) {
      unsigned idx = i + firstSet;
      VK_FROM_HANDLE(panvk_descriptor_set, set, pDescriptorSets[i]);

      descriptors_state->sets[idx].set = set;

      if (layout->num_dynoffsets) {
         assert(dynamicOffsetCount >= set->layout->num_dynoffsets);

         descriptors_state->sets[idx].dynoffsets =
            pan_pool_alloc_aligned(&cmdbuf->desc_pool.base,
                                   ALIGN(layout->num_dynoffsets, 4) *
                                   sizeof(*pDynamicOffsets),
                                   16);
         memcpy(descriptors_state->sets[idx].dynoffsets.cpu,
                pDynamicOffsets,
                sizeof(*pDynamicOffsets) * set->layout->num_dynoffsets);
         dynamicOffsetCount -= set->layout->num_dynoffsets;
         pDynamicOffsets += set->layout->num_dynoffsets;
      }

      if (set->layout->num_ubos || set->layout->num_dynoffsets)
         descriptors_state->ubos = 0;

      if (set->layout->num_textures)
         descriptors_state->textures = 0;

      if (set->layout->num_samplers)
         descriptors_state->samplers = 0;
   }

   assert(!dynamicOffsetCount);
}

void
panvk_CmdPushConstants(VkCommandBuffer commandBuffer,
                       VkPipelineLayout layout,
                       VkShaderStageFlags stageFlags,
                       uint32_t offset,
                       uint32_t size,
                       const void *pValues)
{
   panvk_stub();
}

void
panvk_CmdBindPipeline(VkCommandBuffer commandBuffer,
                      VkPipelineBindPoint pipelineBindPoint,
                      VkPipeline _pipeline)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_pipeline, pipeline, _pipeline);

   cmdbuf->state.bind_point = pipelineBindPoint;
   cmdbuf->state.pipeline = pipeline;
   cmdbuf->state.varyings = pipeline->varyings;
   cmdbuf->state.vb.attrib_bufs = cmdbuf->state.vb.attribs = 0;
   cmdbuf->state.fs_rsd = 0;
   memset(cmdbuf->descriptors[pipelineBindPoint].sysvals, 0,
          sizeof(cmdbuf->descriptors[pipelineBindPoint].sysvals));

   /* Sysvals are passed through UBOs, we need dirty the UBO array if the
    * pipeline contain shaders using sysvals.
    */
   if (pipeline->num_sysvals)
      cmdbuf->descriptors[pipelineBindPoint].ubos = 0;
}

void
panvk_CmdSetViewport(VkCommandBuffer commandBuffer,
                     uint32_t firstViewport,
                     uint32_t viewportCount,
                     const VkViewport *pViewports)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   assert(viewportCount == 1);
   assert(!firstViewport);

   cmdbuf->state.viewport = pViewports[0];
   cmdbuf->state.vpd = 0;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_VIEWPORT;
}

void
panvk_CmdSetScissor(VkCommandBuffer commandBuffer,
                    uint32_t firstScissor,
                    uint32_t scissorCount,
                    const VkRect2D *pScissors)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   assert(scissorCount == 1);
   assert(!firstScissor);

   cmdbuf->state.scissor = pScissors[0];
   cmdbuf->state.vpd = 0;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_SCISSOR;
}

void
panvk_CmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   cmdbuf->state.rast.line_width = lineWidth;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_LINE_WIDTH;
}

void
panvk_CmdSetDepthBias(VkCommandBuffer commandBuffer,
                      float depthBiasConstantFactor,
                      float depthBiasClamp,
                      float depthBiasSlopeFactor)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   cmdbuf->state.rast.depth_bias.constant_factor = depthBiasConstantFactor;
   cmdbuf->state.rast.depth_bias.clamp = depthBiasClamp;
   cmdbuf->state.rast.depth_bias.slope_factor = depthBiasSlopeFactor;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_DEPTH_BIAS;
   cmdbuf->state.fs_rsd = 0;
}

void
panvk_CmdSetBlendConstants(VkCommandBuffer commandBuffer,
                           const float blendConstants[4])
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   memcpy(cmdbuf->state.blend.constants, blendConstants,
          sizeof(cmdbuf->state.blend.constants));
   cmdbuf->state.dirty |= PANVK_DYNAMIC_BLEND_CONSTANTS;
   cmdbuf->state.fs_rsd = 0;
}

void
panvk_CmdSetDepthBounds(VkCommandBuffer commandBuffer,
                        float minDepthBounds,
                        float maxDepthBounds)
{
   panvk_stub();
}

void
panvk_CmdSetStencilCompareMask(VkCommandBuffer commandBuffer,
                               VkStencilFaceFlags faceMask,
                               uint32_t compareMask)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
      cmdbuf->state.zs.s_front.compare_mask = compareMask;

   if (faceMask & VK_STENCIL_FACE_BACK_BIT)
      cmdbuf->state.zs.s_back.compare_mask = compareMask;

   cmdbuf->state.dirty |= PANVK_DYNAMIC_STENCIL_COMPARE_MASK;
   cmdbuf->state.fs_rsd = 0;
}

void
panvk_CmdSetStencilWriteMask(VkCommandBuffer commandBuffer,
                             VkStencilFaceFlags faceMask,
                             uint32_t writeMask)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
      cmdbuf->state.zs.s_front.write_mask = writeMask;

   if (faceMask & VK_STENCIL_FACE_BACK_BIT)
      cmdbuf->state.zs.s_back.write_mask = writeMask;

   cmdbuf->state.dirty |= PANVK_DYNAMIC_STENCIL_WRITE_MASK;
   cmdbuf->state.fs_rsd = 0;
}

void
panvk_CmdSetStencilReference(VkCommandBuffer commandBuffer,
                             VkStencilFaceFlags faceMask,
                             uint32_t reference)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
      cmdbuf->state.zs.s_front.ref = reference;

   if (faceMask & VK_STENCIL_FACE_BACK_BIT)
      cmdbuf->state.zs.s_back.ref = reference;

   cmdbuf->state.dirty |= PANVK_DYNAMIC_STENCIL_REFERENCE;
   cmdbuf->state.fs_rsd = 0;
}

void
panvk_CmdExecuteCommands(VkCommandBuffer commandBuffer,
                         uint32_t commandBufferCount,
                         const VkCommandBuffer *pCmdBuffers)
{
   panvk_stub();
}

VkResult
panvk_CreateCommandPool(VkDevice _device,
                        const VkCommandPoolCreateInfo *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator,
                        VkCommandPool *pCmdPool)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_cmd_pool *pool;

   pool = vk_object_alloc(&device->vk, pAllocator, sizeof(*pool),
                          VK_OBJECT_TYPE_COMMAND_POOL);
   if (pool == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (pAllocator)
      pool->alloc = *pAllocator;
   else
      pool->alloc = device->vk.alloc;

   list_inithead(&pool->active_cmd_buffers);
   list_inithead(&pool->free_cmd_buffers);

   pool->queue_family_index = pCreateInfo->queueFamilyIndex;
   panvk_bo_pool_init(&pool->desc_bo_pool);
   panvk_bo_pool_init(&pool->varying_bo_pool);
   panvk_bo_pool_init(&pool->tls_bo_pool);
   *pCmdPool = panvk_cmd_pool_to_handle(pool);
   return VK_SUCCESS;
}

void
panvk_DestroyCommandPool(VkDevice _device,
                         VkCommandPool commandPool,
                         const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_cmd_pool, pool, commandPool);

   list_for_each_entry_safe(struct panvk_cmd_buffer, cmdbuf,
                            &pool->active_cmd_buffers, pool_link)
      panvk_destroy_cmdbuf(cmdbuf);

   list_for_each_entry_safe(struct panvk_cmd_buffer, cmdbuf,
                            &pool->free_cmd_buffers, pool_link)
      panvk_destroy_cmdbuf(cmdbuf);

   panvk_bo_pool_cleanup(&pool->desc_bo_pool);
   panvk_bo_pool_cleanup(&pool->varying_bo_pool);
   panvk_bo_pool_cleanup(&pool->tls_bo_pool);
   vk_object_free(&device->vk, pAllocator, pool);
}

VkResult
panvk_ResetCommandPool(VkDevice device,
                       VkCommandPool commandPool,
                       VkCommandPoolResetFlags flags)
{
   VK_FROM_HANDLE(panvk_cmd_pool, pool, commandPool);
   VkResult result;

   list_for_each_entry(struct panvk_cmd_buffer, cmdbuf, &pool->active_cmd_buffers,
                       pool_link)
   {
      result = panvk_reset_cmdbuf(cmdbuf);
      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

void
panvk_TrimCommandPool(VkDevice device,
                      VkCommandPool commandPool,
                      VkCommandPoolTrimFlags flags)
{
   VK_FROM_HANDLE(panvk_cmd_pool, pool, commandPool);

   if (!pool)
      return;

   list_for_each_entry_safe(struct panvk_cmd_buffer, cmdbuf,
                            &pool->free_cmd_buffers, pool_link)
      panvk_destroy_cmdbuf(cmdbuf);
}

static void
panvk_pack_color_32(uint32_t *packed, uint32_t v)
{
   for (unsigned i = 0; i < 4; ++i)
      packed[i] = v;
}

static void
panvk_pack_color_64(uint32_t *packed, uint32_t lo, uint32_t hi)
{
   for (unsigned i = 0; i < 4; i += 2) {
      packed[i + 0] = lo;
      packed[i + 1] = hi;
   }
}

void
panvk_pack_color(struct panvk_clear_value *out,
                 const VkClearColorValue *in,
                 enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   /* Alpha magicked to 1.0 if there is no alpha */
   bool has_alpha = util_format_has_alpha(format);
   float clear_alpha = has_alpha ? in->float32[3] : 1.0f;
   uint32_t *packed = out->color;

   if (util_format_is_rgba8_variant(desc) && desc->colorspace != UTIL_FORMAT_COLORSPACE_SRGB) {
      panvk_pack_color_32(packed,
                          ((uint32_t) float_to_ubyte(clear_alpha) << 24) |
                          ((uint32_t) float_to_ubyte(in->float32[2]) << 16) |
                          ((uint32_t) float_to_ubyte(in->float32[1]) <<  8) |
                          ((uint32_t) float_to_ubyte(in->float32[0]) <<  0));
   } else if (format == PIPE_FORMAT_B5G6R5_UNORM) {
      /* First, we convert the components to R5, G6, B5 separately */
      unsigned r5 = _mesa_roundevenf(SATURATE(in->float32[0]) * 31.0);
      unsigned g6 = _mesa_roundevenf(SATURATE(in->float32[1]) * 63.0);
      unsigned b5 = _mesa_roundevenf(SATURATE(in->float32[2]) * 31.0);

      /* Then we pack into a sparse u32. TODO: Why these shifts? */
      panvk_pack_color_32(packed, (b5 << 25) | (g6 << 14) | (r5 << 5));
   } else if (format == PIPE_FORMAT_B4G4R4A4_UNORM) {
      /* Convert to 4-bits */
      unsigned r4 = _mesa_roundevenf(SATURATE(in->float32[0]) * 15.0);
      unsigned g4 = _mesa_roundevenf(SATURATE(in->float32[1]) * 15.0);
      unsigned b4 = _mesa_roundevenf(SATURATE(in->float32[2]) * 15.0);
      unsigned a4 = _mesa_roundevenf(SATURATE(clear_alpha) * 15.0);

      /* Pack on *byte* intervals */
      panvk_pack_color_32(packed, (a4 << 28) | (b4 << 20) | (g4 << 12) | (r4 << 4));
   } else if (format == PIPE_FORMAT_B5G5R5A1_UNORM) {
      /* Scale as expected but shift oddly */
      unsigned r5 = _mesa_roundevenf(SATURATE(in->float32[0]) * 31.0);
      unsigned g5 = _mesa_roundevenf(SATURATE(in->float32[1]) * 31.0);
      unsigned b5 = _mesa_roundevenf(SATURATE(in->float32[2]) * 31.0);
      unsigned a1 = _mesa_roundevenf(SATURATE(clear_alpha) * 1.0);

      panvk_pack_color_32(packed, (a1 << 31) | (b5 << 25) | (g5 << 15) | (r5 << 5));
   } else {
      /* Otherwise, it's generic subject to replication */

      union util_color out = { 0 };
      unsigned size = util_format_get_blocksize(format);

      util_pack_color(in->float32, format, &out);

      if (size == 1) {
         unsigned b = out.ui[0];
         unsigned s = b | (b << 8);
         panvk_pack_color_32(packed, s | (s << 16));
      } else if (size == 2)
         panvk_pack_color_32(packed, out.ui[0] | (out.ui[0] << 16));
      else if (size == 3 || size == 4)
         panvk_pack_color_32(packed, out.ui[0]);
      else if (size == 6 || size == 8)
         panvk_pack_color_64(packed, out.ui[0], out.ui[1]);
      else if (size == 12 || size == 16)
         memcpy(packed, out.ui, 16);
      else
         unreachable("Unknown generic format size packing clear colour");
   }
}

static void
panvk_cmd_prepare_clear_values(struct panvk_cmd_buffer *cmdbuf,
                               const VkClearValue *in)
{
   for (unsigned i = 0; i < cmdbuf->state.pass->attachment_count; i++) {
       const struct panvk_render_pass_attachment *attachment =
          &cmdbuf->state.pass->attachments[i];
       enum pipe_format fmt = attachment->format;

       if (util_format_is_depth_or_stencil(fmt)) {
          if (attachment->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR ||
              attachment->stencil_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
             cmdbuf->state.clear[i].depth = in[i].depthStencil.depth;
             cmdbuf->state.clear[i].stencil = in[i].depthStencil.stencil;
          }
       } else if (attachment->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
          panvk_pack_color(&cmdbuf->state.clear[i], &in[i].color, fmt);
       }
   }
}

void
panvk_CmdBeginRenderPass2(VkCommandBuffer commandBuffer,
                          const VkRenderPassBeginInfo *pRenderPassBegin,
                          const VkSubpassBeginInfo *pSubpassBeginInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_render_pass, pass, pRenderPassBegin->renderPass);
   VK_FROM_HANDLE(panvk_framebuffer, fb, pRenderPassBegin->framebuffer);

   cmdbuf->state.pass = pass;
   cmdbuf->state.subpass = pass->subpasses;
   cmdbuf->state.framebuffer = fb;
   cmdbuf->state.render_area = pRenderPassBegin->renderArea;
   cmdbuf->state.batch = vk_zalloc(&cmdbuf->pool->alloc,
                                   sizeof(*cmdbuf->state.batch), 8,
                                   VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   util_dynarray_init(&cmdbuf->state.batch->jobs, NULL);
   util_dynarray_init(&cmdbuf->state.batch->event_ops, NULL);
   cmdbuf->state.clear = vk_zalloc(&cmdbuf->pool->alloc,
                                   sizeof(*cmdbuf->state.clear) *
                                   pRenderPassBegin->clearValueCount, 8,
                                   VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   assert(pRenderPassBegin->clearValueCount == pass->attachment_count);
   panvk_cmd_prepare_clear_values(cmdbuf, pRenderPassBegin->pClearValues);
   memset(&cmdbuf->state.compute, 0, sizeof(cmdbuf->state.compute));
}

void
panvk_CmdBeginRenderPass(VkCommandBuffer cmd,
                         const VkRenderPassBeginInfo *info,
                         VkSubpassContents contents)
{
   VkSubpassBeginInfo subpass_info = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
      .contents = contents
   };

   return panvk_CmdBeginRenderPass2(cmd, info, &subpass_info);
}

void
panvk_cmd_open_batch(struct panvk_cmd_buffer *cmdbuf)
{
   assert(!cmdbuf->state.batch);
   cmdbuf->state.batch = vk_zalloc(&cmdbuf->pool->alloc,
                                   sizeof(*cmdbuf->state.batch), 8,
                                   VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   assert(cmdbuf->state.batch);
}

void
panvk_CmdDrawIndexed(VkCommandBuffer commandBuffer,
                     uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance)
{
   panvk_stub();
}

void
panvk_CmdDrawIndirect(VkCommandBuffer commandBuffer,
                      VkBuffer _buffer,
                      VkDeviceSize offset,
                      uint32_t drawCount,
                      uint32_t stride)
{
   panvk_stub();
}

void
panvk_CmdDrawIndexedIndirect(VkCommandBuffer commandBuffer,
                             VkBuffer _buffer,
                             VkDeviceSize offset,
                             uint32_t drawCount,
                             uint32_t stride)
{
   panvk_stub();
}

void
panvk_CmdDispatchBase(VkCommandBuffer commandBuffer,
                      uint32_t base_x,
                      uint32_t base_y,
                      uint32_t base_z,
                      uint32_t x,
                      uint32_t y,
                      uint32_t z)
{
   panvk_stub();
}

void
panvk_CmdDispatch(VkCommandBuffer commandBuffer,
                  uint32_t x,
                  uint32_t y,
                  uint32_t z)
{
   panvk_stub();
}

void
panvk_CmdDispatchIndirect(VkCommandBuffer commandBuffer,
                          VkBuffer _buffer,
                          VkDeviceSize offset)
{
   panvk_stub();
}

void
panvk_CmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
   panvk_stub();
}
