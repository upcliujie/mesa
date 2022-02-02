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

#include "virtio_priv.h"

static int
bo_allocate(struct virtio_bo *virtio_bo)
{
   struct fd_bo *bo = &virtio_bo->base;
   if (!virtio_bo->offset) {
      struct drm_virtgpu_map req = {
         .handle = bo->handle,
      };
      int ret;

      ret = drmIoctl(bo->dev->fd, DRM_IOCTL_VIRTGPU_MAP, &req);
      if (ret) {
         ERROR_MSG("alloc failed: %s", strerror(errno));
         return ret;
      }

      virtio_bo->offset = req.offset;
   }

   return 0;
}

static int
virtio_bo_offset(struct fd_bo *bo, uint64_t *offset)
{
   struct virtio_bo *virtio_bo = to_virtio_bo(bo);
   int ret = bo_allocate(virtio_bo);
   if (ret)
      return ret;
   *offset = virtio_bo->offset;
   return 0;
}

static int
virtio_bo_cpu_prep(struct fd_bo *bo, struct fd_pipe *pipe, uint32_t op)
{
   struct msm_ccmd_gem_cpu_prep_req req = {
         .hdr = {
               .cmd = MSM_CCMD_GEM_CPU_PREP,
               .len = sizeof(req),
         },
         .host_handle = to_virtio_bo(bo)->host_handle,
         .op = op,
         .timeout = 5000000000,
   };
   struct msm_ccmd_gem_cpu_prep_rsp *rsp;

   rsp = virtio_alloc_rsp(pipe->dev, sizeof(*rsp), &req.hdr.resp_off);

   int ret = virtio_execbuf(pipe->dev, &req.hdr, true);
   if (ret)
      goto out;

   ret = rsp->ret;

out:
   virtio_free_rsp(pipe->dev, sizeof(*rsp), req.hdr.resp_off);

   return ret;
}

static void
virtio_bo_cpu_fini(struct fd_bo *bo)
{
   /* no-op */
}

static int
virtio_bo_madvise(struct fd_bo *bo, int willneed)
{
   /* TODO:
    * Currently unsupported, synchronous WILLNEED calls would introduce too
    * much latency.. ideally we'd keep state in the guest and only flush
    * down to host when host is under memory pressure.  (Perhaps virtio-balloon
    * could signal this?)
    */
   return willneed;
}

static uint64_t
virtio_bo_iova(struct fd_bo *bo)
{
   /* The shmem bo is allowed to have no iova, as it is only used for
    * guest<->host communications:
    */
   assert(bo->iova || (to_virtio_bo(bo)->blob_id == 0));
   return bo->iova;
}

static void
virtio_bo_set_name(struct fd_bo *bo, const char *fmt, va_list ap)
{
   char name[32];
   int sz;

   /* Note, we cannot set name on the host for the shmem bo, as
    * that isn't a real gem obj on the host side.. not having
    * an iova is a convenient way to detect this case:
    */
   if (!bo->iova)
      return;

   sz = vsnprintf(name, sizeof(name), fmt, ap);
   sz = MIN2(sz, sizeof(name));

   unsigned req_len = sizeof(struct msm_ccmd_gem_set_name_req) + align(sz, 4);

   uint8_t buf[req_len];
   struct msm_ccmd_gem_set_name_req *req = (void *)buf;

   req->hdr.cmd = MSM_CCMD_GEM_SET_NAME;
   req->hdr.len = req_len;
   req->host_handle = to_virtio_bo(bo)->host_handle;
   req->len = sz;

   memcpy(req->payload, name, sz);

   virtio_execbuf(bo->dev, &req->hdr, false);
}

static void
virtio_bo_destroy(struct fd_bo *bo)
{
   struct virtio_bo *virtio_bo = to_virtio_bo(bo);
   free(virtio_bo);
}

static const struct fd_bo_funcs funcs = {
   .offset = virtio_bo_offset,
   .cpu_prep = virtio_bo_cpu_prep,
   .cpu_fini = virtio_bo_cpu_fini,
   .madvise = virtio_bo_madvise,
   .iova = virtio_bo_iova,
   .set_name = virtio_bo_set_name,
   .destroy = virtio_bo_destroy,
};

static struct fd_bo *
bo_from_handle(struct fd_device *dev, uint32_t size, uint32_t handle)
{
   struct virtio_bo *virtio_bo;
   struct fd_bo *bo;

   virtio_bo = calloc(1, sizeof(*virtio_bo));
   if (!virtio_bo)
      return NULL;

   bo = &virtio_bo->base;
   bo->funcs = &funcs;
   bo->handle = handle;

   return bo;
}

/* allocate a new buffer object from existing handle */
struct fd_bo *
virtio_bo_from_handle(struct fd_device *dev, uint32_t size, uint32_t handle)
{
   struct fd_bo *bo = bo_from_handle(dev, size, handle);

   /* We'll need to add some protocol for this.. TODO how do we know
    * the blob_id, since that would be the obvious way to look things
    * up on the host side
    *
    * I think we can do this with DRM_IOCTL_VIRTGPU_RESOURCE_INFO /
    * struct drm_virtgpu_resource_info
    */
   unreachable("finishme: Need to get iova, host handle, etc");

   return bo;
}

/* allocate a buffer handle: */
struct fd_bo *
virtio_bo_new(struct fd_device *dev, uint32_t size, uint32_t flags)
{
   struct drm_virtgpu_resource_create_blob args = {
      .blob_mem   = VIRTGPU_BLOB_MEM_HOST3D,
      .blob_flags = (flags & FD_BO_NOMAP) ? 0 : VIRTGPU_BLOB_FLAG_USE_MAPPABLE,
      .size       = size,
   };
   struct msm_ccmd_gem_new_req req = {
         .hdr = {
               .cmd = MSM_CCMD_GEM_NEW,
               .len = sizeof(req),
         },
         .size = size,
   };
   struct msm_ccmd_gem_new_rsp *rsp = NULL;
   int ret;

   if (flags & FD_BO_SCANOUT)
      req.flags |= MSM_BO_SCANOUT;

   if (flags & FD_BO_GPUREADONLY)
      req.flags |= MSM_BO_GPU_READONLY;

   if (flags & FD_BO_CACHED_COHERENT) {
      req.flags |= MSM_BO_CACHED_COHERENT;
   } else {
      req.flags |= MSM_BO_WC;
   }

   if (flags & _FD_BO_VIRTIO_SHM) {
      args.blob_id = 0;
   } else {
      args.blob_flags |= VIRTGPU_BLOB_FLAG_USE_SHAREABLE;
      if (flags & (FD_BO_SHARED | FD_BO_SCANOUT))
         args.blob_flags |= VIRTGPU_BLOB_FLAG_USE_CROSS_DEVICE;
      args.blob_id = p_atomic_inc_return(&to_virtio_device(dev)->next_blob_id);
      args.cmd = (intptr_t)&req;
      args.cmd_size = sizeof(req);

      /* tunneled cmds are processed separately on host side,
       * before the renderer->get_blob() callback.. the blob_id
       * is used to like the created bo to the get_blob() call
       */
      req.blob_id = args.blob_id;

      rsp = virtio_alloc_rsp(dev, sizeof(*rsp), &req.hdr.resp_off);
   }

   ret = drmIoctl(dev->fd, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE_BLOB, &args);
   if (ret)
      goto fail;

   struct fd_bo *bo = bo_from_handle(dev, size, args.bo_handle);
   struct virtio_bo *virtio_bo = to_virtio_bo(bo);

   virtio_bo->blob_id = args.blob_id;

   if (rsp) {
      /* RESOURCE_CREATE_BLOB is async, so we need to wait for host..
       * which is a bit unfortunate, but better to sync here than
       * add extra code to check if we need to wait each time we
       * emit a reloc.
       */
      virtio_host_sync(dev);

      virtio_bo->host_handle = rsp->host_handle;
      bo->iova = rsp->iova;
   }

   return bo;

fail:
   if (rsp)
      virtio_free_rsp(dev, sizeof(*rsp), req.hdr.resp_off);
   return NULL;
}
