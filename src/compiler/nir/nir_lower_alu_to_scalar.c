/*
 * Copyright © 2014-2015 Broadcom
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
 */

#include "nir.h"
#include "nir_builder.h"

struct alu_to_scalar_data {
   nir_instr_filter_cb cb;
   const void *data;
};

/** @file nir_lower_alu_to_scalar.c
 *
 * Replaces nir_alu_instr operations with more than one channel used in the
 * arguments with individual per-channel operations.
 *
 */

static uint8_t
scalar_cb(const nir_instr *instr, const void *data)
{
   /* return vectorization-width = 1 for filtered instructions */
   const struct alu_to_scalar_data *filter = data;
   return filter->cb(instr, filter->data) ? 1 : 0;
}


bool
nir_lower_alu_to_scalar(nir_shader *shader, nir_instr_filter_cb cb, const void *_data)
{
   struct alu_to_scalar_data data = {
      .cb = cb,
      .data = _data,
   };

   return nir_lower_alu_width(shader, scalar_cb, &data);
}
