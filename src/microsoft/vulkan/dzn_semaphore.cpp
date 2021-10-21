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

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateSemaphore(VkDevice _device,
                    const VkSemaphoreCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkSemaphore *pSemaphore)
{
   VK_FROM_HANDLE(dzn_device, device, _device);

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);

   dzn_semaphore *sem = (dzn_semaphore *)
      vk_object_alloc(&device->vk, pAllocator,
                      sizeof(*sem), VK_OBJECT_TYPE_SEMAPHORE);
   if (sem == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   /* TODO: do something useful ;) */

   *pSemaphore = dzn_semaphore_to_handle(sem);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroySemaphore(VkDevice _device,
                     VkSemaphore semaphore,
                     const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_semaphore, sem, semaphore);

   if (sem == NULL)
      return;

   /* TODO: do something useful ;) */

   vk_object_free(&device->vk, pAllocator, sem);
}
