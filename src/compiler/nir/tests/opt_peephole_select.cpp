/*
 * Copyright (c) 2020 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include <gtest/gtest.h>
#include "nir.h"
#include "nir_builder.h"

class nir_opt_peephole_select_test : public ::testing::Test {
protected:
   nir_opt_peephole_select_test();
   ~nir_opt_peephole_select_test();

   nir_builder bld;
};

nir_opt_peephole_select_test::nir_opt_peephole_select_test()
{
   glsl_type_singleton_init_or_ref();

   static const nir_shader_compiler_options options = { };
   bld = nir_builder_init_simple_shader(MESA_SHADER_VERTEX, &options, "peephole test");
}

nir_opt_peephole_select_test::~nir_opt_peephole_select_test()
{
   ralloc_free(bld.shader);
   glsl_type_singleton_decref();
}

TEST_F(nir_opt_peephole_select_test, opt_load_ubo)
{
   /* Tests that opt_peephole_select correctly optimizes ubo loads:
    *
    * vec1 32 ssa_0 = load_const (0x00000001)
    * vec1 32 ssa_1 = load_const (0x00000002)
    * vec1 32 ssa_2 = load_const (0x0000000a)
    * vec1 1 ssa_3 = ieq ssa_0, ssa_1
    * if ssa_3 {
    *    block block_1:
    *    vec1 32 ssa_4 = intrinsic load_ubo (ssa_0, ssa_2) (0, 16, 0, 16, 16)
    * } else {
    *    block block_2:
    *    vec1 32 ssa_5 = intrinsic load_ubo (ssa_0, ssa_2) (0, 16, 0, 16, 16)
    * }
    */

   nir_ssa_def *one = nir_imm_int(&bld, 1);
   nir_ssa_def *two = nir_imm_int(&bld, 2);
   nir_ssa_def *ten = nir_imm_int(&bld, 10);

   nir_ssa_def *cmp_result = nir_ieq(&bld, one, two);
   nir_if *nif = nir_push_if(&bld, cmp_result);

   nir_load_ubo(&bld, 1, 32, one, ten, .align_mul = 16, .align_offset = 0, .range_base = 16, .range = 16);

   nir_push_else(&bld, NULL);

   nir_load_ubo(&bld, 1, 32, one, ten, .align_mul = 16, .align_offset = 0, .range_base = 16, .range = 16);

   nir_pop_if(&bld, NULL);

   ASSERT_TRUE(nir_opt_peephole_select(bld.shader, 16, true, true));

   nir_validate_shader(bld.shader, NULL);

   ASSERT_TRUE(exec_list_is_empty((&nir_if_first_then_block(nif)->instr_list)));
   ASSERT_TRUE(exec_list_is_empty((&nir_if_first_else_block(nif)->instr_list)));
}
