#include <stdio.h>
#include <sys/ioctl.h>

#include "drm-uapi/i915_drm.h"

#include "frontend/drm_driver.h"

#include "common/intel_gem.h"
#include "i915_drm_winsys.h"
#include "i915_drm_public.h"
#include "util/u_memory.h"


/*
 * Helper functions
 */


static int
i915_drm_aperture_size(struct i915_winsys *iws)
{
   struct i915_drm_winsys *idws = i915_drm_winsys(iws);
   size_t aper_size, mappable_size;

   drm_intel_get_aperture_sizes(idws->fd, &mappable_size, &aper_size);

   return aper_size >> 20;
}

static void
i915_drm_winsys_destroy(struct i915_winsys *iws)
{
   struct i915_drm_winsys *idws = i915_drm_winsys(iws);

   drm_intel_bufmgr_destroy(idws->gem_manager);

   FREE(idws);
}

struct i915_winsys *
i915_drm_winsys_create(int drmFD)
{
   struct i915_drm_winsys *idws;

   idws = CALLOC_STRUCT(i915_drm_winsys);
   if (!idws)
      return NULL;

   i915_drm_winsys_init_batchbuffer_functions(idws);
   i915_drm_winsys_init_buffer_functions(idws);
   i915_drm_winsys_init_fence_functions(idws);

   idws->fd = drmFD;
   idws->base.pci_id = intel_getparam_integer(drmFD, I915_PARAM_CHIPSET_ID);
   assert(idws->base.pci_id != -1);
   idws->max_batch_size = 1 * 4096;

   idws->base.aperture_size = i915_drm_aperture_size;
   idws->base.destroy = i915_drm_winsys_destroy;

   idws->gem_manager = drm_intel_bufmgr_gem_init(idws->fd, idws->max_batch_size);
   drm_intel_bufmgr_gem_enable_reuse(idws->gem_manager);
   drm_intel_bufmgr_gem_enable_fenced_relocs(idws->gem_manager);

   idws->dump_cmd = debug_get_bool_option("I915_DUMP_CMD", false);
   idws->dump_raw_file = debug_get_option("I915_DUMP_RAW_FILE", NULL);
   idws->send_cmd = !debug_get_bool_option("I915_NO_HW", false);

   return &idws->base;
}
