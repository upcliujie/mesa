/*
 * Copyright © 2019 Google, Inc.
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

/**
 * A unitless opaque "size" type.  Often drivers in various places are
 * dealing with sizes in units of bytes, dwords, vec4s, etc.  Which can
 * be at worst, error-prone (mixing units, adding a value in bytes to
 * another value in dwords), and at best confusing (are we aligning this
 * value to a multiple of dwords of vec4?)
 *
 * For the helpers that convert from usize to a concrete size, the _ru
 * variants round up to the destination units, and the other variant
 * asserts that the value is aligned.  (Probably if you are truncating,
 * that is a bug rather than intentional.)
 */

#ifndef _UTIL_SIZE_H
#define _UTIL_SIZE_H

#include "util/bitscan.h"
#include "util/macros.h"
#include "util/u_math.h"

typedef struct {
   int raw;
} usize;

static inline usize
bytes_to_usize(int bytes)
{
   return (usize){bytes};
}

static inline usize
dwords_to_usize(int dwords)
{
   return bytes_to_usize(4 * dwords);
}

static inline usize
vec4s_to_usize(int vec4s)
{
   return dwords_to_usize(4 * vec4s);
}

static inline usize
usize_zero(void)
{
	return bytes_to_usize(0);
}

static inline int
usize_compare(usize a, usize b)
{
   return a.raw - b.raw;
}

static inline bool
usize_eq(usize a, usize b)
{
   return usize_compare(a, b) == 0;
}

static inline bool
usize_lt(usize a, usize b)
{
   return usize_compare(a, b) < 0;
}

static inline bool
usize_le(usize a, usize b)
{
   return usize_compare(a, b) <= 0;
}

static inline bool
usize_gt(usize a, usize b)
{
   return usize_compare(a, b) > 0;
}

static inline bool
usize_ge(usize a, usize b)
{
   return usize_compare(a, b) >= 0;
}

static inline usize
usize_align(usize val, usize alignment)
{
   return bytes_to_usize(align(val.raw, alignment.raw));
}

static inline usize
usize_round_down(usize val, usize alignment)
{
   assert(util_is_power_of_two_nonzero(alignment.raw));
   return bytes_to_usize((val.raw) & ~(alignment.raw - 1));
}

static inline usize
usize_add(usize a, usize b)
{
   return bytes_to_usize(a.raw + b.raw);
}

static inline usize
usize_sub(usize a, usize b)
{
   return bytes_to_usize(a.raw - b.raw);
}

static inline usize
usize_mul(usize a, int b)
{
   return bytes_to_usize(a.raw * b);
}

static inline usize
usize_min(usize a, usize b)
{
   return bytes_to_usize(MIN2(a.raw, b.raw));
}

static inline usize
usize_max(usize a, usize b)
{
   return bytes_to_usize(MAX2(a.raw, b.raw));
}

#define assert_aligned(val, alignment)  assert(usize_compare(val, usize_align(val, alignment)) == 0)

static inline int
usize_to_bytes(usize sz)
{
   return sz.raw;
}
static inline int
usize_to_dwords(usize sz)
{
   assert_aligned(sz, dwords_to_usize(1));
   return usize_to_bytes(sz) / 4;
}

static inline int
usize_to_dwords_ru(usize sz)
{
   return usize_to_bytes(usize_align(sz, dwords_to_usize(1))) / 4;
}

static inline int
usize_to_vec4s(usize sz)
{
   assert_aligned(sz, vec4s_to_usize(1));
   return usize_to_dwords(sz) / 4;
}

static inline int
usize_to_vec4s_ru(usize sz)
{
   return usize_to_dwords(usize_align(sz, vec4s_to_usize(1))) / 4;
}

#endif   /* _UTIL_SIZE_H */
