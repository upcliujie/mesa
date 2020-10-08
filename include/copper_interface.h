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
 *
 * XXX type safety would be nice, wouldn't it
 */
#define __DRI_COPPER "DRI_Copper"
#define __DRI_COPPER_VERSION 1

struct __DRIcopperExtensionRec {
    __DRIextension base;

    /* vulkan setup glue */
    void *(*CreateInstance)(uint32_t num_extensions,
                            const char * const * extensions);
    void *(*GetInstanceProcAddr)(void *instance, const char *proc);
    void (*EnumeratePhysicalDevices)(void *instance,
                                     unsigned int *count,
                                     void ***devs);

    __DRIscreen *(*createVkScreen)(/* VkPhysicalDevice */ void *pdev,
                                   const __DRIextension **loader_extensions,
                                   const __DRIextension **driver_extensions,
                                   const __DRIconfig ***driver_configs,
                                   void *loaderPrivate);

    void (*destroyScreen)(__DRIscreen *screen);

    const __DRIextension **(*getExtensions)(__DRIscreen *screen);

    __DRIdrawable *(*createNewDrawable)(__DRIscreen *screen,
					const __DRIconfig *config,
					void *loaderPrivate);

    void (*destroyDrawable)(__DRIdrawable *drawable);

    void (*swapBuffers)(__DRIdrawable *drawable);

    /* XXX delete this */
    __DRIcontext *(*createNewContext)(__DRIscreen *screen,
				      const __DRIconfig *config,
				      __DRIcontext *shared,
				      void *loaderPrivate);

   __DRIcontext *(*createContextAttribs)(__DRIscreen *screen,
					 int api,
					 const __DRIconfig *config,
					 __DRIcontext *shared,
					 unsigned num_attribs,
					 const uint32_t *attribs,
					 unsigned *error,
					 void *loaderPrivate);

    void (*destroyContext)(__DRIcontext *context);

    int (*bindContext)(__DRIcontext *ctx,
		       __DRIdrawable *pdraw,
		       __DRIdrawable *pread);

    int (*unbindContext)(__DRIcontext *ctx);
};

/**
 * Copper loader extension.
 */
#define __DRI_COPPER_LOADER "DRI_CopperLoader"
#define __DRI_COPPER_LOADER_VERSION 1
struct __DRIcopperLoaderExtensionRec {
    __DRIextension base;

    void *(*GetInstance)(void *vkscr);
};
#endif /* COPPER_INTERFACE_H */
