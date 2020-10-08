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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <unistd.h>
#include <fcntl.h>

#include "loader.h"
#include "target-helpers/drm_helper_public.h"
#include "frontend/drm_driver.h"
#include "pipe_loader_priv.h"

#include "util/os_file.h"
#include "util/u_memory.h"
#include "util/u_dl.h"
#include "util/u_debug.h"
#include "util/xmlconfig.h"

#include <vulkan/vulkan.h>
#include "zink/zink_public.h"

struct pipe_loader_vk_device {
   struct pipe_loader_device base;
   // const struct drm_driver_descriptor *dd;
#ifndef GALLIUM_STATIC_TARGETS
   struct util_dl_library *lib;
#endif
   VkInstance instance;
   VkPhysicalDevice dev;
};

#define pipe_loader_vk_device(dev) ((struct pipe_loader_vk_device *)dev)

static const struct pipe_loader_ops pipe_loader_vk_ops;

bool
pipe_loader_vk_probe_one(struct pipe_loader_device **dev, void *instance,
                         void *pdev)
{
   struct pipe_loader_vk_device *ddev = CALLOC_STRUCT(pipe_loader_vk_device);

   if (!ddev)
      return false;

   /* XXX more */
   ddev->base.driver_name = "zink";
   ddev->base.ops = &pipe_loader_vk_ops;
   ddev->instance = instance;
   ddev->dev = pdev;

   *dev = &ddev->base;
   return true;

//fail:
#ifndef GALLIUM_STATIC_TARGETS
   if (ddev->lib)
      util_dl_close(ddev->lib);
#endif
   FREE(ddev);
   return false;
}

/* XXX copypasta from zink_screen.c */
static VkInstance
create_instance(void)
{
   VkApplicationInfo ai = {};
   ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

   /* XXX just can't be bothered to find the header decling it yet */
#if 0
   char proc_name[128];
   if (os_get_process_name(proc_name, ARRAY_SIZE(proc_name)))
      ai.pApplicationName = proc_name;
   else
#endif
      ai.pApplicationName = "unknown";

   ai.pEngineName = "mesa zink";
   ai.apiVersion = VK_API_VERSION_1_0;

   const char *extensions[] = {
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
   };

   VkInstanceCreateInfo ici = {};
   ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   ici.pApplicationInfo = &ai;
   ici.ppEnabledExtensionNames = extensions;
   ici.enabledExtensionCount = ARRAY_SIZE(extensions);

   VkInstance instance = VK_NULL_HANDLE;
   VkResult err = vkCreateInstance(&ici, NULL, &instance);
   if (err != VK_SUCCESS)
      return VK_NULL_HANDLE;

   return instance;
}

int
pipe_loader_vk_probe(struct pipe_loader_device **devs, int ndev)
{
   uint32_t i = 0, j = 0, pdev_count = 0;
   VkPhysicalDevice *pdevs;

   VkInstance instance = create_instance();

   vkEnumeratePhysicalDevices(instance, &pdev_count, NULL);
   if (pdev_count == 0)
      return 0;

   pdevs = malloc(sizeof(*pdevs) * pdev_count);
   vkEnumeratePhysicalDevices(instance, &pdev_count, pdevs);
   assert(pdev_count > 0);

   for (i = 0; i < pdev_count; i++) {
      struct pipe_loader_device *dev;

      if (!pipe_loader_vk_probe_one(&dev, instance, pdevs[i]))
         continue;

      if (j < ndev) {
         devs[j] = dev;
      } else {
         dev->ops->release(&dev);
      }
      j++;
   }

   return j;
}

static void
pipe_loader_vk_release(struct pipe_loader_device **dev)
{
   // struct pipe_loader_vk_device *ddev = pipe_loader_vk_device(*dev);

#ifndef GALLIUM_STATIC_TARGETS
   if (ddev->lib)
      util_dl_close(ddev->lib);
#endif

   pipe_loader_base_release(dev);
}

static const struct driOptionDescription *
pipe_loader_vk_get_driconf(struct pipe_loader_device *dev, unsigned *count)
{
   *count = 0;
   return NULL; /* XXX */
}

static struct pipe_screen *
pipe_loader_vk_create_screen(struct pipe_loader_device *dev,
                             const struct pipe_screen_config *config)
{
   struct pipe_loader_vk_device *ddev = pipe_loader_vk_device(dev);

   return zink_vk_create_screen(ddev->dev, ddev->instance, config);
}

static const struct pipe_loader_ops pipe_loader_vk_ops = {
   .create_screen = pipe_loader_vk_create_screen,
   .get_driconf = pipe_loader_vk_get_driconf,
   .release = pipe_loader_vk_release
};
