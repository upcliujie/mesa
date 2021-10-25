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

dzn_fence::dzn_fence(dzn_device *device,
                     const VkFenceCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator)
{
   BOOL initial_state =
      pCreateInfo->flags & VK_FENCE_CREATE_SIGNALED_BIT ? TRUE : FALSE;

   /* I suspect that this approach is bunk, and we should instead create the actual
    * fence object on the cmdqueue and just signal things on the dzn_fence object
    * instead. But I don't know yet, so let's just leave it like this for now...
    */
   if (FAILED(device->dev->CreateFence(initial_state ? 1 : 0, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&fence))))
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   event = CreateEventA(NULL, TRUE, initial_state, NULL);
   fence->SetEventOnCompletion(1, event);

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_FENCE);
}

dzn_fence::~dzn_fence()
{
   vk_object_base_finish(&base);
   CloseHandle(event);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateFence(VkDevice device,
                const VkFenceCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkFence *pFence)
{
   return dzn_fence_factory::create(device, pCreateInfo, pAllocator, pFence);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyFence(VkDevice device,
                 VkFence fence,
                 const VkAllocationCallbacks *pAllocator)
{
   return dzn_fence_factory::destroy(device, fence, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetFenceStatus(VkDevice _device,
                   VkFence _fence)
{
   VK_FROM_HANDLE(dzn_fence, fence, _fence);

   if (fence->fence->GetCompletedValue() != 1)
      return VK_NOT_READY;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_ResetFences(VkDevice _device,
                uint32_t fenceCount,
                const VkFence *pFences)
{
   VK_FROM_HANDLE(dzn_device, device, _device);

   for (uint32_t i = 0; i < fenceCount; i++) {
      VK_FROM_HANDLE(dzn_fence, fence, pFences[i]);
      fence->fence->Signal(0);
      ResetEvent(fence->event);
      fence->fence->SetEventOnCompletion(1, fence->event);
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
      VK_FROM_HANDLE(dzn_fence, fence, pFences[i]);
      handles[i] = fence->event;
   }
   HRESULT hr = WaitForMultipleObjects(fenceCount, handles, waitAll, timeout / 1000000);

   if (hr == WAIT_TIMEOUT)
      return VK_TIMEOUT;      

   return VK_SUCCESS;
}
