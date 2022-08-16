/*
 * Copyright (c) 2023 Collabora, Ltd.
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
 */

#include <gtest/gtest.h>

#include "nir.h"
#include "nir_builder.h"

extern "C" {
   extern bool
   etna_nir_lower_global(nir_shader *shader);
}

class nir_lower_global_test : public ::testing::Test {
protected:
   nir_lower_global_test();
   ~nir_lower_global_test();

   nir_intrinsic_instr *intrinsic(nir_intrinsic_op op);
   unsigned count_intrinsic(nir_intrinsic_op op);

   nir_builder b;
};

nir_lower_global_test::nir_lower_global_test()
{
   glsl_type_singleton_init_or_ref();

   static const nir_shader_compiler_options options = { };
   b = nir_builder_init_simple_shader(MESA_SHADER_VERTEX, &options, "ubo lowering tests");
}

nir_lower_global_test::~nir_lower_global_test()
{
   if (HasFailure()) {
      printf("\nShader from the failed test:\n\n");
      nir_print_shader(b.shader, stdout);
   }

   ralloc_free(b.shader);

   glsl_type_singleton_decref();
}

nir_intrinsic_instr *
nir_lower_global_test::intrinsic(nir_intrinsic_op op)
{
   nir_foreach_block(block, b.impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic == op)
            return intr;
      }
   }
   return NULL;
}

unsigned
nir_lower_global_test::count_intrinsic(nir_intrinsic_op op)
{
   unsigned count = 0;

   nir_foreach_block(block, b.impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic == op)
            count++;
      }
   }
   return count;
}

TEST_F(nir_lower_global_test, nothing_to_lower)
{
   nir_ssa_def *offset = nir_imm_int(&b, 4);
   nir_load_uniform(&b, 1, 32, offset);

   nir_validate_shader(b.shader, NULL);

   ASSERT_FALSE(etna_nir_lower_global(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_uniform), 1);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_global), 0);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_global_etna), 0);
}

TEST_F(nir_lower_global_test, load)
{
   nir_ssa_def *offset = nir_imm_int(&b, 4);
   nir_ssa_def *address = nir_load_uniform(&b, 1, 32, offset);
   nir_load_global(&b, address, 4, 2, 32);

   nir_validate_shader(b.shader, NULL);

   ASSERT_TRUE(etna_nir_lower_global(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_uniform), 1);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_global), 0);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_global_etna), 1);
   nir_intrinsic_instr *intrin = intrinsic(nir_intrinsic_load_global_etna);
   ASSERT_EQ(intrin->src[0].ssa, address);
   ASSERT_EQ(nir_src_as_uint(intrin->src[1]), 0);
}

TEST_F(nir_lower_global_test, load_with_add)
{
   nir_ssa_def *offset = nir_imm_int(&b, 4);
   nir_ssa_def *address = nir_load_uniform(&b, 1, 32, offset);
   nir_ssa_def *imm = nir_imm_int(&b, 4);
   nir_ssa_def *iadd = nir_iadd(&b, address, imm);
   nir_load_global(&b, iadd, 4, 2, 32);

   nir_validate_shader(b.shader, NULL);

   ASSERT_TRUE(etna_nir_lower_global(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_uniform), 1);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_global), 0);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_global_etna), 1);
   nir_intrinsic_instr *intrin = intrinsic(nir_intrinsic_load_global_etna);
   ASSERT_EQ(intrin->src[0].ssa, address);
   ASSERT_EQ(intrin->src[1].ssa, imm);
}

TEST_F(nir_lower_global_test, store)
{
   nir_ssa_def *offset = nir_imm_int(&b, 4);
   nir_ssa_def *address = nir_load_uniform(&b, 1, 32, offset);
   nir_ssa_def *value = nir_imm_int(&b, 123);
   nir_store_global(&b, address, 4, value, BITFIELD_MASK(1));

   nir_validate_shader(b.shader, NULL);

   ASSERT_TRUE(etna_nir_lower_global(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_uniform), 1);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_store_global), 0);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_store_global_etna), 1);
   nir_intrinsic_instr *intrin = intrinsic(nir_intrinsic_store_global_etna);
   ASSERT_EQ(intrin->src[0].ssa, value);
   ASSERT_EQ(intrin->src[1].ssa, address);
   ASSERT_EQ(nir_src_as_uint(intrin->src[2]), 0);
}

TEST_F(nir_lower_global_test, store_with_add)
{
   nir_ssa_def *offset = nir_imm_int(&b, 4);
   nir_ssa_def *address = nir_load_uniform(&b, 1, 32, offset);
   nir_ssa_def *imm = nir_imm_int(&b, 4);
   nir_ssa_def *iadd = nir_iadd(&b, address, imm);
   nir_ssa_def *value = nir_imm_int(&b, 123);
   nir_store_global(&b, iadd, 4, value, BITFIELD_MASK(1));

   nir_validate_shader(b.shader, NULL);

   ASSERT_TRUE(etna_nir_lower_global(b.shader));
   nir_validate_shader(b.shader, NULL);

   ASSERT_EQ(count_intrinsic(nir_intrinsic_load_uniform), 1);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_store_global), 0);
   ASSERT_EQ(count_intrinsic(nir_intrinsic_store_global_etna), 1);
   nir_intrinsic_instr *intrin = intrinsic(nir_intrinsic_store_global_etna);
   ASSERT_EQ(intrin->src[0].ssa, value);
   ASSERT_EQ(intrin->src[1].ssa, address);
   ASSERT_EQ(intrin->src[2].ssa, imm);
}
