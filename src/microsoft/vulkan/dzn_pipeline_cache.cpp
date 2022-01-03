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

#include "dzn_private.h"

#include "vk_alloc.h"

dzn_pipeline_cache::dzn_pipeline_cache(dzn_device *device,
                                       const VkPipelineCacheCreateInfo *pCreateInfo,
                                       const VkAllocationCallbacks *pAllocator)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_PIPELINE_CACHE);
   /* TODO: cache-ism! */
}

dzn_pipeline_cache::~dzn_pipeline_cache()
{
   vk_object_base_finish(&base);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreatePipelineCache(VkDevice device,
                        const VkPipelineCacheCreateInfo *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator,
                        VkPipelineCache *pPipelineCache)
{
   return dzn_pipeline_cache_factory::create(device, pCreateInfo,
                                             pAllocator, pPipelineCache);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyPipelineCache(VkDevice device,
                         VkPipelineCache pipelineCache,
                         const VkAllocationCallbacks *pAllocator)
{
   return dzn_pipeline_cache_factory::destroy(device, pipelineCache,
                                              pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPipelineCacheData(VkDevice device,
                         VkPipelineCache pipelineCache,
                         size_t *pDataSize,
                         void *pData)
{
   // FIXME
   dzn_stub();
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_MergePipelineCaches(VkDevice device,
                        VkPipelineCache dstCache,
                        uint32_t srcCacheCount,
                        const VkPipelineCache *pSrcCaches)
{
   // FIXME
   dzn_stub();
   return VK_SUCCESS;
}
