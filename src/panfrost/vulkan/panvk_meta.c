/*
 * Copyright © 2021 Collabora Ltd.
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

#include "nir/nir_builder.h"
#include "pan_blitter.h"
#include "pan_encoder.h"

#include "panvk_private.h"
#include "panvk_meta.h"

#include "vk_format.h"

void
panvk_meta_close_batch(struct panvk_cmd_buffer *cmdbuf)
{
   const struct panfrost_device *pdev =
      &cmdbuf->device->physical_device->pdev;
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (!pan_is_bifrost(pdev) && batch->scoreboard.first_tiler) {
      mali_ptr polygon_list =
         batch->tiler.ctx.midgard.polygon_list->ptr.gpu;
      struct panfrost_ptr writeval_job =
         panfrost_scoreboard_initialize_tiler(&cmdbuf->desc_pool.base,
                                              &batch->scoreboard,
                                              polygon_list);
      if (writeval_job.cpu)
         util_dynarray_append(&batch->jobs, void *, writeval_job.cpu);

      memcpy(&batch->tiler.templ.midgard,
             pan_section_ptr(batch->fb.desc.cpu,
                             MULTI_TARGET_FRAMEBUFFER, TILER),
             sizeof(batch->tiler.templ.midgard));
   }

   list_addtail(&cmdbuf->state.batch->node, &cmdbuf->batches);
   cmdbuf->state.batch = NULL;
}

void
panvk_CmdBlitImage(VkCommandBuffer commandBuffer,
                   VkImage srcImage,
                   VkImageLayout srcImageLayout,
                   VkImage destImage,
                   VkImageLayout destImageLayout,
                   uint32_t regionCount,
                   const VkImageBlit *pRegions,
                   VkFilter filter)

{
   panvk_stub();
}

void
panvk_CmdCopyImage(VkCommandBuffer commandBuffer,
                   VkImage srcImage,
                   VkImageLayout srcImageLayout,
                   VkImage destImage,
                   VkImageLayout destImageLayout,
                   uint32_t regionCount,
                   const VkImageCopy *pRegions)
{
   panvk_stub();
}

void
panvk_CmdCopyBufferToImage(VkCommandBuffer commandBuffer,
                           VkBuffer srcBuffer,
                           VkImage destImage,
                           VkImageLayout destImageLayout,
                           uint32_t regionCount,
                           const VkBufferImageCopy *pRegions)
{
   panvk_stub();
}

void
panvk_CmdCopyImageToBuffer(VkCommandBuffer commandBuffer,
                           VkImage srcImage,
                           VkImageLayout srcImageLayout,
                           VkBuffer destBuffer,
                           uint32_t regionCount,
                           const VkBufferImageCopy *pRegions)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_buffer, buf, destBuffer);
   VK_FROM_HANDLE(panvk_image, img, srcImage);

   for (unsigned i = 0; i < regionCount; i++) {
      panvk_meta_copy_img2buf(cmdbuf, buf, img, &pRegions[i]);
   }
}

void
panvk_CmdCopyBuffer(VkCommandBuffer commandBuffer,
                    VkBuffer srcBuffer,
                    VkBuffer destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *pRegions)
{
   panvk_stub();
}

void
panvk_CmdResolveImage(VkCommandBuffer cmd_buffer_h,
                      VkImage src_image_h,
                      VkImageLayout src_image_layout,
                      VkImage dest_image_h,
                      VkImageLayout dest_image_layout,
                      uint32_t region_count,
                      const VkImageResolve *regions)
{
   panvk_stub();
}

void
panvk_CmdFillBuffer(VkCommandBuffer commandBuffer,
                    VkBuffer dstBuffer,
                    VkDeviceSize dstOffset,
                    VkDeviceSize fillSize,
                    uint32_t data)
{
   panvk_stub();
}

void
panvk_CmdUpdateBuffer(VkCommandBuffer commandBuffer,
                      VkBuffer dstBuffer,
                      VkDeviceSize dstOffset,
                      VkDeviceSize dataSize,
                      const void *pData)
{
   panvk_stub();
}

void
panvk_CmdClearColorImage(VkCommandBuffer commandBuffer,
                         VkImage image,
                         VkImageLayout imageLayout,
                         const VkClearColorValue *pColor,
                         uint32_t rangeCount,
                         const VkImageSubresourceRange *pRanges)
{
   panvk_stub();
}

void
panvk_CmdClearDepthStencilImage(VkCommandBuffer commandBuffer,
                                VkImage image_h,
                                VkImageLayout imageLayout,
                                const VkClearDepthStencilValue *pDepthStencil,
                                uint32_t rangeCount,
                                const VkImageSubresourceRange *pRanges)
{
   panvk_stub();
}

void
panvk_CmdClearAttachments(VkCommandBuffer commandBuffer,
                          uint32_t attachmentCount,
                          const VkClearAttachment *pAttachments,
                          uint32_t rectCount,
                          const VkClearRect *pRects)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   const struct panvk_subpass *subpass = cmdbuf->state.subpass;

   for (unsigned i = 0; i < attachmentCount; i++) {
      for (unsigned j = 0; j < rectCount; j++) {

         uint32_t attachment;
         if (pAttachments[i].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
            unsigned idx = pAttachments[i].colorAttachment;
            attachment = subpass->color_attachments[idx].idx;
         } else {
            attachment = subpass->zs_attachment.idx;
         }

         if (attachment == VK_ATTACHMENT_UNUSED)
               continue;

         panvk_meta_clear_attachment(cmdbuf, attachment,
                                     pAttachments[i].aspectMask,
                                     &pAttachments[i].clearValue,
                                     &pRects[j]);
      }
   }
}

void
panvk_meta_init(struct panvk_physical_device *dev)
{
   panvk_pool_init(&dev->meta.bin_pool, &dev->pdev, NULL, PAN_BO_EXECUTE,
                   16 * 1024, "panvk_meta binary pool", false);
   panvk_pool_init(&dev->meta.desc_pool, &dev->pdev, NULL, 0,
                   16 * 1024, "panvk_meta descriptor pool", false);
   panvk_pool_init(&dev->meta.blitter.bin_pool, &dev->pdev, NULL,
                   PAN_BO_EXECUTE, 16 * 1024,
                   "panvk_meta blitter binary pool", false);
   panvk_pool_init(&dev->meta.blitter.desc_pool, &dev->pdev, NULL,
                   0, 16 * 1024, "panvk_meta blitter descriptor pool",
                   false);
   pan_blitter_init(&dev->pdev, &dev->meta.blitter.bin_pool.base,
                    &dev->meta.blitter.desc_pool.base);
   panvk_meta_clear_attachment_init(dev);
   panvk_meta_copy_img2buf_init(dev);
}

void
panvk_meta_cleanup(struct panvk_physical_device *dev)
{
   pan_blitter_cleanup(&dev->pdev);
   panvk_pool_cleanup(&dev->meta.blitter.desc_pool);
   panvk_pool_cleanup(&dev->meta.blitter.bin_pool);
   panvk_pool_cleanup(&dev->meta.desc_pool);
   panvk_pool_cleanup(&dev->meta.bin_pool);
}
