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
#include "vk_debug_report.h"
#include "vk_util.h"

#include "util/macros.h"

dzn_semaphore::dzn_semaphore(dzn_device *device,
                             const VkSemaphoreCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_SEMAPHORE);
   /* TODO: do something useful ;) */
}

dzn_semaphore::~dzn_semaphore()
{
   vk_object_base_finish(&base);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateSemaphore(VkDevice device,
                    const VkSemaphoreCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkSemaphore *pSemaphore)
{
   return dzn_semaphore_factory::create(device, pCreateInfo, pAllocator, pSemaphore);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroySemaphore(VkDevice device,
                     VkSemaphore semaphore,
                     const VkAllocationCallbacks *pAllocator)
{
   return dzn_semaphore_factory::destroy(device, semaphore, pAllocator);
}
