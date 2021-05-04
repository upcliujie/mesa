/*
 * Copyright © 2017 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "wsi_common_cmd.h"

#include "vk_util.h"
#include "wsi_common_private.h"

struct wsi_cmd_builder {
   const struct wsi_swapchain *chain;
   const struct wsi_image *image;
   const struct VkImageCreateInfo *image_info;

   uint32_t present_blit_buffer_width;

   VkCommandBuffer *cmd_buffers;
};

static void
wsi_cmd_free_cmd_buffers(const struct wsi_swapchain *chain,
                         VkCommandBuffer *cmd_buffers,
                         uint32_t queue_family_count)
{
   const struct wsi_device *wsi = chain->wsi;

   for (uint32_t i = 0; i < queue_family_count; i++) {
      wsi->FreeCommandBuffers(chain->device, chain->cmd_pools[i], 1,
                              &cmd_buffers[i]);
   }
   vk_free(&chain->alloc, cmd_buffers);
}

static void
wsi_cmd_builder_present_blit(struct wsi_cmd_builder *builder,
                             uint32_t queue_family_index)
{
   const struct wsi_device *wsi = builder->chain->wsi;
   VkImage image = builder->image->image;
   VkBuffer buffer = builder->image->prime.buffer;
   VkCommandBuffer cmd_buffer = builder->cmd_buffers[queue_family_index];

   const struct VkBufferImageCopy buffer_image_copy = {
      .bufferOffset = 0,
      .bufferRowLength = builder->present_blit_buffer_width,
      .bufferImageHeight = 0,
      .imageSubresource = {
         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
         .mipLevel = 0,
         .baseArrayLayer = 0,
         .layerCount = 1,
      },
      .imageOffset = { .x = 0, .y = 0, .z = 0 },
      .imageExtent = builder->image_info->extent,
   };

   /* PRESENT_SRC is the layout although it is not really valid with
    * vkCmdCopyImageToBuffer
    */
   wsi->CmdCopyImageToBuffer(cmd_buffer, image,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, buffer, 1,
                             &buffer_image_copy);
}

static VkResult
wsi_cmd_builder_alloc(struct wsi_cmd_builder *builder)
{
   const struct wsi_swapchain *chain = builder->chain;
   const struct wsi_device *wsi = chain->wsi;

   VkCommandBuffer *cmd_buffers = vk_zalloc(
      &chain->alloc, sizeof(VkCommandBuffer) * wsi->queue_family_count, 8,
      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!cmd_buffers)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   for (uint32_t i = 0; i < wsi->queue_family_count; i++) {
      const VkCommandBufferAllocateInfo cmd_buffer_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .pNext = NULL,
         .commandPool = chain->cmd_pools[i],
         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
         .commandBufferCount = 1,
      };

      VkResult result = wsi->AllocateCommandBuffers(
         chain->device, &cmd_buffer_info, &cmd_buffers[i]);
      if (result != VK_SUCCESS) {
         wsi_cmd_free_cmd_buffers(chain, cmd_buffers, i);
         return result;
      }
   }

   builder->cmd_buffers = cmd_buffers;

   return VK_SUCCESS;
}

VkResult
wsi_create_image_cmd_buffers(const struct wsi_swapchain *chain,
                             struct wsi_image *image,
                             const struct VkImageCreateInfo *image_info,
                             uint32_t present_blit_buffer_width)
{
   const struct wsi_device *wsi = chain->wsi;
   struct wsi_cmd_builder builder = {
      .chain = chain,
      .image = image,
      .image_info = image_info,
      .present_blit_buffer_width = present_blit_buffer_width,
   };
   VkResult result;

   result = wsi_cmd_builder_alloc(&builder);
   if (result != VK_SUCCESS)
      return result;

   /* take the ownership */
   image->prime.blit_cmd_buffers = builder.cmd_buffers;

   for (uint32_t i = 0; i < wsi->queue_family_count; i++) {
      const VkCommandBufferBeginInfo begin_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      };
      wsi->BeginCommandBuffer(image->prime.blit_cmd_buffers[i], &begin_info);

      wsi_cmd_builder_present_blit(&builder, i);

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

   wsi_cmd_free_cmd_buffers(chain, image->prime.blit_cmd_buffers,
                            wsi->queue_family_count);
}
