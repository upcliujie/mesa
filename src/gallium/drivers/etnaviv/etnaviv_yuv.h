/*
 * Copyright 2024 Igalia, S.L.
 * SPDX-License-Identifier: MIT
 */

#ifndef H_ETNA_YUV
#define H_ETNA_YUV

#include <stdbool.h>

#include "util/format/u_formats.h"

struct pipe_context;
struct pipe_blit_info;

static inline bool
etna_format_needs_yuv_tiler(enum pipe_format format)
{
   return format == PIPE_FORMAT_NV12;
}

bool
etna_try_yuv_blit(struct pipe_context *pctx,
                  const struct pipe_blit_info *blit_info);

#endif
