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

#include <gtest/gtest.h>
#include "mesa-gtest-extras.h"

#include "spirv/assembler/spirv_assembler.h"
#include "util/ralloc.h"

#include <vector>

class Assembler : public ::testing::Test {
protected:
   Assembler()
      : mem_ctx(ralloc_context(NULL)),
        result()
   {
   }

   ~Assembler()
   {
      ralloc_free(mem_ctx);
   }

   void
   assemble_and_verify(uint32_t version, const char *input, std::vector<uint32_t> expected)
   {
      result.words = spirv_assemble(mem_ctx, version, input, &result.word_count);
      EXPECT_TRUE(result.words) << "Couldn't parse SPIR-V";

      EXPECT_EQ(expected.size(), result.word_count);
      EXPECT_U32_ARRAY_EQUAL(expected.data(), result.words,
                             MIN2(expected.size(), result.word_count));
   }

   void *mem_ctx;

   struct {
      uint32_t *words;
      uint32_t word_count;
   } result;
};

TEST_F(Assembler, Basic)
{
   assemble_and_verify(
      0x10300,
      R"||(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 1 1 1
               OpMemberDecorate %_struct_7 0 Offset 0
               OpDecorate %_struct_7 BufferBlock
               OpDecorate %9 DescriptorSet 0
               OpDecorate %9 Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
  %_struct_7 = OpTypeStruct %uint
%_ptr_Uniform__struct_7 = OpTypePointer Uniform %_struct_7
          %9 = OpVariable %_ptr_Uniform__struct_7 Uniform
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
%_ptr_Uniform_uint = OpTypePointer Uniform %uint
          %4 = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Uniform_uint %9 %int_0
         %14 = OpLoad %uint %13 Volatile
               OpStore %13 %14
               OpReturn
               OpFunctionEnd
   )||",
      {
         0x07230203, 0x00010300, 0x00070000, 0x0000000f, 0x00000000, 0x00020011,
         0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
         0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0005000f, 0x00000005,
         0x00000002, 0x6e69616d, 0x00000000, 0x00060010, 0x00000002, 0x00000011,
         0x00000001, 0x00000001, 0x00000001, 0x00050048, 0x00000003, 0x00000000,
         0x00000023, 0x00000000, 0x00030047, 0x00000003, 0x00000003, 0x00040047,
         0x00000004, 0x00000022, 0x00000000, 0x00040047, 0x00000004, 0x00000021,
         0x00000000, 0x00020013, 0x00000005, 0x00030021, 0x00000006, 0x00000005,
         0x00040015, 0x00000007, 0x00000020, 0x00000000, 0x0003001e, 0x00000003,
         0x00000007, 0x00040020, 0x00000008, 0x00000002, 0x00000003, 0x0004003b,
         0x00000008, 0x00000004, 0x00000002, 0x00040015, 0x00000009, 0x00000020,
         0x00000001, 0x0004002b, 0x00000009, 0x0000000a, 0x00000000, 0x00040020,
         0x0000000b, 0x00000002, 0x00000007, 0x00050036, 0x00000005, 0x00000002,
         0x00000000, 0x00000006, 0x000200f8, 0x0000000c, 0x00050041, 0x0000000b,
         0x0000000d, 0x00000004, 0x0000000a, 0x0005003d, 0x00000007, 0x0000000e,
         0x0000000d, 0x00000001, 0x0003003e, 0x0000000d, 0x0000000e, 0x000100fd,
         0x00010038,
      });
}

TEST_F(Assembler, Basic2)
{
   assemble_and_verify(
      0x10500,
      R"||(
               OpCapability Shader
               OpCapability VulkanMemoryModel
               OpCapability VulkanMemoryModelDeviceScope
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical Vulkan
               OpEntryPoint GLCompute %4 "main" %9
               OpExecutionMode %4 LocalSize 1 1 1
               OpMemberDecorate %_struct_7 0 Offset 0
               OpDecorate %_struct_7 Block
               OpDecorate %9 DescriptorSet 0
               OpDecorate %9 Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
  %_struct_7 = OpTypeStruct %uint
%_ptr_StorageBuffer__struct_7 = OpTypePointer StorageBuffer %_struct_7
          %9 = OpVariable %_ptr_StorageBuffer__struct_7 StorageBuffer
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
%_ptr_StorageBuffer_uint = OpTypePointer StorageBuffer %uint
     %device = OpConstant %int 1
          %4 = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_StorageBuffer_uint %9 %int_0
         %14 = OpLoad %uint %13 NonPrivatePointer|MakePointerVisible %device
               OpStore %13 %14
               OpReturn
               OpFunctionEnd
   )||",
      {
         0x07230203, 0x00010500, 0x00070000, 0x00000010, 0x00000000, 0x00020011,
         0x00000001, 0x00020011, 0x000014e1, 0x00020011, 0x000014e2, 0x0006000b,
         0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e,
         0x00000000, 0x00000003, 0x0006000f, 0x00000005, 0x00000002, 0x6e69616d,
         0x00000000, 0x00000003, 0x00060010, 0x00000002, 0x00000011, 0x00000001,
         0x00000001, 0x00000001, 0x00050048, 0x00000004, 0x00000000, 0x00000023,
         0x00000000, 0x00030047, 0x00000004, 0x00000002, 0x00040047, 0x00000003,
         0x00000022, 0x00000000, 0x00040047, 0x00000003, 0x00000021, 0x00000000,
         0x00020013, 0x00000005, 0x00030021, 0x00000006, 0x00000005, 0x00040015,
         0x00000007, 0x00000020, 0x00000000, 0x0003001e, 0x00000004, 0x00000007,
         0x00040020, 0x00000008, 0x0000000c, 0x00000004, 0x0004003b, 0x00000008,
         0x00000003, 0x0000000c, 0x00040015, 0x00000009, 0x00000020, 0x00000001,
         0x0004002b, 0x00000009, 0x0000000a, 0x00000000, 0x00040020, 0x0000000b,
         0x0000000c, 0x00000007, 0x0004002b, 0x00000009, 0x0000000c, 0x00000001,
         0x00050036, 0x00000005, 0x00000002, 0x00000000, 0x00000006, 0x000200f8,
         0x0000000d, 0x00050041, 0x0000000b, 0x0000000e, 0x00000003, 0x0000000a,
         0x0006003d, 0x00000007, 0x0000000f, 0x0000000e, 0x00000030, 0x0000000c,
         0x0003003e, 0x0000000e, 0x0000000f, 0x000100fd, 0x00010038,
      });
}

TEST_F(Assembler, BreakIfConditionWithLoop)
{
   // From https://gitlab.khronos.org/spirv/SPIR-V/-/issues/659.

   assemble_and_verify(
      0x10500,
      R"||(
OpCapability Shader
OpMemoryModel Logical Simple
OpEntryPoint GLCompute %100 "main"
OpExecutionMode %100 LocalSize 1 1 1
%void = OpTypeVoid
%8 = OpTypeFunction %void
%bool = OpTypeBool
%cond = OpConstantNull %bool ;  a boring "false".

%100 = OpFunction %void None %8
%10 = OpLabel
OpBranch %20

%20 = OpLabel
OpLoopMerge %90 %80 None
OpBranch %30

   %30 = OpLabel
   OpSelectionMerge %50 None
   OpBranchConditional %cond %90 %50

   %50 = OpLabel
   OpBranch %80

   %80 = OpLabel ; continue target for loop
   OpBranch %20

%90 = OpLabel ; merge for loop
OpReturn
OpFunctionEnd
   )||",
      {
         0x07230203, 0x00010500, 0x00070000, 0x0000000c, 0x00000000, 0x00020011,
         0x00000001, 0x0003000e, 0x00000000, 0x00000000, 0x0005000f, 0x00000005,
         0x00000001, 0x6e69616d, 0x00000000, 0x00060010, 0x00000001, 0x00000011,
         0x00000001, 0x00000001, 0x00000001, 0x00020013, 0x00000002, 0x00030021,
         0x00000003, 0x00000002, 0x00020014, 0x00000004, 0x0003002e, 0x00000004,
         0x00000005, 0x00050036, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
         0x000200f8, 0x00000006, 0x000200f9, 0x00000007, 0x000200f8, 0x00000007,
         0x000400f6, 0x00000008, 0x00000009, 0x00000000, 0x000200f9, 0x0000000a,
         0x000200f8, 0x0000000a, 0x000300f7, 0x0000000b, 0x00000000, 0x000400fa,
         0x00000005, 0x00000008, 0x0000000b, 0x000200f8, 0x0000000b, 0x000200f9,
         0x00000009, 0x000200f8, 0x00000009, 0x000200f9, 0x00000007, 0x000200f8,
         0x00000008, 0x000100fd, 0x00010038,
      });
}
