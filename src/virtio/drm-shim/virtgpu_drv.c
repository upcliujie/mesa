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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "util/libsync.h"

#include "drm-shim/drm_shim.h"
#include "drm-uapi/virtgpu_drm.h"

#define VIRGL_RENDERER_UNSTABLE_APIS 1
#include "vtest/vtest_protocol.h"

#include "virtio-gpu/virglrenderer_hw.h"

#include "virtgpu_sync.h"
#include "virtgpu_vtest.h"

#define U642VOID(x) ((void *)(unsigned long)(x))

bool drm_shim_driver_prefers_first_render_node = true;

static struct vtest *v;

struct virtgpu_shim_fd {
   /**
    * Track per-ring_idx sync objects to track EXECBUF completion.
    */
   struct virtgpu_syncobj sync[NUM_RINGS];

   /**
    * Track per-ring_idx timelines for fence-fd's
    */
   struct virtgpu_timeline timeline[NUM_RINGS];
};

static struct virtgpu_shim_fd *
get_virtgpu_fd(struct shim_fd *shim_fd)
{
   if (!shim_fd->driver_priv) {
      // TODO sort out where shim_fd->driver_priv gets free'd..
      shim_fd->driver_priv = calloc(1, sizeof(struct virtgpu_shim_fd));
   }
   return shim_fd->driver_priv;
}

struct virtgpu_shim_bo {
   struct shim_bo base;
   uint32_t res_id;
   uint32_t blob_mem;
   struct virtgpu_resv resv;
   int fd;
};
static struct virtgpu_shim_bo *
to_virtgpu_bo(struct shim_bo *shim_bo)
{
   return (struct virtgpu_shim_bo *)shim_bo;
}

static struct virtgpu_shim_bo *
bo_new(size_t size)
{
   struct virtgpu_shim_bo *bo = calloc(1, sizeof(*bo));
   int ret;

   ret = drm_shim_bo_init(&bo->base, size);
   if (ret) {
      free(bo);
      return NULL;
   }

   return bo;
}

static int
virtgpu_ioctl_map(int fd, unsigned long request, void *arg)
{
   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct drm_virtgpu_map *args = arg;
   struct shim_bo *bo = drm_shim_bo_lookup(shim_fd, args->handle);

   args->offset = drm_shim_bo_get_mmap_offset(shim_fd, bo);

   drm_shim_bo_put(bo);

   return 0;
}

static void
send_commands(struct vtest *v, uint32_t *cmds, uint32_t cmd_size,
              struct virtgpu_syncobj *syncobj, uint32_t ring_idx)
{
   struct vcmd_submit_cmd2_batch batch;

   uint32_t header_size = sizeof(batch) + sizeof(uint32_t);
   uint32_t sync_size = 0;
   if (syncobj) {
      sync_size = sizeof(uint32_t) + sizeof(uint64_t);
   }
   uint32_t total_size = header_size + cmd_size + sync_size;

   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = total_size / sizeof(uint32_t);
   vtest_hdr[VTEST_CMD_ID] = VCMD_SUBMIT_CMD2;

   vtest_write(v, &vtest_hdr, sizeof(vtest_hdr));

   const uint32_t batch_count = 1;
   vtest_write(v, &batch_count, sizeof(batch_count));

   batch = (struct vcmd_submit_cmd2_batch){
      .cmd_offset = header_size / sizeof(uint32_t),
      .cmd_size = cmd_size / sizeof(uint32_t),
      .sync_offset = (header_size + cmd_size) / sizeof(uint32_t),
   };

   if (syncobj) {
      batch.flags |= VCMD_SUBMIT_CMD2_FLAG_SYNC_QUEUE;
      batch.sync_count = 1;
      batch.sync_queue_id = ring_idx;
      batch.sync_queue_index = ring_idx;
   }

   vtest_write(v, &batch, sizeof(batch));
   vtest_write(v, cmds, cmd_size);

   if (syncobj) {
      syncobj->next_val++;
      const uint32_t sync[3] = {
            syncobj->id,
            (uint32_t)syncobj->next_val,
            (uint32_t)(syncobj->next_val >> 32),
      };
      vtest_write(v, sync, sizeof(sync));
   }
}

static int
virtgpu_ioctl_execbuffer(int fd, unsigned long request, void *arg)
{
   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct virtgpu_shim_fd *virtgpu_fd = get_virtgpu_fd(shim_fd);
   struct drm_virtgpu_execbuffer *args = arg;

   /* Note, explicitly test the flags we support rather than using
    * VIRTGPU_EXECBUF_FLAGS in case uabi header gets updated without
    * corresponding update to vtest shim
    */
   if (args->flags & ~(VIRTGPU_EXECBUF_FENCE_FD_IN |
                       VIRTGPU_EXECBUF_FENCE_FD_OUT |
                       VIRTGPU_EXECBUF_RING_IDX)) {
      return -EINVAL;
   }

   if (args->flags & VIRTGPU_EXECBUF_FENCE_FD_IN) {
      sync_wait(args->fence_fd, -1);
   }

   int ring_idx = -1;
   if (args->flags & VIRTGPU_EXECBUF_RING_IDX) {
      if (args->ring_idx >= NUM_RINGS)
         return -EINVAL;
      ring_idx = args->ring_idx;
   } else if ((args->num_bo_handles > 0) ||
              (args->flags & VIRTGPU_EXECBUF_FENCE_FD_OUT)) {
      /* This perhaps isn't *quite* right, since in this case there
       * is a single global timeline.  But we can't really emulate
       * that, so this is the next best thing.
       */
      ring_idx = 0;
   }

   if (args->flags & VIRTGPU_EXECBUF_FENCE_FD_OUT) {
      int ret = virtgpu_timeline_activate(&virtgpu_fd->timeline[ring_idx], v);
      if (ret)
         return ret;
   }

   struct virtgpu_syncobj *sync = NULL;
   if (ring_idx >= 0) {
      sync = &virtgpu_fd->sync[args->ring_idx];
   }

   vtest_lock(v);
   send_commands(v, U642VOID(args->command), args->size, sync, ring_idx);

   if (args->flags & VIRTGPU_EXECBUF_FENCE_FD_OUT) {
      args->fence_fd = virtgpu_timeline_get_fence_fd(
            &virtgpu_fd->timeline[ring_idx], sync);
   }

   uint32_t *bo_handles = U642VOID(args->bo_handles);
   for (unsigned i = 0; i < args->num_bo_handles; i++) {
      struct shim_bo *bo = drm_shim_bo_lookup(shim_fd, bo_handles[i]);

      virtgpu_resv_lock(&to_virtgpu_bo(bo)->resv, sync);

      drm_shim_bo_put(bo);
   }

   vtest_unlock(v);

   return 0;
}

static int
virtgpu_ioctl_resource_info(int fd, unsigned long request, void *arg)
{
   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct drm_virtgpu_resource_info *args = arg;
   struct shim_bo *bo = drm_shim_bo_lookup(shim_fd, args->bo_handle);

   if (!bo)
      return -ENOENT;

   args->size = bo->size;
   args->res_handle = to_virtgpu_bo(bo)->res_id;
   args->blob_mem = to_virtgpu_bo(bo)->blob_mem;

   drm_shim_bo_put(bo);

   return 0;
}

static int
getparam(struct vtest *v, enum vcmd_param param, uint64_t *val)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_get_param[VCMD_GET_PARAM_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_GET_PARAM_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_GET_PARAM;
   vcmd_get_param[VCMD_GET_PARAM_PARAM] = param;

   vtest_lock(v);

   vtest_write(v, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(v, vcmd_get_param, sizeof(vcmd_get_param));

   vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 2);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_GET_PARAM);

   uint32_t resp[2];
   vtest_read(v, resp, sizeof(resp));

   vtest_unlock(v);

   if (!resp[0])
      return -EINVAL;

   *val = resp[1];
   return 0;
}

/* XXX WIP kernel uapi needed by venus */
#ifndef VIRTGPU_PARAM_MAX_SYNC_QUEUE_COUNT
#define VIRTGPU_PARAM_MAX_SYNC_QUEUE_COUNT 100
#endif /* VIRTGPU_PARAM_MAX_SYNC_QUEUE_COUNT */

static int
virtgpu_ioctl_getparam(int fd, unsigned long request, void *arg)
{
   struct drm_virtgpu_getparam *args = arg;
   uint64_t *val = U642VOID(args->value);

   switch (args->param) {
   case VIRTGPU_PARAM_MAX_SYNC_QUEUE_COUNT:
      return getparam(v, VCMD_PARAM_MAX_SYNC_QUEUE_COUNT, val);
   case VIRTGPU_PARAM_3D_FEATURES:
   case VIRTGPU_PARAM_CAPSET_QUERY_FIX:
   case VIRTGPU_PARAM_RESOURCE_BLOB:
   case VIRTGPU_PARAM_HOST_VISIBLE:
   case VIRTGPU_PARAM_CROSS_DEVICE:
   case VIRTGPU_PARAM_CONTEXT_INIT:
      *val = 1;
      return 0;
   case VIRTGPU_PARAM_SUPPORTED_CAPSET_IDs:
      /* TODO I don't think vtest gives us a way to query this yet
       *
       * TODO expose VIRGL and VIRGL2 when more of the ioctls and
       * host storage is supported
       *
       * TODO expose VENUS when host storage is supported
       */
      *val = BITFIELD_BIT(VIRGL_RENDERER_CAPSET_DRM);
      return 0;
   default:
      return -EINVAL;
   }
}

static int
virtgpu_ioctl_wait(int fd, unsigned long request, void *arg)
{
   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct drm_virtgpu_3d_wait *args = arg;
   struct shim_bo *bo = drm_shim_bo_lookup(shim_fd, args->handle);

   if (args->flags & ~VIRTGPU_WAIT_NOWAIT)
      return -EINVAL;

   bool wait = !(args->flags & VIRTGPU_WAIT_NOWAIT);

   int ret = virtgpu_resv_wait(&to_virtgpu_bo(bo)->resv, wait, v);

   drm_shim_bo_put(bo);

   return ret;
}

static int
get_caps(struct vtest *v, struct drm_virtgpu_get_caps *args)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_get_capset[VCMD_GET_CAPSET_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_GET_CAPSET_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_GET_CAPSET;
   vcmd_get_capset[VCMD_GET_CAPSET_ID] = args->cap_set_id;
   vcmd_get_capset[VCMD_GET_CAPSET_VERSION] = args->cap_set_ver;

   vtest_write(v, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(v, vcmd_get_capset, sizeof(vcmd_get_capset));

   vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_GET_CAPSET);

   uint32_t valid;
   vtest_read(v, &valid, sizeof(valid));
   if (!valid) {
      /* unsupported id or version */
      return -EINVAL;
   }

   size_t read_size = (vtest_hdr[VTEST_CMD_LEN] - 1) * 4;
   void *capset = U642VOID(args->addr);
   if (args->size >= read_size) {
      vtest_read(v, capset, read_size);
      memset(capset + read_size, 0, args->size - read_size);
   } else {
      vtest_read(v, capset, args->size);

      char temp[256];
      read_size -= args->size;
      while (read_size) {
         const size_t temp_size = MIN2(read_size, ARRAY_SIZE(temp));
         vtest_read(v, temp, temp_size);
         read_size -= temp_size;
      }
   }

   return 0;
}

static int
virtgpu_ioctl_get_caps(int fd, unsigned long request, void *arg)
{
   struct drm_virtgpu_get_caps *args = arg;

   if (!args->size)
      return -ENOSYS;

   vtest_lock(v);
   int ret = get_caps(v, args);
   vtest_unlock(v);

   return ret;
}

static int
virtgpu_ioctl_resource_create_blob(int fd, unsigned long request, void *arg)
{
   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct drm_virtgpu_resource_create_blob *args = arg;

   /* TODO support guest storage as well, for virgl */
   if (args->blob_mem != VIRTGPU_BLOB_MEM_HOST3D)
      return -EINVAL;

   if (args->blob_flags & ~(VIRTGPU_BLOB_FLAG_USE_MAPPABLE |
                            VIRTGPU_BLOB_FLAG_USE_SHAREABLE |
                            VIRTGPU_BLOB_FLAG_USE_CROSS_DEVICE))
      return -EINVAL;

   if (args->cmd_size % 4)
      return -EINVAL;

   struct virtgpu_shim_bo *bo = bo_new(args->size);
   if (!bo)
      return -ENOMEM;

   bo->blob_mem = args->blob_mem;

   enum vcmd_blob_type type = VCMD_BLOB_TYPE_HOST3D;
   uint32_t flags = 0;

   /* TODO we should only set this if _USE_MAPPABLE blob_flag is set,
    * but vtest tries to unconditionally export to fd and send that
    * back to us.  We need a way to signal to vtest that we don't
    * want an fd (and to skip the vtest_receive_fd() below):
    */
   //if (args->blob_flags & VIRTGPU_BLOB_FLAG_USE_MAPPABLE)
      flags |= VCMD_BLOB_FLAG_MAPPABLE;

   if (args->blob_flags & VIRTGPU_BLOB_FLAG_USE_SHAREABLE)
      flags |= VCMD_BLOB_FLAG_SHAREABLE;

   if (args->blob_flags & VIRTGPU_BLOB_FLAG_USE_CROSS_DEVICE)
      flags |= VCMD_BLOB_FLAG_CROSS_DEVICE;

   vtest_lock(v);

   if (args->cmd_size)
      send_commands(v, U642VOID(args->cmd), args->cmd_size, NULL, 0);

   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_SIZE];

   vtest_hdr[VTEST_CMD_LEN] = VCMD_RES_CREATE_BLOB_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_RESOURCE_CREATE_BLOB;

   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_TYPE] = type;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_FLAGS] = flags;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_SIZE_LO] = (uint32_t)args->size;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_SIZE_HI] =
      (uint32_t)(args->size >> 32);
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_ID_LO] = (uint32_t)args->blob_id;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_ID_HI] =
      (uint32_t)(args->blob_id >> 32);

   vtest_write(v, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(v, vcmd_res_create_blob, sizeof(vcmd_res_create_blob));

   vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 1);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_RESOURCE_CREATE_BLOB);

   vtest_read(v, &bo->res_id, sizeof(uint32_t));
   args->res_handle = bo->res_id;

   bo->fd = vtest_receive_fd(v);

   /* Since we can't *not* request a bo fd, the next best thing is to
    * immediately close it if it is unneeded.
    */
   if (!(args->blob_flags & (VIRTGPU_BLOB_FLAG_USE_MAPPABLE |
                             VIRTGPU_BLOB_FLAG_USE_CROSS_DEVICE |
                             VIRTGPU_BLOB_FLAG_USE_SHAREABLE))) {
      close(bo->fd);
      bo->fd = -1;
   }

   vtest_unlock(v);

   args->bo_handle = drm_shim_bo_get_handle(shim_fd, &bo->base);
   drm_shim_bo_put(&bo->base);

   return 0;
}

static void
context_init(struct vtest *v, struct virtgpu_shim_fd *virtgpu_fd, uint32_t capset_id)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_context_init[VCMD_CONTEXT_INIT_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_CONTEXT_INIT_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_CONTEXT_INIT;
   vcmd_context_init[VCMD_CONTEXT_INIT_CAPSET_ID] = capset_id;

   vtest_write(v, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(v, vcmd_context_init, sizeof(vcmd_context_init));

   for (int i = 0; i < NUM_RINGS; i++) {
      struct virtgpu_syncobj *sync = &virtgpu_fd->sync[i];

      uint32_t vtest_hdr[VTEST_HDR_SIZE];
      uint32_t vcmd_sync_create[VCMD_SYNC_CREATE_SIZE];

      vtest_hdr[VTEST_CMD_LEN] = VCMD_SYNC_CREATE_SIZE;
      vtest_hdr[VTEST_CMD_ID] = VCMD_SYNC_CREATE;

      /* Counter starts at zero: */
      vcmd_sync_create[VCMD_SYNC_CREATE_VALUE_LO] = 0;
      vcmd_sync_create[VCMD_SYNC_CREATE_VALUE_HI] = 0;

      vtest_write(v, vtest_hdr, sizeof(vtest_hdr));
      vtest_write(v, vcmd_sync_create, sizeof(vcmd_sync_create));

      vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
      assert(vtest_hdr[VTEST_CMD_LEN] == 1);
      assert(vtest_hdr[VTEST_CMD_ID] == VCMD_SYNC_CREATE);

      uint32_t id;
      vtest_read(v, &id, sizeof(id));

      virtgpu_syncobj_init(sync, id, i);
   }
}

static int
virtgpu_ioctl_context_init(int fd, unsigned long request, void *arg)
{
   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct drm_virtgpu_context_init *args = arg;
   struct drm_virtgpu_context_set_param *params = U642VOID(args->ctx_set_params);

   for (unsigned i = 0; i < args->num_params; i++) {
      switch (params[i].param) {
      case VIRTGPU_CONTEXT_PARAM_CAPSET_ID:
         vtest_lock(v);
         context_init(v, get_virtgpu_fd(shim_fd), params[i].value);
         vtest_unlock(v);
         break;
      case VIRTGPU_CONTEXT_PARAM_NUM_RINGS:
      case VIRTGPU_CONTEXT_PARAM_POLL_RINGS_MASK:
         /* ignore for now */
         // TODO track these for extra error checking on EXECBUF ioctl
         break;
      default:
         return -EINVAL;
      }
   }

   return 0;
}

static void
virtgpu_bo_free(struct shim_bo *shim_bo)
{
   struct virtgpu_shim_bo *virtgpu_bo = to_virtgpu_bo(shim_bo);
   if (virtgpu_bo->fd >= 0)
      close(virtgpu_bo->fd);
}

static void *
virtgpu_bo_mmap(struct shim_bo *shim_bo, int prot, int flags)
{
   struct virtgpu_shim_bo *virtgpu_bo = to_virtgpu_bo(shim_bo);

   return mmap(NULL, shim_bo->size, prot, flags, virtgpu_bo->fd, 0);
}

static int
virtgpu_bo_to_fd(struct shim_bo *shim_bo)
{
   struct virtgpu_shim_bo *virtgpu_bo = to_virtgpu_bo(shim_bo);
   return dup(virtgpu_bo->fd);
}

static ioctl_fn_t driver_ioctls[] = {
   [DRM_VIRTGPU_MAP] = virtgpu_ioctl_map,
   [DRM_VIRTGPU_EXECBUFFER] = virtgpu_ioctl_execbuffer,
   [DRM_VIRTGPU_GETPARAM] = virtgpu_ioctl_getparam,
//   [DRM_VIRTGPU_RESOURCE_CREATE] = virtgpu_ioctl_resource_create,
   [DRM_VIRTGPU_RESOURCE_INFO] = virtgpu_ioctl_resource_info,
//   [DRM_VIRTGPU_TRANSFER_FROM_HOST] = virtgpu_ioctl_transfer_from_host,
//   [DRM_VIRTGPU_TRANSFER_TO_HOST] = virtgpu_ioctl_transfer_to_host,
   [DRM_VIRTGPU_WAIT] = virtgpu_ioctl_wait,
   [DRM_VIRTGPU_GET_CAPS] = virtgpu_ioctl_get_caps,
   [DRM_VIRTGPU_RESOURCE_CREATE_BLOB] = virtgpu_ioctl_resource_create_blob,
   [DRM_VIRTGPU_CONTEXT_INIT] = virtgpu_ioctl_context_init,
};

void
drm_shim_driver_init(void)
{
   shim_device.bus_type = DRM_BUS_PLATFORM;
   shim_device.driver_name = "virtio_gpu";
   shim_device.driver_ioctls = driver_ioctls;
   shim_device.driver_ioctl_count = ARRAY_SIZE(driver_ioctls);

   shim_device.version_major = 0;
   shim_device.version_minor = 1;
   shim_device.version_patchlevel = 0;

   shim_device.driver_bo_free = virtgpu_bo_free;
   shim_device.driver_bo_mmap = virtgpu_bo_mmap;
   shim_device.driver_bo_to_fd = virtgpu_bo_to_fd;

   drm_shim_override_file("DRIVER=virtio_gpu\n"
                          "MODALIAS=virtio:d00000010v00001AF4\n",
                          "/sys/dev/char/%d:%d/device/uevent", DRM_MAJOR,
                          render_node_minor);

   v = vtest_connect();
}
