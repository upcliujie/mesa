/*
 * Copyright © 2020 Mike Blumenkrantz
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
 * 
 * Authors:
 *    Mike Blumenkrantz <michael.blumenkrantz@gmail.com>
 */

#include "tgsi/tgsi_from_mesa.h"



#include "zink_context.h"
#include "zink_descriptors.h"
#include "zink_program.h"
#include "zink_resource.h"
#include "zink_screen.h"

#define XXH_INLINE_ALL
#include "util/xxhash.h"

#define MAX_SET_ITER_COUNT 4

void
debug_describe_zink_descriptor_pool(char *buf, const struct zink_descriptor_pool *ptr)
{
   sprintf(buf, "zink_descriptor_pool");
}

static bool
desc_state_equal(const void *a, const void *b)
{
   const struct zink_descriptor_state_key *a_k = (void*)a;
   const struct zink_descriptor_state_key *b_k = (void*)b;

   if (a_k->type != b_k->type)
      return false;

   for (unsigned i = 0; i < ZINK_SHADER_COUNT; i++) {
      if (a_k->descriptor_states[i].state[a_k->type] != b_k->descriptor_states[i].state[b_k->type])
         return false;
   }
   return true;
}

static uint32_t
desc_state_hash(const void *key)
{
   const struct zink_descriptor_state_key *d_key = (void*)key;
   uint32_t hash = 0;
   for (unsigned i = 0; i < ZINK_SHADER_COUNT; i++) {
      if (d_key->descriptor_states[i].state[d_key->type])
         hash = maybe_hash_u32(d_key->descriptor_states[i].state[d_key->type], hash);
   }
   return hash;
}

static struct zink_descriptor_pool *
descriptor_pool_create(struct zink_screen *screen, enum zink_descriptor_type type, VkDescriptorSetLayoutBinding *bindings, unsigned num_bindings, VkDescriptorPoolSize *sizes, unsigned num_type_sizes)
{
   struct zink_descriptor_pool *pool = rzalloc(NULL, struct zink_descriptor_pool);
   if (!pool)
      return NULL;
   pipe_reference_init(&pool->reference, 1);
   pool->type = type;
   pool->num_descriptors = num_bindings;
   for (unsigned i = 0; i < num_bindings; i++) {
       pool->num_resources += bindings[i].descriptorCount;
   }
   pool->desc_sets = _mesa_hash_table_create(NULL, desc_state_hash, desc_state_equal);
   if (!pool->desc_sets)
      goto fail;

   pool->free_desc_sets = _mesa_hash_table_create(NULL, desc_state_hash, desc_state_equal);
   if (!pool->free_desc_sets)
      goto fail;

   util_dynarray_init(&pool->alloc_desc_sets, NULL);

   VkDescriptorSetLayoutCreateInfo dcslci = {};
   dcslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   dcslci.pNext = NULL;
   dcslci.flags = 0;
   dcslci.bindingCount = num_bindings;
   dcslci.pBindings = bindings;
   if (vkCreateDescriptorSetLayout(screen->dev, &dcslci, 0, &pool->dsl) != VK_SUCCESS) {
      debug_printf("vkCreateDescriptorSetLayout failed\n");
      goto fail;
   }

   VkDescriptorPoolCreateInfo dpci = {};
   dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   dpci.pPoolSizes = sizes;
   dpci.poolSizeCount = num_type_sizes;
   dpci.flags = 0;
   dpci.maxSets = ZINK_DEFAULT_MAX_DESCS;
   if (vkCreateDescriptorPool(screen->dev, &dpci, 0, &pool->descpool) != VK_SUCCESS) {
      debug_printf("vkCreateDescriptorPool failed\n");
      goto fail;
   }

   return pool;
fail:
   zink_descriptor_pool_free(screen, pool);
   return NULL;
}

static struct zink_descriptor_pool *
descriptor_pool_get(struct zink_context *ctx, enum zink_descriptor_type type, VkDescriptorSetLayoutBinding *bindings, unsigned num_bindings, VkDescriptorPoolSize *sizes, unsigned num_type_sizes)
{
   uint32_t hash = 0;

   for (unsigned i = 0; i < num_bindings; i++)
      hash = XXH32(&bindings[i], sizeof(VkDescriptorSetLayoutBinding), hash);
   for (unsigned i = 0; i < num_type_sizes; i++)
      hash = XXH32(&sizes[i], sizeof(VkDescriptorPoolSize), hash);

   struct hash_entry *he = _mesa_hash_table_search_pre_hashed(ctx->descriptor_pools[type], hash, (void*)(uintptr_t)hash);
   if (he)
      return (void*)he->data;
   struct zink_descriptor_pool *pool = descriptor_pool_create(zink_screen(ctx->base.screen), type, bindings, num_bindings, sizes, num_type_sizes);
   _mesa_hash_table_insert_pre_hashed(ctx->descriptor_pools[type], hash, (void*)(uintptr_t)hash, pool);
   return pool;
}

static bool
get_invalidated_desc_set(struct hash_entry *he)
{
   static int count = 0;
   struct zink_descriptor_set *zds = he->data;

   /* only skip the first few valid sets since this can end up being very time consuming */
   count += !zds->invalid;
   if (count > MAX_SET_ITER_COUNT || zds->invalid) {
      count = 0;
      return true;
   }
   return zds->invalid;
}

static struct zink_descriptor_set *
allocate_desc_set(struct zink_screen *screen, struct zink_program *pg, enum zink_descriptor_type type, unsigned descs_used, bool is_compute)
{
   VkDescriptorSetAllocateInfo dsai;
   struct zink_descriptor_pool *pool = pg->pool[type];
#define DESC_BUCKET_FACTOR 10
   unsigned bucket_size = pool->num_descriptors ? DESC_BUCKET_FACTOR : 1;
   unsigned desc_factor = DESC_BUCKET_FACTOR;
   if (pool->num_descriptors) {
      do {
         if (descs_used >= desc_factor)
            bucket_size = desc_factor;
         else
            break;
         desc_factor *= DESC_BUCKET_FACTOR;
      } while (1);
   }
   VkDescriptorSetLayout layouts[bucket_size];
   memset((void *)&dsai, 0, sizeof(dsai));
   dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   dsai.pNext = NULL;
   dsai.descriptorPool = pool->descpool;
   dsai.descriptorSetCount = bucket_size;
   for (unsigned i = 0; i < bucket_size; i ++)
      layouts[i] = pool->dsl;
   dsai.pSetLayouts = layouts;

   VkDescriptorSet desc_set[bucket_size];
   if (vkAllocateDescriptorSets(screen->dev, &dsai, desc_set) != VK_SUCCESS) {
      debug_printf("ZINK: %p failed to allocate descriptor set :/\n", pg);
      return VK_NULL_HANDLE;
   }

   struct zink_descriptor_set *alloc = ralloc_array(pool, struct zink_descriptor_set, bucket_size);
   assert(alloc);
   unsigned num_resources = pool->num_resources;
   struct zink_resource_object **res_objs = rzalloc_array(pool, struct zink_resource_object*, num_resources * bucket_size);
   assert(res_objs);
   void **samplers = NULL;
   if (type == ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW) {
      samplers = rzalloc_array(pool, void*, num_resources * bucket_size);
      assert(samplers);
   }
   for (unsigned i = 0; i < bucket_size; i ++) {
      struct zink_descriptor_set *zds = &alloc[i];
      pipe_reference_init(&zds->reference, 1);
      zds->pool = pool;
      zds->hash = 0;
      zds->batch_uses.usage = 0;
      zds->invalid = true;
      zds->recycled = false;
      if (num_resources) {
         util_dynarray_init(&zds->barriers, alloc);
         if (!util_dynarray_grow(&zds->barriers, struct zink_descriptor_barrier, num_resources)) {
            debug_printf("ZINK: %p failed to allocate descriptor set barriers :/\n", pg);
            return NULL;
         }
      }
#ifndef NDEBUG
      zds->num_resources = num_resources;
#endif
      if (type == ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW) {
         zds->sampler_views = (struct zink_sampler_view**)&res_objs[i * pool->num_descriptors];
         zds->samplers = (struct zink_sampler**)&samplers[i * pool->num_descriptors];
      } else
         zds->res_objs = (struct zink_resource_object**)&res_objs[i * pool->num_descriptors];
      zds->desc_set = desc_set[i];
      if (i > 0)
         util_dynarray_append(&pool->alloc_desc_sets, struct zink_descriptor_set *, zds);
   }
   pool->num_sets_allocated += bucket_size;
   return alloc;
}

static void
punt_invalid_set(struct zink_descriptor_set *zds)
{
  /* this is no longer usable, so we punt it for now until it gets recycled */
   struct hash_entry *he = _mesa_hash_table_search_pre_hashed(zds->pool->desc_sets, zds->hash, &zds->key);
   if (he)
      _mesa_hash_table_remove(zds->pool->desc_sets, he);
   //printf("%u PUNT %u\n", zds->pool->type, zds->hash);
   zds->hash = 0;
}

struct zink_descriptor_set *
zink_descriptor_set_get(struct zink_context *ctx,
                               enum zink_descriptor_type type,
                               bool is_compute,
                               bool *cache_hit,
                               bool *need_resource_refs)
{
   struct zink_descriptor_set *zds;
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   struct zink_program *pg = is_compute ? (struct zink_program *)ctx->curr_compute : (struct zink_program *)ctx->curr_program;
   struct zink_batch *batch = &ctx->batch;
   struct zink_descriptor_pool *pool = pg->pool[type];
   unsigned descs_used = 1;
   assert(type < ZINK_DESCRIPTOR_TYPES);
   uint32_t hash = pool->num_descriptors ? ctx->descriptor_states[is_compute].state[type] : 0;
   assert(hash || !pool->num_descriptors);

   struct zink_descriptor_state cs_state[ZINK_SHADER_COUNT] = {};
   cs_state[0] = ctx->descriptor_states[is_compute];
   struct zink_descriptor_state *states = is_compute ? cs_state : ctx->gfx_descriptor_states;
   struct zink_descriptor_state_key key = { states, type };

   if (pg->last_set[type] && pg->last_set[type]->hash == hash) {
      zds = pg->last_set[type];
      *cache_hit = !zds->invalid;
      if (hash) {
         if (zds->recycled) {
            struct hash_entry *he = _mesa_hash_table_search_pre_hashed(pool->free_desc_sets, hash, &key);
            if (he)
               _mesa_hash_table_remove(pool->free_desc_sets, he);
         } else if (zds->invalid && zink_batch_usage_exists(&zds->batch_uses)) {
             punt_invalid_set(zds);
             zds = NULL;
         }
      }
      if (zds) {
         //printf("%u LAST %s %u %p %u\n", type, zds->invalid ? "MISS" : "HIT", batch->state->batch_id, pg, hash);
         goto out;
      }
   }

   if (hash) {
      struct hash_entry *he = _mesa_hash_table_search_pre_hashed(pool->desc_sets, hash, &key);
      bool recycled = false, punted = false;
      if (he) {
          zds = (void*)he->data;
          if (zds->invalid && zink_batch_usage_exists(&zds->batch_uses)) {
             punt_invalid_set(zds);
             zds = NULL;
             punted = true;
          }
      }
      if (!he) {
         he = _mesa_hash_table_search_pre_hashed(pool->free_desc_sets, hash, &key);
         recycled = true;
      }
      if (he && !punted) {
         zds = (void*)he->data;
         *cache_hit = !zds->invalid;
         //printf("%u CACHE %s%u %p %u -> %u %s\n", type, zds->invalid ? "MISS" : "HIT", batch->state->batch_id, pg, he->hash, hash, recycled ? "RECYCLED" : "VALID");
         if (recycled) {
            /* need to migrate this entry back to the in-use hash */
            _mesa_hash_table_remove(pool->free_desc_sets, he);
            goto out;
         }
         goto quick_out;
      }
      *cache_hit = false;

      if (util_dynarray_num_elements(&pool->alloc_desc_sets, struct zink_descriptor_set *)) {
         /* grab one off the allocated array */
         zds = util_dynarray_pop(&pool->alloc_desc_sets, struct zink_descriptor_set *);
         //printf("%u POP%u %p %u (%lu left)\n", type, batch->state->batch_id, pg, hash,
                //util_dynarray_num_elements(&pool->alloc_desc_sets, struct zink_descriptor_set *));
         goto out;
      }

      if (_mesa_hash_table_num_entries(pool->free_desc_sets)) {
         /* try for an invalidated set first */
         he = _mesa_hash_table_random_entry(pool->free_desc_sets, get_invalidated_desc_set);
         if (!he)
            he = _mesa_hash_table_random_entry(pool->free_desc_sets, NULL);
         if (he) {
            zds = (void*)he->data;
            assert(p_atomic_read(&zds->reference.count) == 1);
            zink_descriptor_set_invalidate(zds);
            _mesa_hash_table_remove(pool->free_desc_sets, he);
            //printf("%u REUSE%u %p %u (%u total - %u / %u)\n", type, batch->state->batch_id, pg, hash,
                   //_mesa_hash_table_num_entries(pool->desc_sets) + _mesa_hash_table_num_entries(pool->free_desc_sets) + 1,
                   //_mesa_hash_table_num_entries(pool->desc_sets) + 1, _mesa_hash_table_num_entries(pool->free_desc_sets));
            goto out;
         }
      }

      if (pool->num_sets_allocated + pool->num_descriptors > ZINK_DEFAULT_MAX_DESCS) {
         zink_wait_on_batch(ctx, 0);
         zink_batch_reference_program(batch, pg);
         return zink_descriptor_set_get(ctx, type, is_compute, cache_hit, need_resource_refs);
      }
   } else {
      if (pg->last_set[type] && !pg->last_set[type]->hash) {
         zds = pg->last_set[type];
         *cache_hit = true;
         goto quick_out;
      }
   }

   zds = allocate_desc_set(screen, pg, type, descs_used, is_compute);
   //if (hash)
      //printf("%u NEW%u %p %u (%u total - %u / %u)\n", type, batch->state->batch_id, pg, hash,
             //_mesa_hash_table_num_entries(pool->desc_sets) + _mesa_hash_table_num_entries(pool->free_desc_sets),
             //_mesa_hash_table_num_entries(pool->desc_sets), _mesa_hash_table_num_entries(pool->free_desc_sets));
out:
   zds->hash = hash;
   zds->recycled = false;
   for (unsigned i = 0; i < ZINK_SHADER_COUNT; i++)
      zds->descriptor_states[i].state[type] = states[i].state[type];
   zds->key.type = type;
   zds->key.descriptor_states = zds->descriptor_states;
   if (hash)
      _mesa_hash_table_insert_pre_hashed(pool->desc_sets, hash, &zds->key, zds);
   else {
      /* we can safely apply the null set to all the slots which will need it here */
      for (unsigned i = 0; i < ZINK_DESCRIPTOR_TYPES; i++) {
         if (!pool->num_descriptors)
            pg->last_set[i] = zds;
      }
   }
quick_out:
   if (pool->num_descriptors && !*cache_hit)
      util_dynarray_clear(&zds->barriers);
   zds->invalid = false;
   *need_resource_refs = false;
   if (zink_batch_add_desc_set(batch, zds)) {
      batch->state->descs_used += pool->num_descriptors;
      *need_resource_refs = true;
   }
   pg->last_set[type] = zds;
   return zds;
}

void
zink_descriptor_set_recycle(struct zink_descriptor_set *zds)
{
   struct zink_descriptor_pool *pool = zds->pool;
   /* if desc set is still in use by a batch, don't recache */
   uint32_t refcount = p_atomic_read(&zds->reference.count);
   if (refcount != 1)
      return;
   /* this is a null set */
   if (!zds->hash && !pool->num_descriptors)
      return;

   if (zds->hash) {
      /* if we've previously punted this set, then it won't have a hash or be in either of the tables */
      struct hash_entry *he = _mesa_hash_table_search_pre_hashed(pool->desc_sets, zds->hash, &zds->key);
      if (!he)
         /* desc sets can be used multiple times in the same batch */
         return;
      _mesa_hash_table_remove(pool->desc_sets, he);
   }

   if (zds->invalid) {
      zink_descriptor_set_invalidate(zds);
      util_dynarray_append(&pool->alloc_desc_sets, struct zink_descriptor_set *, zds);
   } else {
      zds->recycled = true;
      _mesa_hash_table_insert_pre_hashed(pool->free_desc_sets, zds->hash, &zds->key, zds);
   }
}


static void
desc_set_ref_add(struct zink_descriptor_set *zds, struct zink_descriptor_refs *refs, void **ref_ptr, void *ptr)
{
   struct zink_descriptor_reference ref = {ref_ptr, &zds->invalid};
   *ref_ptr = ptr;
   if (ptr)
      util_dynarray_append(&refs->refs, struct zink_descriptor_reference, ref);
}

void
zink_image_view_desc_set_add(struct zink_image_view *image_view, struct zink_descriptor_set *zds, unsigned idx)
{
   desc_set_ref_add(zds, &image_view->desc_set_refs, (void**)&zds->image_views[idx], image_view);
}

void
zink_sampler_desc_set_add(struct zink_sampler *sampler, struct zink_descriptor_set *zds, unsigned idx)
{
   desc_set_ref_add(zds, &sampler->desc_set_refs, (void**)&zds->samplers[idx], sampler);
}

void
zink_sampler_view_desc_set_add(struct zink_sampler_view *sampler_view, struct zink_descriptor_set *zds, unsigned idx)
{
   desc_set_ref_add(zds, &sampler_view->desc_set_refs, (void**)&zds->sampler_views[idx], sampler_view);
}

void
zink_resource_desc_set_add(struct zink_resource *res, struct zink_descriptor_set *zds, unsigned idx)
{
   desc_set_ref_add(zds, res ? &res->obj->desc_set_refs : NULL, (void**)&zds->res_objs[idx], res ? res->obj : NULL);
}

void
zink_descriptor_set_refs_clear(struct zink_descriptor_refs *refs, void *ptr)
{
   util_dynarray_foreach(&refs->refs, struct zink_descriptor_reference, ref) {
      if (*ref->ref == ptr) {
         *ref->invalid = true;
         *ref->ref = NULL;
      }
   }
   util_dynarray_fini(&refs->refs);
}

bool
zink_descriptor_program_init(struct zink_context *ctx,
                       struct zink_shader *stages[ZINK_SHADER_COUNT],
                       struct zink_program *pg)
{
   VkDescriptorSetLayoutBinding bindings[ZINK_DESCRIPTOR_TYPES][PIPE_SHADER_TYPES * 32];
   int num_bindings[ZINK_DESCRIPTOR_TYPES] = {};

   VkDescriptorPoolSize sizes[6] = {};
   int type_map[12];
   unsigned num_types = 0;
   memset(type_map, -1, sizeof(type_map));

   for (int i = 0; i < ZINK_SHADER_COUNT; i++) {
      struct zink_shader *shader = stages[i];
      if (!shader)
         continue;

      VkShaderStageFlagBits stage_flags = zink_shader_stage(pipe_shader_type_from_mesa(shader->nir->info.stage));
      for (int j = 0; j < ZINK_DESCRIPTOR_TYPES; j++) {
         for (int k = 0; k < shader->num_bindings[j]; k++) {
            assert(num_bindings[j] < ARRAY_SIZE(bindings[j]));
            bindings[j][num_bindings[j]].binding = shader->bindings[j][k].binding;
            bindings[j][num_bindings[j]].descriptorType = shader->bindings[j][k].type;
            bindings[j][num_bindings[j]].descriptorCount = shader->bindings[j][k].size;
            bindings[j][num_bindings[j]].stageFlags = stage_flags;
            bindings[j][num_bindings[j]].pImmutableSamplers = NULL;
            if (type_map[shader->bindings[j][k].type] == -1) {
               type_map[shader->bindings[j][k].type] = num_types++;
               sizes[type_map[shader->bindings[j][k].type]].type = shader->bindings[j][k].type;
            }
            sizes[type_map[shader->bindings[j][k].type]].descriptorCount += shader->bindings[j][k].size;
            ++num_bindings[j];
         }
      }
   }

   unsigned total_descs = 0;
   for (unsigned i = 0; i < ZINK_DESCRIPTOR_TYPES; i++) {
      total_descs += num_bindings[i];;
   }
   if (!total_descs) return true;

   for (int i = 0; i < num_types; i++)
      sizes[i].descriptorCount *= ZINK_DEFAULT_MAX_DESCS;

   bool found_descriptors = false;
   for (unsigned i = ZINK_DESCRIPTOR_TYPES - 1; i < ZINK_DESCRIPTOR_TYPES; i--) {
      struct zink_descriptor_pool *pool;
      if (!num_bindings[i]) {
         if (!found_descriptors)
            continue;
         VkDescriptorSetLayoutBinding null_binding;
         null_binding.binding = 1;
         null_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
         null_binding.descriptorCount = 1;
         null_binding.pImmutableSamplers = NULL;
         null_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                                   VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                                   VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
         VkDescriptorPoolSize null_size = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ZINK_DEFAULT_MAX_DESCS};
         pool = descriptor_pool_get(ctx, i, &null_binding, 1, &null_size, 1);
         if (!pool)
            return false;
         pool->num_descriptors = 0;
         zink_descriptor_pool_reference(zink_screen(ctx->base.screen), &pg->pool[i], pool);
         continue;
      }
      found_descriptors = true;

      VkDescriptorPoolSize type_sizes[2] = {};
      int num_type_sizes = 0;
      switch (i) {
      case ZINK_DESCRIPTOR_TYPE_UBO:
         if (type_map[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] != -1) {
            type_sizes[num_type_sizes] = sizes[type_map[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER]];
            num_type_sizes++;
         }
         if (type_map[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] != -1) {
            type_sizes[num_type_sizes] = sizes[type_map[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC]];
            num_type_sizes++;
         }
         break;
      case ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW:
         if (type_map[VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER] != -1) {
            type_sizes[num_type_sizes] = sizes[type_map[VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER]];
            num_type_sizes++;
         }
         if (type_map[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] != -1) {
            type_sizes[num_type_sizes] = sizes[type_map[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]];
            num_type_sizes++;
         }
         break;
      case ZINK_DESCRIPTOR_TYPE_SSBO:
         if (type_map[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] != -1) {
            num_type_sizes = 1;
            type_sizes[0] = sizes[type_map[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER]];
         }
         break;
      case ZINK_DESCRIPTOR_TYPE_IMAGE:
         if (type_map[VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER] != -1) {
            type_sizes[num_type_sizes] = sizes[type_map[VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER]];
            num_type_sizes++;
         }
         if (type_map[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE] != -1) {
            type_sizes[num_type_sizes] = sizes[type_map[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE]];
            num_type_sizes++;
         }
         break;
      }
      pool = descriptor_pool_get(ctx, i, bindings[i], num_bindings[i], type_sizes, num_type_sizes);
      if (!pool)
         return false;
      zink_descriptor_pool_reference(zink_screen(ctx->base.screen), &pg->pool[i], pool);
   }
   return true;
}

void
zink_descriptor_set_invalidate(struct zink_descriptor_set *zds)
{
   zds->hash = 0;
   zds->invalid = true;
}

static void
descriptor_pool_clear(struct hash_table *ht)
{
 //printf("CLEAR %p\n", pg);
   hash_table_foreach(ht, entry) {
      struct zink_descriptor_set *zds = entry->data;
      zink_descriptor_set_invalidate(zds);
   }
   _mesa_hash_table_clear(ht, NULL);
}

void
zink_descriptor_pool_free(struct zink_screen *screen, struct zink_descriptor_pool *pool)
{
   if (!pool)
      return;
   if (pool->dsl)
      vkDestroyDescriptorSetLayout(screen->dev, pool->dsl, NULL);
   if (pool->descpool)
      vkDestroyDescriptorPool(screen->dev, pool->descpool, NULL);

#ifndef NDEBUG
   if (pool->desc_sets)
      descriptor_pool_clear(pool->desc_sets);
   if (pool->free_desc_sets)
      descriptor_pool_clear(pool->free_desc_sets);
#endif
   if (pool->desc_sets)
      _mesa_hash_table_destroy(pool->desc_sets, NULL);
   if (pool->free_desc_sets)
      _mesa_hash_table_destroy(pool->free_desc_sets, NULL);

   util_dynarray_fini(&pool->alloc_desc_sets);
   ralloc_free(pool);
}

void
zink_descriptor_pool_deinit(struct zink_context *ctx)
{
   for (unsigned i = 0; i < ZINK_DESCRIPTOR_TYPES; i++) {
      hash_table_foreach(ctx->descriptor_pools[i], entry) {
         struct zink_descriptor_pool *pool = (void*)entry->data;
         zink_descriptor_pool_reference(zink_screen(ctx->base.screen), &pool, NULL);
      }
      _mesa_hash_table_destroy(ctx->descriptor_pools[i], NULL);
   }
}

bool
zink_descriptor_pool_init(struct zink_context *ctx)
{
   for (unsigned i = 0; i < ZINK_DESCRIPTOR_TYPES; i++) {
      ctx->descriptor_pools[i] = _mesa_hash_table_create(ctx, NULL, _mesa_key_pointer_equal);
      if (!ctx->descriptor_pools[i])
         return false;
   }
   return true;
}


static void
desc_set_res_add(struct zink_descriptor_set *zds, struct zink_resource *res, unsigned int i, bool cache_hit)
{
   /* if we got a cache hit, we have to verify that the cached set is still valid;
    * we store the vk resource to the set here to avoid a more complex and costly mechanism of maintaining a
    * hash table on every resource with the associated descriptor sets that then needs to be iterated through
    * whenever a resource is destroyed
    */
   assert(!cache_hit || zds->res_objs[i] == (res ? res->obj : NULL));
   if (!cache_hit)
      zink_resource_desc_set_add(res, zds, i);
}

static void
desc_set_sampler_add(struct zink_context *ctx, struct zink_descriptor_set *zds, struct zink_sampler_view *sv,
                     struct zink_sampler *sampler, unsigned int i, bool is_buffer, bool cache_hit)
{
   /* if we got a cache hit, we have to verify that the cached set is still valid;
    * we store the vk resource to the set here to avoid a more complex and costly mechanism of maintaining a
    * hash table on every resource with the associated descriptor sets that then needs to be iterated through
    * whenever a resource is destroyed
    */
#ifndef NDEBUG
   uint32_t cur_hash = zink_get_sampler_view_hash(ctx, zds->sampler_views[i], is_buffer);
   uint32_t new_hash = zink_get_sampler_view_hash(ctx, sv, is_buffer);
#endif
   assert(!cache_hit || cur_hash == new_hash);
   assert(!cache_hit || zds->samplers[i] == sampler);
   if (!cache_hit) {
      zink_sampler_view_desc_set_add(sv, zds, i);
      zink_sampler_desc_set_add(sampler, zds, i);
   }
}

static void
desc_set_image_add(struct zink_context *ctx, struct zink_descriptor_set *zds, struct zink_image_view *image_view,
                   unsigned int i, bool is_buffer, bool cache_hit)
{
   /* if we got a cache hit, we have to verify that the cached set is still valid;
    * we store the vk resource to the set here to avoid a more complex and costly mechanism of maintaining a
    * hash table on every resource with the associated descriptor sets that then needs to be iterated through
    * whenever a resource is destroyed
    */
#ifndef NDEBUG
   uint32_t cur_hash = zink_get_image_view_hash(ctx, zds->image_views[i], is_buffer);
   uint32_t new_hash = zink_get_image_view_hash(ctx, image_view, is_buffer);
#endif
   assert(!cache_hit || cur_hash == new_hash);
   if (!cache_hit)
      zink_image_view_desc_set_add(image_view, zds, i);
}

static bool
barrier_equals(const void *a, const void *b)
{
   const struct zink_descriptor_barrier *t1 = a, *t2 = b;
   if (t1->res != t2->res)
      return false;
   if ((t1->access & t2->access) != t2->access)
      return false;
   if (t1->layout != t2->layout)
      return false;
   return true;
}

static uint32_t
barrier_hash(const void *key)
{
   return _mesa_hash_data(key, offsetof(struct zink_descriptor_barrier, stage));
}

static inline void
add_barrier(struct zink_resource *res, VkImageLayout layout, VkAccessFlags flags, enum pipe_shader_type stage, struct util_dynarray *barriers, struct set *ht)
{
   VkPipelineStageFlags pipeline = zink_pipeline_flags_from_stage(zink_shader_stage(stage));
   struct zink_descriptor_barrier key = {res, layout, flags, 0}, *t;

   uint32_t hash = barrier_hash(&key);
   struct set_entry *entry = _mesa_set_search_pre_hashed(ht, hash, &key);
   if (entry)
      t = (struct zink_descriptor_barrier*)entry->key;
   else {
      util_dynarray_append(barriers, struct zink_descriptor_barrier, key);
      t = util_dynarray_element(barriers, struct zink_descriptor_barrier,
                                util_dynarray_num_elements(barriers, struct zink_descriptor_barrier) - 1);
      t->stage = 0;
      t->layout = layout;
      t->res = res;
      t->access = flags;
      _mesa_set_add_pre_hashed(ht, hash, t);
   }
   t->stage |= pipeline;
}

static int
cmp_dynamic_offset_binding(const void *a, const void *b)
{
   const uint32_t *binding_a = a, *binding_b = b;
   return *binding_a - *binding_b;
}

static void
write_descriptors(struct zink_context *ctx, unsigned num_wds, VkWriteDescriptorSet *wds, bool cache_hit)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);

   if (!cache_hit && num_wds)
      vkUpdateDescriptorSets(screen->dev, num_wds, wds, 0, NULL);
}

static unsigned
init_write_descriptor(struct zink_shader *shader, struct zink_descriptor_set *zds, int idx, VkWriteDescriptorSet *wd, unsigned num_wds)
{
    wd->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wd->pNext = NULL;
    wd->dstBinding = shader->bindings[zds->pool->type][idx].binding;
    wd->dstArrayElement = 0;
    wd->descriptorCount = shader->bindings[zds->pool->type][idx].size;
    wd->descriptorType = shader->bindings[zds->pool->type][idx].type;
    wd->dstSet = zds->desc_set;
    return num_wds + 1;
}

static void
update_ubo_descriptors(struct zink_context *ctx, struct zink_descriptor_set *zds,
                       bool is_compute, bool cache_hit, bool need_resource_refs,
                       uint32_t *dynamic_offsets, unsigned *dynamic_offset_idx)
{
   struct zink_program *pg = is_compute ? (struct zink_program *)ctx->curr_compute : (struct zink_program *)ctx->curr_program;
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   unsigned num_descriptors = pg->pool[zds->pool->type]->num_descriptors;
   unsigned num_bindings = zds->pool->num_resources;
   VkWriteDescriptorSet wds[num_descriptors];
   VkDescriptorBufferInfo buffer_infos[num_bindings];
   unsigned num_wds = 0;
   unsigned num_buffer_info = 0;
   unsigned num_resources = 0;
   struct zink_shader **stages;
   struct {
      uint32_t binding;
      uint32_t offset;
   } dynamic_buffers[PIPE_MAX_CONSTANT_BUFFERS];
   unsigned dynamic_offset_count = 0;
   struct set *ht = NULL;
   if (!cache_hit) {
      ht = _mesa_set_create(NULL, barrier_hash, barrier_equals);
      _mesa_set_resize(ht, num_bindings);
   }

   unsigned num_stages = is_compute ? 1 : ZINK_SHADER_COUNT;
   if (is_compute)
      stages = &ctx->curr_compute->shader;
   else
      stages = &ctx->gfx_stages[0];

   for (int i = 0; i < num_stages; i++) {
      struct zink_shader *shader = stages[i];
      if (!shader)
         continue;
      enum pipe_shader_type stage = pipe_shader_type_from_mesa(shader->nir->info.stage);

      for (int j = 0; j < shader->num_bindings[zds->pool->type]; j++) {
         int index = shader->bindings[zds->pool->type][j].index;
         assert(shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
             shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
         assert(ctx->ubos[stage][index].buffer_size <= screen->info.props.limits.maxUniformBufferRange);
         struct zink_resource *res = zink_resource(ctx->ubos[stage][index].buffer);
         assert(num_resources < num_bindings);
         assert(!res || ctx->ubos[stage][index].buffer_size > 0);
         assert(!res || ctx->ubos[stage][index].buffer);
         desc_set_res_add(zds, res, num_resources++, cache_hit);
         assert(num_buffer_info < num_bindings);
         buffer_infos[num_buffer_info].buffer = res ? res->obj->buffer : VK_NULL_HANDLE;
         if (shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
            buffer_infos[num_buffer_info].offset = 0;
            /* we're storing this to qsort later */
            dynamic_buffers[dynamic_offset_count].binding = shader->bindings[zds->pool->type][j].binding;
            dynamic_buffers[dynamic_offset_count++].offset = ctx->ubos[stage][index].buffer_offset;
         } else
            buffer_infos[num_buffer_info].offset = res ? ctx->ubos[stage][index].buffer_offset : 0;
         buffer_infos[num_buffer_info].range  = res ? ctx->ubos[stage][index].buffer_size : VK_WHOLE_SIZE;
         if (!cache_hit && res)
            add_barrier(res, 0, VK_ACCESS_UNIFORM_READ_BIT, stage, &zds->barriers, ht);
         wds[num_wds].pBufferInfo = buffer_infos + num_buffer_info;
         ++num_buffer_info;

         num_wds = init_write_descriptor(shader, zds, j, &wds[num_wds], num_wds);
      }
   }
   _mesa_set_destroy(ht, NULL);
   /* Values are taken from pDynamicOffsets in an order such that all entries for set N come before set N+1;
    * within a set, entries are ordered by the binding numbers in the descriptor set layouts
    * - vkCmdBindDescriptorSets spec
    *
    * because of this, we have to sort all the dynamic offsets by their associated binding to ensure they
    * match what the driver expects
    */
   if (dynamic_offset_count > 1)
      qsort(dynamic_buffers, dynamic_offset_count, sizeof(uint32_t) * 2, cmp_dynamic_offset_binding);
   for (int i = 0; i < dynamic_offset_count; i++)
      dynamic_offsets[i] = dynamic_buffers[i].offset;
   *dynamic_offset_idx = dynamic_offset_count;

   write_descriptors(ctx, num_wds, wds, cache_hit);
}

static void
update_ssbo_descriptors(struct zink_context *ctx, struct zink_descriptor_set *zds,
                        bool is_compute, bool cache_hit, bool need_resource_refs)
{
   struct zink_program *pg = is_compute ? (struct zink_program *)ctx->curr_compute : (struct zink_program *)ctx->curr_program;
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   unsigned num_descriptors = pg->pool[zds->pool->type]->num_descriptors;
   unsigned num_bindings = zds->pool->num_resources;
   VkWriteDescriptorSet wds[num_descriptors];
   VkDescriptorBufferInfo buffer_infos[num_bindings];
   unsigned num_wds = 0;
   unsigned num_buffer_info = 0;
   unsigned num_resources = 0;
   struct zink_shader **stages;
   struct set *ht = NULL;
   if (!cache_hit) {
      ht = _mesa_set_create(NULL, barrier_hash, barrier_equals);
      _mesa_set_resize(ht, num_bindings);
   }

   unsigned num_stages = is_compute ? 1 : ZINK_SHADER_COUNT;
   if (is_compute)
      stages = &ctx->curr_compute->shader;
   else
      stages = &ctx->gfx_stages[0];

   for (int i = 0; (!cache_hit || need_resource_refs) && i < num_stages; i++) {
      struct zink_shader *shader = stages[i];
      if (!shader)
         continue;
      enum pipe_shader_type stage = pipe_shader_type_from_mesa(shader->nir->info.stage);

      for (int j = 0; j < shader->num_bindings[zds->pool->type]; j++) {
         int index = shader->bindings[zds->pool->type][j].index;
         assert(shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
         assert(num_resources < num_bindings);
         struct zink_resource *res = zink_resource(ctx->ssbos[stage][index].buffer);
         desc_set_res_add(zds, res, num_resources++, cache_hit);
         if (res) {
            assert(ctx->ssbos[stage][index].buffer_size > 0);
            assert(ctx->ssbos[stage][index].buffer_size <= screen->info.props.limits.maxStorageBufferRange);
            assert(num_buffer_info < num_bindings);
            unsigned flag = VK_ACCESS_SHADER_READ_BIT;
            if (ctx->writable_ssbos & (1 << index))
               flag |= VK_ACCESS_SHADER_WRITE_BIT;
            if (!cache_hit)
               add_barrier(res, 0, flag, stage, &zds->barriers, ht);
            buffer_infos[num_buffer_info].buffer = res->obj->buffer;
            buffer_infos[num_buffer_info].offset = ctx->ssbos[stage][index].buffer_offset;
            buffer_infos[num_buffer_info].range  = ctx->ssbos[stage][index].buffer_size;
         } else {
            assert(screen->info.rb2_feats.nullDescriptor);
            buffer_infos[num_buffer_info].buffer = VK_NULL_HANDLE;
            buffer_infos[num_buffer_info].offset = 0;
            buffer_infos[num_buffer_info].range  = VK_WHOLE_SIZE;
         }
         wds[num_wds].pBufferInfo = buffer_infos + num_buffer_info;
         ++num_buffer_info;

         num_wds = init_write_descriptor(shader, zds, j, &wds[num_wds], num_wds);
      }
   }
   _mesa_set_destroy(ht, NULL);
   write_descriptors(ctx, num_wds, wds, cache_hit);
}

static struct zink_sampler *
handle_image_descriptor(struct zink_screen *screen, struct zink_resource *res, enum zink_descriptor_type type, VkDescriptorType vktype, VkWriteDescriptorSet *wd,
                        VkImageLayout layout, unsigned *num_image_info, VkDescriptorImageInfo *image_info,
                        unsigned *num_buffer_info, VkBufferView *buffer_info,
                        struct zink_sampler_state *sampler,
                        VkImageView imageview, VkBufferView bufferview, bool do_set)
{
    struct zink_sampler *ret = NULL;
    if (!res) {
        /* if we're hitting this assert often, we can probably just throw a junk buffer in since
         * the results of this codepath are undefined in ARB_texture_buffer_object spec
         */
        assert(screen->info.rb2_feats.nullDescriptor);
        
        switch (vktype) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
           *buffer_info = VK_NULL_HANDLE;
           if (do_set)
              wd->pTexelBufferView = buffer_info;
           ++(*num_buffer_info);
           break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
           image_info->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
           image_info->imageView = VK_NULL_HANDLE;
           if (sampler)
              image_info->sampler = sampler->samplers[0]->sampler;
           if (do_set)
              wd->pImageInfo = image_info;
           ++(*num_image_info);
           break;
        default:
           unreachable("unknown descriptor type");
        }
     } else if (res->base.b.target != PIPE_BUFFER) {
        assert(layout != VK_IMAGE_LAYOUT_UNDEFINED);
        image_info->imageLayout = layout;
        image_info->imageView = imageview;
        if (sampler) {
           VkFormatProperties props = screen->format_props[res->base.b.format];
           bool can_linear = (res->optimal_tiling && props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ||
                             (!res->optimal_tiling && props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
           if (can_linear)
              ret = sampler->samplers[0];
           else
              ret = sampler->samplers[1] ? sampler->samplers[1] : sampler->samplers[0];
           image_info->sampler = ret->sampler;
        }
        if (do_set)
           wd->pImageInfo = image_info;
        ++(*num_image_info);
     } else {
        if (do_set)
           wd->pTexelBufferView = buffer_info;
        *buffer_info = bufferview;
        ++(*num_buffer_info);
     }
     return ret;
}

static void
update_sampler_descriptors(struct zink_context *ctx, struct zink_descriptor_set *zds,
                           bool is_compute, bool cache_hit, bool need_resource_refs)
{
   struct zink_program *pg = is_compute ? (struct zink_program *)ctx->curr_compute : (struct zink_program *)ctx->curr_program;
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   unsigned num_descriptors = pg->pool[zds->pool->type]->num_descriptors;
   unsigned num_bindings = zds->pool->num_resources;
   VkWriteDescriptorSet wds[num_descriptors];
   VkDescriptorImageInfo image_infos[num_bindings];
   VkBufferView buffer_views[num_bindings];
   unsigned num_wds = 0;
   unsigned num_image_info = 0;
   unsigned num_buffer_info = 0;
   unsigned num_resources = 0;
   struct zink_shader **stages;
   struct set *ht = NULL;
   if (!cache_hit) {
      ht = _mesa_set_create(NULL, barrier_hash, barrier_equals);
      _mesa_set_resize(ht, num_bindings);
   }

   unsigned num_stages = is_compute ? 1 : ZINK_SHADER_COUNT;
   if (is_compute)
      stages = &ctx->curr_compute->shader;
   else
      stages = &ctx->gfx_stages[0];

   for (int i = 0; (!cache_hit || need_resource_refs) && i < num_stages; i++) {
      struct zink_shader *shader = stages[i];
      if (!shader)
         continue;
      enum pipe_shader_type stage = pipe_shader_type_from_mesa(shader->nir->info.stage);

      for (int j = 0; j < shader->num_bindings[zds->pool->type]; j++) {
         int index = shader->bindings[zds->pool->type][j].index;
         assert(shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

         for (unsigned k = 0; k < shader->bindings[zds->pool->type][j].size; k++) {
            VkImageView imageview = VK_NULL_HANDLE;
            VkBufferView bufferview = VK_NULL_HANDLE;
            struct zink_resource *res = NULL;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            struct zink_sampler_state *sampler_state = NULL;
            struct zink_sampler *sampler;

            struct pipe_sampler_view *psampler_view = ctx->sampler_views[stage][index + k];
            struct zink_sampler_view *sampler_view = zink_sampler_view(psampler_view);
            res = psampler_view ? zink_resource(psampler_view->texture) : NULL;
            if (res && res->base.b.target == PIPE_BUFFER) {
               bufferview = sampler_view->buffer_view->buffer_view;
            } else if (res) {
               imageview = sampler_view->image_view->image_view;
               layout = (res->bind_history & BITFIELD64_BIT(ZINK_DESCRIPTOR_TYPE_IMAGE)) ?
                        VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
               sampler_state = ctx->sampler_states[stage][index + k];
            }
            assert(num_resources < num_bindings);
            if (res) {
               if (!cache_hit)
                  add_barrier(res, layout, VK_ACCESS_SHADER_READ_BIT, stage, &zds->barriers, ht);
            }
            assert(num_image_info < num_bindings);
            sampler = handle_image_descriptor(screen, res, zds->pool->type, shader->bindings[zds->pool->type][j].type,
                                    &wds[num_wds], layout, &num_image_info, &image_infos[num_image_info],
                                    &num_buffer_info, &buffer_views[num_buffer_info],
                                    sampler_state, imageview, bufferview, !k);
            desc_set_sampler_add(ctx, zds, sampler_view, sampler, num_resources++,
                                 shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                 cache_hit);
            struct zink_batch *batch = &ctx->batch;
            if (sampler_view)
               zink_batch_reference_sampler_view(batch, sampler_view);
            if (sampler)
              zink_batch_reference_sampler(batch, sampler);
         }
         assert(num_wds < num_descriptors);

         num_wds = init_write_descriptor(shader, zds, j, &wds[num_wds], num_wds);
      }
   }
   _mesa_set_destroy(ht, NULL);
   write_descriptors(ctx, num_wds, wds, cache_hit);
}

static void
update_image_descriptors(struct zink_context *ctx, struct zink_descriptor_set *zds,
                         bool is_compute, bool cache_hit, bool need_resource_refs)
{
   struct zink_program *pg = is_compute ? (struct zink_program *)ctx->curr_compute : (struct zink_program *)ctx->curr_program;
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   unsigned num_descriptors = pg->pool[zds->pool->type]->num_descriptors;
   unsigned num_bindings = zds->pool->num_resources;
   VkWriteDescriptorSet wds[num_descriptors];
   VkDescriptorImageInfo image_infos[num_bindings];
   VkBufferView buffer_views[num_bindings];
   unsigned num_wds = 0;
   unsigned num_image_info = 0;
   unsigned num_buffer_info = 0;
   unsigned num_resources = 0;
   struct zink_shader **stages;
   struct set *ht = NULL;
   if (!cache_hit) {
      ht = _mesa_set_create(NULL, barrier_hash, barrier_equals);
      _mesa_set_resize(ht, num_bindings);
   }

   unsigned num_stages = is_compute ? 1 : ZINK_SHADER_COUNT;
   if (is_compute)
      stages = &ctx->curr_compute->shader;
   else
      stages = &ctx->gfx_stages[0];

   for (int i = 0; (!cache_hit || need_resource_refs) && i < num_stages; i++) {
      struct zink_shader *shader = stages[i];
      if (!shader)
         continue;
      enum pipe_shader_type stage = pipe_shader_type_from_mesa(shader->nir->info.stage);

      for (int j = 0; j < shader->num_bindings[zds->pool->type]; j++) {
         int index = shader->bindings[zds->pool->type][j].index;
         assert(shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

         for (unsigned k = 0; k < shader->bindings[zds->pool->type][j].size; k++) {
            VkImageView imageview = VK_NULL_HANDLE;
            VkBufferView bufferview = VK_NULL_HANDLE;
            struct zink_resource *res = NULL;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            struct zink_image_view *image_view = &ctx->image_views[stage][index + k];
            assert(image_view);
            res = zink_resource(image_view->base.resource);

            if (res && image_view->base.resource->target == PIPE_BUFFER) {
               bufferview = image_view->buffer_view->buffer_view;
            } else if (res) {
               imageview = image_view->surface->image_view;
               layout = VK_IMAGE_LAYOUT_GENERAL;
            }
            assert(num_resources < num_bindings);
            desc_set_image_add(ctx, zds, image_view, num_resources++,
                               shader->bindings[zds->pool->type][j].type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                               cache_hit);
            if (res) {
               VkAccessFlags flags = 0;
               if (image_view->base.access & PIPE_IMAGE_ACCESS_READ)
                  flags |= VK_ACCESS_SHADER_READ_BIT;
               if (image_view->base.access & PIPE_IMAGE_ACCESS_WRITE)
                  flags |= VK_ACCESS_SHADER_WRITE_BIT;
               if (!cache_hit)
                  add_barrier(res, layout, flags, stage, &zds->barriers, ht);
            }

            assert(num_image_info < num_bindings);
            handle_image_descriptor(screen, res, zds->pool->type, shader->bindings[zds->pool->type][j].type,
                                    &wds[num_wds], layout, &num_image_info, &image_infos[num_image_info],
                                    &num_buffer_info, &buffer_views[num_buffer_info],
                                    NULL, imageview, bufferview, !k);

            struct zink_batch *batch = &ctx->batch;
            if (res)
               zink_batch_reference_image_view(batch, image_view);
         }
         assert(num_wds < num_descriptors);

         num_wds = init_write_descriptor(shader, zds, j, &wds[num_wds], num_wds);
      }
   }
   _mesa_set_destroy(ht, NULL);
   write_descriptors(ctx, num_wds, wds, cache_hit);
}

struct set *
zink_descriptors_update(struct zink_context *ctx, struct zink_screen *screen, bool is_compute)
{
   struct zink_program *pg = is_compute ? (struct zink_program *)ctx->curr_compute : (struct zink_program *)ctx->curr_program;

   zink_context_update_descriptor_states(ctx, is_compute);
   bool cache_hit[ZINK_DESCRIPTOR_TYPES];
   bool need_resource_refs[ZINK_DESCRIPTOR_TYPES];
   struct zink_descriptor_set *zds[ZINK_DESCRIPTOR_TYPES];
   for (int h = 0; h < ZINK_DESCRIPTOR_TYPES; h++) {
      if (pg->pool[h])
         zds[h] = zink_descriptor_set_get(ctx, h, is_compute, &cache_hit[h], &need_resource_refs[h]);
      else
         zds[h] = NULL;
   }
   struct zink_batch *batch = &ctx->batch;
   zink_batch_reference_program(batch, pg);

   struct set *persistent = NULL;
   if (ctx->num_persistent_maps)
      persistent = _mesa_pointer_set_create(NULL);

   uint32_t dynamic_offsets[PIPE_MAX_CONSTANT_BUFFERS];
   unsigned dynamic_offset_idx = 0;

   if (zds[ZINK_DESCRIPTOR_TYPE_UBO])
      update_ubo_descriptors(ctx, zds[ZINK_DESCRIPTOR_TYPE_UBO],
                                           is_compute, cache_hit[ZINK_DESCRIPTOR_TYPE_UBO],
                                           need_resource_refs[ZINK_DESCRIPTOR_TYPE_UBO], dynamic_offsets, &dynamic_offset_idx);
   if (zds[ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW])
      update_sampler_descriptors(ctx, zds[ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW],
                                               is_compute, cache_hit[ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW],
                                               need_resource_refs[ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW]);
   if (zds[ZINK_DESCRIPTOR_TYPE_SSBO])
      update_ssbo_descriptors(ctx, zds[ZINK_DESCRIPTOR_TYPE_SSBO],
                                               is_compute, cache_hit[ZINK_DESCRIPTOR_TYPE_SSBO],
                                               need_resource_refs[ZINK_DESCRIPTOR_TYPE_SSBO]);
   if (zds[ZINK_DESCRIPTOR_TYPE_IMAGE])
      update_image_descriptors(ctx, zds[ZINK_DESCRIPTOR_TYPE_IMAGE],
                                               is_compute, cache_hit[ZINK_DESCRIPTOR_TYPE_IMAGE],
                                               need_resource_refs[ZINK_DESCRIPTOR_TYPE_IMAGE]);

   for (int h = 0; zds[h] && h < ZINK_DESCRIPTOR_TYPES; h++) {
      /* skip null descriptor sets since they have no resources */
      if (!zds[h]->hash)
         continue;
      assert(zds[h]->desc_set);
      util_dynarray_foreach(&zds[h]->barriers, struct zink_descriptor_barrier, barrier) {
         if (barrier->res->persistent_maps)
            _mesa_set_add(persistent, barrier->res);
         if (need_resource_refs[h])
            zink_batch_reference_resource_rw(batch, barrier->res, zink_resource_access_is_write(barrier->access));
         zink_resource_barrier(ctx, NULL, barrier->res,
                               barrier->layout, barrier->access, barrier->stage);
      }
   }

   if (!is_compute)
      batch = zink_batch_rp(ctx);

   for (unsigned h = 0; h < ZINK_DESCRIPTOR_TYPES; h++) {
      if (zds[h]) {
         vkCmdBindDescriptorSets(batch->state->cmdbuf, is_compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pg->layout, zds[h]->pool->type, 1, &zds[h]->desc_set,
                                 zds[h]->pool->type == ZINK_DESCRIPTOR_TYPE_UBO ? dynamic_offset_idx : 0, dynamic_offsets);
      }
   }
   return persistent;
}
