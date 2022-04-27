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

#include "vk_format.h"
#include "vk_instance.h"
#include "vk_physical_device.h"
#include "vk_util.h"
#include "wsi_common_entrypoints.h"
#include "wsi_common_private.h"

#define D3D12_IGNORE_SDK_LAYERS
#define COBJMACROS
#include <dxgi1_4.h>
#include <directx/d3d12.h>

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

enum wsi_win32_image_state {
   WSI_IMAGE_IDLE,
   WSI_IMAGE_DRAWING,
   WSI_IMAGE_QUEUED,
};

struct wsi_win32_image {
   struct wsi_image base;
   enum wsi_win32_image_state state;
   struct wsi_win32_swapchain *chain;
   ID3D12Resource *swapchain_res;
   HDC dc;
   HBITMAP bmp;
   int bmp_row_pitch;
   void *ppvBits;
};


struct wsi_win32_swapchain {
   struct wsi_swapchain         base;
   IDXGISwapChain1            *dxgi;
   struct wsi_win32           *wsi;
   VkIcdSurfaceWin32          *surface;
   uint64_t                     flip_sequence;
   VkResult                     status;
   VkExtent2D                 extent;
   HWND wnd;
   HDC chain_dc;
   struct wsi_win32_image     images[0];
};

VKAPI_ATTR VkBool32 VKAPI_CALL
wsi_GetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                 uint32_t queueFamilyIndex)
{
   return TRUE;
}

VKAPI_ATTR VkResult VKAPI_CALL
wsi_CreateWin32SurfaceKHR(VkInstance _instance,
                          const VkWin32SurfaceCreateInfoKHR *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          VkSurfaceKHR *pSurface)
{
   VK_FROM_HANDLE(vk_instance, instance, _instance);
   VkIcdSurfaceWin32 *surface;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR);

   surface = vk_zalloc2(&instance->alloc, pAllocator, sizeof(*surface), 8,
                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (surface == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface->base.platform = VK_ICD_WSI_PLATFORM_WIN32;

   surface->hinstance = pCreateInfo->hinstance;
   surface->hwnd = pCreateInfo->hwnd;

   *pSurface = VkIcdSurfaceBase_to_handle(&surface->base);

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

static VkResult
wsi_win32_surface_get_capabilities(VkIcdSurfaceBase *surf,
                                struct wsi_device *wsi_device,
                                VkSurfaceCapabilitiesKHR* caps)
{
   VkIcdSurfaceWin32 *surface = (VkIcdSurfaceWin32 *)surf;

   RECT win_rect;
   if (!GetClientRect(surface->hwnd, &win_rect))
      return VK_ERROR_SURFACE_LOST_KHR;

   caps->minImageCount = 1;

   if (!wsi_device->sw && wsi_device->win32.get_d3d12_command_queue) {
      /* DXGI doesn't support random presenting order (images need to
       * be presented in the order they were acquired), so we can't
       * expose more than two image per swapchain.
       */
      caps->minImageCount = caps->maxImageCount = 2;
   } else {
      caps->minImageCount = 1;
      /* Software callbacke, there is no real maximum */
      caps->maxImageCount = 0;
   }

   caps->currentExtent = (VkExtent2D) {
      win_rect.right - win_rect.left,
      win_rect.bottom - win_rect.top
   };
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
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
      VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

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
   VK_OUTARRAY_MAKE_TYPED(VkSurfaceFormatKHR, out, pSurfaceFormats, pSurfaceFormatCount);

   VkFormat sorted_formats[ARRAY_SIZE(available_surface_formats)];
   get_sorted_vk_formats(wsi_device, sorted_formats);

   for (unsigned i = 0; i < ARRAY_SIZE(sorted_formats); i++) {
      vk_outarray_append_typed(VkSurfaceFormatKHR, &out, f) {
         f->format = sorted_formats[i];
         f->colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
      }
   }

   return vk_outarray_status(&out);
}

static VkResult
wsi_win32_surface_get_formats2(VkIcdSurfaceBase *icd_surface,
                               struct wsi_device *wsi_device,
                               const void *info_next,
                               uint32_t* pSurfaceFormatCount,
                               VkSurfaceFormat2KHR* pSurfaceFormats)
{
   VK_OUTARRAY_MAKE_TYPED(VkSurfaceFormat2KHR, out, pSurfaceFormats, pSurfaceFormatCount);

   VkFormat sorted_formats[ARRAY_SIZE(available_surface_formats)];
   get_sorted_vk_formats(wsi_device, sorted_formats);

   for (unsigned i = 0; i < ARRAY_SIZE(sorted_formats); i++) {
      vk_outarray_append_typed(VkSurfaceFormat2KHR, &out, f) {
         assert(f->sType == VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR);
         f->surfaceFormat.format = sorted_formats[i];
         f->surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
      }
   }

   return vk_outarray_status(&out);
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
   VK_OUTARRAY_MAKE_TYPED(VkRect2D, out, pRects, pRectCount);

   vk_outarray_append_typed(VkRect2D, &out, rect) {
      /* We don't know a size so just return the usual "I don't know." */
      *rect = (VkRect2D) {
         .offset = { 0, 0 },
         .extent = { UINT32_MAX, UINT32_MAX },
      };
   }

   return vk_outarray_status(&out);
}

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

static uint32_t
select_image_memory_type(const struct wsi_device *wsi,
                         uint32_t type_bits)
{
   return select_memory_type(wsi,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             type_bits);
}

static uint32_t
select_buffer_memory_type(const struct wsi_device *wsi,
                          uint32_t type_bits)
{
   return select_memory_type(wsi,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             type_bits);
}

static VkResult
wsi_create_win32_image_mem(const struct wsi_swapchain *drv_chain,
                           const struct wsi_image_info *info,
                           struct wsi_image *image)
{
   struct wsi_win32_swapchain *chain = (struct wsi_win32_swapchain *)drv_chain;
   const struct wsi_device *wsi = chain->base.wsi;

   if (chain->base.blit.type == WSI_SWAPCHAIN_BUFFER_BLIT) {
      VkResult result =
         wsi_create_buffer_blit_context(&chain->base, info, image, 0, false);
      if (result != VK_SUCCESS)
         return result;

      const VkImageSubresource image_subresource = {
         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
         .mipLevel = 0,
         .arrayLayer = 0,
      };
      VkSubresourceLayout image_layout;
      wsi->GetImageSubresourceLayout(chain->base.device, image->image,
                                     &image_subresource, &image_layout);

      image->row_pitches[0] = image_layout.rowPitch;
      return VK_SUCCESS;
   }

   struct wsi_win32_image *win32_image =
      container_of(image, struct wsi_win32_image, base);
   uint32_t image_idx =
      ((uintptr_t)win32_image - (uintptr_t)chain->images) /
      sizeof(*win32_image);
   if (FAILED(IDXGISwapChain_GetBuffer(chain->dxgi, image_idx,
                                       &IID_ID3D12Resource,
                                       &win32_image->swapchain_res)))
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;

   VkResult result =
      wsi->win32.create_image_memory(chain->base.device,
                                     win32_image->swapchain_res,
                                     &chain->base.alloc,
                                     chain->base.blit.type == WSI_SWAPCHAIN_NO_BLIT ?
                                     &image->memory : &image->blit.memory);
   if (result != VK_SUCCESS)
      return result;

   if (chain->base.blit.type == WSI_SWAPCHAIN_NO_BLIT)
      return VK_SUCCESS;

   VkImageCreateInfo create = info->create;

   create.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;

   result = wsi->CreateImage(chain->base.device, &create,
                             &chain->base.alloc, &image->blit.image);
   if (result != VK_SUCCESS)
      return result;

   result = wsi->BindImageMemory(chain->base.device, image->blit.image,
                                 image->blit.memory, 0);
   if (result != VK_SUCCESS)
      return result;

   VkMemoryRequirements reqs;
   wsi->GetImageMemoryRequirements(chain->base.device, image->image, &reqs);

   const VkMemoryDedicatedAllocateInfo memory_dedicated_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .image = image->blit.image,
      .buffer = VK_NULL_HANDLE,
   };
   const VkMemoryAllocateInfo memory_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memory_dedicated_info,
      .allocationSize = reqs.size,
      .memoryTypeIndex =
         info->select_image_memory_type(wsi, reqs.memoryTypeBits),
   };

   return wsi->AllocateMemory(chain->base.device, &memory_info,
                              &chain->base.alloc, &image->memory);
}

#define WSI_WIN32_LINEAR_STRIDE_ALIGN 256

static VkResult
wsi_configure_win32_image(struct wsi_win32_swapchain *chain,
                          const VkSwapchainCreateInfoKHR *pCreateInfo)
{
   struct wsi_image_info *info = &chain->base.image_info;
   VkResult result =
      wsi_configure_image(&chain->base, pCreateInfo, 0, info);
   if (result != VK_SUCCESS)
      return result;

   wsi_configure_blit_context(&chain->base, pCreateInfo, info);
   info->create_mem = wsi_create_win32_image_mem;
   info->select_image_memory_type = select_image_memory_type;
   info->select_blit_dst_memory_type =
      chain->base.blit.type == WSI_SWAPCHAIN_BUFFER_BLIT ?
      select_buffer_memory_type : select_image_memory_type;

   const uint32_t cpp = vk_format_get_blocksize(info->create.format);
   info->linear_stride = ALIGN_POT(info->create.extent.width * cpp,
                                   WSI_WIN32_LINEAR_STRIDE_ALIGN);
   info->size_align = 4096;

   return VK_SUCCESS;
}


static VkResult
wsi_win32_image_init(VkDevice device_h,
                     struct wsi_win32_swapchain *chain,
                     const VkSwapchainCreateInfoKHR *create_info,
                     const VkAllocationCallbacks *allocator,
                     struct wsi_win32_image *image)
{
   VkResult result = wsi_create_image(&chain->base, &chain->base.image_info,
                                      &image->base);
   if (result != VK_SUCCESS)
      return result;

    VkIcdSurfaceWin32 *win32_surface = (VkIcdSurfaceWin32 *)create_info->surface;
    chain->wnd = win32_surface->hwnd;
    chain->chain_dc = GetDC(chain->wnd);

    image->dc = CreateCompatibleDC(chain->chain_dc);
    HBITMAP bmp = NULL;

    BITMAPINFO info = { 0 };
    info.bmiHeader.biSize = sizeof(BITMAPINFO);
    info.bmiHeader.biWidth = create_info->imageExtent.width;
    info.bmiHeader.biHeight = -create_info->imageExtent.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    bmp = CreateDIBSection(image->dc, &info, DIB_RGB_COLORS, &image->ppvBits, NULL, 0);
    assert(bmp && image->ppvBits);

    SelectObject(image->dc, bmp);

    BITMAP header;
    int status = GetObject(bmp, sizeof(BITMAP), &header);
    (void)status;
    image->bmp_row_pitch = header.bmWidthBytes;
    image->bmp = bmp;
    image->chain = chain;

   return VK_SUCCESS;
}

static void
wsi_win32_image_finish(struct wsi_win32_swapchain *chain,
                       const VkAllocationCallbacks *allocator,
                       struct wsi_win32_image *image)
{
   if (image->swapchain_res)
      ID3D12Resource_Release(image->swapchain_res);

   DeleteDC(image->dc);
   if(image->bmp)
      DeleteObject(image->bmp);
   wsi_destroy_image(&chain->base, &image->base);
}

static VkResult
wsi_win32_swapchain_destroy(struct wsi_swapchain *drv_chain,
                              const VkAllocationCallbacks *allocator)
{
   struct wsi_win32_swapchain *chain =
      (struct wsi_win32_swapchain *) drv_chain;

   for (uint32_t i = 0; i < chain->base.image_count; i++)
      wsi_win32_image_finish(chain, allocator, &chain->images[i]);

   DeleteDC(chain->chain_dc);

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

   /* Bail early if the swapchain is broken */
   if (chain->status != VK_SUCCESS)
      return chain->status;

   /* TODO: wait on image idleness */
   for (uint32_t i = 0; i < chain->base.image_count; i++) {
      if (chain->images[i].state == WSI_IMAGE_IDLE) {
         *image_index = i;
         chain->images[i].state = WSI_IMAGE_DRAWING;
         return VK_SUCCESS;
      }
   }

   return VK_TIMEOUT;
}

static VkResult
wsi_win32_queue_present(struct wsi_swapchain *drv_chain,
                          uint32_t image_index,
                          const VkPresentRegionKHR *damage)
{
   struct wsi_win32_swapchain *chain = (struct wsi_win32_swapchain *) drv_chain;
   assert(image_index < chain->base.image_count);
   struct wsi_win32_image *image = &chain->images[image_index];
   VkResult result;

   assert(image->state == WSI_IMAGE_DRAWING);

   if (chain->dxgi) {
      uint32_t rect_count = damage ? damage->rectangleCount : 0;
      STACK_ARRAY(RECT, rects, rect_count);

      for (uint32_t r = 0; r < rect_count; r++) {
         rects[r].left = damage->pRectangles[r].offset.x;
         rects[r].top = damage->pRectangles[r].offset.y;
         rects[r].right = damage->pRectangles[r].offset.x + damage->pRectangles[r].extent.width;
         rects[r].bottom = damage->pRectangles[r].offset.y + damage->pRectangles[r].extent.height;
      }

      DXGI_PRESENT_PARAMETERS params = {
         .DirtyRectsCount = rect_count,
         .pDirtyRects = rects,
      };

      image->state = WSI_IMAGE_QUEUED;

      HRESULT hres = IDXGISwapChain1_Present1(chain->dxgi, 0, 0, &params);
      switch (hres) {
      case DXGI_ERROR_DEVICE_REMOVED: return VK_ERROR_SURFACE_LOST_KHR;
      case E_OUTOFMEMORY: return VK_ERROR_OUT_OF_DEVICE_MEMORY;
      default:
         if (FAILED(hres))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
         break;
      }

      /* Mark the other image idle */
      chain->images[(image_index + 1) % 2].state = WSI_IMAGE_IDLE;
      chain->status = VK_SUCCESS;
      return VK_SUCCESS;
   }

   assert(chain->base.blit.type == WSI_SWAPCHAIN_BUFFER_BLIT);

   char *ptr;
   char *dptr = image->ppvBits;
   result = chain->base.wsi->MapMemory(chain->base.device,
                                       image->base.blit.memory,
                                       0, image->base.sizes[0], 0, (void**)&ptr);

   for (unsigned h = 0; h < chain->extent.height; h++) {
      memcpy(dptr, ptr, chain->extent.width * 4);
      dptr += image->bmp_row_pitch;
      ptr += image->base.row_pitches[0];
   }
   if(StretchBlt(chain->chain_dc, 0, 0, chain->extent.width, chain->extent.height, image->dc, 0, 0, chain->extent.width, chain->extent.height, SRCCOPY))
      result = VK_SUCCESS;
   else
     result = VK_ERROR_MEMORY_MAP_FAILED;

   chain->base.wsi->UnmapMemory(chain->base.device, image->base.blit.memory);
   if (result != VK_SUCCESS)
      chain->status = result;

   image->state = WSI_IMAGE_IDLE;

   if (result != VK_SUCCESS)
      return result;

   return chain->status;
}

static IDXGIFactory4 *
dxgi_get_factory(bool debug)
{
   static const GUID IID_IDXGIFactory4 = {
      0x1bc6ea02, 0xef36, 0x464f,
      { 0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a }
   };

   HMODULE dxgi_mod = LoadLibraryA("DXGI.DLL");
   if (!dxgi_mod) {
      return NULL;
   }

   typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY2)(UINT flags, REFIID riid, void **ppFactory);
   PFN_CREATE_DXGI_FACTORY2 CreateDXGIFactory2;

   CreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(dxgi_mod, "CreateDXGIFactory2");
   if (!CreateDXGIFactory2) {
      return NULL;
   }

   UINT flags = 0;
   if (debug)
      flags |= DXGI_CREATE_FACTORY_DEBUG;

   IDXGIFactory4 *factory;
   HRESULT hr = CreateDXGIFactory2(flags, &IID_IDXGIFactory4, &factory);
   if (FAILED(hr)) {
      return NULL;
   }

   return factory;
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
   VkIcdSurfaceWin32 *surface = (VkIcdSurfaceWin32 *)icd_surface;
   struct wsi_win32 *wsi =
      (struct wsi_win32 *) wsi_device->wsi[VK_ICD_WSI_PLATFORM_WIN32];

   assert(create_info->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);

   const unsigned num_images = create_info->minImageCount;
   struct wsi_win32_swapchain *chain;
   size_t size = sizeof(*chain) + num_images * sizeof(chain->images[0]);

   chain = vk_zalloc(allocator, size,
                8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (chain == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult result = wsi_swapchain_init(wsi_device, &chain->base, device,
                                        create_info, allocator,
                                        WSI_SWAPCHAIN_BUFFER_BLIT);
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
   chain->base.blit.type = WSI_SWAPCHAIN_BUFFER_BLIT;

   chain->surface = surface;

   if (!wsi_device->sw &&
       wsi_device->win32.get_d3d12_command_queue) {
      ID3D12CommandQueue *queue =
         wsi_device->win32.get_d3d12_command_queue(device);
      IUnknown *unknown;
      ID3D12CommandQueue_QueryInterface(queue, &IID_IUnknown, &unknown);

      IDXGIFactory4 *factory = dxgi_get_factory(true);
      if (!factory)
         return VK_ERROR_INITIALIZATION_FAILED;

      assert(factory);

      DXGI_SWAP_CHAIN_DESC1 desc = {
         .Width = create_info->imageExtent.width,
         .Height = create_info->imageExtent.height,
         .Format = create_info->imageFormat == VK_FORMAT_B8G8R8A8_SRGB ?
                   DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM,
         .Stereo = create_info->imageArrayLayers > 1,
         .SampleDesc = { .Count = 1 },
	 .BufferCount = create_info->minImageCount,
	 .Scaling = DXGI_SCALING_NONE,
         .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
         .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
         .Flags = 0,
      };

      if (create_info->imageUsage &
          (VK_IMAGE_USAGE_SAMPLED_BIT |
	   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
         desc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;

      if (create_info->imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
         desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;

      if (FAILED(IDXGIFactory2_CreateSwapChainForHwnd(factory, unknown, surface->hwnd,
                                                      &desc, NULL, NULL, &chain->dxgi)))
         return VK_ERROR_INITIALIZATION_FAILED;

      /* Release the reference taken by QueryInterface() */
      ID3D12CommandQueue_Release(queue);

      /* d3d12 doesn't support DXGI_USAGE_UNORDERED_ACCESS, so we need an
       * extra blit in that case.
       */
      chain->base.blit.type =
         (create_info->imageUsage & VK_IMAGE_USAGE_STORAGE_BIT) ?
         WSI_SWAPCHAIN_IMAGE_BLIT : WSI_SWAPCHAIN_NO_BLIT;
      chain->base.blit.type = WSI_SWAPCHAIN_IMAGE_BLIT;
   }

   result = wsi_configure_win32_image(chain, create_info);
   if (result != VK_SUCCESS) {
      vk_free(allocator, chain);
      goto fail_init_images;
   }

   for (uint32_t image = 0; image < chain->base.image_count; image++) {
      result = wsi_win32_image_init(device, chain,
                                    create_info, allocator,
                                    &chain->images[image]);
      if (result != VK_SUCCESS) {
         while (image > 0) {
            --image;
            wsi_win32_image_finish(chain, allocator,
                                   &chain->images[image]);
         }
         wsi_destroy_image_info(&chain->base, &chain->base.image_info);
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
