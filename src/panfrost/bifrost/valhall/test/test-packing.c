/*
 * Copyright (C) 2021 Collabora, Ltd.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "va_compiler.h"
#include "bi_test.h"
#include "bi_builder.h"

#define CASE(instr, expected) do { \
   uint64_t _value = va_pack_instr(instr, 0); \
   if (_value == expected) { \
      nr_pass++; \
   } else { \
      fprintf(stderr, "Got %" PRIx64 ", expected %" PRIx64 "\n", _value, (uint64_t) expected); \
      bi_print_instr(instr, stderr); \
      fprintf(stderr, "\n"); \
      nr_fail++; \
   } \
} while(0)

int
main(int argc, const char **argv)
{
   unsigned nr_fail = 0, nr_pass = 0;
   bi_builder *b = bit_builder(NULL);
   bi_index zero = bi_fau(BIR_FAU_IMMEDIATE | 0, false);

   CASE(bi_mov_i32_to(b, bi_register(1), bi_register(2)),
         0x0091c10000000002ULL);
   CASE(bi_mov_i32_to(b, bi_register(1), bi_fau(BIR_FAU_UNIFORM | 5, false)),
         0x0091c1000000008aULL);
   CASE(bi_fadd_f32_to(b, bi_register(0), bi_register(1), bi_register(2), BI_ROUND_NONE),
         0x00a4c00000000201ULL);
   CASE(bi_fadd_f32_to(b, bi_register(0), bi_register(1), bi_abs(bi_register(2)), BI_ROUND_NONE),
         0x00a4c02000000201ULL);
   CASE(bi_fadd_f32_to(b, bi_register(0), bi_register(1), bi_neg(bi_register(2)), BI_ROUND_NONE),
         0x00a4c01000000201ULL);

   {
      bi_instr *I = bi_fadd_f32_to(b, bi_register(0), bi_register(1),
                                   bi_neg(bi_abs(bi_register(2))),
                                   BI_ROUND_NONE);
      CASE(I, 0x00a4c03000000201ULL);

      I->clamp = BI_CLAMP_CLAMP_M1_1;
      CASE(I, 0x00a4c03200000201ULL);
   }

   CASE(bi_fadd_v2f16_to(b, bi_register(0), bi_swz_16(bi_register(1), false, false),
                         bi_swz_16(bi_register(0), true, true), BI_ROUND_NONE),
         0x00a5c0000c000001ULL);

   CASE(bi_fadd_v2f16_to(b, bi_register(0), bi_register(1), bi_register(0), BI_ROUND_NONE),
         0x00a5c00028000001ULL);

   CASE(bi_fadd_v2f16_to(b, bi_register(0), bi_register(1),
                         bi_swz_16(bi_register(0), true, false), BI_ROUND_NONE),
         0x00a5c00024000001ULL);

   CASE(bi_fadd_v2f16_to(b, bi_register(0), bi_discard(bi_abs(bi_register(0))),
                         bi_neg(zero), BI_ROUND_NONE),
         0x00a5c0902800c040ULL);

   CASE(bi_fadd_f32_to(b, bi_register(0), bi_register(1),
                       zero, BI_ROUND_NONE),
         0x00a4c0000000c001ULL);

   CASE(bi_fadd_f32_to(b, bi_register(0), bi_register(1),
                       bi_neg(zero), BI_ROUND_NONE),
         0x00a4c0100000c001ULL);

   CASE(bi_fadd_f32_to(b, bi_register(0), bi_register(1),
                       bi_half(bi_register(0), true), BI_ROUND_NONE),
         0x00a4c00008000001ULL);

   CASE(bi_fadd_f32_to(b, bi_register(0), bi_register(1),
                       bi_half(bi_register(0), false), BI_ROUND_NONE),
         0x00a4c00004000001ULL);

   CASE(bi_fma_f32_to(b, bi_register(1), bi_discard(bi_register(1)),
                         bi_fau(BIR_FAU_UNIFORM | 4, false), bi_neg(zero), BI_ROUND_NONE),
         0x00b2c10400c08841ULL);

   CASE(bi_fround_f32_to(b, bi_register(2), bi_discard(bi_neg(bi_register(2))),
                         BI_ROUND_RTN),
         0x0090c240800d0042ULL);

   CASE(bi_fround_v2f16_to(b, bi_half(bi_register(0), false), bi_register(0),
                         BI_ROUND_RTN),
         0x00904000a00f0000ULL);

   CASE(bi_fround_v2f16_to(b, bi_half(bi_register(0), false),
                           bi_swz_16(bi_register(1), true, false), BI_ROUND_RTN),
         0x00904000900f0001ULL);

   CASE(bi_fadd_imm_f32_to(b, bi_register(2), bi_discard(bi_register(2)), 0x4847C6C0),
         0x0114C24847C6C042ULL);

   CASE(bi_fadd_imm_v2f16_to(b, bi_register(2), bi_discard(bi_register(2)), 0x70AC6784),
         0x0115C270AC678442ULL);

   {
      bi_instr *I =
         bi_icmp_v2s16_to(b, bi_register(2),
                          bi_discard(bi_swz_16(bi_register(3), true, false)),
                          bi_discard(bi_swz_16(bi_register(2), true, false)),
                          BI_CMPF_GT,
                          BI_RESULT_TYPE_M1);
      I->src[2] = zero; // TODO: model in the IR

      CASE(I, 0x00f9c21184c04243);

      I->op = BI_OPCODE_FCMP_V2F16;
      I->src[1] = bi_discard(bi_swz_16(bi_register(2), false, false));
      CASE(I, 0x00f5c20190c04243);
   }

   CASE(bi_v2s16_to_v2f16_to(b, bi_register(2), bi_discard(bi_register(2)), BI_ROUND_NONE),
         0x0090c22000070042);

   {
      bi_instr *I = bi_branchz_i16(b, bi_half(bi_register(2), false), bi_null(), BI_CMPF_EQ);
      I->branch_offset = 1;
      CASE(I, 0x001fc03000000102);
   }

   {
      bi_instr *I = bi_branchz_i16(b, zero, bi_null(), BI_CMPF_EQ);
      I->branch_offset = -8;
      CASE(I, 0x001fc017fffff8c0);
   }

   TEST_END(nr_pass, nr_fail);
}
