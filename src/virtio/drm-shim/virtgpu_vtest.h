/*
 * Copyright © 2022 Google LLC
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __VIRTGPU_VTEST_H__
#define __VIRTGPU_VTEST_H__

#include "util/simple_mtx.h"

struct vtest {
   int sock_fd;
   simple_mtx_t lock;
   unsigned protocol_version;
};

struct vtest *vtest_connect(void);

int vtest_write(struct vtest *v, const void *buf, int size);
int vtest_read(struct vtest *v, void *buf, int size);
int vtest_receive_fd(struct vtest *v);

static inline void
vtest_lock(struct vtest *v)
{
   simple_mtx_lock(&v->lock);
}

static inline void
vtest_unlock(struct vtest *v)
{
   simple_mtx_unlock(&v->lock);
}

#endif /*  __VIRTGPU_VTEST_H__ */
