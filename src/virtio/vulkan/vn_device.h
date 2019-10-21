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

#include "vn_cs.h"
#include "vn_renderer.h"
#include "vn_wsi.h"

struct vn_instance {
   struct vn_cs_object base;

   VkAllocationCallbacks allocator;

   uint32_t api_version;
   struct vn_instance_extension_table enabled_extensions;

   struct vn_instance_dispatch_table dispatch;
   struct vn_physical_device_dispatch_table physical_device_dispatch;
   struct vn_device_dispatch_table device_dispatch;

   struct vn_renderer *renderer;
   struct vn_renderer_info renderer_info;
   uint32_t renderer_version;

   mtx_t cs_mutex;
   struct vn_cs cs;
   struct {
      struct vn_renderer_bo *bo;
      size_t size;
      size_t used;
      void *ptr;

      struct vn_renderer_sync *sync;
      uint64_t sync_value;
   } cs_reply;

   mtx_t physical_device_mutex;
   struct vn_physical_device *physical_devices;
   uint32_t physical_device_count;
};
VK_DEFINE_HANDLE_CASTS(vn_instance,
                       base.base,
                       VkInstance,
                       VK_OBJECT_TYPE_INSTANCE)

struct vn_physical_device {
   struct vn_cs_object base;

   struct vn_instance *instance;

   uint32_t renderer_version;
   struct vn_device_extension_table renderer_extensions;

   struct vn_device_extension_table supported_extensions;
   uint32_t *extension_spec_versions;

   VkPhysicalDeviceFeatures2 features;
   VkPhysicalDeviceVulkan11Features vulkan_1_1_features;
   VkPhysicalDeviceVulkan12Features vulkan_1_2_features;
   VkPhysicalDeviceTransformFeedbackFeaturesEXT transform_feedback_features;

   VkPhysicalDeviceProperties2 properties;
   VkPhysicalDeviceVulkan11Properties vulkan_1_1_properties;
   VkPhysicalDeviceVulkan12Properties vulkan_1_2_properties;
   VkPhysicalDeviceTransformFeedbackPropertiesEXT
      transform_feedback_properties;

   VkQueueFamilyProperties2 *queue_family_properties;
   uint32_t *queue_family_sync_queue_bases;
   uint32_t queue_family_count;

   VkPhysicalDeviceMemoryProperties2 memory_properties;

   struct wsi_device wsi_device;
};
VK_DEFINE_HANDLE_CASTS(vn_physical_device,
                       base.base,
                       VkPhysicalDevice,
                       VK_OBJECT_TYPE_PHYSICAL_DEVICE)

struct vn_device {
   struct vn_cs_device base;

   VkAllocationCallbacks allocator;

   struct vn_instance *instance;
   struct vn_physical_device *physical_device;
   struct vn_device_extension_table enabled_extensions;

   struct vn_device_dispatch_table dispatch;

   struct vn_queue *queues;
   uint32_t queue_count;
};
VK_DEFINE_HANDLE_CASTS(vn_device,
                       base.base.base,
                       VkDevice,
                       VK_OBJECT_TYPE_DEVICE)

struct vn_queue {
   struct vn_cs_object base;

   struct vn_device *device;
   uint32_t family;
   uint32_t index;
   uint32_t flags;

   uint32_t sync_queue_index;

   struct vn_renderer_sync *idle_sync;
   uint64_t idle_sync_value;
};
VK_DEFINE_HANDLE_CASTS(vn_queue, base.base, VkQueue, VK_OBJECT_TYPE_QUEUE)

enum vn_sync_type {
   /* no payload */
   VN_SYNC_TYPE_INVALID,

   /* When we signal or reset, we update both the device object and the
    * renderer sync.  When we wait or query, we use the renderer sync only.
    *
    * TODO VkFence does not need the device object
    */
   VN_SYNC_TYPE_SYNC,

   /* device object only; no renderer sync */
   VN_SYNC_TYPE_DEVICE_ONLY,

   /* already signaled by WSI */
   VN_SYNC_TYPE_WSI_SIGNALED,
};

struct vn_sync_payload {
   enum vn_sync_type type;
   struct vn_renderer_sync *sync;
};

struct vn_fence {
   struct vn_cs_object base;

   struct vn_sync_payload *payload;

   struct vn_sync_payload permanent;
   struct vn_sync_payload temporary;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_fence,
                               base.base,
                               VkFence,
                               VK_OBJECT_TYPE_FENCE)

struct vn_semaphore {
   struct vn_cs_object base;

   VkSemaphoreType type;

   struct vn_sync_payload *payload;

   struct vn_sync_payload permanent;
   struct vn_sync_payload temporary;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_semaphore,
                               base.base,
                               VkSemaphore,
                               VK_OBJECT_TYPE_SEMAPHORE)

struct vn_device_memory {
   struct vn_cs_object base;

   struct vn_renderer_bo *bo;
   VkDeviceSize size;
   VkDeviceSize map_end;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_device_memory,
                               base.base,
                               VkDeviceMemory,
                               VK_OBJECT_TYPE_DEVICE_MEMORY)

struct vn_buffer {
   struct vn_cs_object base;

   VkMemoryRequirements2 memory_requirements;
   VkMemoryDedicatedRequirements dedicated_requirements;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_buffer,
                               base.base,
                               VkBuffer,
                               VK_OBJECT_TYPE_BUFFER)

struct vn_buffer_view {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_buffer_view,
                               base.base,
                               VkBufferView,
                               VK_OBJECT_TYPE_BUFFER_VIEW)

struct vn_image {
   struct vn_cs_object base;

   VkMemoryRequirements2 memory_requirements[4];
   VkMemoryDedicatedRequirements dedicated_requirements[4];
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_image,
                               base.base,
                               VkImage,
                               VK_OBJECT_TYPE_IMAGE)

struct vn_image_view {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_image_view,
                               base.base,
                               VkImageView,
                               VK_OBJECT_TYPE_IMAGE_VIEW)

struct vn_sampler {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_sampler,
                               base.base,
                               VkSampler,
                               VK_OBJECT_TYPE_SAMPLER)

struct vn_sampler_ycbcr_conversion {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_sampler_ycbcr_conversion,
                               base.base,
                               VkSamplerYcbcrConversion,
                               VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION)

struct vn_descriptor_set_layout_binding {
   bool has_immutable_samplers;
};

struct vn_descriptor_set_layout {
   struct vn_cs_object base;
   struct vn_descriptor_set_layout_binding bindings[];
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_descriptor_set_layout,
                               base.base,
                               VkDescriptorSetLayout,
                               VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)

struct vn_descriptor_pool {
   struct vn_cs_object base;

   VkAllocationCallbacks allocator;
   struct list_head descriptor_sets;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_descriptor_pool,
                               base.base,
                               VkDescriptorPool,
                               VK_OBJECT_TYPE_DESCRIPTOR_POOL)

struct vn_update_descriptor_sets {
   uint32_t write_count;
   VkWriteDescriptorSet *writes;
   VkDescriptorImageInfo *images;
   VkDescriptorBufferInfo *buffers;
   VkBufferView *views;
};

struct vn_descriptor_set {
   struct vn_cs_object base;

   const struct vn_descriptor_set_layout *layout;
   struct list_head head;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_descriptor_set,
                               base.base,
                               VkDescriptorSet,
                               VK_OBJECT_TYPE_DESCRIPTOR_SET)

struct vn_descriptor_update_template_entry {
   size_t offset;
   size_t stride;
};

struct vn_descriptor_update_template {
   struct vn_cs_object base;

   mtx_t mutex;
   struct vn_update_descriptor_sets *update;

   struct vn_descriptor_update_template_entry entries[];
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_descriptor_update_template,
                               base.base,
                               VkDescriptorUpdateTemplate,
                               VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE)

struct vn_render_pass {
   struct vn_cs_object base;

   VkExtent2D granularity;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_render_pass,
                               base.base,
                               VkRenderPass,
                               VK_OBJECT_TYPE_RENDER_PASS)

struct vn_framebuffer {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_framebuffer,
                               base.base,
                               VkFramebuffer,
                               VK_OBJECT_TYPE_FRAMEBUFFER)

struct vn_event {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_event,
                               base.base,
                               VkEvent,
                               VK_OBJECT_TYPE_EVENT)

struct vn_query_pool {
   struct vn_cs_object base;

   VkAllocationCallbacks allocator;
   uint32_t result_array_size;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_query_pool,
                               base.base,
                               VkQueryPool,
                               VK_OBJECT_TYPE_QUERY_POOL)

struct vn_shader_module {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_shader_module,
                               base.base,
                               VkShaderModule,
                               VK_OBJECT_TYPE_SHADER_MODULE)

struct vn_pipeline_layout {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_pipeline_layout,
                               base.base,
                               VkPipelineLayout,
                               VK_OBJECT_TYPE_PIPELINE_LAYOUT)

struct vn_pipeline_cache {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_pipeline_cache,
                               base.base,
                               VkPipelineCache,
                               VK_OBJECT_TYPE_PIPELINE_CACHE)

struct vn_pipeline {
   struct vn_cs_object base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_pipeline,
                               base.base,
                               VkPipeline,
                               VK_OBJECT_TYPE_PIPELINE)

struct vn_command_pool {
   struct vn_cs_object base;

   VkAllocationCallbacks allocator;
   struct list_head command_buffers;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_command_pool,
                               base.base,
                               VkCommandPool,
                               VK_OBJECT_TYPE_COMMAND_POOL)

enum vn_command_buffer_state {
   VN_COMMAND_BUFFER_STATE_INITIAL,
   VN_COMMAND_BUFFER_STATE_RECORDING,
   VN_COMMAND_BUFFER_STATE_EXECUTABLE,
   VN_COMMAND_BUFFER_STATE_INVALID,
};

struct vn_command_buffer {
   struct vn_cs_object base;

   struct vn_device *device;

   struct list_head head;

   enum vn_command_buffer_state state;
   struct vn_cs cs;
};
VK_DEFINE_HANDLE_CASTS(vn_command_buffer,
                       base.base,
                       VkCommandBuffer,
                       VK_OBJECT_TYPE_COMMAND_BUFFER)

static inline struct vn_cs *
vn_instance_lock_cs(struct vn_instance *instance)
{
   mtx_lock(&instance->cs_mutex);
   return &instance->cs;
}

struct vn_renderer_bo *
vn_instance_get_cs_reply_bo_locked(struct vn_instance *instance,
                                   size_t size,
                                   void **ptr);

static inline bool
vn_instance_submit_cs_locked(struct vn_instance *instance,
                             struct vn_renderer_bo *reply_bo,
                             uint64_t *reply_sync_val)
{
   struct vn_cs *cs = &instance->cs;

   if (unlikely(vn_cs_has_error(cs))) {
      vn_cs_reset(cs);
      return false;
   }

   vn_cs_end_out(cs);

   VkResult result;
   if (reply_bo) {
      *reply_sync_val = ++instance->cs_reply.sync_value;
      const struct vn_renderer_submit submit = {
         .cs = cs,
         .bos = &reply_bo,
         .bo_count = 1,
         .batches =
            &(const struct vn_renderer_submit_batch){
               .cs_size = vn_cs_get_out_len(cs),
               .sync_queue_cpu = true,
               .syncs = &instance->cs_reply.sync,
               .sync_values = reply_sync_val,
               .sync_count = 1,
            },
         .batch_count = 1,
      };
      result = vn_renderer_submit(instance->renderer, &submit);
   } else {
      result = vn_renderer_submit_cs(instance->renderer, cs);
   }

   vn_cs_reset(cs);

   return result == VK_SUCCESS;
}

static inline void
vn_instance_unlock_cs(struct vn_instance *instance)
{
   assert(mtx_trylock(&instance->cs_mutex) == thrd_busy);
   mtx_unlock(&instance->cs_mutex);
}

static inline void
vn_instance_wait_cs_reply(struct vn_instance *instance,
                          uint64_t reply_sync_val)
{
   const struct vn_renderer_wait wait = {
      .timeout = UINT64_MAX,
      .syncs = &instance->cs_reply.sync,
      .sync_values = &reply_sync_val,
      .sync_count = 1,
   };
   vn_renderer_wait(instance->renderer, &wait);
}

static inline void
vn_instance_free_cs_reply_bo(struct vn_instance *instance,
                             struct vn_renderer_bo *bo)
{
   vn_renderer_bo_unref(bo, &instance->allocator);
}

void
vn_fence_signal_wsi(struct vn_device *dev, struct vn_fence *fence);

void
vn_semaphore_signal_wsi(struct vn_device *dev, struct vn_semaphore *sem);

#endif /* VN_DEVICE_H */
