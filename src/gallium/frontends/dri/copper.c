/*
 * Copyright 2020 Red Hat, Inc.
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

#include "util/format/u_format.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_box.h"
#include "pipe/p_context.h"
#include "pipe-loader/pipe_loader.h"
#include "state_tracker/st_context.h"
#include "os/os_process.h"
#include "zink/zink_public.h"
#include "zink/zink_instance.h"

#include "dri_screen.h"
#include "dri_context.h"
#include "dri_drawable.h"
#include "dri_helpers.h"
#include "dri_query_renderer.h"

#include <vulkan/vulkan.h>

static __DRIimageExtension driVkImageExtension = {
    .base = { __DRI_IMAGE, 6 },

    .createImageFromRenderbuffer  = dri2_create_image_from_renderbuffer,
    .createImageFromTexture = dri2_create_from_texture,
    .destroyImage = dri2_destroy_image,
};

static const __DRIextension *drivk_screen_extensions[] = {
   &driTexBufferExtension.base,
   &dri2RendererQueryExtension.base,
   &dri2ConfigQueryExtension.base,
   &dri2FenceExtension.base,
   &dri2NoErrorExtension.base,
   &driVkImageExtension.base,
   &dri2FlushControlExtension.base,
   NULL
};

static const __DRIconfig **
copper_init_screen(__DRIscreen * sPriv)
{
   // const __DRIcopperLoaderExtension *loader = sPriv->copper.loader;
   const __DRIconfig **configs;
   struct dri_screen *screen;
   struct pipe_screen *pscreen = NULL;

   screen = CALLOC_STRUCT(dri_screen);
   if (!screen)
      return NULL;

   screen->sPriv = sPriv;
   screen->fd = -1; // XXX maybe use this to OOB that it's vulkan

   // screen->swrast_no_present = debug_get_option_swrast_no_present();

   sPriv->driverPrivate = (void *)screen;

   if (pipe_loader_sw_probe_vk(&screen->dev)) {
      dri_init_options(screen);

      pscreen = pipe_loader_create_screen(screen->dev);
   }

   if (!pscreen)
      goto fail;

   configs = dri_init_screen_helper(screen, pscreen);
   if (!configs)
      goto fail;

   sPriv->extensions = drivk_screen_extensions;

   return configs;
fail:
   dri_destroy_screen_helper(screen);
   if (screen->dev)
      pipe_loader_release(&screen->dev, 1);
   FREE(screen);
   return NULL;
}

// copypasta alert

static inline void
drisw_present_texture(struct pipe_context *pipe, __DRIdrawable *dPriv,
                      struct pipe_resource *ptex, struct pipe_box *sub_box)
{
   struct dri_drawable *drawable = dri_drawable(dPriv);
   struct dri_screen *screen = dri_screen(drawable->sPriv);

   if (screen->swrast_no_present)
      return;

   screen->base.screen->flush_frontbuffer(screen->base.screen, pipe, ptex, 0, 0, drawable, sub_box);
}

static inline void
drisw_invalidate_drawable(__DRIdrawable *dPriv)
{
   struct dri_drawable *drawable = dri_drawable(dPriv);

   drawable->texture_stamp = dPriv->lastStamp - 1;

   p_atomic_inc(&drawable->base.stamp);
}

static inline void
drisw_copy_to_front(struct pipe_context *pipe,
                    __DRIdrawable * dPriv,
                    struct pipe_resource *ptex)
{
   drisw_present_texture(pipe, dPriv, ptex, NULL);

   drisw_invalidate_drawable(dPriv);
}

/*
 * Backend functions for st_framebuffer interface and swap_buffers.
 */

static void
copper_swap_buffers(__DRIdrawable *draw)
{
   struct dri_context *ctx = dri_get_current(draw->driScreenPriv);
   struct dri_drawable *drawable = dri_drawable(draw);
   struct pipe_resource *ptex;
    
   if (!ctx)
      return;
    
   ptex = drawable->textures[ST_ATTACHMENT_BACK_LEFT];
    
   if (ptex) {
      if (ctx->pp)
         pp_run(ctx->pp, ptex, ptex, drawable->textures[ST_ATTACHMENT_DEPTH_STENCIL]);
    
      if (ctx->hud)
         hud_run(ctx->hud, ctx->st->cso_context, ptex);
    
      ctx->st->flush(ctx->st, ST_FLUSH_FRONT, NULL, NULL, NULL);
    
      if (drawable->stvis.samples > 1) {
         /* Resolve the back buffer. */
         dri_pipe_blit(ctx->st->pipe,
                       drawable->textures[ST_ATTACHMENT_BACK_LEFT],
                       drawable->msaa_textures[ST_ATTACHMENT_BACK_LEFT]);
      }

      drisw_copy_to_front(ctx->st->pipe, draw, ptex);
   }
}

static void
copper_allocate_textures(struct dri_context *ctx,
                         struct dri_drawable *drawable,
                         const enum st_attachment_type *statts,
                         unsigned statts_count)
{
   /* insert swapchain allocation here */
}

static inline void
get_drawable_info(__DRIdrawable *dPriv, int *w, int *h)
{
   __DRIscreen *sPriv = dPriv->driScreenPriv;
   const __DRIcopperLoaderExtension *loader = sPriv->copper.loader;

   loader->GetDrawableInfo(dPriv, w, h,
                           dPriv->loaderPrivate);
}

static void
copper_update_drawable_info(struct dri_drawable *drawable)
{
   __DRIdrawable *dPriv = drawable->dPriv;

   get_drawable_info(dPriv, &dPriv->w, &dPriv->h);
}

static void
copper_flush_frontbuffer(struct dri_context *ctx,
                         struct dri_drawable *drawable,
                         enum st_attachment_type statt)
{
   *(int *)0 = 0;
}

static void
copper_update_tex_buffer(struct dri_drawable *drawable,
                         struct dri_context *ctx,
                         struct pipe_resource *res)
{
   *(int *)0 = 0;
}

static void
copper_flush_swapbuffers(struct dri_context *ctx,
                         struct dri_drawable *drawable)
{
   *(int *)0 = 0;
}

static boolean
copper_create_buffer(__DRIscreen * sPriv,
                     __DRIdrawable * dPriv,
                     const struct gl_config *visual, boolean isPixmap)
{
   struct dri_drawable *drawable = NULL;

   if (!dri_create_buffer(sPriv, dPriv, visual, isPixmap))
      return FALSE;

   drawable = dPriv->driverPrivate;

   drawable->allocate_textures = copper_allocate_textures;
   drawable->update_drawable_info = copper_update_drawable_info;
   drawable->flush_frontbuffer = copper_flush_frontbuffer;
   drawable->update_tex_buffer = copper_update_tex_buffer;
   drawable->flush_swapbuffers = copper_flush_swapbuffers;

   return TRUE;
}

/* XXX this is a stub, basically this is to check whether vulkan
 * might work at all, and you really only need it to keep copper
 * gracefully off the vtables if it won't work. so just return
 * true for now...
 */
static void *
copperCreateInstance(uint32_t num, const char * const * extensions)
{
   return (void *)1;
}

static void *
copperGetInstanceProcAddr(VkInstance instance, const char *proc)
{
   return (void *) vkGetInstanceProcAddr(instance, proc);
}

static struct zink_screen *
to_zink_screen(struct pipe_screen *screen)
{
   return (struct zink_screen *)screen;
}

static VkInstance
copperGetInstance(__DRIscreen *screen)
{
   struct zink_screen *z = to_zink_screen(dri_screen(screen)->base.screen);
   return zink_get_screen_instance(z);
}

static VkSwapchainKHR
copperCreateSwapchain(__DRIscreen *screen,
                      const VkSwapchainCreateInfoKHR *ci)
{
   struct zink_screen *z = to_zink_screen(dri_screen(screen)->base.screen);
   return zink_create_swapchain(z, ci);
}

const __DRIcopperExtension driCopperExtension = {
   .base = { __DRI_COPPER, 1 },

   .CreateInstance            = copperCreateInstance,
   .GetInstanceProcAddr       = copperGetInstanceProcAddr,
   .GetInstance               = copperGetInstance,
   .CreateSwapchain           = copperCreateSwapchain,
};

const struct __DriverAPIRec galliumvk_driver_api = {
   .InitScreen = copper_init_screen,
   .DestroyScreen = dri_destroy_screen,
   .CreateContext = dri_create_context,
   .DestroyContext = dri_destroy_context,
   .CreateBuffer = copper_create_buffer,
   .DestroyBuffer = dri_destroy_buffer,
   .SwapBuffers = copper_swap_buffers,
   .MakeCurrent = dri_make_current,
   .UnbindContext = dri_unbind_context,
   .CopySubBuffer = NULL, //copper_copy_sub_buffer,
};

const __DRIextension *galliumvk_driver_extensions[] = {
   &driCoreExtension.base,
   &driImageDriverExtension.base,
   &driCopperExtension.base,
   &gallium_config_options.base,
   NULL
};

/* vim: set sw=3 ts=8 sts=3 expandtab: */
