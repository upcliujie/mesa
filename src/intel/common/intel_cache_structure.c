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

/**
 * This helper is here to help a driver find what flushes & invalidations
 * should be applied to make a data used by a set of HW units visible to
 * another set of HW units. The 2 sets can overlap.
 */

#include "util/bitscan.h"
#include "util/macros.h"

#include "dev/intel_device_info.h"

#include "intel_cache_structure.h"

/* We can see the cache structure as a list of imbricated boxes with a single
 * parent-child relationship.
 */
struct intel_block {
   /* Name of the current unit */
   enum intel_hw_cache_unit       unit;
   /* Parent unit */
   const struct intel_block      *parent;
   /* Flushes associated with the current unit */
   enum intel_pipe_control_bits   flush;
   /* For some HW units (like DATA, whether to flush is conditional the upper
    * level HDC)
    */
   enum intel_pipe_control_bits (*flush_func)(enum intel_pipe_control_bits higher_flushes);

   /* Invalidates associated with the current unit */
   enum intel_pipe_control_bits   inval;
};

/* To access all the blocks of a given generation. */
struct intel_cache_hierarchy {
   const struct intel_block *(*get_block)(enum intel_hw_cache_unit unit);
};

#define BLOCK(_name, _unit, _flush, _inval, _parent)            \
   const struct intel_block P(_name) = {                        \
      .unit   = INTEL_HW_CACHE_UNIT_##_unit,                    \
      .parent = _parent,                                        \
      .flush  = INTEL_PIPE_CONTROL_##_flush,                    \
      .inval  = INTEL_PIPE_CONTROL_##_inval,                    \
   }
#define BLOCKF(_name, _unit, _flush, _inval, _parent, func)         \
   const struct intel_block P(_name) = {                            \
      .unit       = INTEL_HW_CACHE_UNIT_##_unit,                    \
      .parent     = _parent,                                        \
      .flush      = INTEL_PIPE_CONTROL_##_flush,                    \
      .inval      = INTEL_PIPE_CONTROL_##_inval,                    \
      .flush_func = func,                                           \
   }

#define PLATFORM_CACHE()                                                \
   static const struct intel_block *                                    \
   P(get_block)(enum intel_hw_cache_unit unit)                          \
   {                                                                    \
      switch (unit) {                                                   \
      case INTEL_HW_CACHE_UNIT_VF:                                      \
         return &P(vf_unit);                                            \
      case INTEL_HW_CACHE_UNIT_DEPTH:                                   \
         return &P(depth_unit);                                         \
      case INTEL_HW_CACHE_UNIT_CONSTANT:                                \
         return &P(const_unit);                                         \
      case INTEL_HW_CACHE_UNIT_DATA:                                    \
         return &P(data_unit);                                          \
      case INTEL_HW_CACHE_UNIT_TEXTURE:                                 \
         return &P(tex_unit);                                           \
      case INTEL_HW_CACHE_UNIT_RENDERTARGET:                            \
         return &P(rt_unit);                                            \
      case INTEL_HW_CACHE_UNIT_MAIN_MEMORY:                             \
         return &P(main_memory);                                        \
      case INTEL_HW_CACHE_UNIT_L3:                                      \
         return &P(l3_unit);                                            \
      case INTEL_HW_CACHE_UNIT_CS:                                      \
         return &P(cs_unit);                                            \
      case INTEL_HW_CACHE_UNIT_CPU:                                     \
         return &P(cpu_unit);                                           \
      default:                                                          \
         unreachable("invalid unit");                                   \
      }                                                                 \
   }                                                                    \
                                                                        \
   static const struct intel_cache_hierarchy                            \
   P(cache_hierarchy) = {                                               \
      .get_block = P(get_block),                                        \
   };


#define INTEL_PIPE_CONTROL_NONE (0)

/* Gfx8 cache hierarchy, each section gets its dedicated L3 portion in
 * addition to a local L1/L2 :
 *
 * --------------------------------------  ------ ------           ----
 * | RT | Depth | Tex/Const/Inst | Data |  | VF | | CS |              |  L1/L2 cache
 * |------------------------------------|  |    | |    | -------   ----
 * |                 L3                 |  |    | |    | | CPU |      |  L3 cache
 * |------------------------------------------------------------   ----
 * |                      Main memory                          |
 * -------------------------------------------------------------
 */

#define P(_name) gfx8_##_name

/*               unit,      enum unit,             flush,                  inval,    parent */
BLOCK(    main_memory,    MAIN_MEMORY,              NONE,                   NONE,            NULL);
BLOCK(        l3_unit,             L3,              NONE,                   NONE, &P(main_memory));
BLOCK(       cpu_unit,            CPU,              NONE,                   NONE, &P(main_memory));
BLOCK(        vf_unit,             VF,              NONE,    VF_CACHE_INVALIDATE, &P(main_memory));
BLOCK(        cs_unit,             CS,              NONE,               CS_STALL, &P(main_memory));
BLOCK(       tex_unit,        TEXTURE,              NONE,   TEX_CACHE_INVALIDATE,    &P(l3_unit));
BLOCK(     depth_unit,          DEPTH, DEPTH_CACHE_FLUSH,                   NONE,    &P(l3_unit));
BLOCK(        rt_unit,   RENDERTARGET,    RT_CACHE_FLUSH,                   NONE,    &P(l3_unit));
BLOCK(      data_unit,           DATA,  DATA_CACHE_FLUSH,                   NONE,    &P(l3_unit));
BLOCK(     const_unit,       CONSTANT,              NONE, CONST_CACHE_INVALIDATE,    &P(l3_unit));

PLATFORM_CACHE();

#undef P

/* Gfx12 cache hierarchy, L3 is now divided in 3 sections Tile, ReadOnly,
 * Data :
 *    - Tile contains color & depth data
 *    - ReadOnly is constant loads, textures & instructions
 *    - Data is read/writes from things like SSBOs
 *
 * On the Data portion of L3, a new Hdc unit is available to flush the L1 down
 * to main memory, making the L3 Data flushes mostly irrelevant.
 *
 * -----------------------------------------  ------           ----
 * | RT | Depth | Tex/Const/Inst |   Hdc   |  | VF |              |  L1/L2 cache
 * |------------|----------------|---------|  |    | -------   ----
 * |  Tile L3   |  ReadOnly L3   | Data L3 |  |    | | CPU |      |  L3 cache
 * |--------------------------------------------------------   ----
 * |                      Main memory                      |
 * ---------------------------------------------------------
 */

#define P(_name) gfx12_##_name

static enum intel_pipe_control_bits
P(flush_l3)(enum intel_pipe_control_bits higher_flushes)
{
   enum intel_pipe_control_bits result = 0;
   /* Flush L3 depend on what you want to flush from above. To flush :
    *    - depth or color, use TILE flush
    *    - to flush data, use DC flush
    */
   if (higher_flushes & (INTEL_PIPE_CONTROL_DEPTH_CACHE_FLUSH |
                         INTEL_PIPE_CONTROL_RT_CACHE_FLUSH))
      result |= INTEL_PIPE_CONTROL_TILE_CACHE_FLUSH;
   if (higher_flushes & INTEL_PIPE_CONTROL_HDC_CACHE_FLUSH)
      result |= INTEL_PIPE_CONTROL_DATA_CACHE_FLUSH |
                INTEL_PIPE_CONTROL_UNTYPED_DATA_FLUSH;
   return result;
}

/*               unit,      enum unit,             flush,                  inval,    parent */
BLOCK(    main_memory,    MAIN_MEMORY,              NONE,                   NONE,            NULL);
BLOCK(       cpu_unit,            CPU,              NONE,                   NONE, &P(main_memory));
BLOCK(        cs_unit,             CS,              NONE,               CS_STALL, &P(main_memory));
BLOCKF(       l3_unit,             L3,              NONE,                   NONE, &P(main_memory), P(flush_l3));
BLOCK(        vf_unit,             VF,              NONE,    VF_CACHE_INVALIDATE,     &P(l3_unit));
BLOCK(       tex_unit,        TEXTURE,              NONE,   TEX_CACHE_INVALIDATE,     &P(l3_unit));
BLOCK(     depth_unit,          DEPTH, DEPTH_CACHE_FLUSH,                   NONE,     &P(l3_unit));
BLOCK(        rt_unit,   RENDERTARGET,    RT_CACHE_FLUSH,                   NONE,     &P(l3_unit));
BLOCK(      data_unit,           DATA,   HDC_CACHE_FLUSH,                   NONE,     &P(l3_unit));
BLOCK(     const_unit,       CONSTANT,              NONE, CONST_CACHE_INVALIDATE,     &P(l3_unit));

PLATFORM_CACHE();

#undef P

static uint64_t
get_block_flush(const struct intel_block *block,
                enum intel_pipe_control_bits higher_flushes)
{
   if (!block->flush_func)
      return block->flush;

   return block->flush_func(higher_flushes);
}

static uint64_t
block_bits(const struct intel_block *block)
{
   uint64_t bits = 0;

   while (block) {
      bits |= block->unit;
      block = block->parent;
   }

   return bits;
}

static const struct intel_block *
get_common_parent_block(const struct intel_cache_hierarchy *cache,
                        const struct intel_block *b1,
                        const struct intel_block *b2)
{
   if (!b1) {
      assert(b2);
      return b2;
   }
   if (!b2) {
      assert(b1);
      return b1;
   }

   uint64_t b1_parent_bits = block_bits(b1);
   uint64_t b2_parent_bits = b2->unit;
   while ((b1_parent_bits & b2_parent_bits) == 0) {
      b2_parent_bits |= b2->unit;
      b2 = b2->parent;
   }

   uint64_t match_bit = b1_parent_bits & b2_parent_bits;
   assert(util_bitcount64(match_bit) == 1);
   return cache->get_block(match_bit);
}

#define DEBUG_CACHE 0

static const char *unit_name(enum intel_hw_cache_unit unit)
{
   switch (unit) {
#define CASE(_name) case INTEL_HW_CACHE_UNIT_##_name: return #_name
      CASE(VF);
      CASE(DEPTH);
      CASE(CONSTANT);
      CASE(DATA);
      CASE(TEXTURE);
      CASE(RENDERTARGET);
      CASE(L3);
      CASE(MAIN_MEMORY);
      CASE(CS);
      CASE(CPU);
   default: unreachable("invalid unit");
#undef CASE
   }
}

void
intel_cache_print(enum intel_hw_cache_unit dst_hw_units,
                  enum intel_hw_cache_unit src_hw_units)
{
   fprintf(stderr, "flushing from: ");
   enum intel_hw_cache_unit units = src_hw_units;
   while (units) {
      int idx = u_bit_scan(&units);
      fprintf(stderr, "%s,", unit_name(1 << idx));
   }
   fprintf(stderr, " to: ");
   units = dst_hw_units;
   while (units) {
      int idx = u_bit_scan(&units);
      fprintf(stderr, "%s,", unit_name(1 << idx));
      }
   fprintf(stderr, "\n");
}

enum intel_pipe_control_bits
intel_cache_pipe_control_bits_for(const struct intel_cache_hierarchy *cache,
                                  enum intel_hw_cache_unit dst_hw_units,
                                  enum intel_hw_cache_unit src_hw_units)
{
   /* No synchronization required */
   if (src_hw_units == 0 || dst_hw_units == 0)
      return 0;

#if DEBUG_CACHE
   intel_cache_print(dst_hw_units, src_hw_units);
#endif

   /* Applications can request that the usage from multiple HW units be made
    * available to another set of HW units.
    *
    * Rather than computing a M * N (M = numberOf(src_hw_units), N =
    * numberOf(dst_hw_units)) problem, we start by finding the common block
    * for the M source units and the common block for the N destination units.
    * We then use those 2 blocks (CS, DS) to find the common block (CB) and
    * compute the flushes from sources down to common block and invalidates
    * from destinations down to common block.
    *
    *     S1 S2 S3   (src_hw_units)
    *      \ | /
    *       CS       (src_common_block)
    *        |
    *       CB       (common_block)
    *        |
    *       CD       (dst_common_block)
    *      / | \
    *     D1 D2 D3   (dst_hw_units)
    */
   const struct intel_block *src_common_block = NULL;
   u_foreach_bit64(src_hw_bit, src_hw_units) {
      const struct intel_block *src_block =
         cache->get_block(BITFIELD64_BIT(src_hw_bit));
      src_common_block =
         get_common_parent_block(cache, src_block, src_common_block);
   }

   const struct intel_block *dst_common_block = NULL;
   u_foreach_bit64(dst_hw_bit, dst_hw_units) {
      const struct intel_block *dst_block =
         cache->get_block(BITFIELD64_BIT(dst_hw_bit));
      dst_common_block =
         get_common_parent_block(cache, dst_block, dst_common_block);
   }

   const struct intel_block *common_block =
      get_common_parent_block(cache, src_common_block, dst_common_block);

   enum intel_pipe_control_bits result = 0;

   /* Compute the flushes by collecting the flush flags for all the source
    * units down to the common unit.
    */
   u_foreach_bit64(src_hw_bit, src_hw_units) {
      const struct intel_block *src_block =
         cache->get_block(BITFIELD64_BIT(src_hw_bit));
      const struct intel_block *iter = src_block;
      while (iter != common_block) {
         result |= get_block_flush(iter, result);
         iter = iter->parent;
      }
   }

   /* Compute the invalidations by collecting the inval flags for all
    * destination units down to the common unit.
    */
   u_foreach_bit64(dst_hw_bit, dst_hw_units) {
      const struct intel_block *dst_block =
         cache->get_block(BITFIELD64_BIT(dst_hw_bit));
      const struct intel_block *iter = dst_block;
      while (iter != common_block) {
         result |= iter->inval;
         iter = iter->parent;
      }
   }

   return result;
}

const struct intel_cache_hierarchy *
intel_cache_hierarchy_get_for_device(const struct intel_device_info *devinfo)
{
   if (devinfo->ver >= 12)
      return &gfx12_cache_hierarchy;

   return &gfx8_cache_hierarchy;
}
