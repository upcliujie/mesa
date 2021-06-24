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
dzn_CreateFence(VkDevice _device,
                const VkFenceCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkFence *pFence)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);

   dzn_fence *fence = (dzn_fence *)
      vk_object_alloc(&device->vk, pAllocator, sizeof(*fence),
                      VK_OBJECT_TYPE_FENCE);
   if (fence == NULL)
      return vk_errorfi(device->instance, NULL, VK_ERROR_OUT_OF_HOST_MEMORY, NULL);

   BOOL initial_state = FALSE;
   if (pCreateInfo->flags & VK_FENCE_CREATE_SIGNALED_BIT) {
      initial_state = TRUE;
   }

   fence->value = 0;
   /* I suspect that this approach is bunk, and we should instead create the actual
    * fence object on the cmdqueue and just signal things on the dzn_fence object
    * instead. But I don't know yet, so let's just leave it like this for now...
    */
   if (FAILED(device->dev->CreateFence(fence->value, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&fence->fence)))) {
      vk_object_free(&device->vk, pAllocator, fence);
      return vk_errorfi(device->instance, NULL, VK_ERROR_OUT_OF_HOST_MEMORY, NULL);
   }

   fence->event = CreateEventA(NULL, TRUE, initial_state, NULL);

   *pFence = dzn_fence_to_handle(fence);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyFence(VkDevice _device,
                 VkFence _fence,
                 const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_fence, fence, _fence);

   if (fence == NULL)
      return;

   fence->fence->Release();
   CloseHandle(fence->event);

   vk_object_free(&device->vk, pAllocator, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetFenceStatus(VkDevice _device,
                   VkFence _fence)
{
   DZN_FROM_HANDLE(dzn_fence, fence, _fence);

   if (fence->fence->GetCompletedValue() < fence->value)
      return VK_NOT_READY;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_ResetFences(VkDevice _device,
                uint32_t fenceCount,
                const VkFence *pFences)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);

   for (uint32_t i = 0; i < fenceCount; i++) {
      DZN_FROM_HANDLE(dzn_fence, fence, pFences[i]);
      ResetEvent(fence->event);
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_WaitForFences(VkDevice _device,
                  uint32_t fenceCount,
                  const VkFence *pFences,
                  VkBool32 waitAll,
                  uint64_t timeout)
{
   /* TODO: deal with more fences! */
   assert(fenceCount < MAXIMUM_WAIT_OBJECTS);

   HANDLE handles[MAXIMUM_WAIT_OBJECTS];
   for (int i = 0; i < fenceCount; ++i) {
      DZN_FROM_HANDLE(dzn_fence, fence, pFences[i]);
      handles[i] = fence;
   }
   HRESULT hr = WaitForMultipleObjects(fenceCount, handles, waitAll, timeout / 1000000);

   if (hr == WAIT_TIMEOUT)
      return VK_TIMEOUT;      

   return VK_SUCCESS;
}
