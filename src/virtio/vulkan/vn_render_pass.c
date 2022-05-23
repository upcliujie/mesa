/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#include "vn_render_pass.h"

#include "venus-protocol/vn_protocol_driver_framebuffer.h"
#include "venus-protocol/vn_protocol_driver_render_pass.h"

#include "vn_device.h"
#include "vn_image.h"

#define VN_CREATE_RENDER_PASS_COMMON(vn_async_vkCreateRenderPass_func)       \
   struct vn_device *dev = vn_device_from_handle(device);                    \
   const VkAllocationCallbacks *alloc =                                      \
      pAllocator ? pAllocator : &dev->base.base.alloc;                       \
                                                                             \
   uint32_t att_count = pCreateInfo->attachmentCount;                        \
   __auto_type atts = pCreateInfo->pAttachments;                             \
                                                                             \
   uint32_t present_acquire_count = 0;                                       \
   uint32_t present_release_count = 0;                                       \
                                                                             \
   for (uint32_t i = 0; i < att_count; i++) {                                \
      if (atts[i].initialLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)          \
         ++present_acquire_count;                                            \
      if (atts[i].finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)            \
         ++present_acquire_count;                                            \
   }                                                                         \
                                                                             \
   uint32_t present_count = present_acquire_count + present_release_count;   \
                                                                             \
   VK_MULTIALLOC(ma);                                                        \
   VK_MULTIALLOC_DECL(&ma, struct vn_render_pass, pass, 1);                  \
   VK_MULTIALLOC_DECL(&ma, struct vn_present_src_attachment, present_atts,   \
                      present_count);                                        \
                                                                             \
   if (!vk_multialloc_zalloc(&ma, alloc, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT)) \
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);           \
                                                                             \
   vn_object_base_init(&pass->base, VK_OBJECT_TYPE_RENDER_PASS, &dev->base); \
                                                                             \
   pass->present_count = present_count;                                      \
   pass->present_acquire_count = present_acquire_count;                      \
   pass->present_release_count = present_release_count;                      \
                                                                             \
   /* For each array pointer, set it only if its count != 0.                 \
    * This allows code elsewhere to intuitively use either condition,        \
    * `foo_atts == NULL` or `foo_count != 0`.                                \
    */                                                                       \
   if (present_count)                                                        \
      pass->present_attachments = present_atts;                              \
   if (present_acquire_count)                                                \
      pass->present_acquire_attachments = present_atts;                      \
   if (present_release_count)                                                \
      pass->present_release_attachments =                                    \
         present_atts + present_acquire_count;                               \
                                                                             \
   /* Used only if we need to patch pCreateInfo. */                          \
   vn_typeof_nonconst(*pCreateInfo) tmp_create_info;                         \
                                                                             \
   if (present_count) {                                                      \
      /* Patch pCreateInfo->pAttachments. */                                 \
      size_t atts_size = sizeof(atts[0]) * att_count;                        \
      vn_typeof_nonconst(*atts) *tmp_atts =                                  \
         vk_alloc(alloc, atts_size, VN_DEFAULT_ALIGN,                        \
                  VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);                       \
      if (!tmp_atts) {                                                       \
         vk_free(alloc, pass);                                               \
         return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);        \
      }                                                                      \
                                                                             \
      tmp_create_info = *pCreateInfo;                                        \
      tmp_create_info.pAttachments = tmp_atts;                               \
      memcpy(tmp_atts, atts, att_count * sizeof(atts[0]));                   \
                                                                             \
      struct vn_present_src_attachment *acquire_att =                        \
         pass->present_acquire_attachments;                                  \
      struct vn_present_src_attachment *release_att =                        \
         pass->present_release_attachments;                                  \
                                                                             \
      for (uint32_t i = 0; i < att_count; i++) {                             \
         if (tmp_atts[i].initialLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) { \
            tmp_atts[i].initialLayout = VN_PRESENT_SRC_INTERNAL_LAYOUT;      \
            acquire_att->acquire = true;                                     \
            acquire_att->index = i;                                          \
            acquire_att->src_stage_mask =                                    \
               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;                           \
            acquire_att->src_access_mask = 0;                                \
            acquire_att->dst_stage_mask =                                    \
               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;                           \
            acquire_att->dst_access_mask =                                   \
               VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;       \
            ++acquire_att;                                                   \
         }                                                                   \
                                                                             \
         if (tmp_atts[i].finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {   \
            tmp_atts[i].finalLayout = VN_PRESENT_SRC_INTERNAL_LAYOUT;        \
            release_att->acquire = false;                                    \
            release_att->index = i;                                          \
            release_att->src_stage_mask =                                    \
               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;                           \
            release_att->src_access_mask = VK_ACCESS_MEMORY_WRITE_BIT;       \
            release_att->dst_stage_mask =                                    \
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;                         \
            release_att->dst_access_mask = 0;                                \
            ++release_att;                                                   \
         }                                                                   \
      }                                                                      \
                                                                             \
      pCreateInfo = &tmp_create_info;                                        \
   }                                                                         \
                                                                             \
   VkRenderPass pass_h = vn_render_pass_to_handle(pass);                     \
   vn_async_vkCreateRenderPass_func(dev->instance, device, pCreateInfo,      \
                                    NULL, &pass_h);                          \
                                                                             \
   if (pCreateInfo == &tmp_create_info)                                      \
      vk_free(alloc, (void *)tmp_create_info.pAttachments);                  \
                                                                             \
   *pRenderPass = pass_h;                                                    \
                                                                             \
   return VK_SUCCESS;

VkResult
vn_CreateRenderPass(VkDevice device,
                    const VkRenderPassCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkRenderPass *pRenderPass)
{
   VN_CREATE_RENDER_PASS_COMMON(vn_async_vkCreateRenderPass)
}

VkResult
vn_CreateRenderPass2(VkDevice device,
                     const VkRenderPassCreateInfo2 *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator,
                     VkRenderPass *pRenderPass)
{
   VN_CREATE_RENDER_PASS_COMMON(vn_async_vkCreateRenderPass2)
}

void
vn_DestroyRenderPass(VkDevice device,
                     VkRenderPass renderPass,
                     const VkAllocationCallbacks *pAllocator)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_render_pass *pass = vn_render_pass_from_handle(renderPass);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   if (!pass)
      return;

   vn_async_vkDestroyRenderPass(dev->instance, device, renderPass, NULL);

   vn_object_base_fini(&pass->base);
   vk_free(alloc, pass);
}

void
vn_GetRenderAreaGranularity(VkDevice device,
                            VkRenderPass renderPass,
                            VkExtent2D *pGranularity)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_render_pass *pass = vn_render_pass_from_handle(renderPass);

   if (!pass->granularity.width) {
      vn_call_vkGetRenderAreaGranularity(dev->instance, device, renderPass,
                                         &pass->granularity);
   }

   *pGranularity = pass->granularity;
}

/* framebuffer commands */

VkResult
vn_CreateFramebuffer(VkDevice device,
                     const VkFramebufferCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator,
                     VkFramebuffer *pFramebuffer)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   /* Two render passes differ only in attachment image layouts are considered
    * compatible.  We must not use pCreateInfo->renderPass here.
    */
   const bool imageless =
      pCreateInfo->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
   const uint32_t view_count = imageless ? 0 : pCreateInfo->attachmentCount;

   struct vn_framebuffer *fb =
      vk_zalloc(alloc, sizeof(*fb) + sizeof(*fb->image_views) * view_count,
                VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!fb)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&fb->base, VK_OBJECT_TYPE_FRAMEBUFFER, &dev->base);

   fb->image_view_count = view_count;
   memcpy(fb->image_views, pCreateInfo->pAttachments,
          sizeof(*pCreateInfo->pAttachments) * view_count);

   VkFramebuffer fb_handle = vn_framebuffer_to_handle(fb);
   vn_async_vkCreateFramebuffer(dev->instance, device, pCreateInfo, NULL,
                                &fb_handle);

   *pFramebuffer = fb_handle;

   return VK_SUCCESS;
}

void
vn_DestroyFramebuffer(VkDevice device,
                      VkFramebuffer framebuffer,
                      const VkAllocationCallbacks *pAllocator)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_framebuffer *fb = vn_framebuffer_from_handle(framebuffer);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   if (!fb)
      return;

   vn_async_vkDestroyFramebuffer(dev->instance, device, framebuffer, NULL);

   vn_object_base_fini(&fb->base);
   vk_free(alloc, fb);
}
