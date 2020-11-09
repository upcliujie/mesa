/*
 * Copyright © 2015 Intel Corporation
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
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vk_util.h"
#include "wsi_common_private.h"
#include "wsi_common_win32.h"

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"      // warning: cast to pointer from integer of different size
#endif

struct wsi_win32;

struct wsi_win32 {
   struct wsi_interface                     base;

   struct wsi_device *wsi;

   const VkAllocationCallbacks *alloc;
   VkPhysicalDevice physical_device;
};


enum wsi_image_state {
   WSI_IMAGE_IDLE,
   WSI_IMAGE_DRAWING,
   WSI_IMAGE_QUEUED,
   WSI_IMAGE_FLIPPING,
   WSI_IMAGE_DISPLAYING
};

struct wsi_win32_image {
   struct wsi_image             base;
   struct wsi_win32_swapchain *chain;
   enum wsi_image_state         state;
   uint32_t                     fb_id;
   uint32_t                     buffer[4];
   uint64_t                     flip_sequence;
   HDC dc;
   HBITMAP bmp;
   void *ppvBits;
};


struct wsi_win32_swapchain {
   struct wsi_swapchain         base;
   struct wsi_win32           *wsi;
   VkIcdSurfaceWin32          *surface;
   uint64_t                     flip_sequence;
   VkResult                     status;
   VkExtent2D                 extent;
   HWND wnd;
   HDC chain_dc;
   struct wsi_win32_image     images[0];
};

VkBool32
wsi_win32_get_presentation_support(struct wsi_device *wsi_device)
{
//    struct wsi_win32 *wsi =
//       (struct wsi_win32 *)wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32];

   VkResult ret = VK_SUCCESS;
//    ret = wsi_win32_win32_init(wsi, &display, wl_display, false);
//    if (ret == VK_SUCCESS)
//       wsi_win32_win32_finish(&display);

   return ret == VK_SUCCESS;
}

VkResult
wsi_create_win32_surface(VkInstance instance,
                           const VkAllocationCallbacks *allocator,
                           const VkWin32SurfaceCreateInfoKHR *create_info,
                           VkSurfaceKHR *surface_khr)
{
   VkIcdSurfaceWin32 *surface = vk_zalloc(allocator, sizeof *surface, 8,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (surface == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface->base.platform = VK_ICD_WSI_PLATFORM_WIN32;

   surface->hinstance = create_info->hinstance;
   surface->hwnd = create_info->hwnd;

   *surface_khr = VkIcdSurfaceBase_to_handle(&surface->base);
   return VK_SUCCESS;
}

static VkResult
wsi_win32_surface_get_support(VkIcdSurfaceBase *surface,
                           struct wsi_device *wsi_device,
                           uint32_t queueFamilyIndex,
                           VkBool32* pSupported)
{
   *pSupported = true;

   return VK_SUCCESS;
}

// static const VkPresentModeKHR present_modes[] = {
//    VK_PRESENT_MODE_MAILBOX_KHR,
//    VK_PRESENT_MODE_FIFO_KHR,
// };

static VkResult
wsi_win32_surface_get_capabilities(VkIcdSurfaceBase *surface,
                                struct wsi_device *wsi_device,
                                VkSurfaceCapabilitiesKHR* caps)
{
   /* For true mailbox mode, we need at least 4 images:
    *  1) One to scan out from
    *  2) One to have queued for scan-out
    *  3) One to be currently held by the Wayland compositor
    *  4) One to render to
    */
   caps->minImageCount = 4;
   /* There is no real maximum */
   caps->maxImageCount = 0;

   caps->currentExtent = (VkExtent2D) { UINT32_MAX, UINT32_MAX };
   caps->minImageExtent = (VkExtent2D) { 1, 1 };
   caps->maxImageExtent = (VkExtent2D) {
      wsi_device->maxImageDimension2D,
      wsi_device->maxImageDimension2D,
   };

   caps->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->maxImageArrayLayers = 1;

   caps->supportedCompositeAlpha =
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR |
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;

   caps->supportedUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_STORAGE_BIT |
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   return VK_SUCCESS;
}

static VkResult
wsi_win32_surface_get_capabilities2(VkIcdSurfaceBase *surface,
                                 struct wsi_device *wsi_device,
                                 const void *info_next,
                                 VkSurfaceCapabilities2KHR* caps)
{
   assert(caps->sType == VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR);

   VkResult result =
      wsi_win32_surface_get_capabilities(surface, wsi_device,
                                      &caps->surfaceCapabilities);

   vk_foreach_struct(ext, caps->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR: {
         VkSurfaceProtectedCapabilitiesKHR *protected = (void *)ext;
         protected->supportsProtected = VK_FALSE;
         break;
      }

      default:
         /* Ignored */
         break;
      }
   }

   return result;
}


static const struct {
   VkFormat     format;
} available_surface_formats[] = {
   { .format = VK_FORMAT_B8G8R8A8_SRGB },
   { .format = VK_FORMAT_B8G8R8A8_UNORM },
};


static void
get_sorted_vk_formats(struct wsi_device *wsi_device, VkFormat *sorted_formats)
{
   for (unsigned i = 0; i < ARRAY_SIZE(available_surface_formats); i++)
      sorted_formats[i] = available_surface_formats[i].format;

   if (wsi_device->force_bgra8_unorm_first) {
      for (unsigned i = 0; i < ARRAY_SIZE(available_surface_formats); i++) {
         if (sorted_formats[i] == VK_FORMAT_B8G8R8A8_UNORM) {
            sorted_formats[i] = sorted_formats[0];
            sorted_formats[0] = VK_FORMAT_B8G8R8A8_UNORM;
            break;
         }
      }
   }
}

static VkResult
wsi_win32_surface_get_formats(VkIcdSurfaceBase *icd_surface,
			   struct wsi_device *wsi_device,
                           uint32_t* pSurfaceFormatCount,
                           VkSurfaceFormatKHR* pSurfaceFormats)
{
   VK_OUTARRAY_MAKE(out, pSurfaceFormats, pSurfaceFormatCount);

   VkFormat sorted_formats[ARRAY_SIZE(available_surface_formats)];
   get_sorted_vk_formats(wsi_device, sorted_formats);

   for (unsigned i = 0; i < ARRAY_SIZE(sorted_formats); i++) {
      vk_outarray_append(&out, f) {
         f->format = sorted_formats[i];
         f->colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
      }
   }

   return vk_outarray_status(&out);
//    VkIcdSurfaceWin32 *surface = (VkIcdSurfaceWin32 *)icd_surface;
//    struct wsi_win32 *wsi =
//       (struct wsi_win32 *)wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32];
// 
//    struct wsi_win32_display display;
//    if (wsi_win32_win32_init(wsi, &display, surface->display, true))
//       return VK_ERROR_SURFACE_LOST_KHR;
// 
//    VK_OUTARRAY_MAKE(out, pSurfaceFormats, pSurfaceFormatCount);
// 
//    VkFormat *disp_fmt;
//    u_vector_foreach(disp_fmt, display.formats) {
//       vk_outarray_append(&out, out_fmt) {
//          out_fmt->format = *disp_fmt;
//          out_fmt->colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
//       }
//    }
// 
//    wsi_win32_win32_finish(&display);
// 
//    return vk_outarray_status(&out);
}

static VkResult
wsi_win32_surface_get_formats2(VkIcdSurfaceBase *icd_surface,
			    struct wsi_device *wsi_device,
                            const void *info_next,
                            uint32_t* pSurfaceFormatCount,
                            VkSurfaceFormat2KHR* pSurfaceFormats)
{
      VK_OUTARRAY_MAKE(out, pSurfaceFormats, pSurfaceFormatCount);

   VkFormat sorted_formats[ARRAY_SIZE(available_surface_formats)];
   get_sorted_vk_formats(wsi_device, sorted_formats);

   for (unsigned i = 0; i < ARRAY_SIZE(sorted_formats); i++) {
      vk_outarray_append(&out, f) {
         assert(f->sType == VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR);
         f->surfaceFormat.format = sorted_formats[i];
         f->surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
      }
   }

   return vk_outarray_status(&out);

//    VkIcdSurfaceWin32 *surface = (VkIcdSurfaceWin32 *)icd_surface;
//    struct wsi_win32 *wsi =
//       (struct wsi_win32 *)wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32];
// 
//    struct wsi_win32_display display;
//    if (wsi_win32_win32_init(wsi, &display, surface->display, true))
//       return VK_ERROR_SURFACE_LOST_KHR;
// 
//    VK_OUTARRAY_MAKE(out, pSurfaceFormats, pSurfaceFormatCount);
// 
//    VkFormat *disp_fmt;
//    u_vector_foreach(disp_fmt, display.formats) {
//       vk_outarray_append(&out, out_fmt) {
//          out_fmt->surfaceFormat.format = *disp_fmt;
//          out_fmt->surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
//       }
//    }
// 
//    wsi_win32_win32_finish(&display);
// 
//    return vk_outarray_status(&out);
//    return VK_SUCCESS;
}

static const VkPresentModeKHR present_modes[] = {
   //VK_PRESENT_MODE_MAILBOX_KHR,
   VK_PRESENT_MODE_FIFO_KHR,
};

static VkResult
wsi_win32_surface_get_present_modes(VkIcdSurfaceBase *surface,
                                 uint32_t* pPresentModeCount,
                                 VkPresentModeKHR* pPresentModes)
{
   if (pPresentModes == NULL) {
      *pPresentModeCount = ARRAY_SIZE(present_modes);
      return VK_SUCCESS;
   }

   *pPresentModeCount = MIN2(*pPresentModeCount, ARRAY_SIZE(present_modes));
   typed_memcpy(pPresentModes, present_modes, *pPresentModeCount);

   if (*pPresentModeCount < ARRAY_SIZE(present_modes))
      return VK_INCOMPLETE;
   else
      return VK_SUCCESS;
}

static VkResult
wsi_win32_surface_get_present_rectangles(VkIcdSurfaceBase *surface,
                                      struct wsi_device *wsi_device,
                                      uint32_t* pRectCount,
                                      VkRect2D* pRects)
{
   VK_OUTARRAY_MAKE(out, pRects, pRectCount);

   vk_outarray_append(&out, rect) {
      /* We don't know a size so just return the usual "I don't know." */
      *rect = (VkRect2D) {
         .offset = { 0, 0 },
         .extent = { UINT32_MAX, UINT32_MAX },
      };
   }

   return vk_outarray_status(&out);
}

// static void
// wsi_win32_destroy_buffer(struct wsi_win32 *wsi,
//                            uint32_t buffer)
// {
//    (void) drmIoctl(wsi->fd, DRM_IOCTL_GEM_CLOSE,
//                    &((struct drm_gem_close) { .handle = buffer }));
// }


static uint32_t
select_memory_type(const struct wsi_device *wsi,
                   VkMemoryPropertyFlags props,
                   uint32_t type_bits)
{
   for (uint32_t i = 0; i < wsi->memory_props.memoryTypeCount; i++) {
       const VkMemoryType type = wsi->memory_props.memoryTypes[i];
       if ((type_bits & (1 << i)) && (type.propertyFlags & props) == props)
         return i;
   }

   unreachable("No memory type found");
}


VkResult
wsi_create_native_image(const struct wsi_swapchain *chain,
                        const VkSwapchainCreateInfoKHR *pCreateInfo,
                        uint32_t num_modifier_lists,
                        const uint32_t *num_modifiers,
                        const uint64_t *const *modifiers,
                        struct wsi_image *image)
{
   const struct wsi_device *wsi = chain->wsi;
   VkResult result;

   memset(image, 0, sizeof(*image));
   for (int i = 0; i < ARRAY_SIZE(image->fds); i++)
      image->fds[i] = -1;

   VkImageCreateInfo image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = pCreateInfo->imageFormat,
      .extent = {
         .width = pCreateInfo->imageExtent.width,
         .height = pCreateInfo->imageExtent.height,
         .depth = 1,
      },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = pCreateInfo->imageUsage,
      .sharingMode = pCreateInfo->imageSharingMode,
      .queueFamilyIndexCount = pCreateInfo->queueFamilyIndexCount,
      .pQueueFamilyIndices = pCreateInfo->pQueueFamilyIndices,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
   };

   VkImageFormatListCreateInfoKHR image_format_list;
   if (pCreateInfo->flags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR) {
      image_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT |
                          VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR;

      const VkImageFormatListCreateInfoKHR *format_list =
         vk_find_struct_const(pCreateInfo->pNext,
                              IMAGE_FORMAT_LIST_CREATE_INFO_KHR);

#ifndef NDEBUG
      assume(format_list && format_list->viewFormatCount > 0);
      bool format_found = false;
      for (int i = 0; i < format_list->viewFormatCount; i++)
         if (pCreateInfo->imageFormat == format_list->pViewFormats[i])
            format_found = true;
      assert(format_found);
#endif

      image_format_list = *format_list;
      image_format_list.pNext = NULL;
      __vk_append_struct(&image_info, &image_format_list);
   }

   struct wsi_image_create_info image_wsi_info;
   VkImageDrmFormatModifierListCreateInfoEXT image_modifier_list;

   uint32_t image_modifier_count = 0, modifier_prop_count = 0;
   struct VkDrmFormatModifierPropertiesEXT *modifier_props = NULL;
   uint64_t *image_modifiers = NULL;
   if (num_modifier_lists == 0) {
      /* If we don't have modifiers, fall back to the legacy "scanout" flag */
      image_wsi_info = (struct wsi_image_create_info) {
         .sType = VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA,
         .scanout = true,
      };
      __vk_append_struct(&image_info, &image_wsi_info);
   } else {
      /* The winsys can't request modifiers if we don't support them. */
      assert(wsi->supports_modifiers);
      struct VkDrmFormatModifierPropertiesListEXT modifier_props_list = {
         .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
      };
      VkFormatProperties2 format_props = {
         .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
         .pNext = &modifier_props_list,
      };
      wsi->GetPhysicalDeviceFormatProperties2KHR(wsi->pdevice,
                                                 pCreateInfo->imageFormat,
                                                 &format_props);
      assert(modifier_props_list.drmFormatModifierCount > 0);
      modifier_props = vk_alloc(&chain->alloc,
                                sizeof(*modifier_props) *
                                modifier_props_list.drmFormatModifierCount,
                                8,
                                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      if (!modifier_props) {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         goto fail;
      }

      modifier_props_list.pDrmFormatModifierProperties = modifier_props;
      wsi->GetPhysicalDeviceFormatProperties2KHR(wsi->pdevice,
                                                 pCreateInfo->imageFormat,
                                                 &format_props);

      /* Call GetImageFormatProperties with every modifier and filter the list
       * down to those that we know work.
       */
      modifier_prop_count = 0;
      for (uint32_t i = 0; i < modifier_props_list.drmFormatModifierCount; i++) {
         VkPhysicalDeviceImageDrmFormatModifierInfoEXT mod_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
            .drmFormatModifier = modifier_props[i].drmFormatModifier,
            .sharingMode = pCreateInfo->imageSharingMode,
            .queueFamilyIndexCount = pCreateInfo->queueFamilyIndexCount,
            .pQueueFamilyIndices = pCreateInfo->pQueueFamilyIndices,
         };
         VkPhysicalDeviceImageFormatInfo2 format_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
            .format = pCreateInfo->imageFormat,
            .type = VK_IMAGE_TYPE_2D,
            .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
            .usage = pCreateInfo->imageUsage,
            .flags = image_info.flags,
         };

         VkImageFormatListCreateInfoKHR format_list;
         if (image_info.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) {
            format_list = image_format_list;
            format_list.pNext = NULL;
            __vk_append_struct(&format_info, &format_list);
         }

         VkImageFormatProperties2 format_props = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
            .pNext = NULL,
         };
         __vk_append_struct(&format_info, &mod_info);
         result = wsi->GetPhysicalDeviceImageFormatProperties2(wsi->pdevice,
                                                               &format_info,
                                                               &format_props);
         if (result == VK_SUCCESS)
            modifier_props[modifier_prop_count++] = modifier_props[i];
      }

      uint32_t max_modifier_count = 0;
      for (uint32_t l = 0; l < num_modifier_lists; l++)
         max_modifier_count = MAX2(max_modifier_count, num_modifiers[l]);

      image_modifiers = vk_alloc(&chain->alloc,
                                 sizeof(*image_modifiers) *
                                 max_modifier_count,
                                 8,
                                 VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      if (!image_modifiers) {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         goto fail;
      }

      image_modifier_count = 0;
      for (uint32_t l = 0; l < num_modifier_lists; l++) {
         /* Walk the modifier lists and construct a list of supported
          * modifiers.
          */
         for (uint32_t i = 0; i < num_modifiers[l]; i++) {
            for (uint32_t j = 0; j < modifier_prop_count; j++) {
               if (modifier_props[j].drmFormatModifier == modifiers[l][i])
                  image_modifiers[image_modifier_count++] = modifiers[l][i];
            }
         }

         /* We only want to take the modifiers from the first list */
         if (image_modifier_count > 0)
            break;
      }

      if (image_modifier_count > 0) {
         image_modifier_list = (VkImageDrmFormatModifierListCreateInfoEXT) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
            .drmFormatModifierCount = image_modifier_count,
            .pDrmFormatModifiers = image_modifiers,
         };
         image_info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
         __vk_append_struct(&image_info, &image_modifier_list);
      } else {
         /* TODO: Add a proper error here */
         assert(!"Failed to find a supported modifier!  This should never "
                 "happen because LINEAR should always be available");
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         goto fail;
      }
   }

   result = wsi->CreateImage(chain->device, &image_info,
                             &chain->alloc, &image->image);
   if (result != VK_SUCCESS)
      goto fail;

   VkMemoryRequirements reqs;
   wsi->GetImageMemoryRequirements(chain->device, image->image, &reqs);

   const struct wsi_memory_allocate_info memory_wsi_info = {
      .sType = VK_STRUCTURE_TYPE_WSI_MEMORY_ALLOCATE_INFO_MESA,
      .pNext = NULL,
      .implicit_sync = true,
   };
   const VkExportMemoryAllocateInfo memory_export_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      .pNext = &memory_wsi_info,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
   };
   const VkMemoryDedicatedAllocateInfo memory_dedicated_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .pNext = &memory_export_info,
      .image = image->image,
      .buffer = VK_NULL_HANDLE,
   };
   const VkMemoryAllocateInfo memory_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memory_dedicated_info,
      .allocationSize = reqs.size,
      .memoryTypeIndex = select_memory_type(wsi, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            reqs.memoryTypeBits),
   };
   result = wsi->AllocateMemory(chain->device, &memory_info,
                                &chain->alloc, &image->memory);
   if (result != VK_SUCCESS)
      goto fail;

   result = wsi->BindImageMemory(chain->device, image->image,
                                 image->memory, 0);
   if (result != VK_SUCCESS)
      goto fail;

   int fd = -1;
   if (!wsi->sw) {
      const VkMemoryGetFdInfoKHR memory_get_fd_info = {
         .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
         .pNext = NULL,
         .memory = image->memory,
         .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      };

      result = wsi->GetMemoryFdKHR(chain->device, &memory_get_fd_info, &fd);
      if (result != VK_SUCCESS)
         goto fail;
   }

//    if (!wsi->sw && num_modifier_lists > 0) {
//       VkImageDrmFormatModifierPropertiesEXT image_mod_props = {
//          .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT,
//       };
//       result = wsi->GetImageDrmFormatModifierPropertiesEXT(chain->device,
//                                                            image->image,
//                                                            &image_mod_props);
//       if (result != VK_SUCCESS) {
//          close(fd);
//          goto fail;
//       }
//       image->drm_modifier = image_mod_props.drmFormatModifier;
//       assert(image->drm_modifier != DRM_FORMAT_MOD_INVALID);
// 
//       for (uint32_t j = 0; j < modifier_prop_count; j++) {
//          if (modifier_props[j].drmFormatModifier == image->drm_modifier) {
//             image->num_planes = modifier_props[j].drmFormatModifierPlaneCount;
//             break;
//          }
//       }
// 
//       for (uint32_t p = 0; p < image->num_planes; p++) {
//          const VkImageSubresource image_subresource = {
//             .aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT << p,
//             .mipLevel = 0,
//             .arrayLayer = 0,
//          };
//          VkSubresourceLayout image_layout;
//          wsi->GetImageSubresourceLayout(chain->device, image->image,
//                                         &image_subresource, &image_layout);
//          image->sizes[p] = image_layout.size;
//          image->row_pitches[p] = image_layout.rowPitch;
//          image->offsets[p] = image_layout.offset;
//          if (p == 0) {
//             image->fds[p] = fd;
//          } else {
//             image->fds[p] = os_dupfd_cloexec(fd);
//             if (image->fds[p] == -1) {
//                for (uint32_t i = 0; i < p; i++)
//                   close(image->fds[i]);
// 
//                result = VK_ERROR_OUT_OF_HOST_MEMORY;
//                goto fail;
//             }
//          }
//       }
//    } else {
{
      const VkImageSubresource image_subresource = {
         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
         .mipLevel = 0,
         .arrayLayer = 0,
      };
      VkSubresourceLayout image_layout;
      wsi->GetImageSubresourceLayout(chain->device, image->image,
                                     &image_subresource, &image_layout);

      //image->drm_modifier = DRM_FORMAT_MOD_INVALID;
      image->num_planes = 1;
      image->sizes[0] = reqs.size;
      image->row_pitches[0] = image_layout.rowPitch;
      image->offsets[0] = 0;
      image->fds[0] = fd;
   }

   vk_free(&chain->alloc, modifier_props);
   vk_free(&chain->alloc, image_modifiers);

   return VK_SUCCESS;

fail:
   vk_free(&chain->alloc, modifier_props);
   vk_free(&chain->alloc, image_modifiers);
   wsi_destroy_image(chain, image);

   return result;
}



static VkResult
wsi_win32_image_init(VkDevice device_h,
                       struct wsi_swapchain *drv_chain,
                       const VkSwapchainCreateInfoKHR *create_info,
                       const VkAllocationCallbacks *allocator,
                       struct wsi_win32_image *image)
{
   struct wsi_win32_swapchain *chain = (struct wsi_win32_swapchain *) drv_chain;
//    struct wsi_win32 *wsi = chain->wsi;
//    uint32_t drm_format = 0;
// 
//    for (unsigned i = 0; i < ARRAY_SIZE(available_surface_formats); i++) {
//       if (create_info->imageFormat == available_surface_formats[i].format) {
//          drm_format = available_surface_formats[i].drm_format;
//          break;
//       }
//    }

   /* the application provided an invalid format, bail */
//    if (drm_format == 0)
//       return VK_ERROR_DEVICE_LOST;

   VkResult result = wsi_create_native_image(&chain->base, create_info,
                                             0, NULL, NULL,
                                             &image->base);
   if (result != VK_SUCCESS)
      return result;

//    memset(image->buffer, 0, sizeof (image->buffer));
//    VkIcdSurfaceBase *base = (VkIcdSurfaceBase *)create_info->surface;
   VkIcdSurfaceWin32 *win32_surface = (VkIcdSurfaceWin32 *)create_info->surface;
    chain->wnd = win32_surface->hwnd;
    chain->chain_dc = GetDC(chain->wnd);
//     chain->dc = GetDC(ret->wnd);
   
   for (unsigned int i = 0; i < image->base.num_planes; i++) {
      HDC dc = CreateCompatibleDC(chain->chain_dc);
      HBITMAP bmp = NULL;

      BITMAPINFO info = {};
      info.bmiHeader.biSize = sizeof(info.bmiHeader);
      info.bmiHeader.biWidth = create_info->imageExtent.width;
      info.bmiHeader.biHeight = create_info->imageExtent.height;
//       create_info->imageExtent.width,
//                            create_info->imageExtent.height,
      info.bmiHeader.biPlanes = 1;
      info.bmiHeader.biBitCount = 32;
      info.bmiHeader.biCompression = BI_RGB;
      info.bmiHeader.biSizeImage = 0;
      info.bmiHeader.biXPelsPerMeter = info.bmiHeader.biYPelsPerMeter = 96;
      info.bmiHeader.biClrUsed = 0;
      info.bmiHeader.biClrImportant = 0;

      bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &image->ppvBits, NULL, 0);
      assert(bmp && image->ppvBits);

      SelectObject(dc, bmp);

      image->dc = dc;
      image->bmp = bmp;
      
//       int ret = drmPrimeFDToHandle(wsi->fd, image->base.fds[i],
//                                    &image->buffer[i]);

//       close(image->base.fds[i]);
//       image->base.fds[i] = -1;
//       if (ret < 0)
//          goto fail_handle;
   }

   image->chain = chain;
   image->state = WSI_IMAGE_IDLE;
//    image->fb_id = 0;

//    int ret = drmModeAddFB2(wsi->fd,
//                            create_info->imageExtent.width,
//                            create_info->imageExtent.height,
//                            drm_format,
//                            image->buffer,
//                            image->base.row_pitches,
//                            image->base.offsets,
//                            &image->fb_id, 0);
// 
//    if (ret)
//       goto fail_fb;

   return VK_SUCCESS;

// fail_fb:
// fail_handle:
//    for (unsigned int i = 0; i < image->base.num_planes; i++) {
//       if (image->buffer[i])
//          wsi_win32_destroy_buffer(wsi, image->buffer[i]);
// //       if (image->base.fds[i] != -1) {
// //          close(image->base.fds[i]);
// //          image->base.fds[i] = -1;
// //       }
//    }
// 
//    wsi_destroy_image(&chain->base, &image->base);

//    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

static void
wsi_win32_image_finish(struct wsi_swapchain *drv_chain,
                         const VkAllocationCallbacks *allocator,
                         struct wsi_win32_image *image)
{
   struct wsi_win32_swapchain *chain =
      (struct wsi_win32_swapchain *) drv_chain;
//    struct wsi_win32 *wsi = chain->wsi;

   if(image->dc)
      DeleteDC(image->dc);

   if(image->bmp)
      DeleteObject(image->bmp);
//    drmModeRmFB(wsi->fd, image->fb_id);
//    for (unsigned int i = 0; i < image->base.num_planes; i++)
//       wsi_win32_destroy_buffer(wsi, image->buffer[i]);
   wsi_destroy_image(&chain->base, &image->base);
}

static VkResult
wsi_win32_swapchain_destroy(struct wsi_swapchain *drv_chain,
                              const VkAllocationCallbacks *allocator)
{
   struct wsi_win32_swapchain *chain =
      (struct wsi_win32_swapchain *) drv_chain;

   for (uint32_t i = 0; i < chain->base.image_count; i++)
      wsi_win32_image_finish(drv_chain, allocator, &chain->images[i]);

   wsi_swapchain_finish(&chain->base);
   vk_free(allocator, chain);
   return VK_SUCCESS;
}


static struct wsi_image *
wsi_win32_get_wsi_image(struct wsi_swapchain *drv_chain,
                          uint32_t image_index)
{
   struct wsi_win32_swapchain *chain =
      (struct wsi_win32_swapchain *) drv_chain;

   return &chain->images[image_index].base;
}


static VkResult
wsi_win32_acquire_next_image(struct wsi_swapchain *drv_chain,
                               const VkAcquireNextImageInfoKHR *info,
                               uint32_t *image_index)
{
   struct wsi_win32_swapchain *chain =
      (struct wsi_win32_swapchain *)drv_chain;
//    struct wsi_win32 *wsi = chain->wsi;
   int ret = 0;
   VkResult result = VK_SUCCESS;

   /* Bail early if the swapchain is broken */
   if (chain->status != VK_SUCCESS)
      return chain->status;

   // FIXME
   *image_index = 0;
   return VK_SUCCESS;


//    uint64_t timeout = info->timeout;
//    if (timeout != 0 && timeout != UINT64_MAX)
//       timeout = wsi_rel_to_abs_time(timeout);

//    pthread_mutex_lock(&wsi->wait_mutex);
   for (;;) {
      for (uint32_t i = 0; i < chain->base.image_count; i++) {
         if (chain->images[i].state == WSI_IMAGE_IDLE) {
            *image_index = i;
//             wsi_display_debug("image %d available\n", i);
            chain->images[i].state = WSI_IMAGE_DRAWING;
            result = VK_SUCCESS;
            goto done;
         }
//          wsi_display_debug("image %d state %d\n", i, chain->images[i].state);
      }

      if (ret == ETIMEDOUT) {
         result = VK_TIMEOUT;
         goto done;
      }
      // FIXME
      Sleep(10);
//       ret = wsi_display_wait_for_event(wsi, timeout);

      if (ret && ret != ETIMEDOUT) {
         result = VK_ERROR_SURFACE_LOST_KHR;
         goto done;
      }
   }
done:
//    pthread_mutex_unlock(&wsi->wait_mutex);

   if (result != VK_SUCCESS)
      return result;

   return chain->status;
}

static VkResult
wsi_win32_queue_present(struct wsi_swapchain *drv_chain,
                          uint32_t image_index,
                          const VkPresentRegionKHR *damage)
{
   struct wsi_win32_swapchain *chain = (struct wsi_win32_swapchain *) drv_chain;
//    struct wsi_win32 *wsi = chain->wsi;
   assert(image_index < chain->base.image_count);
   struct wsi_win32_image *image = &chain->images[image_index];
   VkResult result;
   

   /* Bail early if the swapchain is broken */
//    if (chain->status != VK_SUCCESS)
//       return chain->status;

//    assert(image->state == WSI_IMAGE_DRAWING);
//    wsi_win32_debug("present %d\n", image_index);

//    pthread_mutex_lock(&wsi->wait_mutex);

//    image->flip_sequence = ++chain->flip_sequence;
   image->state = WSI_IMAGE_QUEUED;

   result = chain->base.wsi->MapMemory(chain->base.device,
                                       image->base.memory,
                                       0, 0, 0, &image->ppvBits);

   if(BitBlt(chain->chain_dc, 0, 0, chain->extent.width, chain->extent.height, chain->images[image_index].dc, 0, 0, SRCCOPY))
      result = VK_SUCCESS;
   else
     result = VK_ERROR_MEMORY_MAP_FAILED;

   if (result != VK_SUCCESS)
      chain->status = result;

//    pthread_mutex_unlock(&wsi->wait_mutex);

   if (result != VK_SUCCESS)
      return result;

   return chain->status;
}

static VkResult
wsi_win32_surface_create_swapchain(
   VkIcdSurfaceBase *icd_surface,
   VkDevice device,
   struct wsi_device *wsi_device,
   const VkSwapchainCreateInfoKHR *create_info,
   const VkAllocationCallbacks *allocator,
   struct wsi_swapchain **swapchain_out)
{
   struct wsi_win32 *wsi =
      (struct wsi_win32 *) wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32];

   assert(create_info->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);

   const unsigned num_images = create_info->minImageCount;
   struct wsi_win32_swapchain *chain =
      vk_zalloc(allocator,
                sizeof(*chain) + num_images * sizeof(chain->images[0]),
                8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (chain == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult result = wsi_swapchain_init(wsi_device, &chain->base, device,
                                        create_info, allocator);
   if (result != VK_SUCCESS) {
      vk_free(allocator, chain);
      return result;
   }

   chain->base.destroy = wsi_win32_swapchain_destroy;
   chain->base.get_wsi_image = wsi_win32_get_wsi_image;
   chain->base.acquire_next_image = wsi_win32_acquire_next_image;
   chain->base.queue_present = wsi_win32_queue_present;
   chain->base.present_mode = wsi_swapchain_get_present_mode(wsi_device, create_info);
   chain->base.image_count = num_images;
   chain->extent = create_info->imageExtent;

   chain->wsi = wsi;
   chain->status = VK_SUCCESS;

   chain->surface = (VkIcdSurfaceWin32 *) icd_surface;

   for (uint32_t image = 0; image < chain->base.image_count; image++) {
      result = wsi_win32_image_init(device, &chain->base,
                                      create_info, allocator,
                                      &chain->images[image]);
      if (result != VK_SUCCESS) {
         while (image > 0) {
            --image;
            wsi_win32_image_finish(&chain->base, allocator,
                                     &chain->images[image]);
         }
         vk_free(allocator, chain);
         goto fail_init_images;
      }
   }

   *swapchain_out = &chain->base;

   return VK_SUCCESS;

fail_init_images:
   return result;
}


VkResult
wsi_win32_init_wsi(struct wsi_device *wsi_device,
                const VkAllocationCallbacks *alloc,
                VkPhysicalDevice physical_device)
{
   struct wsi_win32 *wsi;
   VkResult result;

   wsi = vk_alloc(alloc, sizeof(*wsi), 8,
                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!wsi) {
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto fail;
   }

   wsi->physical_device = physical_device;
   wsi->alloc = alloc;
   wsi->wsi = wsi_device;

   wsi->base.get_support = wsi_win32_surface_get_support;
   wsi->base.get_capabilities2 = wsi_win32_surface_get_capabilities2;
   wsi->base.get_formats = wsi_win32_surface_get_formats;
   wsi->base.get_formats2 = wsi_win32_surface_get_formats2;
   wsi->base.get_present_modes = wsi_win32_surface_get_present_modes;
   wsi->base.get_present_rectangles = wsi_win32_surface_get_present_rectangles;
   wsi->base.create_swapchain = wsi_win32_surface_create_swapchain;

   wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32] = &wsi->base;

   return VK_SUCCESS;

fail:
   wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32] = NULL;

   return result;
}

void
wsi_win32_finish_wsi(struct wsi_device *wsi_device,
                  const VkAllocationCallbacks *alloc)
{
   struct wsi_win32 *wsi =
      (struct wsi_win32 *)wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32];
   if (!wsi)
      return;

   vk_free(alloc, wsi);
}
