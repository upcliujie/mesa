/*
 * Copyright © 2022 Google, Inc.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "virtio_priv.h"

static const char *ccmds[MSM_CCMD_LAST] = {
#define NAME(n) [MSM_CCMD_##n] = #n
      NAME(NOP),
      NAME(IOCTL_SIMPLE),
      NAME(GEM_NEW),
      NAME(GEM_INFO),
      NAME(GEM_CPU_PREP),
      NAME(GEM_SET_NAME),
      NAME(GEM_SUBMIT),
      NAME(GEM_UPLOAD),
      NAME(SUBMITQUEUE_QUERY),
      NAME(WAIT_FENCE),
#undef NAME
};

static void
virtio_device_dump_stats(struct fd_device *dev)
{
   struct virtio_device *virtio_dev = to_virtio_device(dev);
   int64_t t = os_time_get_nano();

   if ((t - virtio_dev->last_stat_time) < NSEC_PER_SEC)
      return;

   virtio_dev->last_stat_time = t;

   for (unsigned i = 0; i < MSM_CCMD_LAST; i++) {
      if (!ccmds[i])
         continue;
      struct virtio_ccmd_stat *stat = &virtio_dev->stats[i];
      int64_t avg = stat->count ? stat->waittime / stat->count : 0;
      mesa_logi("%-20s: %u calls, waited %"PRId64" ns (avg)",
                ccmds[i], stat->count, avg);

      stat->count = 0;
      stat->waittime = 0;
   }
}

static void
virtio_device_destroy(struct fd_device *dev)
{
   struct virtio_device *virtio_dev = to_virtio_device(dev);
   free(virtio_dev);
}

static const struct fd_device_funcs funcs = {
   .bo_new = virtio_bo_new,
   .bo_from_handle = virtio_bo_from_handle,
   .pipe_new = virtio_pipe_new,
   .dump_stats = virtio_device_dump_stats,
   .destroy = virtio_device_destroy,
};

static int
get_capset(int fd, struct virgl_renderer_capset_msm *caps)
{
   struct drm_virtgpu_get_caps args = {
         .cap_set_id = VIRGL_RENDERER_CAPSET_MSM,
         .cap_set_ver = 0,
         .addr = VOID2U64(caps),
         .size = sizeof(*caps),
   };

   return drmIoctl(fd, DRM_IOCTL_VIRTGPU_GET_CAPS, &args);
}

static int
set_context(int fd)
{
   struct drm_virtgpu_context_init args = {
      .num_params = 1,
      .ctx_set_params = (uintptr_t) &
                        (struct drm_virtgpu_context_set_param){
                           .param = VIRTGPU_CONTEXT_PARAM_CAPSET_ID,
                           .value = VIRGL_RENDERER_CAPSET_MSM,
                        },
   };

   return drmIoctl(fd, DRM_IOCTL_VIRTGPU_CONTEXT_INIT, &args);
}

struct fd_device *
virtio_device_new(int fd, drmVersionPtr version)
{
   struct virgl_renderer_capset_msm caps;
   struct virtio_device *virtio_dev;
   struct fd_device *dev;
   int ret;

   STATIC_ASSERT(FD_BO_PREP_READ == MSM_PREP_READ);
   STATIC_ASSERT(FD_BO_PREP_WRITE == MSM_PREP_WRITE);
   STATIC_ASSERT(FD_BO_PREP_NOSYNC == MSM_PREP_NOSYNC);

   /* Debug option to force fallback to virgl: */
   if (debug_get_bool_option("FD_NO_VIRTIO", false))
      return NULL;

   ret = get_capset(fd, &caps);
   if (ret) {
      INFO_MSG("could not get caps: %s", strerror(errno));
      return NULL;
   }

   INFO_MSG("wire_format_version: %u", caps.wire_format_version);
   INFO_MSG("version_major:       %u", caps.version_major);
   INFO_MSG("version_minor:       %u", caps.version_minor);
   INFO_MSG("version_patchlevel:  %u", caps.version_patchlevel);

   if (caps.wire_format_version != 1) {
      ERROR_MSG("Unsupported protocol version: %u", caps.wire_format_version);
      return NULL;
   }

   if ((caps.version_major != 1) || (caps.version_minor < FD_VERSION_SOFTPIN)) {
      ERROR_MSG("unsupported version: %u.%u.%u", caps.version_major,
                caps.version_minor, caps.version_patchlevel);
      return NULL;
   }

   ret = set_context(fd);
   if (ret) {
      INFO_MSG("Could not set context type: %s", strerror(errno));
      return NULL;
   }

   virtio_dev = calloc(1, sizeof(*virtio_dev));
   if (!virtio_dev)
      return NULL;

   dev = &virtio_dev->base;
   dev->funcs = &funcs;
   dev->version = caps.version_minor;

   p_atomic_set(&virtio_dev->next_blob_id, 1);

   util_queue_init(&dev->submit_queue, "sq", 8, 1, 0, NULL);

   if (dev->version >= FD_VERSION_CACHED_COHERENT) {
/// TODO
//      struct drm_msm_gem_new new_req = {
//         .size = 0x1000,
//         .flags = MSM_BO_CACHED_COHERENT,
//      };
//
//      /* The kernel is new enough to support MSM_BO_CACHED_COHERENT,
//       * but that is not a guarantee that the device we are running
//       * on supports it.  So do a test allocation to find out.
//       */
//      if (!drmCommandWriteRead(fd, DRM_MSM_GEM_NEW,
//                               &new_req, sizeof(new_req))) {
//         struct drm_gem_close close_req = {
//            .handle = new_req.handle,
//         };
//         drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close_req);
//
//         dev->has_cached_coherent = true;
//      }
   }

   dev->bo_size = sizeof(struct virtio_bo);

   simple_mtx_init(&virtio_dev->rsp_lock, mtx_plain);
   simple_mtx_init(&virtio_dev->eb_lock, mtx_plain);

   return dev;
}

void *
virtio_alloc_rsp(struct fd_device *dev, uint32_t sz, uint32_t *out_off)
{
   struct virtio_device *virtio_dev = to_virtio_device(dev);
   unsigned off;

   simple_mtx_lock(&virtio_dev->rsp_lock);

   /* One would like to do this in virtio_device_new(), but we'd
    * have to bypass/reinvent fd_bo_new().. revisit this
    *
    * TODO move to pipe creation?
    */
   if (unlikely(!virtio_dev->shmem)) {
      virtio_dev->shmem_bo = fd_bo_new(dev, sizeof(*virtio_dev->shmem),
                                       _FD_BO_VIRTIO_SHM, "shmem");
      virtio_dev->shmem = fd_bo_map(virtio_dev->shmem_bo);

      virtio_dev->shmem_bo->bo_reuse = NO_CACHE;
   }

   sz = align(sz, 8);

   /* TODO we don't actually want to rely on response msgs being freed
    * in order, because there can be multiple threads involved, and
    * something like a wait could take longer.  So this is a bit YOLO,
    * just hoping that older responses are freed before we wrap around
    * and start overwriting them.  A proper allocator is needed.
    */

   if ((virtio_dev->next_rsp_off + sz) >= sizeof(virtio_dev->shmem->rsp_mem))
      virtio_dev->next_rsp_off = 0;

   off = virtio_dev->next_rsp_off;
   virtio_dev->next_rsp_off += sz;

   simple_mtx_unlock(&virtio_dev->rsp_lock);

   *out_off = off;

   return &virtio_dev->shmem->rsp_mem[off];
}

/**
 * Helper for "execbuf" ioctl.. note that in virtgpu execbuf is just
 * a generic "send commands to host", not necessarily specific to
 * cmdstream execution.
 */
int
virtio_execbuf_fenced(struct fd_device *dev, struct msm_ccmd_req *req,
                      int in_fence_fd, int *out_fence_fd)
{
   struct virtio_device *virtio_dev = to_virtio_device(dev);

   simple_mtx_lock(&virtio_dev->eb_lock);
   req->seqno = ++virtio_dev->next_seqno;
   virtio_dev->stats[req->cmd].count++;

#define COND(bool, val) ((bool) ? (val) : 0)
   struct drm_virtgpu_execbuffer eb = {
         .flags = COND(out_fence_fd, VIRTGPU_EXECBUF_FENCE_FD_OUT) |
                  COND(in_fence_fd != -1, VIRTGPU_EXECBUF_FENCE_FD_IN),
         .fence_fd = in_fence_fd,
         .size  = req->len,
         .command = VOID2U64(req),
   };

   int ret = drmIoctl(dev->fd, DRM_IOCTL_VIRTGPU_EXECBUFFER, &eb);
   simple_mtx_unlock(&virtio_dev->eb_lock);
   if (ret) {
      ERROR_MSG("EXECBUFFER failed: %s", strerror(errno));
      return ret;
   }

   if (out_fence_fd)
      *out_fence_fd = eb.fence_fd;

   return 0;
}

int
virtio_execbuf(struct fd_device *dev, struct msm_ccmd_req *req, bool sync)
{
   int ret = virtio_execbuf_fenced(dev, req, -1, NULL);

   if (ret)
      return ret;

   if (sync)
      virtio_host_sync(dev, req);

   return 0;
}

/**
 * Wait until host as processed the specified request.
 */
void
virtio_host_sync(struct fd_device *dev, const struct msm_ccmd_req *req)
{
   struct virtio_device *virtio_dev = to_virtio_device(dev);
   int64_t t = os_time_get_nano();

   while (fd_fence_before(virtio_dev->shmem->seqno, req->seqno))
      sched_yield();

   t = os_time_get_nano() - t;
   virtio_dev->stats[req->cmd].waittime += t;
}

/**
 * Helper for simple pass-thru ioctls
 */
int
virtio_simple_ioctl(struct fd_device *dev, unsigned cmd, void *_req)
{
   unsigned req_len = sizeof(struct msm_ccmd_ioctl_simple_req);
   unsigned rsp_len = sizeof(struct msm_ccmd_ioctl_simple_rsp);

   req_len += _IOC_SIZE(cmd);
   if (cmd & IOC_OUT)
      rsp_len += _IOC_SIZE(cmd);

   uint8_t buf[req_len];
   struct msm_ccmd_ioctl_simple_req *req = (void *)buf;
   struct msm_ccmd_ioctl_simple_rsp *rsp;

   req->hdr = MSM_CCMD(IOCTL_SIMPLE, req_len);
   req->cmd = cmd;
   memcpy(req->payload, _req, _IOC_SIZE(cmd));

   rsp = virtio_alloc_rsp(dev, rsp_len, &req->hdr.rsp_off);

   int ret = virtio_execbuf(dev, &req->hdr, true);

   if (cmd & IOC_OUT)
      memcpy(_req, rsp->payload, _IOC_SIZE(cmd));

   ret = rsp->ret;

   return ret;
}
