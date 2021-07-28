/*
 * Copyright © 2021 Collabora Ltd.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Erik Faye-Lund <erik.faye-lund@collabora.com>
 */

#include "nir.h"
#include "nir_builder.h"

static nir_ssa_def *
lower_fddx_fddy(nir_builder *b, nir_instr *instr, void *_data)
{
   enum gl_hint_mode mode = *((enum gl_hint_mode *)_data);
   nir_alu_instr *alu = nir_instr_as_alu(instr);
   nir_ssa_def *val = nir_ssa_for_alu_src(b, alu, 0);

   if (alu->op == nir_op_fddx) {
      if (mode == HINT_NICEST)
         return nir_fddx_fine(b, val);
      else {
         assert(mode == HINT_FASTEST);
         return nir_fddx_coarse(b, val);
      }
   } else {
      assert(alu->op == nir_op_fddy);
      if (mode == HINT_NICEST)
         return nir_fddy_fine(b, val);
      else {
         assert(mode == HINT_FASTEST);
         return nir_fddy_coarse(b, val);
      }
   }
}

static bool
inst_is_fddx_fddy(const nir_instr *instr, UNUSED const void *_state)
{
   if (instr->type != nir_instr_type_alu)
      return false;

   switch (nir_instr_as_alu(instr)->op) {
   case nir_op_fddx:
   case nir_op_fddy:
      return true;
   default:
      return false;
   }
}

bool
nir_lower_fddx_fddy_precision(nir_shader *shader, enum gl_hint_mode mode)
{
   assert(mode == HINT_NICEST || mode == HINT_FASTEST);
   return nir_shader_lower_instructions(shader,
                                        inst_is_fddx_fddy,
                                        lower_fddx_fddy,
                                        (void *)&mode);
}
