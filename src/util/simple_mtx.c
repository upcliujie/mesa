/*
 * Copyright © 2020 Google, Inc.
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

#if defined(HAVE_VALGRIND) && !defined(NDEBUG)
#  include <valgrind.h>
#  include <helgrind.h>
#  include "simple_mtx.h"

void
__simple_mtx_init_post(simple_mtx_t *mtx)
{
   VALGRIND_HG_MUTEX_INIT_POST(mtx, 0);
}

void
__simple_mtx_destroy_pre(simple_mtx_t *mtx)
{
   VALGRIND_HG_MUTEX_DESTROY_PRE(mtx);
}

void
__simple_mtx_lock_pre(simple_mtx_t *mtx)
{
   VALGRIND_HG_MUTEX_LOCK_PRE(mtx, 0);
}

void
__simple_mtx_lock_post(simple_mtx_t *mtx)
{
   VALGRIND_HG_MUTEX_LOCK_POST(mtx);
}

void
__simple_mtx_unlock_pre(simple_mtx_t *mtx)
{
   VALGRIND_HG_MUTEX_UNLOCK_PRE(mtx);
}

void
__simple_mtx_unlock_post(simple_mtx_t *mtx)
{
   VALGRIND_HG_MUTEX_UNLOCK_POST(mtx);
}

#endif
