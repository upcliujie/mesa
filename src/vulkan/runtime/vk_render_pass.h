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
#ifndef VK_RENDER_PASS_H
#define VK_RENDER_PASS_H

#include "vk_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pseudo-extension struct that may be chained into VkRenderingInfo,
 * VkCommandBufferInheritanceRenderingInfo, or VkPipelineRenderingCreateInfo
 * to provide self-dependency information.
 */
typedef struct VkRenderingSelfDependencyInfoMESA {
    VkStructureType    sType;
#define VK_STRUCTURE_TYPE_RENDERING_SELF_DEPENDENCY_INFO_MESA (VkStructureType)1000044900
    const void*        pNext;
    uint32_t           colorSelfDependencies;
    VkBool32           depthSelfDependency;
    VkBool32           stencilSelfDependency;
} VkRenderingSelfDependencyInfoMESA;

/**
 * Pseudo-extension struct that may be chained into VkRenderingAttachmentInfo
 * to indicate an initial layout for the attachment.  This is only allowed if
 * all of the following conditions are met:
 *
 *    1. VkRenderingAttachmentInfo::loadOp == LOAD_OP_CLEAR
 *
 *    2. VkRenderingInfo::renderArea is tne entire image view LOD
 *
 *    3. VkRenderingInfo::viewMask == 0 AND VkRenderingInfo::layerCount
 *       references the entire bound image view OR VkRenderingInfo::viewMask
 *       is dense (no holes) and references the entire bound image view.
 *
 * The only allowed value for initialLayout is VK_IMAGE_LAYOUT_UNDEFINED.
 */
typedef struct VkRenderingAttachmentInitialLayoutInfoMESA {
    VkStructureType    sType;
#define VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INITIAL_LAYOUT_INFO_MESA (VkStructureType)1000044901
    const void*        pNext;
    VkImageLayout      initialLayout;
} VkRenderingAttachmentInitialLayoutInfoMESA;

struct vk_subpass_attachment {
   uint32_t attachment;
   VkImageAspectFlags aspects;
   VkImageUsageFlagBits usage;
   VkImageLayout layout;
   VkImageLayout stencil_layout;

   /** A per-view mask for if this is the last use of this attachment
    *
    * If the same render pass attachment is used multiple ways within a
    * subpass, corresponding last_subpass bits will be set in all of them.
    * For the non-multiview case, only the first bit is used.
    */
   uint32_t last_subpass;

   struct vk_subpass_attachment *resolve;
};

struct vk_subpass {
   uint32_t attachment_count;
   struct vk_subpass_attachment *attachments;

   uint32_t input_count;
   struct vk_subpass_attachment *input_attachments;

   uint32_t color_count;
   struct vk_subpass_attachment *color_attachments;

   uint32_t color_resolve_count;
   struct vk_subpass_attachment *color_resolve_attachments;

   struct vk_subpass_attachment *depth_stencil_attachment;
   struct vk_subpass_attachment *depth_stencil_resolve_attachment;
   struct vk_subpass_attachment *fragment_shading_rate_attachment;

   /** VkSubpassDescription2::viewMask or 1 for non-multiview
    *
    * For all view masks in the vk_render_pass data structure, we use a mask
    * of 1 for non-multiview instead of a mask of 0.  To determine if the
    * render pass is multiview or not, see vk_render_pass::is_multiview.
    */
   uint32_t view_mask;

   VkResolveModeFlagBitsKHR depth_resolve_mode;
   VkResolveModeFlagBitsKHR stencil_resolve_mode;

   VkExtent2D fragment_shading_rate_attachment_texel_size;

   VkRenderingSelfDependencyInfoMESA self_dep_info;
   VkPipelineRenderingCreateInfo pipeline_info;
   VkCommandBufferInheritanceRenderingInfo inheritance_info;
};

struct vk_render_pass_attachment {
   VkFormat format;
   VkImageAspectFlags aspects;
   uint32_t samples;

   uint32_t view_mask;

   VkAttachmentLoadOp load_op;
   VkAttachmentStoreOp store_op;
   VkAttachmentLoadOp stencil_load_op;
   VkAttachmentLoadOp stencil_store_op;

   VkImageLayout initial_layout;
   VkImageLayout final_layout;
   VkImageLayout initial_stencil_layout;
   VkImageLayout final_stencil_layout;
};

struct vk_subpass_dependency {
   VkDependencyFlags flags;

   uint32_t src_subpass;
   uint32_t dst_subpass;
   VkPipelineStageFlags2 src_stage_mask;
   VkPipelineStageFlags2 dst_stage_mask;
   VkAccessFlags2 src_access_mask;
   VkAccessFlags2 dst_access_mask;
   int32_t view_offset;
};

struct vk_render_pass {
   struct vk_object_base base;

   bool is_multiview;

   uint32_t attachment_count;
   struct vk_render_pass_attachment *attachments;

   uint32_t subpass_count;
   struct vk_subpass *subpasses;

   uint32_t dependency_count;
   struct vk_subpass_dependency *dependencies;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(vk_render_pass, base, VkRenderPass,
                               VK_OBJECT_TYPE_RENDER_PASS)

const VkPipelineRenderingCreateInfo *
vk_get_pipeline_rendering_create_info(const VkGraphicsPipelineCreateInfo *info);

const VkCommandBufferInheritanceRenderingInfo *
vk_get_command_buffer_inheritance_rendering_info(
   VkCommandBufferLevel level,
   const VkCommandBufferBeginInfo *pBeginInfo);

#ifdef __cplusplus
}
#endif

#endif /* VK_RENDER_PASS_H */
