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
#ifndef VK_TIMELINE_H
#define VK_TIMELINE_H

#include "c11/threads.h"
#include "util/list.h"
#include "util/macros.h"

#include "vk_sync.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vk_timeline_type {
   struct vk_sync_type sync;

   /* Type of each individual time point */
   struct vk_sync_type *point_sync_type;
};

#define VK_DECL_TIMELINE_TYPE(__name, __point_sync_type) \
   static const struct vk_timeline_type __name = { \
      .sync = { \
         .size = sizeof(struct vk_timeline), \
         .init = vk_timeline_init, \
         .finish = vk_timeline_finish, \
         .signal = vk_timeline_signal, \
         .get_value = vk_timeline_get_value, \
         .wait = vk_timeline_all, \
      }, \
      .point_sync_type = __point_sync_type, \
   }

struct vk_timeline_point {
   struct list_head link;

   uint64_t value;

   /* Number of waiter on this point, when > 0 the point should not be garbage
    * collected.
    */
   int waiting;

   struct vk_sync sync;
};

struct vk_timeline {
   struct vk_sync sync;

   mtx_t mutex;
   cnd_t cond;

   uint64_t highest_past;
   uint64_t highest_pending;

   struct list_head points;
   struct list_head free_points;
};

VkResult vk_timeline_init(struct vk_device *device,
                          struct vk_sync *sync,
                          uint64_t initial_value);

void vk_timeline_finish(struct vk_device *device,
                        struct vk_sync *sync);

VkResult vk_timeline_add_point(struct vk_device *device,
                               struct vk_timeline *timeline,
                               uint64_t value,
                               struct vk_timeline_point **point_out);

VkResult vk_timeline_get_point(struct vk_device *device,
                               struct vk_timeline *timeline,
                               uint64_t wait_value,
                               struct vk_timeline_point **point_out);

VkResult vk_timeline_signal(struct vk_device *device,
                            struct vk_sync *sync,
                            uint64_t value);

VkResult vk_timeline_get_value(struct vk_device *device,
                               struct vk_sync *sync,
                               uint64_t *value);

VkResult vk_timeline_wait(struct vk_device *device,
                          struct vk_sync *sync,
                          uint64_t wait_value,
                          enum vk_sync_wait_type wait_type,
                          uint64_t abs_timeout_ns);

static inline struct vk_timeline *
vk_sync_as_timeline(struct vk_sync *sync)
{
   if (sync->type->init != vk_timeline_init)
      return NULL;

   return container_of(sync, struct vk_timeline, sync);
}

#ifdef __cplusplus
}
#endif

#endif /* VK_TIMELINE_H */
