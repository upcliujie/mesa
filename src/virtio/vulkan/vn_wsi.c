/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#include "vn_wsi.h"

#include "vn_device.h"

static PFN_vkVoidFunction
vn_wsi_proc_addr(VkPhysicalDevice physicalDevice, const char *pName)
{
   return vn_lookup_entrypoint(pName);
}

VkResult
vn_wsi_init(struct vn_physical_device *physical_dev)
{
   VkResult result = wsi_device_init(
      &physical_dev->wsi_device, vn_physical_device_to_handle(physical_dev),
      vn_wsi_proc_addr, &physical_dev->instance->allocator, -1, NULL, false);
   if (result != VK_SUCCESS)
      return result;

   if (physical_dev->supported_extensions.EXT_image_drm_format_modifier)
      physical_dev->wsi_device.supports_modifiers = true;

   return VK_SUCCESS;
}

void
vn_wsi_fini(struct vn_physical_device *physical_dev)
{
   wsi_device_finish(&physical_dev->wsi_device,
                     &physical_dev->instance->allocator);
}

/* surface commands */

void
vn_DestroySurfaceKHR(VkInstance _instance,
                     VkSurfaceKHR surface,
                     const VkAllocationCallbacks *pAllocator)
{
   struct vn_instance *instance = vn_instance_from_handle(_instance);
   ICD_FROM_HANDLE(VkIcdSurfaceBase, surf, surface);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &instance->allocator;

   vk_free(alloc, surf);
}

VkResult
vn_GetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                      uint32_t queueFamilyIndex,
                                      VkSurfaceKHR surface,
                                      VkBool32 *pSupported)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result = wsi_common_get_surface_support(
      &physical_dev->wsi_device, queueFamilyIndex, surface, pSupported);

   return vn_result(physical_dev->instance, result);
}

VkResult
vn_GetPhysicalDeviceSurfaceCapabilitiesKHR(
   VkPhysicalDevice physicalDevice,
   VkSurfaceKHR surface,
   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result = wsi_common_get_surface_capabilities(
      &physical_dev->wsi_device, surface, pSurfaceCapabilities);

   return vn_result(physical_dev->instance, result);
}

VkResult
vn_GetPhysicalDeviceSurfaceCapabilities2KHR(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
   VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result = wsi_common_get_surface_capabilities2(
      &physical_dev->wsi_device, pSurfaceInfo, pSurfaceCapabilities);

   return vn_result(physical_dev->instance, result);
}

VkResult
vn_GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                      VkSurfaceKHR surface,
                                      uint32_t *pSurfaceFormatCount,
                                      VkSurfaceFormatKHR *pSurfaceFormats)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result =
      wsi_common_get_surface_formats(&physical_dev->wsi_device, surface,
                                     pSurfaceFormatCount, pSurfaceFormats);

   return vn_result(physical_dev->instance, result);
}

VkResult
vn_GetPhysicalDeviceSurfaceFormats2KHR(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
   uint32_t *pSurfaceFormatCount,
   VkSurfaceFormat2KHR *pSurfaceFormats)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result =
      wsi_common_get_surface_formats2(&physical_dev->wsi_device, pSurfaceInfo,
                                      pSurfaceFormatCount, pSurfaceFormats);

   return vn_result(physical_dev->instance, result);
}

VkResult
vn_GetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice,
                                           VkSurfaceKHR surface,
                                           uint32_t *pPresentModeCount,
                                           VkPresentModeKHR *pPresentModes)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result = wsi_common_get_surface_present_modes(
      &physical_dev->wsi_device, surface, pPresentModeCount, pPresentModes);

   return vn_result(physical_dev->instance, result);
}

VkResult
vn_GetDeviceGroupPresentCapabilitiesKHR(
   VkDevice device, VkDeviceGroupPresentCapabilitiesKHR *pCapabilities)
{
   memset(pCapabilities->presentMask, 0, sizeof(pCapabilities->presentMask));
   pCapabilities->presentMask[0] = 0x1;
   pCapabilities->modes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;

   return VK_SUCCESS;
}

VkResult
vn_GetDeviceGroupSurfacePresentModesKHR(
   VkDevice device,
   VkSurfaceKHR surface,
   VkDeviceGroupPresentModeFlagsKHR *pModes)
{
   *pModes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;

   return VK_SUCCESS;
}

VkResult
vn_GetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice,
                                         VkSurfaceKHR surface,
                                         uint32_t *pRectCount,
                                         VkRect2D *pRects)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result = wsi_common_get_present_rectangles(
      &physical_dev->wsi_device, surface, pRectCount, pRects);

   return vn_result(physical_dev->instance, result);
}

/* swapchain commands */

VkResult
vn_CreateSwapchainKHR(VkDevice device,
                      const VkSwapchainCreateInfoKHR *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkSwapchainKHR *pSwapchain)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->allocator;

   VkResult result =
      wsi_common_create_swapchain(&dev->physical_device->wsi_device, device,
                                  pCreateInfo, alloc, pSwapchain);

   return vn_result(dev->instance, result);
}

void
vn_DestroySwapchainKHR(VkDevice device,
                       VkSwapchainKHR swapchain,
                       const VkAllocationCallbacks *pAllocator)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->allocator;

   wsi_common_destroy_swapchain(device, swapchain, alloc);
}

VkResult
vn_GetSwapchainImagesKHR(VkDevice device,
                         VkSwapchainKHR swapchain,
                         uint32_t *pSwapchainImageCount,
                         VkImage *pSwapchainImages)
{
   struct vn_device *dev = vn_device_from_handle(device);

   VkResult result = wsi_common_get_images(swapchain, pSwapchainImageCount,
                                           pSwapchainImages);

   return vn_result(dev->instance, result);
}

VkResult
vn_AcquireNextImageKHR(VkDevice device,
                       VkSwapchainKHR swapchain,
                       uint64_t timeout,
                       VkSemaphore semaphore,
                       VkFence fence,
                       uint32_t *pImageIndex)
{
   const VkAcquireNextImageInfoKHR acquire_info = {
      .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
      .swapchain = swapchain,
      .timeout = timeout,
      .semaphore = semaphore,
      .fence = fence,
      .deviceMask = 0x1,
   };

   return vn_AcquireNextImage2KHR(device, &acquire_info, pImageIndex);
}

VkResult
vn_QueuePresentKHR(VkQueue _queue, const VkPresentInfoKHR *pPresentInfo)
{
   struct vn_queue *queue = vn_queue_from_handle(_queue);

   VkResult result =
      wsi_common_queue_present(&queue->device->physical_device->wsi_device,
                               vn_device_to_handle(queue->device), _queue,
                               queue->family, pPresentInfo);

   return vn_result(queue->device->instance, result);
}

VkResult
vn_AcquireNextImage2KHR(VkDevice device,
                        const VkAcquireNextImageInfoKHR *pAcquireInfo,
                        uint32_t *pImageIndex)
{
   struct vn_device *dev = vn_device_from_handle(device);

   VkResult result = wsi_common_acquire_next_image2(
      &dev->physical_device->wsi_device, device, pAcquireInfo, pImageIndex);

   /* XXX this relies on implicit sync */
   if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
      struct vn_semaphore *sem =
         vn_semaphore_from_handle(pAcquireInfo->semaphore);
      if (sem)
         vn_semaphore_signal_wsi(dev, sem);

      struct vn_fence *fence = vn_fence_from_handle(pAcquireInfo->fence);
      if (fence)
         vn_fence_signal_wsi(dev, fence);
   }

   return vn_result(dev->instance, result);
}
