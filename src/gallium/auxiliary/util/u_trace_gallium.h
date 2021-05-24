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

void __trace_surface(struct u_trace *ut, const struct pipe_surface *psurf);
void __trace_framebuffer(struct u_trace *ut, const struct pipe_framebuffer_state *pfb);

static inline void
trace_framebuffer_state(struct u_trace *ut, const struct pipe_framebuffer_state *pfb)
{
   if (likely(!ut->enabled))
      return;

   __trace_framebuffer(ut, pfb);
   for (unsigned i = 0; i < pfb->nr_cbufs; i++) {
      if (pfb->cbufs[i]) {
         __trace_surface(ut, pfb->cbufs[i]);
      }
   }
   if (pfb->zsbuf) {
      __trace_surface(ut, pfb->zsbuf);
   }
}

#ifdef  __cplusplus
}
#endif

#endif  /* _U_TRACE_GALLIUM_H */
