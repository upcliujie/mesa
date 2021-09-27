/*
 * Copyright 2020 Red Hat, Inc.
 * Copyright © 2021 Valve Corporation
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


#include "zink_screen.h"
#include "zink_resource.h"
#include "zink_copper.h"

union copper_loader_info {
   VkBaseOutStructure bos;
   VkXcbSurfaceCreateInfoKHR xcb;
};

struct copper_winsys
{
   // probably just embed this all in the pipe_screen
   struct sw_winsys base;

   const struct copper_loader_funcs *loader;
};

#define copper_displaytarget(dt) ((struct copper_displaytarget*)dt)

// not sure if cute or vile
static struct zink_screen *
copper_winsys_screen(struct sw_winsys *ws)
{
    return container_of(ws, struct zink_screen, winsys);
}

static VkSurfaceKHR
copper_CreateSurface(struct zink_screen *screen, struct copper_displaytarget *cdt, const union copper_loader_info *info)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult error = VK_SUCCESS;

    VkStructureType type = info->bos.sType;
    switch (type) {
    case VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR:
       error = VKSCR(CreateXcbSurfaceKHR)(screen->instance, &info->xcb, NULL, &surface);
       break;
    default:
       unreachable("unsupported!");
    }

    if (error != VK_SUCCESS) {
       return VK_NULL_HANDLE;
    }

    VkBool32 supported;
    error = VKSCR(GetPhysicalDeviceSurfaceSupportKHR)(screen->pdev, screen->gfx_queue, surface, &supported);
    if (!zink_screen_handle_vkresult(screen, error) || !supported) {
       VKSCR(DestroySurfaceKHR)(screen->instance, surface, NULL);
       return VK_NULL_HANDLE;
    }

    error = VKSCR(GetPhysicalDeviceSurfaceCapabilitiesKHR)(screen->pdev, surface, &cdt->caps);
    if (!zink_screen_handle_vkresult(screen, error)) {
       VKSCR(DestroySurfaceKHR)(screen->instance, surface, NULL);
       return VK_NULL_HANDLE;
    }

    return surface;
}

static VkSwapchainKHR
copper_CreateSwapchain(struct zink_screen *screen, struct copper_displaytarget *cdt)
{
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult error = VK_SUCCESS;

    /* static init */
    if (cdt->swapchain == VK_NULL_HANDLE) {
        cdt->scci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        cdt->scci.pNext = NULL;
        cdt->scci.flags = 0;                   // probably not that interesting...
        cdt->scci.minImageCount = cdt->caps.minImageCount;   // n-buffering
        cdt->scci.imageFormat = zink_get_format(screen, cdt->format);
        cdt->scci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        cdt->scci.imageArrayLayers = 1;        // XXX stereo
        cdt->scci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        cdt->scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;        // XXX no idea
        cdt->scci.queueFamilyIndexCount = 0;
        cdt->scci.pQueueFamilyIndices = NULL;
        cdt->scci.preTransform = cdt->caps.currentTransform;
        cdt->scci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // XXX handle 
        cdt->scci.presentMode = VK_PRESENT_MODE_FIFO_KHR;              // XXX swapint
        cdt->scci.clipped = VK_TRUE;                                   // XXX hmm
    }

    cdt->scci.surface = cdt->surface;
    cdt->scci.imageExtent.width = MAX3(cdt->extent.width, cdt->caps.currentExtent.width, cdt->caps.minImageExtent.width);
    cdt->scci.imageExtent.height = MAX3(cdt->extent.height, cdt->caps.currentExtent.height, cdt->caps.minImageExtent.height);
    cdt->scci.oldSwapchain = cdt->swapchain;

    error = VKSCR(CreateSwapchainKHR)(screen->dev, &cdt->scci, NULL,
                                 &swapchain);
    if (error != VK_SUCCESS) {
        // do something
    }

    return swapchain;
}

static bool
copper_GetSwapchainImages(struct zink_screen *screen, struct copper_displaytarget *cdt)
{
   free(cdt->images);
   VkResult error = VKSCR(GetSwapchainImagesKHR)(screen->dev, cdt->swapchain, &cdt->num_images, NULL);
   cdt->images = malloc(sizeof(VkImage) * cdt->num_images);
   error = VKSCR(GetSwapchainImagesKHR)(screen->dev, cdt->swapchain, &cdt->num_images, cdt->images);
   return zink_screen_handle_vkresult(screen, error);

}

static struct sw_displaytarget *
copper_displaytarget_create(struct sw_winsys *ws, unsigned tex_usage,
                            enum pipe_format format, unsigned width,
                            unsigned height, unsigned alignment,
                            const void *loader_private, unsigned *stride)
{
   struct zink_screen *screen = copper_winsys_screen(ws);
   struct copper_displaytarget *cdt;
   const union copper_loader_info *info = loader_private;
   unsigned nblocksy, size, format_stride;

   cdt = CALLOC_STRUCT(copper_displaytarget);
   if (!cdt)
      return NULL;

   cdt->refcount = 1;
   cdt->format = format;
   cdt->extent.width = width;
   cdt->extent.height = height;
   cdt->loader_private = (void*)loader_private;

   cdt->surface = copper_CreateSurface(screen, cdt, info);
   if (!cdt->surface)
      goto out;

   cdt->swapchain = copper_CreateSwapchain(screen, cdt);
   if (!cdt->swapchain)
      goto out;

   if (!copper_GetSwapchainImages(screen, cdt))
      goto out;

   *stride = cdt->stride;
   return (struct sw_displaytarget *)cdt;

//moar cleanup
out:
   return NULL;
}

static void
copper_displaytarget_destroy(struct sw_winsys *ws, struct sw_displaytarget *dt)
{
   struct zink_screen *screen = copper_winsys_screen(ws);
   struct copper_displaytarget *cdt = copper_displaytarget(dt);
   if (!p_atomic_dec_zero(&cdt->refcount))
      return;
   VKSCR(DestroySwapchainKHR)(screen->dev, cdt->swapchain, NULL);
   VKSCR(DestroySurfaceKHR)(screen->instance, cdt->surface, NULL);
   free(cdt->images);
   FREE(dt);
}

struct sw_winsys zink_copper = {
   .destroy = NULL,
   .is_displaytarget_format_supported = NULL,
   .displaytarget_create = copper_displaytarget_create,
   .displaytarget_from_handle = NULL,
   .displaytarget_get_handle = NULL,
   .displaytarget_map = NULL,
   .displaytarget_unmap = NULL,
   .displaytarget_display = NULL,
   .displaytarget_destroy = copper_displaytarget_destroy,
};

bool
zink_copper_acquire(struct zink_screen *screen, struct zink_resource *res, uint64_t timeout)
{
   assert(res->obj->dt);
   struct copper_displaytarget *cdt = copper_displaytarget(res->obj->dt);
   if (res->obj->acquire)
      return true;
   VkSemaphoreCreateInfo sci = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      NULL,
      0
   };
   VkSemaphore acquire;
   VkResult ret = VKSCR(CreateSemaphore)(screen->dev, &sci, NULL, &acquire);
   assert(acquire);
   if (ret != VK_SUCCESS)
      return false;
   unsigned prev = res->obj->dt_idx;
   ret = VKSCR(AcquireNextImageKHR)(screen->dev, cdt->swapchain, timeout, acquire, VK_NULL_HANDLE, &res->obj->dt_idx);
   if (ret != VK_SUCCESS && ret != VK_SUBOPTIMAL_KHR) {
      VKSCR(DestroySemaphore)(screen->dev, acquire, NULL);
      return false;
   }
   assert(prev != res->obj->dt_idx);
   VKSCR(DestroySemaphore)(screen->dev, res->obj->acquire, NULL);
   res->obj->acquire = acquire;
   res->obj->image = cdt->images[res->obj->dt_idx];
   res->obj->acquired = false;
   return ret == VK_SUCCESS;
}

VkSemaphore
zink_copper_acquire_submit(struct zink_screen *screen, struct zink_resource *res)
{
   assert(res->obj->dt);
   struct copper_displaytarget *cdt = copper_displaytarget(res->obj->dt);
   if (res->obj->acquired)
      return VK_NULL_HANDLE;
   assert(res->obj->acquire);
   res->obj->acquired = true;
   return res->obj->acquire;
}

VkSemaphore
zink_copper_present(struct zink_screen *screen, struct zink_resource *res)
{
   assert(res->obj->dt);
   struct copper_displaytarget *cdt = copper_displaytarget(res->obj->dt);
   assert(!res->obj->present);
   VkSemaphoreCreateInfo sci = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      NULL,
      0
   };
   assert(res->obj->acquired);
   //error checking
   VkResult ret = VKSCR(CreateSemaphore)(screen->dev, &sci, NULL, &res->obj->present);
   return res->obj->present;
}

struct copper_present_info {
   VkPresentInfoKHR info;
   uint32_t image;
   VkSemaphore sem;
};

static void
copper_present(void *data, void *gdata, int thread_idx)
{
   struct copper_present_info *cpi = data;
   struct zink_screen *screen = gdata;
   VkResult error;
   cpi->info.pResults = &error;

   VkResult error2 = VKSCR(QueuePresentKHR)(screen->thread_queue, &cpi->info);
   free(cpi);
}

void
zink_copper_present_queue(struct zink_screen *screen, struct zink_resource *res)
{
   assert(res->obj->dt);
   struct copper_displaytarget *cdt = copper_displaytarget(res->obj->dt);
   assert(res->obj->present);
   assert(res->obj->acquired);
   struct copper_present_info *cpi = malloc(sizeof(struct copper_present_info));
   cpi->sem = res->obj->present;
   cdt->last_image = cpi->image = res->obj->dt_idx;
   cpi->info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   cpi->info.pNext = NULL;
   cpi->info.waitSemaphoreCount = 1;
   cpi->info.pWaitSemaphores = &cpi->sem;
   cpi->info.swapchainCount = 1;
   cpi->info.pSwapchains = &cdt->swapchain;
   cpi->info.pImageIndices = &cpi->image;
   cpi->info.pResults = NULL;
   res->obj->present = NULL;
   if (screen->threaded
#ifndef _WIN32
       && !screen->renderdoc_api
#endif
       ) {
      util_queue_add_job(&screen->flush_queue, cpi, NULL,
                         copper_present, NULL, 0);
   } else {
      copper_present(cpi, screen, 0);
   }
   res->obj->acquire = VK_NULL_HANDLE;
   res->obj->acquired = false;
}
