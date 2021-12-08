/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vn_renderer_util.h"

VkResult
vn_renderer_submit_simple_sync(struct vn_renderer *renderer,
                               const void *cs_data,
                               size_t cs_size)
{
   struct vn_renderer_sync *sync;
   VkResult result =
      vn_renderer_sync_create(renderer, 0, VN_RENDERER_SYNC_BINARY, &sync);
   if (result != VK_SUCCESS)
      return result;

   const struct vn_renderer_submit submit = {
      .batches =
         &(const struct vn_renderer_submit_batch){
            .cs_data = cs_data,
            .cs_size = cs_size,
            .sync_queue_cpu = true,
            .syncs = &sync,
            .sync_values = &(const uint64_t){ 1 },
            .sync_count = 1,
         },
      .batch_count = 1,
   };
   const struct vn_renderer_wait wait = {
      .timeout = UINT64_MAX,
      .syncs = &sync,
      .sync_values = &(const uint64_t){ 1 },
      .sync_count = 1,
   };

   result = vn_renderer_submit(renderer, &submit);
   if (result == VK_SUCCESS)
      result = vn_renderer_wait(renderer, &wait);

   vn_renderer_sync_destroy(renderer, sync);

   return result;
}

void
vn_renderer_shmem_pool_init(UNUSED struct vn_renderer *renderer,
                            struct vn_renderer_shmem_pool *pool,
                            size_t min_alloc_size)
{
   *pool = (struct vn_renderer_shmem_pool){
      .min_alloc_size = util_next_power_of_two(min_alloc_size),
   };
}

void
vn_renderer_shmem_pool_fini(struct vn_renderer *renderer,
                            struct vn_renderer_shmem_pool *pool)
{
   if (pool->shmem)
      vn_renderer_shmem_unref(renderer, pool->shmem);
}

static size_t
vn_renderer_shmem_pool_space(UNUSED struct vn_renderer *renderer,
                             const struct vn_renderer_shmem_pool *pool)
{
   return pool->shmem_size - pool->shmem_used;
}

static bool
vn_renderer_shmem_pool_realloc(struct vn_renderer *renderer,
                               struct vn_renderer_shmem_pool *pool,
                               size_t size)
{
   size_t alloc_size = pool->min_alloc_size;
   while (alloc_size < size) {
      alloc_size <<= 1;
      if (!alloc_size)
         return false;
   }

   struct vn_renderer_shmem *shmem =
      vn_renderer_shmem_create(renderer, alloc_size);
   if (!shmem)
      return false;

   if (pool->shmem)
      vn_renderer_shmem_unref(renderer, pool->shmem);

   pool->shmem = shmem;
   pool->shmem_size = alloc_size;
   pool->shmem_used = 0;

   return true;
}

struct vn_renderer_shmem *
vn_renderer_shmem_pool_alloc(struct vn_renderer *renderer,
                             struct vn_renderer_shmem_pool *pool,
                             size_t size,
                             size_t *out_offset)
{
   if (unlikely(size > vn_renderer_shmem_pool_space(renderer, pool))) {
      if (!vn_renderer_shmem_pool_realloc(renderer, pool, size))
         return NULL;

      assert(size <= vn_renderer_shmem_pool_space(renderer, pool));
   }

   struct vn_renderer_shmem *shmem =
      vn_renderer_shmem_ref(renderer, pool->shmem);
   *out_offset = pool->shmem_used;
   pool->shmem_used += size;

   return shmem;
}
