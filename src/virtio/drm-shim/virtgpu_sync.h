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

#ifndef __VIRTGPU_SYNC_H__
#define __VIRTGPU_SYNC_H__

#define NUM_RINGS 64

#include <stdbool.h>
#include <stdint.h>

#include "util/u_queue.h"

#include "virtgpu_vtest.h"

/**
 * Per-ring syncobj, for tracking EXECBUF completion.
 */
struct virtgpu_syncobj {
   uint64_t next_val;
   uint32_t id;
   uint32_t ring_idx;
};

void virtgpu_syncobj_init(struct virtgpu_syncobj *syncobj, uint32_t id,
                          uint32_t ring_idx);

/**
 * Tracking for buffer object business across multiple rings.  Serves a
 * similar purpose of dma_resv on the kernel side, but OFC cannot handle
 * implicit sync with buffers shared across devices or processes.  (But
 * fence-fd's are expected to be used for that in all scenarios where
 * the vtest drm-shim can work.)  This should be sufficient for the
 * WAIT ioctl implementation for usermode driver internal buffers.
 */
struct virtgpu_resv {
   uint64_t sync_mask : NUM_RINGS;
   uint64_t sync_val[NUM_RINGS];
   uint32_t sync_id[NUM_RINGS];
};

void virtgpu_resv_lock(struct virtgpu_resv *resv, struct virtgpu_syncobj *syncobj);
int virtgpu_resv_wait(struct virtgpu_resv *resv, bool wait, struct vtest *v);

/**
 * A per-ring fence fd timeline, which uses SW_SYNC to create and signal
 * dma-buf fence-fd's
 */
struct virtgpu_timeline {
   struct vtest *v;

   /**
    * Note that sw_sync uses 32b fence counter.  The fence counter is
    * decoupled from virtgpu_syncobj::next_val, as it is only incremented
    * when we need to create a fence fd (whereas the syncobj next_val is
    * incremented on each EXECBUF).
    */
   uint32_t next_val;

   int sw_sync_fd;
   struct util_queue signal_queue;
};

int virtgpu_timeline_activate(struct virtgpu_timeline *timeline, struct vtest *v);
int virtgpu_timeline_get_fence_fd(struct virtgpu_timeline *timeline,
                                  struct virtgpu_syncobj *syncobj);

#endif /*  __VIRTGPU_SYNC_H__ */
