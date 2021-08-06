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

#ifndef UTIL_U_ATOMIC_LIST_H
#define UTIL_U_ATOMIC_LIST_H

#include "macros.h"
#include "pipe/p_config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct
#ifdef _MSC_VER
#if _WIN64
__declspec(align(16))
#else
 __declspec(align(8))
#endif
#elif defined(__LP64__)
 __attribute__((aligned(16)))
#else
 __attribute__((aligned(8)))
#endif
   u_atomic_list
{
#if defined(__LP64__) || defined(_WIN64)
   char data[16];
#else
   char data[8];
#endif
};

struct u_atomic_link {
   struct u_atomic_link *next;
};

#if defined(PIPE_ARCH_X86_64) && !defined(PIPE_CC_MSVC) && \
    !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)

#include "u_atomic_list_x86_64.h"

#define u_atomic_list_init u_atomic_list_init_x86_64
#define u_atomic_list_add_list u_atomic_list_add_list_x86_64
#define u_atomic_list_del u_atomic_list_del_x86_64
#define u_atomic_list_finish u_atomic_list_finish_x86_64

#elif defined(PIPE_CC_MSVC) || \
      defined(PIPE_ARCH_X86_64) || \
      defined(PIPE_ARCH_X86) || \
      defined(PIPE_ARCH_ARM) || \
      defined(PIPE_ARCH_AARCH64)

#include "u_atomic_list_cmpxchg.h"

#define u_atomic_list_init u_atomic_list_init_dp
#define u_atomic_list_add_list u_atomic_list_add_list_dp
#define u_atomic_list_del u_atomic_list_del_dp
#define u_atomic_list_finish u_atomic_list_finish_dp

#else

#include "u_atomic_list_mtx.h"

#define u_atomic_list_init u_atomic_list_init_mtx
#define u_atomic_list_add_list u_atomic_list_add_list_mtx
#define u_atomic_list_del u_atomic_list_del_mtx
#define u_atomic_list_finish u_atomic_list_finish_mtx

#endif

static inline void
u_atomic_list_add(struct u_atomic_list *list, struct u_atomic_link *item)
{
   u_atomic_list_add_list(list, item, item, 1);
}

#endif /* UTIL_U_ATOMIC_LIST_H */
