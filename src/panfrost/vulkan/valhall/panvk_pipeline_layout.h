/*
 * Copyright © 2024 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_PIPELINE_LAYOUT_H
#define PANVK_PIPELINE_LAYOUT_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vk_pipeline_layout.h"

#define MAX_SETS                    16
#define MAX_DYNAMIC_UNIFORM_BUFFERS 16
#define MAX_DYNAMIC_STORAGE_BUFFERS 8
#define MAX_DYNAMIC_BUFFERS                                                    \
   (MAX_DYNAMIC_UNIFORM_BUFFERS + MAX_DYNAMIC_STORAGE_BUFFERS)

struct panvk_pipeline_layout {
   struct vk_pipeline_layout vk;
   unsigned char sha1[20];
   unsigned num_dyn_bufs;

   struct {
      uint32_t size;
   } push_constants;

   struct {
      unsigned dyn_buf_offset;
   } sets[MAX_SETS];
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_pipeline_layout, vk.base, VkPipelineLayout,
                               VK_OBJECT_TYPE_PIPELINE_LAYOUT)

#endif /* PANVK_PIPELINE_LAYOUT_H */
