/*
 * Copyright © 2018 Intel Corporation
 * Copyright (c) 2021 Lima Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "compiler/nir/nir.h"
#include "util/blob.h"
#include "util/build_id.h"
#include "util/disk_cache.h"
#include "util/mesa-sha1.h"

#include "lima_context.h"
#include "lima_screen.h"
#include "lima_disk_cache.h"

static void
lima_vs_disk_cache_compute_key(struct disk_cache *cache,
                               const struct lima_vs_key *key,
                               cache_key cache_key)
{
   uint8_t data[sizeof(key->uncomp_shader->nir_sha1)];

   memcpy(data, key->uncomp_shader->nir_sha1, sizeof(key->uncomp_shader->nir_sha1));

   disk_cache_compute_key(cache, data, sizeof(data), cache_key);
}

static void
lima_fs_disk_cache_compute_key(struct disk_cache *cache,
                               const struct lima_fs_key *key,
                               cache_key cache_key)
{
   uint8_t data[sizeof(key->tex) + sizeof(key->uncomp_shader->nir_sha1)];

   memcpy(data, key->uncomp_shader->nir_sha1, sizeof(key->uncomp_shader->nir_sha1));
   memcpy(data + sizeof(key->uncomp_shader->nir_sha1), &key->tex, sizeof(key->tex));

   disk_cache_compute_key(cache, data, sizeof(data), cache_key);
}

void
lima_vs_disk_cache_store(struct disk_cache *cache,
                         const struct lima_vs_key *key,
                         const struct lima_vs_compiled_shader *shader)
{
   if (!cache)
      return;

   cache_key cache_key;
   lima_vs_disk_cache_compute_key(cache, key, cache_key);

   if (lima_debug & LIMA_DEBUG_DISK_CACHE) {
      char sha1[41];
      _mesa_sha1_format(sha1, cache_key);
      fprintf(stderr, "[mesa disk cache] storing %s\n", sha1);
   }

   struct blob blob;
   blob_init(&blob);

#define BLOB_WRITE(what) blob_write_bytes(&blob, &what, sizeof(what))
   BLOB_WRITE(shader->shader_size);
   blob_write_bytes(&blob, shader->shader, shader->shader_size);
   BLOB_WRITE(shader->prefetch);
   BLOB_WRITE(shader->uniform_size);
   BLOB_WRITE(shader->constant_size);
   blob_write_bytes(&blob, shader->constant, shader->constant_size);
   BLOB_WRITE(shader->varying);
   BLOB_WRITE(shader->varying_stride);
   BLOB_WRITE(shader->num_outputs);
   BLOB_WRITE(shader->num_varyings);
   BLOB_WRITE(shader->gl_pos_idx);
   BLOB_WRITE(shader->point_size_idx);
#undef BLOB_WRITE

   disk_cache_put(cache, cache_key, blob.data, blob.size, NULL);
   blob_finish(&blob);
}

void
lima_fs_disk_cache_store(struct disk_cache *cache,
                         const struct lima_fs_key *key,
                         const struct lima_fs_compiled_shader *shader)
{
   if (!cache)
      return;

   cache_key cache_key;
   lima_fs_disk_cache_compute_key(cache, key, cache_key);

   if (lima_debug & LIMA_DEBUG_DISK_CACHE) {
      char sha1[41];
      _mesa_sha1_format(sha1, cache_key);
      fprintf(stderr, "[mesa disk cache] storing %s\n", sha1);
   }

   struct blob blob;
   blob_init(&blob);

#define BLOB_WRITE(what) blob_write_bytes(&blob, &what, sizeof(what))
   BLOB_WRITE(shader->shader_size);
   blob_write_bytes(&blob, shader->shader, shader->shader_size);
   BLOB_WRITE(shader->stack_size);
   BLOB_WRITE(shader->uses_discard);
#undef BLOB_WRITE

   disk_cache_put(cache, cache_key, blob.data, blob.size, NULL);
   blob_finish(&blob);
}

struct lima_vs_compiled_shader *
lima_vs_disk_cache_retrieve(struct disk_cache *cache,
                            struct lima_vs_key *key)
{
   struct lima_vs_compiled_shader *shader = NULL;

   if (!cache)
      return NULL;

   cache_key cache_key;
   lima_vs_disk_cache_compute_key(cache, key, cache_key);

   if (lima_debug & LIMA_DEBUG_DISK_CACHE) {
      char sha1[41];
      _mesa_sha1_format(sha1, cache_key);
      fprintf(stderr, "[mesa disk cache] retrieving %s: ", sha1);
   }

   size_t size;
   void *buffer = disk_cache_get(cache, cache_key, &size);

   if (lima_debug & LIMA_DEBUG_DISK_CACHE)
      fprintf(stderr, "%s\n", buffer ? "found" : "missing");

   if (!buffer)
      return NULL;

   shader = rzalloc(NULL, struct lima_vs_compiled_shader);
   if (!shader)
      goto out;

   struct blob_reader blob;
#define BLOB_COPY(what) blob_copy_bytes(&blob, &what, sizeof(what))
   blob_reader_init(&blob, buffer, size);
   BLOB_COPY(shader->shader_size);
   shader->shader = rzalloc_size(shader, shader->shader_size);
   if (!shader->shader)
      goto err;
   blob_copy_bytes(&blob, shader->shader, shader->shader_size);
   BLOB_COPY(shader->prefetch);
   BLOB_COPY(shader->uniform_size);
   BLOB_COPY(shader->constant_size);
   shader->constant = rzalloc_size(shader, shader->constant_size);
   if (!shader->constant)
      goto err;
   blob_copy_bytes(&blob, shader->constant, shader->constant_size);
   BLOB_COPY(shader->varying);
   BLOB_COPY(shader->varying_stride);
   BLOB_COPY(shader->num_outputs);
   BLOB_COPY(shader->num_varyings);
   BLOB_COPY(shader->gl_pos_idx);
   BLOB_COPY(shader->point_size_idx);
#undef BLOB_COPY

out:
   free(buffer);
   return shader;

err:
   ralloc_free(shader);
   return NULL;
}

struct lima_fs_compiled_shader *
lima_fs_disk_cache_retrieve(struct disk_cache *cache,
                            struct lima_fs_key *key)
{
   struct lima_fs_compiled_shader *shader = NULL;

   if (!cache)
      return NULL;

   cache_key cache_key;
   lima_fs_disk_cache_compute_key(cache, key, cache_key);

   if (lima_debug & LIMA_DEBUG_DISK_CACHE) {
      char sha1[41];
      _mesa_sha1_format(sha1, cache_key);
      fprintf(stderr, "[mesa disk cache] retrieving %s: ", sha1);
   }

   size_t size;
   void *buffer = disk_cache_get(cache, cache_key, &size);

   if (lima_debug & LIMA_DEBUG_DISK_CACHE)
      fprintf(stderr, "%s\n", buffer ? "found" : "missing");

   if (!buffer)
      return NULL;

   shader = rzalloc(NULL, struct lima_fs_compiled_shader);
   if (!shader)
      goto out;

   struct blob_reader blob;
#define BLOB_COPY(what) blob_copy_bytes(&blob, &what, sizeof(what))
   blob_reader_init(&blob, buffer, size);
   BLOB_COPY(shader->shader_size);
   shader->shader = rzalloc_size(shader, shader->shader_size);
   if (!shader)
      goto err;
   blob_copy_bytes(&blob, shader->shader, shader->shader_size);
   BLOB_COPY(shader->stack_size);
   BLOB_COPY(shader->uses_discard);
#undef BLOB_COPY

out:
   free(buffer);
   return shader;

err:
   ralloc_free(shader);
   return NULL;
}

void
lima_disk_cache_init(struct lima_screen *screen)
{
   if (lima_debug & LIMA_DEBUG_NO_DISK_CACHE)
      return;

   const struct build_id_note *note =
      build_id_find_nhdr_for_addr(lima_disk_cache_init);
   assert(note && build_id_length(note) == 20); /* sha1 */

   const uint8_t *id_sha1 = build_id_data(note);
   assert(id_sha1);

   char timestamp[41];
   _mesa_sha1_format(timestamp, id_sha1);

   screen->disk_cache = disk_cache_create(screen->base.get_name(&screen->base), timestamp, 0);
}
