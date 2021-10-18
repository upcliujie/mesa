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

#include "vk_drm_syncobj.h"

#include <xf86drm.h>

#include "drm-uapi/drm.h"

#include "vk_device.h"
#include "vk_log.h"
#include "vk_util.h"

static struct vk_drm_syncobj *
to_drm_syncobj(struct vk_sync *sync)
{
   assert(sync->type == &vk_drm_binary_syncobj_type ||
          sync->type == &vk_drm_timeline_syncobj_type);

   return container_of(sync, struct vk_drm_syncobj, base);
}

static VkResult
vk_drm_binary_syncobj_init(struct vk_device *device,
                           struct vk_sync *sync,
                           uint64_t initial_value)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   uint32_t flags = 0;
   if (initial_value)
      flags |= DRM_SYNCOBJ_CREATE_SIGNALED;

   assert(device->drm_fd >= 0);
   int err = drmSyncobjCreate(device->drm_fd, flags, &sobj->syncobj);
   if (err < 0) {
      return vk_errorf(device, VK_ERROR_OUT_OF_HOST_MEMORY,
                       "DRM_IOCTL_SYNCOBJ_CREATE failed: %m");
   }

   return VK_SUCCESS;
}

static void
vk_drm_syncobj_finish(struct vk_device *device,
                      struct vk_sync *sync)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   ASSERTED int err = drmSyncobjDestroy(device->drm_fd, sobj->syncobj);
   assert(err == 0);
}

static VkResult
vk_drm_timeline_syncobj_init(struct vk_device *device,
                             struct vk_sync *sync,
                             uint64_t initial_value)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   int err = drmSyncobjCreate(device->drm_fd, 0, &sobj->syncobj);
   if (err < 0) {
      return vk_errorf(device, VK_ERROR_OUT_OF_HOST_MEMORY,
                       "DRM_IOCTL_SYNCOBJ_CREATE failed: %m");
   }

   if (initial_value) {
      err = drmSyncobjTimelineSignal(device->drm_fd, &sobj->syncobj,
                                     &initial_value, 1);
      if (err < 0) {
         vk_drm_syncobj_finish(device, sync);
         return vk_errorf(device, VK_ERROR_OUT_OF_HOST_MEMORY,
                          "DRM_IOCTL_SYNCOBJ_CREATE failed: %m");
      }
   }

   return VK_SUCCESS;
}

static VkResult
vk_drm_syncobj_signal(struct vk_device *device,
                      struct vk_sync *sync,
                      uint64_t value)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   int err;
   if (sync->type->is_timeline)
      err = drmSyncobjTimelineSignal(device->drm_fd, &sobj->syncobj, &value, 1);
   else
      err = drmSyncobjSignal(device->drm_fd, &sobj->syncobj, 1);
   if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_SIGNAL failed: %m");
   }

   return VK_SUCCESS;
}

static VkResult
vk_drm_syncobj_get_value(struct vk_device *device,
                         struct vk_sync *sync,
                         uint64_t *value)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   int err = drmSyncobjQuery(device->drm_fd, &sobj->syncobj, value, 1);
   if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_QUERY failed: %m");
   }

   return VK_SUCCESS;
}

static VkResult
vk_drm_syncobj_reset(struct vk_device *device,
                     struct vk_sync *sync)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   int err = drmSyncobjReset(device->drm_fd, &sobj->syncobj, 1);
   if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_RESET failed: %m");
   }

   return VK_SUCCESS;
}

static uint32_t
wait_type_to_flags(enum vk_sync_wait_type wait_type)
{
   switch (wait_type) {
   case VK_SYNC_WAIT_PENDING:
      return DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT |
             DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE;
   case VK_SYNC_WAIT_COMPLETE:
      return DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;
   }
   unreachable("Invalid wait type");
}

static VkResult
vk_drm_syncobj_wait(struct vk_device *device,
                    uint32_t sync_count,
                    struct vk_sync **const syncs,
                    uint64_t *wait_values,
                    uint32_t syncobj_wait_flags,
                    uint64_t abs_timeout_ns)
{
   assert(sync_count > 0);
   const struct vk_sync_type *sync_type = syncs[0]->type;

   STACK_ARRAY(uint32_t, handles, sync_count);
   for (uint32_t i = 0; i < sync_count; i++) {
      /* TODO: We might be able to do timeline and non-timeline waits at the
       * same time if we're really careful.  Maybe one day we'll decide we
       * care.
       */
      assert(syncs[i]->type == sync_type);
      handles[i] = to_drm_syncobj(syncs[i])->syncobj;
   }

   assert(device->drm_fd >= 0);
   int err;
   if (sync_type->is_timeline) {
      err = drmSyncobjTimelineWait(device->drm_fd, handles, wait_values,
                                   sync_count, abs_timeout_ns,
                                   syncobj_wait_flags,
                                   NULL /* first_signaled */);
   } else {
      err = drmSyncobjWait(device->drm_fd, handles,
                           sync_count, abs_timeout_ns,
                           syncobj_wait_flags,
                           NULL /* first_signaled */);
   }

   STACK_ARRAY_FINISH(handles);

   if (err && errno == ETIME) {
      return VK_TIMEOUT;
   } else if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_WAIT failed: %m");
   }

   return VK_SUCCESS;
}

static VkResult
vk_drm_syncobj_wait_all(struct vk_device *device,
                        uint32_t sync_count,
                        struct vk_sync **const syncs,
                        uint64_t *wait_values,
                        enum vk_sync_wait_type wait_type,
                        uint64_t abs_timeout_ns)
{
   return vk_drm_syncobj_wait(device, sync_count, syncs, wait_values,
                              wait_type_to_flags(wait_type) |
                              DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL,
                              abs_timeout_ns);
}

static VkResult
vk_drm_syncobj_wait_any(struct vk_device *device,
                        uint32_t sync_count,
                        struct vk_sync **const syncs,
                        uint64_t *wait_values,
                        enum vk_sync_wait_type wait_type,
                        uint64_t abs_timeout_ns)
{
   return vk_drm_syncobj_wait(device, sync_count, syncs, wait_values,
                              wait_type_to_flags(wait_type),
                              abs_timeout_ns);
}

static VkResult
vk_drm_syncobj_import_opaque_fd(struct vk_device *device,
                                struct vk_sync *sync,
                                int fd)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   uint32_t new_handle;
   int err = drmSyncobjFDToHandle(device->drm_fd, fd, &new_handle);
   if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD failed: %m");
   }

   err = drmSyncobjDestroy(device->drm_fd, sobj->syncobj);
   assert(!err);

   sobj->syncobj = new_handle;

   return VK_SUCCESS;
}

static VkResult
vk_drm_syncobj_export_opaque_fd(struct vk_device *device,
                                struct vk_sync *sync,
                                int *fd)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   int err = drmSyncobjHandleToFD(device->drm_fd, sobj->syncobj, fd);
   if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD failed: %m");
   }

   return VK_SUCCESS;
}

static VkResult
vk_drm_syncobj_import_sync_file(struct vk_device *device,
                                struct vk_sync *sync,
                                int sync_file)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   int err = drmSyncobjImportSyncFile(device->drm_fd, sobj->syncobj, sync_file);
   if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE failed: %m");
   }

   return VK_SUCCESS;
}

static VkResult
vk_drm_syncobj_export_sync_file(struct vk_device *device,
                                struct vk_sync *sync,
                                int *sync_file)
{
   struct vk_drm_syncobj *sobj = to_drm_syncobj(sync);

   assert(device->drm_fd >= 0);
   int err = drmSyncobjExportSyncFile(device->drm_fd, sobj->syncobj, sync_file);
   if (err) {
      return vk_errorf(device, VK_ERROR_UNKNOWN,
                       "DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD failed: %m");
   }

   return VK_SUCCESS;
}

const struct vk_sync_type vk_drm_binary_syncobj_type = {
   .init = vk_drm_binary_syncobj_init,
   .finish = vk_drm_syncobj_finish,
   .signal = vk_drm_syncobj_signal,
   .reset = vk_drm_syncobj_reset,
   .wait_all = vk_drm_syncobj_wait_all,
   .wait_any = vk_drm_syncobj_wait_any,
   .import_opaque_fd = vk_drm_syncobj_import_opaque_fd,
   .export_opaque_fd = vk_drm_syncobj_export_opaque_fd,
   .import_sync_file = vk_drm_syncobj_import_sync_file,
   .export_sync_file = vk_drm_syncobj_export_sync_file,
};

const struct vk_sync_type vk_drm_timeline_syncobj_type = {
   .init = vk_drm_timeline_syncobj_init,
   .finish = vk_drm_syncobj_finish,
   .signal = vk_drm_syncobj_signal,
   .get_value = vk_drm_syncobj_get_value,
   .wait_all = vk_drm_syncobj_wait_all,
   .wait_any = vk_drm_syncobj_wait_any,
   .import_opaque_fd = vk_drm_syncobj_import_opaque_fd,
   .export_opaque_fd = vk_drm_syncobj_export_opaque_fd,
};
