/*
 * Copyright © 2017 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "wsi_common_cmd.h"

#include "vk_util.h"
#include "wsi_common_private.h"

VkResult
wsi_create_image_cmd_buffers(const struct wsi_swapchain *chain,
                             struct wsi_image *image,
                             const struct VkImageCreateInfo *image_info,
                             uint32_t present_blit_buffer_width)
{
   const struct wsi_device *wsi = chain->wsi;
   VkResult result;

   image->prime.blit_cmd_buffers = vk_zalloc(
      &chain->alloc, sizeof(VkCommandBuffer) * wsi->queue_family_count, 8,
      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!image->prime.blit_cmd_buffers)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   for (uint32_t i = 0; i < wsi->queue_family_count; i++) {
      const VkCommandBufferAllocateInfo cmd_buffer_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .pNext = NULL,
         .commandPool = chain->cmd_pools[i],
         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
         .commandBufferCount = 1,
      };
      result = wsi->AllocateCommandBuffers(chain->device, &cmd_buffer_info,
                                           &image->prime.blit_cmd_buffers[i]);
      if (result != VK_SUCCESS)
         break;

      const VkCommandBufferBeginInfo begin_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      };
      wsi->BeginCommandBuffer(image->prime.blit_cmd_buffers[i], &begin_info);

      struct VkBufferImageCopy buffer_image_copy = {
         .bufferOffset = 0,
         .bufferRowLength = present_blit_buffer_width,
         .bufferImageHeight = 0,
         .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
         },
         .imageOffset = { .x = 0, .y = 0, .z = 0 },
         .imageExtent = image_info->extent,
      };
      wsi->CmdCopyImageToBuffer(image->prime.blit_cmd_buffers[i],
                                image->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                image->prime.buffer, 1, &buffer_image_copy);

      result = wsi->EndCommandBuffer(image->prime.blit_cmd_buffers[i]);
      if (result != VK_SUCCESS)
         break;
   }

   return result;
}

void
wsi_destroy_image_cmd_buffers(const struct wsi_swapchain *chain,
                              struct wsi_image *image)
{
   const struct wsi_device *wsi = chain->wsi;

   if (!image->prime.blit_cmd_buffers)
      return;

   for (uint32_t i = 0; i < wsi->queue_family_count; i++) {
      wsi->FreeCommandBuffers(chain->device, chain->cmd_pools[i], 1,
                              &image->prime.blit_cmd_buffers[i]);
   }

   vk_free(&chain->alloc, image->prime.blit_cmd_buffers);
}
