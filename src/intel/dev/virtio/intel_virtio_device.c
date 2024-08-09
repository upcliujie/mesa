/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "intel_device_info.h"
#include "intel_virtio_priv.h"

static simple_mtx_t dev_list_lock = SIMPLE_MTX_INITIALIZER;
static struct list_head dev_list = {
   .next = &dev_list,
   .prev = &dev_list,
};

struct intel_virtio_device *
fd_to_intel_virtio_device(int fd)
{
   struct intel_virtio_device *dev = NULL;

   simple_mtx_lock(&dev_list_lock);

   list_for_each_entry(struct intel_virtio_device, itr,
                       &dev_list, list_item) {
      int err = os_same_file_description(itr->fd, fd);
      if (!err) {
         dev = itr;
         break;
      }
   }

   simple_mtx_unlock(&dev_list_lock);

   return dev;
}

bool is_intel_virtio_fd(int fd)
{
   return fd_to_intel_virtio_device(fd) != NULL;
}

bool
intel_virtio_get_pci_device_info(int fd, struct intel_device_info *devinfo)
{
   struct intel_virtio_device *dev = fd_to_intel_virtio_device(fd);
   struct virgl_renderer_capset_drm caps;

   if (!dev)
      return false;

   caps = dev->vdrm->caps;

   devinfo->pci_bus = caps.u.intel.pci_bus;
   devinfo->pci_dev = caps.u.intel.pci_dev;
   devinfo->pci_func = caps.u.intel.pci_func;
   devinfo->pci_domain = caps.u.intel.pci_domain;
   devinfo->pci_device_id = caps.u.intel.pci_device_id;
   devinfo->pci_revision_id = caps.u.intel.pci_revision_id;

   return true;
}

static bool is_virtio_fd(int fd)
{
   drmVersionPtr version = drmGetVersion(fd);
   bool is_virtio = !strcmp(version->name, "virtio_gpu");
   drmFreeVersion(version);

   if (debug_get_bool_option("INTEL_VIRTIO_FORCE_VTEST", false))
      is_virtio = true;

   return is_virtio;
}

int intel_virtio_init_fd(int fd)
{
   if (!is_virtio_fd(fd))
      return 0;

   struct intel_virtio_device *dev = calloc(1, sizeof(*dev));
   if (!dev)
      return -EINVAL;

   if (debug_get_bool_option("INTEL_VIRTIO_FORCE_VTEST", false))
      dev->vdrm = vdrm_device_connect(-1, VIRTGPU_DRM_CONTEXT_INTEL);
   else
      dev->vdrm = vdrm_device_connect(fd, VIRTGPU_DRM_CONTEXT_INTEL);

   if (!dev->vdrm) {
      free(dev);
      return -EINVAL;
   }

   dev->fd = os_dupfd_cloexec(fd);

   p_atomic_set(&dev->refcnt, 1);

   simple_mtx_lock(&dev_list_lock);
   list_add(&dev->list_item, &dev_list);
   simple_mtx_unlock(&dev_list_lock);

   return 1;
}

void intel_virtio_ref_fd(int fd)
{
   struct intel_virtio_device *dev = fd_to_intel_virtio_device(fd);

   if (dev)
      p_atomic_inc(&dev->refcnt);
}

void intel_virtio_unref_fd(int fd)
{
   struct intel_virtio_device *dev = fd_to_intel_virtio_device(fd);

   if (dev && !p_atomic_dec_return(&dev->refcnt)) {
      simple_mtx_lock(&dev_list_lock);
      list_del(&dev->list_item);
      simple_mtx_unlock(&dev_list_lock);

      vdrm_device_close(dev->vdrm);
      close(dev->fd);
      free(dev);
   }
}
