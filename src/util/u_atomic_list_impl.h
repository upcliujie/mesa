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

/*
 * Implementation of u_atomic_list.  This file should never be included
 * directly but will be included as appropriate by u_atomic_list.h or
 * u_atomic_list_x86_64.c
 */

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
#ifdef _MSC_VER
#if _WIN64
      return InterlockedCompareExchange128((LONG64 *)list->data,
                                           ((LONG64 *)_new->data)[1],
                                           ((LONG64 *)_new->data)[0],
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

   /* This read may not be atomic and almost certainly won't be for
    * double-word reads.  However, the worst that can happen if we read the
    * list wrong is that we'll have a bogus old value when we go to do the
    * cmpxchg and it will fail.
    */
   old = *(volatile struct u_atomic_list *)list;
   do {
      last->next = get_head(old);
      _new = pack(first, get_serial(old) + 1);
   } while (!__u_atomic_list_cmpxchg(list, &old, _new, bytes));
}

static inline struct u_atomic_link *
__u_atomic_list_del(struct u_atomic_list *list,
                    struct u_atomic_link *(*get_head)(struct u_atomic_list),
                    uintptr_t (*get_serial)(struct u_atomic_list),
                    struct u_atomic_list (*pack)(struct u_atomic_link *,
                                                 uintptr_t),
                    unsigned bytes)
{
   struct u_atomic_list old, _new;

   /* This read may not be atomic and almost certainly won't be for
    * double-word reads.  However, the worst that can happen if we read the
    * list wrong is that we'll have a bogus old value when we go to do the
    * cmpxchg and it will fail.
    */
   old = *(volatile struct u_atomic_list *)list;
   do {
      struct u_atomic_link *old_head = get_head(old);
      if (old_head == NULL)
         return NULL;

      _new = pack(old_head->next, get_serial(old) + 1);
   } while (!__u_atomic_list_cmpxchg(list, &old, _new, bytes));

   return get_head(old);
}

static inline struct u_atomic_link *
__u_atomic_list_del_all(struct u_atomic_list *list,
                        struct u_atomic_link *(*get_head)(struct u_atomic_list),
                        uintptr_t (*get_serial)(struct u_atomic_list),
                        struct u_atomic_list (*pack)(struct u_atomic_link *,
                                                     uintptr_t),
                        unsigned bytes)
{
   struct u_atomic_list old, _new;

   /* This read may not be atomic and almost certainly won't be for
    * double-word reads.  However, the worst that can happen if we read the
    * list wrong is that we'll have a bogus old value when we go to do the
    * cmpxchg and it will fail.
    */
   old = *(volatile struct u_atomic_list *)list;
   do {
      struct u_atomic_link *old_head = get_head(old);
      if (old_head == NULL)
         return NULL;

      _new = pack(NULL, get_serial(old) + 1);
   } while (!__u_atomic_list_cmpxchg(list, &old, _new, bytes));

   return get_head(old);
}

static inline void
__u_atomic_list_finish(UNUSED struct u_atomic_list *list,
                       UNUSED struct u_atomic_link *(*get_head)(struct u_atomic_list),
                       UNUSED unsigned bytes)
{
#ifndef NDEBUG
   struct u_atomic_list l = *(volatile struct u_atomic_list *)list;
   assert(get_head(l) == NULL);
#endif
}

/** Helper implementations for double-pointer */
static inline struct u_atomic_link *
__u_atomic_list_get_dp_head(struct u_atomic_list list)
{
   return *(struct u_atomic_link **)list.data;
}

static inline uintptr_t
__u_atomic_list_get_dp_serial(struct u_atomic_list list)
{
   return *(uintptr_t *)(list.data + sizeof(struct u_atomic_link *));
}

static inline struct u_atomic_list
__u_atomic_list_pack_dp(struct u_atomic_link *link, uintptr_t serial)
{
   struct u_atomic_list list = { .data = { 0, } };
   *(struct u_atomic_link **)list.data = link;
   *(uintptr_t *)(list.data + sizeof(struct u_atomic_link *)) = serial;
   return list;
}
