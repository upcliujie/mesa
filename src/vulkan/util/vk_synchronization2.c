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

#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_util.h"

#define STACK_ARRAY_SIZE 8

#define STACK_ARRAY(type, name, size) \
   type _stack_##name[STACK_ARRAY_SIZE], *const name = \
      (size) <= STACK_ARRAY_SIZE ? _stack_##name : malloc((size) * sizeof(type))

#define STACK_ARRAY_FINISH(name) \
   if (name != _stack_##name) free(name)

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdWriteTimestamp(
   VkCommandBuffer                             commandBuffer,
   VkPipelineStageFlagBits                     pipelineStage,
   VkQueryPool                                 queryPool,
   uint32_t                                    query)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   disp->device->dispatch_table.CmdWriteTimestamp2KHR(commandBuffer,
                                                      (VkPipelineStageFlags2KHR) pipelineStage,
                                                      queryPool,
                                                      query);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdPipelineBarrier(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    VkDependencyFlags                           dependencyFlags,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   STACK_ARRAY(VkMemoryBarrier2KHR, memory_barriers, memoryBarrierCount);
   STACK_ARRAY(VkBufferMemoryBarrier2KHR, buffer_barriers, bufferMemoryBarrierCount);
   STACK_ARRAY(VkImageMemoryBarrier2KHR, image_barriers, imageMemoryBarrierCount);

   VkPipelineStageFlags2KHR src_stage_mask2 = (VkPipelineStageFlags2KHR) srcStageMask;
   VkPipelineStageFlags2KHR dst_stage_mask2 = (VkPipelineStageFlags2KHR) dstStageMask;

   for (uint32_t i = 0; i < memoryBarrierCount; i++) {
      memory_barriers[i] = (VkMemoryBarrier2KHR) {
         .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
         .srcStageMask  = src_stage_mask2,
         .srcAccessMask = (VkAccessFlags2KHR) pMemoryBarriers[i].srcAccessMask,
         .dstStageMask  = dst_stage_mask2,
         .dstAccessMask = (VkAccessFlags2KHR) pMemoryBarriers[i].dstAccessMask,
      };
   }
   for (uint32_t i = 0; i < bufferMemoryBarrierCount; i++) {
      buffer_barriers[i] = (VkBufferMemoryBarrier2KHR) {
         .sType                = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
         .srcStageMask         = src_stage_mask2,
         .srcAccessMask        = (VkAccessFlags2KHR) pBufferMemoryBarriers[i].srcAccessMask,
         .dstStageMask         = dst_stage_mask2,
         .dstAccessMask        = (VkAccessFlags2KHR) pBufferMemoryBarriers[i].dstAccessMask,
         .srcQueueFamilyIndex  = pBufferMemoryBarriers[i].srcQueueFamilyIndex,
         .dstQueueFamilyIndex  = pBufferMemoryBarriers[i].dstQueueFamilyIndex,
         .buffer               = pBufferMemoryBarriers[i].buffer,
         .offset               = pBufferMemoryBarriers[i].offset,
         .size                 = pBufferMemoryBarriers[i].size,
      };
   }
   for (uint32_t i = 0; i < imageMemoryBarrierCount; i++) {
      image_barriers[i] = (VkImageMemoryBarrier2KHR) {
         .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
         .srcStageMask         = src_stage_mask2,
         .srcAccessMask        = (VkAccessFlags2KHR) pImageMemoryBarriers[i].srcAccessMask,
         .dstStageMask         = dst_stage_mask2,
         .dstAccessMask        = (VkAccessFlags2KHR) pImageMemoryBarriers[i].dstAccessMask,
         .oldLayout            = pImageMemoryBarriers[i].oldLayout,
         .newLayout            = pImageMemoryBarriers[i].newLayout,
         .srcQueueFamilyIndex  = pImageMemoryBarriers[i].srcQueueFamilyIndex,
         .dstQueueFamilyIndex  = pImageMemoryBarriers[i].dstQueueFamilyIndex,
         .image                = pImageMemoryBarriers[i].image,
         .subresourceRange     = pImageMemoryBarriers[i].subresourceRange,
      };
   }

   VkDependencyInfoKHR dep_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .memoryBarrierCount = memoryBarrierCount,
      .pMemoryBarriers = memory_barriers,
      .bufferMemoryBarrierCount = bufferMemoryBarrierCount,
      .pBufferMemoryBarriers = buffer_barriers,
      .imageMemoryBarrierCount = imageMemoryBarrierCount,
      .pImageMemoryBarriers = image_barriers,
   };

   disp->device->dispatch_table.CmdPipelineBarrier2KHR(commandBuffer, &dep_info);

   STACK_ARRAY_FINISH(memory_barriers);
   STACK_ARRAY_FINISH(buffer_barriers);
   STACK_ARRAY_FINISH(image_barriers);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdSetEvent(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   VkMemoryBarrier2KHR mem_barrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
      .srcStageMask = (VkPipelineStageFlags2KHR) stageMask,
      .dstStageMask = (VkPipelineStageFlags2KHR) stageMask,
   };
   VkDependencyInfoKHR dep_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .memoryBarrierCount = 1,
      .pMemoryBarriers = &mem_barrier,
   };

   disp->device->dispatch_table.CmdSetEvent2KHR(commandBuffer, event, &dep_info);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdResetEvent(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   disp->device->dispatch_table.CmdResetEvent2KHR(commandBuffer,
                                                  event,
                                                  (VkPipelineStageFlags2KHR) stageMask);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdWaitEvents(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        destStageMask,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   STACK_ARRAY(VkMemoryBarrier2KHR, memory_barriers, memoryBarrierCount);
   STACK_ARRAY(VkBufferMemoryBarrier2KHR, buffer_barriers, bufferMemoryBarrierCount);
   STACK_ARRAY(VkImageMemoryBarrier2KHR, image_barriers, imageMemoryBarrierCount);

   VkPipelineStageFlags2KHR src_stage_mask2 = (VkPipelineStageFlags2KHR) srcStageMask;
   VkPipelineStageFlags2KHR dst_stage_mask2 = (VkPipelineStageFlags2KHR) destStageMask;

   for (uint32_t i = 0; i < memoryBarrierCount; i++) {
      memory_barriers[i] = (VkMemoryBarrier2KHR) {
         .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
         .srcStageMask  = src_stage_mask2,
         .srcAccessMask = (VkAccessFlags2KHR) pMemoryBarriers[i].srcAccessMask,
         .dstStageMask  = dst_stage_mask2,
         .dstAccessMask = (VkAccessFlags2KHR) pMemoryBarriers[i].dstAccessMask,
      };
   }
   for (uint32_t i = 0; i < bufferMemoryBarrierCount; i++) {
      buffer_barriers[i] = (VkBufferMemoryBarrier2KHR) {
         .sType                = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
         .srcStageMask         = src_stage_mask2,
         .srcAccessMask        = (VkAccessFlags2KHR) pBufferMemoryBarriers[i].srcAccessMask,
         .dstStageMask         = dst_stage_mask2,
         .dstAccessMask        = (VkAccessFlags2KHR) pBufferMemoryBarriers[i].dstAccessMask,
         .srcQueueFamilyIndex  = pBufferMemoryBarriers[i].srcQueueFamilyIndex,
         .dstQueueFamilyIndex  = pBufferMemoryBarriers[i].dstQueueFamilyIndex,
         .buffer               = pBufferMemoryBarriers[i].buffer,
         .offset               = pBufferMemoryBarriers[i].offset,
         .size                 = pBufferMemoryBarriers[i].size,
      };
   }
   for (uint32_t i = 0; i < imageMemoryBarrierCount; i++) {
      image_barriers[i] = (VkImageMemoryBarrier2KHR) {
         .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
         .srcStageMask         = src_stage_mask2,
         .srcAccessMask        = (VkAccessFlags2KHR) pImageMemoryBarriers[i].srcAccessMask,
         .dstStageMask         = dst_stage_mask2,
         .dstAccessMask        = (VkAccessFlags2KHR) pImageMemoryBarriers[i].dstAccessMask,
         .oldLayout            = pImageMemoryBarriers[i].oldLayout,
         .newLayout            = pImageMemoryBarriers[i].newLayout,
         .srcQueueFamilyIndex  = pImageMemoryBarriers[i].srcQueueFamilyIndex,
         .dstQueueFamilyIndex  = pImageMemoryBarriers[i].dstQueueFamilyIndex,
         .image                = pImageMemoryBarriers[i].image,
         .subresourceRange     = pImageMemoryBarriers[i].subresourceRange,
      };
   }

   VkDependencyInfoKHR dep_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .memoryBarrierCount = memoryBarrierCount,
      .pMemoryBarriers = memory_barriers,
      .bufferMemoryBarrierCount = bufferMemoryBarrierCount,
      .pBufferMemoryBarriers = buffer_barriers,
      .imageMemoryBarrierCount = imageMemoryBarrierCount,
      .pImageMemoryBarriers = image_barriers,
   };

   disp->device->dispatch_table.CmdWaitEvents2KHR(commandBuffer,
                                                  eventCount,
                                                  pEvents,
                                                  &dep_info);

   STACK_ARRAY_FINISH(memory_barriers);
   STACK_ARRAY_FINISH(buffer_barriers);
   STACK_ARRAY_FINISH(image_barriers);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdWriteBufferMarkerAMD(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlagBits                     pipelineStage,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    uint32_t                                    marker)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   disp->device->dispatch_table.CmdWriteBufferMarker2AMD(commandBuffer,
                                                         (VkPipelineStageFlags2KHR) pipelineStage,
                                                         dstBuffer,
                                                         dstOffset,
                                                         marker);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetQueueCheckpointDataNV(
    VkQueue                                     queue,
    uint32_t*                                   pCheckpointDataCount,
    VkCheckpointDataNV*                         pCheckpointData)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)queue;

   uint32_t n_checkpoint_data_2 = 0;
   disp->device->dispatch_table.GetQueueCheckpointData2NV(queue, &n_checkpoint_data_2, NULL);

   STACK_ARRAY(VkCheckpointData2NV, checkpoint_data_2, n_checkpoint_data_2);
   disp->device->dispatch_table.GetQueueCheckpointData2NV(queue, &n_checkpoint_data_2, checkpoint_data_2);

   VK_OUTARRAY_MAKE(out, pCheckpointData, pCheckpointDataCount);

   for (uint32_t i = 0; i < n_checkpoint_data_2; i++) {
      vk_outarray_append(&out, check_point) {
         check_point->stage = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM & checkpoint_data_2[i].stage;
         check_point->pCheckpointMarker = checkpoint_data_2[i].pCheckpointMarker;
      }
   }

   STACK_ARRAY_FINISH(checkpoint_data_2);
}

static inline VkBaseInStructure *
append_in_struct(VkBaseInStructure *first, VkBaseInStructure *item)
{
   if (!first)
      return item;

   VkBaseInStructure *iter = first;
   while (iter->pNext)
      iter = (VkBaseInStructure *) iter->pNext;
   iter->pNext = item;

   return first;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_QueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)queue;

   STACK_ARRAY(VkSubmitInfo2KHR, submit_info_2, submitCount);
   STACK_ARRAY(VkPerformanceQuerySubmitInfoKHR, perf_query_submit_info, submitCount);

   for (uint32_t s = 0; s < submitCount; s++) {
      const VkTimelineSemaphoreSubmitInfoKHR *timeline_info =
         vk_find_struct_const(pSubmits[s].pNext,
                              TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR);
      const uint64_t *wait_values =
         timeline_info && timeline_info->waitSemaphoreValueCount ?
         timeline_info->pWaitSemaphoreValues : NULL;
      const uint64_t *signal_values =
         timeline_info && timeline_info->signalSemaphoreValueCount ?
         timeline_info->pSignalSemaphoreValues : NULL;

      const VkDeviceGroupSubmitInfo *group_info =
         vk_find_struct_const(pSubmits[s].pNext, DEVICE_GROUP_SUBMIT_INFO);

      VkSemaphoreSubmitInfoKHR *wait_semaphores;
      VkCommandBufferSubmitInfoKHR *command_buffers;
      VkSemaphoreSubmitInfoKHR *signal_semaphores;

      wait_semaphores = malloc(sizeof(*wait_semaphores) * pSubmits[s].waitSemaphoreCount);
      command_buffers = malloc(sizeof(*command_buffers) * pSubmits[s].commandBufferCount);
      signal_semaphores = malloc(sizeof(*signal_semaphores) * pSubmits[s].signalSemaphoreCount);

      for (uint32_t i = 0; i < pSubmits[s].waitSemaphoreCount; i++) {
         wait_semaphores[i] = (VkSemaphoreSubmitInfoKHR) {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
            .semaphore   = pSubmits[s].pWaitSemaphores[i],
            .value       = wait_values ? wait_values[i] : 0,
            .stageMask   = pSubmits[s].pWaitDstStageMask[i],
            .deviceIndex = group_info ? group_info->pWaitSemaphoreDeviceIndices[i] : 0,
         };
      }
      for (uint32_t i = 0; i < pSubmits[s].commandBufferCount; i++) {
         command_buffers[i] = (VkCommandBufferSubmitInfoKHR) {
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
            .commandBuffer = pSubmits[s].pCommandBuffers[i],
            .deviceMask    = group_info ? group_info->pCommandBufferDeviceMasks[i] : 0,
         };
      }
      for (uint32_t i = 0; i < pSubmits[s].signalSemaphoreCount; i++) {
         signal_semaphores[i] = (VkSemaphoreSubmitInfoKHR) {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
            .semaphore = pSubmits[s].pSignalSemaphores[i],
            .value     = signal_values ? signal_values[i] : 0,
            .stageMask = 0,
            .deviceIndex = group_info ? group_info->pSignalSemaphoreDeviceIndices[i] : 0,
         };
      }

      VkBaseInStructure *p_next = NULL;

      const VkPerformanceQuerySubmitInfoKHR *query_info =
         vk_find_struct_const(pSubmits[s].pNext,
                              PERFORMANCE_QUERY_SUBMIT_INFO_KHR);
      if (query_info) {
         perf_query_submit_info[s] = *query_info;
         append_in_struct(p_next,
                          (VkBaseInStructure *) &perf_query_submit_info[s]);
      }

      const VkProtectedSubmitInfo *protected_info =
         vk_find_struct_const(pSubmits[s].pNext, PROTECTED_SUBMIT_INFO);

      submit_info_2[s] = (VkSubmitInfo2KHR) {
         .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
         .pNext                    = p_next,
         .flags                    = ((protected_info && protected_info->protectedSubmit) ?
                                      VK_SUBMIT_PROTECTED_BIT_KHR : 0),
         .waitSemaphoreInfoCount   = pSubmits[s].waitSemaphoreCount,
         .pWaitSemaphoreInfos      = wait_semaphores,
         .commandBufferInfoCount   = pSubmits[s].commandBufferCount,
         .pCommandBufferInfos      = command_buffers,
         .signalSemaphoreInfoCount = pSubmits[s].signalSemaphoreCount,
         .pSignalSemaphoreInfos    = signal_semaphores,
      };
   }

   VkResult result = disp->device->dispatch_table.QueueSubmit2KHR(queue,
                                                                  submitCount,
                                                                  submit_info_2,
                                                                  fence);

   for (uint32_t s = 0; s < submitCount; s++) {
      free((void *) submit_info_2[s].pWaitSemaphoreInfos);
      free((void *) submit_info_2[s].pCommandBufferInfos);
      free((void *) submit_info_2[s].pSignalSemaphoreInfos);
   }

   STACK_ARRAY_FINISH(submit_info_2);

   return result;
}
