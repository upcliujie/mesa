/*
 * Copyright © 2017 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef WSI_COMMON_CMD_H
#define WSI_COMMON_CMD_H

#include "wsi_common.h"

struct wsi_swapchain;
struct wsi_image;

VkResult
wsi_create_image_cmd_buffers(const struct wsi_swapchain *chain,
                             struct wsi_image *image,
                             const VkImageCreateInfo *image_info,
                             uint32_t present_blit_buffer_width);

void
wsi_destroy_image_cmd_buffers(const struct wsi_swapchain *chain,
                              struct wsi_image *image);

#endif /* WSI_COMMON_CMD_H */
