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

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateMicromapEXT(VkDevice device, const VkMicromapCreateInfoEXT *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator, VkMicromapEXT *pMicromap)
{
   unreachable("Unimplemented");
}

VKAPI_ATTR void VKAPI_CALL
radv_DestroyMicromapEXT(VkDevice device, VkMicromapEXT micromap,
                        const VkAllocationCallbacks *pAllocator)
{
   unreachable("Unimplemented");
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
   unreachable("Unimplemented");
}
