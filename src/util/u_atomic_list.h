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

/** Atomic singly-linked list
 *
 * This list primitive is an invasive singly-linked list that is guaranteed to
 * be thread-safe so long as you only use the primitives provided and never
 * attempt to walk the list itself.
 *
 * The following functions are provided (with several implementations
 * depending on platform):
 *
 * void u_atomic_list_init(struct u_atomic_list *list);
 *
 * void u_atomic_list_finish(struct u_atomic_list *list);
 *
 * void u_atomic_list_add(struct u_atomic_list *list,
 *                        struct u_atomic_link *item);
 *
 * void u_atomic_list_add_list(struct u_atomic_list *list,
 *                             struct u_atomic_link *first,
 *                             struct u_atomic_link *last,
 *                             unsigned count);
 *
 * struct u_atomic_link *u_atomic_list_del(struct u_atomic_list *list,
 *                                         bool del_all);
 */

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

#ifdef _MSC_VER
#include <windows.h>
#include <intrin.h>
#endif

static inline bool
__u_atomic_list_cmpxchg(struct u_atomic_list *list,
                        struct u_atomic_list *cmp_res,
                        struct u_atomic_list _new,
                        unsigned bytes)
{
   if (bytes == 8) {
      int64_t *dst = (int64_t *)list->data;
      int64_t cmp64 = *(int64_t *)cmp_res->data;
      int64_t _new64 = *(int64_t *)_new.data;
#ifdef _MSC_VER
      int64_t res64 = InterlockedCompareExchange64(dst, _new64, cmp64);
#else
      int64_t res64 = __sync_val_compare_and_swap_8(dst, cmp64, _new64);
#endif
      *(int64_t *)cmp_res->data = res64;
      return cmp64 == res64;
   } else {
      assert(bytes == 16);
      assert(sizeof(list->data) == 16);
#ifdef _MSC_VER
#if _WIN64
      return InterlockedCompareExchange128((LONG64 *)list->data,
                                           ((LONG64 *)_new.data)[1],
                                           ((LONG64 *)_new.data)[0],
                                           (LONG64 *)cmp_res->data);
#else
      unreachable("InterlockedCompareExchange128 not available on 32-bit");
#endif
#elif defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
      __int128 *dst = (__int128 *)list->data;
      __int128 cmp128 = *(__int128 *)cmp_res->data;
      __int128 _new128 = *(__int128 *)_new.data;
      __int128 res128 = __sync_val_compare_and_swap_16(dst, cmp128, _new128);
      *(__int128 *)cmp_res->data = res128;
      return cmp128 == res128;
#else
      unreachable("__sync_val_compare_and_swap_16 not available on 32-bit");
#endif
   }
}

static void
__u_atomic_list_read(struct u_atomic_list *list,
                     struct u_atomic_list *res,
                     unsigned bytes)
{
   /* This read may not be atomic and almost certainly won't be for
    * double-word reads.  However, the worst that can happen if we read the
    * list wrong is that we'll have a bogus old value when we go to do the
    * cmpxchg and it will fail.
    */
   for (unsigned i = 0; i < bytes; i += 8)
      *(int64_t *)(res->data + i) = *(volatile int64_t *)(list->data + i);
}

static inline void
__u_atomic_list_add_list(struct u_atomic_list *list,
                         struct u_atomic_link *first,
                         struct u_atomic_link *last,
                         unsigned count,
                         struct u_atomic_link *(*get_head)(struct u_atomic_list),
                         uintptr_t (*get_serial)(struct u_atomic_list),
                         struct u_atomic_list (*pack)(struct u_atomic_link *,
                                                      uintptr_t),
                         unsigned bytes)
{
   struct u_atomic_list old, _new;
#ifndef NDEBUG
   unsigned check_count = 1;
   for (struct u_atomic_link *i = first; i != last; i = i->next)
      check_count++;
   assert(check_count == count);
#endif

   __u_atomic_list_read(list, &old, bytes);
   do {
      last->next = get_head(old);
      _new = pack(first, get_serial(old) + 1);
   } while (!__u_atomic_list_cmpxchg(list, &old, _new, bytes));
}

static inline struct u_atomic_link *
__u_atomic_list_del(struct u_atomic_list *list, bool del_all,
                    struct u_atomic_link *(*get_head)(struct u_atomic_list),
                    uintptr_t (*get_serial)(struct u_atomic_list),
                    struct u_atomic_list (*pack)(struct u_atomic_link *,
                                                 uintptr_t),
                    unsigned bytes)
{
   struct u_atomic_list old, _new;

   __u_atomic_list_read(list, &old, bytes);
   do {
      struct u_atomic_link *old_head = get_head(old);
      if (old_head == NULL)
         return NULL;

      _new = pack(del_all ? NULL : old_head->next, get_serial(old) + 1);
   } while (!__u_atomic_list_cmpxchg(list, &old, _new, bytes));

   return get_head(old);
}

static inline void
__u_atomic_list_finish(UNUSED struct u_atomic_list *list,
                       UNUSED struct u_atomic_link *(*get_head)(struct u_atomic_list))
{
#ifndef NDEBUG
   assert(get_head(*list) == NULL);
#endif
}

struct u_atomic_list_dp_impl {
   struct u_atomic_link *head;
   uintptr_t serial;
};

/**
 * Double-pointer implementation
 */

#if defined(_WIN64) || \
    (defined(PIPE_ARCH_X86_64) && \
     defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16))
#define U_ATOMIC_LIST_HAVE_CMPXCHG16B 1
#endif

#if defined(U_ATOMIC_LIST_HAVE_CMPXCHG16B) || \
    defined(PIPE_ARCH_X86) || \
    defined(PIPE_ARCH_ARM) || \
    defined(PIPE_ARCH_AARCH64)

#define U_ATOMIC_LIST_HAVE_DP_IMPL 1

static inline struct u_atomic_link *
__u_atomic_list_get_dp_head(struct u_atomic_list list)
{
   return ((struct u_atomic_list_dp_impl *)list.data)->head;
}

static inline uintptr_t
__u_atomic_list_get_dp_serial(struct u_atomic_list list)
{
   return ((struct u_atomic_list_dp_impl *)list.data)->serial;
}

static inline struct u_atomic_list
__u_atomic_list_pack_dp(struct u_atomic_link *link, uintptr_t serial)
{
   struct u_atomic_list list = { .data = { 0, } };
   ((struct u_atomic_list_dp_impl *)list.data)->head = link;
   ((struct u_atomic_list_dp_impl *)list.data)->serial = serial;
   return list;
}

static inline void
u_atomic_list_init_dp(struct u_atomic_list *list)
{
   memset(list, 0, sizeof(*list));
}

static inline void
u_atomic_list_finish_dp(struct u_atomic_list *list)
{
   __u_atomic_list_finish(list, __u_atomic_list_get_dp_head);
}

static inline void u_atomic_list_add_list_dp(struct u_atomic_list *list,
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
u_atomic_list_del_dp(struct u_atomic_list *list, bool del_all)
{
   return __u_atomic_list_del(list, del_all,
                              __u_atomic_list_get_dp_head,
                              __u_atomic_list_get_dp_serial,
                              __u_atomic_list_pack_dp,
                              sizeof(void *) * 2);
}

#endif

/*
 * 48-bit pointer implementation
 */

#if defined(PIPE_ARCH_X86_64)

#define U_ATOMIC_LIST_HAVE_48BIT_IMPL 1

/** Helper implementations for 48-bit pointers */
static inline struct u_atomic_link *
__u_atomic_list_get_48bit_head(struct u_atomic_list list)
{
   int64_t p = *(int64_t *)list.data;
   p = (p << 16) >> 16;
   return (struct u_atomic_link *)p;
}

static inline uintptr_t
__u_atomic_list_get_48bit_serial(struct u_atomic_list list)
{
   uint64_t p = *(uint64_t *)list.data;
   return p >> 48;
}

static inline struct u_atomic_list
__u_atomic_list_pack_48bit(struct u_atomic_link *link, uintptr_t serial)
{
   int64_t p = (int64_t)link;
   /* Make sure it's a canonical 48-bit pointer */
   assert(p == ((p << 16) >> 16));
   p = (p & 0x0000ffffffffffffull) | ((uint64_t)serial << 48);
   struct u_atomic_list list = { .data = { 0, } };
   *(uint64_t *)list.data = p;
   return list;
}

static inline void
u_atomic_list_init_48bit(struct u_atomic_list *list)
{
   memset(list, 0, sizeof(*list));
}

static inline void
u_atomic_list_finish_48bit(struct u_atomic_list *list)
{
   __u_atomic_list_finish(list, __u_atomic_list_get_48bit_head);
}

static inline void
u_atomic_list_add_list_48bit(struct u_atomic_list *list,
                             struct u_atomic_link *first,
                             struct u_atomic_link *last,
                             unsigned count)
{
   __u_atomic_list_add_list(list, first, last, count,
                            __u_atomic_list_get_48bit_head,
                            __u_atomic_list_get_48bit_serial,
                            __u_atomic_list_pack_48bit,
                            8);
}

static inline struct u_atomic_link *
u_atomic_list_del_48bit(struct u_atomic_list *list, bool del_all)
{
   return __u_atomic_list_del(list, del_all,
                              __u_atomic_list_get_48bit_head,
                              __u_atomic_list_get_48bit_serial,
                              __u_atomic_list_pack_48bit,
                              8);
}

#endif

/*
 * Mutex-locked implementation
 */

#define U_ATOMIC_LIST_HAVE_MTX_IMPL 1

#include "simple_mtx.h"

struct u_atomic_list_mtx_impl {
   struct u_atomic_link *head;
   simple_mtx_t mtx;
};

static inline void
u_atomic_list_init_mtx(struct u_atomic_list *list)
{
   struct u_atomic_list_mtx_impl *impl = (void *)list->data;

   impl->head = NULL;
   simple_mtx_init(&impl->mtx, mtx_plain);
}

static inline void
u_atomic_list_finish_mtx(struct u_atomic_list *list)
{
   struct u_atomic_list_mtx_impl *impl = (void *)list->data;
   assert(impl->head == NULL);
   simple_mtx_destroy(&impl->mtx);
}

static inline void
u_atomic_list_add_list_mtx(struct u_atomic_list *list,
                           struct u_atomic_link *first,
                           struct u_atomic_link *last,
                           unsigned count)
{
   struct u_atomic_list_mtx_impl *impl = (void *)list->data;

#ifndef NDEBUG
   unsigned check_count = 1;
   for (struct u_atomic_link *i = first; i != last; i = i->next)
      check_count++;
   assert(check_count == count);
#endif

   simple_mtx_lock(&impl->mtx);
   last->next = impl->head;
   impl->head = first;
   simple_mtx_unlock(&impl->mtx);
}

static inline struct u_atomic_link *
u_atomic_list_del_mtx(struct u_atomic_list *list, bool del_all)
{
   struct u_atomic_list_mtx_impl *impl = (void *)list->data;

   simple_mtx_lock(&impl->mtx);
   struct u_atomic_link *head = impl->head;
   if (head != NULL)
      impl->head = del_all ? NULL : head->next;
   simple_mtx_unlock(&impl->mtx);

   return head;
}

/*
 * x86_64 implementation.  See u_atomic_list_x86_64.c for more details
 */


#if defined(PIPE_ARCH_X86_64) && !defined(PIPE_CC_MSVC)

#define U_ATOMIC_LIST_HAVE_X86_64_IMPL 1

extern void (*u_atomic_list_add_list_x86_64_fn)(struct u_atomic_list *,
                                                struct u_atomic_link *,
                                                struct u_atomic_link *,
                                                unsigned);
extern struct u_atomic_link *(*u_atomic_list_del_x86_64_fn)(struct u_atomic_list *list,
                                                            bool del_all);

void u_atomic_list_init_x86_64(struct u_atomic_list *list);
void u_atomic_list_finish_x86_64(struct u_atomic_list *list);

static inline void
u_atomic_list_add_list_x86_64(struct u_atomic_list *list,
                              struct u_atomic_link *first,
                              struct u_atomic_link *last,
                              unsigned count)
{
   u_atomic_list_add_list_x86_64_fn(list, first, last, count);
}

static inline struct u_atomic_link *
u_atomic_list_del_x86_64(struct u_atomic_list *list, bool del_all)
{
   return u_atomic_list_del_x86_64_fn(list, del_all);
}

#endif

#if defined(U_ATOMIC_LIST_HAVE_DP_IMPL)

#define u_atomic_list_init u_atomic_list_init_dp
#define u_atomic_list_add_list u_atomic_list_add_list_dp
#define u_atomic_list_del u_atomic_list_del_dp
#define u_atomic_list_finish u_atomic_list_finish_dp

#elif defined(U_ATOMIC_LIST_HAVE_X86_64_IMPL)

#define u_atomic_list_init u_atomic_list_init_x86_64
#define u_atomic_list_add_list u_atomic_list_add_list_x86_64
#define u_atomic_list_del u_atomic_list_del_x86_64
#define u_atomic_list_finish u_atomic_list_finish_x86_64

#elif defined(U_ATOMIC_LIST_HAVE_MTX_IMPL)

#define u_atomic_list_init u_atomic_list_init_mtx
#define u_atomic_list_add_list u_atomic_list_add_list_mtx
#define u_atomic_list_del u_atomic_list_del_mtx
#define u_atomic_list_finish u_atomic_list_finish_mtx

#else

#error("No implementation found");

#endif

static inline void
u_atomic_list_add(struct u_atomic_list *list, struct u_atomic_link *item)
{
   u_atomic_list_add_list(list, item, item, 1);
}

#endif /* UTIL_U_ATOMIC_LIST_H */
