/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_RENDERER_UTIL_H
#define VN_RENDERER_UTIL_H

#include "vn_renderer.h"

/* for suballocaions of short-lived shmems, not thread-safe */
struct vn_renderer_shmem_pool {
   size_t min_alloc_size;

   struct vn_renderer_shmem *shmem;
   size_t shmem_size;
   size_t shmem_used;
};

static inline VkResult
vn_renderer_submit_simple(struct vn_renderer *renderer,
                          const void *cs_data,
                          size_t cs_size)
{
   const struct vn_renderer_submit submit = {
      .batches =
         &(const struct vn_renderer_submit_batch){
            .cs_data = cs_data,
            .cs_size = cs_size,
         },
      .batch_count = 1,
   };
   return vn_renderer_submit(renderer, &submit);
}

VkResult
vn_renderer_submit_simple_sync(struct vn_renderer *renderer,
                               const void *cs_data,
                               size_t cs_size);

void
vn_renderer_shmem_pool_init(struct vn_renderer *renderer,
                            struct vn_renderer_shmem_pool *pool,
                            size_t min_alloc_size);

void
vn_renderer_shmem_pool_fini(struct vn_renderer *renderer,
                            struct vn_renderer_shmem_pool *pool);

bool
vn_renderer_shmem_pool_realloc(struct vn_renderer *renderer,
                               struct vn_renderer_shmem_pool *pool,
                               size_t size);

static inline size_t
vn_renderer_shmem_pool_space(struct vn_renderer *renderer,
                             const struct vn_renderer_shmem_pool *pool)
{
   return pool->shmem_size - pool->shmem_used;
}

static inline struct vn_renderer_shmem *
vn_renderer_shmem_pool_alloc(struct vn_renderer *renderer,
                             struct vn_renderer_shmem_pool *pool,
                             size_t size,
                             size_t *out_offset)
{
   struct vn_renderer_shmem *shmem = NULL;

   if (unlikely(size > vn_renderer_shmem_pool_space(renderer, pool))) {
      if (!vn_renderer_shmem_pool_realloc(renderer, pool, size))
         return NULL;
      assert(size <= vn_renderer_shmem_pool_space(renderer, pool));
   }

   shmem = vn_renderer_shmem_ref(renderer, pool->shmem);
   *out_offset = pool->shmem_used;
   pool->shmem_used += size;

   return shmem;
}

#endif /* VN_RENDERER_UTIL_H */
