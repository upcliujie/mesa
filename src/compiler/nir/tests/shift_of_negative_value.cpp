/*
 * Copyright © 2020 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "gtest/gtest.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_constant_expressions.h"

namespace {

class nir_shift_of_negative_value : public ::testing::Test {
protected:
   nir_shift_of_negative_value()
   {
      glsl_type_singleton_init_or_ref();

      static const nir_shader_compiler_options options = { };
      bld = nir_builder_init_simple_shader(MESA_SHADER_VERTEX,
                                           &options,
                                           "shifts of negative value test");
   }
   ~nir_shift_of_negative_value()
   {
      ralloc_free(bld.shader);
      glsl_type_singleton_decref();
   }

   nir_builder bld;
};

} /* namespace */

TEST_F(nir_shift_of_negative_value, ishl)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_ishl, &dst, 1, 64, src, 0);
   int64_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-2, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, ishr)
{
   nir_const_value src0 = nir_const_value_for_int(-16, 64);
   nir_const_value src1 = nir_const_value_for_int(2, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_ishr, &dst, 1, 64, src, 0);
   int64_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-4, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, ibfe)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 32);
   nir_const_value src1 = nir_const_value_for_int(4, 32);
   nir_const_value src2 = nir_const_value_for_int(24, 32);
   nir_const_value *src[3] = { &src0, &src1, &src2 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_ibfe, &dst, 1, 32, src, 0);
   int32_t res = nir_const_value_as_int(dst, 32);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, ibitfield_extract)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 32);
   nir_const_value src1 = nir_const_value_for_int(2, 32);
   nir_const_value src2 = nir_const_value_for_int(26, 32);
   nir_const_value *src[3] = { &src0, &src1, &src2 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_ibitfield_extract, &dst, 1, 32, src, 0);
   int32_t res = nir_const_value_as_int(dst, 32);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, imad24_ir3)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value src2 = nir_const_value_for_int(1, 64);
   nir_const_value *src[3] = { &src0, &src1, &src2 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_imad24_ir3, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(2, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, imul24)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_imul24, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, imadsh_mix16)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value src2 = nir_const_value_for_int(1, 64);
   nir_const_value *src[3] = { &src0, &src1, &src2 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_imadsh_mix16, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(0x10001, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, umax_4x8_vc4)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_umax_4x8_vc4, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, umin_4x8_vc4)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_umin_4x8_vc4, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, umul_unorm_4x8_vc4)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_umul_unorm_4x8_vc4, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, ussub_4x8_vc4)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_ussub_4x8_vc4, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(0, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, usadd_4x8_vc4)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_usadd_4x8_vc4, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, extract_i16)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_extract_i16, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, extract_i8)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_extract_i8, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, ifind_msb)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value *src[1] = { &src0 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_ifind_msb, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, ihadd)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-3, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_ihadd, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-2, res);

   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, irhadd)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(-5, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_irhadd, &dst, 1, 64, src, 0);
   int32_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-3, res);
   nir_validate_shader(bld.shader, NULL);
}

TEST_F(nir_shift_of_negative_value, imul_high)
{
   nir_const_value src0 = nir_const_value_for_int(-1, 64);
   nir_const_value src1 = nir_const_value_for_int(1, 64);
   nir_const_value *src[2] = { &src0, &src1 };
   nir_const_value dst;

   nir_eval_const_opcode(nir_op_imul_high, &dst, 1, 64, src, 0);
   int64_t res = nir_const_value_as_int(dst, 64);
   EXPECT_EQ(-1, res);
   nir_validate_shader(bld.shader, NULL);
}
