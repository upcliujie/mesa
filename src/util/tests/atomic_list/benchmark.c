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
uint64_t cpu_time_ns;

struct u_atomic_list list;

#define NSEC_PER_SEC (1000 * USEC_PER_SEC)
#define USEC_PER_SEC (1000 * MSEC_PER_SEC)
#define MSEC_PER_SEC (1000)

static uint64_t
gettime_ns(void)
{
   struct timespec current;
   clock_gettime(CLOCK_MONOTONIC, &current);
   return (uint64_t)current.tv_sec * NSEC_PER_SEC + current.tv_nsec;
}

typedef void (*add_fn)(struct u_atomic_list *,
                       struct u_atomic_link *,
                       struct u_atomic_link *,
                       unsigned);

static void
run_adds(unsigned id, add_fn fn)
{
   unsigned num_elems = TOTAL_ELEMS / num_threads;
   struct u_atomic_link *my_elems = elems + (id * num_elems);

   uint64_t start = gettime_ns();

   for (uint64_t i = 0; i < adds_per_thread; i++) {
      struct u_atomic_link *e = &my_elems[i % num_elems];
      fn(&list, e, e, 1);
   }

   uint64_t end = gettime_ns();

   __sync_fetch_and_add(&cpu_time_ns, end - start);
}

#define DECL_TEST(suffix) \
static int \
run_##suffix(void *state) \
{ \
   run_adds((uintptr_t)state, u_atomic_list_add_list_##suffix); \
   return 0; \
}

#if defined(PIPE_ARCH_X86_64)
DECL_TEST(x86_64)
DECL_TEST(48bit)
#endif
#if defined(U_ATOMIC_LIST_HAVE_DOUBLE_POINTER)
DECL_TEST(dp)
#endif
DECL_TEST(mtx)

static void
run_threads(int (*fn)(void*), const char *name)
{
   thrd_t threads[MAX_NUM_THREADS];

   adds_per_thread = MAX_NUM_ADDS / num_threads;
   uint64_t num_adds = adds_per_thread * num_threads;

   cpu_time_ns = 0;
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

   printf("    %s took %"PRIu64"us (%"PRIu64"ns/add)\n", name,
          cpu_time_ns / 1000, cpu_time_ns / num_adds);
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
#if defined(PIPE_ARCH_X86_64)
      RUN_TEST(x86_64, "x86_64 trampoline");
      RUN_TEST(48bit, "x86_64 48-bit pointers");
#endif
#if defined(U_ATOMIC_LIST_HAVE_DOUBLE_POINTER)
      RUN_TEST(dp, "dual-pointer cmpxchg");
#endif
      RUN_TEST(mtx, "mutex-guarded");
   }

   free(elems);
}
