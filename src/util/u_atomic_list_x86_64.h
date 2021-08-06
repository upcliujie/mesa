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

extern void (*u_atomic_list_add_list_x86_64_fn)(struct u_atomic_list *,
                                                struct u_atomic_link *,
                                                struct u_atomic_link *,
                                                unsigned);
extern struct u_atomic_link *(*u_atomic_list_del_x86_64_fn)(struct u_atomic_list *list,
                                                            bool del_all);

void u_atomic_list_init_x86_64(struct u_atomic_list *list);

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

void u_atomic_list_finish_x86_64(struct u_atomic_list *list);
