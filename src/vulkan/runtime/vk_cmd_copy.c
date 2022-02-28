/*
 * Copyright © 2021 Intel Corporation
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

#include "vk_command_buffer.h"
#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_util.h"

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdCopyBuffer(VkCommandBuffer commandBuffer,
                        VkBuffer srcBuffer,
                        VkBuffer dstBuffer,
                        uint32_t regionCount,
                        const VkBufferCopy *pRegions)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   const struct vk_cmd_dispatch_table *disp = cmd_buffer->dispatch_table;

   STACK_ARRAY(VkBufferCopy2KHR, region2s, regionCount);

   for (uint32_t r = 0; r < regionCount; r++) {
      region2s[r] = (VkBufferCopy2KHR) {
         .sType      = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR,
         .srcOffset  = pRegions[r].srcOffset,
         .dstOffset  = pRegions[r].dstOffset,
         .size       = pRegions[r].size,
      };
   }

   VkCopyBufferInfo2KHR info = {
      .sType         = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR,
      .srcBuffer     = srcBuffer,
      .dstBuffer     = dstBuffer,
      .regionCount   = regionCount,
      .pRegions      = region2s,
   };

   disp->CmdCopyBuffer2KHR(commandBuffer, &info);

   STACK_ARRAY_FINISH(region2s);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdCopyImage(VkCommandBuffer commandBuffer,
                       VkImage srcImage,
                       VkImageLayout srcImageLayout,
                       VkImage dstImage,
                       VkImageLayout dstImageLayout,
                       uint32_t regionCount,
                       const VkImageCopy *pRegions)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   const struct vk_cmd_dispatch_table *disp = cmd_buffer->dispatch_table;

   STACK_ARRAY(VkImageCopy2KHR, region2s, regionCount);

   for (uint32_t r = 0; r < regionCount; r++) {
      region2s[r] = (VkImageCopy2KHR) {
         .sType            = VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR,
         .srcSubresource   = pRegions[r].srcSubresource,
         .srcOffset        = pRegions[r].srcOffset,
         .dstSubresource   = pRegions[r].dstSubresource,
         .dstOffset        = pRegions[r].dstOffset,
         .extent           = pRegions[r].extent,
      };
   }

   VkCopyImageInfo2KHR info = {
      .sType            = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR,
      .srcImage         = srcImage,
      .srcImageLayout   = srcImageLayout,
      .dstImage         = dstImage,
      .dstImageLayout   = dstImageLayout,
      .regionCount      = regionCount,
      .pRegions         = region2s,
   };

   disp->CmdCopyImage2KHR(commandBuffer, &info);

   STACK_ARRAY_FINISH(region2s);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdCopyBufferToImage(VkCommandBuffer commandBuffer,
                               VkBuffer srcBuffer,
                               VkImage dstImage,
                               VkImageLayout dstImageLayout,
                               uint32_t regionCount,
                               const VkBufferImageCopy *pRegions)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   const struct vk_cmd_dispatch_table *disp = cmd_buffer->dispatch_table;

   STACK_ARRAY(VkBufferImageCopy2KHR, region2s, regionCount);

   for (uint32_t r = 0; r < regionCount; r++) {
      region2s[r] = (VkBufferImageCopy2KHR) {
         .sType               = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR,
         .bufferOffset        = pRegions[r].bufferOffset,
         .bufferRowLength     = pRegions[r].bufferRowLength,
         .bufferImageHeight   = pRegions[r].bufferImageHeight,
         .imageSubresource    = pRegions[r].imageSubresource,
         .imageOffset         = pRegions[r].imageOffset,
         .imageExtent         = pRegions[r].imageExtent,
      };
   }

   VkCopyBufferToImageInfo2KHR info = {
      .sType            = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,
      .srcBuffer        = srcBuffer,
      .dstImage         = dstImage,
      .dstImageLayout   = dstImageLayout,
      .regionCount      = regionCount,
      .pRegions         = region2s,
   };

   disp->CmdCopyBufferToImage2KHR(commandBuffer, &info);

   STACK_ARRAY_FINISH(region2s);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdCopyImageToBuffer(VkCommandBuffer commandBuffer,
                               VkImage srcImage,
                               VkImageLayout srcImageLayout,
                               VkBuffer dstBuffer,
                               uint32_t regionCount,
                               const VkBufferImageCopy *pRegions)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   const struct vk_cmd_dispatch_table *disp = cmd_buffer->dispatch_table;

   STACK_ARRAY(VkBufferImageCopy2KHR, region2s, regionCount);

   for (uint32_t r = 0; r < regionCount; r++) {
      region2s[r] = (VkBufferImageCopy2KHR) {
         .sType               = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR,
         .bufferOffset        = pRegions[r].bufferOffset,
         .bufferRowLength     = pRegions[r].bufferRowLength,
         .bufferImageHeight   = pRegions[r].bufferImageHeight,
         .imageSubresource    = pRegions[r].imageSubresource,
         .imageOffset         = pRegions[r].imageOffset,
         .imageExtent         = pRegions[r].imageExtent,
      };
   }

   VkCopyImageToBufferInfo2KHR info = {
      .sType            = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR,
      .srcImage         = srcImage,
      .srcImageLayout   = srcImageLayout,
      .dstBuffer        = dstBuffer,
      .regionCount      = regionCount,
      .pRegions         = region2s,
   };

   disp->CmdCopyImageToBuffer2KHR(commandBuffer, &info);

   STACK_ARRAY_FINISH(region2s);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdBlitImage(VkCommandBuffer commandBuffer,
                       VkImage srcImage,
                       VkImageLayout srcImageLayout,
                       VkImage dstImage,
                       VkImageLayout dstImageLayout,
                       uint32_t regionCount,
                       const VkImageBlit *pRegions,
                       VkFilter filter)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   const struct vk_cmd_dispatch_table *disp = cmd_buffer->dispatch_table;

   STACK_ARRAY(VkImageBlit2KHR, region2s, regionCount);

   for (uint32_t r = 0; r < regionCount; r++) {
      region2s[r] = (VkImageBlit2KHR) {
         .sType            = VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR,
         .srcSubresource   = pRegions[r].srcSubresource,
         .srcOffsets       = {
            pRegions[r].srcOffsets[0],
            pRegions[r].srcOffsets[1],
         },
         .dstSubresource   = pRegions[r].dstSubresource,
         .dstOffsets       = {
            pRegions[r].dstOffsets[0],
            pRegions[r].dstOffsets[1],
         },
      };
   }

   VkBlitImageInfo2KHR info = {
      .sType            = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR,
      .srcImage         = srcImage,
      .srcImageLayout   = srcImageLayout,
      .dstImage         = dstImage,
      .dstImageLayout   = dstImageLayout,
      .regionCount      = regionCount,
      .pRegions         = region2s,
      .filter           = filter,
   };

   disp->CmdBlitImage2KHR(commandBuffer, &info);

   STACK_ARRAY_FINISH(region2s);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdResolveImage(VkCommandBuffer commandBuffer,
                          VkImage srcImage,
                          VkImageLayout srcImageLayout,
                          VkImage dstImage,
                          VkImageLayout dstImageLayout,
                          uint32_t regionCount,
                          const VkImageResolve *pRegions)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   const struct vk_cmd_dispatch_table *disp = cmd_buffer->dispatch_table;

   STACK_ARRAY(VkImageResolve2KHR, region2s, regionCount);

   for (uint32_t r = 0; r < regionCount; r++) {
      region2s[r] = (VkImageResolve2KHR) {
         .sType            = VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2_KHR,
         .srcSubresource   = pRegions[r].srcSubresource,
         .srcOffset        = pRegions[r].srcOffset,
         .dstSubresource   = pRegions[r].dstSubresource,
         .dstOffset        = pRegions[r].dstOffset,
         .extent           = pRegions[r].extent,
      };
   }

   VkResolveImageInfo2KHR info = {
      .sType            = VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2_KHR,
      .srcImage         = srcImage,
      .srcImageLayout   = srcImageLayout,
      .dstImage         = dstImage,
      .dstImageLayout   = dstImageLayout,
      .regionCount      = regionCount,
      .pRegions         = region2s,
   };

   disp->CmdResolveImage2KHR(commandBuffer, &info);

   STACK_ARRAY_FINISH(region2s);
}
