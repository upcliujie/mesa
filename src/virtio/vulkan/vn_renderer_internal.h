/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_RENDERER_INTERNAL_H
#define VN_RENDERER_INTERNAL_H

#include "vn_renderer.h"

struct vn_renderer_shmem_cache {
   simple_mtx_t mutex;

   struct vn_renderer_shmem_bucket {
      struct list_head shmems;
   } buckets[64];

   struct {
      uint32_t shmem_count;

      uint32_t cache_skip_count;
      uint32_t cache_hit_count;
      uint32_t cache_miss_count;
   } debug;
};

void
vn_renderer_shmem_cache_init(struct vn_renderer_shmem_cache *cache);

void
vn_renderer_shmem_cache_fini(struct vn_renderer_shmem_cache *cache,
                             void (*destroy)(struct vn_renderer *renderer,
                                             struct vn_renderer_shmem *shmem),
                             struct vn_renderer *renderer);

bool
vn_renderer_shmem_cache_add(struct vn_renderer_shmem_cache *cache,
                            struct vn_renderer_shmem *shmem);

struct vn_renderer_shmem *
vn_renderer_shmem_cache_get(struct vn_renderer_shmem_cache *cache,
                            size_t size);

void
vn_renderer_shmem_cache_debug_dump(struct vn_renderer_shmem_cache *cache);

#endif /* VN_RENDERER_INTERNAL_H */
