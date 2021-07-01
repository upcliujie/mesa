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

extern "C" {
   /* we really do not want to include etnaviv_nir.h as it makes it
    * harder to get this test compiling. as we are only working with
    * nir we do not need any etnaviv specifc stuff here. */

   extern bool
   etna_nir_lower_multiple_uniform_usage(nir_shader *shader);
}

class nir_lower_multiple_uniform_usage_test : public ::testing::Test {
protected:
   nir_lower_multiple_uniform_usage_test();
   ~nir_lower_multiple_uniform_usage_test();

   unsigned count_mov();

   nir_builder b;
};

nir_lower_multiple_uniform_usage_test::nir_lower_multiple_uniform_usage_test()
{
   glsl_type_singleton_init_or_ref();

   static const nir_shader_compiler_options options = { };
   b = nir_builder_init_simple_shader(MESA_SHADER_VERTEX, &options, "multiple uniform tests");
}

nir_lower_multiple_uniform_usage_test::~nir_lower_multiple_uniform_usage_test()
{
   if (HasFailure()) {
      printf("\nShader from the failed test:\n\n");
      nir_print_shader(b.shader, stdout);
   }

   ralloc_free(b.shader);

   glsl_type_singleton_decref();
}

unsigned
nir_lower_multiple_uniform_usage_test::count_mov()
{
   unsigned count = 0;

   nir_foreach_block(block, b.impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_alu)
            continue;

         nir_alu_instr *alu = nir_instr_as_alu(instr);
         if (alu->op == nir_op_mov)
            count++;
      }
   }
   return count;
}

TEST_F(nir_lower_multiple_uniform_usage_test, same_uniform)
{
   nir_ssa_def *c = nir_imm_vec4(&b, 0.0f, 0.1f, 0.2f, 0.3f);

   nir_ffma(&b, c, c, c);

   ASSERT_FALSE(etna_nir_lower_multiple_uniform_usage(b.shader));
   ASSERT_EQ(count_mov(), 0);

   nir_validate_shader(b.shader, NULL);
}

TEST_F(nir_lower_multiple_uniform_usage_test, two_uniforms)
{
   nir_ssa_def *c0 = nir_imm_vec4(&b, 0.0f, 0.1f, 0.2f, 0.3f);
   nir_ssa_def *c1 = nir_imm_vec4(&b, 0.4f, 0.5f, 0.6f, 0.7f);

   nir_ffma(&b, c0, c0, c1);

   ASSERT_TRUE(etna_nir_lower_multiple_uniform_usage(b.shader));
   ASSERT_EQ(count_mov(), 1);

   nir_validate_shader(b.shader, NULL);
}

TEST_F(nir_lower_multiple_uniform_usage_test, three_uniforms)
{
   nir_ssa_def *c0 = nir_imm_vec4(&b, 0.0f, 0.1f, 0.2f, 0.3f);
   nir_ssa_def *c1 = nir_imm_vec4(&b, 0.4f, 0.5f, 0.6f, 0.7f);
   nir_ssa_def *c2 = nir_imm_vec4(&b, 0.8f, 0.9f, 0.2f, 1.0f);

   nir_ffma(&b, c0, c1, c2);

   ASSERT_TRUE(etna_nir_lower_multiple_uniform_usage(b.shader));
   ASSERT_EQ(count_mov(), 2);

   nir_validate_shader(b.shader, NULL);
}
