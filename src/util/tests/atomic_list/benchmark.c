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

#include "util/os_time.h"
#include "util/u_atomic.h"
#include "util/u_atomic_list.h"
#include "util/u_cpu_detect.h"
#include "c11/threads.h"

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#define MAX_NUM_THREADS 16
#define MAX_NUM_ADDS (1 << 22)
#define TOTAL_ELEMS (1 << 12)

unsigned num_threads;
uint64_t adds_per_thread;
struct u_atomic_link *elems;
int64_t cpu_time_ns;

struct u_atomic_list list;

typedef void (*add_fn)(struct u_atomic_list *,
                       struct u_atomic_link *,
                       struct u_atomic_link *,
                       unsigned);

static void
run_adds(unsigned id, add_fn fn)
{
   unsigned num_elems = TOTAL_ELEMS / num_threads;
   struct u_atomic_link *my_elems = elems + (id * num_elems);

   int64_t start = os_time_get_nano();

   for (uint64_t i = 0; i < adds_per_thread; i++) {
      struct u_atomic_link *e = &my_elems[i % num_elems];
      fn(&list, e, e, 1);
   }

   int64_t end = os_time_get_nano();

   p_atomic_add(&cpu_time_ns, end - start);
}

#define DECL_TEST(suffix) \
static int \
run_##suffix(void *state) \
{ \
   run_adds((uintptr_t)state, u_atomic_list_add_list_##suffix); \
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
run_threads(int (*fn)(void*), const char *name)
{
   thrd_t threads[MAX_NUM_THREADS];

   adds_per_thread = MAX_NUM_ADDS / num_threads;
   uint64_t num_adds = adds_per_thread * num_threads;

   p_atomic_set(&cpu_time_ns, 0);
   if (num_threads == 1) {
      fn((void*)0);
   } else {
      for (int t = 0; t < num_threads; t++) {
         int ret = thrd_create(&threads[t], fn, (void*)(intptr_t)t);
         if (ret != thrd_success)
            abort();
      }

      for (int t = 0; t < num_threads; t++) {
         int ret = thrd_join(threads[t], NULL);
         if (ret != thrd_success)
            abort();
      }
   }

   int64_t time_ns = p_atomic_read(&cpu_time_ns);
   printf("    %s took %"PRId64"us (%"PRId64"ns/add)\n", name,
          time_ns / 1000, time_ns / num_adds);
}

#define RUN_TEST(suffix, name) \
   do { \
      u_atomic_list_init_##suffix(&list); \
      run_threads(run_##suffix, name); \
      u_atomic_list_del_##suffix(&list, true); \
      u_atomic_list_finish_##suffix(&list); \
   } while(0)

int
main(int argc, char **argv)
{
   util_cpu_detect();

   elems = calloc(TOTAL_ELEMS, sizeof(*elems));

   for (num_threads = 1; num_threads <= MAX_NUM_THREADS; num_threads *= 2) {
      if (num_threads > 1)
         printf("\n");

      printf("Running with %u threads: \n", num_threads);
#if defined(U_ATOMIC_LIST_HAVE_DP_IMPL)
      RUN_TEST(dp, "dual-pointer cmpxchg");
#endif
#if defined(U_ATOMIC_LIST_HAVE_X86_64_IMPL)
      RUN_TEST(x86_64, "x86_64 trampoline");
#endif
#if defined(U_ATOMIC_LIST_HAVE_48BIT_IMPL)
      RUN_TEST(48bit, "x86_64 48-bit pointers");
#endif
      RUN_TEST(mtx, "mutex-guarded");
   }

   free(elems);
}
