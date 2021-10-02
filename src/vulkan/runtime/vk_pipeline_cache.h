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
#ifndef VK_PIPELINE_CACHE_H
#define VK_PIPELINE_CACHE_H

#include "vk_object.h"
#include "vk_util.h"

#include "util/simple_mtx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* #include "util/blob.h" */
struct blob;
struct blob_reader;

/* #include "util/set.h" */
struct set;

struct vk_pipeline_cache;
struct vk_pipeline_cache_object;

#define VK_PIPELINE_CACHE_BLOB_ALIGN 8

struct vk_pipeline_cache_object_ops {
   bool (*serialize)(struct vk_pipeline_cache_object *object,
                     struct blob *blob);

   struct vk_pipeline_cache_object *(*deserialize)(struct vk_device *device,
                                                   struct blob_reader *blob);

   void (*destroy)(struct vk_pipeline_cache_object *object);
};

struct vk_pipeline_cache_object {
   struct vk_device *device;
   const struct vk_pipeline_cache_object_ops *ops;
   uint32_t ref_cnt;

   uint32_t data_size;
   const void *key_data;
   uint32_t key_size;
};

static inline void
vk_pipeline_cache_object_init(struct vk_device *device,
                              struct vk_pipeline_cache_object *object,
                              const struct vk_pipeline_cache_object_ops *ops,
                              const void *key_data, uint32_t key_size)
{
   object->device = device;
   object->ops = ops;
   p_atomic_set(&object->ref_cnt, 1);
   object->data_size = 0; /* Unknown */
   object->key_data = key_data;
   object->key_size = key_size;
}

static inline void
vk_pipeline_cache_object_finish(struct vk_pipeline_cache_object *object)
{
   assert(p_atomic_read(&object->ref_cnt) <= 1);
}

static inline struct vk_pipeline_cache_object *
vk_pipeline_cache_object_ref(struct vk_pipeline_cache_object *object)
{
   assert(object && p_atomic_read(&object->ref_cnt) >= 1);
   p_atomic_inc(&object->ref_cnt);
   return object;
}

static inline void
vk_pipeline_cache_object_unref(struct vk_pipeline_cache_object *object)
{
   assert(object && p_atomic_read(&object->ref_cnt) >= 1);
   if (p_atomic_dec_zero(&object->ref_cnt))
      object->ops->destroy(object);
}

struct vk_pipeline_cache {
   struct vk_object_base base;

   /* pCreateInfo::flags */
   VkPipelineCacheCreateFlags flags;

   struct vk_pipeline_cache_header header;

   /** Protects object_cache */
   simple_mtx_t lock;

   struct set *object_cache;
};

VK_DEFINE_HANDLE_CASTS(vk_pipeline_cache, base, VkPipelineCache,
                       VK_OBJECT_TYPE_PIPELINE_CACHE)

struct vk_pipeline_cache *
vk_pipeline_cache_create(struct vk_device *device,
                         const VkPipelineCacheCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         bool force_enable);
void
vk_pipeline_cache_destroy(struct vk_pipeline_cache *cache,
                          const VkAllocationCallbacks *pAllocator);

struct vk_pipeline_cache_object * MUST_CHECK
vk_pipeline_cache_lookup_object(struct vk_pipeline_cache *cache,
                                const void *key_data, size_t key_size,
                                const struct vk_pipeline_cache_object_ops *ops,
                                bool *cache_hit);

struct vk_pipeline_cache_object * MUST_CHECK
vk_pipeline_cache_add_object(struct vk_pipeline_cache *cache,
                             struct vk_pipeline_cache_object *object);

#ifdef __cplusplus
}
#endif

#endif /* VK_PIPELINE_CACHE_H */
