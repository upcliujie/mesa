/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#ifndef VN_IMAGE_H
#define VN_IMAGE_H

#include "vn_common.h"

enum vn_image_wsi_comamnd_type {
   VN_IMAGE_WSI_COMMAND_ACQUIRE,
   VN_IMAGE_WSI_COMMAND_RELEASE,

   VN_IMAGE_WSI_COMMAND_COUNT,
};

struct vn_image_wsi {
   uint32_t queue_family_count;

   /* These are optional.  When non-NULL, they indicate that the command pools
    * are shared by all swapchains of the device.  The command buffers must be
    * explicitly freed.
    *
    * When NULL, the command buffers must NOT be explicitly freed.
    */
   const VkCommandPool *command_pools;
   mtx_t *command_pool_mutex;

   /* the queue the image is last presented on */
   struct vn_queue *last_present_queue;

   /* For queue family ownership transfer of WSI images */
   VkCommandBuffer command_buffers[][VN_IMAGE_WSI_COMMAND_COUNT];
};

struct vn_image {
   struct vn_object_base base;

   VkSharingMode sharing_mode;

   VkMemoryRequirements2 memory_requirements[4];
   VkMemoryDedicatedRequirements dedicated_requirements[4];

   /* For VK_ANDROID_native_buffer, the WSI image owns the memory, */
   VkDeviceMemory private_memory;

   struct vn_image_wsi *wsi;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_image,
                               base.base,
                               VkImage,
                               VK_OBJECT_TYPE_IMAGE)

struct vn_image_view {
   struct vn_object_base base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_image_view,
                               base.base,
                               VkImageView,
                               VK_OBJECT_TYPE_IMAGE_VIEW)

struct vn_sampler {
   struct vn_object_base base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_sampler,
                               base.base,
                               VkSampler,
                               VK_OBJECT_TYPE_SAMPLER)

struct vn_sampler_ycbcr_conversion {
   struct vn_object_base base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_sampler_ycbcr_conversion,
                               base.base,
                               VkSamplerYcbcrConversion,
                               VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION)

VkResult
vn_image_create(struct vn_device *dev,
                const VkImageCreateInfo *create_info,
                const VkAllocationCallbacks *alloc,
                struct vn_image **out_img);

VkResult
vn_image_init_wsi(struct vn_device *dev,
                  struct vn_image *img,
                  uint32_t queue_family_count,
                  const VkAllocationCallbacks *alloc);

VkResult
vn_image_record_wsi_commands(struct vn_device *dev,
                             struct vn_image *img,
                             const VkCommandPool *pools,
                             mtx_t *pool_mutex,
                             const VkAllocationCallbacks *alloc);

static inline const VkCommandBuffer *
vn_image_get_wsi_command(const struct vn_image *img,
                         uint32_t queue_family_index,
                         enum vn_image_wsi_comamnd_type type)
{
   assert(img->wsi && queue_family_index < img->wsi->queue_family_count &&
          type < VN_IMAGE_WSI_COMMAND_COUNT);
   return &img->wsi->command_buffers[queue_family_index][type];
}

#endif /* VN_IMAGE_H */
