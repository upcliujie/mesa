/*
 * Copyright © 2017 Intel Corporation
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

#include <unistd.h>
#include <fcntl.h>

#include "c11/threads.h"
#include "util/os_file.h"
#include "util/u_hash_table.h"
#include "util/u_pointer.h"

#include "iris_drm_public.h"
#include "iris/iris_screen.h"

extern struct pipe_screen *iris_screen_create(int fd, const struct pipe_screen_config *config);

static struct hash_table *iris_screen_ht = NULL;
static mtx_t iris_screen_mutex = _MTX_INITIALIZER_NP;

static void
iris_drm_screen_destroy(struct pipe_screen *pscreen)
{
   struct iris_screen *screen = (void *) pscreen;

   mtx_lock(&iris_screen_mutex);

   bool destroy = --screen->refcount == 0;

   if (destroy) {
      int fd = screen->fd;
      _mesa_hash_table_remove_key(iris_screen_ht, intptr_to_pointer(fd));

      if (!iris_screen_ht->entries) {
         _mesa_hash_table_destroy(iris_screen_ht, NULL);
         iris_screen_ht = NULL;
      }
   }

   mtx_unlock(&iris_screen_mutex);

   if (destroy) {
      pscreen->destroy = screen->loader_priv;
      pscreen->destroy(pscreen);
   }
}

struct pipe_screen *
iris_drm_screen_create(int fd, const struct pipe_screen_config *config)
{
   struct pipe_screen *pscreen = NULL;

   mtx_lock(&iris_screen_mutex);

   if (!iris_screen_ht) {
      iris_screen_ht = util_hash_table_create_fd_keys();
      if (!iris_screen_ht)
         goto unlock;
   }

   pscreen = util_hash_table_get(iris_screen_ht, intptr_to_pointer(fd));
   if (pscreen) {
      ((struct iris_screen *) pscreen)->refcount++;
   } else {
      int newfd = os_dupfd_cloexec(fd);
      if (newfd < 0)
         goto unlock;

      pscreen = iris_screen_create(newfd, config);
      if (!pscreen) {
         close(newfd);
         goto unlock;
      }

      _mesa_hash_table_insert(iris_screen_ht, intptr_to_pointer(newfd),
                              pscreen);

      /* We override the pipe driver's screen->destroy() to point at our
       * reference counted one.  This is a bit of a hack, but it avoids
       * the pipe driver having to call back into our loader code.
       */
      ((struct iris_screen *) pscreen)->loader_priv = pscreen->destroy;
      pscreen->destroy = iris_drm_screen_destroy;
   }

unlock:
   mtx_unlock(&iris_screen_mutex);
   return pscreen;
}
