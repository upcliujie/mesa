/*
 * Copyright © 2020 Google, Inc.
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

#ifndef _U_TRACE_GALLIUM_H
#define _U_TRACE_GALLIUM_H

#include "util/perf/u_trace.h"
#include "pipe/p_state.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* Gallium specific u_trace helpers */

/*
 * In some cases it is useful to have composite tracepoints like this,
 * to log more complex data structures.
 */

static inline void
trace_framebuffer_state(struct u_trace *ut, const struct pipe_framebuffer_state *pfb)
{
   if (likely(!ut->enabled))
      return;

   trace_framebuffer(ut,
      pfb->width,
      pfb->height,
      pfb->layers,
      pfb->samples,
      pfb->nr_cbufs);

   for (unsigned i = 0; i < pfb->nr_cbufs; i++) {
      if (pfb->cbufs[i]) {
        struct pipe_surface *psurf = pfb->cbufs[i];
         trace_surface(ut,
            psurf->width,
            psurf->height,
            psurf->nr_samples,
            util_format_short_name(psurf->format));
      }
   }
   if (pfb->zsbuf) {
        struct pipe_surface *psurf = pfb->zsbuf;
         trace_surface(ut,
            psurf->width,
            psurf->height,
            psurf->nr_samples,
            util_format_short_name(psurf->format));
   }
}

static inline void
trace_grid_info_pipe(struct u_trace *ut, const struct pipe_grid_info *pgrid)
{
   trace_grid_info(ut,
      pgrid->work_dim,
      pgrid->block[0],
      pgrid->block[1],
      pgrid->block[2],
      pgrid->grid[0],
      pgrid->grid[1],
      pgrid->grid[2]);
}

#ifdef  __cplusplus
}
#endif

#endif  /* _U_TRACE_GALLIUM_H */
