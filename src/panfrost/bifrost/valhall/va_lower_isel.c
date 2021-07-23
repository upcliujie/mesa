/*
 * Copyright (C) 2021 Collabora Ltd.
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
#include "valhall.h"
#include "bi_builder.h"

void
va_lower_isel(bi_instr *I)
{
   switch (I->op) {

   /* Integer addition has swizzles and addition with 0 is canonical swizzle */
   case BI_OPCODE_SWZ_V2I16:
      I->op = BI_OPCODE_IADD_V2U16;
      I->src[1] = bi_zero();
      break;

   /* Extra source in Valhall not yet modeled in the Bifrost IR */
   case BI_OPCODE_ICMP_I32:
      I->op = BI_OPCODE_ICMP_U32;
      I->src[2] = bi_zero();
      break;

   case BI_OPCODE_ICMP_V2I16:
      I->op = BI_OPCODE_ICMP_V2U16;
      I->src[2] = bi_zero();
      break;

   case BI_OPCODE_ICMP_V4I8:
      I->op = BI_OPCODE_ICMP_V4U8;
      I->src[2] = bi_zero();
      break;

   case BI_OPCODE_ICMP_U32:
   case BI_OPCODE_ICMP_V2U16:
   case BI_OPCODE_ICMP_V4U8:
   case BI_OPCODE_ICMP_S32:
   case BI_OPCODE_ICMP_V2S16:
   case BI_OPCODE_ICMP_V4S8:
   case BI_OPCODE_FCMP_F32:
   case BI_OPCODE_FCMP_V2F16:
      I->src[2] = bi_zero();
      break;
   default:
      break;
   }
}
