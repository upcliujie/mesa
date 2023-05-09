/*
 * Copyright © 2022 Google LLC
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <fcntl.h>
#include <poll.h>
#include <xf86drm.h>

#include "util/bitscan.h"
#include "util/macros.h"

#define VIRGL_RENDERER_UNSTABLE_APIS 1
#include "vtest/vtest_protocol.h"

#include "sw_sync.h"
#include "virtgpu_sync.h"
#include "virtgpu_vtest.h"

static int
get_wait_fd(struct vtest *v, uint64_t sync_mask, uint32_t *sync_id,
            uint64_t *sync_vals)
{
   const uint32_t timeout = UINT32_MAX;
   const uint32_t flags = 0;
   const uint32_t count = util_bitcount((uint32_t)sync_mask) +
         util_bitcount((uint32_t)(sync_mask >> 32));

   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_SYNC_WAIT_SIZE(count);
   vtest_hdr[VTEST_CMD_ID] = VCMD_SYNC_WAIT;

   vtest_write(v, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(v, &flags, sizeof(flags));
   vtest_write(v, &timeout, sizeof(timeout));
   u_foreach_bit (i, sync_mask) {
      const uint64_t val = sync_vals[i];
      const uint32_t sync[3] = {
         sync_id[i],
         (uint32_t)val,
         (uint32_t)(val >> 32),
      };
      vtest_write(v, sync, sizeof(sync));
   }

   vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 0);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_SYNC_WAIT);

   return vtest_receive_fd(v);
}

static int
sync_wait_poll(int fd, bool wait)
{
   struct pollfd pollfd = {
      .fd = fd,
      .events = POLLIN,
   };
   int ret;
   do {
      ret = poll(&pollfd, 1, wait ? UINT32_MAX : 0);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

   return (ret == 1) ? 0 : -EBUSY;
}

void
virtgpu_syncobj_init(struct virtgpu_syncobj *syncobj, uint32_t id, uint32_t ring_idx)
{
   syncobj->next_val = 0;
   syncobj->id = id;
   syncobj->ring_idx = ring_idx;
}

void
virtgpu_resv_lock(struct virtgpu_resv *resv, struct virtgpu_syncobj *syncobj)
{
   resv->sync_mask |= BITFIELD_BIT(syncobj->ring_idx);
   resv->sync_val[syncobj->ring_idx] = syncobj->next_val;
   resv->sync_id[syncobj->ring_idx]  = syncobj->id;
}

int
virtgpu_resv_wait(struct virtgpu_resv *resv, bool wait, struct vtest *v)
{
   vtest_lock(v);
   uint64_t sync_mask = resv->sync_mask;
   int wait_fd = get_wait_fd(v, sync_mask, resv->sync_id, resv->sync_val);
   vtest_unlock(v);

   int ret = sync_wait_poll(wait_fd, wait);
   if (!ret) {
      /* If the fence has already passed, we can clear it from the mask: */
      vtest_lock(v);
      resv->sync_mask &= ~sync_mask;
      vtest_unlock(v);
   }

   close(wait_fd);

   return ret;
}

/**
 * Lazy-init the timeline, because userspace is probably not using all possible
 * ring_idx values.
 */
int
virtgpu_timeline_activate(struct virtgpu_timeline *timeline, struct vtest *v)
{
   if (util_queue_is_initialized(&timeline->signal_queue))
      return 0;

   timeline->sw_sync_fd = open("/sys/kernel/debug/sync/sw_sync", O_RDWR);
   if (timeline->sw_sync_fd < 0)
      return -ENODEV;

   util_queue_init(&timeline->signal_queue, "sw_sync", 64, 1,
                   UTIL_QUEUE_INIT_RESIZE_IF_FULL, timeline);

   timeline->v = v;
   timeline->next_val = 0;

   return 0;
}

struct virtgpu_sync_wait {
   struct util_queue_fence fence;
   uint64_t sync_val;
   uint32_t sync_id;
};

static void
signal_queue_wait_execute(void *job, void *gdata, int thread_index)
{
   struct virtgpu_sync_wait *wait = job;
   struct virtgpu_timeline *timeline = gdata;

   vtest_lock(timeline->v);
   int wait_fd = get_wait_fd(timeline->v, 1, &wait->sync_id, &wait->sync_val);
   vtest_unlock(timeline->v);

   sync_wait_poll(wait_fd, true);

   close(wait_fd);

   __u32 fence_inc = 1;

   /* Signal the fence: */
   drmIoctl(timeline->sw_sync_fd, SW_SYNC_IOC_INC, &fence_inc);
}

static void
signal_queue_wait_cleanup(void *job, void *gdata, int thread_index)
{
   // TODO slab allocator
   free(job);
}

int
virtgpu_timeline_get_fence_fd(struct virtgpu_timeline *timeline,
                              struct virtgpu_syncobj *syncobj)
{
   simple_mtx_assert_locked(&timeline->v->lock);

   struct sw_sync_create_fence_data create_fence = {
         .value = ++timeline->next_val,
   };

   int ret = drmIoctl(timeline->sw_sync_fd, SW_SYNC_IOC_CREATE_FENCE, &create_fence);
   if (ret)
      return ret;

   // TODO use slab allocator
   struct virtgpu_sync_wait *wait = malloc(sizeof(*wait));

   util_queue_fence_init(&wait->fence);
   wait->sync_val = syncobj->next_val;
   wait->sync_id = syncobj->id;

   util_queue_add_job(&timeline->signal_queue, wait, &wait->fence,
                      signal_queue_wait_execute,
                      signal_queue_wait_cleanup, 1);

   return create_fence.fence;
}
