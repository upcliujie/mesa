/*
 * Copyright 2021 Red Hat, Inc.
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

/**
 * @file
 * Vulkan WSI as software rasterizer winsys.
 */

#include <stdio.h>

#include "pipe/p_format.h"
#include "util/u_memory.h"
#include "frontend/sw_winsys.h"
#include "vkwsi_winsys.h"


static bool
vkwsi_is_displaytarget_format_supported(struct sw_winsys *ws,
                                        unsigned tex_usage,
                                        enum pipe_format format)
{
   return false;
}


static void *
vkwsi_displaytarget_map(struct sw_winsys *ws,
                        struct sw_displaytarget *dt,
                        unsigned flags)
{
   assert(0);
   return NULL;
}


static void
vkwsi_displaytarget_unmap(struct sw_winsys *ws,
                          struct sw_displaytarget *dt)
{
   assert(0);
}


static void
vkwsi_displaytarget_destroy(struct sw_winsys *winsys,
                            struct sw_displaytarget *dt)
{
   assert(0);
}


static struct sw_displaytarget *
vkwsi_displaytarget_create(struct sw_winsys *winsys,
                           unsigned tex_usage,
                           enum pipe_format format,
                           unsigned width, unsigned height,
                           unsigned alignment,
                           const void *front_private,
                           unsigned *stride)
{
   fprintf(stderr, "vkwsi_displaytarget_create() returning NULL\n");
   return NULL;
}


static struct sw_displaytarget *
vkwsi_displaytarget_from_handle(struct sw_winsys *winsys,
                                const struct pipe_resource *templat,
                                struct winsys_handle *whandle,
                                unsigned *stride)
{
   return NULL;
}


static bool
vkwsi_displaytarget_get_handle(struct sw_winsys *winsys,
                               struct sw_displaytarget *dt,
                               struct winsys_handle *whandle)
{
   assert(0);
   return false;
}


static void
vkwsi_displaytarget_display(struct sw_winsys *winsys,
                            struct sw_displaytarget *dt,
                            void *context_private,
                            struct pipe_box *box)
{
   assert(0);
}


static void
vkwsi_destroy(struct sw_winsys *winsys)
{
   FREE(winsys);
}


struct sw_winsys *
vkwsi_create_winsys(void)
{
   static struct sw_winsys *winsys;

   winsys = CALLOC_STRUCT(sw_winsys);
   if (!winsys)
      return NULL;

   winsys->destroy = vkwsi_destroy;
   winsys->is_displaytarget_format_supported = vkwsi_is_displaytarget_format_supported;
   winsys->displaytarget_create = vkwsi_displaytarget_create;
   winsys->displaytarget_from_handle = vkwsi_displaytarget_from_handle;
   winsys->displaytarget_get_handle = vkwsi_displaytarget_get_handle;
   winsys->displaytarget_map = vkwsi_displaytarget_map;
   winsys->displaytarget_unmap = vkwsi_displaytarget_unmap;
   winsys->displaytarget_display = vkwsi_displaytarget_display;
   winsys->displaytarget_destroy = vkwsi_displaytarget_destroy;

   return winsys;
}
