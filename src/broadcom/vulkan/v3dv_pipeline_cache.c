/*
 * Copyright © 2019 Raspberry Pi
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

#include "v3dv_private.h"
#include "vulkan/util/vk_util.h"
#include "util/blob.h"
#include "nir/nir_serialize.h"

static const bool dump_stats = false;
static const bool dump_stats_verbose = false;
static const bool dump_stats_on_destroy = false;

/* Shared for nir/variants */
#define V3DV_MAX_PIPELINE_CACHE_ENTRIES 4096

static uint32_t
sha1_hash_func(const void *sha1)
{
   return _mesa_hash_data(sha1, 20);
}

static bool
sha1_compare_func(const void *sha1_a, const void *sha1_b)
{
   return memcmp(sha1_a, sha1_b, 20) == 0;
}

struct serialized_nir {
   unsigned char sha1_key[20];
   size_t size;
   char data[0];
};

struct cache_entry {
   uint32_t ref_cnt;

   unsigned char sha1_key[20];

   struct v3dv_descriptor_map ubo_map;
   struct v3dv_descriptor_map ssbo_map;
   struct v3dv_descriptor_map sampler_map;
   struct v3dv_descriptor_map texture_map;

   struct v3dv_shader_variant *variants[BROADCOM_SHADER_STAGES];
};

static void
cache_dump_stats(struct v3dv_pipeline_cache *cache, bool verbose)
{
   if (!verbose)
      return;

   fprintf(stderr, "  NIR cache entries:      %d\n", cache->nir_stats.count);
   fprintf(stderr, "  NIR cache miss count:   %d\n", cache->nir_stats.miss);
   fprintf(stderr, "  NIR cache hit  count:   %d\n", cache->nir_stats.hit);

   fprintf(stderr, "  cache entries:      %d\n", cache->stats.count);
   fprintf(stderr, "  cache miss count:   %d\n", cache->stats.miss);
   fprintf(stderr, "  cache hit  count:   %d\n", cache->stats.hit);
}

void
v3dv_pipeline_cache_upload_nir(struct v3dv_pipeline *pipeline,
                               struct v3dv_pipeline_cache *cache,
                               nir_shader *nir,
                               unsigned char sha1_key[20])
{
   if (!cache || !cache->nir_cache)
      return;

   if (cache->nir_stats.count > V3DV_MAX_PIPELINE_CACHE_ENTRIES)
      return;

   pthread_mutex_lock(&cache->mutex);
   struct hash_entry *entry =
      _mesa_hash_table_search(cache->nir_cache, sha1_key);
   pthread_mutex_unlock(&cache->mutex);
   if (entry)
      return;

   struct blob blob;
   blob_init(&blob);

   nir_serialize(&blob, nir, false);
   if (blob.out_of_memory) {
      blob_finish(&blob);
      return;
   }

   pthread_mutex_lock(&cache->mutex);
   /* Because ralloc isn't thread-safe, we have to do all this inside the
    * lock.  We could unlock for the big memcpy but it's probably not worth
    * the hassle.
    */
   entry = _mesa_hash_table_search(cache->nir_cache, sha1_key);
   if (entry) {
      blob_finish(&blob);
      pthread_mutex_unlock(&cache->mutex);
      return;
   }

   struct serialized_nir *snir =
      ralloc_size(cache->nir_cache, sizeof(*snir) + blob.size);
   memcpy(snir->sha1_key, sha1_key, 20);
   snir->size = blob.size;
   memcpy(snir->data, blob.data, blob.size);

   blob_finish(&blob);

   cache->nir_stats.count++;
   if (dump_stats) {
      char sha1buf[41];
      _mesa_sha1_format(sha1buf, snir->sha1_key);
      fprintf(stderr, "pipeline cache %p, new nir entry %s\n", cache, sha1buf);
      cache_dump_stats(cache, dump_stats_verbose);
   }

   _mesa_hash_table_insert(cache->nir_cache, snir->sha1_key, snir);

   pthread_mutex_unlock(&cache->mutex);
}

nir_shader*
v3dv_pipeline_cache_search_for_nir(struct v3dv_pipeline *pipeline,
                                   struct v3dv_pipeline_cache *cache,
                                   const nir_shader_compiler_options *nir_options,
                                   unsigned char sha1_key[20])
{
   if (!cache || !cache->nir_cache)
      return NULL;

   if (dump_stats) {
      char sha1buf[41];
      _mesa_sha1_format(sha1buf, sha1_key);

      fprintf(stderr, "pipeline cache %p, search for nir %s\n", cache, sha1buf);
   }

   const struct serialized_nir *snir = NULL;

   pthread_mutex_lock(&cache->mutex);
   struct hash_entry *entry =
      _mesa_hash_table_search(cache->nir_cache, sha1_key);
   if (entry)
      snir = entry->data;
   pthread_mutex_unlock(&cache->mutex);

   if (snir) {
      struct blob_reader blob;
      blob_reader_init(&blob, snir->data, snir->size);

      /* We use context NULL as we want the p_stage to keep the reference to
       * nir, as we keep open the possibility of provide a shader variant
       * after cache creation
       */
      nir_shader *nir = nir_deserialize(NULL, nir_options, &blob);
      if (blob.overrun) {
         ralloc_free(nir);
      } else {
         cache->nir_stats.hit++;
         if (dump_stats) {
            fprintf(stderr, "\tnir cache hit: %p\n", nir);
            cache_dump_stats(cache, dump_stats_verbose);
         }
         return nir;
      }
   }

   cache->nir_stats.miss++;
   if (dump_stats) {
      fprintf(stderr, "\tnir cache miss\n");
      cache_dump_stats(cache, dump_stats_verbose);
   }

   return NULL;
}

void
v3dv_pipeline_cache_init(struct v3dv_pipeline_cache *cache,
                         struct v3dv_device *device,
                         const VkAllocationCallbacks *pAllocator,
                         bool cache_enabled)
{
   cache->device = device;
   pthread_mutex_init(&cache->mutex, NULL);

   if (cache_enabled) {
      if (pAllocator)
         cache->alloc = *pAllocator;
      else
         cache->alloc = device->vk.alloc;

      cache->nir_cache = _mesa_hash_table_create(NULL, sha1_hash_func,
                                                 sha1_compare_func);
      cache->nir_stats.miss = 0;
      cache->nir_stats.hit = 0;
      cache->nir_stats.count = 0;

      cache->cache = _mesa_hash_table_create(NULL, sha1_hash_func,
                                             sha1_compare_func);
      cache->stats.miss = 0;
      cache->stats.hit = 0;
      cache->stats.count = 0;
   } else {
      cache->nir_cache = NULL;
      cache->cache = NULL;
   }

}

/*
 * Note that we had cache_entry_ref and cache_entry_ref_variants because when
 * the pipeline looks up for info using a key, only the variants increase the
 * ref. Everything else will be copied.
 */
static void
cache_entry_ref_variants(struct cache_entry *cache_entry)
{
   for (uint8_t stage = 0; stage < BROADCOM_SHADER_STAGES; stage++) {
      if (cache_entry->variants[stage] != NULL)
         v3dv_shader_variant_ref(cache_entry->variants[stage]);
   }
}
/*
 * cache_entry refs are used to share the same entries between pipeline
 * caches, so handled internally.
 *
 * Additionally, as we can disable pipeline cache, we need to keep the
 * reference count on the variants, so pipeline can handle own/free variants
 * in the same way in both cases.
 */
static void
cache_entry_ref(struct cache_entry *cache_entry)
{
   assert(cache_entry && cache_entry->ref_cnt >= 1);
   p_atomic_inc(&cache_entry->ref_cnt);

   cache_entry_ref_variants(cache_entry);
}

static void
cache_entry_destroy(struct v3dv_pipeline_cache *cache,
                    struct cache_entry *cache_entry)
{
   vk_free(&cache->alloc, cache_entry);
}

static void
cache_entry_unref_variants(struct v3dv_device *device,
                           struct cache_entry *cache_entry)
{
   for (uint8_t stage = 0; stage < BROADCOM_SHADER_STAGES; stage++) {
      if (cache_entry->variants[stage] != NULL)
         v3dv_shader_variant_unref(device, cache_entry->variants[stage]);
   }
}

static void
cache_entry_unref(struct v3dv_pipeline_cache *cache,
                  struct cache_entry *cache_entry)
{
   assert(cache_entry && cache_entry->ref_cnt >= 1);
   cache_entry_unref_variants(cache->device, cache_entry);

   if (p_atomic_dec_zero(&cache_entry->ref_cnt))
      cache_entry_destroy(cache, cache_entry);
}

/*
 * It searchs for pipeline cached data, and fills the pipeline with it.
 *
 * FIXME: we use this method to fill up the cached data so we don't need to
 * expose the definition of cache_entry, but perhaps it would be clearer if it
 * returns the cached data, and let the caller to fill up.
 */
bool
v3dv_pipeline_cache_search_for_pipeline(struct v3dv_pipeline *pipeline,
                                        struct v3dv_pipeline_cache *cache)
{
   if (!cache || !cache->cache)
      return NULL;

   if (dump_stats) {
      char sha1buf[41];
      _mesa_sha1_format(sha1buf, pipeline->sha1);

      fprintf(stderr, "pipeline cache %p, search pipeline with key %s\n", cache, sha1buf);
   }

   pthread_mutex_lock(&cache->mutex);

   struct hash_entry *entry =
      _mesa_hash_table_search(cache->cache, pipeline->sha1);

   if (entry) {
      struct cache_entry *cache_entry =
         (struct cache_entry *) entry->data;

      cache->stats.hit++;
      if (dump_stats) {
         fprintf(stderr, "\tcache hit: %p\n", cache_entry);
         cache_dump_stats(cache, dump_stats_verbose);
      }

      /* Now the pipeline will use the existing variants so we ref them */
      if (cache_entry)
         cache_entry_ref_variants(cache_entry);

      if (pipeline->cs) {
         assert(cache_entry->variants[BROADCOM_SHADER_COMPUTE]);

         pipeline->cs->current_variant = cache_entry->variants[BROADCOM_SHADER_COMPUTE];
      } else {
         assert(cache_entry->variants[BROADCOM_SHADER_VERTEX]);
         assert(cache_entry->variants[BROADCOM_SHADER_VERTEX_BIN]);
         assert(cache_entry->variants[BROADCOM_SHADER_FRAGMENT]);

         pipeline->vs->current_variant =
            cache_entry->variants[BROADCOM_SHADER_VERTEX];
         pipeline->vs_bin->current_variant =
            cache_entry->variants[BROADCOM_SHADER_VERTEX_BIN];
         pipeline->fs->current_variant =
            cache_entry->variants[BROADCOM_SHADER_FRAGMENT];
      }

      memcpy(&pipeline->ubo_map, &cache_entry->ubo_map,
             sizeof(struct v3dv_descriptor_map));
      memcpy(&pipeline->ssbo_map, &cache_entry->ssbo_map,
             sizeof(struct v3dv_descriptor_map));
      memcpy(&pipeline->sampler_map, &cache_entry->sampler_map,
             sizeof(struct v3dv_descriptor_map));
      memcpy(&pipeline->texture_map, &cache_entry->texture_map,
             sizeof(struct v3dv_descriptor_map));

      pthread_mutex_unlock(&cache->mutex);
      return true;
   }

   cache->stats.miss++;
   if (dump_stats) {
      fprintf(stderr, "\tcache miss\n");
      cache_dump_stats(cache, dump_stats_verbose);
   }

   pthread_mutex_unlock(&cache->mutex);
   return false;
}

static struct cache_entry *
cache_entry_new(struct v3dv_pipeline_cache *cache,
                const unsigned char sha1_key[20],
                struct v3dv_shader_variant **variants,
                const bool variants_owned,
                const struct v3dv_descriptor_map *ubo_map,
                const struct v3dv_descriptor_map *ssbo_map,
                const struct v3dv_descriptor_map *sampler_map,
                const struct v3dv_descriptor_map *texture_map)
{
   size_t size = sizeof(struct cache_entry);
   struct cache_entry *new_entry =
      vk_object_zalloc(&cache->device->vk, &cache->alloc, size,
                       VK_SYSTEM_ALLOCATION_SCOPE_CACHE);

   if (new_entry == NULL)
      return NULL;

   new_entry->ref_cnt = 1;

   memcpy(new_entry->sha1_key, sha1_key, 20);

   memcpy(&new_entry->ubo_map, ubo_map, sizeof(struct v3dv_descriptor_map));
   memcpy(&new_entry->ssbo_map, ssbo_map, sizeof(struct v3dv_descriptor_map));
   memcpy(&new_entry->sampler_map, sampler_map, sizeof(struct v3dv_descriptor_map));
   memcpy(&new_entry->texture_map, texture_map, sizeof(struct v3dv_descriptor_map));

   for (uint8_t stage = 0; stage < BROADCOM_SHADER_STAGES; stage++)
      new_entry->variants[stage] = variants[stage];

   /* variants_owned is used to distinguish two cases: when you are uploading
    * pipeline data, so the variants first reference belongs to the pipeline,
    * or when we are deserializing pipeline cache data, so those newly created
    * variants belongs initially to the cache_entry.
    */
   if (!variants_owned)
      cache_entry_ref_variants(new_entry);

   return new_entry;
}

void
v3dv_pipeline_cache_upload_pipeline(struct v3dv_pipeline *pipeline,
                                    struct v3dv_pipeline_cache *cache)
{
   if (!cache || !cache->cache)
      return;

   if (cache->stats.count > V3DV_MAX_PIPELINE_CACHE_ENTRIES)
      return;

   pthread_mutex_lock(&cache->mutex);
   struct hash_entry *entry =
      _mesa_hash_table_search(cache->cache, pipeline->sha1);

   if (entry) {
      pthread_mutex_unlock(&cache->mutex);
      return;
   }

   struct v3dv_shader_variant *variants[BROADCOM_SHADER_STAGES] =
      {NULL, NULL, NULL, NULL};

   if (pipeline->cs != NULL) {
      variants[BROADCOM_SHADER_COMPUTE] = pipeline->cs->current_variant;
   } else {
      variants[BROADCOM_SHADER_VERTEX] = pipeline->vs->current_variant;
      variants[BROADCOM_SHADER_VERTEX_BIN] = pipeline->vs_bin->current_variant;
      variants[BROADCOM_SHADER_FRAGMENT] = pipeline->fs->current_variant;
   }

   struct cache_entry *new_entry =
      cache_entry_new(cache,
                      pipeline->sha1, variants, false,
                      &pipeline->ubo_map,
                      &pipeline->ssbo_map,
                      &pipeline->sampler_map,
                      &pipeline->texture_map);

   if (new_entry == NULL)
      return;

   _mesa_hash_table_insert(cache->cache, new_entry->sha1_key, new_entry);
   cache->stats.count++;
   if (dump_stats) {
      char sha1buf[41];
      _mesa_sha1_format(sha1buf, pipeline->sha1);

      fprintf(stderr, "pipeline cache %p, new cache entry with sha1 key %s\n\n",
              cache, sha1buf);
      cache_dump_stats(cache, dump_stats_verbose);
   }

   pthread_mutex_unlock(&cache->mutex);
}

static struct serialized_nir*
serialized_nir_create_from_blob(struct v3dv_pipeline_cache *cache,
                                struct blob_reader *blob)
{
   const unsigned char *sha1_key = blob_read_bytes(blob, 20);
   uint32_t snir_size = blob_read_uint32(blob);
   const char* snir_data = blob_read_bytes(blob, snir_size);
   if (blob->overrun)
      return NULL;

   struct serialized_nir *snir =
      ralloc_size(cache->nir_cache, sizeof(*snir) + snir_size);
   memcpy(snir->sha1_key, sha1_key, 20);
   snir->size = snir_size;
   memcpy(snir->data, snir_data, snir_size);

   return snir;
}

static struct v3dv_shader_variant*
shader_variant_create_from_blob(struct v3dv_device *device,
                                struct blob_reader *blob)
{
   VkResult result;

   gl_shader_stage stage = blob_read_uint32(blob);
   bool is_coord = blob_read_uint8(blob);

   uint32_t prog_data_size = blob_read_uint32(blob);
   /* FIXME: as we include the stage perhaps we can avoid prog_data_size? */
   assert(prog_data_size == v3d_prog_data_size(stage));

   const void *prog_data = blob_read_bytes(blob, prog_data_size);
   if (blob->overrun)
      return NULL;

   uint32_t ulist_count = blob_read_uint32(blob);
   uint32_t contents_size = sizeof(enum quniform_contents) * ulist_count;
   const void *contents_data = blob_read_bytes(blob, contents_size);
   if (blob->overrun)
      return NULL;

   uint ulist_data_size = sizeof(uint32_t) * ulist_count;
   const void *ulist_data_data = blob_read_bytes(blob, ulist_data_size);
   if (blob->overrun)
      return NULL;

   uint32_t qpu_insts_size = blob_read_uint32(blob);
   const uint64_t *qpu_insts = blob_read_bytes(blob, qpu_insts_size);
   if (blob->overrun)
      return NULL;

   /* shader_variant_create expects a newly created prog_data for their own,
    * as it is what the v3d compiler returns. So we are also allocating one
    * (including the uniform list) and filled it up with the data that we read
    * from the blob
    */
   struct v3d_prog_data *new_prog_data = rzalloc_size(NULL, prog_data_size);
   memcpy(new_prog_data, prog_data, prog_data_size);
   struct v3d_uniform_list *ulist = &new_prog_data->uniforms;
   ulist->count = ulist_count;
   ulist->contents = ralloc_array(new_prog_data, enum quniform_contents, ulist->count);
   memcpy(ulist->contents, contents_data, contents_size);
   ulist->data = ralloc_array(new_prog_data, uint32_t, ulist->count);
   memcpy(ulist->data, ulist_data_data, ulist_data_size);

   return v3dv_shader_variant_create(device, stage, is_coord,
                                     new_prog_data, prog_data_size,
                                     qpu_insts, qpu_insts_size,
                                     &result);
}

static struct cache_entry *
cache_entry_create_from_blob(struct v3dv_pipeline_cache *cache,
                             struct blob_reader *blob)
{
   const unsigned char *sha1_key = blob_read_bytes(blob, 20);

   const struct v3dv_descriptor_map *ubo_map =
      blob_read_bytes(blob, sizeof(struct v3dv_descriptor_map));
   const struct v3dv_descriptor_map *ssbo_map =
      blob_read_bytes(blob, sizeof(struct v3dv_descriptor_map));
   const struct v3dv_descriptor_map *sampler_map =
      blob_read_bytes(blob, sizeof(struct v3dv_descriptor_map));
   const struct v3dv_descriptor_map *texture_map =
      blob_read_bytes(blob, sizeof(struct v3dv_descriptor_map));

   if (blob->overrun)
      return NULL;

   uint8_t variant_count = blob_read_uint8(blob);

   struct v3dv_shader_variant *variants[BROADCOM_SHADER_STAGES] =
      {NULL, NULL, NULL, NULL};

   for (uint8_t count = 0; count < variant_count; count++) {
      uint8_t stage = blob_read_uint8(blob);
      struct v3dv_shader_variant *variant =
         shader_variant_create_from_blob(cache->device, blob);
      variants[stage] = variant;
   }

   return cache_entry_new(cache, sha1_key, variants, true,
                          ubo_map, ssbo_map, sampler_map, texture_map);
}

static void
pipeline_cache_load(struct v3dv_pipeline_cache *cache,
                    size_t size,
                    const void *data)
{
   struct v3dv_device *device = cache->device;
   struct v3dv_physical_device *pdevice = &device->instance->physicalDevice;
   struct vk_pipeline_cache_header header;

   if (cache->cache == NULL || cache->nir_cache == NULL)
      return;

   struct blob_reader blob;
   blob_reader_init(&blob, data, size);

   blob_copy_bytes(&blob, &header, sizeof(header));
   if (size < sizeof(header))
      return;
   memcpy(&header, data, sizeof(header));
   if (header.header_size < sizeof(header))
      return;
   if (header.header_version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
      return;
   if (header.vendor_id != v3dv_physical_device_vendor_id(pdevice))
      return;
   if (header.device_id != v3dv_physical_device_device_id(pdevice))
      return;
   if (memcmp(header.uuid, pdevice->pipeline_cache_uuid, VK_UUID_SIZE) != 0)
      return;

   uint32_t nir_count = blob_read_uint32(&blob);
   if (blob.overrun)
      return;

   for (uint32_t i = 0; i < nir_count; i++) {
      struct serialized_nir *snir =
         serialized_nir_create_from_blob(cache, &blob);

      if (!snir)
         break;

      _mesa_hash_table_insert(cache->nir_cache, snir->sha1_key, snir);
      cache->nir_stats.count++;
   }

   uint32_t count = blob_read_uint32(&blob);
   if (blob.overrun)
      return;

   for (uint32_t i = 0; i < count; i++) {
      struct cache_entry *cache_entry =
         cache_entry_create_from_blob(cache, &blob);
      if (!cache_entry)
         break;

      _mesa_hash_table_insert(cache->cache, cache_entry->sha1_key, cache_entry);
      cache->stats.count++;
   }

   if (dump_stats) {
      fprintf(stderr, "pipeline cache %p, loaded %i nir shaders and "
              "%i entries\n", cache, nir_count, count);
      cache_dump_stats(cache, dump_stats_verbose);
   }
}

VkResult
v3dv_CreatePipelineCache(VkDevice _device,
                         const VkPipelineCacheCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkPipelineCache *pPipelineCache)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   struct v3dv_pipeline_cache *cache;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);

   cache = vk_object_zalloc(&device->vk, pAllocator,
                            sizeof(*cache),
                            VK_OBJECT_TYPE_PIPELINE_CACHE);

   if (cache == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   v3dv_pipeline_cache_init(cache, device, pAllocator,
                            device->instance->pipeline_cache_enabled);

   if (pCreateInfo->initialDataSize > 0) {
      pipeline_cache_load(cache,
                          pCreateInfo->initialDataSize,
                          pCreateInfo->pInitialData);
   }

   *pPipelineCache = v3dv_pipeline_cache_to_handle(cache);

   return VK_SUCCESS;
}

void
v3dv_pipeline_cache_finish(struct v3dv_pipeline_cache *cache)
{
   pthread_mutex_destroy(&cache->mutex);

   if (dump_stats_on_destroy)
      cache_dump_stats(cache, true);

   if (cache->nir_cache) {
      hash_table_foreach(cache->nir_cache, entry)
         ralloc_free(entry->data);

      _mesa_hash_table_destroy(cache->nir_cache, NULL);
   }

   if (cache->cache) {
      hash_table_foreach(cache->cache, entry) {
         struct cache_entry *cache_entry = entry->data;
         if (cache_entry)
            cache_entry_unref(cache, cache_entry);
      }

      _mesa_hash_table_destroy(cache->cache, NULL);
   }

}

void
v3dv_DestroyPipelineCache(VkDevice _device,
                          VkPipelineCache _cache,
                          const VkAllocationCallbacks *pAllocator)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   V3DV_FROM_HANDLE(v3dv_pipeline_cache, cache, _cache);

   if (!cache)
      return;

   v3dv_pipeline_cache_finish(cache);

   vk_object_free(&device->vk, pAllocator, cache);
}

VkResult
v3dv_MergePipelineCaches(VkDevice device,
                         VkPipelineCache dstCache,
                         uint32_t srcCacheCount,
                         const VkPipelineCache *pSrcCaches)
{
   V3DV_FROM_HANDLE(v3dv_pipeline_cache, dst, dstCache);

   if (!dst->cache || !dst->nir_cache)
      return VK_SUCCESS;

   for (uint32_t i = 0; i < srcCacheCount; i++) {
      V3DV_FROM_HANDLE(v3dv_pipeline_cache, src, pSrcCaches[i]);
      if (!src->cache || !src->nir_cache)
         continue;

      hash_table_foreach(src->nir_cache, entry) {
         struct serialized_nir *src_snir = entry->data;
         assert(src_snir);

         if (_mesa_hash_table_search(dst->nir_cache, src_snir->sha1_key))
            continue;

         /* FIXME: we are using serialized nir shaders because they are
          * convenient to create and store on the cache, but requires to do a
          * copy here (and some other places) of the serialized NIR. Perhaps
          * it would make sense to move to handle the NIR shaders with shared
          * structures with ref counts, as the variants.
          */
         struct serialized_nir *snir_dst =
            ralloc_size(dst->nir_cache, sizeof(*snir_dst) + src_snir->size);
         memcpy(snir_dst->sha1_key, src_snir->sha1_key, 20);
         snir_dst->size = src_snir->size;
         memcpy(snir_dst->data, src_snir->data, src_snir->size);

         _mesa_hash_table_insert(dst->nir_cache, snir_dst->sha1_key, snir_dst);
         dst->nir_stats.count++;
         if (dump_stats) {
            char sha1buf[41];
            _mesa_sha1_format(sha1buf, snir_dst->sha1_key);

            fprintf(stderr, "pipeline cache %p, added nir entry %s "
                    "from pipeline cache %p\n",
                    dst, sha1buf, src);
            cache_dump_stats(dst, dump_stats_verbose);
         }
      }

      hash_table_foreach(src->cache, entry) {
         struct cache_entry *cache_entry = entry->data;
         assert(cache_entry);

         if (_mesa_hash_table_search(dst->cache, cache_entry->sha1_key))
            continue;

         cache_entry_ref(cache_entry);
         _mesa_hash_table_insert(dst->cache, cache_entry->sha1_key, cache_entry);

         dst->stats.count++;
         if (dump_stats) {
            char sha1buf[41];
            _mesa_sha1_format(sha1buf, cache_entry->sha1_key);

            fprintf(stderr, "pipeline cache %p, added entry %s "
                    "from pipeline cache %p\n",
                    dst, sha1buf, src);
            cache_dump_stats(dst, dump_stats_verbose);
         }
      }
   }

   return VK_SUCCESS;
}

static bool
shader_variant_write_to_blob(const struct v3dv_shader_variant *variant,
                             struct blob *blob)
{
   blob_write_uint32(blob, variant->stage);
   blob_write_uint8(blob, variant->is_coord);

   blob_write_uint32(blob, variant->prog_data_size);
   blob_write_bytes(blob, variant->prog_data.base, variant->prog_data_size);

   struct v3d_uniform_list *ulist = &variant->prog_data.base->uniforms;
   blob_write_uint32(blob, ulist->count);
   blob_write_bytes(blob, ulist->contents, sizeof(enum quniform_contents) * ulist->count);
   blob_write_bytes(blob, ulist->data, sizeof(uint32_t) * ulist->count);

   blob_write_uint32(blob, variant->qpu_insts_size);
   assert(variant->assembly_bo->map);
   blob_write_bytes(blob, variant->assembly_bo->map, variant->qpu_insts_size);

   return !blob->out_of_memory;
}

static bool
cache_entry_write_to_blob(const struct cache_entry *cache_entry,
                          struct blob *blob)
{
   blob_write_bytes(blob, cache_entry->sha1_key, 20);

   blob_write_bytes(blob, &cache_entry->ubo_map, sizeof(struct v3dv_descriptor_map));
   blob_write_bytes(blob, &cache_entry->ssbo_map, sizeof(struct v3dv_descriptor_map));
   blob_write_bytes(blob, &cache_entry->sampler_map, sizeof(struct v3dv_descriptor_map));
   blob_write_bytes(blob, &cache_entry->texture_map, sizeof(struct v3dv_descriptor_map));

   /* FIXME: this would change when we introduce Geometry Shaders */
   uint8_t variant_count = cache_entry->variants[BROADCOM_SHADER_COMPUTE] ? 1 : 3;
   blob_write_uint8(blob, variant_count);

   uint8_t real_count = 0;
   for (uint8_t stage = 0; stage < BROADCOM_SHADER_STAGES; stage++) {
      if (cache_entry->variants[stage] == NULL)
         continue;

      blob_write_uint8(blob, stage);
      if (!shader_variant_write_to_blob(cache_entry->variants[stage], blob))
         return false;
      real_count++;
   }
   assert(real_count == variant_count);

   return !blob->out_of_memory;
}


VkResult
v3dv_GetPipelineCacheData(VkDevice _device,
                          VkPipelineCache _cache,
                          size_t *pDataSize,
                          void *pData)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   V3DV_FROM_HANDLE(v3dv_pipeline_cache, cache, _cache);

   struct blob blob;
   if (pData) {
      blob_init_fixed(&blob, pData, *pDataSize);
   } else {
      blob_init_fixed(&blob, NULL, SIZE_MAX);
   }

   struct v3dv_physical_device *pdevice = &device->instance->physicalDevice;
   VkResult result = VK_SUCCESS;

   pthread_mutex_lock(&cache->mutex);

   struct vk_pipeline_cache_header header = {
      .header_size = sizeof(struct vk_pipeline_cache_header),
      .header_version = VK_PIPELINE_CACHE_HEADER_VERSION_ONE,
      .vendor_id = v3dv_physical_device_vendor_id(pdevice),
      .device_id = v3dv_physical_device_device_id(pdevice),
   };
   memcpy(header.uuid, pdevice->pipeline_cache_uuid, VK_UUID_SIZE);
   blob_write_bytes(&blob, &header, sizeof(header));

   uint32_t nir_count = 0;
   intptr_t nir_count_offset = blob_reserve_uint32(&blob);
   if (nir_count_offset < 0) {
      *pDataSize = 0;
      blob_finish(&blob);
      pthread_mutex_unlock(&cache->mutex);
      return VK_INCOMPLETE;
   }

   if (cache->nir_cache) {
      hash_table_foreach(cache->nir_cache, entry) {
         const struct serialized_nir *snir = entry->data;

         size_t save_size = blob.size;

         blob_write_bytes(&blob, snir->sha1_key, 20);
         blob_write_uint32(&blob, snir->size);
         blob_write_bytes(&blob, snir->data, snir->size);

         if (blob.out_of_memory) {
            blob.size = save_size;
            pthread_mutex_unlock(&cache->mutex);
            result = VK_INCOMPLETE;
            break;
         }

         nir_count++;
      }
   }
   blob_overwrite_uint32(&blob, nir_count_offset, nir_count);

   uint32_t count = 0;
   intptr_t count_offset = blob_reserve_uint32(&blob);
   if (count_offset < 0) {
      *pDataSize = 0;
      blob_finish(&blob);
      pthread_mutex_unlock(&cache->mutex);
      return VK_INCOMPLETE;
   }

   if (cache->cache) {
      hash_table_foreach(cache->cache, entry) {
         struct cache_entry *cache_entry = entry->data;

         size_t save_size = blob.size;
         if (!cache_entry_write_to_blob(cache_entry, &blob)) {
            /* If it fails reset to the previous size and bail */
            blob.size = save_size;
            pthread_mutex_unlock(&cache->mutex);
            result = VK_INCOMPLETE;
            break;
         }

         count++;
      }
   }

   blob_overwrite_uint32(&blob, count_offset, count);

   *pDataSize = blob.size;

   blob_finish(&blob);

   if (dump_stats) {
      assert(count <= cache->stats.count);
      fprintf(stderr, "GetPipelineCacheData: serializing cache %p, "
              "%i nir shader entries "
              "%i entries, %u DataSize\n",
              cache, nir_count, count, (uint32_t) *pDataSize);
   }

   pthread_mutex_unlock(&cache->mutex);

   return result;
}
