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
      cpb.SurfaceType            = SURFTYPE_2D;
      cpb.SurfacePitch           = info->surf->row_pitch_B - 1;
      /* The surface size is 1/8th of the render target, but we need to
       * program the same size as the render target.
       */
      cpb.Width                  = info->surf->logical_level0_px.width * 8 - 1;
      cpb.Height                 = info->surf->logical_level0_px.height * 8 - 1;
      cpb.Depth                  = info->view->array_len - 1;
      cpb.MOCS                   = info->mocs;
      cpb.RenderTargetViewExtent = info->view->array_len - 1;
      cpb.SurfLOD                = info->view->base_level;
      cpb.MinimumArrayElement    = info->view->base_array_layer;
      cpb.SurfaceQPitch          = isl_surf_get_array_pitch_sa_rows(info->surf) >> 2;
      cpb.TiledMode              = isl_encode_tiling[info->surf->tiling];
      cpb.SurfaceBaseAddress     = info->address;

      /* We don't use miptails yet. The PRM recommends that you set "Mip Tail
       * Start LOD" to 15 to prevent the hardware from trying to use them.
       */
      cpb.MipTailStartLOD        = 15;
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
