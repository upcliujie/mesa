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

#define CASE_CB(b, left) va_lower_isel(left)
#define CASE(instr, expected) INSTRUCTION_CASE(instr, expected, CASE_CB)
#define NEGCASE(instr) CASE(instr, instr)

int
main(int argc, const char **argv)
{
   unsigned nr_fail = 0, nr_pass = 0;
   bi_builder *b = bit_builder(NULL);
   bi_index reg = bi_register(1);

   /* 16-bit swizzles as iadd */
   for (unsigned i = 0; i < 2; ++i) {
      for (unsigned j = 0; j < 2; ++j) {
         CASE(bi_swz_v2i16_to(b, reg, bi_swz_16(reg, i, j)),
              bi_iadd_v2u16_to(b, reg, bi_swz_16(reg, i, j), bi_zero(), false));
      }
   }

   /* Discard gets implicit R60 destination for coverage mask */
   {
      bi_instr *I = bi_discard_f32(b, reg, reg, BI_CMPF_EQ);
      bi_instr *J = bi_discard_f32(b, reg, reg, BI_CMPF_EQ);
      J->dest[0] = bi_register(60);

      CASE(I, J);
   }

   /* Jumps lowered to branches */
   CASE(bi_jump(b, bi_imm_u32(0xDEADBEEF)),
        bi_branchz_i16(b, bi_zero(), bi_imm_u32(0xDEADBEEF), BI_CMPF_EQ));

   /* Negative smoke tests */
   NEGCASE(bi_fadd_f32_to(b, reg, reg, reg, BI_ROUND_RTP));

   TEST_END(nr_pass, nr_fail);
}
