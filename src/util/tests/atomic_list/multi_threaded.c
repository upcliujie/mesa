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
#define NUM_RUNS 16
#define NUM_ADDS_PER_THREAD (1 << 10)

struct elem {
   struct u_atomic_link link;
   uint32_t id;
};

struct add_thread_data {
   thrd_t thrd;

   struct elem elems[NUM_ADDS_PER_THREAD];
};

struct del_thread_data {
   thrd_t thrd;

   uint32_t found[NUM_THREADS * NUM_ADDS_PER_THREAD];
   uint32_t num_found;
};

volatile bool add_running;
struct u_atomic_list list;

static int
add_thread(void *_state)
{
   struct add_thread_data *data = _state;

   for (unsigned i = 0; i < NUM_ADDS_PER_THREAD; i++)
      u_atomic_list_add(&list, &data->elems[i].link);

   return 0;
}

static int
del_thread(void *_state)
{
   struct del_thread_data *data = _state;

   data->num_found = 0;

   while (1) {
      bool end = !add_running;

      /* Check add_running before we grab another element and only actually
       * quit if the adds are done AND the list is empty.
       */
      struct u_atomic_link *l = u_atomic_list_del(&list);
      if (end && l == NULL)
         break;

      if (l != NULL) {
         struct elem *e = (struct elem *)l;
         data->found[data->num_found++] = e->id;
      }
   }

   return 0;
}

static int
del_all_thread(void *_state)
{
   struct del_thread_data *data = _state;

   data->num_found = 0;

   while (1) {
      bool end = !add_running;

      /* Check add_running before we grab another element and only actually
       * quit if the adds are done AND the list is empty.
       */
      struct u_atomic_link *l = u_atomic_list_del_all(&list);
      if (end && l == NULL)
         break;

      for (; l; l = l->next) {
         struct elem *e = (struct elem *)l;
         data->found[data->num_found++] = e->id;
      }
   }

   return 0;
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
run_test(int (*del_fn)(void *))
{
   struct add_thread_data add_data[NUM_THREADS];
   struct del_thread_data del_data[NUM_THREADS];

   for (unsigned t = 0; t < NUM_THREADS; t++) {
      for (unsigned i = 0; i < NUM_ADDS_PER_THREAD; i++)
         add_data[t].elems[i].id = t * NUM_ADDS_PER_THREAD + i;
   }

   for (int add_threads = 1; add_threads < NUM_THREADS - 1; add_threads++) {
      int del_threads = NUM_THREADS - add_threads;
      assert(del_threads > 0);

      u_atomic_list_init(&list);

      add_running = true;

      for (int i = 0; i < add_threads; i++) {
         int ret = thrd_create(&add_data[i].thrd, add_thread, &add_data[i]);
         assert(ret == thrd_success);
      }

      for (int i = 0; i < del_threads; i++) {
         del_data[i].num_found = 0;
         int ret = thrd_create(&del_data[i].thrd, del_fn, &del_data[i]);
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

      u_atomic_list_finish(&list);

      validate(del_data, del_threads);
   }
}

int
main(int argc, char **argv)
{
   util_cpu_detect();

   for (unsigned i = 0; i < NUM_RUNS; i++) {
      run_test(del_thread);
      run_test(del_all_thread);
   }
}

