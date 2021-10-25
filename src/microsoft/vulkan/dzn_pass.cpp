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
#include "vk_format.h"

static unsigned
num_subpass_attachments2(const VkSubpassDescription2KHR *desc)
{
   return desc->inputAttachmentCount +
          desc->colorAttachmentCount +
          (desc->pResolveAttachments ? desc->colorAttachmentCount : 0) +
          (desc->pDepthStencilAttachment != NULL);
}

dzn_render_pass::dzn_render_pass(dzn_device *device,
                                 const VkRenderPassCreateInfo2KHR *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator)
{
   subpass_count = pCreateInfo->subpassCount;
   assert(subpasses);
   for (uint32_t i = 0; i < subpass_count; i++) {
      const VkSubpassDescription2 *subpass = &pCreateInfo->pSubpasses[i];

      subpasses[i].color_count = subpass->colorAttachmentCount;
      for (uint32_t j = 0; j < subpasses[i].color_count; j++) {
         subpasses[i].colors[j].idx = subpass->pColorAttachments[j].attachment;
      }

      subpasses[i].zs.idx = VK_ATTACHMENT_UNUSED;
      if (subpass->pDepthStencilAttachment) {
         subpasses[i].zs.idx = subpass->pDepthStencilAttachment->attachment;
      }
   }

   attachment_count = pCreateInfo->attachmentCount;
   assert(!attachment_count || attachments);
   for (uint32_t i = 0; i < attachment_count; i++) {
      const VkAttachmentDescription2 *attachment = &pCreateInfo->pAttachments[i];

      attachments[i].format = dzn_get_format(attachment->format);
      assert(attachments[i].format);
      if (vk_format_is_depth_or_stencil(attachment->format)) {
         attachments[i].during = D3D12_RESOURCE_STATE_DEPTH_WRITE;
         attachments[i].clear.depth =
            attachment->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR;
         attachments[i].clear.stencil =
            attachment->stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR;
      } else {
         attachments[i].during = D3D12_RESOURCE_STATE_RENDER_TARGET;
         attachments[i].clear.color =
            attachment->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR;
      }
      attachments[i].samples = attachment->samples;
      attachments[i].before = dzn_get_states(attachment->initialLayout);
      attachments[i].after = dzn_get_states(attachment->finalLayout);
   }

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_RENDER_PASS);
}

dzn_render_pass::~dzn_render_pass()
{
   vk_object_base_finish(&base);
}

dzn_render_pass *
dzn_render_pass_factory::allocate(dzn_device *device,
                                  const VkRenderPassCreateInfo2KHR *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR);

   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, dzn_render_pass, pass, 1);
   VK_MULTIALLOC_DECL(&ma, dzn_subpass, subpasses,
                           pCreateInfo->subpassCount);
   VK_MULTIALLOC_DECL(&ma, dzn_attachment, attachments,
                           pCreateInfo->attachmentCount);

   if (!vk_multialloc_zalloc2(&ma, &device->vk.alloc, pAllocator,
                              VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   pass->subpasses = subpasses;
   pass->attachments = attachments;
   return pass;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateRenderPass2(VkDevice device,
                      const VkRenderPassCreateInfo2KHR *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkRenderPass *pRenderPass)
{
   return dzn_render_pass_factory::create(device, pCreateInfo,
                                          pAllocator, pRenderPass);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyRenderPass(VkDevice device,
                      VkRenderPass pass,
                      const VkAllocationCallbacks *pAllocator)
{
   dzn_render_pass_factory::destroy(device, pass, pAllocator);
}
