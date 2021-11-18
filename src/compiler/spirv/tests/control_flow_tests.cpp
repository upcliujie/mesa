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

#include "helpers.h"

class ControlFlow : public spirv_test {};

TEST_F(ControlFlow, Basic)
{
   get_nir_from_asm(
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
   )||");

   ASSERT_TRUE(shader);
}

TEST_F(ControlFlow, BreakIfConditionWithLoop)
{
   // From https://gitlab.khronos.org/spirv/SPIR-V/-/issues/659.

   get_nir_from_asm(
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
   )||");

   ASSERT_TRUE(shader);
}

TEST_F(ControlFlow, DISABLED_EarlyMerge)
{
   // https://gitlab.khronos.org/spirv/SPIR-V/-/issues/640

   get_nir_from_asm(
      0x10000,
      R"||(
            OpCapability Shader
       %1 = OpExtInstImport "GLSL.std.450"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %main "main" %colour
            OpExecutionMode %main OriginUpperLeft
            OpSource GLSL 460
            OpName %main "main"
            OpName %colour "colour"
            OpDecorate %colour Location 0
    %void = OpTypeVoid
       %3 = OpTypeFunction %void
    %bool = OpTypeBool
   %false = OpConstantFalse %bool
    %true = OpConstantTrue %bool
   %float = OpTypeFloat 32
    %vec4 = OpTypeVector %float 4
   %pvec4 = OpTypePointer Output %vec4
  %colour = OpVariable %pvec4 Output
      %f0 = OpConstant %float 0.0
      %f1 = OpConstant %float 1.0
      %13 = OpConstantComposite %vec4 %f0 %f1 %f0 %f1
    %main = OpFunction %void None %3
      %B5 = OpLabel
            OpSelectionMerge %B8 None
            OpBranchConditional %true %B6 %B7
      %B6 = OpLabel
            OpBranch %B7
      %B7 = OpLabel
            OpBranch %B8
      %B8 = OpLabel
            OpStore %colour %13
            OpReturn
            OpFunctionEnd
   )||", MESA_SHADER_FRAGMENT);

   ASSERT_TRUE(shader);
}
