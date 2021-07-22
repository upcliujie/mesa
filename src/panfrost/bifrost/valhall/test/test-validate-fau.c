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
   if (va_validate_fau(instr) == expected) { \
      nr_pass++; \
   } else { \
      fprintf(stderr, "Incorrect validation for:\n"); \
      bi_print_instr(instr, stderr); \
      fprintf(stderr, "\n"); \
      nr_fail++; \
   } \
} while(0)

#define VALID(instr) CASE(instr, true)
#define INVALID(instr) CASE(instr, false)

int
main(int argc, const char **argv)
{
   unsigned nr_fail = 0, nr_pass = 0;
   bi_builder *b = bit_builder(NULL);
   bi_index zero = bi_fau(BIR_FAU_IMMEDIATE | 0, false);
   bi_index imm1 = bi_fau(BIR_FAU_IMMEDIATE | 1, false);
   bi_index imm2 = bi_fau(BIR_FAU_IMMEDIATE | 2, false);
   bi_index unif = bi_fau(BIR_FAU_UNIFORM | 5, false);
   bi_index unif2 = bi_fau(BIR_FAU_UNIFORM | 6, false);
   bi_index core_id = bi_fau(BIR_FAU_CORE_ID, false);
   bi_index lane_id = bi_fau(BIR_FAU_LANE_ID, false);

   /* An instruction may access no more than a single 64-bit uniform slot. */
   VALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), bi_register(3),
            unif, BI_ROUND_NONE));
   VALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), bi_word(unif, 1),
            unif, BI_ROUND_NONE));
   VALID(bi_fma_f32_to(b, bi_register(1), unif, unif, bi_word(unif, 1),
            BI_ROUND_NONE));
   INVALID(bi_fma_f32_to(b, bi_register(1), unif, unif2, bi_register(1),
            BI_ROUND_NONE));
   INVALID(bi_fma_f32_to(b, bi_register(1), unif, unif2, bi_word(unif, 1),
            BI_ROUND_NONE));

   /* An instruction may access no more than 64-bits of combined uniforms and constants. */
   VALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), bi_word(unif, 1),
            unif, BI_ROUND_NONE));
   VALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), zero,
            unif, BI_ROUND_NONE));
   VALID(bi_fma_f32_to(b, bi_register(1), zero, imm1, imm1, BI_ROUND_NONE));
   INVALID(bi_fma_f32_to(b, bi_register(1), zero, bi_word(unif, 1),
            unif, BI_ROUND_NONE));
   INVALID(bi_fma_f32_to(b, bi_register(1), zero, imm1, imm2, BI_ROUND_NONE));

   /* An instruction may only access uniforms in the default immediate mode. */
   INVALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), bi_word(unif, 1),
            lane_id, BI_ROUND_NONE));
   INVALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), bi_word(unif, 1),
            core_id, BI_ROUND_NONE));

   /* An instruction may access no more than a single special immediate (e.g. lane_id). */
   VALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), bi_register(2),
            lane_id, BI_ROUND_NONE));
   VALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), bi_register(2),
            core_id, BI_ROUND_NONE));
   INVALID(bi_fma_f32_to(b, bi_register(1), bi_register(2), lane_id,
            core_id, BI_ROUND_NONE));

   /* Smoke testing */
   VALID(bi_mov_i32_to(b, bi_register(1), bi_register(2)));
   VALID(bi_mov_i32_to(b, bi_register(1), bi_fau(BIR_FAU_UNIFORM | 5, false)));
   VALID(bi_fma_f32_to(b, bi_register(1), bi_discard(bi_register(1)),
                         bi_fau(BIR_FAU_UNIFORM | 4, false), bi_neg(zero), BI_ROUND_NONE));

   TEST_END(nr_pass, nr_fail);
}
