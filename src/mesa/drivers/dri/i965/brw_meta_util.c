/*
 * Copyright © 2014 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "brw_context.h"
#include "brw_defines.h"
#include "intel_fbo.h"
#include "brw_meta_util.h"
#include "brw_state.h"
#include "main/blend.h"
#include "main/fbobject.h"
#include "util/format_srgb.h"

#define FILE_DEBUG_FLAG DEBUG_BLIT

/**
 * Helper function for handling mirror image blits.
 *
 * If coord0 > coord1, swap them and invert the "mirror" boolean.
 */
static inline void
fixup_mirroring(bool *mirror, int *coord0, int *coord1)
{
   if (*coord0 > *coord1) {
      *mirror = !*mirror;
      int tmp = *coord0;
      *coord0 = *coord1;
      *coord1 = tmp;
   }
}

/**
 * Compute the number of pixels to clip for each side of a rect
 *
 * \param x0 The rect's left coordinate
 * \param y0 The rect's bottom coordinate
 * \param x1 The rect's right coordinate
 * \param y1 The rect's top coordinate
 * \param min_x The clipping region's left coordinate
 * \param min_y The clipping region's bottom coordinate
 * \param max_x The clipping region's right coordinate
 * \param max_y The clipping region's top coordinate
 * \param clipped_x0 The number of pixels to clip from the left side
 * \param clipped_y0 The number of pixels to clip from the bottom side
 * \param clipped_x1 The number of pixels to clip from the right side
 * \param clipped_y1 The number of pixels to clip from the top side
 *
 * \return false if we clip everything away, true otherwise
 */
static inline bool
compute_pixels_clipped(int x0, int y0, int x1, int y1,
                       int min_x, int min_y, int max_x, int max_y,
                       int *clipped_x0, int *clipped_y0, int *clipped_x1, int *clipped_y1)
{
   /* If we are going to clip everything away, stop. */
   if (!(min_x <= max_x &&
         min_y <= max_y &&
         x0 <= max_x &&
         y0 <= max_y &&
         min_x <= x1 &&
         min_y <= y1 &&
         x0 <= x1 &&
         y0 <= y1)) {
      return false;
   }

   if (x0 < min_x)
      *clipped_x0 = min_x - x0;
   else
      *clipped_x0 = 0;
   if (max_x < x1)
      *clipped_x1 = x1 - max_x;
   else
      *clipped_x1 = 0;

   if (y0 < min_y)
      *clipped_y0 = min_y - y0;
   else
      *clipped_y0 = 0;
   if (max_y < y1)
      *clipped_y1 = y1 - max_y;
   else
      *clipped_y1 = 0;

   return true;
}

static inline int
round_scaled_position(int base, double scaled)
{
   int res = round(scaled);
   if (scaled != 0.0) {
      // Scaled value on clipping shouldn't give us 0-position
      // At least 1-pixel has to be except it is really 0-pixel
      int const rounded = res;
      int const diff = base + rounded;
      if (diff == 0)
         res = scaled >= 0.0f ? rounded - 1 : rounded + 1;
      DBG("%s b/s/r/d/r: %d/%lf/%d/%d/%d\n",
          __func__, base, scaled, rounded, diff, res);
   }
   return res;
}

/**
 * Clips a coordinate (left, right, top or bottom) for the src or dst rect
 * (whichever requires the largest clip) and adjusts the coordinate
 * for the other rect accordingly.
 *
 * \param mirror true if mirroring is required
 * \param src the source rect coordinate (for example srcX0)
 * \param dst0 the dst rect coordinate (for example dstX0)
 * \param dst1 the opposite dst rect coordinate (for example dstX1)
 * \param clipped_src0 number of pixels to clip from the src coordinate
 * \param clipped_dst0 number of pixels to clip from the dst coordinate
 * \param clipped_dst1 number of pixels to clip from the opposite dst coordinate
 * \param scale the src vs dst scale involved for that coordinate
 * \param isLeftOrBottom true if we are clipping the left or bottom sides
 *        of the rect.
 */
static inline void
clip_coordinates(bool mirror,
                 int *src, int *dst0, int *dst1,
                 int clipped_src0,
                 int clipped_dst0,
                 int clipped_dst1,
                 double scale,
                 bool isLeftOrBottom)
{
   /* When clipping we need to add or subtract pixels from the original
    * coordinates depending on whether we are acting on the left/bottom
    * or right/top sides of the rect respectively. We assume we have to
    * add them in the code below, and multiply by -1 when we should
    * subtract.
    */
   int mult = isLeftOrBottom ? 1 : -1;

   if (!mirror) {
      if (clipped_src0 >= clipped_dst0 * scale) {
         double const scale_res = clipped_src0 / scale * mult;
         *src += clipped_src0 * mult;
         *dst0 += round_scaled_position(*dst0, scale_res);
      } else {
         double const scale_res = clipped_dst0 * scale * mult;
         *dst0 += clipped_dst0 * mult;
         *src += round_scaled_position(*src, scale_res);
      }
   } else {
      if (clipped_src0 >= clipped_dst1 * scale) {
         double const scale_res = clipped_src0 / scale * mult;
         *src += clipped_src0 * mult;
         *dst1 -= round_scaled_position(-*dst1, scale_res);
      } else {
         double const scale_res = clipped_dst1 * scale * mult;
         *dst1 -= clipped_dst1 * mult;
         *src += round_scaled_position(*src, scale_res);
      }
   }
}

/* INT_MIN has a specific:
 * Result of '0 - INT_MIN' is always negative.
 * So its impossible compute a clip-region for negative dimention.
 * Looks like a workaround but fixes boundary case.
 */
static inline void
fixup_limits(GLint *srcX0, GLint *srcY0,
             GLint *srcX1, GLint *srcY1,
             GLint *dstX0, GLint *dstY0,
             GLint *dstX1, GLint *dstY1)
{
   if (*srcX0 == INT_MIN)
      *srcX0 += 1;
   if (*srcY0 == INT_MIN)
      *srcY0 += 1;
   if (*srcX1 == INT_MIN)
      *srcX1 += 1;
   if (*srcY1 == INT_MIN)
      *srcY1 += 1;
   if (*dstX0 == INT_MIN)
      *dstX0 += 1;
   if (*dstY0 == INT_MIN)
      *dstY0 += 1;
   if (*dstX1 == INT_MIN)
      *dstX1 += 1;
   if (*dstY1 == INT_MIN)
      *dstY1 += 1;
}

bool
brw_meta_mirror_clip_and_scissor(const struct gl_context *ctx,
                                 const struct gl_framebuffer *read_fb,
                                 const struct gl_framebuffer *draw_fb,
                                 GLint *srcX0, GLint *srcY0,
                                 GLint *srcX1, GLint *srcY1,
                                 GLint *dstX0, GLint *dstY0,
                                 GLint *dstX1, GLint *dstY1,
                                 GLdouble *scale_x, GLdouble *scale_y,
                                 bool *mirror_x, bool *mirror_y)
{
   *mirror_x = false;
   *mirror_y = false;
   *scale_x = 0.0;
   *scale_y = 0.0;

   fixup_limits(srcX0, srcY0, srcX1, srcY1,
                dstX0, dstY0, dstX1, dstY1);

   /* Detect if the blit needs to be mirrored */
   fixup_mirroring(mirror_x, srcX0, srcX1);
   fixup_mirroring(mirror_x, dstX0, dstX1);
   fixup_mirroring(mirror_y, srcY0, srcY1);
   fixup_mirroring(mirror_y, dstY0, dstY1);

   /* Compute number of pixels to clip for each side of both rects. Return
    * early if we are going to clip everything away.
    */
   int clip_src_x0;
   int clip_src_x1;
   int clip_src_y0;
   int clip_src_y1;
   int clip_dst_x0;
   int clip_dst_x1;
   int clip_dst_y0;
   int clip_dst_y1;

   if (!compute_pixels_clipped(*srcX0, *srcY0, *srcX1, *srcY1,
                               0, 0, read_fb->Width, read_fb->Height,
                               &clip_src_x0, &clip_src_y0, &clip_src_x1, &clip_src_y1))
   {
      DBG("%s wrong src: (%d,%d;%d,%d) - clipping skipped\n",
          __func__, *srcX0, *srcY0, *srcX1, *srcY1);
      return true;
   }

   if (!compute_pixels_clipped(*dstX0, *dstY0, *dstX1, *dstY1,
                               draw_fb->_Xmin, draw_fb->_Ymin, draw_fb->_Xmax, draw_fb->_Ymax,
                               &clip_dst_x0, &clip_dst_y0, &clip_dst_x1, &clip_dst_y1))
   {
      DBG("%s wrong dst: (%d,%d;%d,%d) - clipping skipped\n",
          __func__, *dstX0, *dstY0, *dstX1, *dstY1);
      return true;
   }

   /* When clipping any of the two rects we need to adjust the coordinates in
    * the other rect considering the scaling factor involved. To obtain the best
    * precision we want to make sure that we only clip once per side to avoid
    * accumulating errors due to the scaling adjustment.
    *
    * For example, if srcX0 and dstX0 need both to be clipped we want to avoid
    * the situation where we clip srcX0 first, then adjust dstX0 accordingly
    * but then we realize that the resulting dstX0 still needs to be clipped,
    * so we clip dstX0 and adjust srcX0 again. Because we are applying scaling
    * factors to adjust the coordinates in each clipping pass we lose some
    * precision and that can affect the results of the blorp blit operation
    * slightly. What we want to do here is detect the rect that we should
    * clip first for each side so that when we adjust the other rect we ensure
    * the resulting coordinate does not need to be clipped again.
    *
    * The code below implements this by comparing the number of pixels that
    * we need to clip for each side of both rects  considering the scales
    * involved. For example, clip_src_x0 represents the number of pixels to be
    * clipped for the src rect's left side, so if clip_src_x0 = 5,
    * clip_dst_x0 = 4 and scaleX = 2 it means that we are clipping more from
    * the dst rect so we should clip dstX0 only and adjust srcX0. This is
    * because clipping 4 pixels in the dst is equivalent to clipping
    * 4 * 2 = 8 > 5 in the src.
    */

   if (*srcX0 == *srcX1 || *srcY0 == *srcY1
       || *dstX0 == *dstX1 || *dstY0 == *dstY1)
      return true;

   *scale_x = (*srcX1 - *srcX0) / (double)(*dstX1 - *dstX0);
   *scale_y = (*srcY1 - *srcY0) / (double)(*dstY1 - *dstY0);

   DBG("%s initial src: (%d,%d;%d,%d), dst: (%d,%d;%d,%d)"
       " -> scaleXY(%lf,%lf)\n",
       __func__, *srcX0, *srcY0, *srcX1, *srcY1,
       *dstX0, *dstY0, *dstX1, *dstY1,
       *scale_x, *scale_y);

   /* Clip left side */
   clip_coordinates(*mirror_x,
                    srcX0, dstX0, dstX1,
                    clip_src_x0, clip_dst_x0, clip_dst_x1,
                    *scale_x, true);

   /* Clip right side */
   clip_coordinates(*mirror_x,
                    srcX1, dstX1, dstX0,
                    clip_src_x1, clip_dst_x1, clip_dst_x0,
                    *scale_x, false);

   /* Clip bottom side */
   clip_coordinates(*mirror_y,
                    srcY0, dstY0, dstY1,
                    clip_src_y0, clip_dst_y0, clip_dst_y1,
                    *scale_y, true);

   /* Clip top side */
   clip_coordinates(*mirror_y,
                    srcY1, dstY1, dstY0,
                    clip_src_y1, clip_dst_y1, clip_dst_y0,
                    *scale_y, false);

   /* Account for the fact that in the system framebuffer, the origin is at
    * the lower left.
    */
   if (read_fb->FlipY) {
      GLint tmp = read_fb->Height - *srcY0;
      *srcY0 = read_fb->Height - *srcY1;
      *srcY1 = tmp;
      *mirror_y = !*mirror_y;
   }
   if (draw_fb->FlipY) {
      GLint tmp = draw_fb->Height - *dstY0;
      *dstY0 = draw_fb->Height - *dstY1;
      *dstY1 = tmp;
      *mirror_y = !*mirror_y;
   }

   DBG("%s clipSrc: (%d,%d;%d,%d), clipDst: (%d,%d;%d,%d)\n",
       __func__, clip_src_x0, clip_src_y0, clip_src_x1, clip_src_y1,
       clip_dst_x0, clip_dst_y0, clip_dst_x1, clip_dst_y1);
   DBG("%s result src: (%d,%d;%d,%d), dst: (%d,%d;%d,%d),"
       " mirror_x: %d, mirror_y: %d\n",
       __func__, *srcX0, *srcY0, *srcX1, *srcY1,
       *dstX0, *dstY0, *dstX1, *dstY1, *mirror_x, *mirror_y);

   /* Check for invalid bounds
    * Can't blit for 0-dimensions
    */
   return *srcX0 == *srcX1 || *srcY0 == *srcY1
      || *dstX0 == *dstX1 || *dstY0 == *dstY1;
}

/**
 * Determine if fast color clear supports the given clear color.
 *
 * Fast color clear can only clear to color values of 1.0 or 0.0.  At the
 * moment we only support floating point, unorm, and snorm buffers.
 */
bool
brw_is_color_fast_clear_compatible(struct brw_context *brw,
                                   const struct intel_mipmap_tree *mt,
                                   const union gl_color_union *color)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   const struct gl_context *ctx = &brw->ctx;

   /* If we're mapping the render format to a different format than the
    * format we use for texturing then it is a bit questionable whether it
    * should be possible to use a fast clear. Although we only actually
    * render using a renderable format, without the override workaround it
    * wouldn't be possible to have a non-renderable surface in a fast clear
    * state so the hardware probably legitimately doesn't need to support
    * this case. At least on Gen9 this really does seem to cause problems.
    */
   if (devinfo->gen >= 9 &&
       brw_isl_format_for_mesa_format(mt->format) !=
       brw->mesa_to_isl_render_format[mt->format])
      return false;

   const mesa_format format = _mesa_get_render_format(ctx, mt->format);
   if (_mesa_is_format_integer_color(format)) {
      if (devinfo->gen >= 8) {
         perf_debug("Integer fast clear not enabled for (%s)",
                    _mesa_get_format_name(format));
      }
      return false;
   }

   for (int i = 0; i < 4; i++) {
      if (!_mesa_format_has_color_component(format, i)) {
         continue;
      }

      if (devinfo->gen < 9 &&
          color->f[i] != 0.0f && color->f[i] != 1.0f) {
         return false;
      }
   }
   return true;
}

/**
 * Convert the given color to a bitfield suitable for ORing into DWORD 7 of
 * SURFACE_STATE (DWORD 12-15 on SKL+).
 */
union isl_color_value
brw_meta_convert_fast_clear_color(const struct brw_context *brw,
                                  const struct intel_mipmap_tree *mt,
                                  const union gl_color_union *color)
{
   union isl_color_value override_color = {
      .u32 = {
         color->ui[0],
         color->ui[1],
         color->ui[2],
         color->ui[3],
      },
   };

   /* The sampler doesn't look at the format of the surface when the fast
    * clear color is used so we need to implement luminance, intensity and
    * missing components manually.
    */
   switch (_mesa_get_format_base_format(mt->format)) {
   case GL_INTENSITY:
      override_color.u32[3] = override_color.u32[0];
      /* flow through */
   case GL_LUMINANCE:
   case GL_LUMINANCE_ALPHA:
      override_color.u32[1] = override_color.u32[0];
      override_color.u32[2] = override_color.u32[0];
      break;
   default:
      for (int i = 0; i < 3; i++) {
         if (!_mesa_format_has_color_component(mt->format, i))
            override_color.u32[i] = 0;
      }
      break;
   }

   switch (_mesa_get_format_datatype(mt->format)) {
   case GL_UNSIGNED_NORMALIZED:
      for (int i = 0; i < 4; i++)
         override_color.f32[i] = CLAMP(override_color.f32[i], 0.0f, 1.0f);
      break;

   case GL_SIGNED_NORMALIZED:
      for (int i = 0; i < 4; i++)
         override_color.f32[i] = CLAMP(override_color.f32[i], -1.0f, 1.0f);
      break;

   case GL_UNSIGNED_INT:
      for (int i = 0; i < 4; i++) {
         unsigned bits = _mesa_get_format_bits(mt->format, GL_RED_BITS + i);
         if (bits < 32) {
            uint32_t max = (1u << bits) - 1;
            override_color.u32[i] = MIN2(override_color.u32[i], max);
         }
      }
      break;

   case GL_INT:
      for (int i = 0; i < 4; i++) {
         unsigned bits = _mesa_get_format_bits(mt->format, GL_RED_BITS + i);
         if (bits < 32) {
            int32_t max = (1 << (bits - 1)) - 1;
            int32_t min = -(1 << (bits - 1));
            override_color.i32[i] = CLAMP(override_color.i32[i], min, max);
         }
      }
      break;

   case GL_FLOAT:
      if (!_mesa_is_format_signed(mt->format)) {
         for (int i = 0; i < 4; i++)
            override_color.f32[i] = MAX2(override_color.f32[i], 0.0f);
      }
      break;
   }

   if (!_mesa_format_has_color_component(mt->format, 3)) {
      if (_mesa_is_format_integer_color(mt->format))
         override_color.u32[3] = 1;
      else
         override_color.f32[3] = 1.0f;
   }

   /* Handle linear to SRGB conversion */
   if (brw->ctx.Color.sRGBEnabled &&
       _mesa_get_srgb_format_linear(mt->format) != mt->format) {
      for (int i = 0; i < 3; i++) {
         override_color.f32[i] =
            util_format_linear_to_srgb_float(override_color.f32[i]);
      }
   }

   return override_color;
}
