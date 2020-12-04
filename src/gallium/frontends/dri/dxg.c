/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "util/u_memory.h"
#include "pipe-loader/pipe_loader.h"

#include "dri_util.h"
#include "dri_screen.h"
#include "dri_context.h"
#include "dri_helpers.h"
#include "dri_drawable.h"

#include "d3d12/d3d12_public.h"

static void
dxg_drawable_allocate_textures(struct dri_context *ctx,
                               struct dri_drawable *drawable,
                               const enum st_attachment_type *statts,
                               unsigned statts_count)
{
}

static boolean
dxg_create_buffer(__DRIscreen * sPriv,
                  __DRIdrawable * dPriv,
                  const struct gl_config * visual, boolean isPixmap)
{
   if (!dri_create_buffer(sPriv, dPriv, visual, isPixmap))
      return FALSE;

   struct dri_drawable *drawable = dPriv->driverPrivate;
   drawable->allocate_textures = dxg_drawable_allocate_textures;
   
   return true;
}

static const __DRIconfig **
dxg_init_screen(__DRIscreen *sPriv, void *dxcore_adapter)
{
   struct dri_screen *screen = CALLOC_STRUCT(dri_screen);
   if (!screen)
      return NULL;

   screen->sPriv = sPriv;
   screen->fd = -1;
   sPriv->driverPrivate = screen;

   struct pipe_screen *pscreen = NULL;
   const __DRIconfig **configs;
   if (pipe_loader_dxg_probe_one(&screen->dev, dxcore_adapter)) {
      dri_init_options(screen);
      pscreen = pipe_loader_create_screen(screen->dev);
   }

   if (!pscreen)
      goto fail;

   configs = dri_init_screen_helper(screen, pscreen);
   if (!configs)
      goto fail;

   return configs;

fail:
   dri_destroy_screen_helper(screen);
   if (screen->dev)
      pipe_loader_release(&screen->dev, 1);
   FREE(screen);
   return NULL;
}

static __DRIscreen *
dxg_create_new_screen(void *dxcore_adapter,
                      const __DRIextension **loader_extensions,
                      const __DRIextension **driver_extensions,
                      const __DRIconfig ***driver_configs,
                      void *loader_private)
{
   __DRIscreen *psp = driCreateNewScreenPrologue(-1, -1, loader_extensions, driver_extensions, loader_private);

   *driver_configs = dxg_init_screen(psp, dxcore_adapter);
   if (*driver_configs == NULL) {
      free(psp);
      return NULL;
   }

   driCreateNewScreenEpilogue(psp);

   return psp;
}

const __DRIdxgExtension driDxgExtension = {
   .base = { __DRI_DXG, 1 },

   .createDXCoreFactory = d3d12_create_dxcore_factory,
   .createD3DScreen = dxg_create_new_screen,
   .createNewContext = driCreateContextAttribs,
   .createNewDrawable = driCreateNewDrawable,
};

const struct __DriverAPIRec gallium_dxg_driver_api = {
   .InitScreen = NULL,
   .DestroyScreen = dri_destroy_screen,
   .CreateContext = dri_create_context,
   .DestroyContext = dri_destroy_context,
   .MakeCurrent = dri_make_current,
   .UnbindContext = dri_unbind_context,
   .CreateBuffer = dxg_create_buffer,
   .DestroyBuffer = dri_destroy_buffer,
};

const __DRIextension *gallium_dxg_driver_extensions[] = {
   &driCoreExtension.base,
   &driDxgExtension.base,
   &gallium_config_options.base,
   NULL
};
