/* This file was derived from drisw_priv.h which carries the following
 * copyright:
 *
 * Copyright 2008 George Sapountzis
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef DRIVK_PRIV_H
#define DRIVK_PRIV_H

// #include <X11/extensions/XShm.h>
#include <vulkan/vulkan.h>

struct drivk_display
{
   __GLXDRIdisplay base;

   void *driver;
   const __DRIcoreExtension *core;
   const __DRIvkrastExtension *vkrast;
   const __DRIextension **extensions;
   VkInstance instance;

   PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
   PFN_vkCreateInstance vkCreateInstance;
   PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
   PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
};

struct drivk_context
{
   struct glx_context base;
   __DRIcontext *driContext;

};

struct drivk_screen
{
   struct glx_screen base;

   __DRIscreen *driScreen;
   __GLXDRIscreen vtable;
   const __DRIcoreExtension *core;
   const __DRIvkrastExtension *vkrast;
   VkInstance instance;
};

struct drivk_drawable
{
   __GLXDRIdrawable base;
   __DRIdrawable *driDrawable;
   struct glx_config *config;
};

#if 0
_X_HIDDEN int
drisw_query_renderer_integer(struct glx_screen *base, int attribute,
                             unsigned int *value);
_X_HIDDEN int
drisw_query_renderer_string(struct glx_screen *base, int attribute,
                            const char **value);

#endif

#endif
