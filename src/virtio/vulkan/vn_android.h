/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#ifndef VN_ANDROID_H
#define VN_ANDROID_H

#include "vn_common.h"

#include <vulkan/vk_android_native_buffer.h>
#include <vulkan/vulkan.h>

/* venus implements VK_ANDROID_native_buffer up to spec version 7 */
#define VN_ANDROID_NATIVE_BUFFER_SPEC_VERSION 7

#ifdef ANDROID

static inline const VkNativeBufferANDROID *
vn_android_find_native_buffer(const VkImageCreateInfo *create_info)
{
   return vk_find_struct_const(create_info->pNext, NATIVE_BUFFER_ANDROID);
}

VkResult
vn_image_from_anb(struct vn_device *dev,
                  const VkImageCreateInfo *image_info,
                  const VkNativeBufferANDROID *anb_info,
                  const VkAllocationCallbacks *alloc,
                  struct vn_image **out_img);

#else

static inline const VkNativeBufferANDROID *
vn_android_find_native_buffer(const VkImageCreateInfo *create_info)
{
   return NULL;
}

static inline VkResult
vn_image_from_anb(struct vn_device *dev,
                  const VkImageCreateInfo *image_info,
                  const VkNativeBufferANDROID *anb_info,
                  const VkAllocationCallbacks *alloc,
                  struct vn_image **out_img)
{
   return VK_ERROR_OUT_OF_HOST_MEMORY;
}

#endif /* ANDROID */

#endif /* VN_ANDROID_H */
