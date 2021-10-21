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

VkResult
dzn_CreateRenderPass2(VkDevice _device,
                      const VkRenderPassCreateInfo2KHR *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkRenderPass *pRenderPass)
{
   VK_FROM_HANDLE(dzn_device, device, _device);

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR);

   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, dzn_render_pass, pass, 1);
   VK_MULTIALLOC_DECL(&ma, dzn_subpass, subpasses,
                           pCreateInfo->subpassCount);
   VK_MULTIALLOC_DECL(&ma, dzn_attachment, attachments,
                           pCreateInfo->attachmentCount);
#if 0
   VK_MULTIALLOC_DECL(&ma, enum dzn_pipe_bits, subpass_flushes,
                           pCreateInfo->subpassCount + 1);
#endif

   uint32_t subpass_attachment_count = 0;
   for (uint32_t i = 0; i < pCreateInfo->subpassCount; i++) {
      subpass_attachment_count +=
         num_subpass_attachments2(&pCreateInfo->pSubpasses[i]);
   }
#if 0
   VK_MULTIALLOC_DECL(&ma, dzn_subpass_attachment, subpass_attachments,
                      subpass_attachment_count);
#endif

   if (!vk_object_multizalloc(&device->vk, &ma, pAllocator,
                              VK_OBJECT_TYPE_RENDER_PASS))
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   pass->subpass_count = pCreateInfo->subpassCount;
   pass->subpasses = subpasses;
   for (uint32_t i = 0; i < pass->subpass_count; i++) {
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

   pass->attachment_count = pCreateInfo->attachmentCount;
   pass->attachments = attachments;
   for (uint32_t i = 0; i < pass->attachment_count; i++) {
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

   *pRenderPass = dzn_render_pass_to_handle(pass);

   return VK_SUCCESS;
}

void
dzn_DestroyRenderPass(VkDevice _device,
                      VkRenderPass _pass,
                      const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_render_pass, pass, _pass);

   if (!pass)
      return;

   vk_object_free(&device->vk, pAllocator, pass);
}
