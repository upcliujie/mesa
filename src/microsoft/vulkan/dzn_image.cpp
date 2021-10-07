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
#include "vk_format.h"
#include "vk_util.h"

static inline VkExtent3D
dzn_sanitize_image_extent(const VkImageType imageType,
                          const VkExtent3D imageExtent)
{
   switch (imageType) {
   case VK_IMAGE_TYPE_1D:
      return VkExtent3D { imageExtent.width, 1, 1 };
   case VK_IMAGE_TYPE_2D:
      return VkExtent3D { imageExtent.width, imageExtent.height, 1 };
   case VK_IMAGE_TYPE_3D:
      return imageExtent;
   default:
      unreachable("invalid image type");
   }
}

VkResult
dzn_image_create(VkDevice _device,
                 const VkImageCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks* alloc,
                 VkImage *pImage)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   dzn_image *image = NULL;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
   assert(pCreateInfo->mipLevels > 0);
   assert(pCreateInfo->arrayLayers > 0);
   assert(pCreateInfo->samples > 0);
   assert(pCreateInfo->extent.width > 0);
   assert(pCreateInfo->extent.height > 0);
   assert(pCreateInfo->extent.depth > 0);

   image = (dzn_image *)
      vk_object_alloc(&device->vk, alloc, sizeof(*image),
                      VK_OBJECT_TYPE_IMAGE);
   if (!image)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   image->type = pCreateInfo->imageType;
   image->extent = dzn_sanitize_image_extent(pCreateInfo->imageType,
                                             pCreateInfo->extent);
   image->vk_format = pCreateInfo->format;
   // image->format = dzn_get_format(pCreateInfo->format);
   image->aspects = vk_format_aspects(image->vk_format);
   image->levels = pCreateInfo->mipLevels;
   image->array_size = pCreateInfo->arrayLayers;
   image->samples = pCreateInfo->samples;
   image->usage = pCreateInfo->usage;
   image->create_flags = pCreateInfo->flags;
   image->tiling = pCreateInfo->tiling;

   image->desc.Dimension = (D3D12_RESOURCE_DIMENSION)(D3D12_RESOURCE_DIMENSION_TEXTURE1D + pCreateInfo->imageType);

   image->desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
   if (pCreateInfo->samples > 1)
      image->desc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;

   image->desc.Width = image->extent.width;
   image->desc.Height = image->extent.height;
   image->desc.DepthOrArraySize = pCreateInfo->imageType == VK_IMAGE_TYPE_3D ?
                                  image->extent.depth :
                                  pCreateInfo->arrayLayers;
   image->desc.MipLevels = pCreateInfo->mipLevels;
   image->desc.Format = dzn_get_format(pCreateInfo->format);
   image->desc.SampleDesc.Count = pCreateInfo->samples;
   image->desc.SampleDesc.Quality = 0;

   /* PROBLEM: D3D12 requires D3D12_TEXTURE_LAYOUT_ROW_MAJOR resources
    * to be allocated on a heap with the D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER
    * flag. We can't reasonably know this up-front, and using the flag always
    * comes with a bunch more limitations. So we have to stop using it, full
    * stop. That's going to be hairy, as we'll have to use buffer resources
    * instead.
    */
   image->desc.Layout = (D3D12_TEXTURE_LAYOUT)pCreateInfo->tiling;

   image->desc.Flags = D3D12_RESOURCE_FLAG_NONE;

   if (image->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      image->desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

   if (image->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      image->desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

      if (!(image->usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)))
         image->desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
   }

   if (image->usage & VK_IMAGE_USAGE_STORAGE_BIT)
      image->desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

   *pImage = dzn_image_to_handle(image);

   return VK_SUCCESS;
}

VkResult
dzn_CreateImage(VkDevice device,
                const VkImageCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkImage *pImage)
{
   const VkExternalMemoryImageCreateInfo *create_info =
      (const VkExternalMemoryImageCreateInfo *)vk_find_struct_const(
          pCreateInfo->pNext, EXTERNAL_MEMORY_IMAGE_CREATE_INFO);

#if 0
    VkExternalMemoryHandleTypeFlags supported =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;

   if (create_info && (create_info->handleTypes & supported))
      return dzn_image_from_external(device, pCreateInfo, create_info,
                                     pAllocator, pImage);
#endif

#if 0
   const VkImageSwapchainCreateInfoKHR *swapchain_info = (const VkImageSwapchainCreateInfoKHR *)
      vk_find_struct_const(pCreateInfo->pNext, IMAGE_SWAPCHAIN_CREATE_INFO_KHR);
   if (swapchain_info && swapchain_info->swapchain != VK_NULL_HANDLE)
      return dzn_image_from_swapchain(device, pCreateInfo, swapchain_info,
                                      pAllocator, pImage);
#endif

   return dzn_image_create(device, pCreateInfo, pAllocator, pImage);
}

void
dzn_DestroyImage(VkDevice _device, VkImage _image,
                 const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_image, image, _image);

   if (!image)
      return;

   // TODO: release image

   vk_object_free(&device->vk, pAllocator, image);
}

static dzn_image *
dzn_swapchain_get_image(VkSwapchainKHR swapchain,
                        uint32_t index)
{
   uint32_t n_images = index + 1;
   VkImage *images = (VkImage *)malloc(sizeof(*images) * n_images);
   VkResult result = wsi_common_get_images(swapchain, &n_images, images);

   if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
      free(images);
      return NULL;
   }

   DZN_FROM_HANDLE(dzn_image, image, images[index]);
   free(images);

   return image;
}

VkResult dzn_BindImageMemory2(
    VkDevice                                    _device,
    uint32_t                                    bindInfoCount,
    const VkBindImageMemoryInfo*                pBindInfos)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);

   for (uint32_t i = 0; i < bindInfoCount; i++) {
      const VkBindImageMemoryInfo *bind_info = &pBindInfos[i];
      DZN_FROM_HANDLE(dzn_device_memory, mem, bind_info->memory);
      DZN_FROM_HANDLE(dzn_image, image, bind_info->image);
      bool did_bind = false;

      vk_foreach_struct_const(s, bind_info->pNext) {
         switch (s->sType) {
         case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR: {
            const VkBindImageMemorySwapchainInfoKHR *swapchain_info =
               (const VkBindImageMemorySwapchainInfoKHR *) s;
            dzn_image *swapchain_image =
               dzn_swapchain_get_image(swapchain_info->swapchain,
                                       swapchain_info->imageIndex);
            assert(swapchain_image);
            assert(image->aspects == swapchain_image->aspects);
            assert(mem == NULL);

            /* TODO: something something binding the image memory */
            assert(false);

            did_bind = true;
            break;
         }
         default:
            dzn_debug_ignored_stype(s->sType);
            break;
         }
      }

      if (!did_bind) {
         image->mem = mem;
         HRESULT hr = device->dev->CreatePlacedResource(mem->heap,
                                                        bind_info->memoryOffset,
                                                        &image->desc,
                                                        mem->initial_state,
                                                        NULL, IID_PPV_ARGS(&image->res));
         assert(hr == S_OK);
         did_bind = true;
      }
   }

   return VK_SUCCESS;
}

void
dzn_GetImageMemoryRequirements2(VkDevice _device,
                                const VkImageMemoryRequirementsInfo2 *pInfo,
                                VkMemoryRequirements2 *pMemoryRequirements)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_image, image, pInfo->image);

   vk_foreach_struct_const(ext, pInfo->pNext) {
      dzn_debug_ignored_stype(ext->sType);
   }

   vk_foreach_struct(ext, pMemoryRequirements->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS: {
         VkMemoryDedicatedRequirements *requirements =
            (VkMemoryDedicatedRequirements *)ext;
         /* TODO: figure out dedicated allocations */
         requirements->prefersDedicatedAllocation = false;
         requirements->requiresDedicatedAllocation = false;
         break;
      }

      default:
         dzn_debug_ignored_stype(ext->sType);
         break;
      }
   }

   D3D12_RESOURCE_ALLOCATION_INFO info = device->dev->GetResourceAllocationInfo(0, 1, &image->desc);

   pMemoryRequirements->memoryRequirements = VkMemoryRequirements {
      .size = info.SizeInBytes,
      .alignment = info.Alignment,
      .memoryTypeBits = (1ull << device->physical_device->memory.memoryTypeCount) - 1,
   };
}

UINT
dzn_get_subresource_index(const D3D12_RESOURCE_DESC *desc,
                          VkImageAspectFlags aspectMask,
                          unsigned mipLevel, unsigned arrayLayer)
{
   int planeSlice = aspectMask ==
      VK_IMAGE_ASPECT_STENCIL_BIT ? 1 : 0;

   return mipLevel +
          arrayLayer * desc->MipLevels +
          planeSlice * desc->MipLevels * desc->DepthOrArraySize;
}

static UINT
get_subresource_index(const D3D12_RESOURCE_DESC *desc,
                      const VkImageSubresource *subresource)
{
   return dzn_get_subresource_index(desc, subresource->aspectMask, subresource->mipLevel, subresource->arrayLayer);
}


void
dzn_GetImageSubresourceLayout(VkDevice _device,
                              VkImage _image,
                              const VkImageSubresource *subresource,
                              VkSubresourceLayout *layout)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_image, image, _image);

   UINT subres_index = get_subresource_index(&image->desc, subresource);

   D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
   UINT num_rows;
   UINT64 row_size, total_size;
   device->dev->GetCopyableFootprints(&image->desc,
                                      subres_index, 1,
                                      0, // base-offset?
                                      &footprint,
                                      &num_rows, &row_size,
                                      &total_size);

   layout->offset = footprint.Offset;
   layout->rowPitch = footprint.Footprint.RowPitch;
   layout->depthPitch = layout->rowPitch * footprint.Footprint.Height;
   layout->arrayPitch = layout->depthPitch; // uuuh... why is this even here?
   layout->size = total_size;
}

static D3D12_SRV_DIMENSION
translate_view_type(VkImageViewType in, uint32_t samples)
{
   switch (in) {
   case VK_IMAGE_VIEW_TYPE_1D: return D3D12_SRV_DIMENSION_TEXTURE1D;
   case VK_IMAGE_VIEW_TYPE_2D:
      return samples > 1 ?
             D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
   case VK_IMAGE_VIEW_TYPE_3D: return D3D12_SRV_DIMENSION_TEXTURE3D;
   case VK_IMAGE_VIEW_TYPE_CUBE: return D3D12_SRV_DIMENSION_TEXTURECUBE;
   case VK_IMAGE_VIEW_TYPE_1D_ARRAY: return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
   case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
      return samples > 1 ?
             D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY :
             D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
   case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: return D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
   default: unreachable("Invalid type");
   }
}

static D3D12_SHADER_COMPONENT_MAPPING
translate_swizzle(VkComponentSwizzle in, uint32_t comp)
{
   switch (in) {
   case VK_COMPONENT_SWIZZLE_IDENTITY:
      return (D3D12_SHADER_COMPONENT_MAPPING)
             (comp + D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0);
   case VK_COMPONENT_SWIZZLE_ZERO:
      return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
   case VK_COMPONENT_SWIZZLE_ONE:
      return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
   case VK_COMPONENT_SWIZZLE_R:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
   case VK_COMPONENT_SWIZZLE_G:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
   case VK_COMPONENT_SWIZZLE_B:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
   case VK_COMPONENT_SWIZZLE_A:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
   default: unreachable("Invalid swizzle");
   }
}

VkResult
dzn_CreateImageView(VkDevice _device,
                    const VkImageViewCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkImageView *pView)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_image, image, pCreateInfo->image);
   dzn_image_view *iview;

   iview = (dzn_image_view *)
      vk_object_zalloc(&device->vk, pAllocator, sizeof(*iview),
                       VK_OBJECT_TYPE_IMAGE_VIEW);
   if (iview == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   const VkImageSubresourceRange *range = &pCreateInfo->subresourceRange;

   assert(range->layerCount > 0);
   assert(range->baseMipLevel < image->levels);

   /* View usage should be a subset of image usage */
   assert(image->usage & (VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_STORAGE_BIT |
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));

   switch (image->type) {
   default:
      unreachable("bad VkImageType");
   case VK_IMAGE_TYPE_1D:
   case VK_IMAGE_TYPE_2D:
      assert(range->baseArrayLayer + dzn_get_layerCount(image, range) - 1 <= image->array_size);
      break;
   case VK_IMAGE_TYPE_3D:
      assert(range->baseArrayLayer + dzn_get_layerCount(image, range) - 1
             <= u_minify(image->extent.depth, range->baseMipLevel));
      break;
   }

   iview->image = image;
   iview->vk_format = pCreateInfo->format;

   /* Format is undefined, this can happen when using external formats. Set
    * view format from the passed conversion info.
    */

   iview->extent = VkExtent3D {
      .width  = u_minify(image->extent.width , range->baseMipLevel),
      .height = u_minify(image->extent.height, range->baseMipLevel),
      .depth  = u_minify(image->extent.depth , range->baseMipLevel),
   };

   /* TODO: have a shader-invisible pool for iview descs, and copy those with
    * CopyDescriptors() when UpdateDescriptorSets() is called.
    */
   iview->desc.Format = dzn_get_format(pCreateInfo->format);
   iview->desc.ViewDimension =
      translate_view_type(pCreateInfo->viewType, image->samples);
   iview->desc.Shader4ComponentMapping =
      D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
         translate_swizzle(pCreateInfo->components.r, 0),
         translate_swizzle(pCreateInfo->components.g, 1),
         translate_swizzle(pCreateInfo->components.b, 2),
         translate_swizzle(pCreateInfo->components.a, 3));
   switch (iview->desc.ViewDimension) {
   case D3D12_SRV_DIMENSION_TEXTURE1D:
      iview->desc.Texture1D.MostDetailedMip =
         pCreateInfo->subresourceRange.baseMipLevel;
      iview->desc.Texture1D.MipLevels =
         pCreateInfo->subresourceRange.levelCount;
      break;
   case D3D12_SRV_DIMENSION_TEXTURE2D:
      iview->desc.Texture2D.MostDetailedMip =
         pCreateInfo->subresourceRange.baseMipLevel;
      iview->desc.Texture2D.MipLevels =
         pCreateInfo->subresourceRange.levelCount;
      break;
   case D3D12_SRV_DIMENSION_TEXTURE2DMS:
      break;
   case D3D12_SRV_DIMENSION_TEXTURE3D:
      iview->desc.Texture3D.MostDetailedMip =
         pCreateInfo->subresourceRange.baseMipLevel;
      iview->desc.Texture3D.MipLevels =
         pCreateInfo->subresourceRange.levelCount;
      break;
   case D3D12_SRV_DIMENSION_TEXTURECUBE:
      iview->desc.TextureCube.MostDetailedMip =
         pCreateInfo->subresourceRange.baseMipLevel;
      iview->desc.TextureCube.MipLevels =
         pCreateInfo->subresourceRange.levelCount;
      break;
   case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
      iview->desc.Texture1DArray.MostDetailedMip =
         pCreateInfo->subresourceRange.baseMipLevel;
      iview->desc.Texture1DArray.MipLevels =
         pCreateInfo->subresourceRange.levelCount;
      iview->desc.Texture1DArray.FirstArraySlice =
         pCreateInfo->subresourceRange.baseArrayLayer;
      iview->desc.Texture1DArray.ArraySize =
         pCreateInfo->subresourceRange.layerCount;
      break;
   case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
      iview->desc.Texture2DArray.MostDetailedMip =
         pCreateInfo->subresourceRange.baseMipLevel;
      iview->desc.Texture2DArray.MipLevels =
         pCreateInfo->subresourceRange.levelCount;
      iview->desc.Texture2DArray.FirstArraySlice =
         pCreateInfo->subresourceRange.baseArrayLayer;
      iview->desc.Texture2DArray.ArraySize =
         pCreateInfo->subresourceRange.layerCount;
      break;
   case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
      iview->desc.Texture2DMSArray.FirstArraySlice =
         pCreateInfo->subresourceRange.baseArrayLayer;
      iview->desc.Texture2DMSArray.ArraySize =
         pCreateInfo->subresourceRange.layerCount;
      break;
   case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
      iview->desc.TextureCubeArray.MostDetailedMip =
         pCreateInfo->subresourceRange.baseMipLevel;
      iview->desc.TextureCubeArray.MipLevels =
         pCreateInfo->subresourceRange.levelCount;
      iview->desc.TextureCubeArray.First2DArrayFace =
         pCreateInfo->subresourceRange.baseArrayLayer;
      iview->desc.TextureCubeArray.NumCubes =
         pCreateInfo->subresourceRange.layerCount / 6;
      break;
   default: unreachable("Invalid dimension");
   }

   if (image->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      D3D12_RENDER_TARGET_VIEW_DESC desc;
      desc.Format = iview->desc.Format;

      /* TODO: fill these out based on stuff above */
      assert(image->type == VK_IMAGE_TYPE_2D);
      desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
      assert(range->baseMipLevel == 0);
      assert(range->layerCount == 1);
      desc.Texture2D.MipSlice = 0;
      desc.Texture2D.PlaneSlice = 0;

      d3d12_descriptor_pool_alloc_handle(device->rtv_pool, &iview->rt_handle);
      device->dev->CreateRenderTargetView(image->res, &desc,
                                          iview->rt_handle.cpu_handle);
   }

   if (image->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      D3D12_DEPTH_STENCIL_VIEW_DESC desc = { };
      desc.Format = iview->desc.Format;

      /* TODO: fill these out based on stuff above */
      assert(image->type == VK_IMAGE_TYPE_2D);
      desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipSlice = 0;
      d3d12_descriptor_pool_alloc_handle(device->dsv_pool, &iview->zs_handle);
      device->dev->CreateDepthStencilView(image->res, &desc,
                                          iview->zs_handle.cpu_handle);
   }

   *pView = dzn_image_view_to_handle(iview);

   return VK_SUCCESS;
}

void
dzn_DestroyImageView(VkDevice _device,
                     VkImageView imageView,
                     const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_image_view, iview, imageView);

   if (!iview)
      return;

   if (iview->image->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      d3d12_descriptor_handle_free(&iview->rt_handle);

   if (iview->image->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      d3d12_descriptor_handle_free(&iview->zs_handle);

   vk_object_free(&device->vk, pAllocator, iview);
}

VkResult
dzn_CreateBufferView(VkDevice _device,
                     const VkBufferViewCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator,
                     VkBufferView *pView)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_buffer, buf, pCreateInfo->buffer);
   enum pipe_format pfmt = vk_format_to_pipe_format(pCreateInfo->format);
   unsigned blksz = util_format_get_blocksize(pfmt);
   VkDeviceSize size =
      pCreateInfo->range == VK_WHOLE_SIZE ?
      buf->size - pCreateInfo->offset : pCreateInfo->range;
   dzn_buffer_view *bview;

   bview = (dzn_buffer_view *)
      vk_object_zalloc(&device->vk, pAllocator, sizeof(*bview),
                       VK_OBJECT_TYPE_BUFFER_VIEW);

   bview->buffer = buf;
   bview->desc.Format = dzn_get_format(pCreateInfo->format);
   bview->desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
   bview->desc.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   bview->desc.Buffer.FirstElement = pCreateInfo->offset / blksz;
   bview->desc.Buffer.NumElements = size / blksz;
   bview->desc.Buffer.StructureByteStride = blksz;
   bview->desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

   *pView = dzn_buffer_view_to_handle(bview);
   return VK_SUCCESS;
}

void
dzn_DestroyBufferView(VkDevice _device,
                      VkBufferView bufferView,
                      const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_buffer_view, bview, bufferView);

   if (!bview)
      return;

   vk_object_free(&device->vk, pAllocator, bview);
}
