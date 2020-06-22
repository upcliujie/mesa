/*
 * Copyright © 2020 Intel Corporation
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

#ifndef MESA_BRANCH_AND_BOUND_H
#define MESA_BRANCH_AND_BOUND_H

#include <inttypes.h>
#include "util/list.h"

struct candidate_node_vtable;

struct candidate_node {
   struct list_head link;

   const struct candidate_node_vtable *vtable;

   uint64_t cost_so_far;
   uint64_t cost_lower_bound;

   bool is_solution;
};

struct candidate_node_vtable {
   void (*dtor)(struct candidate_node *);

   void (*generate_branches)(struct candidate_node *,
                             struct list_head *branches,
                             void *state);
};

struct candidate_node *
_mesa_branch_and_bound_solve(struct candidate_node *start, void *state);

#endif /* MESA_BRANCH_AND_BOUND_H */
