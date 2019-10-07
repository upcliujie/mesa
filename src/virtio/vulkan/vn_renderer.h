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
      bool valid;
      uint16_t domain;
      uint8_t bus;
      uint8_t device;
      uint8_t function;
   } pci;

   bool supports_dmabuf;

   uint32_t sync_queue_count;

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
   VkResult (*init_shm)(struct vn_renderer_bo *bo, VkDeviceSize size);

   /* import a VkDeviceMemory as the storage */
   VkResult (*init_memory)(struct vn_renderer_bo *bo,
                           VkDeviceSize size,
                           vn_cs_object_id obj_id,
                           VkMemoryPropertyFlags flags,
                           VkExternalMemoryHandleTypeFlagBits external);

   int (*export_dmabuf)(struct vn_renderer_bo *bo);

   void *(*map)(struct vn_renderer_bo *bo);
   void (*flush)(struct vn_renderer_bo *bo,
                 VkDeviceSize offset,
                 VkDeviceSize size);
   void (*invalidate)(struct vn_renderer_bo *bo,
                      VkDeviceSize offset,
                      VkDeviceSize size);
};

/*
 * A sync consists of a uint64_t counter.  It can be updated by CPU or by GPU.
 * It can also be waited on by CPU or by GPU until it reaches a certain value.
 *
 * The caller should make sure the counter always increases monotonically.
 *
 * This models after timeline VkSemaphore rather than timeline drm_sync.
 */
struct vn_renderer_sync {
   uint32_t sync_id;

   void (*destroy)(struct vn_renderer_sync *sync,
                   const VkAllocationCallbacks *alloc);

   VkResult (*init)(struct vn_renderer_sync *sync,
                    uint64_t initial_point,
                    bool shareable,
                    bool binary);

   /* release the counter for later re-init */
   void (*release)(struct vn_renderer_sync *sync);

   /* reset the counter */
   VkResult (*reset)(struct vn_renderer_sync *sync, uint64_t initial_point);

   /* write a new value (larger than the current one) to the counter */
   VkResult (*write)(struct vn_renderer_sync *sync, uint64_t point);

   /* read the current value from the counter */
   VkResult (*read)(struct vn_renderer_sync *sync, uint64_t *point);
};

struct vn_renderer_submit {
   const struct vn_cs *cs;

   /* referenced BOs */
   struct vn_renderer_bo *const *bos;
   uint32_t bo_count;

   /* syncs to write when GPU queue completes */
   uint32_t sync_queue_index;
   uint64_t sync_queue_id;
   struct vn_renderer_sync *const *syncs;
   const uint64_t *sync_points;
   uint32_t sync_count;

   /* wait until CPU completes */
   /* TODO async */
   bool wait_cpu;
};

struct vn_renderer {
   void (*destroy)(struct vn_renderer *renderer,
                   const VkAllocationCallbacks *alloc);

   void (*get_info)(struct vn_renderer *renderer,
                    struct vn_renderer_info *info);

   VkResult (*submit)(struct vn_renderer *renderer,
                      const struct vn_renderer_submit *submit);

   /* when points is NULL, it is assumed to be all 1's */
   VkResult (*wait)(struct vn_renderer *renderer,
                    struct vn_renderer_sync *const *syncs,
                    const uint64_t *points,
                    uint32_t count,
                    bool wait_any,
                    uint64_t timeout);

   struct vn_renderer_bo *(*bo_create)(struct vn_renderer *renderer,
                                       const VkAllocationCallbacks *alloc,
                                       VkSystemAllocationScope alloc_scope);

   struct vn_renderer_sync *(*sync_create)(
      struct vn_renderer *renderer,
      const VkAllocationCallbacks *alloc,
      VkSystemAllocationScope alloc_scope);
};

VkResult
vn_renderer_create_vtest(struct vn_instance *instance,
                         const VkAllocationCallbacks *alloc,
                         struct vn_renderer **renderer);

static inline VkResult
vn_renderer_create(struct vn_instance *instance,
                   const VkAllocationCallbacks *alloc,
                   struct vn_renderer **renderer)
{
   if (VN_DEBUG(VTEST))
      return vn_renderer_create_vtest(instance, alloc, renderer);
   else
      return VK_ERROR_INCOMPATIBLE_DRIVER;
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
                   const struct vn_cs *cs,
                   struct vn_renderer_bo *const *bos,
                   uint32_t bo_count,
                   bool wait_cpu)
{
   struct vn_renderer_submit submit = {
      .cs = cs,
      .bos = bos,
      .bo_count = bo_count,
      .wait_cpu = wait_cpu,
   };

   return renderer->submit(renderer, &submit);
}

static inline VkResult
vn_renderer_wait(struct vn_renderer *renderer,
                 struct vn_renderer_sync *const *syncs,
                 const uint64_t *points,
                 uint32_t count,
                 bool wait_any,
                 uint64_t timeout)
{
   return renderer->wait(renderer, syncs, points, count, wait_any, timeout);
}

static inline VkResult
vn_renderer_bo_create_shm(struct vn_renderer *renderer,
                          VkDeviceSize size,
                          const VkAllocationCallbacks *alloc,
                          VkSystemAllocationScope alloc_scope,
                          struct vn_renderer_bo **_bo)
{
   struct vn_renderer_bo *bo =
      renderer->bo_create(renderer, alloc, alloc_scope);
   if (!bo)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult result = bo->init_shm(bo, size);
   if (result != VK_SUCCESS) {
      bo->destroy(bo, alloc);
      return result;
   }

   atomic_init(&bo->refcount, 1);

   *_bo = bo;
   return VK_SUCCESS;
}

static inline VkResult
vn_renderer_bo_create_memory(struct vn_renderer *renderer,
                             VkDeviceSize size,
                             vn_cs_object_id mem_id,
                             VkMemoryPropertyFlags flags,
                             VkExternalMemoryHandleTypeFlagBits external,
                             const VkAllocationCallbacks *alloc,
                             VkSystemAllocationScope alloc_scope,
                             struct vn_renderer_bo **_bo)
{
   struct vn_renderer_bo *bo =
      renderer->bo_create(renderer, alloc, alloc_scope);
   if (!bo)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult result = bo->init_memory(bo, size, mem_id, flags, external);
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
   return bo->export_dmabuf ? bo->export_dmabuf(bo) : -1;
}

static inline void *
vn_renderer_bo_map(struct vn_renderer_bo *bo)
{
   return bo->map ? bo->map(bo) : NULL;
}

static inline void
vn_renderer_bo_flush(struct vn_renderer_bo *bo,
                     VkDeviceSize offset,
                     VkDeviceSize end)
{
   if (bo->flush)
      bo->flush(bo, offset, end);
}

static inline void
vn_renderer_bo_invalidate(struct vn_renderer_bo *bo,
                          VkDeviceSize offset,
                          VkDeviceSize size)
{
   if (bo->invalidate)
      bo->invalidate(bo, offset, size);
}

static inline VkResult
vn_renderer_sync_create(struct vn_renderer *renderer,
                        const VkAllocationCallbacks *alloc,
                        VkSystemAllocationScope alloc_scope,
                        struct vn_renderer_sync **_sync)
{
   struct vn_renderer_sync *sync =
      renderer->sync_create(renderer, alloc, alloc_scope);
   if (!sync)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   *_sync = sync;
   return VK_SUCCESS;
}

static inline void
vn_renderer_sync_destroy(struct vn_renderer_sync *sync,
                         const VkAllocationCallbacks *alloc)
{
   sync->destroy(sync, alloc);
}

static inline VkResult
vn_renderer_sync_init_fence(struct vn_renderer_sync *sync,
                            bool signaled,
                            VkExternalFenceHandleTypeFlagBits external)
{
   const uint64_t initial_point = signaled;
   const bool shareable = external;
   const bool binary = true;
   return sync->init(sync, initial_point, shareable, binary);
}

static inline VkResult
vn_renderer_sync_init_semaphore(struct vn_renderer_sync *sync,
                                VkSemaphoreType type,
                                uint64_t initial_point,
                                VkExternalSemaphoreHandleTypeFlagBits external)
{
   const bool shareable = external;
   const bool binary = type == VK_SEMAPHORE_TYPE_BINARY;
   return sync->init(sync, initial_point, shareable, binary);
}

static inline void
vn_renderer_sync_release(struct vn_renderer_sync *sync)
{
   sync->release(sync);
}

static inline VkResult
vn_renderer_sync_reset(struct vn_renderer_sync *sync, uint64_t initial_point)
{
   return sync->reset(sync, initial_point);
}

static inline VkResult
vn_renderer_sync_write(struct vn_renderer_sync *sync, uint64_t point)
{
   return sync->write(sync, point);
}

static inline VkResult
vn_renderer_sync_read(struct vn_renderer_sync *sync, uint64_t *point)
{
   return sync->read(sync, point);
}

#endif /* VN_RENDERER_H */
