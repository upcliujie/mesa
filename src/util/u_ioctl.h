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

#ifndef U_IOCTL_H
#define U_IOCTL_H

#include "macros.h"

#include <errno.h>
#include <sys/ioctl.h>

static inline int MUST_CHECK
u_ioctl(int fd, unsigned long request, void *arg)
{
   int ret = ioctl(fd, request, arg);

   /* The Linux man page for ioctl(2) says:
    *
    *    "Usually, on success zero is returned.  A few ioctl() requests use
    *    the return value as an output parameter and return a nonnegative
    *    value on success.  On error, -1 is returned, and errno is set
    *    appropriately."
    *
    * The POSIX spec for ioctl says:
    *
    *    "Upon successful completion, ioctl() shall return a value other than
    *    -1 that depends upon the STREAMS device control function. Otherwise,
    *    it shall return -1 and set errno to indicate the error."
    *
    * It's the job of the caller to know whether or not its ioctl falls into
    * one of the weird edge cases allowed by the POSIX spec of returning a
    * userful negative value that isn't just -errno and avoiding this helper
    * in that case.
    */
   if (ret == -1)
      return -errno;

   return ret;
}

static inline int MUST_CHECK
u_ioctl_retry(int fd, unsigned long request, void *arg)
{
   int ret;

   do {
      ret = u_ioctl(fd, request, arg);
   } while (ret == -EINTR || ret == -EAGAIN);

   return ret;
}

static void
u_ioctl_assert(int fd, unsigned long request, void *arg)
{
   ASSERTED int ret = u_ioctl_retry(fd, request, arg);
   assert(ret == 0);
}

#endif /* U_IOCTL_H */
