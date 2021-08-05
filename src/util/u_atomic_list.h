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

static inline void
u_atomic_list_init(struct u_atomic_list *list)
{
   memset(list, 0, sizeof(*list));
}

static inline void u_atomic_list_add_list(struct u_atomic_list *list,
                                          struct u_atomic_link *first,
                                          struct u_atomic_link *last,
                                          unsigned count);

static inline void
u_atomic_list_add(struct u_atomic_list *list, struct u_atomic_link *item)
{
   u_atomic_list_add_list(list, item, item, 1);
}

static inline struct u_atomic_link *u_atomic_list_del(struct u_atomic_list *list);
static inline struct u_atomic_link *u_atomic_list_del_all(struct u_atomic_list *list);
static inline void u_atomic_list_finish(struct u_atomic_list *list);

#if defined(PIPE_ARCH_X86_64) && \
    (defined(U_ATOMIC_LIST_X86_64_C) || \
     !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16))

/* For x86_64, things are really annoying because the CMPXCHG16B instruction
 * doesn't actually exist everywhere.  We have to work around this by swapping
 * out the implementation based on CPU ID.
 */

extern void (*__u_atomic_list_add_list_x86_64)(struct u_atomic_list *,
                                               struct u_atomic_link *,
                                               struct u_atomic_link *,
                                               unsigned);
extern struct u_atomic_link *(*__u_atomic_list_del_x86_64)(struct u_atomic_list *);
extern struct u_atomic_link *(*__u_atomic_list_del_all_x86_64)(struct u_atomic_list *);
extern void (*__u_atomic_list_finish_x86_64)(struct u_atomic_list *);

static inline void u_atomic_list_add_list(struct u_atomic_list *list,
                                          struct u_atomic_link *first,
                                          struct u_atomic_link *last,
                                          unsigned count)
{
   __u_atomic_list_add_list_x86_64(list, first, last, count);
}

static inline struct u_atomic_link *
u_atomic_list_del(struct u_atomic_list *list)
{
   return __u_atomic_list_del_x86_64(list);
}

static inline struct u_atomic_link *
u_atomic_list_del_all(struct u_atomic_list *list)
{
   return __u_atomic_list_del_all_x86_64(list);
}

static inline void
u_atomic_list_finish(struct u_atomic_list *list)
{
   __u_atomic_list_finish_x86_64(list);
}

#else

#include "u_atomic_list_impl.h"

static inline void u_atomic_list_add_list(struct u_atomic_list *list,
                                          struct u_atomic_link *first,
                                          struct u_atomic_link *last,
                                          unsigned count)
{
   __u_atomic_list_add_list(list, first, last, count,
                            __u_atomic_list_get_dp_head,
                            __u_atomic_list_get_dp_serial,
                            __u_atomic_list_pack_dp,
                            sizeof(void *) * 2);
}

static inline struct u_atomic_link *
u_atomic_list_del(struct u_atomic_list *list)
{
   return __u_atomic_list_del(list,
                              __u_atomic_list_get_dp_head,
                              __u_atomic_list_get_dp_serial,
                              __u_atomic_list_pack_dp,
                              sizeof(void *) * 2);
}

static inline struct u_atomic_link *
u_atomic_list_del_all(struct u_atomic_list *list)
{
   return __u_atomic_list_del_all(list,
                                  __u_atomic_list_get_dp_head,
                                  __u_atomic_list_get_dp_serial,
                                  __u_atomic_list_pack_dp,
                                  sizeof(void *) * 2);
}

static inline void
u_atomic_list_finish(struct u_atomic_list *list)
{
   __u_atomic_list_finish(list, __u_atomic_list_get_dp_head,
                          sizeof(void *) * 2);
}

#endif

#endif /* UTIL_U_ATOMIC_LIST_H */
