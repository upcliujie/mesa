/*
 * Copyright © 2020 Intel Corporation
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
#ifndef VK_SYNC_H
#define VK_SYNC_H

#include <stdbool.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vk_device;
struct vk_sync;

enum vk_sync_wait_type {
   VK_SYNC_WAIT_PENDING,
   VK_SYNC_WAIT_COMPLETE,
};

struct vk_sync_wait;

struct vk_sync_type {
   /** Size of this sync type */
   size_t size;

   /** True if this is a timeline sync type */
   bool is_timeline;

   VkResult (*init)(struct vk_device *device,
                    struct vk_sync *sync,
                    uint64_t initial_value);

   void (*finish)(struct vk_device *device,
                  struct vk_sync *sync);

   VkResult (*signal)(struct vk_device *device,
                      struct vk_sync *sync,
                      uint64_t value);

   VkResult (*get_value)(struct vk_device *device,
                         struct vk_sync *sync,
                         uint64_t *value);

   VkResult (*reset)(struct vk_device *device,
                     struct vk_sync *sync);

   VkResult (*wait)(struct vk_device *device,
                    struct vk_sync *sync,
                    uint64_t wait_value,
                    enum vk_sync_wait_type wait_type,
                    uint64_t abs_timeout_ns);

   VkResult (*wait_all)(struct vk_device *device,
                        uint32_t wait_count,
                        const struct vk_sync_wait *waits,
                        enum vk_sync_wait_type wait_type,
                        uint64_t abs_timeout_ns);

   VkResult (*wait_any)(struct vk_device *device,
                        uint32_t wait_count,
                        const struct vk_sync_wait *waits,
                        enum vk_sync_wait_type wait_type,
                        uint64_t abs_timeout_ns);

   VkResult (*import_opaque_fd)(struct vk_device *device,
                                struct vk_sync *sync,
                                int fd);

   VkResult (*export_opaque_fd)(struct vk_device *device,
                                struct vk_sync *sync,
                                int *fd);

   VkResult (*import_sync_file)(struct vk_device *device,
                                struct vk_sync *sync,
                                int sync_file);

   VkResult (*export_sync_file)(struct vk_device *device,
                                struct vk_sync *sync,
                                int *sync_file);

   VkResult (*move)(struct vk_device *device,
                    struct vk_sync *dst,
                    struct vk_sync *src);
};

static inline bool
vk_sync_type_has_cpu_wait(const struct vk_sync_type *type)
{
   return type->wait || type->wait_all;
}

struct vk_sync {
   const struct vk_sync_type *type;
};

/* See VkSemaphoreSubmitInfoKHR */
struct vk_sync_wait {
   struct vk_sync *sync;
   VkPipelineStageFlags2KHR stage_mask;
   uint64_t wait_value;
};

VkResult vk_sync_init(struct vk_device *device,
                      struct vk_sync *sync,
                      const struct vk_sync_type *type,
                      uint64_t initial_value);

void vk_sync_finish(struct vk_device *device,
                    struct vk_sync *sync);

VkResult vk_sync_create(struct vk_device *device,
                        const struct vk_sync_type *type,
                        uint64_t initial_value,
                        struct vk_sync **sync_out);

void vk_sync_destroy(struct vk_device *device,
                     struct vk_sync *sync);

VkResult vk_sync_signal(struct vk_device *device,
                        struct vk_sync *sync,
                        uint64_t value);

VkResult vk_sync_get_value(struct vk_device *device,
                           struct vk_sync *sync,
                           uint64_t *value);

VkResult vk_sync_reset(struct vk_device *device,
                       struct vk_sync *sync);

VkResult vk_sync_wait(struct vk_device *device,
                      struct vk_sync *sync,
                      uint64_t wait_value,
                      enum vk_sync_wait_type wait_type,
                      uint64_t abs_timeout_ns);

VkResult vk_sync_wait_all(struct vk_device *device,
                          uint32_t wait_count,
                          const struct vk_sync_wait *waits,
                          enum vk_sync_wait_type wait_type,
                          uint64_t abs_timeout_ns);

VkResult vk_sync_wait_any(struct vk_device *device,
                          uint32_t wait_count,
                          const struct vk_sync_wait *waits,
                          enum vk_sync_wait_type wait_type,
                          uint64_t abs_timeout_ns);

VkResult vk_sync_import_opaque_fd(struct vk_device *device,
                                  struct vk_sync *sync,
                                  int fd);

VkResult vk_sync_export_opaque_fd(struct vk_device *device,
                                  struct vk_sync *sync,
                                  int *fd);

VkResult vk_sync_import_sync_file(struct vk_device *device,
                                  struct vk_sync *sync,
                                  int sync_file);

VkResult vk_sync_export_sync_file(struct vk_device *device,
                                  struct vk_sync *sync,
                                  int *sync_file);

VkResult vk_sync_move(struct vk_device *device,
                      struct vk_sync *dst,
                      struct vk_sync *src);

#ifdef __cplusplus
}
#endif

#endif /* VK_SYNC_H */
