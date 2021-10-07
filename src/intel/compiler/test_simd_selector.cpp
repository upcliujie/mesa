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
      info = rzalloc(mem_ctx, shader_info);
   }

   ~SIMDSelectorTest() {
      ralloc_free(mem_ctx);
   };

   void *mem_ctx;
   intel_device_info *devinfo;
   shader_info *info;
};

class SIMDSelectorCS : public SIMDSelectorTest {
protected:
   SIMDSelectorCS() {
      info->stage = MESA_SHADER_COMPUTE;
      info->workgroup_size[0] = 32;
      info->workgroup_size[1] = 1;
      info->workgroup_size[2] = 1;

      devinfo->max_cs_workgroup_threads = 64;
   }
};

TEST_F(SIMDSelectorCS, VaryingDefaultsToSIMD16)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, not_spilled);
   ASSERT_FALSE(s.should_compile(SIMD32));

   ASSERT_EQ(s.result(), SIMD16);
}

TEST_F(SIMDSelectorCS, APIConstantDefaultsToSIMD16)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, not_spilled);
   ASSERT_FALSE(s.should_compile(SIMD32));

   ASSERT_EQ(s.result(), SIMD16);
}

TEST_F(SIMDSelectorCS, TooBigFor16)
{
   info->workgroup_size[0] = devinfo->max_cs_workgroup_threads;
   info->workgroup_size[1] = 32;
   info->workgroup_size[2] = 1;

   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_FALSE(s.should_compile(SIMD8));
   ASSERT_FALSE(s.should_compile(SIMD16));
   ASSERT_TRUE(s.should_compile(SIMD32));
   s.passed(SIMD32, spilled);
   ASSERT_EQ(s.result(), SIMD32);
}

TEST_F(SIMDSelectorCS, WorkgroupSize1)
{
   info->workgroup_size[0] = 1;
   info->workgroup_size[1] = 1;
   info->workgroup_size[2] = 1;

   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, not_spilled);
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), SIMD16);
}

TEST_F(SIMDSelectorCS, WorkgroupSize8)
{
   info->workgroup_size[0] = 1;
   info->workgroup_size[1] = 1;
   info->workgroup_size[2] = 1;

   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, not_spilled);
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), SIMD16);
}

TEST_F(SIMDSelectorCS, WorkgroupSizeVariable)
{
   info->workgroup_size_variable = true;
   info->workgroup_size[0] = 0;
   info->workgroup_size[1] = 0;
   info->workgroup_size[2] = 0;

   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_API_CONSTANT);

   /* Just ensure that we should compile all the shader variants, since the
    * actual selection will happen later at dispatch time.
    */

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, spilled);
   ASSERT_TRUE(s.should_compile(SIMD32));
   s.passed(SIMD32, spilled);
}

TEST_F(SIMDSelectorCS, SpillAtSIMD8)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, spilled);
   ASSERT_FALSE(s.should_compile(SIMD16));
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), SIMD8);
}

TEST_F(SIMDSelectorCS, SpillAtSIMD16)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, spilled);
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), SIMD8);
}

TEST_F(SIMDSelectorCS, EnvironmentVariable32)
{
   intel_debug |= DEBUG_DO32;

   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD32));
   s.passed(SIMD32, not_spilled);
   ASSERT_EQ(s.result(), SIMD32);
}

TEST_F(SIMDSelectorCS, EnvironmentVariable32ButSpills)
{
   intel_debug |= DEBUG_DO32;

   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_VARYING);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, not_spilled);
   ASSERT_TRUE(s.should_compile(SIMD32));
   s.passed(SIMD32, spilled);
   ASSERT_EQ(s.result(), SIMD16);
}

TEST_F(SIMDSelectorCS, Require8)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_REQUIRE_8);

   ASSERT_TRUE(s.should_compile(SIMD8));
   s.passed(SIMD8, not_spilled);
   ASSERT_FALSE(s.should_compile(SIMD16));
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), SIMD8);
}

TEST_F(SIMDSelectorCS, Require8ErrorWhenNotCompile)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_REQUIRE_8);

   ASSERT_TRUE(s.should_compile(SIMD8));
   ASSERT_FALSE(s.should_compile(SIMD16));
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), -1);
}

TEST_F(SIMDSelectorCS, Require16)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_REQUIRE_16);

   ASSERT_FALSE(s.should_compile(SIMD8));
   ASSERT_TRUE(s.should_compile(SIMD16));
   s.passed(SIMD16, not_spilled);
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), SIMD16);
}

TEST_F(SIMDSelectorCS, Require16ErrorWhenNotCompile)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_REQUIRE_16);

   ASSERT_FALSE(s.should_compile(SIMD8));
   ASSERT_TRUE(s.should_compile(SIMD16));
   ASSERT_FALSE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), -1);
}

TEST_F(SIMDSelectorCS, Require32)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_REQUIRE_32);

   ASSERT_FALSE(s.should_compile(SIMD8));
   ASSERT_FALSE(s.should_compile(SIMD16));
   ASSERT_TRUE(s.should_compile(SIMD32));
   s.passed(SIMD32, not_spilled);
   ASSERT_EQ(s.result(), SIMD32);
}

TEST_F(SIMDSelectorCS, Require32ErrorWhenNotCompile)
{
   simd_selector s(mem_ctx, devinfo, info, BRW_SUBGROUP_SIZE_REQUIRE_32);

   ASSERT_FALSE(s.should_compile(SIMD8));
   ASSERT_FALSE(s.should_compile(SIMD16));
   ASSERT_TRUE(s.should_compile(SIMD32));
   ASSERT_EQ(s.result(), -1);
}
