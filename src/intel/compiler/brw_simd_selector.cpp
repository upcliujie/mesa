/*
 * Copyright © 2021 Intel Corporation
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

#include "brw_simd_selector.h"

#include "compiler/shader_info.h"
#include "intel/dev/intel_debug.h"
#include "intel/dev/intel_device_info.h"
#include "util/ralloc.h"

using namespace brw;

simd_selector::simd_selector(void *mem_ctx,
                             const struct intel_device_info *devinfo,
                             const struct shader_info *info,
                             enum brw_subgroup_size_type subgroup_size_type)
   : mem_ctx(mem_ctx),
     devinfo(devinfo),
     info(info),
     required(0),
     next_simd(0)
{
   for (unsigned i = 0; i < ARRAY_SIZE(pass); i++)
      should[i] = pass[i] = spill[i] = false;

   if ((int)subgroup_size_type >= (int)BRW_SUBGROUP_SIZE_REQUIRE_8) {
      assert(gl_shader_stage_uses_workgroup(info->stage));
      /* These enum values are expressly chosen to be equal to the subgroup
       * size that they require.
       */
      required = (unsigned)subgroup_size_type;
   }

   if (gl_shader_stage_is_compute(info->stage) && info->cs.subgroup_size > 0) {
      assert(required == 0 || required == info->cs.subgroup_size);
      required = info->cs.subgroup_size;
   }
}

bool
simd_selector::should_compile(unsigned simd)
{
   assert(!pass[simd]);
   assert(next_simd == simd);

   next_simd++;

   const unsigned width = 8 << simd;

   /* For shaders with variable size workgroup, we will always compile all the
    * variants, since the choice will happen only at dispatch time.
    */

   if (!info->workgroup_size_variable) {
      unsigned workgroup_size = info->workgroup_size[0] *
                                info->workgroup_size[1] *
                                info->workgroup_size[2];

      /* TODO: Handle other stages. */
      assert(gl_shader_stage_uses_workgroup(info->stage));
      unsigned max_threads = devinfo->max_cs_workgroup_threads;

      if (spill[simd]) {
         error[simd] = ralloc_asprintf(
            mem_ctx, "SIMD%d skipped because would spill", width);
         return false;
      }

      if (required && required != width) {
         error[simd] = ralloc_asprintf(
            mem_ctx, "SIMD%d skipped because required dispatch width is %d",
            width, required);
         return false;
      }

      /* TODO: Ignore SIMD larger than workgroup if previous SIMD already passed. */

      if (DIV_ROUND_UP(workgroup_size, width) > max_threads) {
         error[simd] = ralloc_asprintf(
            mem_ctx, "SIMD%d can't fit all %d invocations in %d threads",
            width, workgroup_size, max_threads);
         return false;
      }

      /* The SIMD32 is only enabled for cases it is needed unless forced.
       *
       * TODO: Use performance_analysis and drop this rule.
       */
      if (width == 32) {
         if (!(INTEL_DEBUG & DEBUG_DO32) && (pass[0] || pass[1])) {
            error[simd] = ralloc_strdup(
               mem_ctx, "SIMD32 skipped because not required");
            return false;
         }
      }
   }

   static const bool env_skip[3] = {
      (INTEL_DEBUG & DEBUG_NO8)  != 0,
      (INTEL_DEBUG & DEBUG_NO16) != 0,
      (INTEL_DEBUG & DEBUG_NO32) != 0,
   };

   if (unlikely(env_skip[simd])) {
      error[simd] = ralloc_asprintf(
         mem_ctx, "SIMD%d skipped because INTEL_DEBUG=no%d",
         width, width);
      return false;
   }

   should[simd] = true;
   return true;
}

void
simd_selector::passed(unsigned simd, bool spilled)
{
   assert(next_simd == simd + 1);
   assert(should[simd]);
   assert(!pass[simd]);

   pass[simd] = true;

   /* If a SIMD spilled, all the larger ones would spill too. */
   for (unsigned i = simd; i < ARRAY_SIZE(spill); i++)
      spill[i] = spilled;
}

int
simd_selector::result()
{
   assert(next_simd == ARRAY_SIZE(pass));
   int r = -1;
   if (pass[0] || pass[1] || pass[2]) {
      /* Pick the largest one that doesn't spill, unless there's only one. */
      for (unsigned i = 0; i < ARRAY_SIZE(pass); i++) {
         if (pass[i] && (r == -1 || !spill[i])) {
            assert(should[i]);
            r = i;
         }
      }
   }
   return r;
}
