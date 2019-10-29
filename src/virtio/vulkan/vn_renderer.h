/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_RENDERER_H
#define VN_RENDERER_H

#include "vn_common.h"

struct vn_renderer {
   void (*destroy)(struct vn_renderer *renderer,
                   const VkAllocationCallbacks *alloc);
};

VkResult
vn_renderer_create_vtest(struct vn_instance *instance,
                         const VkAllocationCallbacks *alloc,
                         struct vn_renderer **renderer);

static inline VkResult
vn_renderer_create(struct vn_instance *instance,
                   const VkAllocationCallbacks *alloc,
                   struct vn_renderer **renderer)
{
   if (VN_DEBUG(VTEST))
      return vn_renderer_create_vtest(instance, alloc, renderer);
   else
      return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static inline void
vn_renderer_destroy(struct vn_renderer *renderer,
                    const VkAllocationCallbacks *alloc)
{
   renderer->destroy(renderer, alloc);
}

#endif /* VN_RENDERER_H */
