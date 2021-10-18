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

   /** Initialize a vk_sync
    *
    * The base vk_sync will already be initialized and the sync type set
    * before this function is called.  If any OS primitives need to be
    * allocated, that should be done here.
    */
   VkResult (*init)(struct vk_device *device,
                    struct vk_sync *sync,
                    uint64_t initial_value);

   /** Finish a vk_sync
    *
    * This should free any internal data stored in this vk_sync.
    */
   void (*finish)(struct vk_device *device,
                  struct vk_sync *sync);

   /** Signal a vk_sync
    *
    * For non-timeline sync types, value == 0.
    */
   VkResult (*signal)(struct vk_device *device,
                      struct vk_sync *sync,
                      uint64_t value);

   /** Get the timeline value for a vk_sync */
   VkResult (*get_value)(struct vk_device *device,
                         struct vk_sync *sync,
                         uint64_t *value);

   /** Reset a non-timeline vk_sync */
   VkResult (*reset)(struct vk_device *device,
                     struct vk_sync *sync);

   /** Wait on a vk_sync
    *
    * For a timeline vk_sync, wait_value is the timeline value to wait for.
    * This function should not return VK_SUCCESS until get_value on that
    * vk_sync would return a value >= wait_value.  A wait_value of zero is
    * allowed in which case the wait is a no-op.  For a non-timeline vk_sync,
    * wait_value should be ignored.
    *
    * This function is optional.  If the sync type needs to support CPU waits,
    * one of wait or wait_all must be provided.  If any of wait, wait_all, or
    * wait_any are missing, they will be implemented in terms of wait or
    * wait_all.
    */
   VkResult (*wait)(struct vk_device *device,
                    struct vk_sync *sync,
                    uint64_t wait_value,
                    enum vk_sync_wait_type wait_type,
                    uint64_t abs_timeout_ns);

   /** Wait for all the given vk_sync events
    *
    * See wait for more details.
    */
   VkResult (*wait_all)(struct vk_device *device,
                        uint32_t wait_count,
                        const struct vk_sync_wait *waits,
                        enum vk_sync_wait_type wait_type,
                        uint64_t abs_timeout_ns);

   /** Wait for all one of the given vk_sync events
    *
    * See wait for more details.
    */
   VkResult (*wait_any)(struct vk_device *device,
                        uint32_t wait_count,
                        const struct vk_sync_wait *waits,
                        enum vk_sync_wait_type wait_type,
                        uint64_t abs_timeout_ns);

   /** Permanently imports the given FD into this vk_sync
    *
    * This replaces the guts of the given vk_sync with whatever is in the FD.
    * In a sense, this vk_sync now aliases whatever vk_sync the FD was
    * exported from.
    */
   VkResult (*import_opaque_fd)(struct vk_device *device,
                                struct vk_sync *sync,
                                int fd);

   /** Export the guts of this vk_sync to an FD */
   VkResult (*export_opaque_fd)(struct vk_device *device,
                                struct vk_sync *sync,
                                int *fd);

   /** Imports a sync file into this binary vk_sync
    *
    * If this completes successfully, the vk_sync will now signal whenever
    * the sync file signals.
    *
    * If sync_file == -1, the vk_sync should be signaled immediately.  If
    * the vk_sync_type implements signal, sync_file will never be -1.
    */
   VkResult (*import_sync_file)(struct vk_device *device,
                                struct vk_sync *sync,
                                int sync_file);

   /** Exports the current binary vk_sync state as a sync file.
    *
    * The resulting sync file will contain the current event stored in this
    * binary vk_sync must be turned into a sync file.  If the vk_sync is later
    * modified to contain a new event, the sync file is unaffected.
    */
   VkResult (*export_sync_file)(struct vk_device *device,
                                struct vk_sync *sync,
                                int *sync_file);

   /** Moves the guts of one binary vk_sync to another
    *
    * This moves the current binary vk_sync event from src to dst and resets
    * src.  If dst contained an event, it is discarded.
    *
    * This function is optional if you implement import/export_sync_file.
    */
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
