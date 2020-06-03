/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#ifndef VN_DEVICE_H
#define VN_DEVICE_H

#include "vn_common.h"

struct vn_instance {
   struct vk_object_base base;

   VkAllocationCallbacks allocator;

   uint32_t api_version;
   struct vn_instance_extension_table enabled_extensions;

   struct vn_instance_dispatch_table dispatch;
   struct vn_physical_device_dispatch_table physical_device_dispatch;
   struct vn_device_dispatch_table device_dispatch;
};
VK_DEFINE_HANDLE_CASTS(vn_instance, base, VkInstance, VK_OBJECT_TYPE_INSTANCE)

struct vn_physical_device {
   struct vk_object_base base;

   struct vn_instance *instance;
};
VK_DEFINE_HANDLE_CASTS(vn_physical_device,
                       base,
                       VkPhysicalDevice,
                       VK_OBJECT_TYPE_PHYSICAL_DEVICE)

struct vn_device {
   struct vk_device base;

   struct vn_device_dispatch_table dispatch;
};
VK_DEFINE_HANDLE_CASTS(vn_device, base.base, VkDevice, VK_OBJECT_TYPE_DEVICE)

struct vn_queue {
   struct vk_object_base base;

   struct vn_device *device;
};
VK_DEFINE_HANDLE_CASTS(vn_queue, base, VkQueue, VK_OBJECT_TYPE_QUEUE)

struct vn_command_buffer {
   struct vk_object_base base;

   struct vn_device *device;
};
VK_DEFINE_HANDLE_CASTS(vn_command_buffer,
                       base,
                       VkCommandBuffer,
                       VK_OBJECT_TYPE_COMMAND_BUFFER)

#endif /* VN_DEVICE_H */
