/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vn_renderer_internal.h"

void
vn_renderer_shmem_cache_init(struct vn_renderer_shmem_cache *cache)
{
   simple_mtx_init(&cache->mutex, mtx_plain);

   for (uint32_t i = 0; i < ARRAY_SIZE(cache->buckets); i++) {
      struct vn_renderer_shmem_bucket *bucket = &cache->buckets[i];
      list_inithead(&bucket->shmems);
   }

   cache->initialized = true;
}

void
vn_renderer_shmem_cache_fini(struct vn_renderer_shmem_cache *cache,
                             void (*destroy)(struct vn_renderer *renderer,
                                             struct vn_renderer_shmem *shmem),
                             struct vn_renderer *renderer)
{
   if (!cache->initialized)
      return;

   for (uint32_t i = 0; i < ARRAY_SIZE(cache->buckets); i++) {
      struct vn_renderer_shmem_bucket *bucket = &cache->buckets[i];
      list_for_each_entry_safe(struct vn_renderer_shmem, shmem,
                               &bucket->shmems, cache_head)
         destroy(renderer, shmem);
   }

   simple_mtx_destroy(&cache->mutex);
}

static struct vn_renderer_shmem_bucket *
choose_bucket(struct vn_renderer_shmem_cache *cache, size_t size, bool add)
{
   assert(size);
   if (unlikely(!util_is_power_of_two_or_zero64(size)))
      return NULL;

   const uint32_t idx = ffsll(size) - 1;
   if (unlikely(idx >= ARRAY_SIZE(cache->buckets)))
      return NULL;

   return &cache->buckets[idx];
}

bool
vn_renderer_shmem_cache_add(struct vn_renderer_shmem_cache *cache,
                            struct vn_renderer_shmem *shmem)
{
   assert(!vn_refcount_is_valid(&shmem->refcount));

   struct vn_renderer_shmem_bucket *bucket =
      choose_bucket(cache, shmem->mmap_size, true);
   if (!bucket)
      return false;

   simple_mtx_lock(&cache->mutex);
   list_add(&shmem->cache_head, &bucket->shmems);
   cache->debug.shmem_count++;
   simple_mtx_unlock(&cache->mutex);

   return true;
}

struct vn_renderer_shmem *
vn_renderer_shmem_cache_get(struct vn_renderer_shmem_cache *cache,
                            size_t size)
{
   struct vn_renderer_shmem_bucket *bucket =
      choose_bucket(cache, size, false);
   if (!bucket) {
      simple_mtx_lock(&cache->mutex);
      cache->debug.cache_skip_count++;
      simple_mtx_unlock(&cache->mutex);
      return NULL;
   }

   struct vn_renderer_shmem *shmem = NULL;

   simple_mtx_lock(&cache->mutex);
   if (list_is_empty(&bucket->shmems)) {
      cache->debug.cache_miss_count++;
   } else {
      shmem = list_first_entry(&bucket->shmems, struct vn_renderer_shmem,
                               cache_head);
      list_del(&shmem->cache_head);

      cache->debug.shmem_count--;
      cache->debug.cache_hit_count++;
   }
   simple_mtx_unlock(&cache->mutex);

   return shmem;
}

/* for debugging only */
void
vn_renderer_shmem_cache_debug_dump(struct vn_renderer_shmem_cache *cache)
{
   simple_mtx_lock(&cache->mutex);

   vn_log(NULL, "dumping shmem cache");
   vn_log(NULL, "  shmem count: %d", cache->debug.shmem_count);
   vn_log(NULL, "  cache skip: %d", cache->debug.cache_skip_count);
   vn_log(NULL, "  cache hit: %d", cache->debug.cache_hit_count);
   vn_log(NULL, "  cache miss: %d", cache->debug.cache_miss_count);
   for (uint32_t i = 0; i < ARRAY_SIZE(cache->buckets); i++) {
      const struct vn_renderer_shmem_bucket *bucket = &cache->buckets[i];
      uint32_t count = 0;
      list_for_each_entry(struct vn_renderer_shmem, shmem, &bucket->shmems,
                          cache_head)
         count++;
      if (count)
         vn_log(NULL, "  buckets[%d]: %d shmems", i, count);
   }

   simple_mtx_unlock(&cache->mutex);
}
