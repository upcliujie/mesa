/*
 * Copyright © 2021 Valve Corporation
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

#include <gtest/gtest.h>

#include "nir.h"
#include "nir_builder.h"

namespace {

class nir_opt_shared_atomics_test : public ::testing::Test {
protected:
   nir_opt_shared_atomics_test();
   ~nir_opt_shared_atomics_test();

   unsigned count_intrinsics(nir_intrinsic_op intrinsic);

   nir_intrinsic_instr *get_intrinsic(nir_intrinsic_op intrinsic,
                                      unsigned index);

   static bool callback(nir_intrinsic_op op, uint8_t bit_size, void *data);

   bool run_pass(bool has_float32_atomic_add = true,
                 bool has_float32_atomic_min_max = true,
                 bool has_float64_atomic_min_max = true);

   nir_builder *b, _b;
   std::map<unsigned, nir_alu_instr*> movs;
   std::map<unsigned, nir_alu_src*> loads;
   std::map<unsigned, nir_ssa_def*> res_map;
};

nir_opt_shared_atomics_test::nir_opt_shared_atomics_test()
{
   glsl_type_singleton_init_or_ref();

   static const nir_shader_compiler_options options = { };
   _b = nir_builder_init_simple_shader(MESA_SHADER_COMPUTE, &options, "load store tests");
   b = &_b;
}

nir_opt_shared_atomics_test::~nir_opt_shared_atomics_test()
{
   if (HasFailure()) {
      printf("\nShader from the failed test:\n\n");
      nir_print_shader(b->shader, stdout);
   }

   ralloc_free(b->shader);

   glsl_type_singleton_decref();
}

unsigned
nir_opt_shared_atomics_test::count_intrinsics(nir_intrinsic_op intrinsic)
{
   unsigned count = 0;
   nir_foreach_block(block, b->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;
         nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
         if (intrin->intrinsic == intrinsic)
            count++;
      }
   }
   return count;
}

nir_intrinsic_instr *
nir_opt_shared_atomics_test::get_intrinsic(nir_intrinsic_op intrinsic,
                             unsigned index)
{
   nir_foreach_block(block, b->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;
         nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
         if (intrin->intrinsic == intrinsic) {
            if (index == 0)
               return intrin;
            index--;
         }
      }
   }
   return NULL;
}

bool
nir_opt_shared_atomics_test::callback(nir_intrinsic_op op, uint8_t bit_size,
                                      void *data)
{
   if ((bit_size == 32 || bit_size == 64) &&
       (op == nir_intrinsic_shared_atomic_add ||
        op == nir_intrinsic_shared_atomic_imin ||
        op == nir_intrinsic_shared_atomic_umin ||
        op == nir_intrinsic_shared_atomic_imax ||
        op == nir_intrinsic_shared_atomic_umax ||
        op == nir_intrinsic_shared_atomic_and ||
        op == nir_intrinsic_shared_atomic_or ||
        op == nir_intrinsic_shared_atomic_xor))
      return true;

   if (bit_size == 32 &&
       (op == nir_intrinsic_shared_atomic_fadd ||
        op == nir_intrinsic_shared_atomic_fmin ||
        op == nir_intrinsic_shared_atomic_fmax))
      return true;

   return false;
}

bool
nir_opt_shared_atomics_test::run_pass(bool has_float32_atomic_add,
                                      bool has_float32_atomic_min_max,
                                      bool has_float64_atomic_min_max)
{
   return nir_opt_shared_atomics(b->shader, callback, NULL);
} // namespace
}

TEST_F(nir_opt_shared_atomics_test, offset_mismatch)
{
   nir_ssa_def *addr0 = nir_imm_int(b, 0);
   nir_ssa_def *load0 = nir_build_load_shared(b, 1, 32, addr0, .base = 384,
                                              .align_mul = 4, .align_offset = 0);
   nir_ssa_def *addr1 = nir_imm_int(b, 4);
   nir_ssa_def *load1 = nir_build_load_shared(b, 1, 32, addr1, .base = 388,
                                              .align_mul = 4, .align_offset = 0);

   nir_ssa_def *iadd = nir_iadd(b, load0, load1);

   nir_build_store_shared(b, iadd, addr1, .base = 392, .write_mask = 0x1,
                          .align_mul = 4, .align_offset = 0);

   nir_validate_shader(b->shader, NULL);
   EXPECT_FALSE(run_pass(b->shader));
}

TEST_F(nir_opt_shared_atomics_test, simple_iadd_32bit)
{
   nir_ssa_def *addr0 = nir_imm_int(b, 0);
   nir_ssa_def *load0 = nir_build_load_shared(b, 1, 32, addr0, .base = 384,
                                              .align_mul = 4, .align_offset = 0);
   nir_ssa_def *addr1 = nir_imm_int(b, 4);
   nir_ssa_def *load1 = nir_build_load_shared(b, 1, 32, addr1, .base = 388,
                                              .align_mul = 4, .align_offset = 0);

   nir_ssa_def *iadd = nir_iadd(b, load0, load1);

   nir_build_store_shared(b, iadd, addr1, .base = 388, .write_mask = 0x1,
                          .align_mul = 4, .align_offset = 0);

   nir_validate_shader(b->shader, NULL);
   EXPECT_TRUE(run_pass(b->shader));

   ASSERT_EQ(count_intrinsics(nir_intrinsic_shared_atomic_add), 1);

   nir_intrinsic_instr *atomic_add = get_intrinsic(nir_intrinsic_shared_atomic_add, 0);
   ASSERT_EQ(atomic_add->dest.ssa.bit_size, 32);
   ASSERT_EQ(atomic_add->dest.ssa.num_components, 1);
   ASSERT_EQ(atomic_add->src[0].ssa, addr1);
   ASSERT_EQ(atomic_add->src[1].ssa, load0);
   ASSERT_EQ(nir_intrinsic_base(atomic_add), 388);
}

TEST_F(nir_opt_shared_atomics_test, simple_iadd_32bit_constant)
{
   nir_ssa_def *addr = nir_imm_int(b, 0);
   nir_ssa_def *load = nir_build_load_shared(b, 1, 32, addr, .base = 388,
                                             .align_mul = 4, .align_offset = 0);
   nir_ssa_def *constant = nir_imm_int(b, 42);

   nir_ssa_def *iadd = nir_iadd(b, load, constant);

   nir_build_store_shared(b, iadd, addr, .base = 388, .write_mask = 0x1,
                          .align_mul = 4, .align_offset = 0);

   nir_validate_shader(b->shader, NULL);
   EXPECT_TRUE(run_pass(b->shader));

   ASSERT_EQ(count_intrinsics(nir_intrinsic_shared_atomic_add), 1);

   nir_intrinsic_instr *atomic_add = get_intrinsic(nir_intrinsic_shared_atomic_add, 0);
   ASSERT_EQ(atomic_add->dest.ssa.bit_size, 32);
   ASSERT_EQ(atomic_add->dest.ssa.num_components, 1);
   ASSERT_EQ(atomic_add->src[0].ssa, addr);
   ASSERT_EQ(atomic_add->src[1].ssa, constant);
   ASSERT_EQ(nir_intrinsic_base(atomic_add), 388);
}

TEST_F(nir_opt_shared_atomics_test, simple_iadd_64bit)
{
   nir_ssa_def *addr0 = nir_imm_int(b, 0);
   nir_ssa_def *load0 = nir_build_load_shared(b, 1, 64, addr0, .base = 384,
                                              .align_mul = 4, .align_offset = 0);
   nir_ssa_def *addr1 = nir_imm_int(b, 4);
   nir_ssa_def *load1 = nir_build_load_shared(b, 1, 64, addr1, .base = 388,
                                              .align_mul = 4, .align_offset = 0);

   nir_ssa_def *iadd = nir_iadd(b, load0, load1);

   nir_build_store_shared(b, iadd, addr1, .base = 388, .write_mask = 0x1,
                          .align_mul = 4, .align_offset = 0);

   nir_validate_shader(b->shader, NULL);
   EXPECT_TRUE(run_pass(b->shader));

   ASSERT_EQ(count_intrinsics(nir_intrinsic_shared_atomic_add), 1);

   nir_intrinsic_instr *atomic_add = get_intrinsic(nir_intrinsic_shared_atomic_add, 0);
   ASSERT_EQ(atomic_add->dest.ssa.bit_size, 64);
   ASSERT_EQ(atomic_add->dest.ssa.num_components, 1);
   ASSERT_EQ(atomic_add->src[0].ssa, addr1);
   ASSERT_EQ(atomic_add->src[1].ssa, load0);
   ASSERT_EQ(nir_intrinsic_base(atomic_add), 388);
}
