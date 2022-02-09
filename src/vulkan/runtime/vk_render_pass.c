/*
 * Copyright © 2020 Valve Corporation
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

#include "vk_render_pass.h"

#include "vk_alloc.h"
#include "vk_command_buffer.h"
#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_format.h"
#include "vk_framebuffer.h"
#include "vk_image.h"
#include "vk_util.h"

#include "util/log.h"

static void
translate_references(VkAttachmentReference2 **reference_ptr,
                     uint32_t reference_count,
                     const VkAttachmentReference *reference,
                     const VkRenderPassCreateInfo *pass_info,
                     bool is_input_attachment)
{
   VkAttachmentReference2 *reference2 = *reference_ptr;
   *reference_ptr += reference_count;
   for (uint32_t i = 0; i < reference_count; i++) {
      reference2[i] = (VkAttachmentReference2) {
         .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
         .pNext = NULL,
         .attachment = reference[i].attachment,
         .layout = reference[i].layout,
      };

      if (is_input_attachment &&
          reference2[i].attachment != VK_ATTACHMENT_UNUSED) {
         assert(reference2[i].attachment < pass_info->attachmentCount);
         const VkAttachmentDescription *att =
            &pass_info->pAttachments[reference2[i].attachment];
         reference2[i].aspectMask = vk_format_aspects(att->format);
      }
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateRenderPass(VkDevice _device,
                           const VkRenderPassCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           VkRenderPass *pRenderPass)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   uint32_t reference_count = 0;
   for (uint32_t i = 0; i < pCreateInfo->subpassCount; i++) {
      reference_count += pCreateInfo->pSubpasses[i].inputAttachmentCount;
      reference_count += pCreateInfo->pSubpasses[i].colorAttachmentCount;
      if (pCreateInfo->pSubpasses[i].pResolveAttachments)
         reference_count += pCreateInfo->pSubpasses[i].colorAttachmentCount;
      if (pCreateInfo->pSubpasses[i].pDepthStencilAttachment)
         reference_count += 1;
   }

   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, VkRenderPassCreateInfo2, create_info, 1);
   VK_MULTIALLOC_DECL(&ma, VkSubpassDescription2, subpasses,
                           pCreateInfo->subpassCount);
   VK_MULTIALLOC_DECL(&ma, VkAttachmentDescription2, attachments,
                           pCreateInfo->attachmentCount);
   VK_MULTIALLOC_DECL(&ma, VkSubpassDependency2, dependencies,
                           pCreateInfo->dependencyCount);
   VK_MULTIALLOC_DECL(&ma, VkAttachmentReference2, references,
                           reference_count);
   if (!vk_multialloc_alloc2(&ma, &device->alloc, pAllocator,
                             VK_SYSTEM_ALLOCATION_SCOPE_COMMAND))
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkAttachmentReference2 *reference_ptr = references;

   const VkRenderPassMultiviewCreateInfo *multiview_info = NULL;
   const VkRenderPassInputAttachmentAspectCreateInfo *aspect_info = NULL;
   vk_foreach_struct(ext, pCreateInfo->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO:
         aspect_info = (const VkRenderPassInputAttachmentAspectCreateInfo *)ext;
         /* We don't care about this information */
         break;

      case VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO:
         multiview_info = (const VkRenderPassMultiviewCreateInfo*) ext;
         break;

      default:
         mesa_logd("%s: ignored VkStructureType %u\n", __func__, ext->sType);
         break;
      }
   }

   for (uint32_t i = 0; i < pCreateInfo->attachmentCount; i++) {
      attachments[i] = (VkAttachmentDescription2) {
         .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
         .pNext = NULL,
         .flags = pCreateInfo->pAttachments[i].flags,
         .format = pCreateInfo->pAttachments[i].format,
         .samples = pCreateInfo->pAttachments[i].samples,
         .loadOp = pCreateInfo->pAttachments[i].loadOp,
         .storeOp = pCreateInfo->pAttachments[i].storeOp,
         .stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp,
         .stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp,
         .initialLayout = pCreateInfo->pAttachments[i].initialLayout,
         .finalLayout = pCreateInfo->pAttachments[i].finalLayout,
      };
   }

   for (uint32_t i = 0; i < pCreateInfo->subpassCount; i++) {
      subpasses[i] = (VkSubpassDescription2) {
         .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
         .pNext = NULL,
         .flags = pCreateInfo->pSubpasses[i].flags,
         .pipelineBindPoint = pCreateInfo->pSubpasses[i].pipelineBindPoint,
         .viewMask = 0,
         .inputAttachmentCount = pCreateInfo->pSubpasses[i].inputAttachmentCount,
         .colorAttachmentCount = pCreateInfo->pSubpasses[i].colorAttachmentCount,
         .preserveAttachmentCount = pCreateInfo->pSubpasses[i].preserveAttachmentCount,
         .pPreserveAttachments = pCreateInfo->pSubpasses[i].pPreserveAttachments,
      };

      if (multiview_info && multiview_info->subpassCount) {
         assert(multiview_info->subpassCount == pCreateInfo->subpassCount);
         subpasses[i].viewMask = multiview_info->pViewMasks[i];
      }

      subpasses[i].pInputAttachments = reference_ptr;
      translate_references(&reference_ptr,
                           subpasses[i].inputAttachmentCount,
                           pCreateInfo->pSubpasses[i].pInputAttachments,
                           pCreateInfo, true);
      subpasses[i].pColorAttachments = reference_ptr;
      translate_references(&reference_ptr,
                           subpasses[i].colorAttachmentCount,
                           pCreateInfo->pSubpasses[i].pColorAttachments,
                           pCreateInfo, false);
      subpasses[i].pResolveAttachments = NULL;
      if (pCreateInfo->pSubpasses[i].pResolveAttachments) {
         subpasses[i].pResolveAttachments = reference_ptr;
         translate_references(&reference_ptr,
                              subpasses[i].colorAttachmentCount,
                              pCreateInfo->pSubpasses[i].pResolveAttachments,
                              pCreateInfo, false);
      }
      subpasses[i].pDepthStencilAttachment = NULL;
      if (pCreateInfo->pSubpasses[i].pDepthStencilAttachment) {
         subpasses[i].pDepthStencilAttachment = reference_ptr;
         translate_references(&reference_ptr, 1,
                              pCreateInfo->pSubpasses[i].pDepthStencilAttachment,
                              pCreateInfo, false);
      }
   }

   assert(reference_ptr == references + reference_count);

   if (aspect_info != NULL) {
      for (uint32_t i = 0; i < aspect_info->aspectReferenceCount; i++) {
         const VkInputAttachmentAspectReference *ref =
            &aspect_info->pAspectReferences[i];

         assert(ref->subpass < pCreateInfo->subpassCount);
         VkSubpassDescription2 *subpass = &subpasses[ref->subpass];

         assert(ref->inputAttachmentIndex < subpass->inputAttachmentCount);
         VkAttachmentReference2 *att = (VkAttachmentReference2 *)
            &subpass->pInputAttachments[ref->inputAttachmentIndex];

         att->aspectMask = ref->aspectMask;
      }
   }

   for (uint32_t i = 0; i < pCreateInfo->dependencyCount; i++) {
      dependencies[i] = (VkSubpassDependency2) {
         .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
         .pNext = NULL,
         .srcSubpass = pCreateInfo->pDependencies[i].srcSubpass,
         .dstSubpass = pCreateInfo->pDependencies[i].dstSubpass,
         .srcStageMask = pCreateInfo->pDependencies[i].srcStageMask,
         .dstStageMask = pCreateInfo->pDependencies[i].dstStageMask,
         .srcAccessMask = pCreateInfo->pDependencies[i].srcAccessMask,
         .dstAccessMask = pCreateInfo->pDependencies[i].dstAccessMask,
         .dependencyFlags = pCreateInfo->pDependencies[i].dependencyFlags,
         .viewOffset = 0,
      };

      if (multiview_info && multiview_info->dependencyCount) {
         assert(multiview_info->dependencyCount == pCreateInfo->dependencyCount);
         dependencies[i].viewOffset = multiview_info->pViewOffsets[i];
      }
   }

   *create_info = (VkRenderPassCreateInfo2) {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .pNext = pCreateInfo->pNext,
      .flags = pCreateInfo->flags,
      .attachmentCount = pCreateInfo->attachmentCount,
      .pAttachments = attachments,
      .subpassCount = pCreateInfo->subpassCount,
      .pSubpasses = subpasses,
      .dependencyCount = pCreateInfo->dependencyCount,
      .pDependencies = dependencies,
   };

   if (multiview_info && multiview_info->correlationMaskCount > 0) {
      create_info->correlatedViewMaskCount = multiview_info->correlationMaskCount;
      create_info->pCorrelatedViewMasks = multiview_info->pCorrelationMasks;
   }

   VkResult result =
      device->dispatch_table.CreateRenderPass2(_device, create_info,
                                               pAllocator, pRenderPass);

   vk_free2(&device->alloc, pAllocator, create_info);

   return result;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdBeginRenderPass(VkCommandBuffer commandBuffer,
                             const VkRenderPassBeginInfo* pRenderPassBegin,
                             VkSubpassContents contents)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   VkSubpassBeginInfo info = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
      .contents = contents,
   };

   disp->device->dispatch_table.CmdBeginRenderPass2(commandBuffer,
                                                    pRenderPassBegin, &info);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdEndRenderPass(VkCommandBuffer commandBuffer)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   VkSubpassEndInfo info = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO,
   };

   disp->device->dispatch_table.CmdEndRenderPass2(commandBuffer, &info);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdNextSubpass(VkCommandBuffer commandBuffer,
                         VkSubpassContents contents)
{
   /* We don't have a vk_command_buffer object but we can assume, since we're
    * using common dispatch, that it's a vk_object of some sort.
    */
   struct vk_object_base *disp = (struct vk_object_base *)commandBuffer;

   VkSubpassBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
      .contents = contents,
   };

   VkSubpassEndInfo end_info = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO,
   };

   disp->device->dispatch_table.CmdNextSubpass2(commandBuffer, &begin_info,
                                                &end_info);
}

static unsigned
num_subpass_attachments2(const VkSubpassDescription2 *desc)
{
   bool has_depth_stencil_attachment =
      desc->pDepthStencilAttachment != NULL &&
      desc->pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED;

   const VkSubpassDescriptionDepthStencilResolveKHR *ds_resolve =
      vk_find_struct_const(desc->pNext,
                           SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR);

   bool has_depth_stencil_resolve_attachment =
      ds_resolve && ds_resolve->pDepthStencilResolveAttachment &&
      ds_resolve->pDepthStencilResolveAttachment->attachment != VK_ATTACHMENT_UNUSED;

   return desc->inputAttachmentCount +
          desc->colorAttachmentCount +
          (desc->pResolveAttachments ? desc->colorAttachmentCount : 0) +
          has_depth_stencil_attachment +
          has_depth_stencil_resolve_attachment;
}

static void
vk_render_pass_attachment_init(struct vk_render_pass_attachment *att,
                               const VkAttachmentDescription2 *desc)
{
   *att = (struct vk_render_pass_attachment) {
      .format                 = desc->format,
      .samples                = desc->samples,
      .view_mask              = 0,
      .load_op                = desc->loadOp,
      .store_op               = desc->storeOp,
      .stencil_load_op        = desc->stencilLoadOp,
      .stencil_store_op       = desc->stencilStoreOp,
      .initial_layout         = desc->initialLayout,
      .final_layout           = desc->finalLayout,
      .initial_stencil_layout = vk_att_desc_stencil_layout(desc, false),
      .final_stencil_layout   = vk_att_desc_stencil_layout(desc, true),
   };
}

static void
vk_subpass_attachment_init(struct vk_subpass_attachment *att,
                           struct vk_render_pass *pass,
                           uint32_t subpass_idx,
                           const VkAttachmentReference2 *ref,
                           const VkAttachmentDescription2 *attachments,
                           VkImageUsageFlagBits usage)
{
   if (ref->attachment >= pass->attachment_count) {
      assert(ref->attachment == VK_ATTACHMENT_UNUSED);
      *att = (struct vk_subpass_attachment) {
         .attachment = VK_ATTACHMENT_UNUSED,
      };
      return;
   }

   struct vk_render_pass_attachment *pass_att =
      &pass->attachments[ref->attachment];

   *att = (struct vk_subpass_attachment) {
      .attachment =     ref->attachment,
      .aspects =        vk_format_aspects(pass_att->format),
      .usage =          usage,
      .layout =         ref->layout,
      .stencil_layout = vk_att_ref_stencil_layout(ref, attachments),
   };

   switch (usage) {
   case VK_IMAGE_USAGE_TRANSFER_DST_BIT:
      break; /* No special aspect requirements */

   case VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
      /* From the Vulkan 1.2.184 spec:
       *
       *    "aspectMask is ignored when this structure is used to describe
       *    anything other than an input attachment reference."
       */
      assert(!(ref->aspectMask & ~att->aspects));
      att->aspects = ref->aspectMask;
      break;

   case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
      assert(att->aspects == VK_IMAGE_ASPECT_COLOR_BIT);
      break;

   case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
      assert(!(att->aspects & ~(VK_IMAGE_ASPECT_DEPTH_BIT |
                                VK_IMAGE_ASPECT_STENCIL_BIT)));
      break;

   default:
      unreachable("Invalid subpass attachment usage");
   }
}

static void
vk_subpass_attachment_link_resolve(struct vk_subpass_attachment *att,
                                   struct vk_subpass_attachment *resolve,
                                   const VkRenderPassCreateInfo2 *info)
{
   if (resolve->attachment == VK_ATTACHMENT_UNUSED)
      return;

   assert(att->attachment != VK_ATTACHMENT_UNUSED);
   assert(info->pAttachments[att->attachment].format ==
          info->pAttachments[resolve->attachment].format);
   assert(resolve->aspects == att->aspects);

   att->resolve = resolve;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateRenderPass2(VkDevice _device,
                            const VkRenderPassCreateInfo2 *pCreateInfo,
                            const VkAllocationCallbacks *pAllocator,
                            VkRenderPass *pRenderPass)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR);

   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, struct vk_render_pass, pass, 1);
   VK_MULTIALLOC_DECL(&ma, struct vk_render_pass_attachment, attachments,
                           pCreateInfo->attachmentCount);
   VK_MULTIALLOC_DECL(&ma, struct vk_subpass, subpasses,
                           pCreateInfo->subpassCount);
   VK_MULTIALLOC_DECL(&ma, struct vk_subpass_dependency, dependencies,
                           pCreateInfo->dependencyCount);

   uint32_t subpass_attachment_count = 0;
   uint32_t subpass_color_format_count = 0;
   for (uint32_t i = 0; i < pCreateInfo->subpassCount; i++) {
      subpass_attachment_count +=
         num_subpass_attachments2(&pCreateInfo->pSubpasses[i]);
      subpass_color_format_count +=
         pCreateInfo->pSubpasses[i].colorAttachmentCount;
   }
   VK_MULTIALLOC_DECL(&ma, struct vk_subpass_attachment, subpass_attachments,
                      subpass_attachment_count);
   VK_MULTIALLOC_DECL(&ma, VkFormat, subpass_color_formats,
                      subpass_color_format_count);

   if (!vk_object_multizalloc(device, &ma, pAllocator,
                              VK_OBJECT_TYPE_RENDER_PASS))
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   pass->attachment_count = pCreateInfo->attachmentCount;
   pass->attachments = attachments;
   pass->subpass_count = pCreateInfo->subpassCount;
   pass->subpasses = subpasses;
   pass->dependency_count = pCreateInfo->dependencyCount;
   pass->dependencies = dependencies;

   for (uint32_t a = 0; a < pCreateInfo->attachmentCount; a++) {
      vk_render_pass_attachment_init(&pass->attachments[a],
                                     &pCreateInfo->pAttachments[a]);
   }

   struct vk_subpass_attachment *next_subpass_attachment = subpass_attachments;
   VkFormat *next_subpass_color_format = subpass_color_formats;
   for (uint32_t s = 0; s < pCreateInfo->subpassCount; s++) {
      const VkSubpassDescription2 *desc = &pCreateInfo->pSubpasses[s];
      struct vk_subpass *subpass = &pass->subpasses[s];

      subpass->attachment_count = num_subpass_attachments2(desc);
      subpass->attachments = subpass_attachments;

      /* From the Vulkan 1.3.204 spec:
       *
       *    VUID-VkRenderPassCreateInfo2-viewMask-03058
       *
       *    "The VkSubpassDescription2::viewMask member of all elements of
       *    pSubpasses must either all be 0, or all not be 0"
       */
      if (desc->viewMask)
         pass->is_multiview = true;
      assert(pass->is_multiview == (desc->viewMask != 0));

      /* For all view masks in the vk_render_pass data structure, we use a
       * mask of 1 for non-multiview instead of a mask of 0.
       */
      subpass->view_mask = desc->viewMask ? desc->viewMask : 1;

      assert(desc->colorAttachmentCount <= 32);
      uint32_t color_self_deps = 0;
      bool has_depth_self_dep = false;
      bool has_stencil_self_dep = false;

      subpass->input_count = desc->inputAttachmentCount;
      if (desc->inputAttachmentCount > 0) {
         subpass->input_attachments = next_subpass_attachment;
         next_subpass_attachment += desc->inputAttachmentCount;

         for (uint32_t a = 0; a < desc->inputAttachmentCount; a++) {
            vk_subpass_attachment_init(&subpass->input_attachments[a],
                                       pass, s,
                                       &desc->pInputAttachments[a],
                                       pCreateInfo->pAttachments,
                                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

            if (desc->pInputAttachments[a].attachment != VK_ATTACHMENT_UNUSED) {
               for (uint32_t c = 0; c < desc->colorAttachmentCount; c++) {
                  if (desc->pColorAttachments[c].attachment ==
                      desc->pInputAttachments[a].attachment)
                     color_self_deps |= (1u << c);
               }

               if (desc->pDepthStencilAttachment != NULL &&
                   desc->pDepthStencilAttachment->attachment ==
                      desc->pInputAttachments[a].attachment) {
                  VkImageAspectFlags aspects =
                     subpass->input_attachments[a].aspects;
                  if (aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
                     has_depth_self_dep = true;
                  if (aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
                     has_stencil_self_dep = true;
               }
            }
         }
      }

      subpass->color_count = desc->colorAttachmentCount;
      if (desc->colorAttachmentCount > 0) {
         subpass->color_attachments = next_subpass_attachment;
         next_subpass_attachment += desc->colorAttachmentCount;

         for (uint32_t a = 0; a < desc->colorAttachmentCount; a++) {
            vk_subpass_attachment_init(&subpass->color_attachments[a],
                                       pass, s,
                                       &desc->pColorAttachments[a],
                                       pCreateInfo->pAttachments,
                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
         }
      }

      if (desc->pResolveAttachments) {
         subpass->color_resolve_count = desc->colorAttachmentCount;
         subpass->color_resolve_attachments = next_subpass_attachment;
         next_subpass_attachment += desc->colorAttachmentCount;

         for (uint32_t a = 0; a < desc->colorAttachmentCount; a++) {
            vk_subpass_attachment_init(&subpass->color_resolve_attachments[a],
                                       pass, s,
                                       &desc->pResolveAttachments[a],
                                       pCreateInfo->pAttachments,
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            vk_subpass_attachment_link_resolve(&subpass->color_attachments[a],
                                               &subpass->color_resolve_attachments[a],
                                               pCreateInfo);
         }
      }

      if (desc->pDepthStencilAttachment &&
          desc->pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED) {
         subpass->depth_stencil_attachment = next_subpass_attachment++;

         vk_subpass_attachment_init(subpass->depth_stencil_attachment,
                                    pass, s,
                                    desc->pDepthStencilAttachment,
                                    pCreateInfo->pAttachments,
                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
      }

      const VkSubpassDescriptionDepthStencilResolveKHR *ds_resolve =
         vk_find_struct_const(desc->pNext,
                              SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR);

      if (ds_resolve && ds_resolve->pDepthStencilResolveAttachment &&
          ds_resolve->pDepthStencilResolveAttachment->attachment != VK_ATTACHMENT_UNUSED) {
         subpass->depth_stencil_resolve_attachment = next_subpass_attachment++;

         vk_subpass_attachment_init(subpass->depth_stencil_resolve_attachment,
                                    pass, s,
                                    ds_resolve->pDepthStencilResolveAttachment,
                                    pCreateInfo->pAttachments,
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT);
         vk_subpass_attachment_link_resolve(subpass->depth_stencil_attachment,
                                            subpass->depth_stencil_resolve_attachment,
                                            pCreateInfo);

         subpass->depth_resolve_mode = ds_resolve->depthResolveMode;
         subpass->stencil_resolve_mode = ds_resolve->stencilResolveMode;
      }

      VkFormat *color_formats = NULL;
      VkSampleCountFlagBits samples = 0;
      if (desc->colorAttachmentCount > 0) {
         color_formats = next_subpass_color_format;
         for (uint32_t a = 0; a < desc->colorAttachmentCount; a++) {
            const VkAttachmentReference2 *ref = &desc->pColorAttachments[a];
            if (ref->attachment >= pCreateInfo->attachmentCount) {
               color_formats[a] = VK_FORMAT_UNDEFINED;
            } else {
               const VkAttachmentDescription2 *att =
                  &pCreateInfo->pAttachments[ref->attachment];

               color_formats[a] = att->format;

               assert(samples == 0 || samples == att->samples);
               samples |= att->samples;
            }
         }
         next_subpass_color_format += desc->colorAttachmentCount;
      }

      VkFormat depth_format = VK_FORMAT_UNDEFINED;
      VkFormat stencil_format = VK_FORMAT_UNDEFINED;
      if (desc->pDepthStencilAttachment != NULL) {
         const VkAttachmentReference2 *ref = desc->pDepthStencilAttachment;
         if (ref->attachment < pCreateInfo->attachmentCount) {
            const VkAttachmentDescription2 *att =
               &pCreateInfo->pAttachments[ref->attachment];

            if (vk_format_has_depth(att->format))
               depth_format = att->format;
            if (vk_format_has_stencil(att->format))
               stencil_format = att->format;

            assert(samples == 0 || samples == att->samples);
            samples |= att->samples;
         }
      }

      subpass->self_dep_info = (VkRenderingSelfDependencyInfoMESA) {
         .sType = VK_STRUCTURE_TYPE_RENDERING_SELF_DEPENDENCY_INFO_MESA,
         .colorSelfDependencies = color_self_deps,
         .depthSelfDependency = has_depth_self_dep,
         .stencilSelfDependency = has_stencil_self_dep,
      };

      subpass->pipeline_info = (VkPipelineRenderingCreateInfo) {
         .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
         .pNext = &subpass->self_dep_info,
         .viewMask = desc->viewMask,
         .colorAttachmentCount = desc->colorAttachmentCount,
         .pColorAttachmentFormats = color_formats,
         .depthAttachmentFormat = depth_format,
         .stencilAttachmentFormat = stencil_format,
      };

      subpass->inheritance_info = (VkCommandBufferInheritanceRenderingInfo) {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
         .pNext = &subpass->self_dep_info,
         /* If we're inheriting, the contents are clearly in secondaries */
         .flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT,
         .viewMask = desc->viewMask,
         .colorAttachmentCount = desc->colorAttachmentCount,
         .pColorAttachmentFormats = color_formats,
         .depthAttachmentFormat = depth_format,
         .stencilAttachmentFormat = stencil_format,
         .rasterizationSamples = samples,
      };
   }
   assert(next_subpass_attachment ==
          subpass_attachments + subpass_attachment_count);
   assert(next_subpass_color_format ==
          subpass_color_formats + subpass_color_format_count);

   /* Walk backwards over the subpasses to compute view masks and
    * last_subpass masks for all attachments.
    */
   for (uint32_t s = 0; s < pCreateInfo->subpassCount; s++) {
      struct vk_subpass *subpass =
         &pass->subpasses[(pCreateInfo->subpassCount - 1) - s];

      /* First, compute last_subpass for all the attachments */
      for (uint32_t a = 0; a < subpass->attachment_count; a++) {
         struct vk_subpass_attachment *att = &subpass->attachments[a];
         if (att->attachment == VK_ATTACHMENT_UNUSED)
            continue;

         assert(att->attachment < pass->attachment_count);
         const struct vk_render_pass_attachment *pass_att =
            &pass->attachments[att->attachment];

         att->last_subpass = subpass->view_mask & ~pass_att->view_mask;
      }

      /* Then compute pass_att->view_mask.  We do the two separately so that
       * we end up with the right last_subpass even if the same attachment is
       * used twice within a subpass.
       */
      for (uint32_t a = 0; a < subpass->attachment_count; a++) {
         const struct vk_subpass_attachment *att = &subpass->attachments[a];
         if (att->attachment == VK_ATTACHMENT_UNUSED)
            continue;

         assert(att->attachment < pass->attachment_count);
         struct vk_render_pass_attachment *pass_att =
            &pass->attachments[att->attachment];

         pass_att->view_mask |= subpass->view_mask;
      }
   }

   pass->dependency_count = pCreateInfo->dependencyCount;
   for (uint32_t d = 0; d < pCreateInfo->dependencyCount; d++) {
      const VkSubpassDependency2 *dep = &pCreateInfo->pDependencies[d];

      pass->dependencies[d] = (struct vk_subpass_dependency) {
         .flags = dep->dependencyFlags,
         .src_subpass = dep->srcSubpass,
         .dst_subpass = dep->dstSubpass,
         .src_stage_mask = (VkPipelineStageFlags2)dep->srcStageMask,
         .dst_stage_mask = (VkPipelineStageFlags2)dep->dstStageMask,
         .src_access_mask = (VkAccessFlags2)dep->srcAccessMask,
         .dst_access_mask = (VkAccessFlags2)dep->dstAccessMask,
         .view_offset = dep->viewOffset,
      };

      /* From the Vulkan 1.3.204 spec:
       *
       *    "If a VkMemoryBarrier2 is included in the pNext chain,
       *    srcStageMask, dstStageMask, srcAccessMask, and dstAccessMask
       *    parameters are ignored. The synchronization and access scopes
       *    instead are defined by the parameters of VkMemoryBarrier2."
       */
      const VkMemoryBarrier2 *barrier =
         vk_find_struct_const(dep->pNext, MEMORY_BARRIER_2);
      if (barrier != NULL) {
         pass->dependencies[d].src_stage_mask = barrier->srcStageMask;
         pass->dependencies[d].dst_stage_mask = barrier->dstStageMask;
         pass->dependencies[d].src_access_mask = barrier->srcAccessMask;
         pass->dependencies[d].dst_access_mask = barrier->dstAccessMask;
      }
   }

   *pRenderPass = vk_render_pass_to_handle(pass);

   return VK_SUCCESS;
}

const VkPipelineRenderingCreateInfo *
vk_get_pipeline_rendering_create_info(const VkGraphicsPipelineCreateInfo *info)
{
   VK_FROM_HANDLE(vk_render_pass, render_pass, info->renderPass);
   if (render_pass != NULL) {
      assert(info->subpass < render_pass->subpass_count);
      return &render_pass->subpasses[info->subpass].pipeline_info;
   }

   return vk_find_struct_const(info->pNext, PIPELINE_RENDERING_CREATE_INFO);
}

const VkCommandBufferInheritanceRenderingInfo *
vk_get_command_buffer_inheritance_rendering_info(
   VkCommandBufferLevel level,
   const VkCommandBufferBeginInfo *pBeginInfo)
{
   /* From the Vulkan 1.3.204 spec:
    *
    *    "VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT specifies that a
    *    secondary command buffer is considered to be entirely inside a render
    *    pass. If this is a primary command buffer, then this bit is ignored."
    *
    * Since we're only concerned with the continue case here, we can ignore
    * any primary command buffers.
    */
   if (level == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
      return NULL;

   if (!(pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT))
      return NULL;

   const VkCommandBufferInheritanceInfo *inheritance =
      pBeginInfo->pInheritanceInfo;

   /* From the Vulkan 1.3.204 spec:
    *
    *    "If VkCommandBufferInheritanceInfo::renderPass is not VK_NULL_HANDLE,
    *    or VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT is not specified
    *    in VkCommandBufferBeginInfo::flags, parameters of this structure are
    *    ignored."
    *
    * If we have a render pass that wins, even if a
    * VkCommandBufferInheritanceRenderingInfo struct is included in the pNext
    * chain.
    */
   VK_FROM_HANDLE(vk_render_pass, render_pass, inheritance->renderPass);
   if (render_pass != NULL) {
      assert(inheritance->subpass < render_pass->subpass_count);
      return &render_pass->subpasses[inheritance->subpass].inheritance_info;
   }

   return vk_find_struct_const(inheritance->pNext,
                               COMMAND_BUFFER_INHERITANCE_RENDERING_INFO);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_DestroyRenderPass(VkDevice _device,
                            VkRenderPass renderPass,
                            const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_render_pass, pass, renderPass);

   if (!pass)
      return;

   vk_object_free(device, pAllocator, pass);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetRenderAreaGranularity(VkDevice device,
                                   VkRenderPass renderPass,
                                   VkExtent2D *pGranularity)
{
   *pGranularity = (VkExtent2D) { 1, 1 };
}

static bool
vk_image_layout_supports_input_attachment(VkImageLayout layout)
{
   switch (layout) {
   case VK_IMAGE_LAYOUT_GENERAL:
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
   case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
      return true;
   default:
      return false;
   }
}

struct stage_access {
   VkPipelineStageFlagBits2 stages;
   VkAccessFlagBits2 access;
};

static bool
vk_image_layout_are_all_aspects_read_only(VkImageLayout layout,
                                          VkImageAspectFlags aspects)
{
   while (aspects) {
      VkImageAspectFlagBits aspect = 1u << u_bit_scan(&aspects);
      if (!vk_image_layout_is_read_only(layout, aspect))
         return false;
   }
   return true;
}

static struct stage_access
stage_access_for_layout(VkImageLayout layout, VkImageAspectFlags aspects)
{
   VkPipelineStageFlagBits2 stages = 0;
   VkAccessFlagBits2 access = 0;

   if (vk_image_layout_supports_input_attachment(layout)) {
      stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      access |= VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
   }

   if (aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      if (!vk_image_layout_are_all_aspects_read_only(layout, aspects)) {
         access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

         /* It might be a resolve attachment */
         stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
         access |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
      }
   } else {
      /* Color */
      if (!vk_image_layout_are_all_aspects_read_only(layout, aspects)) {
         /* There are no read-only color attachments */
         stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

         /* It might be a resolve attachment */
         stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
         access |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
      }
   }

   return (struct stage_access) {
      .stages = stages,
      .access = access,
   };
}

static void
transition_image_range(const struct vk_image_view *image_view,
                       VkImageSubresourceRange range,
                       VkImageLayout old_layout,
                       VkImageLayout new_layout,
                       VkImageLayout old_stencil_layout,
                       VkImageLayout new_stencil_layout,
                       uint32_t *barrier_count,
                       uint32_t max_barrier_count,
                       VkImageMemoryBarrier2 *barriers)
{
   VkImageAspectFlags aspects_left = range.aspectMask;
   while (aspects_left) {
      range.aspectMask = aspects_left;

      /* If we have a depth/stencil image and one of the layouts doesn't match
       * between depth and stencil, we need two barriers.  Restrict to depth
       * and we'll pick up stencil on the next iteration.
       */
      if (range.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT |
                               VK_IMAGE_ASPECT_STENCIL_BIT) &&
          (old_layout != old_stencil_layout ||
           new_layout != new_stencil_layout))
         range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

      if (range.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) {
         /* We're down to a single aspect bit so this is going to be the last
          * iteration and it's fine to stomp the input variables here.
          */
         old_layout = old_stencil_layout;
         new_layout = new_stencil_layout;
      }

      if (new_layout != old_layout) {
         /* We could go about carefully calculating every possible way the
          * attachment may have been used in the render pass or we can break
          * out the big hammer and throw in any stage and access flags
          * possible for the given layouts.
          */
         struct stage_access src_sa, dst_sa;
         src_sa = stage_access_for_layout(old_layout, range.aspectMask);
         dst_sa = stage_access_for_layout(new_layout, range.aspectMask);

         assert(*barrier_count < max_barrier_count);
         barriers[(*barrier_count)++] = (VkImageMemoryBarrier2) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = src_sa.stages,
            .srcAccessMask = src_sa.access,
            .dstStageMask = dst_sa.stages,
            .dstAccessMask = dst_sa.access,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk_image_to_handle(image_view->image),
            .subresourceRange = range,
         };
      }

      aspects_left &= ~range.aspectMask;
   }
}

static void
transition_attachment_layout(struct vk_command_buffer *cmd_buffer,
                             uint32_t att_idx,
                             uint32_t view_mask,
                             VkImageLayout layout,
                             VkImageLayout stencil_layout,
                             uint32_t *barrier_count,
                             uint32_t max_barrier_count,
                             VkImageMemoryBarrier2 *barriers)
{
   const struct vk_render_pass *pass = cmd_buffer->render_pass;
   const struct vk_framebuffer *framebuffer = cmd_buffer->framebuffer;
   struct vk_attachment_state *att_state = &cmd_buffer->attachments[att_idx];
   const struct vk_image_view *image_view = att_state->image_view;

   /* 3D is stupidly special.  From the Vulkan 1.3.204 spec:
    *
    *    "When the VkImageSubresourceRange structure is used to select a
    *    subset of the slices of a 3D image’s mip level in order to create
    *    a 2D or 2D array image view of a 3D image created with
    *    VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, baseArrayLayer and
    *    layerCount specify the first slice index and the number of slices
    *    to include in the created image view. Such an image view can be
    *    used as a framebuffer attachment that refers only to the specified
    *    range of slices of the selected mip level. However, any layout
    *    transitions performed on such an attachment view during a render
    *    pass instance still apply to the entire subresource referenced
    *    which includes all the slices of the selected mip level."
    *
    * To deal with this, we expand out the layer range to include the
    * entire 3D image and treat them as having only a single view even when
    * multiview is enabled.  This later part means that we effectively only
    * track one image layout for the entire attachment rather than one per
    * view like we do for all the others.
    */
   if (image_view->image->image_type == VK_IMAGE_TYPE_3D)
      view_mask = 1;

   while (view_mask) {
      int view = u_bit_scan(&view_mask);
      assert(view >= 0 && view < MESA_VK_MAX_MULTIVIEW_VIEW_COUNT);

      struct vk_attachment_view_state *att_view_state = &att_state->views[view];

      /* First, check to see if we even need a transition */
      if (att_view_state->layout == layout &&
          att_view_state->stencil_layout == stencil_layout)
         continue;

      VkImageSubresourceRange range = {
         .aspectMask = image_view->aspects,
         .baseMipLevel = image_view->base_mip_level,
         .levelCount = 1,
      };

      /* 3D is stupidly special.  From the Vulkan 1.3.204 spec:
       *
       *    "When the VkImageSubresourceRange structure is used to select a
       *    subset of the slices of a 3D image’s mip level in order to create
       *    a 2D or 2D array image view of a 3D image created with
       *    VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, baseArrayLayer and
       *    layerCount specify the first slice index and the number of slices
       *    to include in the created image view. Such an image view can be
       *    used as a framebuffer attachment that refers only to the specified
       *    range of slices of the selected mip level. However, any layout
       *    transitions performed on such an attachment view during a render
       *    pass instance still apply to the entire subresource referenced
       *    which includes all the slices of the selected mip level."
       *
       * To deal with this, we expand out the layer range to include the
       * entire 3D image and treat them as having only a single view even when
       * multiview is enabled.  This later part means that we effectively only
       * track one image layout for the entire attachment rather than one per
       * view like we do for all the others.
       */
      if (image_view->image->image_type == VK_IMAGE_TYPE_3D) {
         assert(view == 0);
         range.baseArrayLayer = 0;
         range.layerCount = image_view->extent.depth;
      } else if (pass->is_multiview) {
         range.baseArrayLayer = image_view->base_array_layer + view;
         range.layerCount = 1;
      } else {
         assert(view == 0);
         range.baseArrayLayer = image_view->base_array_layer;
         range.layerCount = framebuffer->layers;
      }

      transition_image_range(image_view, range,
                             att_view_state->layout, layout,
                             att_view_state->stencil_layout, stencil_layout,
                             barrier_count, max_barrier_count, barriers);

      att_view_state->layout = layout;
      att_view_state->stencil_layout = stencil_layout;
   }
}

static void
load_store_attachment(struct vk_command_buffer *cmd_buffer,
                      uint32_t att_idx, uint32_t view_mask,
                      VkImageLayout layout, VkImageLayout stencil_layout)
{
   const struct vk_render_pass *pass = cmd_buffer->render_pass;
   const struct vk_render_pass_attachment *pass_att =
      &pass->attachments[att_idx];
   struct vk_attachment_state *att_state = &cmd_buffer->attachments[att_idx];
   struct vk_device_dispatch_table *disp =
      &cmd_buffer->base.device->dispatch_table;

   /* We only need to load/store if there's a clear */
   bool need_load_store = false;
   if ((att_state->image_view->aspects & ~VK_IMAGE_ASPECT_STENCIL_BIT) &&
       pass_att->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
      need_load_store = true;

   if ((att_state->image_view->aspects & VK_IMAGE_ASPECT_STENCIL_BIT) &&
       pass_att->stencil_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
      need_load_store = true;

   if (!need_load_store)
      return;

   const VkRenderingAttachmentInfo att = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = vk_image_view_to_handle(att_state->image_view),
      .imageLayout = layout,
      .loadOp = pass_att->load_op,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = att_state->clear_value,
   };

   const VkRenderingAttachmentInfo stencil_att = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = vk_image_view_to_handle(att_state->image_view),
      .imageLayout = stencil_layout,
      .loadOp = pass_att->stencil_load_op,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = att_state->clear_value,
   };

   VkRenderingInfo render = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = cmd_buffer->render_area,
      .layerCount = 1,
      .viewMask = view_mask,
   };

   if (att_state->image_view->aspects & (VK_IMAGE_ASPECT_DEPTH_BIT |
                                         VK_IMAGE_ASPECT_STENCIL_BIT)) {
      if (att_state->image_view->aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
         render.pDepthAttachment = &att;
      if (att_state->image_view->aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
         render.pStencilAttachment = &stencil_att;
   } else {
      render.colorAttachmentCount = 1;
      render.pColorAttachments = &att;
   }

   disp->CmdBeginRendering(vk_command_buffer_to_handle(cmd_buffer), &render);
   disp->CmdEndRendering(vk_command_buffer_to_handle(cmd_buffer));
}

static void
begin_subpass(struct vk_command_buffer *cmd_buffer,
              const VkSubpassBeginInfo *begin_info)
{
   struct vk_device_dispatch_table *disp =
      &cmd_buffer->base.device->dispatch_table;
   const struct vk_render_pass *pass = cmd_buffer->render_pass;
   const struct vk_framebuffer *framebuffer = cmd_buffer->framebuffer;
   const uint32_t subpass_idx = cmd_buffer->subpass_idx;
   assert(subpass_idx < pass->subpass_count);
   const struct vk_subpass *subpass = &pass->subpasses[subpass_idx];

   /* First, figure out a general memory barrier from subpass dependencies. */
   bool needs_mem_barrier = false;
   VkMemoryBarrier2 mem_barrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
   };
   for (uint32_t d = 0; d < pass->dependency_count; d++) {
      const struct vk_subpass_dependency *dep = &pass->dependencies[d];
      if (dep->dst_subpass != subpass_idx)
         continue;

      if (dep->flags & VK_DEPENDENCY_VIEW_LOCAL_BIT) {
         /* From the Vulkan 1.3.204 spec:
          *
          *    VUID-VkSubpassDependency2-dependencyFlags-03091
          *
          *    "If dependencyFlags includes VK_DEPENDENCY_VIEW_LOCAL_BIT,
          *    dstSubpass must not be equal to VK_SUBPASS_EXTERNAL"
          */
         assert(dep->src_subpass != VK_SUBPASS_EXTERNAL);

         assert(dep->src_subpass < pass->subpass_count);
         const struct vk_subpass *src_subpass =
            &pass->subpasses[dep->src_subpass];

         /* Figure out the set of views in the source subpass affected by this
          * dependency.
          */
         uint32_t src_dep_view_mask = subpass->view_mask;
         if (dep->view_offset >= 0)
            src_dep_view_mask <<= dep->view_offset;
         else
            src_dep_view_mask >>= -dep->view_offset;

         /* From the Vulkan 1.3.204 spec:
          *
          *    "If the dependency is view-local, then each view (dstView) in
          *    the destination subpass depends on the view dstView +
          *    pViewOffsets[dependency] in the source subpass. If there is not
          *    such a view in the source subpass, then this dependency does
          *    not affect that view in the destination subpass."
          */
         if (!(src_subpass->view_mask & src_dep_view_mask))
            continue;
      }

      needs_mem_barrier = true;
      mem_barrier.srcStageMask |= dep->src_stage_mask;
      mem_barrier.srcAccessMask |= dep->src_access_mask;
      mem_barrier.dstStageMask |= dep->dst_stage_mask;
      mem_barrier.dstAccessMask |= dep->dst_access_mask;
   }

   uint32_t max_image_barrier_count = 0;
   for (uint32_t a = 0; a < subpass->attachment_count; a++) {
      const struct vk_subpass_attachment *att = &subpass->attachments[a];
      if (att->attachment == VK_ATTACHMENT_UNUSED)
         continue;

      struct vk_attachment_state *att_state =
         &cmd_buffer->attachments[att->attachment];
      const struct vk_image_view *image_view = att_state->image_view;

      max_image_barrier_count += util_bitcount(subpass->view_mask) *
                                 util_bitcount(image_view->aspects);
   }
   STACK_ARRAY(VkImageMemoryBarrier2, image_barriers, max_image_barrier_count);
   uint32_t image_barrier_count = 0;

   for (uint32_t a = 0; a < subpass->attachment_count; a++) {
      const struct vk_subpass_attachment *att = &subpass->attachments[a];
      if (att->attachment == VK_ATTACHMENT_UNUSED)
         continue;

      transition_attachment_layout(cmd_buffer,
                                   att->attachment,
                                   subpass->view_mask,
                                   att->layout, att->stencil_layout,
                                   &image_barrier_count,
                                   max_image_barrier_count,
                                   image_barriers);
   }
   assert(image_barrier_count <= max_image_barrier_count);

   if (needs_mem_barrier || image_barrier_count > 0) {
      const VkDependencyInfo dependency_info = {
         .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
         .dependencyFlags = 0,
         .memoryBarrierCount = needs_mem_barrier ? 1 : 0,
         .pMemoryBarriers = needs_mem_barrier ? &mem_barrier : NULL,
         .imageMemoryBarrierCount = image_barrier_count,
         .pImageMemoryBarriers = image_barrier_count > 0 ?
                                 image_barriers : NULL,
      };
      disp->CmdPipelineBarrier2(vk_command_buffer_to_handle(cmd_buffer),
                                &dependency_info);
   }

   STACK_ARRAY_FINISH(image_barriers);

   STACK_ARRAY(VkRenderingAttachmentInfo, color_attachments,
               subpass->color_count);
   for (uint32_t i = 0; i < subpass->color_count; i++) {
      const struct vk_subpass_attachment *att = &subpass->color_attachments[i];
      if (att->attachment == VK_ATTACHMENT_UNUSED) {
         color_attachments[i] = (VkRenderingAttachmentInfo) {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = VK_NULL_HANDLE,
         };
         continue;
      }

      assert(att->attachment < pass->attachment_count);
      const struct vk_render_pass_attachment *pass_att =
         &pass->attachments[att->attachment];
      struct vk_attachment_state *att_state =
         &cmd_buffer->attachments[att->attachment];

      VkRenderingAttachmentInfo *att_info = &color_attachments[i];
      *att_info = (VkRenderingAttachmentInfo) {
         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
         .imageView = vk_image_view_to_handle(att_state->image_view),
         .imageLayout = att->layout,
      };

      if (!(subpass->view_mask & att_state->views_loaded)) {
         /* None of these views have been used before */
         att_info->loadOp = pass_att->load_op;
         att_info->clearValue = att_state->clear_value;
      } else {
         /* We've seen at least one of the views of this attachment before so
          * we need to LOAD_OP_LOAD.
          */
         att_info->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

         if (subpass->view_mask & ~att_state->views_loaded) {
            /* One of the views in our subpass hasn't been used yet but some
             * of the others have.  In this case, there's no way we can clear
             * it with a load op and we need to clear it manually.  Do a quick
             * Begin/EndRendering just to do the clear.
             */
            load_store_attachment(cmd_buffer, att->attachment,
                                  subpass->view_mask & ~att_state->views_loaded,
                                  att->layout, VK_IMAGE_LAYOUT_UNDEFINED);
         }
      }
      att_state->views_loaded |= subpass->view_mask;

      if (!(subpass->view_mask & ~att->last_subpass)) {
         /* This is the last subpass for every view */
         att_info->storeOp = pass_att->store_op;
      } else {
         /* For at least one of our views, this isn't the last subpass */
         att_info->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      }

      if (att->resolve != NULL) {
         assert(att->resolve->attachment < pass->attachment_count);
         struct vk_attachment_state *res_att_state =
            &cmd_buffer->attachments[att->resolve->attachment];

         if (vk_format_is_int(res_att_state->image_view->format))
            att_info->resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
         else
            att_info->resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;

         att_info->resolveImageView =
            vk_image_view_to_handle(res_att_state->image_view);
         att_info->resolveImageLayout = att->resolve->layout;
      }
   }

   VkRenderingAttachmentInfo depth_attachment = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = VK_NULL_HANDLE,
   };
   VkRenderingAttachmentInfo stencil_attachment = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = VK_NULL_HANDLE,
   };
   if (subpass->depth_stencil_attachment != NULL) {
      const struct vk_subpass_attachment *att =
         subpass->depth_stencil_attachment;

      assert(att->attachment < pass->attachment_count);
      const struct vk_render_pass_attachment *pass_att =
         &pass->attachments[att->attachment];
      struct vk_attachment_state *att_state =
         &cmd_buffer->attachments[att->attachment];

      if (att_state->image_view->aspects & VK_IMAGE_ASPECT_DEPTH_BIT) {
         depth_attachment.imageView =
            vk_image_view_to_handle(att_state->image_view);
         depth_attachment.imageLayout = att->layout;
      }

      if (att_state->image_view->aspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
         stencil_attachment.imageView =
            vk_image_view_to_handle(att_state->image_view);
         stencil_attachment.imageLayout = att->stencil_layout;
      }

      if (!(subpass->view_mask & att_state->views_loaded)) {
         /* None of these views have been used before */
         depth_attachment.loadOp = pass_att->load_op;
         depth_attachment.clearValue = att_state->clear_value;
         stencil_attachment.loadOp = pass_att->stencil_load_op;
         stencil_attachment.clearValue = att_state->clear_value;
      } else {
         /* We've seen at least one of the views of this attachment before so
          * we need to LOAD_OP_LOAD.
          */
         depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
         stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

         if (subpass->view_mask & ~att_state->views_loaded) {
            load_store_attachment(cmd_buffer, att->attachment,
                                  subpass->view_mask & ~att_state->views_loaded,
                                  att->layout, att->stencil_layout);
         }
      }
      att_state->views_loaded |= subpass->view_mask;

      if (!(subpass->view_mask & ~att->last_subpass)) {
         /* This is the last subpass for every view */
         depth_attachment.storeOp = pass_att->store_op;
         stencil_attachment.storeOp = pass_att->stencil_store_op;
      } else {
         /* For at least one of our views, this isn't the last subpass */
         depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
         stencil_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      }

      if (att->resolve != NULL) {
         assert(att->resolve->attachment < pass->attachment_count);
         struct vk_attachment_state *res_att_state =
            &cmd_buffer->attachments[att->resolve->attachment];

         if (subpass->depth_resolve_mode != VK_RESOLVE_MODE_NONE) {
            depth_attachment.resolveMode = subpass->depth_resolve_mode;
            depth_attachment.resolveImageView =
               vk_image_view_to_handle(res_att_state->image_view);
            depth_attachment.resolveImageLayout = att->resolve->layout;
         }

         if (subpass->stencil_resolve_mode != VK_RESOLVE_MODE_NONE) {
            stencil_attachment.resolveMode = subpass->stencil_resolve_mode;
            stencil_attachment.resolveImageView =
               vk_image_view_to_handle(res_att_state->image_view);
            stencil_attachment.resolveImageLayout =
               att->resolve->stencil_layout;
         }
      }
   }

   for (uint32_t i = 0; i < subpass->input_count; i++) {
      const struct vk_subpass_attachment *att = &subpass->input_attachments[i];
      if (att->attachment == VK_ATTACHMENT_UNUSED)
         continue;

      assert(att->attachment < pass->attachment_count);
      struct vk_attachment_state *att_state =
         &cmd_buffer->attachments[att->attachment];

      if (subpass->view_mask & ~att_state->views_loaded) {
         load_store_attachment(cmd_buffer, att->attachment,
                               subpass->view_mask & ~att_state->views_loaded,
                               att->layout, att->stencil_layout);
      }
      att_state->views_loaded |= subpass->view_mask;
   }

   const VkRenderingInfo render_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = &subpass->self_dep_info,
      .renderArea = cmd_buffer->render_area,
      .layerCount = pass->is_multiview ? 1 : framebuffer->layers,
      .viewMask = pass->is_multiview ? subpass->view_mask : 0,
      .colorAttachmentCount = subpass->color_count,
      .pColorAttachments = color_attachments,
      .pDepthAttachment = &depth_attachment,
      .pStencilAttachment = &stencil_attachment,
   };
   disp->CmdBeginRendering(vk_command_buffer_to_handle(cmd_buffer),
                           &render_info);
}

static void
end_subpass(struct vk_command_buffer *cmd_buffer,
            const VkSubpassEndInfo *end_info)
{
   struct vk_device_dispatch_table *disp =
      &cmd_buffer->base.device->dispatch_table;
   disp->CmdEndRendering(vk_command_buffer_to_handle(cmd_buffer));
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdBeginRenderPass2(VkCommandBuffer commandBuffer,
                              const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                              const VkSubpassBeginInfo *pSubpassBeginInfo)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(vk_render_pass, pass, pRenderPassBeginInfo->renderPass);
   VK_FROM_HANDLE(vk_framebuffer, framebuffer,
                  pRenderPassBeginInfo->framebuffer);

   assert(cmd_buffer->render_pass == NULL);
   cmd_buffer->render_pass = pass;
   cmd_buffer->subpass_idx = 0;

   assert(cmd_buffer->framebuffer == NULL);
   cmd_buffer->framebuffer = framebuffer;

   cmd_buffer->render_area = pRenderPassBeginInfo->renderArea;

   assert(cmd_buffer->attachments == NULL);
   if (pass->attachment_count > ARRAY_SIZE(cmd_buffer->_attachments)) {
      cmd_buffer->attachments = malloc(pass->attachment_count *
                                       sizeof(*cmd_buffer->attachments));
   } else {
      cmd_buffer->attachments = cmd_buffer->_attachments;
   }

   const VkRenderPassAttachmentBeginInfoKHR *attach_begin =
      vk_find_struct_const(pRenderPassBeginInfo,
                           RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR);
   if (!attach_begin)
      assert(pass->attachment_count == framebuffer->attachment_count);

   const VkImageView *image_views;
   if (attach_begin && attach_begin->attachmentCount != 0) {
      assert(attach_begin->attachmentCount == pass->attachment_count);
      image_views = attach_begin->pAttachments;
   } else {
      assert(framebuffer->attachment_count >= pass->attachment_count);
      image_views = framebuffer->attachments;
   }

   for (uint32_t a = 0; a < pass->attachment_count; ++a) {
      VK_FROM_HANDLE(vk_image_view, image_view, image_views[a]);
      const struct vk_render_pass_attachment *pass_att = &pass->attachments[a];
      struct vk_attachment_state *att_state = &cmd_buffer->attachments[a];

      assert(util_last_bit(pass_att->view_mask) <= image_view->layer_count);

      *att_state = (struct vk_attachment_state) {
         .image_view = image_view,
         .views_loaded = 0,
      };

      for (uint32_t v = 0; v < MESA_VK_MAX_MULTIVIEW_VIEW_COUNT; v++) {
         att_state->views[v] = (struct vk_attachment_view_state) {
            .layout = pass_att->initial_layout,
            .stencil_layout = pass_att->initial_stencil_layout,
         };
      }

      if (a < pRenderPassBeginInfo->clearValueCount)
         att_state->clear_value = pRenderPassBeginInfo->pClearValues[a];
   }

   begin_subpass(cmd_buffer, pSubpassBeginInfo);
}

void
vk_command_buffer_reset_render_pass(struct vk_command_buffer *cmd_buffer)
{
   cmd_buffer->render_pass = NULL;
   cmd_buffer->subpass_idx = 0;
   cmd_buffer->framebuffer = NULL;
   if (cmd_buffer->attachments != cmd_buffer->_attachments)
      free(cmd_buffer->attachments);
   cmd_buffer->attachments = NULL;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdNextSubpass2(VkCommandBuffer commandBuffer,
                          const VkSubpassBeginInfo *pSubpassBeginInfo,
                          const VkSubpassEndInfo *pSubpassEndInfo)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);

   end_subpass(cmd_buffer, pSubpassEndInfo);
   cmd_buffer->subpass_idx++;
   begin_subpass(cmd_buffer, pSubpassBeginInfo);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdEndRenderPass2(VkCommandBuffer commandBuffer,
                            const VkSubpassEndInfo *pSubpassEndInfo)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   const struct vk_render_pass *pass = cmd_buffer->render_pass;
   struct vk_device_dispatch_table *disp =
      &cmd_buffer->base.device->dispatch_table;

   end_subpass(cmd_buffer, pSubpassEndInfo);

   /* From the Vulkan 1.3.204 spec:
    *
    *    "Automatic layout transitions apply to the entire image subresource
    *    attached to the framebuffer. If the attachment view is a 2D or 2D
    *    array view of a 3D image, even if the attachment view only refers to
    *    a subset of the slices of the selected mip level of the 3D image,
    *    automatic layout transitions apply to the entire subresource
    *    referenced which is the entire mip level in this case."
    *
    * We need to ensure that the entire bound subresource ends up in
    * finalLayout regardless of multiview or whether or not the attachment
    * was ever used.
    */
   uint32_t max_image_barrier_count = 0;
   for (uint32_t a = 0; a < pass->attachment_count; a++) {
      const struct vk_render_pass_attachment *pass_att = &pass->attachments[a];
      struct vk_attachment_state *att_state = &cmd_buffer->attachments[a];
      const struct vk_image_view *image_view = att_state->image_view;

      unsigned num_views = util_last_bit(pass_att->view_mask);
      if (!pass->is_multiview ||
          image_view->image->image_type == VK_IMAGE_TYPE_3D)
         num_views = 1;

      max_image_barrier_count += num_views * util_bitcount(image_view->aspects);
      if (pass->is_multiview &&
          image_view->image->image_type != VK_IMAGE_TYPE_3D &&
          image_view->layer_count > num_views)
         max_image_barrier_count += util_bitcount(image_view->aspects);
   }
   STACK_ARRAY(VkImageMemoryBarrier2, image_barriers, max_image_barrier_count);
   uint32_t image_barrier_count = 0;

   for (uint32_t a = 0; a < pass->attachment_count; a++) {
      const struct vk_render_pass_attachment *pass_att = &pass->attachments[a];
      struct vk_attachment_state *att_state = &cmd_buffer->attachments[a];
      const struct vk_image_view *image_view = att_state->image_view;

      unsigned num_views = util_last_bit(pass_att->view_mask);
      assert(num_views <= image_view->layer_count);

      /* For non-multiview or 3D images, handle the case where the attachment
       * was completely unused here.  For non-multiview, we handle it
       * specially below as part of the "more layers than used" case.
       */
      if (!pass->is_multiview ||
          image_view->image->image_type == VK_IMAGE_TYPE_3D)
         num_views = 1;

      transition_attachment_layout(cmd_buffer, a, BITFIELD_MASK(num_views),
                                   pass_att->final_layout,
                                   pass_att->final_stencil_layout,
                                   &image_barrier_count,
                                   max_image_barrier_count,
                                   image_barriers);

      /* For multiview, it's possible that the client has bound more array
       * layers than they've actually used as views.  In this case, we need
       * one more barrier to transition the remaining slices.  We don't for
       * 3D because those are already transitioned an entire miplevel at a
       * time.
       */
      if (pass->is_multiview &&
          image_view->image->image_type != VK_IMAGE_TYPE_3D &&
          image_view->layer_count > num_views) {
         VkImageSubresourceRange range = {
            .aspectMask = image_view->aspects,
            .baseMipLevel = image_view->base_mip_level,
            .levelCount = 1,
            .baseArrayLayer = num_views,
            .layerCount = image_view->layer_count - num_views,
         };
         transition_image_range(image_view, range,
                                pass_att->initial_layout,
                                pass_att->final_layout,
                                pass_att->initial_stencil_layout,
                                pass_att->final_stencil_layout,
                                &image_barrier_count,
                                max_image_barrier_count,
                                image_barriers);
      }
   }
   assert(image_barrier_count <= max_image_barrier_count);

   if (image_barrier_count > 0) {
      const VkDependencyInfo dependency_info = {
         .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
         .dependencyFlags = 0,
         .imageMemoryBarrierCount = image_barrier_count,
         .pImageMemoryBarriers = image_barriers,
      };
      disp->CmdPipelineBarrier2(vk_command_buffer_to_handle(cmd_buffer),
                                &dependency_info);
   }

   STACK_ARRAY_FINISH(image_barriers);

   vk_command_buffer_reset_render_pass(cmd_buffer);
}
