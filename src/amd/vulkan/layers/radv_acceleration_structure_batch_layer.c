/*
 * Copyright © 2023 Valve Corporation
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

#include "radv_private.h"

struct radv_acceleration_structure_build {
   struct list_head item;
   VkAccelerationStructureBuildGeometryInfoKHR info;
   /* Geometry infos and range infos */
};

VKAPI_ATTR void VKAPI_CALL
batch_CmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount,
                                        const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
                                        const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   for (uint32_t i = 0; i < infoCount; i++) {
      VkAccelerationStructureBuildGeometryInfoKHR info = pInfos[i];

      /* Always flush when the acceleration structure type changes in case the application is missing doesn't
       * synchronize properly. Since TLAS builds are rare, this should be cheap.
       */
      if (info.type != cmd_buffer->batch_state.last_accel_struct_type) {
         if (!util_dynarray_num_elements(&cmd_buffer->batch_state.accel_struct_build_infos,
                                         VkAccelerationStructureBuildGeometryInfoKHR)) {
            VkMemoryBarrier2 barrier = {
               .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
               .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
               .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
               .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
               .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            };
            VkDependencyInfo dependency = {
               .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
               .memoryBarrierCount = 1,
               .pMemoryBarriers = &barrier,
            };
            batch_CmdPipelineBarrier2(commandBuffer, &dependency);
         }
         cmd_buffer->batch_state.last_accel_struct_type = info.type;
      }

      uint32_t range_infos_size = info.geometryCount * sizeof(VkAccelerationStructureBuildRangeInfoKHR);
      uint32_t geometries_size = info.geometryCount * sizeof(VkAccelerationStructureGeometryKHR);
      uint8_t *geometry_info = malloc(range_infos_size + geometries_size);
      if (!geometry_info) {
         vk_command_buffer_set_error(&cmd_buffer->vk, VK_ERROR_OUT_OF_HOST_MEMORY);
         return;
      }

      VkAccelerationStructureBuildRangeInfoKHR *range_infos = (void *)geometry_info;
      memcpy(range_infos, ppBuildRangeInfos[i], range_infos_size);

      VkAccelerationStructureGeometryKHR *geometries = (void *)(geometry_info + range_infos_size);
      info.pGeometries = geometries;
      info.ppGeometries = NULL;
      if (pInfos->pGeometries) {
         memcpy(geometries, pInfos->pGeometries, geometries_size);
      } else {
         for (uint32_t j = 0; j < info.geometryCount; j++)
            geometries[j] = *pInfos->ppGeometries[j];
      }

      util_dynarray_append(&cmd_buffer->batch_state.accel_struct_build_infos,
                           VkAccelerationStructureBuildGeometryInfoKHR, info);
      util_dynarray_append(&cmd_buffer->batch_state.accel_struct_geometry_infos, void *, geometry_info);
   }
}

static void
radv_batch_state_handle_dependency(struct radv_cmd_buffer *cmd_buffer, const VkDependencyInfo *dependency)
{
   uint32_t build_count = util_dynarray_num_elements(&cmd_buffer->batch_state.accel_struct_build_infos,
                                                     VkAccelerationStructureBuildGeometryInfoKHR);
   if (!build_count)
      return;

   VkPipelineStageFlags2 src_stage_mask = 0;

   for (uint32_t i = 0; i < dependency->memoryBarrierCount; i++)
      src_stage_mask |= dependency->pMemoryBarriers[i].srcStageMask;
   for (uint32_t i = 0; i < dependency->bufferMemoryBarrierCount; i++)
      src_stage_mask |= dependency->pBufferMemoryBarriers[i].srcStageMask;
   for (uint32_t i = 0; i < dependency->imageMemoryBarrierCount; i++)
      src_stage_mask |= dependency->pImageMemoryBarriers[i].srcStageMask;

   uint32_t flush_stage_mask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
                               VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   if (!(src_stage_mask & flush_stage_mask))
      return;

   cmd_buffer->device->layer_dispatch.app.CmdBuildAccelerationStructuresKHR(
      radv_cmd_buffer_to_handle(cmd_buffer), build_count, cmd_buffer->batch_state.accel_struct_build_infos.data,
      cmd_buffer->batch_state.accel_struct_geometry_infos.data);

   util_dynarray_clear(&cmd_buffer->batch_state.accel_struct_build_infos);
   util_dynarray_clear(&cmd_buffer->batch_state.accel_struct_geometry_infos);
}

VKAPI_ATTR void VKAPI_CALL
batch_CmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                     const VkDependencyInfo *pDependencyInfos)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   for (uint32_t i = 0; i < eventCount; i++)
      radv_batch_state_handle_dependency(cmd_buffer, &pDependencyInfos[i]);

   cmd_buffer->device->layer_dispatch.app.CmdWaitEvents2(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

VKAPI_ATTR void VKAPI_CALL
batch_CmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo *pDependencyInfo)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   radv_batch_state_handle_dependency(cmd_buffer, pDependencyInfo);

   cmd_buffer->device->layer_dispatch.app.CmdPipelineBarrier2(commandBuffer, pDependencyInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL
batch_EndCommandBuffer(VkCommandBuffer commandBuffer)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   VkMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
      .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
   };
   VkDependencyInfo dependency = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .memoryBarrierCount = 1,
      .pMemoryBarriers = &barrier,
   };
   radv_batch_state_handle_dependency(cmd_buffer, &dependency);

   return cmd_buffer->device->layer_dispatch.app.EndCommandBuffer(commandBuffer);
}
