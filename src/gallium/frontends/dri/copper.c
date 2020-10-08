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

#include "dri_screen.h"
#include "dri_context.h"
#include "dri_drawable.h"
#include "dri_helpers.h"
#include "dri_query_renderer.h"

#include <vulkan/vulkan.h>

static const __DRIconfig **
copper_init_screen(__DRIscreen * sPriv)
{
   const __DRIcopperLoaderExtension *loader = sPriv->copper.loader;
   const __DRIconfig **configs;
   struct dri_screen *screen;
   struct pipe_screen *pscreen = NULL;
   VkInstance instance;
   // const struct drivk_loader_funcs *lf = &drisw_lf;

   screen = CALLOC_STRUCT(dri_screen);
   if (!screen)
      return NULL;

   screen->sPriv = sPriv;
   screen->fd = -1;
   instance = loader->GetInstance(sPriv->loaderPrivate);

   // screen->swrast_no_present = debug_get_option_swrast_no_present();

   sPriv->driverPrivate = (void *)screen;

   if (pipe_loader_vk_probe_one(&screen->dev, instance,
                                /* (VkPhysicalDevice) */ sPriv->dev)) {
      dri_init_options(screen);

      pscreen = pipe_loader_create_screen(screen->dev);
   }

   if (!pscreen)
      goto fail;

   configs = dri_init_screen_helper(screen, pscreen);
   if (!configs)
      goto fail;

   // sPriv->extensions = drivk_screen_extensions;

   return configs;
fail:
   dri_destroy_screen_helper(screen);
   if (screen->dev)
      pipe_loader_release(&screen->dev, 1);
   FREE(screen);
   return NULL;
}

static void
copper_allocate_textures(struct dri_context *ctx,
                         struct dri_drawable *drawable,
                         const enum st_attachment_type *statts,
                         unsigned statts_count)
{
   *(int *)0 = 0;
}

static void
copper_update_drawable_info(struct dri_drawable *drawable)
{
   *(int *)0 = 0;
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

static void *
copperCreateInstance(uint32_t num, const char * const * extensions)
{
   VkApplicationInfo ai = {};
   ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   char proc_name[128];
   const char *exts[16]; /* XXX */

   if (os_get_process_name(proc_name, ARRAY_SIZE(proc_name)))
      ai.pApplicationName = proc_name;
   else
      ai.pApplicationName = "unknown";

   ai.pEngineName = "mesa zink";
   ai.apiVersion = VK_API_VERSION_1_0;

   memcpy(exts, extensions, num * sizeof (char *));
   exts[num++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
   exts[num++] = VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME;

   VkInstanceCreateInfo ici = {};
   ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   ici.pApplicationInfo = &ai;
   ici.ppEnabledExtensionNames = exts;
   ici.enabledExtensionCount = num;

   VkInstance instance = VK_NULL_HANDLE;
   VkResult err = vkCreateInstance(&ici, NULL, &instance);
   if (err != VK_SUCCESS)
      return VK_NULL_HANDLE;

   return instance;
}

static void *
copperGetInstanceProcAddr(void *instance, const char *proc)
{
   return (void *) vkGetInstanceProcAddr(instance, proc);
}

static void
copperEnumeratePhysicalDevices(void *instance, unsigned int *count,
                               void ***devs)
{
   uint32_t pdev_count;
   VkPhysicalDevice *pdevs;

   *count = 0;

   vkEnumeratePhysicalDevices(instance, &pdev_count, NULL);
   if (pdev_count == 0)
      return;

   pdevs = malloc(sizeof(*pdevs) * pdev_count);
   vkEnumeratePhysicalDevices(instance, &pdev_count, pdevs);

   *devs = (void **)pdevs;
   *count = pdev_count;
}

const __DRIcopperExtension driCopperExtension = {
   .base = { __DRI_COPPER, 1 },

   .CreateInstance            = copperCreateInstance,
   .GetInstanceProcAddr       = copperGetInstanceProcAddr,
   .EnumeratePhysicalDevices  = copperEnumeratePhysicalDevices,

   .createVkScreen            = driCopperCreateNewScreen,
   .createContextAttribs      = driCreateContextAttribs,
   .createNewDrawable         = driCreateNewDrawable,
};

const struct __DriverAPIRec galliumvk_driver_api = {
   .InitScreen = copper_init_screen,
   .DestroyScreen = dri_destroy_screen,
   .CreateContext = dri_create_context,
   .DestroyContext = dri_destroy_context,
   .CreateBuffer = copper_create_buffer,
   .DestroyBuffer = dri_destroy_buffer,
   .SwapBuffers = NULL, //copper_swap_buffers,
   .MakeCurrent = dri_make_current,
   .UnbindContext = dri_unbind_context,
   .CopySubBuffer = NULL, //copper_copy_sub_buffer,
};

const __DRIextension *galliumvk_driver_extensions[] = {
   &driCoreExtension.base,
   &driCopperExtension.base,
   &gallium_config_options.base,
   NULL
};

/* vim: set sw=3 ts=8 sts=3 expandtab: */
