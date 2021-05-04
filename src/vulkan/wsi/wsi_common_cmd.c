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

   bool need_cmd_types[WSI_CMD_TYPE_COUNT];
   uint32_t need_cmd_type_count;

   VkCommandBuffer (*cmd_buffers)[WSI_CMD_TYPE_COUNT];
};

static void
wsi_cmd_free_cmd_buffers(const struct wsi_swapchain *chain,
                         VkCommandBuffer (*cmd_buffers)[WSI_CMD_TYPE_COUNT],
                         uint32_t queue_family_count)
{
   const struct wsi_device *wsi = chain->wsi;

   for (uint32_t i = 0; i < queue_family_count; i++) {
      /* some elements of cmd_buffers[i] array may be NULL */
      wsi->FreeCommandBuffers(chain->device, chain->cmd_pools[i],
                              WSI_CMD_TYPE_COUNT, cmd_buffers[i]);
   }
   vk_free(&chain->alloc, cmd_buffers);
}

static void
wsi_cmd_builder_ownership_transfer(struct wsi_cmd_builder *builder,
                                   uint32_t queue_family_index,
                                   enum wsi_cmd_type type)
{
   const struct wsi_swapchain *chain = builder->chain;
   const struct wsi_device *wsi = chain->wsi;
   const bool exclusive =
      chain->use_prime_blit ||
      builder->image_info->sharingMode == VK_SHARING_MODE_EXCLUSIVE;
   VkCommandBuffer cmd_buffer =
      builder->cmd_buffers[queue_family_index][type];

   uint32_t src_index;
   uint32_t dst_index;
   if (type == WSI_CMD_TYPE_ACQUIRE) {
      src_index = VK_QUEUE_FAMILY_FOREIGN_EXT;
      dst_index = exclusive ? queue_family_index : VK_QUEUE_FAMILY_IGNORED;
   } else {
      src_index = exclusive ? queue_family_index : VK_QUEUE_FAMILY_IGNORED;
      dst_index = VK_QUEUE_FAMILY_FOREIGN_EXT;
   }

   VkBufferMemoryBarrier buffer_barrier;
   VkImageMemoryBarrier image_barrier;
   if (chain->use_prime_blit) {
      buffer_barrier = (const VkBufferMemoryBarrier){
         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .srcQueueFamilyIndex = src_index,
         .dstQueueFamilyIndex = dst_index,
         .buffer = builder->image->prime.buffer,
         .size = VK_WHOLE_SIZE,
      };
   } else {
      image_barrier = (const VkImageMemoryBarrier){
         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
         .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         .srcQueueFamilyIndex = src_index,
         .dstQueueFamilyIndex = dst_index,
         .image = builder->image->image,
         .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
          },
      };
   }

   wsi->CmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL,
                           chain->use_prime_blit, &buffer_barrier,
                           !chain->use_prime_blit, &image_barrier);
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

      if (!builder->need_cmd_types[type])
         continue;

      const VkCommandBufferBeginInfo begin_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      };
      result = wsi->BeginCommandBuffer(cmd_buffer, &begin_info);
      if (result != VK_SUCCESS)
         return result;

      if (chain->use_prime_blit && type == WSI_CMD_TYPE_PRESENT)
         wsi_cmd_builder_present_blit(builder, queue_family_index);

      if (wsi->ownership_transfer) {
         wsi_cmd_builder_ownership_transfer(builder, queue_family_index,
                                            type);
      }

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
         .commandBufferCount = builder->need_cmd_type_count,
      };

      VkCommandBuffer tmp_cmd_buffers[WSI_CMD_TYPE_COUNT];
      VkResult result = wsi->AllocateCommandBuffers(
         chain->device, &cmd_buffer_info, tmp_cmd_buffers);
      if (result != VK_SUCCESS) {
         wsi_cmd_free_cmd_buffers(chain, cmd_buffers, i);
         return result;
      }

      uint32_t tmp_assigned = 0;
      for (uint32_t j = 0; j < WSI_CMD_TYPE_COUNT; j++) {
         if (builder->need_cmd_types[j])
            cmd_buffers[i][j] = tmp_cmd_buffers[tmp_assigned++];
      }
   }

   builder->cmd_buffers = cmd_buffers;

   return VK_SUCCESS;
}

static void
wsi_cmd_builder_init_needs(struct wsi_cmd_builder *builder)
{
   const struct wsi_swapchain *chain = builder->chain;
   const struct wsi_device *wsi = chain->wsi;

   if (chain->use_prime_blit)
      builder->need_cmd_types[WSI_CMD_TYPE_PRESENT] = true;

   if (wsi->ownership_transfer) {
      builder->need_cmd_types[WSI_CMD_TYPE_ACQUIRE] = true;
      builder->need_cmd_types[WSI_CMD_TYPE_PRESENT] = true;
   }

   for (uint32_t i = 0; i < WSI_CMD_TYPE_COUNT; i++)
      builder->need_cmd_type_count += builder->need_cmd_types[i];
}

static VkResult
wsi_cmd_builder_build(struct wsi_cmd_builder *builder)
{
   const struct wsi_swapchain *chain = builder->chain;
   const struct wsi_device *wsi = chain->wsi;
   VkResult result;

   wsi_cmd_builder_init_needs(builder);
   if (!builder->need_cmd_type_count)
      return VK_SUCCESS;

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
