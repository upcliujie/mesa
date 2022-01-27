/*
 * Copyright 2020 Intel Corporation
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice (including the next
 *  paragraph) shall be included in all copies or substantial portions of the
 *  Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

#include <stdint.h>

#define __gen_address_type uint64_t
#define __gen_user_data void

static uint64_t
__gen_combine_address(__attribute__((unused)) void *data,
                      __attribute__((unused)) void *loc, uint64_t addr,
                      uint32_t delta)
{
   return addr + delta;
}

#include "genxml/gen_macros.h"
#include "genxml/genX_pack.h"

#include "isl_priv.h"

#if GFX_VERx10 >= 125
static const uint8_t isl_encode_tiling[] = {
   [ISL_TILING_4]  = TILE4,
   [ISL_TILING_64] = TILE64,
};
#endif

void
isl_genX(emit_cpb_control_s)(const struct isl_device *dev, void *batch,
                             const struct isl_cpb_emit_info *restrict info)
{
#if GFX_VERx10 >= 125
   struct GENX(3DSTATE_CPSIZE_CONTROL_BUFFER) cpb = {
      GENX(3DSTATE_CPSIZE_CONTROL_BUFFER_header),
   };

   if (info->surf) {
      /* BSpec 46962:
       *
       *   "The Width specified by this field must be less than or equal to
       *    the surface pitch (specified in bytes via the Surface Pitch field).
       *    For cube maps, Width must be set equal to Height.
       *
       *    1. The Width ofthis buffer must be the same as the Width of the
       *       render target(s) (defined in SURFACE_STATE), unless Surface
       *       Type is SURFTYPE_1D or SURFTYPE_2D with Depth = 0 (non-array)
       *       and LOD = 0 (non-mip mapped).
       *
       *    2. Depth buffer (defined in 3DSTATE_DEPTH_BUFFER) unless either
       *       the depth buffer or this buffer surf_typeare SURFTYPE_NULL
       */
      cpb.Width                  = ((info->color_surf &&
                                     info->color_surf->dim != ISL_SURF_DIM_1D &&
                                     (info->color_surf->dim != ISL_SURF_DIM_2D &&
                                      info->color_surf->logical_level0_px.depth == 1 &&
                                      info->color_surf->levels == 1)) ?
                                    info->color_surf->logical_level0_px.width :
                                    (info->depth_surf ?
                                     info->depth_surf->logical_level0_px.width :
                                     info->surf->logical_level0_px.width * 8)) - 1;
      /* BSpec 46962:
       *
       *   "The Height of this buffer must be the same as the
       *
       *    1. Height of the render target(s) (defined in SURFACE_STATE),
       *       unless Surface Type is SURFTYPE_2D with Depth = 0 (non-array)
       *       and LOD = 0 (non-mip mapped).
       *
       *    2. Depth buffer (defined in 3DSTATE_DEPTH_BUFFER) unless either
       *       the depth buffer or this buffer surf_typeare SURFTYPE_NULL"
       */
      cpb.Height                 = ((info->color_surf->dim != ISL_SURF_DIM_1D &&
                                     (info->color_surf->dim != ISL_SURF_DIM_2D &&
                                      info->color_surf->logical_level0_px.depth == 1 &&
                                      info->color_surf->levels == 1)) ?
                                    info->color_surf->logical_level0_px.height :
                                    (info->depth_surf ?
                                     info->depth_surf->logical_level0_px.height :
                                     info->surf->logical_level0_px.height * 8)) - 1;
      /* BSpec 46962:
       *
       *   "The Depth of this buffer must be the same as
       *
       *    1. The Depth of the render target(s) (defined in SURFACE_STATE).
       *
       *    2. Depth buffer (defined in 3DSTATE_DEPTH_BUFFER) unless Depth
       *       buffer surf_type is SURFTYPE_NULL
       */
      cpb.Depth                  = (info->color_view ?
                                    info->color_view->array_len :
                                    (info->depth_view ?
                                     info->depth_view->array_len :
                                     info->view->array_len)) - 1;

      /* "Must be zero" for
       *
       * "(Structure[RENDER_SURFACE_STATE][Surface Type]=='SURFTYPE_CUBE')"
       *
       *  otherwise "Number of array elements- 1"
       */
      cpb.RenderTargetViewExtent =
         (info->color_view &&
          (info->color_view->usage & ISL_SURF_USAGE_CUBE_BIT)) ? 0 : cpb.Depth;

      /* Bspect 46962:
       *
       *   "Minimum array element of the this buffer must be the same as the
       *    Surface Type of the
       *
       *    1. Render target(s) (defined in SURFACE_STATE), unless either the
       *       this buffer or render targets are SURFTYPE_NULL
       *
       *    2. Depth buffer (defined in 3DSTATE_DEPTH_BUFFER) unless either
       *       the depth buffer or this buffer surf_type are SURFTYPE_NULL
       */
      cpb.SurfLOD                = info->color_view ?
                                   info->color_view->base_level :
                                   (info->depth_view ?
                                    info->depth_view->base_level :
                                    info->view->base_array_layer);

      /* BSpec 46962:
       *
       *   "Minimum array element of the this buffer must be the same as the
       *    Surface Type of the
       *
       *    1. Render target(s) (defined in SURFACE_STATE), unless either the
       *       this buffer or render targets are SURFTYPE_NULL
       *
       *    2. Depth buffer (defined in 3DSTATE_DEPTH_BUFFER) unless either
       *       the depth buffer or this buffer surf_type are SURFTYPE_NULL
       */
      cpb.MinimumArrayElement    = info->color_view ?
                                   info->color_view->base_array_layer :
                                   (info->depth_view ?
                                    info->depth_view->base_array_layer :
                                    info->view->base_array_layer);

      /* Here it's coming from the actual control surface */
      cpb.SurfaceType            = SURFTYPE_2D;
      cpb.SurfacePitch           = info->surf->row_pitch_B - 1;
      cpb.MOCS                   = info->mocs;
      cpb.SurfaceQPitch          = isl_surf_get_array_pitch_sa_rows(info->surf) >> 2;
      cpb.TiledMode              = isl_encode_tiling[info->surf->tiling];
      cpb.SurfaceBaseAddress     = info->address;

      /* We don't use miptails yet. The PRM recommends that you set "Mip Tail
       * Start LOD" to 15 to prevent the hardware from trying to use them.
       */
      cpb.MipTailStartLOD        = 15;
      /* TODO:
       *
       * cpb.CPCBCompressionEnable is this CCS compression? Currently disabled
       * in isl_surf_supports_ccs() for CPB buffers.
       */
      assert(cpb.Width <= cpb.SurfacePitch);
   } else {
      cpb.SurfaceType  = SURFTYPE_NULL;
      cpb.TiledMode    = TILE64;
   }

   /* Pack everything into the batch */
   uint32_t *dw = batch;
   GENX(3DSTATE_CPSIZE_CONTROL_BUFFER_pack)(NULL, dw, &cpb);
#else
   unreachable("Coarse pixel shading not supported");
#endif
}
