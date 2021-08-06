/*
 * Copyright © 2021 Intel Corporation
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

/* This file is always built with __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16 so we
 * have to smash U_ATOMIC_LIST_X86_64_C to let u_atomic_list.h know where it
 * was included from so it declares the x86_64 bits anyway.
 */
#if !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#error("This file must be built with -mcx16")
#endif

#include "u_atomic_list.h"
#include "u_atomic_list_x86_64.h"
#include "u_cpu_detect.h"

/* x86_64 is annoying.  The vast majority of x86_64 CPUs in the whild have the
 * CMPXCHG16B instruction which does a 16B compare-exchange.  However, there
 * are some older AMD CPUs and even a handful of Intel CPUs that lack the
 * instruction.  (See https://en.wikipedia.org/wiki/X86-64 for more details).
 * Fortunately, all of those CPUs are restricted to a 48-bit virtual address
 * space so we can use the top 16 bits of the pointer as the tag so long as
 * we're good about canonicalizing the pointer again once we're done with it.
 *
 * To deal with this, the x86_64 implementation is hidden behind a set of
 * function pointers.  The pointers are initialized to a _tramp function which
 * checks the CPU for CMPXCHG16B and initializes the function pointer to
 * either the _cmpxchg16b or the _48bit implementation as appropreate.  It
 * then calls the function pointer so we get the right thing.  Hopefully, CPU
 * branch prediction will get rid of the overhead of the vast majority of our
 * function pointer calls here since they'll always be the same after the
 * first one.
 */

void
u_atomic_list_init_x86_64(struct u_atomic_list *list)
{
   if (util_get_cpu_caps()->has_cx16)
      u_atomic_list_init_dp(list);
   else
      u_atomic_list_init_48bit(list);
}

static void
u_atomic_list_add_list_x86_64_tramp(struct u_atomic_list *list,
                                    struct u_atomic_link *first,
                                    struct u_atomic_link *last,
                                    unsigned count)
{
   if (util_get_cpu_caps()->has_cx16)
      u_atomic_list_add_list_x86_64_fn = u_atomic_list_add_list_dp;
   else
      u_atomic_list_add_list_x86_64_fn = u_atomic_list_add_list_48bit;

   u_atomic_list_add_list_x86_64_fn(list, first, last, count);
}

void (*u_atomic_list_add_list_x86_64_fn)(struct u_atomic_list *,
                                         struct u_atomic_link *,
                                         struct u_atomic_link *,
                                         unsigned) =
   u_atomic_list_add_list_x86_64_tramp;

static struct u_atomic_link *
u_atomic_list_del_x86_64_tramp(struct u_atomic_list *list, bool del_all)
{
   if (util_get_cpu_caps()->has_cx16)
      u_atomic_list_del_x86_64_fn = u_atomic_list_del_dp;
   else
      u_atomic_list_del_x86_64_fn = u_atomic_list_del_48bit;

   return u_atomic_list_del_x86_64_fn(list, del_all);
}

struct u_atomic_link *(*u_atomic_list_del_x86_64_fn)(struct u_atomic_list *,
                                                     bool) =
   u_atomic_list_del_x86_64_tramp;

void
u_atomic_list_finish_x86_64(struct u_atomic_list *list)
{
   if (util_get_cpu_caps()->has_cx16)
      u_atomic_list_finish_dp(list);
   else
      u_atomic_list_finish_48bit(list);
}

