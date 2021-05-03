/*
 * Copyright (c) 2021 Etnaviv Project
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
   etna_nir_alu_combine_const_src(nir_shader *shader);
}

class nir_opt_combine_const_src : public ::testing::Test {
protected:
   nir_opt_combine_const_src();
   ~nir_opt_combine_const_src();

   nir_variable *create_vec_out(unsigned int comps, const char *name);
   unsigned count_load_const();
   unsigned count_equal_alu_src(nir_instr *instr);

   nir_builder b;
};

nir_opt_combine_const_src::nir_opt_combine_const_src()
{
   glsl_type_singleton_init_or_ref();

   static const nir_shader_compiler_options options = { };
   b = nir_builder_init_simple_shader(MESA_SHADER_VERTEX, &options, "combine const src tests");
}

nir_opt_combine_const_src::~nir_opt_combine_const_src()
{
   if (HasFailure()) {
      printf("\nShader from the failed test:\n\n");
      nir_print_shader(b.shader, stdout);
   }

   ralloc_free(b.shader);

   glsl_type_singleton_decref();
}

nir_variable *
nir_opt_combine_const_src::create_vec_out(unsigned int comps, const char *name)
{
   return nir_variable_create(b.shader, nir_var_shader_out, glsl_vector_type(GLSL_TYPE_FLOAT, comps), name);
}

unsigned
nir_opt_combine_const_src::count_load_const()
{
   unsigned count = 0;

   nir_foreach_block(block, b.impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type == nir_instr_type_load_const)
            count++;
      }
   }
   return count;
}

unsigned
nir_opt_combine_const_src::count_equal_alu_src(nir_instr *instr)
{
   nir_alu_instr *alu = nir_instr_as_alu(instr);
   const nir_op_info *info = &nir_op_infos[alu->op];
   int first_uniform = -1;
   unsigned num = 0;

   for (unsigned i = 0; i < info->num_inputs; i++) {
      nir_const_value *cv = nir_src_as_const_value(alu->src[i].src);

      if (!cv)
         continue;

      if (first_uniform == -1) {
         first_uniform = i;
         num++;
         continue;
      }

      if (nir_srcs_equal(alu->src[first_uniform].src, alu->src[i].src))
         num++;
   }

   return num;
}

TEST_F(nir_opt_combine_const_src, vec1)
{
   nir_variable *out = create_vec_out(1, "out");
   nir_deref_instr *out_deref = nir_build_deref_var(&b, out);

   nir_ssa_def *c0 = nir_imm_float(&b, 0.1f);
   nir_ssa_def *c1 = nir_imm_float(&b, 0.2f);
   nir_ssa_def *c2 = nir_imm_float(&b, 0.3f);

   nir_ssa_def *r = nir_ffma(&b, c0, c1, c2);

   nir_store_deref(&b, out_deref, r, 0x1);

   ASSERT_EQ(count_load_const(), 3);
   ASSERT_TRUE(etna_nir_alu_combine_const_src(b.shader));
   ASSERT_TRUE(nir_opt_dce(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_load_const(), 1);
   ASSERT_EQ(count_equal_alu_src(r->parent_instr), 3);
}

TEST_F(nir_opt_combine_const_src, vec2)
{
   nir_variable *out = create_vec_out(2, "out");
   nir_deref_instr *out_deref = nir_build_deref_var(&b, out);

   nir_ssa_def *c0 = nir_imm_vec2(&b, 0.1f, 0.2f);
   nir_ssa_def *c1 = nir_imm_vec2(&b, 0.2f, 0.3f);
   nir_ssa_def *c2 = nir_imm_vec2(&b, 0.3f, 0.4f);

   nir_ssa_def *r = nir_ffma(&b, c0, c1, c2);

   nir_store_deref(&b, out_deref, r, 0x1);

   ASSERT_EQ(count_load_const(), 3);
   ASSERT_TRUE(etna_nir_alu_combine_const_src(b.shader));
   ASSERT_TRUE(nir_opt_dce(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_load_const(), 1);
   ASSERT_EQ(count_equal_alu_src(r->parent_instr), 3);
}

TEST_F(nir_opt_combine_const_src, vec4)
{
   nir_variable *out = create_vec_out(4, "out");
   nir_deref_instr *out_deref = nir_build_deref_var(&b, out);

   nir_ssa_def *c0 = nir_imm_vec4(&b, 0.1f, 0.2f, 0.3f, 0.4f);
   nir_ssa_def *c1 = nir_imm_vec4(&b, 0.1f, 0.2f, 0.3f, 0.4f);
   nir_ssa_def *c2 = nir_imm_vec4(&b, 0.1f, 0.2f, 0.3f, 0.4f);

   nir_ssa_def *r = nir_ffma(&b, c0, c1, c2);

   nir_store_deref(&b, out_deref, r, 0x1);

   ASSERT_EQ(count_load_const(), 3);
   ASSERT_TRUE(etna_nir_alu_combine_const_src(b.shader));
   ASSERT_TRUE(nir_opt_dce(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_load_const(), 1);
   ASSERT_EQ(count_equal_alu_src(r->parent_instr), 3);
}

TEST_F(nir_opt_combine_const_src, vec1_with_non_const)
{
   nir_variable *out = create_vec_out(1, "out");
   nir_deref_instr *out_deref = nir_build_deref_var(&b, out);

   nir_ssa_def *index = nir_imm_int(&b, 1);
   nir_ssa_def *offset = nir_imm_int(&b, 4);
   nir_ssa_def *u0 = nir_load_ubo(&b, 1, 32, index, offset, .align_mul = 16, .align_offset = 0, .range_base = 0, .range = 8);

   nir_ssa_def *c0 = nir_imm_float(&b, 0.1f);
   nir_ssa_def *c1 = nir_imm_float(&b, 0.3f);

   nir_ssa_def *r = nir_ffma(&b, c0, u0, c1);

   nir_store_deref(&b, out_deref, r, 0x1);

   ASSERT_EQ(count_load_const(), 2 + 2);
   ASSERT_TRUE(etna_nir_alu_combine_const_src(b.shader));
   ASSERT_TRUE(nir_opt_dce(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_load_const(), 2 + 1);
   ASSERT_EQ(count_equal_alu_src(r->parent_instr), 2);
}

TEST_F(nir_opt_combine_const_src, nir_op_vec4)
{
   nir_variable *out = create_vec_out(1, "out");
   nir_deref_instr *out_deref = nir_build_deref_var(&b, out);

   nir_ssa_def *c0 = nir_imm_float(&b, 0.1f);
   nir_ssa_def *c1 = nir_imm_float(&b, 0.2f);
   nir_ssa_def *c2 = nir_imm_float(&b, 0.3f);
   nir_ssa_def *c3 = nir_imm_float(&b, 0.4f);

   nir_ssa_def *r = nir_vec4(&b, c0, c1, c2, c3);

   nir_store_deref(&b, out_deref, r, 0x1);

   ASSERT_EQ(count_load_const(), 4);
   ASSERT_FALSE(etna_nir_alu_combine_const_src(b.shader));

   ASSERT_EQ(count_load_const(), 4);
}
