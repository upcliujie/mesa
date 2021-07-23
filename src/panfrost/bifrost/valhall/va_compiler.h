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
 *
 * Authors (Collabora):
 *      Alyssa Rosenzweig <alyssa.rosenzweig@collabora.com>
 */

#ifndef __VALHALL_COMPILER_H
#define __VALHALL_COMPILER_H

#include "compiler.h"
#include "valhall.h"

#ifdef __cplusplus
extern "C" {
#endif

bool va_validate_fau(bi_instr *I);
void va_validate(FILE *fp, bi_context *ctx);
void va_repair_fau(bi_builder *b, bi_instr *I);
void va_fuse_add_imm(bi_instr *I);
uint64_t va_pack_instr(const bi_instr *I, unsigned action);

static inline enum va_immediate_mode
va_fau_mode(enum bir_fau value)
{
   switch (value) {
   case BIR_FAU_TLS_PTR:
   case BIR_FAU_WLS_PTR:
      return VA_MODE_TS;
   case BIR_FAU_LANE_ID:
   case BIR_FAU_CORE_ID:
   case BIR_FAU_PROGRAM_COUNTER:
      return VA_MODE_ID;
   default:
      return VA_MODE_DEFAULT;
   }
}

static inline enum va_immediate_mode
va_select_fau_mode(const bi_instr *I)
{
   bi_foreach_src(I, s) {
      if (I->src[s].type == BI_INDEX_FAU)
         return va_fau_mode((enum bir_fau) I->src[s].value);
   }

   return VA_MODE_DEFAULT;
}

#ifdef __cplusplus
} /* extern C */
#endif

#endif
