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

/*
 * In principle this could all go in dri_interface.h, but:
 * - I want type safety in here, but I don't want to require vulkan.h from
 *   dri_interface.h
 * - I don't especially want this to be an interface outside of Mesa itself
 * - Ideally dri_interface.h wouldn't even be a thing anymore
 *
 * So instead let's just keep this as a Mesa-internal detail.
 */

#ifndef COPPER_INTERFACE_H
#define COPPER_INTERFACE_H

#include <GL/internal/dri_interface.h>
#include <vulkan/vulkan.h>

typedef struct __DRIcopperExtensionRec          __DRIcopperExtension;
typedef struct __DRIcopperLoaderExtensionRec    __DRIcopperLoaderExtension;

/**
 * This extension defines the core GL-atop-VK functionality. This is used by the
 * zink driver to implement GL (or other APIs) natively atop Vulkan, without
 * relying on a particular window system or DRI protocol.
 */
#define __DRI_COPPER "DRI_Copper"
#define __DRI_COPPER_VERSION 1

struct __DRIcopperExtensionRec {
   __DRIextension base;

   /* vulkan setup glue */
   void *(*CreateInstance)(uint32_t num_extensions,
                           const char * const * extensions);
   void *(*GetInstanceProcAddr)(VkInstance instance, const char *proc);

   VkInstance (*GetInstance)(__DRIscreen *screen);

   /* drawable stuff */
   VkSwapchainKHR (*CreateSwapchain)(__DRIscreen *screen,
                                     const VkSwapchainCreateInfoKHR *ci);

   /* Seriously reconsider whether you need anything here that can't
    * be satisfied from the core or image extension. Answer should be no
    * if at all possible.
    */
};

/**
 * Copper loader extension.
 */
#define __DRI_COPPER_LOADER "DRI_CopperLoader"
#define __DRI_COPPER_LOADER_VERSION 0
struct __DRIcopperLoaderExtensionRec {
    __DRIextension base;

    int (*GetDrawableInfo)(__DRIdrawable *draw, int *w, int *h, void *closure);
};
#endif /* COPPER_INTERFACE_H */
