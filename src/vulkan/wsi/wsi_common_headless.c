/*
 * Copyright 2021 Red Hat, Inc.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/** VK_EXT_headless_surface */

#include "util/macros.h"
#include "util/hash_table.h"
#include "util/u_thread.h"
#include "util/xmlconfig.h"
#include "vk_util.h"
#include "vk_enum_to_str.h"
#include "wsi_common_private.h"
#include "wsi_common_queue.h"

VkResult
wsi_create_headless_surface(const VkAllocationCallbacks *pAllocator,
                            const VkHeadlessSurfaceCreateInfoEXT *pCreateInfo,
                            VkSurfaceKHR *pSurface)
{
   VkIcdSurfaceHeadless *surface;

   surface = vk_alloc(pAllocator, sizeof *surface, 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (surface == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface->base.platform = VK_ICD_WSI_PLATFORM_HEADLESS;

   *pSurface = VkIcdSurfaceBase_to_handle(&surface->base);
   return VK_SUCCESS;
}

static VkResult
headless_surface_get_support(VkIcdSurfaceBase *icd_surface,
                             struct wsi_device *wsi_device,
                             uint32_t queueFamilyIndex,
                             VkBool32 *pSupported)
{
   return VK_SUCCESS;
}

static VkResult
headless_surface_get_capabilities2(VkIcdSurfaceBase *icd_surface,
                                   struct wsi_device *wsi_device,
                                   const void *info_next,
                                   VkSurfaceCapabilities2KHR *caps)
{
   return VK_SUCCESS;
}

static VkResult
headless_surface_get_formats(VkIcdSurfaceBase *surface,
                             struct wsi_device *wsi_device,
                             uint32_t *pSurfaceFormatCount,
                             VkSurfaceFormatKHR *pSurfaceFormats)
{
   return VK_SUCCESS;
}

static VkResult
headless_surface_get_formats2(VkIcdSurfaceBase *surface,
                              struct wsi_device *wsi_device,
                              const void *info_next,
                              uint32_t *pSurfaceFormatCount,
                              VkSurfaceFormat2KHR *pSurfaceFormats)
{
   return VK_SUCCESS;
}

static VkResult
headless_surface_get_present_modes(VkIcdSurfaceBase *surface,
                                   uint32_t *pPresentModeCount,
                                   VkPresentModeKHR *pPresentModes)
{
   return VK_SUCCESS;
}

static VkResult
headless_surface_get_present_rectangles(VkIcdSurfaceBase *icd_surface,
                                        struct wsi_device *wsi_device,
                                        uint32_t* pRectCount,
                                        VkRect2D* pRects)
{
   return VK_SUCCESS;
}

static VkResult
headless_surface_create_swapchain(VkIcdSurfaceBase *icd_surface,
                                  VkDevice device,
                                  struct wsi_device *wsi_device,
                                  const VkSwapchainCreateInfoKHR *pCreateInfo,
                                  const VkAllocationCallbacks* pAllocator,
                                  struct wsi_swapchain **swapchain_out)
{
   return VK_SUCCESS;
}

VkResult
wsi_headless_init_wsi(struct wsi_device *wsi_device,
                      const VkAllocationCallbacks *alloc,
                      VkPhysicalDevice physical_device)
{
   struct wsi_interface *wsi;

   wsi = vk_alloc(alloc, sizeof(*wsi), sizeof(void *),
                  VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!wsi)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   wsi->get_support = headless_surface_get_support;
   wsi->get_capabilities2 = headless_surface_get_capabilities2;
   wsi->get_formats = headless_surface_get_formats;
   wsi->get_formats2 = headless_surface_get_formats2;
   wsi->get_present_modes = headless_surface_get_present_modes;
   wsi->get_present_rectangles = headless_surface_get_present_rectangles;
   wsi->create_swapchain = headless_surface_create_swapchain;

   wsi_device->wsi[VK_ICD_WSI_PLATFORM_HEADLESS] = wsi;
   return VK_SUCCESS;
}

void
wsi_headless_finish_wsi(struct wsi_device *wsi_device,
                        const VkAllocationCallbacks *alloc)
{
   vk_free(alloc, wsi_device->wsi[VK_ICD_WSI_PLATFORM_HEADLESS]);
}
