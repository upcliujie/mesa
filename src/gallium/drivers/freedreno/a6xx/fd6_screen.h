/*
 * Copyright (C) 2016 Rob Clark <robclark@freedesktop.org>
 * Copyright © 2018 Google, Inc.
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
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#ifndef FD6_SCREEN_H_
#define FD6_SCREEN_H_

#include "pipe/p_screen.h"

void fd6_screen_init(struct pipe_screen *pscreen);

static inline uint32_t
fd6_clamp_buffer_size(enum pipe_format format, uint32_t size)
{
   /* matches PIPE_CAP_MAX_TEXEL_BUFFER_ELEMENTS_UINT: */
   const unsigned max_texel_buffer_elements = 1 << 27;

   /* The spec says:
    *    The number of texels in the texel array is then clamped to the value of
    *    the implementation-dependent limit GL_MAX_TEXTURE_BUFFER_SIZE.
    *
    * So compute the number of texels, compare to GL_MAX_TEXTURE_BUFFER_SIZE and update it.
    */
   unsigned blocksize = util_format_get_blocksize(format);
   unsigned elements = MIN2(max_texel_buffer_elements, size / blocksize);

   return elements * blocksize;
}


#endif /* FD6_SCREEN_H_ */
