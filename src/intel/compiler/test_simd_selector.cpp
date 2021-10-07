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

#include <gtest/gtest.h>

using namespace brw;

enum {
   SIMD8  = 0,
   SIMD16 = 1,
   SIMD32 = 2,
};

const bool spilled = true;
const bool not_spilled = false;

class SIMDSelectorTest : public ::testing::Test {
protected:
   SIMDSelectorTest() {
      mem_ctx = ralloc_context(NULL);
      devinfo = rzalloc(mem_ctx, intel_device_info);
      prog_data = rzalloc(mem_ctx, struct brw_cs_prog_data);
      required_dispatch_width = 0;
   }

   ~SIMDSelectorTest() {
      ralloc_free(mem_ctx);
   };

   bool should_compile(unsigned simd) {
      return brw_simd_should_compile(mem_ctx, simd, devinfo, prog_data,
                                     required_dispatch_width, &error[simd]);
   }

   void *mem_ctx;
   intel_device_info *devinfo;
   struct brw_cs_prog_data *prog_data;
   const char *error[3];
   unsigned required_dispatch_width;
};

class SIMDSelectorCS : public SIMDSelectorTest {
protected:
   SIMDSelectorCS() {
      prog_data->base.stage = MESA_SHADER_COMPUTE;
      prog_data->local_size[0] = 32;
      prog_data->local_size[1] = 1;
      prog_data->local_size[2] = 1;

      devinfo->max_cs_workgroup_threads = 64;
   }
};

TEST_F(SIMDSelectorCS, DefaultsToSIMD16)
{
   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, not_spilled);
   ASSERT_TRUE(should_compile(SIMD16));
   brw_simd_mark_compiled(SIMD16, prog_data, not_spilled);
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), SIMD16);
}

TEST_F(SIMDSelectorCS, TooBigFor16)
{
   prog_data->local_size[0] =  devinfo->max_cs_workgroup_threads;
   prog_data->local_size[1] = 32;
   prog_data->local_size[2] = 1;

   ASSERT_FALSE(should_compile(SIMD8));
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_TRUE(should_compile(SIMD32));
   brw_simd_mark_compiled(SIMD32, prog_data, spilled);

   ASSERT_EQ(brw_simd_select(prog_data), SIMD32);
}

TEST_F(SIMDSelectorCS, WorkgroupSize1)
{
   prog_data->local_size[0] = 1;
   prog_data->local_size[1] = 1;
   prog_data->local_size[2] = 1;

   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, not_spilled);
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), SIMD8);
}

TEST_F(SIMDSelectorCS, WorkgroupSize8)
{
   prog_data->local_size[0] = 8;
   prog_data->local_size[1] = 1;
   prog_data->local_size[2] = 1;

   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, not_spilled);
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), SIMD8);
}

TEST_F(SIMDSelectorCS, WorkgroupSizeVariable)
{
   prog_data->local_size[0] = 0;
   prog_data->local_size[1] = 0;
   prog_data->local_size[2] = 0;

   /* Just ensure that we should compile all the shader variants, since the
    * actual selection will happen later at dispatch time.
    */

   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, spilled);
   ASSERT_TRUE(should_compile(SIMD16));
   brw_simd_mark_compiled(SIMD16, prog_data, spilled);
   ASSERT_TRUE(should_compile(SIMD32));
   brw_simd_mark_compiled(SIMD32, prog_data, spilled);
}

TEST_F(SIMDSelectorCS, SpillAtSIMD8)
{
   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, spilled);
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), SIMD8);
}

TEST_F(SIMDSelectorCS, SpillAtSIMD16)
{
   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, not_spilled);
   ASSERT_TRUE(should_compile(SIMD16));
   brw_simd_mark_compiled(SIMD16, prog_data, spilled);
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), SIMD8);
}

TEST_F(SIMDSelectorCS, EnvironmentVariable32)
{
   intel_debug |= DEBUG_DO32;

   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, not_spilled);
   ASSERT_TRUE(should_compile(SIMD16));
   brw_simd_mark_compiled(SIMD16, prog_data, not_spilled);
   ASSERT_TRUE(should_compile(SIMD32));
   brw_simd_mark_compiled(SIMD32, prog_data, not_spilled);

   ASSERT_EQ(brw_simd_select(prog_data), SIMD32);
}

TEST_F(SIMDSelectorCS, EnvironmentVariable32ButSpills)
{
   intel_debug |= DEBUG_DO32;

   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, not_spilled);
   ASSERT_TRUE(should_compile(SIMD16));
   brw_simd_mark_compiled(SIMD16, prog_data, not_spilled);
   ASSERT_TRUE(should_compile(SIMD32));
   brw_simd_mark_compiled(SIMD32, prog_data, spilled);

   ASSERT_EQ(brw_simd_select(prog_data), SIMD16);
}

TEST_F(SIMDSelectorCS, Require8)
{
   required_dispatch_width = 8;

   ASSERT_TRUE(should_compile(SIMD8));
   brw_simd_mark_compiled(SIMD8, prog_data, not_spilled);
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), SIMD8);
}

TEST_F(SIMDSelectorCS, Require8ErrorWhenNotCompile)
{
   required_dispatch_width = 8;

   ASSERT_TRUE(should_compile(SIMD8));
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), -1);
}

TEST_F(SIMDSelectorCS, Require16)
{
   required_dispatch_width = 16;

   ASSERT_FALSE(should_compile(SIMD8));
   ASSERT_TRUE(should_compile(SIMD16));
   brw_simd_mark_compiled(SIMD16, prog_data, not_spilled);
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), SIMD16);
}

TEST_F(SIMDSelectorCS, Require16ErrorWhenNotCompile)
{
   required_dispatch_width = 16;

   ASSERT_FALSE(should_compile(SIMD8));
   ASSERT_TRUE(should_compile(SIMD16));
   ASSERT_FALSE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), -1);
}

TEST_F(SIMDSelectorCS, Require32)
{
   required_dispatch_width = 32;

   ASSERT_FALSE(should_compile(SIMD8));
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_TRUE(should_compile(SIMD32));
   brw_simd_mark_compiled(SIMD32, prog_data, not_spilled);

   ASSERT_EQ(brw_simd_select(prog_data), SIMD32);
}

TEST_F(SIMDSelectorCS, Require32ErrorWhenNotCompile)
{
   required_dispatch_width = 32;

   ASSERT_FALSE(should_compile(SIMD8));
   ASSERT_FALSE(should_compile(SIMD16));
   ASSERT_TRUE(should_compile(SIMD32));

   ASSERT_EQ(brw_simd_select(prog_data), -1);
}
