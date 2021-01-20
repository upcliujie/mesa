/*
 * Copyright © 2015 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "tu_private.h"

#include "util/debug.h"
#include "util/disk_cache.h"
#include "util/mesa-sha1.h"
#include "util/u_atomic.h"
#include "vulkan/util/vk_util.h"
#include "nir/nir_serialize.h"

struct tu_serialized_nir
{
   struct nir_shader_compiler_options nir_options;
   size_t size;
   char data[0];
};

struct tu_pipeline_cached_variant
{
   /* Needs reference counting for MergePipelineCache */
   uint32_t ref_cnt;

   struct tu_pipeline_key key;
   struct ir3_shader_variant *variant;
};

static uint32_t
sha1_hash(const void *sha1)
{
   return _mesa_hash_data(sha1, 20);
}

static bool
sha1_compare(const void *sha1_a, const void *sha1_b)
{
   return memcmp(sha1_a, sha1_b, 20) == 0;
}

static void
tu_pipeline_cached_variant_destory(struct tu_pipeline_cache *cache,
                                   struct tu_pipeline_cached_variant *cv)
{
   if (cv->variant)
      ralloc_free(cv->variant);
   vk_free(&cache->alloc, cv);
}

static inline void
tu_pipeline_cached_variant_ref(struct tu_pipeline_cached_variant *cv)
{
   assert(cv && cv->ref_cnt >= 1);
   p_atomic_inc(&cv->ref_cnt);
}

static inline void
tu_pipeline_cached_variant_unref(struct tu_pipeline_cache *cache,
                                 struct tu_pipeline_cached_variant *cv)
{
   assert(cv && cv->ref_cnt >= 1);
   if (p_atomic_dec_zero(&cv->ref_cnt))
      tu_pipeline_cached_variant_destory(cache, cv);
}

static void
tu_pipeline_cache_init(struct tu_pipeline_cache *cache,
                       struct tu_device *device)
{
   cache->device = device;
   pthread_mutex_init(&cache->mutex, NULL);

   cache->nir_cache = _mesa_hash_table_create(NULL, sha1_hash, sha1_compare);
   cache->variant_cache = _mesa_hash_table_create(NULL, sha1_hash, sha1_compare);
}

static void
tu_pipeline_cache_finish(struct tu_pipeline_cache *cache)
{
   if (cache->nir_cache) {
      hash_table_foreach(cache->nir_cache, entry) {
         struct tu_serialized_nir *snir = entry->data;
         if (snir)
            vk_free(&cache->alloc, snir);
      }
      _mesa_hash_table_destroy(cache->nir_cache, NULL);
   }

   if (cache->variant_cache) {
      hash_table_foreach(cache->variant_cache, entry) {
         struct tu_pipeline_cached_variant *cv = entry->data;
         tu_pipeline_cached_variant_unref(cache, cv);
      }
      _mesa_hash_table_destroy(cache->variant_cache, NULL);
   }

   pthread_mutex_destroy(&cache->mutex);
}

static struct hash_entry *
tu_pipeline_cache_search(struct tu_pipeline_cache *cache,
                         const unsigned char *sha1_key,
                         enum tu_pipeline_cache_type type)
{
   struct hash_entry *entry = NULL;

   pthread_mutex_lock(&cache->mutex);

   if (type == TU_CACHE_NIR) {
      if (cache->nir_cache)
         entry = _mesa_hash_table_search(cache->nir_cache, sha1_key);
   } else {
      if (cache->variant_cache)
         entry = _mesa_hash_table_search(cache->variant_cache, sha1_key);
   }

   pthread_mutex_unlock(&cache->mutex);

   return entry;
}

void
tu_pipeline_cache_nir_insert(struct tu_pipeline_cache *cache,
                             struct tu_pipeline_key *key,
                             struct nir_shader *nir)
{
   if (!cache || !cache->nir_cache)
      return;

   struct blob blob;
   blob_init(&blob);

   nir_serialize(&blob, nir, false);
   if (blob.out_of_memory) {
      blob_finish(&blob);
      return;
   }

   pthread_mutex_lock(&cache->mutex);

   struct tu_serialized_nir *snir =
         vk_zalloc(&cache->alloc, sizeof(*snir) + blob.size,
                   8, VK_SYSTEM_ALLOCATION_SCOPE_CACHE);
   snir->size = blob.size;
   snir->nir_options = *nir->options;
   memcpy(snir->data, blob.data, blob.size);

   blob_finish(&blob);

   _mesa_hash_table_insert(cache->nir_cache, key->sha1, snir);

   pthread_mutex_unlock(&cache->mutex);
}

void
tu_pipeline_cache_variant_insert(struct tu_pipeline_cache *cache,
                                 struct tu_pipeline_key *key,
                                 struct ir3_shader_variant *variant)
{
   if (!cache || !cache->variant_cache)
      return;

   struct tu_pipeline_cached_variant *cv =
         vk_zalloc(&cache->alloc, sizeof(*cv), 8,
                   VK_SYSTEM_ALLOCATION_SCOPE_CACHE);

   pthread_mutex_lock(&cache->mutex);

   cv->ref_cnt = 1;
   cv->key = *key;
   cv->variant = variant;
   /* Unparent the variant so the ownership of the variant
    * could move to the cache */
   ralloc_steal(NULL, variant);

   _mesa_hash_table_insert(cache->variant_cache, key->sha1, cv);
   pthread_mutex_unlock(&cache->mutex);
}

static struct tu_pipeline_cached_variant *
pipeline_cached_variant_from_blob(struct tu_pipeline_cache *cache,
                                struct blob_reader *blob)
{
   struct tu_pipeline_cached_variant *cv =
         vk_zalloc(&cache->alloc, sizeof(*cv), 8,
                   VK_SYSTEM_ALLOCATION_SCOPE_CACHE);

   cv->ref_cnt = 1;
   blob_copy_bytes(blob, cv->key.sha1, 20);

   struct ir3_shader_variant *v = rzalloc_size(NULL, sizeof(*v));

   blob_copy_bytes(blob, v, sizeof(*v));
   v->bin = rzalloc_size(v, v->info.size);
   blob_copy_bytes(blob, v->bin, v->info.size);

   if (!v->binning_pass) {
      v->const_state = rzalloc_size(v, sizeof(*v->const_state));
      blob_copy_bytes(blob, v->const_state, sizeof(*v->const_state));

      uint32_t immeds_size = v->const_state->immediates_size *
            sizeof(v->const_state->immediates[0]);
      v->const_state->immediates = ralloc_size(v->const_state, immeds_size);
      blob_copy_bytes(blob, v->const_state->immediates, immeds_size);
   }

   cv->variant = v;

   return cv;
}

static bool
pipeline_cached_variant_to_blob(const struct tu_pipeline_cached_variant *cv,
                             struct blob *blob)
{
   struct ir3_shader_variant *v = cv->variant;

   blob_write_bytes(blob, cv->key.sha1, 20);
   blob_write_bytes(blob, v, sizeof(*v));
   blob_write_bytes(blob, v->bin, v->info.size);

   if (!v->binning_pass) {
      blob_write_bytes(blob, v->const_state, sizeof(*v->const_state));
      uint32_t immeds_size = v->const_state->immediates_size *
            sizeof(v->const_state->immediates[0]);
      blob_write_bytes(blob, v->const_state->immediates, immeds_size);
   }

   return !blob->out_of_memory;
}

static void
tu_pipeline_cache_load(struct tu_pipeline_cache *cache,
                       const void *data,
                       size_t size)
{
   struct tu_device *device = cache->device;
   struct vk_pipeline_cache_header header;

   if (size < sizeof(header))
      return;
   memcpy(&header, data, sizeof(header));

   if (header.header_size < sizeof(header))
      return;
   if (header.header_version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
      return;
   if (header.vendor_id < 0 /* TODO */)
      return;
   if (header.device_id < 0 /* TODO */)
      return;
   if (memcmp(header.uuid, device->physical_device->cache_uuid,
              VK_UUID_SIZE) != 0)
      return;

   struct blob_reader blob;

   blob_reader_init(&blob, data, size);
   blob_copy_bytes(&blob, &header, sizeof(header));

   /* TODO. Handle the nir cache too.*/
   uint32_t count = blob_read_uint32(&blob);
   if (blob.overrun)
      return;

   for (uint32_t i = 0; i < count; i++) {
      struct tu_pipeline_cached_variant *cv =
         pipeline_cached_variant_from_blob(cache, &blob);
      if (!cv)
         break;
      _mesa_hash_table_insert(cache->variant_cache, cv->key.sha1, cv);
   }
}

VkResult
tu_CreatePipelineCache(VkDevice _device,
                       const VkPipelineCacheCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator,
                       VkPipelineCache *pPipelineCache)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   struct tu_pipeline_cache *cache;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);

   cache = vk_object_alloc(&device->vk, pAllocator, sizeof(*cache),
                           VK_OBJECT_TYPE_PIPELINE_CACHE);
   if (cache == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (pAllocator)
      cache->alloc = *pAllocator;
   else
      cache->alloc = device->vk.alloc;

   tu_pipeline_cache_init(cache, device);

   if (pCreateInfo->initialDataSize > 0) {
      tu_pipeline_cache_load(cache, pCreateInfo->pInitialData,
                             pCreateInfo->initialDataSize);
   }

   *pPipelineCache = tu_pipeline_cache_to_handle(cache);

   return VK_SUCCESS;
}

void
tu_DestroyPipelineCache(VkDevice _device,
                        VkPipelineCache _cache,
                        const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_pipeline_cache, cache, _cache);

   if (!cache)
      return;
   tu_pipeline_cache_finish(cache);

   vk_object_free(&device->vk, pAllocator, cache);
}

VkResult
tu_GetPipelineCacheData(VkDevice _device,
                        VkPipelineCache _cache,
                        size_t *pDataSize,
                        void *pData)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_pipeline_cache, cache, _cache);
   struct vk_pipeline_cache_header header = {
      .header_size = sizeof(header),
      .header_version = VK_PIPELINE_CACHE_HEADER_VERSION_ONE,
      .vendor_id = 0,
      .device_id = 0,
   };

   VkResult result = VK_SUCCESS;

   struct blob blob;
   if (pData) {
      blob_init_fixed(&blob, pData, *pDataSize);
   } else {
      blob_init_fixed(&blob, NULL, SIZE_MAX);
   }

   memcpy(header.uuid, device->physical_device->cache_uuid, VK_UUID_SIZE);
   blob_write_bytes(&blob, &header, sizeof(header));

   /* TODO. Handle the nir cache too.*/
   uint32_t count = 0;
   intptr_t count_offset = blob_reserve_uint32(&blob);
   if (count_offset < 0) {
      *pDataSize = 0;
      blob_finish(&blob);
      return VK_INCOMPLETE;
   }

   if (cache->variant_cache) {
      hash_table_foreach(cache->variant_cache, entry) {
         struct tu_pipeline_cached_variant *cv = entry->data;

         size_t save_size = blob.size;
         if (!pipeline_cached_variant_to_blob(cv, &blob)) {
            /* If it fails reset to the previous size and bail */
            blob.size = save_size;
            result = VK_INCOMPLETE;
            break;
         }

         count++;
      }
   }
   blob_overwrite_uint32(&blob, count_offset, count);

   *pDataSize = blob.size;
   blob_finish(&blob);

   return result;
}

static void
tu_pipeline_cache_merge(struct tu_pipeline_cache *dst,
                        struct tu_pipeline_cache *src)
{
   hash_table_foreach(src->variant_cache, entry) {
      struct tu_pipeline_cached_variant *cv = entry->data;

      if (_mesa_hash_table_search(dst->variant_cache, cv->key.sha1))
         continue;

      tu_pipeline_cached_variant_ref(cv);
      _mesa_hash_table_insert(dst->variant_cache, cv->key.sha1, cv);
   }
}

VkResult
tu_MergePipelineCaches(VkDevice _device,
                       VkPipelineCache destCache,
                       uint32_t srcCacheCount,
                       const VkPipelineCache *pSrcCaches)
{
   TU_FROM_HANDLE(tu_pipeline_cache, dst, destCache);

   /* TODO. Handle the nir cache too.*/
   if (!dst->variant_cache)
      return VK_SUCCESS;

   for (uint32_t i = 0; i < srcCacheCount; i++) {
      TU_FROM_HANDLE(tu_pipeline_cache, src, pSrcCaches[i]);

      if (!src->variant_cache)
         continue;

      tu_pipeline_cache_merge(dst, src);
   }

   return VK_SUCCESS;
}

void
tu_pipeline_hash_shader_module(struct tu_pipeline_key *key)
{
   unsigned char sha1[20];

   /* Need more to be hashed for the cache key? */
   _mesa_sha1_compute(key->module->code, key->module->code_size, sha1);

   memcpy(key->sha1, sha1, sizeof(key->sha1));
}

void
tu_pipeline_hash_variant(struct tu_pipeline_key *key)
{
   struct mesa_sha1 ctx;
   unsigned char sha1[20];
   const struct tu_shader *shader = key->shader;

   _mesa_sha1_init(&ctx);
   _mesa_sha1_update(&ctx, &key->key, sizeof(key->key));
   /* Simply we can use the existed shader disk-cache key. */
   _mesa_sha1_update(&ctx, &shader->ir3_shader->cache_key, sizeof(cache_key));
   _mesa_sha1_update(&ctx, &key->binning_pass, sizeof(key->binning_pass));

   _mesa_sha1_final(&ctx, sha1);

   memcpy(key->sha1, sha1, sizeof(key->sha1));
}

void *
tu_pipeline_cache_lookup(struct tu_pipeline_cache *cache,
                         struct tu_pipeline_key *key,
                         enum tu_pipeline_cache_type type)
{
   struct hash_entry *entry;

   if (!cache)
      return NULL;

   if ((entry = tu_pipeline_cache_search(cache, key->sha1, type))) {
      if (type == TU_CACHE_NIR) {
         struct tu_serialized_nir *snir = entry->data;

         struct blob_reader blob;
         blob_reader_init(&blob, snir->data, snir->size);

         nir_shader *nir = nir_deserialize(NULL, &snir->nir_options, &blob);
         if (blob.overrun) {
            ralloc_free(nir);
            nir = NULL;
         }

         return nir;
      } else {
         struct tu_pipeline_cached_variant *cv = entry->data;
         return cv->variant;
      }
   }

   return NULL;
}
