/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#include "util/format/u_format.h"
#include "u_sampler.h"


/**
 * Initialize a pipe_sampler_view.  'view' is considered to have
 * uninitialized contents.
 */
static void
default_template(struct pipe_sampler_view *view,
                 const struct pipe_resource *texture,
                 enum pipe_format format,
                 unsigned expand_green_blue)
{
   memset(view, 0, sizeof(*view));

   /* XXX: Check if format is compatible with texture->format.
    */

   view->target = texture->target;
   view->format = format;
   view->u.tex.first_level = 0;
   view->u.tex.last_level = texture->last_level;
   view->u.tex.first_layer = 0;
   view->u.tex.last_layer = texture->target == PIPE_TEXTURE_3D ?
                               texture->depth0 - 1 : texture->array_size - 1;
   view->swizzle_r = PIPE_SWIZZLE_X;
   view->swizzle_g = PIPE_SWIZZLE_Y;
   view->swizzle_b = PIPE_SWIZZLE_Z;
   view->swizzle_a = PIPE_SWIZZLE_W;

   /* Override default green and blue component expansion to the requested
    * one.
    *
    * Gallium expands nonexistent components to (0,0,0,1), DX9 expands
    * to (1,1,1,1).  Since alpha is always expanded to 1, and red is
    * always present, we only really care about green and blue
    * components.
    *
    * To make it look less hackish, one would have to add
    * PIPE_SWIZZLE_EXPAND to indicate components for expansion
    * and then override without exceptions or favoring one component
    * over another.
    */
   if (format != PIPE_FORMAT_A8_UNORM) {
      const struct util_format_description *desc = util_format_description(format);

      assert(desc);
      if (desc) {
         if (desc->swizzle[1] == PIPE_SWIZZLE_0) {
            view->swizzle_g = expand_green_blue;
         }
         if (desc->swizzle[2] == PIPE_SWIZZLE_0) {
            view->swizzle_b = expand_green_blue;
         }
      }
   }
}

void
u_sampler_view_default_template(struct pipe_sampler_view *view,
                                const struct pipe_resource *texture,
                                enum pipe_format format)
{
   /* Expand to (0, 0, 0, 1) */
   default_template(view,
                    texture,
                    format,
                    PIPE_SWIZZLE_0);
}

void
u_sampler_view_default_dx9_template(struct pipe_sampler_view *view,
                                    const struct pipe_resource *texture,
                                    enum pipe_format format)
{
   /* Expand to (1, 1, 1, 1) */
   default_template(view,
                    texture,
                    format,
                    PIPE_SWIZZLE_1);
}

static inline void
swizzle_src(bool src_is_argb, bool src_is_abgr, unsigned char *swizz)
{
   if (src_is_argb) {
      /* compose swizzle with alpha at the end */
      swizz[0] = PIPE_SWIZZLE_Y;
      swizz[1] = PIPE_SWIZZLE_Z;
      swizz[2] = PIPE_SWIZZLE_W;
      swizz[3] = PIPE_SWIZZLE_X;
   } else if (src_is_abgr) {
      /* this is weird because we're already using a codepath with
       * a swizzle
       */
      swizz[0] = PIPE_SWIZZLE_W;
      swizz[1] = PIPE_SWIZZLE_X;
      swizz[2] = PIPE_SWIZZLE_Y;
      swizz[3] = PIPE_SWIZZLE_Z;
   }
}

void
u_sampler_view_swizzle_argb(struct pipe_sampler_view *view,
                            enum pipe_format dst_format)
{
   bool src_is_argb = util_format_is_argb(view->format);
   bool src_is_abgr = util_format_is_abgr(view->format);
   bool dst_is_argb = util_format_is_argb(dst_format);
   bool dst_is_abgr = util_format_is_abgr(dst_format);

   if (src_is_argb == dst_is_argb && src_is_abgr == dst_is_abgr)
      return;

   unsigned char view_swiz[4] = {
      view->swizzle_r,
      view->swizzle_g,
      view->swizzle_b,
      view->swizzle_a,
   };
   unsigned char dst_swiz[4];

   if (src_is_argb || src_is_abgr) {
      unsigned char reverse_alpha[4];
      swizzle_src(src_is_argb, src_is_abgr, reverse_alpha);
      util_format_compose_swizzles(view_swiz, reverse_alpha, dst_swiz);
   } else if (dst_is_argb) {
      unsigned char reverse_alpha[] = {
         PIPE_SWIZZLE_W,
         PIPE_SWIZZLE_X,
         PIPE_SWIZZLE_Y,
         PIPE_SWIZZLE_Z,
      };
      /* compose swizzle with alpha at the start */
      util_format_compose_swizzles(view_swiz, reverse_alpha, dst_swiz);
   } else if (dst_is_abgr) {
      unsigned char reverse_alpha[] = {
         PIPE_SWIZZLE_Y,
         PIPE_SWIZZLE_Z,
         PIPE_SWIZZLE_W,
         PIPE_SWIZZLE_X,
      };
      util_format_compose_swizzles(view_swiz, reverse_alpha, dst_swiz);
   }
   view->swizzle_r = dst_swiz[PIPE_SWIZZLE_X];
   view->swizzle_g = dst_swiz[PIPE_SWIZZLE_Y];
   view->swizzle_b = dst_swiz[PIPE_SWIZZLE_Z];
   view->swizzle_a = dst_swiz[PIPE_SWIZZLE_W];
}

void
u_sampler_format_swizzle_color_argb(union pipe_color_union *color, bool is_integer)
{
   unsigned char reverse_alpha[4];
   swizzle_src(true, false, reverse_alpha);
   union pipe_color_union src_color = *color;
   util_format_apply_color_swizzle(color, &src_color, reverse_alpha,
                                   is_integer);
}

void
u_sampler_format_swizzle_color_abgr(union pipe_color_union *color, bool is_integer)
{
   unsigned char reverse_alpha[4];
   swizzle_src(false, true, reverse_alpha);
   union pipe_color_union src_color = *color;
   util_format_apply_color_swizzle(color, &src_color, reverse_alpha,
                                   is_integer);
}
