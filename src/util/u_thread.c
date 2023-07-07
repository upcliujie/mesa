/*
 * Copyright 1999-2006 Brian Paul
 * Copyright 2008 VMware, Inc.
 * Copyright 2020 Lag Free Games, LLC
 * Copyright 2022 Yonggang Luo
 * SPDX-License-Identifier: MIT
 */

#include "util/u_thread.h"

#include "macros.h"

#ifdef HAVE_PTHREAD
#include <signal.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif
#endif

#ifdef __HAIKU__
#include <OS.h>
#endif

#if DETECT_OS_LINUX && !defined(ANDROID)
#include <sched.h>
#elif defined(_WIN32) && !defined(HAVE_PTHREAD)
#include <windows.h>
#endif

#ifdef __FreeBSD__
/* pthread_np.h -> sys/param.h -> machine/param.h
 * - defines ALIGN which clashes with our ALIGN
 */
#undef ALIGN
#define cpu_set_t cpuset_t
#endif

int
util_get_current_cpu(void)
{
#if DETECT_OS_LINUX && !defined(ANDROID)
   return sched_getcpu();

#elif defined(_WIN32) && !defined(HAVE_PTHREAD)
   return GetCurrentProcessorNumber();

#else
   return -1;
#endif
}

int u_thread_create(thrd_t *thrd, int (*routine)(void *), void *param)
{
   int ret = thrd_error;
#ifdef HAVE_PTHREAD
   sigset_t saved_set, new_set;

   sigfillset(&new_set);
   sigdelset(&new_set, SIGSYS);

   /* SIGSEGV is commonly used by Vulkan API tracing layers in order to track
    * accesses in device memory mapped to user space. Blocking the signal hinders
    * that tracking mechanism.
    */
   sigdelset(&new_set, SIGSEGV);
   pthread_sigmask(SIG_BLOCK, &new_set, &saved_set);
   ret = thrd_create(thrd, routine, param);
   pthread_sigmask(SIG_SETMASK, &saved_set, NULL);
#else
   ret = thrd_create(thrd, routine, param);
#endif

   return ret;
}

void u_thread_setname( const char *name )
{
#if defined(HAVE_PTHREAD)
#if DETECT_OS_LINUX || DETECT_OS_CYGWIN || DETECT_OS_SOLARIS || defined(__GLIBC__)
   int ret = pthread_setname_np(pthread_self(), name);
   if (ret == ERANGE) {
      char buf[16];
      const size_t len = MIN2(strlen(name), ARRAY_SIZE(buf) - 1);
      memcpy(buf, name, len);
      buf[len] = '\0';
      pthread_setname_np(pthread_self(), buf);
   }
#elif DETECT_OS_FREEBSD || DETECT_OS_OPENBSD
   pthread_set_name_np(pthread_self(), name);
#elif DETECT_OS_NETBSD
   pthread_setname_np(pthread_self(), "%s", (void *)name);
#elif DETECT_OS_APPLE
   pthread_setname_np(name);
#elif DETECT_OS_HAIKU
   rename_thread(find_thread(NULL), name);
#else
#warning Not sure how to call pthread_setname_np
#endif
#endif
   (void)name;
}

bool
util_set_thread_affinity(thrd_t thread,
                         const uint32_t *mask,
                         uint32_t *old_mask,
                         unsigned num_mask_bits)
{
#if defined(HAVE_PTHREAD_SETAFFINITY)
   cpu_set_t cpuset;

   if (old_mask) {
      if (pthread_getaffinity_np(thread, sizeof(cpuset), &cpuset) != 0)
         return false;

      memset(old_mask, 0, num_mask_bits / 8);
      for (unsigned i = 0; i < num_mask_bits && i < CPU_SETSIZE; i++) {
         if (CPU_ISSET(i, &cpuset))
            old_mask[i / 32] |= 1u << (i % 32);
      }
   }

   CPU_ZERO(&cpuset);
   for (unsigned i = 0; i < num_mask_bits && i < CPU_SETSIZE; i++) {
      if (mask[i / 32] & (1u << (i % 32)))
         CPU_SET(i, &cpuset);
   }
   return pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0;

#elif defined(_WIN32) && !defined(HAVE_PTHREAD)
   DWORD_PTR m = mask[0];

   if (sizeof(m) > 4 && num_mask_bits > 32)
      m |= (uint64_t)mask[1] << 32;

   m = SetThreadAffinityMask(thread.handle, m);
   if (!m)
      return false;

   if (old_mask) {
      memset(old_mask, 0, num_mask_bits / 8);

      old_mask[0] = m;
#ifdef _WIN64
      old_mask[1] = m >> 32;
#endif
   }

   return true;
#else
   return false;
#endif
}

int64_t
util_thread_get_time_nano(thrd_t thread)
{
#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__)
   struct timespec ts;
   clockid_t cid;

   pthread_getcpuclockid(thread, &cid);
   clock_gettime(cid, &ts);
   return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#elif defined(_WIN32)
   union {
      FILETIME time;
      ULONGLONG value;
   } kernel_time, user_time;
   GetThreadTimes((HANDLE)thread.handle, NULL, NULL, &kernel_time.time, &user_time.time);
   return (kernel_time.value + user_time.value) * 100;
#else
   (void)thread;
   return 0;
#endif
}

int
util_mtx_monotonic_init(util_mtx_monotonic *mtx, int type)
{
   assert(mtx != NULL);
   if (type != mtx_plain &&
      type != mtx_timed &&
      type != (mtx_plain | mtx_recursive) &&
      type != (mtx_timed | mtx_recursive))
      return thrd_error;
#if defined(HAVE_PTHREAD)
   mtx->mtx = malloc(sizeof(pthread_mutex_t));
   if (mtx->mtx == NULL) {
      return thrd_nomem;
   }
   pthread_mutexattr_t attr;
   if ((type & mtx_recursive) == 0) {
      pthread_mutex_init((pthread_mutex_t *)mtx->mtx, NULL);
      return thrd_success;
   }

   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init((pthread_mutex_t *)mtx->mtx, &attr);
   pthread_mutexattr_destroy(&attr);
   return thrd_success;
#elif defined(_WIN32)
   mtx->mtx = malloc(sizeof(CRITICAL_SECTION));
   if (mtx->mtx == NULL) {
      return thrd_nomem;
   }
   InitializeCriticalSection((PCRITICAL_SECTION)mtx->mtx);
   return thrd_success;
#else
#error Not supported on this platform.
#endif
}

void
util_mtx_monotonic_destroy(util_mtx_monotonic *mtx)
{
   if (mtx != NULL && mtx->mtx != NULL) {
#if defined(HAVE_PTHREAD)
      pthread_mutex_destroy((pthread_mutex_t *)mtx->mtx);
#elif defined(_WIN32)
      DeleteCriticalSection((PCRITICAL_SECTION)mtx->mtx);
#endif
      free(mtx->mtx);
      mtx->mtx = NULL;
   }
}

int
util_mtx_monotonic_lock(util_mtx_monotonic *mtx)
{
   assert(mtx != NULL);
#if defined(HAVE_PTHREAD)
   return (pthread_mutex_lock((pthread_mutex_t *)mtx->mtx) == 0) ? thrd_success : thrd_error;
#elif defined(_WIN32)
   EnterCriticalSection((PCRITICAL_SECTION)mtx->mtx);
   return thrd_success;
#endif
}

int
util_mtx_monotonic_trylock(util_mtx_monotonic *mtx)
{
   assert(mtx != NULL);
#if defined(HAVE_PTHREAD)
   return (pthread_mutex_trylock((pthread_mutex_t *)mtx->mtx) == 0) ? thrd_success : thrd_busy;
#elif defined(_WIN32)
   return TryEnterCriticalSection((PCRITICAL_SECTION)mtx->mtx) ? thrd_success : thrd_busy;
#endif
}

int
util_mtx_monotonic_unlock(util_mtx_monotonic *mtx)
{
   assert(mtx != NULL);
#if defined(HAVE_PTHREAD)
   return (pthread_mutex_unlock((pthread_mutex_t *)mtx->mtx) == 0) ? thrd_success : thrd_error;
#elif defined(_WIN32)
   LeaveCriticalSection((PCRITICAL_SECTION)mtx->mtx);
   return thrd_success;
#endif
}

int
util_cnd_monotonic_init(util_cnd_monotonic *cond)
{
   assert(cond != NULL);

#if defined(HAVE_PTHREAD)
   int ret = thrd_error;
   pthread_condattr_t condattr;
   cond->cond = malloc(sizeof(pthread_cond_t));
   if (cond->cond == NULL) {
      return thrd_nomem;
   }
   if (pthread_condattr_init(&condattr) == 0) {
      if ((pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC) == 0) &&
          (pthread_cond_init((pthread_cond_t *)cond->cond, &condattr) == 0)) {
         ret = thrd_success;
      }

      pthread_condattr_destroy(&condattr);
   }
   if (ret != thrd_success) {
      free(cond->cond);
      cond->cond = NULL;
   }
   return ret;
#elif defined(_WIN32)
   cond->cond = malloc(sizeof(CONDITION_VARIABLE));
   if (cond->cond == NULL) {
      return thrd_nomem;
   }
   InitializeConditionVariable((PCONDITION_VARIABLE)cond->cond);
   return thrd_success;
#else
#error Not supported on this platform.
#endif
}

void
util_cnd_monotonic_destroy(util_cnd_monotonic *cond)
{
   if (cond != NULL && cond->cond != NULL) {
#if defined(HAVE_PTHREAD)
      pthread_cond_destroy((pthread_cond_t *)cond->cond);
#elif defined(_WIN32)
      /* Do nothing*/
#endif
      free(cond->cond);
      cond->cond = NULL;
   }
}

int
util_cnd_monotonic_broadcast(util_cnd_monotonic *cond)
{
   assert(cond != NULL);

#if defined(HAVE_PTHREAD)
   return (pthread_cond_broadcast((pthread_cond_t *)cond->cond) == 0) ? thrd_success : thrd_error;
#elif defined(_WIN32)
   WakeAllConditionVariable((PCONDITION_VARIABLE)cond->cond);
   return thrd_success;
#endif
}

int
util_cnd_monotonic_signal(util_cnd_monotonic *cond)
{
   assert(cond != NULL);

#if defined(HAVE_PTHREAD)
   return (pthread_cond_signal((pthread_cond_t *)cond->cond) == 0) ? thrd_success : thrd_error;
#elif defined(_WIN32)
   WakeConditionVariable((PCONDITION_VARIABLE)cond->cond);
   return thrd_success;
#endif
}

int
util_cnd_monotonic_timedwait(util_cnd_monotonic *cond, util_mtx_monotonic *mtx,
                             const struct timespec *abs_time)
{
   assert(cond != NULL);
   assert(mtx != NULL);
   assert(abs_time != NULL);

#if defined(HAVE_PTHREAD)
   int rt =
      pthread_cond_timedwait((pthread_cond_t *)cond->cond, (pthread_mutex_t *)mtx->mtx, abs_time);
   if (rt == ETIMEDOUT)
      return thrd_timedout;
   return (rt == 0) ? thrd_success : thrd_error;
#elif defined(_WIN32)
   const int64_t future = (abs_time->tv_sec * 1000LL) + (abs_time->tv_nsec / 1000000LL);
   struct timespec now_ts;
   if (timespec_get(&now_ts, TIME_MONOTONIC) != TIME_MONOTONIC) {
      return thrd_error;
   }
   const int64_t now = (now_ts.tv_sec * 1000LL) + (now_ts.tv_nsec / 1000000LL);
   const DWORD timeout = (future > now) ? (DWORD)(future - now) : 0;
   if (SleepConditionVariableCS((PCONDITION_VARIABLE)cond->cond, (PCRITICAL_SECTION)mtx->mtx,
                                timeout))
      return thrd_success;
   return (GetLastError() == ERROR_TIMEOUT) ? thrd_timedout : thrd_error;
#endif
}

int
util_cnd_monotonic_wait(util_cnd_monotonic *cond, util_mtx_monotonic *mtx)
{
   assert(cond != NULL);
   assert(mtx != NULL);

#if defined(HAVE_PTHREAD)
   return (pthread_cond_wait((pthread_cond_t *)cond->cond, (pthread_mutex_t *)mtx->mtx) == 0)
             ? thrd_success
             : thrd_error;
#elif defined(_WIN32)
   SleepConditionVariableCS((PCONDITION_VARIABLE)cond->cond, (PCRITICAL_SECTION)mtx->mtx, INFINITE);
   return thrd_success;
#endif
}

#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__)

void util_barrier_init(util_barrier *barrier, unsigned count)
{
   pthread_barrier_init(barrier, NULL, count);
}

void util_barrier_destroy(util_barrier *barrier)
{
   pthread_barrier_destroy(barrier);
}

bool util_barrier_wait(util_barrier *barrier)
{
   return pthread_barrier_wait(barrier) == PTHREAD_BARRIER_SERIAL_THREAD;
}

#else /* If the OS doesn't have its own, implement barriers using a mutex and a condvar */

void util_barrier_init(util_barrier *barrier, unsigned count)
{
   barrier->count = count;
   barrier->waiters = 0;
   barrier->sequence = 0;
   (void) mtx_init(&barrier->mutex, mtx_plain);
   cnd_init(&barrier->condvar);
}

void util_barrier_destroy(util_barrier *barrier)
{
   assert(barrier->waiters == 0);
   mtx_destroy(&barrier->mutex);
   cnd_destroy(&barrier->condvar);
}

bool util_barrier_wait(util_barrier *barrier)
{
   mtx_lock(&barrier->mutex);

   assert(barrier->waiters < barrier->count);
   barrier->waiters++;

   if (barrier->waiters < barrier->count) {
      uint64_t sequence = barrier->sequence;

      do {
         cnd_wait(&barrier->condvar, &barrier->mutex);
      } while (sequence == barrier->sequence);
   } else {
      barrier->waiters = 0;
      barrier->sequence++;
      cnd_broadcast(&barrier->condvar);
   }

   mtx_unlock(&barrier->mutex);

   return true;
}

#endif
