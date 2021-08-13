/*
 * Copyright © 2019 Red Hat.
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

#include "lvp_private.h"
#include "pipe/p_context.h"
#include "vk_util.h"

static VkResult lvp_create_cmd_buffer(
   struct lvp_device *                         device,
   struct lvp_cmd_pool *                       pool,
   VkCommandBufferLevel                        level,
   VkCommandBuffer*                            pCommandBuffer)
{
   struct lvp_cmd_buffer *cmd_buffer;

   cmd_buffer = vk_alloc(&pool->alloc, sizeof(*cmd_buffer), 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (cmd_buffer == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &cmd_buffer->base,
                       VK_OBJECT_TYPE_COMMAND_BUFFER);
   cmd_buffer->device = device;
   cmd_buffer->pool = pool;

   cmd_buffer->queue.alloc = &pool->alloc;
   list_inithead(&cmd_buffer->queue.cmds);

   cmd_buffer->status = LVP_CMD_BUFFER_STATUS_INITIAL;
   if (pool) {
      list_addtail(&cmd_buffer->pool_link, &pool->cmd_buffers);
   } else {
      /* Init the pool_link so we can safefly call list_del when we destroy
       * the command buffer
       */
      list_inithead(&cmd_buffer->pool_link);
   }
   *pCommandBuffer = lvp_cmd_buffer_to_handle(cmd_buffer);

   return VK_SUCCESS;
}

static void
lvp_cmd_buffer_free_all_cmds(struct lvp_cmd_buffer *cmd_buffer)
{
   struct vk_cmd_queue_entry *tmp, *cmd;
   LIST_FOR_EACH_ENTRY_SAFE(cmd, tmp, &cmd_buffer->queue.cmds, cmd_link) {
      list_del(&cmd->cmd_link);
      vk_free(&cmd_buffer->pool->alloc, cmd);
   }
}

static VkResult lvp_reset_cmd_buffer(struct lvp_cmd_buffer *cmd_buffer)
{
   lvp_cmd_buffer_free_all_cmds(cmd_buffer);
   list_inithead(&cmd_buffer->queue.cmds);
   cmd_buffer->status = LVP_CMD_BUFFER_STATUS_INITIAL;
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL lvp_AllocateCommandBuffers(
   VkDevice                                    _device,
   const VkCommandBufferAllocateInfo*          pAllocateInfo,
   VkCommandBuffer*                            pCommandBuffers)
{
   LVP_FROM_HANDLE(lvp_device, device, _device);
   LVP_FROM_HANDLE(lvp_cmd_pool, pool, pAllocateInfo->commandPool);

   VkResult result = VK_SUCCESS;
   uint32_t i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {

      if (!list_is_empty(&pool->free_cmd_buffers)) {
         struct lvp_cmd_buffer *cmd_buffer = list_first_entry(&pool->free_cmd_buffers, struct lvp_cmd_buffer, pool_link);

         list_del(&cmd_buffer->pool_link);
         list_addtail(&cmd_buffer->pool_link, &pool->cmd_buffers);

         result = lvp_reset_cmd_buffer(cmd_buffer);
         cmd_buffer->level = pAllocateInfo->level;
         vk_object_base_reset(&cmd_buffer->base);

         pCommandBuffers[i] = lvp_cmd_buffer_to_handle(cmd_buffer);
      } else {
         result = lvp_create_cmd_buffer(device, pool, pAllocateInfo->level,
                                        &pCommandBuffers[i]);
         if (result != VK_SUCCESS)
            break;
      }
   }

   if (result != VK_SUCCESS) {
      lvp_FreeCommandBuffers(_device, pAllocateInfo->commandPool,
                             i, pCommandBuffers);
      memset(pCommandBuffers, 0,
             sizeof(*pCommandBuffers) * pAllocateInfo->commandBufferCount);
   }

   return result;
}

static void
lvp_cmd_buffer_destroy(struct lvp_cmd_buffer *cmd_buffer)
{
   lvp_cmd_buffer_free_all_cmds(cmd_buffer);
   list_del(&cmd_buffer->pool_link);
   vk_object_base_finish(&cmd_buffer->base);
   vk_free(&cmd_buffer->pool->alloc, cmd_buffer);
}

VKAPI_ATTR void VKAPI_CALL lvp_FreeCommandBuffers(
   VkDevice                                    device,
   VkCommandPool                               commandPool,
   uint32_t                                    commandBufferCount,
   const VkCommandBuffer*                      pCommandBuffers)
{
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, pCommandBuffers[i]);

      if (cmd_buffer) {
         if (cmd_buffer->pool) {
            list_del(&cmd_buffer->pool_link);
            list_addtail(&cmd_buffer->pool_link, &cmd_buffer->pool->free_cmd_buffers);
         } else
            lvp_cmd_buffer_destroy(cmd_buffer);
      }
   }
}

VKAPI_ATTR VkResult VKAPI_CALL lvp_ResetCommandBuffer(
   VkCommandBuffer                             commandBuffer,
   VkCommandBufferResetFlags                   flags)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   return lvp_reset_cmd_buffer(cmd_buffer);
}

VKAPI_ATTR VkResult VKAPI_CALL lvp_BeginCommandBuffer(
   VkCommandBuffer                             commandBuffer,
   const VkCommandBufferBeginInfo*             pBeginInfo)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);
   VkResult result;
   if (cmd_buffer->status != LVP_CMD_BUFFER_STATUS_INITIAL) {
      result = lvp_reset_cmd_buffer(cmd_buffer);
      if (result != VK_SUCCESS)
         return result;
   }
   cmd_buffer->status = LVP_CMD_BUFFER_STATUS_RECORDING;
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL lvp_EndCommandBuffer(
   VkCommandBuffer                             commandBuffer)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);
   cmd_buffer->status = LVP_CMD_BUFFER_STATUS_EXECUTABLE;
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL lvp_CreateCommandPool(
   VkDevice                                    _device,
   const VkCommandPoolCreateInfo*              pCreateInfo,
   const VkAllocationCallbacks*                pAllocator,
   VkCommandPool*                              pCmdPool)
{
   LVP_FROM_HANDLE(lvp_device, device, _device);
   struct lvp_cmd_pool *pool;

   pool = vk_alloc2(&device->vk.alloc, pAllocator, sizeof(*pool), 8,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (pool == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &pool->base,
                       VK_OBJECT_TYPE_COMMAND_POOL);
   if (pAllocator)
      pool->alloc = *pAllocator;
   else
      pool->alloc = device->vk.alloc;

   list_inithead(&pool->cmd_buffers);
   list_inithead(&pool->free_cmd_buffers);

   *pCmdPool = lvp_cmd_pool_to_handle(pool);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL lvp_DestroyCommandPool(
   VkDevice                                    _device,
   VkCommandPool                               commandPool,
   const VkAllocationCallbacks*                pAllocator)
{
   LVP_FROM_HANDLE(lvp_device, device, _device);
   LVP_FROM_HANDLE(lvp_cmd_pool, pool, commandPool);

   if (!pool)
      return;

   list_for_each_entry_safe(struct lvp_cmd_buffer, cmd_buffer,
                            &pool->cmd_buffers, pool_link) {
      lvp_cmd_buffer_destroy(cmd_buffer);
   }

   list_for_each_entry_safe(struct lvp_cmd_buffer, cmd_buffer,
                            &pool->free_cmd_buffers, pool_link) {
      lvp_cmd_buffer_destroy(cmd_buffer);
   }

   vk_object_base_finish(&pool->base);
   vk_free2(&device->vk.alloc, pAllocator, pool);
}

VKAPI_ATTR VkResult VKAPI_CALL lvp_ResetCommandPool(
   VkDevice                                    device,
   VkCommandPool                               commandPool,
   VkCommandPoolResetFlags                     flags)
{
   LVP_FROM_HANDLE(lvp_cmd_pool, pool, commandPool);
   VkResult result;

   list_for_each_entry(struct lvp_cmd_buffer, cmd_buffer,
                       &pool->cmd_buffers, pool_link) {
      result = lvp_reset_cmd_buffer(cmd_buffer);
      if (result != VK_SUCCESS)
         return result;
   }
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL lvp_TrimCommandPool(
   VkDevice                                    device,
   VkCommandPool                               commandPool,
   VkCommandPoolTrimFlags                      flags)
{
   LVP_FROM_HANDLE(lvp_cmd_pool, pool, commandPool);

   if (!pool)
      return;

   list_for_each_entry_safe(struct lvp_cmd_buffer, cmd_buffer,
                            &pool->free_cmd_buffers, pool_link) {
      lvp_cmd_buffer_destroy(cmd_buffer);
   }
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBeginRenderPass2(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBeginInfo,
    const VkSubpassBeginInfo*                   pSubpassBeginInfo)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_begin_render_pass2(&cmd_buffer->queue,
                                     pRenderPassBeginInfo,
                                     pSubpassBeginInfo);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdNextSubpass2(
    VkCommandBuffer                             commandBuffer,
    const VkSubpassBeginInfo*                   pSubpassBeginInfo,
    const VkSubpassEndInfo*                     pSubpassEndInfo)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_next_subpass2(&cmd_buffer->queue,
                                pSubpassBeginInfo,
                                pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBindVertexBuffers(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    firstBinding,
   uint32_t                                    bindingCount,
   const VkBuffer*                             pBuffers,
   const VkDeviceSize*                         pOffsets)
{
   lvp_CmdBindVertexBuffers2EXT(commandBuffer, firstBinding,
      bindingCount, pBuffers, pOffsets, NULL, NULL);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBindPipeline(
   VkCommandBuffer                             commandBuffer,
   VkPipelineBindPoint                         pipelineBindPoint,
   VkPipeline                                  pipeline)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_bind_pipeline(&cmd_buffer->queue,
                                pipelineBindPoint,
                                pipeline);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBindDescriptorSets(
   VkCommandBuffer                             commandBuffer,
   VkPipelineBindPoint                         pipelineBindPoint,
   VkPipelineLayout                            layout,
   uint32_t                                    firstSet,
   uint32_t                                    descriptorSetCount,
   const VkDescriptorSet*                      pDescriptorSets,
   uint32_t                                    dynamicOffsetCount,
   const uint32_t*                             pDynamicOffsets)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_bind_descriptor_sets(&cmd_buffer->queue,
                                       pipelineBindPoint,
                                       layout,
                                       firstSet,
                                       descriptorSetCount,
                                       pDescriptorSets,
                                       dynamicOffsetCount,
                                       pDynamicOffsets);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDraw(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    vertexCount,
   uint32_t                                    instanceCount,
   uint32_t                                    firstVertex,
   uint32_t                                    firstInstance)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_draw(&cmd_buffer->queue,
                       vertexCount, 
                       instanceCount,
                       firstVertex, 
                       firstInstance);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawMultiEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    drawCount,
    const VkMultiDrawInfoEXT                   *pVertexInfo,
    uint32_t                                    instanceCount,
    uint32_t                                    firstInstance,
    uint32_t                                    stride)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   struct vk_cmd_queue_entry *cmd = vk_zalloc(cmd_buffer->queue.alloc,
                                              sizeof(*cmd), 8,
                                              VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   if (!cmd)
      return;

   cmd->type = VK_CMD_DRAW_MULTI_EXT;
   list_addtail(&cmd->cmd_link, &cmd_buffer->queue.cmds);

   cmd->u.draw_multi_ext.draw_count = drawCount;
   if (pVertexInfo) {
      cmd->u.draw_multi_ext.vertex_info = vk_zalloc(cmd_buffer->queue.alloc, stride * drawCount, 8, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      memcpy(( VkMultiDrawInfoEXT* )cmd->u.draw_multi_ext.vertex_info, pVertexInfo, stride * drawCount);
   }
   cmd->u.draw_multi_ext.instance_count = instanceCount;
   cmd->u.draw_multi_ext.first_instance = firstInstance;
   cmd->u.draw_multi_ext.stride = stride;
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdEndRenderPass2(
   VkCommandBuffer                             commandBuffer,
   const VkSubpassEndInfo*                     pSubpassEndInfo)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_end_render_pass2(&cmd_buffer->queue,
                                   pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetViewport(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    firstViewport,
   uint32_t                                    viewportCount,
   const VkViewport*                           pViewports)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_viewport(&cmd_buffer->queue,
                               firstViewport, 
                               viewportCount,
                               pViewports);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetScissor(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    firstScissor,
   uint32_t                                    scissorCount,
   const VkRect2D*                             pScissors)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_scissor(&cmd_buffer->queue,
                              firstScissor, 
                              scissorCount,
                              pScissors);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetLineWidth(
   VkCommandBuffer                             commandBuffer,
   float                                       lineWidth)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_line_width(&cmd_buffer->queue,
                                 lineWidth);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDepthBias(
   VkCommandBuffer                             commandBuffer,
   float                                       depthBiasConstantFactor,
   float                                       depthBiasClamp,
   float                                       depthBiasSlopeFactor)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_depth_bias(&cmd_buffer->queue,
                                 depthBiasConstantFactor, 
                                 depthBiasClamp,
                                 depthBiasSlopeFactor);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetBlendConstants(
   VkCommandBuffer                             commandBuffer,
   const float                                 blendConstants[4])
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_blend_constants(&cmd_buffer->queue,
                                      blendConstants);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDepthBounds(
   VkCommandBuffer                             commandBuffer,
   float                                       minDepthBounds,
   float                                       maxDepthBounds)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_depth_bounds(&cmd_buffer->queue,
                                   minDepthBounds, 
                                   maxDepthBounds);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetStencilCompareMask(
   VkCommandBuffer                             commandBuffer,
   VkStencilFaceFlags                          faceMask,
   uint32_t                                    compareMask)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_stencil_compare_mask(&cmd_buffer->queue,
                                           faceMask, 
                                           compareMask);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetStencilWriteMask(
   VkCommandBuffer                             commandBuffer,
   VkStencilFaceFlags                          faceMask,
   uint32_t                                    writeMask)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_stencil_write_mask(&cmd_buffer->queue,
                                         faceMask, 
                                         writeMask);
}


VKAPI_ATTR void VKAPI_CALL lvp_CmdSetStencilReference(
   VkCommandBuffer                             commandBuffer,
   VkStencilFaceFlags                          faceMask,
   uint32_t                                    reference)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_stencil_reference(&cmd_buffer->queue,
                                        faceMask, 
                                        reference);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdPushConstants(
   VkCommandBuffer                             commandBuffer,
   VkPipelineLayout                            layout,
   VkShaderStageFlags                          stageFlags,
   uint32_t                                    offset,
   uint32_t                                    size,
   const void*                                 pValues)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_push_constants(&cmd_buffer->queue,
                                        layout,
                                        stageFlags,
                                        offset,
                                        size,
                                        pValues);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBindIndexBuffer(
   VkCommandBuffer                             commandBuffer,
   VkBuffer                                    buffer,
   VkDeviceSize                                offset,
   VkIndexType                                 indexType)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_bind_index_buffer(&cmd_buffer->queue,
                                    buffer,
                                    offset,
                                    indexType);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawIndexed(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    indexCount,
   uint32_t                                    instanceCount,
   uint32_t                                    firstIndex,
   int32_t                                     vertexOffset,
   uint32_t                                    firstInstance)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_draw_indexed(&cmd_buffer->queue,
                               indexCount,
                               instanceCount,
                               firstIndex,
                               vertexOffset,
                               firstInstance);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawMultiIndexedEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    drawCount,
    const VkMultiDrawIndexedInfoEXT            *pIndexInfo,
    uint32_t                                    instanceCount,
    uint32_t                                    firstInstance,
    uint32_t                                    stride,
    const int32_t                              *pVertexOffset)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   struct vk_cmd_queue_entry *cmd = vk_zalloc(cmd_buffer->queue.alloc,
                                              sizeof(*cmd), 8,
                                              VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   if (!cmd)
      return;

   cmd->type = VK_CMD_DRAW_MULTI_INDEXED_EXT;
   list_addtail(&cmd->cmd_link, &cmd_buffer->queue.cmds);

   cmd->u.draw_multi_indexed_ext.draw_count = drawCount;

   if (pIndexInfo) {
      cmd->u.draw_multi_indexed_ext.index_info = vk_zalloc(cmd_buffer->queue.alloc, stride * drawCount, 8, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      memcpy(( VkMultiDrawIndexedInfoEXT* )cmd->u.draw_multi_indexed_ext.index_info, pIndexInfo, stride * drawCount);
   }

   cmd->u.draw_multi_indexed_ext.instance_count = instanceCount;
   cmd->u.draw_multi_indexed_ext.first_instance = firstInstance;
   cmd->u.draw_multi_indexed_ext.stride = stride;

   if (pVertexOffset) {
      cmd->u.draw_multi_indexed_ext.vertex_offset = vk_zalloc(cmd_buffer->queue.alloc, sizeof(*cmd->u.draw_multi_indexed_ext.vertex_offset), 8, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      memcpy(cmd->u.draw_multi_indexed_ext.vertex_offset, pVertexOffset, sizeof(*cmd->u.draw_multi_indexed_ext.vertex_offset));
   }
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawIndirect(
   VkCommandBuffer                             commandBuffer,
   VkBuffer                                    buffer,
   VkDeviceSize                                offset,
   uint32_t                                    drawCount,
   uint32_t                                    stride)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_draw_indirect(&cmd_buffer->queue,
                                buffer,
                                offset,
                                drawCount,
                                stride);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawIndexedIndirect(
   VkCommandBuffer                             commandBuffer,
   VkBuffer                                    buffer,
   VkDeviceSize                                offset,
   uint32_t                                    drawCount,
   uint32_t                                    stride)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_draw_indexed_indirect(&cmd_buffer->queue,
                                        buffer,
                                        offset,
                                        drawCount,
                                        stride);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDispatch(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    x,
   uint32_t                                    y,
   uint32_t                                    z)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_dispatch(&cmd_buffer->queue,
                           x,
                           y,
                           z);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDispatchIndirect(
   VkCommandBuffer                             commandBuffer,
   VkBuffer                                    buffer,
   VkDeviceSize                                offset)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_dispatch_indirect(&cmd_buffer->queue,
                                    buffer,
                                    offset);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdExecuteCommands(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    commandBufferCount,
   const VkCommandBuffer*                      pCmdBuffers)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_execute_commands(&cmd_buffer->queue,
                                   commandBufferCount,
                                   pCmdBuffers);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetEvent(VkCommandBuffer commandBuffer,
                     VkEvent event,
                     VkPipelineStageFlags stageMask)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_event(&cmd_buffer->queue,
                            event,
                            stageMask);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdResetEvent(VkCommandBuffer commandBuffer,
                       VkEvent event,
                       VkPipelineStageFlags stageMask)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_event(&cmd_buffer->queue,
                            event,
                            stageMask);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdWaitEvents(VkCommandBuffer commandBuffer,
                       uint32_t eventCount,
                       const VkEvent* pEvents,
                       VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       uint32_t memoryBarrierCount,
                       const VkMemoryBarrier* pMemoryBarriers,
                       uint32_t bufferMemoryBarrierCount,
                       const VkBufferMemoryBarrier* pBufferMemoryBarriers,
                       uint32_t imageMemoryBarrierCount,
                       const VkImageMemoryBarrier* pImageMemoryBarriers)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_wait_events(&cmd_buffer->queue,
                              eventCount,
                              pEvents,
                              srcStageMask,
                              dstStageMask,
                              memoryBarrierCount,
                              pMemoryBarriers,
                              bufferMemoryBarrierCount,
                              pBufferMemoryBarriers,
                              imageMemoryBarrierCount,
                              pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdCopyBufferToImage2KHR(
   VkCommandBuffer                             commandBuffer,
   const VkCopyBufferToImageInfo2KHR          *info)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_copy_buffer_to_image2_khr(&cmd_buffer->queue,
                                            info);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdCopyImageToBuffer2KHR(
   VkCommandBuffer                             commandBuffer,
   const VkCopyImageToBufferInfo2KHR          *info)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_copy_image_to_buffer2_khr(&cmd_buffer->queue,
                                            info);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdCopyImage2KHR(
   VkCommandBuffer                             commandBuffer,
   const VkCopyImageInfo2KHR                  *info)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_copy_image2_khr(&cmd_buffer->queue,
                                  info);
}


VKAPI_ATTR void VKAPI_CALL lvp_CmdCopyBuffer2KHR(
   VkCommandBuffer                             commandBuffer,
   const VkCopyBufferInfo2KHR                 *info)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_copy_buffer2_khr(&cmd_buffer->queue,
                                   info);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBlitImage2KHR(
   VkCommandBuffer                             commandBuffer,
   const VkBlitImageInfo2KHR                  *info)
{
   
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_blit_image2_khr(&cmd_buffer->queue,
                                  info);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdClearAttachments(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    attachmentCount,
   const VkClearAttachment*                    pAttachments,
   uint32_t                                    rectCount,
   const VkClearRect*                          pRects)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_clear_attachments(&cmd_buffer->queue,
                                    attachmentCount,
                                    pAttachments,
                                    rectCount,
                                    pRects);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdFillBuffer(
   VkCommandBuffer                             commandBuffer,
   VkBuffer                                    dstBuffer,
   VkDeviceSize                                dstOffset,
   VkDeviceSize                                fillSize,
   uint32_t                                    data)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_fill_buffer(&cmd_buffer->queue,
                              dstBuffer,
                              dstOffset,
                              fillSize,
                              data);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdUpdateBuffer(
   VkCommandBuffer                             commandBuffer,
   VkBuffer                                    dstBuffer,
   VkDeviceSize                                dstOffset,
   VkDeviceSize                                dataSize,
   const void*                                 pData)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_update_buffer(&cmd_buffer->queue,
                                dstBuffer,
                                dstOffset,
                                dataSize,
                                pData);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdClearColorImage(
   VkCommandBuffer                             commandBuffer,
   VkImage                                     image_h,
   VkImageLayout                               imageLayout,
   const VkClearColorValue*                    pColor,
   uint32_t                                    rangeCount,
   const VkImageSubresourceRange*              pRanges)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_clear_color_image(&cmd_buffer->queue,
                                    image_h,
                                    imageLayout,
                                    pColor,
                                    rangeCount,
                                    pRanges);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdClearDepthStencilImage(
   VkCommandBuffer                             commandBuffer,
   VkImage                                     image_h,
   VkImageLayout                               imageLayout,
   const VkClearDepthStencilValue*             pDepthStencil,
   uint32_t                                    rangeCount,
   const VkImageSubresourceRange*              pRanges)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_clear_depth_stencil_image(&cmd_buffer->queue,
                                            image_h,
                                            imageLayout,
                                            pDepthStencil,
                                            rangeCount,
                                            pRanges);
}


VKAPI_ATTR void VKAPI_CALL lvp_CmdResolveImage2KHR(
   VkCommandBuffer                             commandBuffer,
   const VkResolveImageInfo2KHR               *info)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_resolve_image2_khr(&cmd_buffer->queue,
                                     info);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdResetQueryPool(
   VkCommandBuffer                             commandBuffer,
   VkQueryPool                                 queryPool,
   uint32_t                                    firstQuery,
   uint32_t                                    queryCount)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_reset_query_pool(&cmd_buffer->queue,
                                   queryPool,
                                   firstQuery,
                                   queryCount);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBeginQueryIndexedEXT(
   VkCommandBuffer                             commandBuffer,
   VkQueryPool                                 queryPool,
   uint32_t                                    query,
   VkQueryControlFlags                         flags,
   uint32_t                                    index)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_begin_query_indexed_ext(&cmd_buffer->queue,
                                          queryPool,
                                          query,
                                          flags,
                                          index);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBeginQuery(
   VkCommandBuffer                             commandBuffer,
   VkQueryPool                                 queryPool,
   uint32_t                                    query,
   VkQueryControlFlags                         flags)
{
   lvp_CmdBeginQueryIndexedEXT(commandBuffer, queryPool, query, flags, 0);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdEndQueryIndexedEXT(
   VkCommandBuffer                             commandBuffer,
   VkQueryPool                                 queryPool,
   uint32_t                                    query,
   uint32_t                                    index)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_end_query_indexed_ext(&cmd_buffer->queue,
                                        queryPool,
                                        query,
                                        index);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdEndQuery(
   VkCommandBuffer                             commandBuffer,
   VkQueryPool                                 queryPool,
   uint32_t                                    query)
{
   lvp_CmdEndQueryIndexedEXT(commandBuffer, queryPool, query, 0);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdWriteTimestamp(
   VkCommandBuffer                             commandBuffer,
   VkPipelineStageFlagBits                     pipelineStage,
   VkQueryPool                                 queryPool,
   uint32_t                                    query)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_write_timestamp(&cmd_buffer->queue,
                                  pipelineStage,
                                  queryPool,
                                  query);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdCopyQueryPoolResults(
   VkCommandBuffer                             commandBuffer,
   VkQueryPool                                 queryPool,
   uint32_t                                    firstQuery,
   uint32_t                                    queryCount,
   VkBuffer                                    dstBuffer,
   VkDeviceSize                                dstOffset,
   VkDeviceSize                                stride,
   VkQueryResultFlags                          flags)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_copy_query_pool_results(&cmd_buffer->queue,
                                          queryPool,
                                          firstQuery,
                                          queryCount,
                                          dstBuffer,
                                          dstOffset,
                                          stride,
                                          flags);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdPipelineBarrier(
   VkCommandBuffer                             commandBuffer,
   VkPipelineStageFlags                        srcStageMask,
   VkPipelineStageFlags                        destStageMask,
   VkBool32                                    byRegion,
   uint32_t                                    memoryBarrierCount,
   const VkMemoryBarrier*                      pMemoryBarriers,
   uint32_t                                    bufferMemoryBarrierCount,
   const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
   uint32_t                                    imageMemoryBarrierCount,
   const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_pipeline_barrier(&cmd_buffer->queue,
                                   srcStageMask,
                                   destStageMask,
                                   byRegion,
                                   memoryBarrierCount,
                                   pMemoryBarriers,
                                   bufferMemoryBarrierCount,
                                   pBufferMemoryBarriers,
                                   imageMemoryBarrierCount,
                                   pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawIndirectCount(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countBufferOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_draw_indirect_count(&cmd_buffer->queue,
                                      buffer,
                                      offset,
                                      countBuffer,
                                      countBufferOffset,
                                      maxDrawCount,
                                      stride);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawIndexedIndirectCount(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countBufferOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_draw_indexed_indirect_count(&cmd_buffer->queue,
                                              buffer,
                                              offset,
                                              countBuffer,
                                              countBufferOffset,
                                              maxDrawCount,
                                              stride);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdPushDescriptorSetKHR(
   VkCommandBuffer                             commandBuffer,
   VkPipelineBindPoint                         pipelineBindPoint,
   VkPipelineLayout                            layout,
   uint32_t                                    set,
   uint32_t                                    descriptorWriteCount,
   const VkWriteDescriptorSet*                 pDescriptorWrites)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);
   struct vk_cmd_push_descriptor_set_khr *pds;

   struct vk_cmd_queue_entry *cmd = vk_zalloc(cmd_buffer->queue.alloc,
                                              sizeof(*cmd), 8,
                                              VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   if (!cmd)
      return;

   pds = &cmd->u.push_descriptor_set_khr;

   cmd->type = VK_CMD_PUSH_DESCRIPTOR_SET_KHR;
   list_addtail(&cmd->cmd_link, &cmd_buffer->queue.cmds);

   pds->pipeline_bind_point = pipelineBindPoint;
   pds->layout = layout;
   pds->set = set;
   pds->descriptor_write_count = descriptorWriteCount;

   if (pDescriptorWrites) {
      pds->descriptor_writes = vk_zalloc(cmd_buffer->queue.alloc,
                                         sizeof(*pds->descriptor_writes) * descriptorWriteCount,
                                         8,
                                         VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      memcpy(pds->descriptor_writes,
             pDescriptorWrites,
             sizeof(*pds->descriptor_writes) * descriptorWriteCount);

      for (unsigned i = 0; i < descriptorWriteCount; i++) {
         switch (pds->descriptor_writes[i].descriptorType) {
         case VK_DESCRIPTOR_TYPE_SAMPLER:
         case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
         case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
         case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            pds->descriptor_writes[i].pImageInfo = vk_zalloc(cmd_buffer->queue.alloc,
                                         sizeof(VkDescriptorImageInfo) * pds->descriptor_writes[i].descriptorCount,
                                         8,
                                         VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
            memcpy((VkDescriptorImageInfo *)pds->descriptor_writes[i].pImageInfo,
                   pDescriptorWrites[i].pImageInfo,
                   sizeof(VkDescriptorImageInfo) * pds->descriptor_writes[i].descriptorCount);
            break;
         case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            pds->descriptor_writes[i].pTexelBufferView = vk_zalloc(cmd_buffer->queue.alloc,
                                         sizeof(VkBufferView) * pds->descriptor_writes[i].descriptorCount,
                                         8,
                                         VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
            memcpy((VkBufferView *)pds->descriptor_writes[i].pTexelBufferView,
                   pDescriptorWrites[i].pTexelBufferView,
                   sizeof(VkBufferView) * pds->descriptor_writes[i].descriptorCount);
            break;
         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         default:
            pds->descriptor_writes[i].pBufferInfo = vk_zalloc(cmd_buffer->queue.alloc,
                                         sizeof(VkDescriptorBufferInfo) * pds->descriptor_writes[i].descriptorCount,
                                         8,
                                         VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
            memcpy((VkDescriptorBufferInfo *)pds->descriptor_writes[i].pBufferInfo,
                   pDescriptorWrites[i].pBufferInfo,
                   sizeof(VkDescriptorBufferInfo) * pds->descriptor_writes[i].descriptorCount);
            break;
         }
      }
   }
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdPushDescriptorSetWithTemplateKHR(
   VkCommandBuffer                             commandBuffer,
   VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
   VkPipelineLayout                            layout,
   uint32_t                                    set,
   const void*                                 pData)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);
   LVP_FROM_HANDLE(lvp_descriptor_update_template, templ, descriptorUpdateTemplate);
   size_t info_size = 0;
   struct vk_cmd_queue_entry *cmd = vk_zalloc(cmd_buffer->queue.alloc,
                                              sizeof(*cmd), 8,
                                              VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   if (!cmd)
      return;

   cmd->type = VK_CMD_PUSH_DESCRIPTOR_SET_WITH_TEMPLATE_KHR;

   /* TODO: see cmd_buf_queue and last_emit */
   list_addtail(&cmd->cmd_link, &cmd_buffer->queue.cmds);

   cmd->u.push_descriptor_set_with_template_khr.descriptor_update_template = descriptorUpdateTemplate;
   cmd->u.push_descriptor_set_with_template_khr.layout = layout;
   cmd->u.push_descriptor_set_with_template_khr.set = set;

   for (unsigned i = 0; i < templ->entry_count; i++) {
      VkDescriptorUpdateTemplateEntry *entry = &templ->entry[i];

      if (entry->descriptorCount > 1) {
         info_size += entry->stride * entry->descriptorCount;
      } else {
         switch (entry->descriptorType) {
         case VK_DESCRIPTOR_TYPE_SAMPLER:
         case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
         case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
         case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            info_size += sizeof(VkDescriptorImageInfo);
            break;
         case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            info_size += sizeof(VkBufferView);
            break;
         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         default:
            info_size += sizeof(VkDescriptorBufferInfo);
            break;
         }
      }
   }

   cmd->u.push_descriptor_set_with_template_khr.data = vk_zalloc(cmd_buffer->queue.alloc, info_size, 8, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   memcpy(cmd->u.push_descriptor_set_with_template_khr.data, pData, info_size);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBindTransformFeedbackBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_bind_transform_feedback_buffers_ext(&cmd_buffer->queue,
                                                      firstBinding,
                                                      bindingCount,
                                                      pBuffers,
                                                      pOffsets,
                                                      pSizes);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBeginTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_begin_transform_feedback_ext(&cmd_buffer->queue,
                                               firstCounterBuffer,
                                               counterBufferCount,
                                               pCounterBuffers,
                                               pCounterBufferOffsets);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdEndTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_end_transform_feedback_ext(&cmd_buffer->queue,
                                             firstCounterBuffer,
                                             counterBufferCount,
                                             pCounterBuffers,
                                             pCounterBufferOffsets);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDrawIndirectByteCountEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    instanceCount,
    uint32_t                                    firstInstance,
    VkBuffer                                    counterBuffer,
    VkDeviceSize                                counterBufferOffset,
    uint32_t                                    counterOffset,
    uint32_t                                    vertexStride)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_draw_indirect_byte_count_ext(&cmd_buffer->queue,
                                               instanceCount,
                                               firstInstance,
                                               counterBuffer,
                                               counterBufferOffset,
                                               counterOffset,
                                               vertexStride);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDeviceMask(
   VkCommandBuffer commandBuffer,
   uint32_t deviceMask)
{
   /* No-op */
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdDispatchBase(
   VkCommandBuffer                             commandBuffer,
   uint32_t                                    base_x,
   uint32_t                                    base_y,
   uint32_t                                    base_z,
   uint32_t                                    x,
   uint32_t                                    y,
   uint32_t                                    z)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_dispatch_base(&cmd_buffer->queue,
                                base_x,
                                base_y,
                                base_z,
                                x,
                                y,
                                z);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBeginConditionalRenderingEXT(
   VkCommandBuffer commandBuffer,
   const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_begin_conditional_rendering_ext(&cmd_buffer->queue,
                                                  pConditionalRenderingBegin);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdEndConditionalRenderingEXT(
   VkCommandBuffer commandBuffer)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_end_conditional_rendering_ext(&cmd_buffer->queue);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetCullModeEXT(
    VkCommandBuffer                             commandBuffer,
    VkCullModeFlags                             cullMode)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_cull_mode_ext(&cmd_buffer->queue,
                                    cullMode);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetVertexInputEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT*  pVertexBindingDescriptions,
    uint32_t                                    vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_vertex_input_ext(&cmd_buffer->queue,
                                       vertexBindingDescriptionCount,
                                       pVertexBindingDescriptions,
                                       vertexAttributeDescriptionCount,
                                       pVertexAttributeDescriptions);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetFrontFaceEXT(
    VkCommandBuffer                             commandBuffer,
    VkFrontFace                                 frontFace)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_front_face_ext(&cmd_buffer->queue,
                                     frontFace);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetLineStippleEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    lineStippleFactor,
    uint16_t                                    lineStipplePattern)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_line_stipple_ext(&cmd_buffer->queue,
                                       lineStippleFactor,
                                       lineStipplePattern);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetPrimitiveTopologyEXT(
    VkCommandBuffer                             commandBuffer,
    VkPrimitiveTopology                         primitiveTopology)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_primitive_topology_ext(&cmd_buffer->queue,
                                             primitiveTopology);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetViewportWithCountEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    viewportCount,
    const VkViewport*                           pViewports)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_viewport_with_count_ext(&cmd_buffer->queue,
                                              viewportCount,
                                              pViewports);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetScissorWithCountEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_scissor_with_count_ext(&cmd_buffer->queue,
                                             scissorCount,
                                             pScissors);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdBindVertexBuffers2EXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes,
    const VkDeviceSize*                         pStrides)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_bind_vertex_buffers2_ext(&cmd_buffer->queue,
                                           firstBinding,
                                           bindingCount,
                                           pBuffers,
                                           pOffsets,
                                           pSizes,
                                           pStrides);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDepthTestEnableEXT(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthTestEnable)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_depth_test_enable_ext(&cmd_buffer->queue,
                                            depthTestEnable);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDepthWriteEnableEXT(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthWriteEnable)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_depth_write_enable_ext(&cmd_buffer->queue,
                                             depthWriteEnable);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDepthCompareOpEXT(
    VkCommandBuffer                             commandBuffer,
    VkCompareOp                                 depthCompareOp)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_depth_compare_op_ext(&cmd_buffer->queue,
                                           depthCompareOp);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDepthBoundsTestEnableEXT(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthBoundsTestEnable)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_depth_bounds_test_enable_ext(&cmd_buffer->queue,
                                                   depthBoundsTestEnable);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetStencilTestEnableEXT(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    stencilTestEnable)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_stencil_test_enable_ext(&cmd_buffer->queue,
                                              stencilTestEnable);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetStencilOpEXT(
    VkCommandBuffer                             commandBuffer,
    VkStencilFaceFlags                          faceMask,
    VkStencilOp                                 failOp,
    VkStencilOp                                 passOp,
    VkStencilOp                                 depthFailOp,
    VkCompareOp                                 compareOp)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_stencil_op_ext(&cmd_buffer->queue,
                                     faceMask,
                                     failOp,
                                     passOp,
                                     depthFailOp,
                                     compareOp);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetDepthBiasEnableEXT(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthBiasEnable)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_depth_bias_enable_ext(&cmd_buffer->queue,
                                            depthBiasEnable);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetLogicOpEXT(
    VkCommandBuffer                             commandBuffer,
    VkLogicOp                                   logicOp)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_logic_op_ext(&cmd_buffer->queue,
                                   logicOp);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetPatchControlPointsEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    patchControlPoints)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_patch_control_points_ext(&cmd_buffer->queue,
                                               patchControlPoints);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetPrimitiveRestartEnableEXT(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    primitiveRestartEnable)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_patch_control_points_ext(&cmd_buffer->queue,
                                               primitiveRestartEnable);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetRasterizerDiscardEnableEXT(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    rasterizerDiscardEnable)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_rasterizer_discard_enable_ext(&cmd_buffer->queue,
                                                    rasterizerDiscardEnable);
}

VKAPI_ATTR void VKAPI_CALL lvp_CmdSetColorWriteEnableEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    attachmentCount,
    const VkBool32*                             pColorWriteEnables)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   vk_enqueue_cmd_set_color_write_enable_ext(&cmd_buffer->queue,
                                             attachmentCount,
                                             pColorWriteEnables);
}
