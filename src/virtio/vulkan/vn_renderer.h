/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_RENDERER_H
#define VN_RENDERER_H

#include "vn_common.h"

#include "vn_cs.h"

struct vn_renderer_info {
   struct {
      uint16_t vendor_id;
      uint16_t device_id;

      bool has_bus_info;
      uint16_t domain;
      uint8_t bus;
      uint8_t device;
      uint8_t function;
   } pci;

   bool has_cache_management;

   uint32_t max_sync_queue_count;

   /* hw capset */
   uint32_t wire_format_version;
   uint32_t vk_xml_version;
   uint32_t vk_ext_command_serialization_spec_version;
   uint32_t vk_mesa_venus_protocol_spec_version;
};

struct vn_renderer_bo {
   atomic_int refcount;

   uint32_t res_id;

   void (*destroy)(struct vn_renderer_bo *bo,
                   const VkAllocationCallbacks *alloc);

   /* allocate a CPU shared memory as the storage */
   VkResult (*init_cpu)(struct vn_renderer_bo *bo, VkDeviceSize size);

   /* import a VkDeviceMemory as the storage */
   VkResult (*init_gpu)(struct vn_renderer_bo *bo,
                        VkDeviceSize size,
                        vn_cs_object_id mem_id,
                        VkMemoryPropertyFlags flags,
                        VkExternalMemoryHandleTypeFlags external_handles);

   /* TODO import */
   int (*export_dmabuf)(struct vn_renderer_bo *bo);

   /* map is not thread-safe */
   void *(*map)(struct vn_renderer_bo *bo);

   void (*flush)(struct vn_renderer_bo *bo,
                 VkDeviceSize offset,
                 VkDeviceSize size);
   void (*invalidate)(struct vn_renderer_bo *bo,
                      VkDeviceSize offset,
                      VkDeviceSize size);
};

/*
 * A sync consists of a uint64_t counter.  The counter can be updated by CPU
 * or by GPU.  It can also be waited on by CPU or by GPU until it reaches
 * certain values.
 *
 * This models after timeline VkSemaphore rather than timeline drm_syncobj.
 * The main difference is that drm_syncobj can have unsignaled value 0.
 */
struct vn_renderer_sync {
   uint32_t sync_id;

   void (*destroy)(struct vn_renderer_sync *sync,
                   const VkAllocationCallbacks *alloc);

   /* a sync can be initialized/released multiple times */
   VkResult (*init)(struct vn_renderer_sync *sync,
                    uint64_t initial_val,
                    bool shareable,
                    bool binary);
   void (*release)(struct vn_renderer_sync *sync);

   /* TODO export/import */

   /* reset the counter */
   VkResult (*reset)(struct vn_renderer_sync *sync, uint64_t initial_val);

   /* read the current value from the counter */
   VkResult (*read)(struct vn_renderer_sync *sync, uint64_t *val);

   /* write a new value (larger than the current one) to the counter */
   VkResult (*write)(struct vn_renderer_sync *sync, uint64_t val);
};

struct vn_renderer_submit_batch {
   size_t cs_offset;
   size_t cs_size;

   /*
    * Submit cs to the virtual sync queue identified by sync_queue_index.  The
    * virtual queue is assumed to be associated with the physical sync queue
    * identified by sync_queue_id.  After the execution completes on the
    * physical sync queue, the virtual sync queue is signaled.
    *
    * sync_queue_index must be less than max_sync_queue_count.
    *
    * sync_queue_id specifies the object id of a VkQueue.
    *
    * When sync_queue_cpu is true, it specifies the special CPU sync queue,
    * and sync_queue_index/sync_queue_id are ignored.  TODO revisit this later
    */
   uint32_t sync_queue_index;
   vn_cs_object_id sync_queue_id;
   bool sync_queue_cpu;

   /* syncs to update when the virtual sync queue is signaled */
   struct vn_renderer_sync *const *syncs;
   /* TODO allow NULL when syncs are all binary? */
   const uint64_t *sync_values;
   uint32_t sync_count;
};

struct vn_renderer_submit {
   const struct vn_cs *cs;

   /* BOs to pin and to fence implicitly */
   struct vn_renderer_bo *const *bos;
   uint32_t bo_count;

   const struct vn_renderer_submit_batch *batches;
   uint32_t batch_count;
};

struct vn_renderer_wait {
   bool wait_any;
   uint64_t timeout;

   struct vn_renderer_sync *const *syncs;
   /* TODO allow NULL when syncs are all binary? */
   const uint64_t *sync_values;
   uint32_t sync_count;
};

struct vn_renderer {
   void (*destroy)(struct vn_renderer *renderer,
                   const VkAllocationCallbacks *alloc);

   void (*get_info)(struct vn_renderer *renderer,
                    struct vn_renderer_info *info);

   VkResult (*submit)(struct vn_renderer *renderer,
                      const struct vn_renderer_submit *submit);

   /*
    * On success, returns VK_SUCCESS or VK_TIMEOUT.  On failure, returns
    * VK_ERROR_DEVICE_LOST or out of device/host memory.
    */
   VkResult (*wait)(struct vn_renderer *renderer,
                    const struct vn_renderer_wait *wait);

   struct vn_renderer_bo *(*bo_create)(struct vn_renderer *renderer,
                                       const VkAllocationCallbacks *alloc,
                                       VkSystemAllocationScope alloc_scope);

   struct vn_renderer_sync *(*sync_create)(
      struct vn_renderer *renderer,
      const VkAllocationCallbacks *alloc,
      VkSystemAllocationScope alloc_scope);
};

VkResult
vn_renderer_create_virtgpu(struct vn_instance *instance,
                           const VkAllocationCallbacks *alloc,
                           struct vn_renderer **renderer);

VkResult
vn_renderer_create_vtest(struct vn_instance *instance,
                         const VkAllocationCallbacks *alloc,
                         struct vn_renderer **renderer);

static inline VkResult
vn_renderer_create(struct vn_instance *instance,
                   const VkAllocationCallbacks *alloc,
                   struct vn_renderer **renderer)
{
   if (VN_DEBUG(VTEST)) {
      VkResult result = vn_renderer_create_vtest(instance, alloc, renderer);
      if (result == VK_SUCCESS)
         return VK_SUCCESS;
   }

   return vn_renderer_create_virtgpu(instance, alloc, renderer);
}

static inline void
vn_renderer_destroy(struct vn_renderer *renderer,
                    const VkAllocationCallbacks *alloc)
{
   renderer->destroy(renderer, alloc);
}

static inline void
vn_renderer_get_info(struct vn_renderer *renderer,
                     struct vn_renderer_info *info)
{
   renderer->get_info(renderer, info);
}

static inline VkResult
vn_renderer_submit(struct vn_renderer *renderer,
                   const struct vn_renderer_submit *submit)
{
   return renderer->submit(renderer, submit);
}

static inline VkResult
vn_renderer_submit_cs(struct vn_renderer *renderer, const struct vn_cs *cs)
{
   const struct vn_renderer_submit submit = {
      .cs = cs,
      .batches =
         &(const struct vn_renderer_submit_batch){
            .cs_size = vn_cs_get_out_len(cs),
         },
      .batch_count = 1,
   };
   return vn_renderer_submit(renderer, &submit);
}

static inline VkResult
vn_renderer_wait(struct vn_renderer *renderer,
                 const struct vn_renderer_wait *wait)
{
   return renderer->wait(renderer, wait);
}

static inline VkResult
vn_renderer_bo_create_cpu(struct vn_renderer *renderer,
                          VkDeviceSize size,
                          const VkAllocationCallbacks *alloc,
                          VkSystemAllocationScope alloc_scope,
                          struct vn_renderer_bo **_bo)
{
   struct vn_renderer_bo *bo =
      renderer->bo_create(renderer, alloc, alloc_scope);
   if (!bo)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult result = bo->init_cpu(bo, size);
   if (result != VK_SUCCESS) {
      bo->destroy(bo, alloc);
      return result;
   }

   atomic_init(&bo->refcount, 1);

   *_bo = bo;
   return VK_SUCCESS;
}

static inline VkResult
vn_renderer_bo_create_gpu(struct vn_renderer *renderer,
                          VkDeviceSize size,
                          vn_cs_object_id mem_id,
                          VkMemoryPropertyFlags flags,
                          VkExternalMemoryHandleTypeFlags external_handles,
                          const VkAllocationCallbacks *alloc,
                          VkSystemAllocationScope alloc_scope,
                          struct vn_renderer_bo **_bo)
{
   struct vn_renderer_bo *bo =
      renderer->bo_create(renderer, alloc, alloc_scope);
   if (!bo)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult result = bo->init_gpu(bo, size, mem_id, flags, external_handles);
   if (result != VK_SUCCESS) {
      bo->destroy(bo, alloc);
      return result;
   }

   atomic_init(&bo->refcount, 1);

   *_bo = bo;
   return VK_SUCCESS;
}

static inline struct vn_renderer_bo *
vn_renderer_bo_ref(struct vn_renderer_bo *bo)
{
   const int old =
      atomic_fetch_add_explicit(&bo->refcount, 1, memory_order_relaxed);
   assert(old >= 1);

   return bo;
}

static inline void
vn_renderer_bo_unref(struct vn_renderer_bo *bo,
                     const VkAllocationCallbacks *alloc)
{
   const int old =
      atomic_fetch_sub_explicit(&bo->refcount, 1, memory_order_release);
   assert(old >= 1);

   if (old == 1) {
      atomic_thread_fence(memory_order_acquire);
      bo->destroy(bo, alloc);
   }
}

static inline int
vn_renderer_bo_export_dmabuf(struct vn_renderer_bo *bo)
{
   return bo->export_dmabuf(bo);
}

static inline void *
vn_renderer_bo_map(struct vn_renderer_bo *bo)
{
   return bo->map(bo);
}

static inline void
vn_renderer_bo_flush(struct vn_renderer_bo *bo,
                     VkDeviceSize offset,
                     VkDeviceSize end)
{
   bo->flush(bo, offset, end);
}

static inline void
vn_renderer_bo_invalidate(struct vn_renderer_bo *bo,
                          VkDeviceSize offset,
                          VkDeviceSize size)
{
   bo->invalidate(bo, offset, size);
}

static inline VkResult
vn_renderer_sync_create_cpu(struct vn_renderer *renderer,
                            const VkAllocationCallbacks *alloc,
                            VkSystemAllocationScope alloc_scope,
                            struct vn_renderer_sync **_sync)
{
   struct vn_renderer_sync *sync =
      renderer->sync_create(renderer, alloc, alloc_scope);
   if (!sync)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   const uint64_t initial_val = 0;
   const bool shareable = false;
   const bool binary = false;
   VkResult result = sync->init(sync, initial_val, shareable, binary);
   if (result != VK_SUCCESS) {
      sync->destroy(sync, alloc);
      return result;
   }

   *_sync = sync;
   return VK_SUCCESS;
}

static inline VkResult
vn_renderer_sync_create_fence(struct vn_renderer *renderer,
                              bool signaled,
                              VkExternalFenceHandleTypeFlags external_handles,
                              const VkAllocationCallbacks *alloc,
                              VkSystemAllocationScope alloc_scope,
                              struct vn_renderer_sync **_sync)
{
   struct vn_renderer_sync *sync =
      renderer->sync_create(renderer, alloc, alloc_scope);
   if (!sync)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   const uint64_t initial_val = signaled;
   const bool shareable = external_handles;
   const bool binary = true;
   VkResult result = sync->init(sync, initial_val, shareable, binary);
   if (result != VK_SUCCESS) {
      sync->destroy(sync, alloc);
      return result;
   }

   *_sync = sync;
   return VK_SUCCESS;
}

static inline VkResult
vn_renderer_sync_create_semaphore(
   struct vn_renderer *renderer,
   VkSemaphoreType type,
   uint64_t initial_val,
   VkExternalSemaphoreHandleTypeFlags external_handles,
   const VkAllocationCallbacks *alloc,
   VkSystemAllocationScope alloc_scope,
   struct vn_renderer_sync **_sync)
{
   struct vn_renderer_sync *sync =
      renderer->sync_create(renderer, alloc, alloc_scope);
   if (!sync)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   const bool shareable = external_handles;
   const bool binary = type == VK_SEMAPHORE_TYPE_BINARY;
   VkResult result = sync->init(sync, initial_val, shareable, binary);
   if (result != VK_SUCCESS) {
      sync->destroy(sync, alloc);
      return result;
   }

   *_sync = sync;
   return VK_SUCCESS;
}

static inline VkResult
vn_renderer_sync_create_empty(struct vn_renderer *renderer,
                              const VkAllocationCallbacks *alloc,
                              VkSystemAllocationScope alloc_scope,
                              struct vn_renderer_sync **_sync)
{
   struct vn_renderer_sync *sync =
      renderer->sync_create(renderer, alloc, alloc_scope);
   if (!sync)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   /* no init */

   *_sync = sync;
   return VK_SUCCESS;
}

static inline void
vn_renderer_sync_destroy(struct vn_renderer_sync *sync,
                         const VkAllocationCallbacks *alloc)
{
   sync->destroy(sync, alloc);
}

static inline void
vn_renderer_sync_release(struct vn_renderer_sync *sync)
{
   sync->release(sync);
}

static inline VkResult
vn_renderer_sync_reset(struct vn_renderer_sync *sync, uint64_t initial_val)
{
   return sync->reset(sync, initial_val);
}

static inline VkResult
vn_renderer_sync_read(struct vn_renderer_sync *sync, uint64_t *val)
{
   return sync->read(sync, val);
}

static inline VkResult
vn_renderer_sync_write(struct vn_renderer_sync *sync, uint64_t val)
{
   return sync->write(sync, val);
}

#endif /* VN_RENDERER_H */
