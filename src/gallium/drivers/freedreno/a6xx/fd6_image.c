/*
 * Copyright (C) 2017 Rob Clark <robclark@freedesktop.org>
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

#include "pipe/p_state.h"

#include "freedreno_resource.h"
#include "freedreno_state.h"

#include "fd6_emit.h"
#include "fd6_format.h"
#include "fd6_image.h"
#include "fd6_resource.h"
#include "fd6_texture.h"

/* Build combined image/SSBO "IBO" state, returns ownership of state reference */
struct fd_ringbuffer *
fd6_build_ibo_state(struct fd_context *ctx, const struct ir3_shader_variant *v,
                    enum pipe_shader_type shader)
{
   struct fd6_context *fd6_ctx = fd6_context(ctx);
   struct fd_shaderbuf_stateobj *bufso = &ctx->shaderbuf[shader];
   struct fd_shaderimg_stateobj *imgso = &ctx->shaderimg[shader];

   struct fd_ringbuffer *state = fd_submit_new_ringbuffer(
      ctx->batch->submit,
      (v->shader->nir->info.num_ssbos + v->shader->nir->info.num_images) * 16 *
         4,
      FD_RINGBUFFER_STREAMING);

   assert(shader == PIPE_SHADER_COMPUTE || shader == PIPE_SHADER_FRAGMENT);

   for (unsigned i = 0; i < v->shader->nir->info.num_ssbos; i++) {
      fd6_emit_single_plane_descriptor(
         state, bufso->sb[i].buffer,
         fd6_ctx->ssbo_descriptors[shader][i]);
   }

   for (unsigned i = 0; i < v->shader->nir->info.num_images; i++) {
      /* If we ensured that tex happened after ibo, we could skip this check. */
      struct fd_resource *rsc = fd_resource(imgso->si[i].resource);
      if (rsc && unlikely(fd6_ctx->image_seqnos[shader][i] != rsc->seqno))
         fd6_image_update(ctx, shader, i);

      fd6_emit_single_plane_descriptor(
         state, imgso->si[i].resource,
         fd6_ctx->image_views[shader][i].storage_descriptor);
   }

   return state;
}

static void
fd6_set_shader_buffers(struct pipe_context *pctx, enum pipe_shader_type shader,
                       unsigned start, unsigned count,
                       const struct pipe_shader_buffer *buffers,
                       unsigned writable_bitmask) in_dt
{
   struct fd_context *ctx = fd_context(pctx);
   struct fd6_context *fd6_ctx = fd6_context(ctx);

   fd_set_shader_buffers(pctx, shader, start, count, buffers, writable_bitmask);

   if (!buffers)
      return;

   for (unsigned i = 0; i < count; i++) {
      unsigned n = i + start;
      const struct pipe_shader_buffer *buf = &buffers[i];
      uint32_t *descriptor = fd6_ctx->ssbo_descriptors[shader][n];

      if (!buf->buffer)
         continue;

      static const uint8_t swiz[4] = {PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y,
                                      PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W};

      fdl6_buffer_view_init(descriptor,
                            ctx->screen->info->a6xx.storage_16bit
                               ? PIPE_FORMAT_R16_UINT
                               : PIPE_FORMAT_R32_UINT,
                            swiz,
                            buf->buffer_offset, /* Using relocs for addresses */
                            buf->buffer_size);
   }
}

void
fd6_image_update(struct fd_context *ctx, enum pipe_shader_type shader,
                 unsigned i)
{
   struct fd6_context *fd6_ctx = fd6_context(ctx);
   struct fd_shaderimg_stateobj *so = &ctx->shaderimg[shader];
   struct pipe_image_view *buf = &so->si[i];
   struct fd_resource *rsc = fd_resource(buf->resource);
   struct fdl6_view *view = &fd6_ctx->image_views[shader][i];

   if (buf->resource->target == PIPE_BUFFER) {
      static const uint8_t swiz[4] = {PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y,
                                       PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W};
      fdl6_buffer_view_init(view->descriptor,
                        buf->format,
                        swiz,
                        buf->u.buf.offset, /* Using relocs for addresses */
                        buf->u.buf.size);

      /* buffer descriptor is the same for TEX and IBO */
      memcpy(view->storage_descriptor, view->descriptor,
               sizeof(view->descriptor));
   } else {
      struct fdl_view_args args = {
         /* Using relocs for addresses */
         .iova = 0,

         .base_miplevel = buf->u.tex.level,
         .level_count = 1,

         .base_array_layer = buf->u.tex.first_layer,
         .layer_count = buf->u.tex.last_layer - buf->u.tex.first_layer + 1,

         .format = buf->format,
         .swiz = {PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z,
                  PIPE_SWIZZLE_W},

         .type = fdl_type_from_pipe_target(buf->resource->target),
         .chroma_offsets = {FDL_CHROMA_LOCATION_COSITED_EVEN,
                           FDL_CHROMA_LOCATION_COSITED_EVEN},
      };

      /* fdl6_view makes the storage descriptor treat cubes like a 2D array (so
       * you can reference a specific layer), but we need to do that for the
       * texture descriptor as well to get our layer.
       */
      if (args.type == FDL_VIEW_TYPE_CUBE)
         args.type = FDL_VIEW_TYPE_2D;

      const struct fdl_layout *layouts[3] = {&rsc->layout, NULL, NULL};
      fdl6_view_init(view, layouts, &args,
                     ctx->screen->info->a6xx.has_z24uint_s8uint);
   }

   fd6_ctx->image_seqnos[shader][i] = rsc->seqno;
}

static void
fd6_set_shader_images(struct pipe_context *pctx, enum pipe_shader_type shader,
                      unsigned start, unsigned count,
                      unsigned unbind_num_trailing_slots,
                      const struct pipe_image_view *images) in_dt
{
   struct fd_context *ctx = fd_context(pctx);
   struct fd_shaderimg_stateobj *so = &ctx->shaderimg[shader];

   fd_set_shader_images(pctx, shader, start, count, unbind_num_trailing_slots,
                        images);

   if (!images)
      return;

   for (unsigned i = 0; i < count; i++) {
      unsigned n = i + start;
      struct pipe_image_view *buf = &so->si[n];

      if (!buf->resource)
         continue;

      struct fd_resource *rsc = fd_resource(buf->resource);
      fd6_validate_format(ctx, rsc, buf->format);

      fd6_image_update(ctx, shader, n);
   }
}

void
fd6_image_init(struct pipe_context *pctx)
{
   pctx->set_shader_buffers = fd6_set_shader_buffers;
   pctx->set_shader_images = fd6_set_shader_images;
}
