/*
 * Copyright © 2023 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_private.h"

struct radv_level_layout {
   uint32_t layer_offset;
   uint32_t level_offset;
   uint32_t row_stride;
};

static void
radv_get_level_layout(struct radv_image *image, VkImageSubresourceLayers res, uint32_t layer,
                      struct radv_level_layout *layout)
{
   struct gfx9_surf_layout *gfx9_layout = &image->planes[0].surface.u.gfx9;

   layout->layer_offset = gfx9_layout->surf_slice_size * (res.baseArrayLayer + layer);
   layout->level_offset = gfx9_layout->offset[res.mipLevel];

   if (image->vk.format == VK_FORMAT_R32G32B32_UINT || image->vk.format == VK_FORMAT_R32G32B32_SINT ||
       image->vk.format == VK_FORMAT_R32G32B32_SFLOAT) {
      /* Adjust the number of bytes between each row because
       * the pitch is actually the number of components per
       * row.
       */
      layout->row_stride = gfx9_layout->surf_pitch * image->planes[0].surface.bpe / 3;
   } else {
      uint32_t pitch = image->planes[0].surface.is_linear ? gfx9_layout->pitch[res.mipLevel] : gfx9_layout->surf_pitch;
      assert(util_is_power_of_two_nonzero(image->planes[0].surface.bpe));
      layout->row_stride = pitch * image->planes[0].surface.bpe;
   }
}

enum radv_copy_dst {
   RADV_COPY_DST_BUFFER,
   RADV_COPY_DST_IMAGE,
};

static void
radv_copy_image_buffer(struct radv_image *image, uint8_t *image_ptr, const VkImageToMemoryCopyEXT *region,
                       uint32_t layer, enum radv_copy_dst dst_res)
{
   struct radv_level_layout layout;
   radv_get_level_layout(image, region->imageSubresource, layer, &layout);

   image_ptr += layout.layer_offset;
   image_ptr += layout.level_offset;

   uint32_t pixel_stride = vk_format_get_blocksize(image->vk.format);

   uint32_t block_width = vk_format_get_blockwidth(image->vk.format);
   uint32_t block_height = vk_format_get_blockheight(image->vk.format);
   uint32_t block_depth = util_format_get_blockdepth(vk_format_to_pipe_format(image->vk.format));

   VkOffset3D offset = {
      .x = region->imageOffset.x / block_width,
      .y = region->imageOffset.y / block_height,
      .z = region->imageOffset.z / block_depth,
   };

   VkExtent3D extent = {
      .width = region->imageExtent.width / block_width,
      .height = region->imageExtent.height / block_height,
      .depth = region->imageExtent.depth / block_depth,
   };

   uint32_t x_offset = pixel_stride * offset.x;

   uint32_t buffer_y_stride = region->memoryRowLength ? region->memoryRowLength : extent.width;
   uint32_t buffer_z_stride = region->memoryImageHeight ? region->memoryImageHeight : extent.height;
   buffer_z_stride *= buffer_y_stride;

   buffer_y_stride *= pixel_stride;
   buffer_z_stride *= pixel_stride;

   uint8_t *buffer = region->pHostPointer;
   buffer += buffer_z_stride * layer;

   if (dst_res == RADV_COPY_DST_BUFFER) {
      for (uint32_t z = 0; z < extent.depth; z++) {
         for (uint32_t y = 0; y < extent.height; y++) {
            uint32_t dst_row_offset = buffer_y_stride * y + buffer_z_stride * z;
            uint32_t src_row_offset = layout.row_stride * (y + offset.y + (z + offset.z) * image->vk.extent.height);

            memcpy(buffer + dst_row_offset, image_ptr + src_row_offset + x_offset, pixel_stride * extent.width);
         }
      }
   } else {
      for (uint32_t z = 0; z < extent.depth; z++) {
         for (uint32_t y = 0; y < extent.height; y++) {
            uint32_t dst_row_offset = layout.row_stride * (y + offset.y + (z + offset.z) * image->vk.extent.height);
            uint32_t src_row_offset = buffer_y_stride * y + buffer_z_stride * z;

            memcpy(image_ptr + dst_row_offset + x_offset, buffer + src_row_offset, pixel_stride * extent.width);
         }
      }
   }
}

static void
radv_copy_image_rect(struct radv_image *dst_image, uint8_t *dst, struct radv_image *src_image, const uint8_t *src,
                     const VkImageCopy2 *region, uint32_t layer)
{
   struct radv_level_layout dst_layout;
   radv_get_level_layout(dst_image, region->dstSubresource, layer, &dst_layout);

   struct radv_level_layout src_layout;
   radv_get_level_layout(src_image, region->srcSubresource, layer, &src_layout);

   dst += dst_layout.layer_offset;
   src += src_layout.layer_offset;

   dst += dst_layout.level_offset;
   src += src_layout.level_offset;

   uint32_t pixel_stride = vk_format_get_blocksize(src_image->vk.format);

   for (uint32_t z = 0; z < region->extent.depth; z++) {
      for (uint32_t y = 0; y < region->extent.height; y++) {
         uint32_t dst_row_offset =
            dst_layout.row_stride * (y + region->dstOffset.y + (z + region->dstOffset.z) * dst_image->vk.extent.height);
         uint32_t src_row_offset =
            src_layout.row_stride * (y + region->srcOffset.y + (z + region->srcOffset.z) * src_image->vk.extent.height);

         uint32_t dst_x_offset = pixel_stride * region->dstOffset.x;
         uint32_t src_x_offset = pixel_stride * region->srcOffset.x;

         memcpy(dst + dst_row_offset + dst_x_offset, src + src_row_offset + src_x_offset,
                pixel_stride * region->extent.width);
      }
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CopyMemoryToImageEXT(VkDevice _device, const VkCopyMemoryToImageInfoEXT *pCopyMemoryToImageInfo)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_image, image, pCopyMemoryToImageInfo->dstImage);

   assert(image->plane_count == 1);

   uint8_t *image_ptr = device->ws->buffer_map(image->bindings[0].bo);
   image_ptr += image->bindings[0].offset;

   for (uint32_t i = 0; i < pCopyMemoryToImageInfo->regionCount; i++) {
      const VkMemoryToImageCopyEXT *region = &pCopyMemoryToImageInfo->pRegions[i];
      VkImageToMemoryCopyEXT tmp_region = {
         .pHostPointer = (void *)region->pHostPointer,
         .memoryRowLength = region->memoryRowLength,
         .memoryImageHeight = region->memoryImageHeight,
         .imageSubresource = region->imageSubresource,
         .imageOffset = region->imageOffset,
         .imageExtent = region->imageExtent,
      };

      assert(!radv_dcc_enabled(image, region->imageSubresource.mipLevel));

      uint32_t layer_count = vk_image_subresource_layer_count(&image->vk, &region->imageSubresource);
      for (uint32_t layer = 0; layer < layer_count; layer++)
         radv_copy_image_buffer(image, image_ptr, &tmp_region, layer, RADV_COPY_DST_IMAGE);
   }

   device->ws->buffer_unmap(image->bindings[0].bo);

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CopyImageToMemoryEXT(VkDevice _device, const VkCopyImageToMemoryInfoEXT *pCopyImageToMemoryInfo)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_image, image, pCopyImageToMemoryInfo->srcImage);

   assert(image->plane_count == 1);

   uint8_t *image_ptr = device->ws->buffer_map(image->bindings[0].bo);
   image_ptr += image->bindings[0].offset;

   for (uint32_t i = 0; i < pCopyImageToMemoryInfo->regionCount; i++) {
      const VkImageToMemoryCopyEXT *region = &pCopyImageToMemoryInfo->pRegions[i];

      assert(!radv_dcc_enabled(image, region->imageSubresource.mipLevel));

      uint32_t layer_count = vk_image_subresource_layer_count(&image->vk, &region->imageSubresource);
      for (uint32_t layer = 0; layer < layer_count; layer++)
         radv_copy_image_buffer(image, image_ptr, region, layer, RADV_COPY_DST_BUFFER);
   }

   device->ws->buffer_unmap(image->bindings[0].bo);

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CopyImageToImageEXT(VkDevice _device, const VkCopyImageToImageInfoEXT *pCopyImageToImageInfo)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_image, src, pCopyImageToImageInfo->srcImage);
   RADV_FROM_HANDLE(radv_image, dst, pCopyImageToImageInfo->dstImage);

   assert(src->plane_count == 1);
   assert(dst->plane_count == 1);

   const uint8_t *src_ptr = device->ws->buffer_map(src->bindings[0].bo);
   src_ptr += src->bindings[0].offset;

   uint8_t *dst_ptr = device->ws->buffer_map(dst->bindings[0].bo);
   dst_ptr += dst->bindings[0].offset;

   for (uint32_t i = 0; i < pCopyImageToImageInfo->regionCount; i++) {
      const VkImageCopy2 *region = &pCopyImageToImageInfo->pRegions[i];

      assert(!radv_dcc_enabled(src, region->srcSubresource.mipLevel));
      assert(!radv_dcc_enabled(dst, region->dstSubresource.mipLevel));

      uint32_t src_layer_count = vk_image_subresource_layer_count(&src->vk, &region->srcSubresource);
      uint32_t dst_layer_count = vk_image_subresource_layer_count(&dst->vk, &region->dstSubresource);

      uint32_t layer_count = MIN2(src_layer_count, dst_layer_count);
      for (uint32_t layer = 0; layer < layer_count; layer++)
         radv_copy_image_rect(dst, dst_ptr, src, src_ptr, region, layer);
   }

   device->ws->buffer_unmap(src->bindings[0].bo);
   device->ws->buffer_unmap(dst->bindings[0].bo);

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_TransitionImageLayoutEXT(VkDevice _device, uint32_t transitionCount,
                              const VkHostImageLayoutTransitionInfoEXT *pTransitions)
{
   return VK_SUCCESS;
}
