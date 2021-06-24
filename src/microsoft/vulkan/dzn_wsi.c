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
#include "vk_util.h"

static PFN_vkVoidFunction VKAPI_PTR
dzn_wsi_proc_addr(VkPhysicalDevice physicalDevice, const char *pName)
{
   DZN_FROM_HANDLE(dzn_physical_device, pdevice, physicalDevice);
   return vk_instance_get_proc_addr_unchecked(&pdevice->instance->vk, pName);
}

VkResult
dzn_wsi_init(struct dzn_physical_device *physical_device)
{
   VkResult result;

   /* TODO: implement a proper, non-sw winsys for D3D12 */
   bool sw_device = true;

   result = wsi_device_init(&physical_device->wsi_device,
                            dzn_physical_device_to_handle(physical_device),
                            dzn_wsi_proc_addr,
                            &physical_device->vk.instance->alloc,
                            -1, NULL, sw_device);

   if (result != VK_SUCCESS)
      return result;

   physical_device->wsi_device.supports_modifiers = false;

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroySurfaceKHR(VkInstance _instance,
                      VkSurfaceKHR _surface,
                      const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_instance, instance, _instance);
   ICD_FROM_HANDLE(VkIcdSurfaceBase, surface, _surface);

   if (!surface)
      return;

   vk_free2(&instance->vk.alloc, pAllocator, surface);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                       uint32_t queueFamilyIndex,
                                       VkSurfaceKHR surface,
                                       VkBool32 *pSupported)
{
   DZN_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   return wsi_common_get_surface_support(&device->wsi_device,
                                         queueFamilyIndex,
                                         surface,
                                         pSupported);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice,
                                            VkSurfaceKHR surface,
                                            VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
   DZN_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   return wsi_common_get_surface_capabilities(&device->wsi_device,
                                              surface,
                                              pSurfaceCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice physicalDevice,
                                             const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                             VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
   DZN_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   return wsi_common_get_surface_capabilities2(&device->wsi_device,
                                               pSurfaceInfo,
                                               pSurfaceCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                       VkSurfaceKHR surface,
                                       uint32_t *pSurfaceFormatCount,
                                       VkSurfaceFormatKHR *pSurfaceFormats)
{
   DZN_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   return wsi_common_get_surface_formats(&device->wsi_device, surface,
                                         pSurfaceFormatCount, pSurfaceFormats);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physicalDevice,
                                        const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                        uint32_t *pSurfaceFormatCount,
                                        VkSurfaceFormat2KHR *pSurfaceFormats)
{
   DZN_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   return wsi_common_get_surface_formats2(&device->wsi_device, pSurfaceInfo,
                                          pSurfaceFormatCount, pSurfaceFormats);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice,
                                            VkSurfaceKHR surface,
                                            uint32_t *pPresentModeCount,
                                            VkPresentModeKHR *pPresentModes)
{
   DZN_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   return wsi_common_get_surface_present_modes(&device->wsi_device, surface,
                                               pPresentModeCount,
                                               pPresentModes);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateSwapchainKHR(VkDevice _device,
                       const VkSwapchainCreateInfoKHR *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator,
                       VkSwapchainKHR *pSwapchain)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   struct wsi_device *wsi_device = &device->physical_device->wsi_device;
   const VkAllocationCallbacks *alloc;

   if (pAllocator)
     alloc = pAllocator;
   else
     alloc = &device->vk.alloc;

   return wsi_common_create_swapchain(wsi_device, _device,
                                      pCreateInfo, alloc, pSwapchain);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroySwapchainKHR(VkDevice _device,
                        VkSwapchainKHR swapchain,
                        const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   const VkAllocationCallbacks *alloc;

   if (pAllocator)
     alloc = pAllocator;
   else
     alloc = &device->vk.alloc;

   wsi_common_destroy_swapchain(_device, swapchain, alloc);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetSwapchainImagesKHR(VkDevice device,
                          VkSwapchainKHR swapchain,
                          uint32_t *pSwapchainImageCount,
                          VkImage *pSwapchainImages)
{
   return wsi_common_get_images(swapchain,
                                pSwapchainImageCount,
                                pSwapchainImages);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_AcquireNextImageKHR(VkDevice device,
                        VkSwapchainKHR swapchain,
                        uint64_t timeout,
                        VkSemaphore semaphore,
                        VkFence fence,
                        uint32_t *pImageIndex)
{
   VkAcquireNextImageInfoKHR acquire_info = {
      .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
      .swapchain = swapchain,
      .timeout = timeout,
      .semaphore = semaphore,
      .fence = fence,
      .deviceMask = 0,
   };

   return dzn_AcquireNextImage2KHR(device, &acquire_info, pImageIndex);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_AcquireNextImage2KHR(VkDevice _device,
                         const VkAcquireNextImageInfoKHR *pAcquireInfo,
                         uint32_t *pImageIndex)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
#if 0
   DZN_FROM_HANDLE(dzn_fence, fence, pAcquireInfo->fence);
   DZN_FROM_HANDLE(dzn_semaphore, semaphore, pAcquireInfo->semaphore);
#endif

   struct dzn_physical_device *pdevice = device->physical_device;

   VkResult result;
   result = wsi_common_acquire_next_image2(&pdevice->wsi_device, _device,
                                           pAcquireInfo, pImageIndex);

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_QueuePresentKHR(VkQueue _queue,
                    const VkPresentInfoKHR *pPresentInfo)
{
   DZN_FROM_HANDLE(dzn_queue, queue, _queue);
   struct dzn_physical_device *pdevice = queue->device->physical_device;

   return wsi_common_queue_present(&pdevice->wsi_device,
                                   dzn_device_to_handle(queue->device),
                                   _queue, 0,
                                   pPresentInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetDeviceGroupPresentCapabilitiesKHR(VkDevice device,
                                         VkDeviceGroupPresentCapabilitiesKHR *pCapabilities)
{
   memset(pCapabilities->presentMask, 0,
          sizeof(pCapabilities->presentMask));
   pCapabilities->presentMask[0] = 0x1;
   pCapabilities->modes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetDeviceGroupSurfacePresentModesKHR(VkDevice device,
                                         VkSurfaceKHR surface,
                                         VkDeviceGroupPresentModeFlagsKHR *pModes)
{
   *pModes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice,
                                          VkSurfaceKHR surface,
                                          uint32_t *pRectCount,
                                          VkRect2D *pRects)
{
   DZN_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   return wsi_common_get_present_rectangles(&device->wsi_device,
                                            surface,
                                            pRectCount, pRects);
}
