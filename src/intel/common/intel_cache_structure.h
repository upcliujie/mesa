/*
 * Copyright (c) 2021 Intel Corporation
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

#ifndef INTEL_CACHE_STRUCTURE_H
#define INTEL_CACHE_STRUCTURE_H

#include <stdint.h>

#include "util/macros.h"

#ifdef __cplusplus
extern "C" {
#endif

enum intel_hw_cache_unit {
   /* Front end of the 3d pipeline */
   INTEL_HW_CACHE_UNIT_VF             = BITFIELD64_BIT(0),

   /* Depth access */
   INTEL_HW_CACHE_UNIT_DEPTH          = BITFIELD64_BIT(1),

   /* Constant, instructions access */
   INTEL_HW_CACHE_UNIT_CONSTANT       = BITFIELD64_BIT(2),

   /* Data access (SSBO, ...) */
   INTEL_HW_CACHE_UNIT_DATA           = BITFIELD64_BIT(3),

   /* Texture sampling, etc... */
   INTEL_HW_CACHE_UNIT_TEXTURE        = BITFIELD64_BIT(4),

   /* Output of the 3d pipeline */
   INTEL_HW_CACHE_UNIT_RENDERTARGET   = BITFIELD64_BIT(5),

   /* L3 cache */
   INTEL_HW_CACHE_UNIT_L3             = BITFIELD64_BIT(6),

   /* Main memory */
   INTEL_HW_CACHE_UNIT_MAIN_MEMORY    = BITFIELD64_BIT(7),

   /* Command streamer */
   INTEL_HW_CACHE_UNIT_CS             = BITFIELD64_BIT(8),

   /* CPU */
   INTEL_HW_CACHE_UNIT_CPU            = BITFIELD64_BIT(9),
};

/* Bitfield of pipe control bits */
enum intel_pipe_control_bits {
   INTEL_PIPE_CONTROL_VF_CACHE_INVALIDATE    = BITFIELD64_BIT(0),
   INTEL_PIPE_CONTROL_TEX_CACHE_INVALIDATE   = BITFIELD64_BIT(1),
   INTEL_PIPE_CONTROL_CONST_CACHE_INVALIDATE = BITFIELD64_BIT(2),

   INTEL_PIPE_CONTROL_CS_STALL               = BITFIELD64_BIT(3),

   INTEL_PIPE_CONTROL_DEPTH_CACHE_FLUSH      = BITFIELD64_BIT(4),
   INTEL_PIPE_CONTROL_DATA_CACHE_FLUSH       = BITFIELD64_BIT(5),
   INTEL_PIPE_CONTROL_HDC_CACHE_FLUSH        = BITFIELD64_BIT(6),
   INTEL_PIPE_CONTROL_TILE_CACHE_FLUSH       = BITFIELD64_BIT(7),
   INTEL_PIPE_CONTROL_RT_CACHE_FLUSH         = BITFIELD64_BIT(8),
   INTEL_PIPE_CONTROL_UNTYPED_DATA_FLUSH     = BITFIELD64_BIT(9),
};

struct intel_device_info;
struct intel_cache_hierarchy;

const struct intel_cache_hierarchy *
intel_cache_hierarchy_get_for_device(const struct intel_device_info *devinfo);

/* Return a bitfield of pipe control bits for flushing cache from one unit to
 * another and having the destination unit clearing the appropriate caches to
 * read the newly available data.
 */
enum intel_pipe_control_bits
intel_cache_pipe_control_bits_for(const struct intel_cache_hierarchy *cache,
                                  enum intel_hw_cache_unit dst_hw_units,
                                  enum intel_hw_cache_unit src_hw_units);

#ifdef __cplusplus
}
#endif

#endif /* INTEL_CACHE_STRUCTURE_H */
