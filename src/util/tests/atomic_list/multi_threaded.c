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

#undef NDEBUG

#include "util/bitset.h"
#include "util/u_atomic_list.h"
#include "util/u_cpu_detect.h"

#include <assert.h>
#include <stdlib.h>
#include "c11/threads.h"

#define NUM_THREADS 16
#define NUM_RUNS 4
#define NUM_ADDS_PER_THREAD (1 << 10)

struct elem {
   struct u_atomic_link link;
   uint32_t id;
};

struct add_thread_data {
   thrd_t thrd;

   struct elem *elems;
};

struct del_thread_data {
   thrd_t thrd;
   bool del_all;

   uint32_t found[NUM_THREADS * NUM_ADDS_PER_THREAD];
   uint32_t num_found;
};

volatile bool add_running;
struct u_atomic_list list;

typedef void (*add_fn)(struct u_atomic_list *,
                       struct u_atomic_link *,
                       struct u_atomic_link *,
                       unsigned);

typedef struct u_atomic_link *(*del_fn)(struct u_atomic_list *, bool);

static inline int
add_thread(struct add_thread_data *data, add_fn fn)
{
   for (unsigned i = 0; i < NUM_ADDS_PER_THREAD; i++) {
      struct u_atomic_link *e = &data->elems[i].link;
      fn(&list, e, e, 1);
   }

   return 0;
}

static inline int
del_thread(struct del_thread_data *data, del_fn fn)
{
   data->num_found = 0;

   while (1) {
      bool end = !add_running;

      /* Check add_running before we grab another element and only actually
       * quit if the adds are done AND the list is empty.
       */
      struct u_atomic_link *l = fn(&list, data->del_all);
      if (end && l == NULL)
         break;

      if (l != NULL) {
         struct elem *e = (struct elem *)l;
         data->found[data->num_found++] = e->id;
      }
   }

   return 0;
}

#define DECL_TEST(suffix) \
static int \
add_thread_##suffix(void *state) \
{ \
   add_thread(state, u_atomic_list_add_list_##suffix); \
   return 0; \
} \
static int \
del_thread_##suffix(void *state) \
{ \
   del_thread(state, u_atomic_list_del_##suffix); \
   return 0; \
}

#if defined(U_ATOMIC_LIST_HAVE_DP_IMPL)
DECL_TEST(dp)
#endif
#if defined(U_ATOMIC_LIST_HAVE_X86_64_IMPL)
DECL_TEST(x86_64)
#endif
#if defined(U_ATOMIC_LIST_HAVE_48BIT_IMPL)
DECL_TEST(48bit)
#endif
DECL_TEST(mtx)

static void
run_threads(struct add_thread_data *add_data, int (*add)(void *), int add_threads,
            struct del_thread_data *del_data, int (*del)(void *), int del_threads)
{
   add_running = true;

   for (int i = 0; i < add_threads; i++) {
      int ret = thrd_create(&add_data[i].thrd, add, &add_data[i]);
      assert(ret == thrd_success);
   }

   for (int i = 0; i < del_threads; i++) {
      del_data[i].num_found = 0;
      int ret = thrd_create(&del_data[i].thrd, del, &del_data[i]);
      assert(ret == thrd_success);
   }

   for (int i = 0; i < add_threads; i++) {
      int ret = thrd_join(add_data[i].thrd, NULL);
      assert(ret == thrd_success);
   }

   add_running = false;

   for (int i = 0; i < del_threads; i++) {
      int ret = thrd_join(del_data[i].thrd, NULL);
      assert(ret == thrd_success);
   }
}

static void
validate(struct del_thread_data *data, unsigned num_threads)
{
   BITSET_DECLARE(found, NUM_THREADS * NUM_ADDS_PER_THREAD);
   memset(found, 0, sizeof(found));

   for (unsigned t = 0; t < num_threads; t++) {
      for (unsigned i = 0; i < data[t].num_found; i++) {
         uint32_t id = data[t].found[i];
         assert(!BITSET_TEST(found, id));
         BITSET_SET(found, id);
      }
   }
}

static void
run_test(bool del_all)
{
   struct add_thread_data add_data[NUM_THREADS];
   struct del_thread_data del_data[NUM_THREADS];

   for (unsigned t = 0; t < NUM_THREADS; t++) {
      add_data[t].elems = calloc(NUM_ADDS_PER_THREAD,
                                 sizeof(*add_data[t].elems));
      for (unsigned i = 0; i < NUM_ADDS_PER_THREAD; i++)
         add_data[t].elems[i].id = t * NUM_ADDS_PER_THREAD + i;

      del_data[t].del_all = del_all;
   }

   for (int add_threads = 1; add_threads < NUM_THREADS - 1; add_threads++) {
      int del_threads = NUM_THREADS - add_threads;
      assert(del_threads > 0);

#define RUN_TEST(suffix) \
   do { \
      u_atomic_list_init_##suffix(&list); \
      run_threads(add_data, add_thread_##suffix, add_threads, \
                  del_data, del_thread_##suffix, del_threads); \
      u_atomic_list_finish_##suffix(&list); \
      validate(del_data, del_threads); \
   } while(0)

#if defined(U_ATOMIC_LIST_HAVE_DP_IMPL)
      RUN_TEST(dp);
#endif
#if defined(U_ATOMIC_LIST_HAVE_X86_64_IMPL)
      RUN_TEST(x86_64);
#endif
#if defined(U_ATOMIC_LIST_HAVE_48BIT_IMPL)
      RUN_TEST(48bit);
#endif
      RUN_TEST(mtx);
   }

   for (unsigned t = 0; t < NUM_THREADS; t++)
      free(add_data[t].elems);
}

int
main(int argc, char **argv)
{
   util_cpu_detect();

   for (unsigned i = 0; i < NUM_RUNS; i++) {
      run_test(false);
      run_test(true);
   }
}

