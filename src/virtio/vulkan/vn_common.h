/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#ifndef VN_COMMON_H
#define VN_COMMON_H

#include <assert.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "c11/threads.h"
#include "util/list.h"
#include "util/macros.h"
#include "vk_alloc.h"
#include "vk_debug_report.h"
#include "vk_object.h"
#include "vk_util.h"

#include "vn_entrypoints.h"
#include "vn_extensions.h"

#define VN_DEFAULT_ALIGN 8

#define VN_DEBUG(category) unlikely(vn_debug &VN_DEBUG_##category)

#define vn_error(instance, error)                                            \
   (VN_DEBUG(RESULT) ? vn_log_result((instance), (error), __func__) : (error))
#define vn_result(instance, result)                                          \
   ((result) >= VK_SUCCESS ? (result) : vn_error((instance), (result)))

struct vn_instance;
struct vn_physical_device;
struct vn_device;
struct vn_queue;
struct vn_fence;
struct vn_semaphore;
struct vn_device_memory;
struct vn_buffer;
struct vn_buffer_view;
struct vn_image;
struct vn_image_view;
struct vn_sampler;
struct vn_sampler_ycbcr_conversion;
struct vn_descriptor_set_layout;
struct vn_descriptor_pool;
struct vn_descriptor_set;
struct vn_descriptor_update_template;
struct vn_render_pass;
struct vn_framebuffer;
struct vn_event;
struct vn_query_pool;
struct vn_shader_module;
struct vn_pipeline_layout;
struct vn_pipeline_cache;
struct vn_pipeline;
struct vn_command_pool;
struct vn_command_buffer;

struct vn_cs;
struct vn_renderer;
struct vn_renderer_bo;
struct vn_renderer_sync;

enum vn_debug {
   VN_DEBUG_INIT = 1ull << 0,
   VN_DEBUG_RESULT = 1ull << 1,
   VN_DEBUG_VTEST = 1ull << 2,
   VN_DEBUG_WSI = 1ull << 3,
};

extern uint64_t vn_debug;
extern const VkAllocationCallbacks vn_default_allocator;

void
vn_debug_init(void);

void
vn_log(struct vn_instance *instance, const char *format, ...)
   PRINTFLIKE(2, 3);

VkResult
vn_log_result(struct vn_instance *instance,
              VkResult result,
              const char *where);

/* missing from vn_entrypoints.h */

bool
vn_instance_entrypoint_is_enabled(
   int index,
   uint32_t core_version,
   const struct vn_instance_extension_table *instance);

bool
vn_physical_device_entrypoint_is_enabled(
   int index,
   uint32_t core_version,
   const struct vn_instance_extension_table *instance);

bool
vn_device_entrypoint_is_enabled(
   int index,
   uint32_t core_version,
   const struct vn_instance_extension_table *instance,
   const struct vn_device_extension_table *device);

int
vn_get_instance_entrypoint_index(const char *name);

int
vn_get_physical_device_entrypoint_index(const char *name);

int
vn_get_device_entrypoint_index(const char *name);

const char *
vn_get_instance_entry_name(int index);

const char *
vn_get_physical_device_entry_name(int index);

const char *
vn_get_device_entry_name(int index);

void *
vn_lookup_entrypoint(const char *name);

/* missing from vn_extensions.h */

uint32_t
vn_physical_device_api_version(struct vn_physical_device *physical_dev);

#endif /* VN_COMMON_H */
