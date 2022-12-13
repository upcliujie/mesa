/*
 * Copyright © 2022 Valve Corporation
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
 *
 */

#include "radv_micromap.h"

#include "radv_acceleration_structure.h"
#include "radv_meta.h"

#include "bvh/build_interface.h"

static const uint32_t micromap_spv[] = {
#include "bvh/micromap.spv.h"
};

VkResult
radv_device_init_micromap_build_state(struct radv_device *device)
{
   return radv_create_build_pipeline(
      device, micromap_spv, sizeof(micromap_spv), sizeof(struct micromap_args),
      &device->meta_state.micromap_build.pipeline, &device->meta_state.micromap_build.p_layout);
}

void
radv_device_finish_micromap_build_state(struct radv_device *device)
{
   struct radv_meta_state *state = &device->meta_state;
   radv_DestroyPipeline(radv_device_to_handle(device), state->micromap_build.pipeline,
                        &state->alloc);
   radv_DestroyPipelineLayout(radv_device_to_handle(device), state->micromap_build.p_layout,
                              &state->alloc);
}

struct micromap_layout {
   uint32_t triangle_count;

   uint32_t data_offset;
   uint32_t data_size;

   uint32_t size;
};

static struct micromap_layout
get_micromap_layout(const VkMicromapBuildInfoEXT *pBuildInfo)
{
   struct micromap_layout layout = {0};

   for (uint32_t i = 0; i < pBuildInfo->usageCountsCount; i++) {
      const VkMicromapUsageEXT *usage;
      if (pBuildInfo->pUsageCounts)
         usage = pBuildInfo->pUsageCounts + i;
      else
         usage = pBuildInfo->ppUsageCounts[i];

      layout.triangle_count += usage->count;
      layout.data_offset += usage->count * sizeof(struct micromap_triangle_header);

      uint32_t subtriangle_count = 1u << (usage->subdivisionLevel * 2);
      /* Always use 4-state opacity to avoid divergence during traversal. */
      layout.data_size += usage->count * MAX2(subtriangle_count * 2 / 8, sizeof(uint32_t));
   }

   layout.size = layout.data_offset + layout.data_size;

   return layout;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateMicromapEXT(VkDevice _device, const VkMicromapCreateInfoEXT *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator, VkMicromapEXT *pMicromap)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_buffer, buffer, pCreateInfo->buffer);

   struct radv_micromap *micromap =
      vk_alloc2(&device->vk.alloc, pAllocator, sizeof(struct radv_micromap), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!micromap)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   vk_object_base_init(&device->vk, &micromap->base, VK_OBJECT_TYPE_MICROMAP_EXT);

   micromap->bo = buffer->bo;
   micromap->mem_offset = buffer->offset + pCreateInfo->offset;
   micromap->size = pCreateInfo->size;
   micromap->va = radv_buffer_get_va(micromap->bo) + micromap->mem_offset;

   *pMicromap = radv_micromap_to_handle(micromap);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
radv_DestroyMicromapEXT(VkDevice _device, VkMicromapEXT _micromap,
                        const VkAllocationCallbacks *pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_micromap, micromap, _micromap);

   if (!micromap)
      return;

   vk_object_base_finish(&micromap->base);
   vk_free2(&device->vk.alloc, pAllocator, micromap);
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdBuildMicromapsEXT(VkCommandBuffer commandBuffer, uint32_t infoCount,
                          const VkMicromapBuildInfoEXT *pInfos)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_BuildMicromapsEXT(VkDevice device, VkDeferredOperationKHR deferredOperation,
                       uint32_t infoCount, const VkMicromapBuildInfoEXT *pInfos)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CopyMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation,
                     const VkCopyMicromapInfoEXT *pInfo)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CopyMicromapToMemoryEXT(VkDevice device, VkDeferredOperationKHR deferredOperation,
                             const VkCopyMicromapToMemoryInfoEXT *pInfo)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CopyMemoryToMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation,
                             const VkCopyMemoryToMicromapInfoEXT *pInfo)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_WriteMicromapsPropertiesEXT(VkDevice device, uint32_t micromapCount,
                                 const VkMicromapEXT *pMicromaps, VkQueryType queryType,
                                 size_t dataSize, void *pData, size_t stride)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdCopyMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapInfoEXT *pInfo)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdCopyMicromapToMemoryEXT(VkCommandBuffer commandBuffer,
                                const VkCopyMicromapToMemoryInfoEXT *pInfo)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdCopyMemoryToMicromapEXT(VkCommandBuffer commandBuffer,
                                const VkCopyMemoryToMicromapInfoEXT *pInfo)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR void VKAPI_CALL
radv_GetDeviceMicromapCompatibilityEXT(VkDevice device,
                                       const VkMicromapVersionInfoEXT *pVersionInfo,
                                       VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR void VKAPI_CALL
radv_GetMicromapBuildSizesEXT(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType,
                              const VkMicromapBuildInfoEXT *pBuildInfo,
                              VkMicromapBuildSizesInfoEXT *pSizeInfo)
{
   pSizeInfo->micromapSize = get_micromap_layout(pBuildInfo).size;
   pSizeInfo->buildScratchSize = sizeof(uint32_t);
   pSizeInfo->discardable = false;
}
