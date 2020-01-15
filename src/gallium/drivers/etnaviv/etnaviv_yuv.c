/*
 * Copyright 2024 Igalia, S.L.
 * SPDX-License-Identifier: MIT
 */

#include "etnaviv_context.h"
#include "etnaviv_emit.h"
#include "etnaviv_yuv.h"
#include "hw/state_3d.xml.h"
#include "hw/state_blt.xml.h"
#include "util/format/u_format.h"
#include "util/macros.h"

struct etna_yuv_config {
   struct etna_resource *planes[3];
   struct etna_resource *dst;
   uint32_t format;
   uint32_t width;
   uint32_t height;
};

static void
emit_plane(struct etna_context *ctx, struct etna_resource *plane,
           enum etna_resource_status status, uint32_t base, uint32_t stride)
{
   if (!plane)
      return;

   etna_resource_used(ctx, &plane->base, status);
   etna_set_state_reloc(ctx->stream, base, &(struct etna_reloc) {
      .bo = plane->bo,
      .offset = plane->levels[0].offset,
      .flags = ETNA_RELOC_READ,
   });
   etna_set_state(ctx->stream, stride, plane->levels[0].stride);
}

static void
emit_blt(struct etna_context *ctx, struct etna_yuv_config *config)
{
   struct etna_cmd_stream *stream = ctx->stream;

   etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
   etna_set_state(stream, VIVS_BLT_YUV_CONFIG,
                  VIVS_BLT_YUV_CONFIG_SOURCE_FORMAT(config->format) | VIVS_BLT_YUV_CONFIG_ENABLE);
   etna_set_state(stream, VIVS_BLT_YUV_WINDOW_SIZE,
                  VIVS_BLT_YUV_WINDOW_SIZE_HEIGHT(config->height) |
                  VIVS_BLT_YUV_WINDOW_SIZE_WIDTH(config->width));

   emit_plane(ctx, config->planes[0], ETNA_PENDING_READ, VIVS_BLT_YUV_SRC_YADDR, VIVS_BLT_YUV_SRC_YSTRIDE);
   emit_plane(ctx, config->planes[1], ETNA_PENDING_READ, VIVS_BLT_YUV_SRC_UADDR, VIVS_BLT_YUV_SRC_USTRIDE);
   emit_plane(ctx, config->planes[2], ETNA_PENDING_READ, VIVS_BLT_YUV_SRC_VADDR, VIVS_BLT_YUV_SRC_VSTRIDE);
   emit_plane(ctx, config->dst, ETNA_PENDING_WRITE, VIVS_BLT_YUV_DEST_ADDR, VIVS_BLT_YUV_DEST_STRIDE);

   /* trigger resolve */
   etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
   etna_set_state(stream, VIVS_BLT_COMMAND, VIVS_BLT_COMMAND_COMMAND_YUV_TILE);
   etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
   etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
}

static void
emit_rs(struct etna_context *ctx, struct etna_yuv_config *config)
{
   struct etna_cmd_stream *stream = ctx->stream;

   etna_set_state(stream, VIVS_YUV_CONFIG,
                  VIVS_YUV_CONFIG_SOURCE_FORMAT(config->format) | VIVS_YUV_CONFIG_ENABLE);
   etna_set_state(stream, VIVS_YUV_WINDOW_SIZE,
                  VIVS_YUV_WINDOW_SIZE_HEIGHT(config->height) |
                  VIVS_YUV_WINDOW_SIZE_WIDTH(config->width));

   emit_plane(ctx, config->planes[0], ETNA_PENDING_READ, VIVS_YUV_Y_BASE, VIVS_YUV_Y_STRIDE);
   emit_plane(ctx, config->planes[1], ETNA_PENDING_READ, VIVS_YUV_U_BASE, VIVS_YUV_U_STRIDE);
   emit_plane(ctx, config->planes[2], ETNA_PENDING_READ, VIVS_YUV_V_BASE, VIVS_YUV_V_STRIDE);
   emit_plane(ctx, config->dst, ETNA_PENDING_WRITE, VIVS_YUV_DEST_BASE, VIVS_YUV_DEST_STRIDE);

   /* configure RS */
   etna_set_state(stream, VIVS_RS_SOURCE_STRIDE, 0);
   etna_set_state(stream, VIVS_RS_CLEAR_CONTROL, 0);

   /* trigger resolve */
   etna_set_state(stream,  VIVS_RS_KICKER, 0xbadabeeb);

   /* disable yuv tiller */
   etna_set_state(stream, VIVS_YUV_CONFIG, 0x0);
}

bool
etna_try_yuv_blit(struct pipe_context *pctx,
                  const struct pipe_blit_info *blit_info)
{
   struct etna_context *ctx = etna_context(pctx);
   struct etna_screen *screen = ctx->screen;
   struct etna_cmd_stream *stream = ctx->stream;
   struct pipe_resource *src = blit_info->src.resource;
   struct etna_yuv_config config = { 0 };
   ASSERTED unsigned num_planes;
   int idx = 0;

   assert(util_format_is_yuv(blit_info->src.format));
   assert(blit_info->dst.format == PIPE_FORMAT_YUYV);
   assert(blit_info->src.level == 0);
   assert(blit_info->dst.level == 0);

   config.dst = etna_resource(blit_info->dst.resource);
   config.height = blit_info->dst.box.height;
   config.width = blit_info->dst.box.width;

   switch (blit_info->src.format) {
   case PIPE_FORMAT_NV12:
      config.format = 0x1;
      num_planes = 2;
      break;
   default:
      return false;
   }

   while (src) {
      config.planes[idx++] = etna_resource(src);
      src = src->next;
   }

   assert(idx == num_planes);

   etna_set_state(stream, VIVS_GL_FLUSH_CACHE,
                  VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
   etna_stall(stream, SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

   etna_set_state(stream, VIVS_TS_FLUSH_CACHE, VIVS_TS_FLUSH_CACHE_FLUSH);
   etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0);

   if (screen->specs.use_blt)
      emit_blt(ctx, &config);
   else
      emit_rs(ctx, &config);

   ctx->dirty |= ETNA_DIRTY_TS;

   return true;
}
