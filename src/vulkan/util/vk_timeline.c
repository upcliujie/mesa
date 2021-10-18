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

#include "vk_timeline.h"

#include <inttypes.h>

#include "util/os_time.h"

#include "vk_alloc.h"
#include "vk_device.h"
#include "vk_log.h"

static struct vk_timeline *
to_vk_timeline(struct vk_sync *sync)
{
   assert(sync->type->init == vk_timeline_init);

   return container_of(sync, struct vk_timeline, sync);
}

VkResult
vk_timeline_init(struct vk_device *device,
                 struct vk_sync *sync,
                 uint64_t initial_value)
{
   struct vk_timeline *timeline = to_vk_timeline(sync);
   int ret;

   ASSERTED const struct vk_timeline_type *ttype =
      container_of(timeline->sync.type, struct vk_timeline_type, sync);
   assert(vk_sync_type_has_cpu_wait(ttype->point_sync_type));

   ret = mtx_init(&timeline->mutex, mtx_plain);
   if (ret != thrd_success)
      return vk_errorf(device, VK_ERROR_UNKNOWN, "mtx_init failed");

   ret = cnd_init(&timeline->cond);
   if (ret != thrd_success) {
      mtx_destroy(&timeline->mutex);
      return vk_errorf(device, VK_ERROR_UNKNOWN, "cnd_init failed");
   }

   timeline->highest_past =
      timeline->highest_pending = initial_value;
   list_inithead(&timeline->points);
   list_inithead(&timeline->free_points);

   return VK_SUCCESS;
}

void
vk_timeline_finish(struct vk_device *device,
                   struct vk_sync *sync)
{
   struct vk_timeline *timeline = to_vk_timeline(sync);

   list_for_each_entry_safe(struct vk_timeline_point, point,
                            &timeline->free_points, link) {
      list_del(&point->link);
      vk_sync_finish(device, &point->sync);
      vk_free(&device->alloc, point);
   }
   list_for_each_entry_safe(struct vk_timeline_point, point,
                            &timeline->points, link) {
      list_del(&point->link);
      vk_sync_finish(device, &point->sync);
      vk_free(&device->alloc, point);
   }

   cnd_destroy(&timeline->cond);
   mtx_destroy(&timeline->mutex);
}

static VkResult
vk_timeline_gc_locked(struct vk_device *device,
                      struct vk_timeline *timeline)
{
   list_for_each_entry_safe(struct vk_timeline_point, point,
                            &timeline->points, link) {
      /* timeline->higest_pending is only incremented once submission has
       * happened. If this point has a greater serial, it means the point
       * hasn't been submitted yet.
       */
      if (point->value > timeline->highest_pending)
         return VK_SUCCESS;

      /* If someone is waiting on this time point, consider it busy and don't
       * try to recycle it. There's a slim possibility that it's no longer
       * busy by the time we look at it but we would be recycling it out from
       * under a waiter and that can lead to weird races.
       *
       * We walk the list in-order so if this time point is still busy so is
       * every following time point
       */
      assert(point->waiting >= 0);
      if (point->waiting)
         return VK_SUCCESS;

      /* Garbage collect any signaled point. */
      VkResult result = vk_sync_wait(device, &point->sync, 0,
                                     VK_SYNC_WAIT_COMPLETE,
                                     0 /* abs_timeout_ns */);
      if (result == VK_TIMEOUT) {
         /* We walk the list in-order so if this time point is still busy so
          * is every following time point
          */
         return VK_SUCCESS;
      } else if (result != VK_SUCCESS) {
         return result;
      }

      assert(timeline->highest_past < point->value);
      timeline->highest_past = point->value;

      list_del(&point->link);
      list_add(&point->link, &timeline->free_points);
   }

   return VK_SUCCESS;
}

static VkResult
vk_timeline_alloc_point_locked(struct vk_device *device,
                               struct vk_timeline *timeline,
                               struct vk_timeline_point **point_out)
{
   struct vk_timeline_point *point;
   VkResult result;

   result = vk_timeline_gc_locked(device, timeline);
   if (unlikely(result != VK_SUCCESS))
      return result;

   if (list_is_empty(&timeline->free_points)) {
      const struct vk_timeline_type *ttype =
         container_of(timeline->sync.type, struct vk_timeline_type, sync);
      const struct vk_sync_type *point_sync_type = ttype->point_sync_type;

      size_t size = offsetof(struct vk_timeline_point, sync) +
                    point_sync_type->size;

      point = vk_zalloc(&device->alloc, size, 8,
                        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
      if (!point)
         return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

      point->timeline = timeline;

      result = vk_sync_init(device, &point->sync, point_sync_type, 0);
      if (unlikely(result != VK_SUCCESS)) {
         vk_free(&device->alloc, point);
         return result;
      }
   } else {
      point = list_first_entry(&timeline->free_points,
                               struct vk_timeline_point, link);

      if (point->sync.type->reset) {
         result = vk_sync_reset(device, &point->sync);
         if (unlikely(result != VK_SUCCESS))
            return result;
      }

      list_del(&point->link);
   }

   *point_out = point;

   return VK_SUCCESS;
}

VkResult
vk_timeline_alloc_point(struct vk_device *device,
                        struct vk_timeline *timeline,
                        struct vk_timeline_point **point_out)
{
   VkResult result;

   mtx_lock(&timeline->mutex);
   result = vk_timeline_alloc_point_locked(device, timeline, point_out);
   mtx_unlock(&timeline->mutex);

   return result;
}

void
vk_timeline_point_free(struct vk_device *device,
                       struct vk_timeline_point *point)
{
   struct vk_timeline *timeline = point->timeline;

   mtx_lock(&timeline->mutex);
   list_add(&point->link, &timeline->free_points);
   mtx_unlock(&timeline->mutex);
}

VkResult
vk_timeline_point_install(struct vk_device *device,
                          struct vk_timeline_point *point,
                          uint64_t value)
{
   struct vk_timeline *timeline = point->timeline;

   mtx_lock(&timeline->mutex);

   assert(value > timeline->highest_pending);
   timeline->highest_pending = value;

   point->value = value;
   assert(point->waiting == 0);
   list_addtail(&point->link, &timeline->points);

   int ret = cnd_broadcast(&timeline->cond);

   mtx_unlock(&timeline->mutex);

   if (ret == thrd_error)
      return vk_errorf(device, VK_ERROR_UNKNOWN, "cnd_broadcast failed");

   return VK_SUCCESS;
}

static VkResult
vk_timeline_get_point_locked(struct vk_device *device,
                             struct vk_timeline *timeline,
                             uint64_t wait_value,
                             struct vk_timeline_point **point_out)
{
   if (timeline->highest_past >= wait_value) {
      /* Nothing to wait on */
      *point_out = NULL;
      return VK_SUCCESS;
   }

   list_for_each_entry(struct vk_timeline_point, point,
                       &timeline->points, link) {
      if (point->value >= wait_value) {
         point->waiting++;
         *point_out = point;
         return VK_SUCCESS;
      }
   }

   return vk_errorf(device, VK_ERROR_UNKNOWN,
                    "Time point >= %"PRIu64" not found", wait_value);
}

VkResult
vk_timeline_get_point(struct vk_device *device,
                      struct vk_timeline *timeline,
                      uint64_t wait_value,
                      struct vk_timeline_point **point_out)
{
   mtx_lock(&timeline->mutex);
   VkResult result = vk_timeline_get_point_locked(device, timeline,
                                                  wait_value, point_out);
   mtx_unlock(&timeline->mutex);

   return result;
}

void
vk_timeline_point_release(struct vk_device *device,
                          struct vk_timeline_point *point)
{
   struct vk_timeline *timeline = point->timeline;

   mtx_lock(&timeline->mutex);
   point->waiting--;
   mtx_unlock(&timeline->mutex);
}

VkResult
vk_timeline_signal(struct vk_device *device,
                   struct vk_sync *sync,
                   uint64_t value)
{
   struct vk_timeline *timeline = to_vk_timeline(sync);

   mtx_lock(&timeline->mutex);

   VkResult result = vk_timeline_gc_locked(device, timeline);
   if (unlikely(result != VK_SUCCESS)) {
      mtx_unlock(&timeline->mutex);
      return result;
   }

   assert(value > timeline->highest_pending);
   timeline->highest_pending = timeline->highest_past = value;

   int ret = cnd_broadcast(&timeline->cond);
   mtx_unlock(&timeline->mutex);

   if (ret == thrd_error)
      return vk_errorf(device, VK_ERROR_UNKNOWN, "cnd_broadcast failed");

   return VK_SUCCESS;
}

VkResult vk_timeline_get_value(struct vk_device *device,
                               struct vk_sync *sync,
                               uint64_t *value)
{
   struct vk_timeline *timeline = to_vk_timeline(sync);

   mtx_lock(&timeline->mutex);
   VkResult result = vk_timeline_gc_locked(device, timeline);
   mtx_unlock(&timeline->mutex);

   if (result != VK_SUCCESS)
      return result;

   *value = timeline->highest_past;

   return VK_SUCCESS;
}

#define NSEC_PER_SEC 1000000000ull

static VkResult
vk_timeline_wait_locked(struct vk_device *device,
                        struct vk_timeline *timeline,
                        uint64_t wait_value,
                        enum vk_sync_wait_type wait_type,
                        uint64_t abs_timeout_ns)
{
   /* Wait on the queue_submit condition variable until the timeline has a
    * time point pending that's at least as high as wait_value.
    */
   while (timeline->highest_pending < wait_value) {
      struct timespec abstime = {
         .tv_sec = abs_timeout_ns / NSEC_PER_SEC,
         .tv_nsec = abs_timeout_ns % NSEC_PER_SEC,
      };

      int ret = cnd_timedwait(&timeline->cond, &timeline->mutex, &abstime);
      if (ret == thrd_error)
         return vk_errorf(device, VK_ERROR_UNKNOWN, "cnd_broadcast failed");

      if ((os_time_get_nano() >= abs_timeout_ns) &&
          timeline->highest_pending < wait_value)
         return VK_TIMEOUT;
   }

   if (wait_type == VK_SYNC_WAIT_PENDING)
      return VK_SUCCESS;

   while (1) {
      VkResult result = vk_timeline_gc_locked(device, timeline);
      if (result != VK_SUCCESS)
         return result;

      if (timeline->highest_past >= wait_value)
         return VK_SUCCESS;

      /* If we got here, our earliest time point has a busy vk_sync */
      struct vk_timeline_point *point =
         list_first_entry(&timeline->points,
                          struct vk_timeline_point, link);

      /* Drop the lock while we wait. */
      point->waiting++;
      mtx_unlock(&timeline->mutex);

      result = vk_sync_wait(device, &point->sync, 0,
                            VK_SYNC_WAIT_COMPLETE,
                            abs_timeout_ns);

      /* Pick the mutex back up */
      mtx_lock(&timeline->mutex);
      point->waiting--;

      /* This covers both VK_TIMEOUT and VK_ERROR_DEVICE_LOST */
      if (result != VK_SUCCESS)
         return result;
   }
}

VkResult
vk_timeline_wait(struct vk_device *device,
                 struct vk_sync *sync,
                 uint64_t wait_value,
                 enum vk_sync_wait_type wait_type,
                 uint64_t abs_timeout_ns)
{
   struct vk_timeline *timeline = to_vk_timeline(sync);

   mtx_lock(&timeline->mutex);
   VkResult result = vk_timeline_wait_locked(device, timeline,
                                             wait_value, wait_type,
                                             abs_timeout_ns);
   mtx_unlock(&timeline->mutex);

   return result;
}
