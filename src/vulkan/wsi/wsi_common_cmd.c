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

   VkCommandBuffer (*cmd_buffers)[WSI_CMD_TYPE_COUNT];
};

static void
wsi_cmd_free_cmd_buffers(const struct wsi_swapchain *chain,
                         VkCommandBuffer (*cmd_buffers)[WSI_CMD_TYPE_COUNT],
                         uint32_t queue_family_count)
{
   const struct wsi_device *wsi = chain->wsi;

   for (uint32_t i = 0; i < queue_family_count; i++) {
      wsi->FreeCommandBuffers(chain->device, chain->cmd_pools[i],
                              WSI_CMD_TYPE_COUNT, cmd_buffers[i]);
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
   VkCommandBuffer cmd_buffer =
      builder->cmd_buffers[queue_family_index][WSI_CMD_TYPE_PRESENT];

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
wsi_cmd_builder_build_for_queue_family(struct wsi_cmd_builder *builder,
                                       uint32_t queue_family_index)
{
   const struct wsi_swapchain *chain = builder->chain;
   const struct wsi_device *wsi = chain->wsi;

   for (uint32_t type = 0; type < WSI_CMD_TYPE_COUNT; type++) {
      VkCommandBuffer cmd_buffer =
         builder->cmd_buffers[queue_family_index][type];
      VkResult result;

      const VkCommandBufferBeginInfo begin_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      };
      result = wsi->BeginCommandBuffer(cmd_buffer, &begin_info);
      if (result != VK_SUCCESS)
         return result;

      if (type == WSI_CMD_TYPE_PRESENT)
         wsi_cmd_builder_present_blit(builder, queue_family_index);

      result = wsi->EndCommandBuffer(cmd_buffer);
      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

static VkResult
wsi_cmd_builder_alloc(struct wsi_cmd_builder *builder)
{
   const struct wsi_swapchain *chain = builder->chain;
   const struct wsi_device *wsi = chain->wsi;

   VkCommandBuffer(*cmd_buffers)[WSI_CMD_TYPE_COUNT] = vk_zalloc(
      &chain->alloc,
      sizeof(VkCommandBuffer) * wsi->queue_family_count * WSI_CMD_TYPE_COUNT,
      8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!cmd_buffers)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   for (uint32_t i = 0; i < wsi->queue_family_count; i++) {
      const VkCommandBufferAllocateInfo cmd_buffer_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .pNext = NULL,
         .commandPool = chain->cmd_pools[i],
         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
         .commandBufferCount = WSI_CMD_TYPE_COUNT,
      };

      VkResult result = wsi->AllocateCommandBuffers(
         chain->device, &cmd_buffer_info, cmd_buffers[i]);
      if (result != VK_SUCCESS) {
         wsi_cmd_free_cmd_buffers(chain, cmd_buffers, i);
         return result;
      }
   }

   builder->cmd_buffers = cmd_buffers;

   return VK_SUCCESS;
}

static VkResult
wsi_cmd_builder_build(struct wsi_cmd_builder *builder)
{
   const struct wsi_swapchain *chain = builder->chain;
   const struct wsi_device *wsi = chain->wsi;
   VkResult result;

   result = wsi_cmd_builder_alloc(builder);
   if (result != VK_SUCCESS)
      return result;

   for (uint32_t i = 0; i < wsi->queue_family_count; i++) {
      result = wsi_cmd_builder_build_for_queue_family(builder, i);
      if (result != VK_SUCCESS)
         break;
   }

   if (result != VK_SUCCESS) {
      wsi_cmd_free_cmd_buffers(builder->chain, builder->cmd_buffers,
                               wsi->queue_family_count);
   }

   return result;
}

VkResult
wsi_create_image_cmd_buffers(const struct wsi_swapchain *chain,
                             struct wsi_image *image,
                             const struct VkImageCreateInfo *image_info,
                             uint32_t present_blit_buffer_width)
{
   struct wsi_cmd_builder builder = {
      .chain = chain,
      .image = image,
      .image_info = image_info,
      .present_blit_buffer_width = present_blit_buffer_width,
   };
   VkResult result;

   result = wsi_cmd_builder_build(&builder);
   if (result != VK_SUCCESS)
      return result;

   /* take the ownership */
   image->cmd_buffers = builder.cmd_buffers;

   return VK_SUCCESS;
}

void
wsi_destroy_image_cmd_buffers(const struct wsi_swapchain *chain,
                              struct wsi_image *image)
{
   const struct wsi_device *wsi = chain->wsi;

   if (!image->cmd_buffers)
      return;

   wsi_cmd_free_cmd_buffers(chain, image->cmd_buffers,
                            wsi->queue_family_count);
}
