/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef DZN_NIR_H
#define DZN_NIR_H

#include "nir.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dzn_indirect_draw_params {
   uint32_t vertex_count;
   uint32_t instance_count;
   uint32_t first_vertex;
   uint32_t first_instance;
};

struct dzn_indirect_indexed_draw_params {
   uint32_t index_count;
   uint32_t instance_count;
   uint32_t first_index;
   int32_t vertex_offset;
   uint32_t first_instance;
};

struct dzn_indirect_draw_exec_params {
   struct {
      uint32_t first_vertex;
      uint32_t base_instance;
   } sysvals;
   union {
      struct dzn_indirect_draw_params draw;
      struct dzn_indirect_indexed_draw_params indexed_draw;
   };
};

enum dzn_indirect_draw_type {
   DZN_INDIRECT_DRAW,
   DZN_INDIRECT_INDEXED_DRAW,
   DZN_NUM_INDIRECT_DRAW_TYPES,
};

struct nir_shader *
dzn_nir_indirect_draw_shader(enum dzn_indirect_draw_type type);

#ifdef __cplusplus
}
#endif

#endif
