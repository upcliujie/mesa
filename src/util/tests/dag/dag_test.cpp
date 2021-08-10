/*
 * Copyright © 2021 Google, Inc.
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

#include <gtest/gtest.h>
#include "util/dag.h"

struct node {
   struct dag_node dag;
   int val;

   /* Overload >> to make describing our test case graphs easier to read */
   struct node &operator>>(struct node &child) {
      dag_add_edge(&dag, &child.dag, NULL);
      return child;
   }
};

static void output_cb(struct dag_node *dag_node, void *data)
{
   struct node *node = container_of(dag_node, struct node, dag);
   struct util_dynarray *output = (struct util_dynarray *)data;
   util_dynarray_append(output, int, node->val);
}

static void
init_nodes(struct dag *dag, struct node *nodes, unsigned num_nodes)
{
   for (unsigned i = 0; i < num_nodes; i++) {
      dag_init_node(dag, &nodes[i].dag);
      nodes[i].val = i;
   }
}

/* Since C/C++ can copy structs with '=' but not arrays, wrap in a struct */
#define F(...) ({ result_type r = {{__VA_ARGS__}}; r; })

#define TEST_INIT(num_nodes)                             \
   typedef struct { int order[num_nodes]; } result_type; \
   void *mem_ctx = ralloc_context(NULL);                 \
   struct dag *dag = dag_create(mem_ctx);                \
   struct util_dynarray expect, actual;                  \
   util_dynarray_init(&expect, mem_ctx);                 \
   util_dynarray_init(&actual, mem_ctx);                 \
   struct node node[(num_nodes)];                        \
   init_nodes(dag, node, (num_nodes))

#define TEST_FINI() ralloc_free(mem_ctx)

#define TEST_CHECK() do {                                                    \
   EXPECT_EQ(util_dynarray_num_elements(&expect, int),                       \
             util_dynarray_num_elements(&actual, int));                      \
                                                                             \
   for (unsigned i = 0; i < util_dynarray_num_elements(&expect, int); i++) { \
      EXPECT_EQ(util_dynarray_pop(&expect, int),                             \
                util_dynarray_pop(&actual, int));                            \
   }                                                                         \
} while (0)

TEST(dag, basic)
{
   TEST_INIT(3);

   /*     0
    *    / \
    *   1   2
    */
   node[0] >> node[1];
   node[0] >> node[2];

   /* Expected traversal order: [1, 2, 0] */
   util_dynarray_append(&expect, result_type, F(1, 2, 0));

   dag_traverse_bottom_up(dag, output_cb, &actual);

   TEST_CHECK();

   TEST_FINI();
}

TEST(dag, basic_many_children)
{
   TEST_INIT(6);

   /*     _ 0 _
    *    / /|\ \
    *   / / | \ \
    *  |  | | |  |
    *  1  2 3 4  5
    */
   node[0] >> node[1];
   node[0] >> node[2];
   node[0] >> node[3];
   node[0] >> node[4];
   node[0] >> node[5];

   /* Expected traversal order: [1, 2, 3, 4, 5, 0] */
   util_dynarray_append(&expect, result_type, F(1, 2, 3, 4, 5, 0));

   dag_traverse_bottom_up(dag, output_cb, &actual);

   TEST_CHECK();

   TEST_FINI();
}

TEST(dag, basic_many_parents)
{
   TEST_INIT(7);

   /*     _ 0 _
    *    / /|\ \
    *   / / | \ \
    *  |  | | |  |
    *  1  2 3 4  5
    *  |  | | |  |
    *   \ \ | / /
    *    \ \|/ /
    *     ‾ 6 ‾
    */
   node[0] >> node[1] >> node[6];
   node[0] >> node[2] >> node[6];
   node[0] >> node[3] >> node[6];
   node[0] >> node[4] >> node[6];
   node[0] >> node[5] >> node[6];

   /* Expected traversal order: [6, 1, 2, 3, 4, 5, 0] */
   util_dynarray_append(&expect, result_type, F(6, 1, 2, 3, 4, 5, 0));

   dag_traverse_bottom_up(dag, output_cb, &actual);

   TEST_CHECK();

   TEST_FINI();
}

TEST(dag, complex)
{
   TEST_INIT(5);

   /*     0
    *    / \
    *   1   3
    *  / \  |
    * 2  |  /
    *  \ / /
    *   4 ‾
    */
   node[0] >> node[1] >> node[2] >> node[4];
   node[1] >> node[4];
   node[0] >> node[3];
   node[3] >> node[4];

   /* Expected traversal order: [4, 2, 1, 3, 0] */
   util_dynarray_append(&expect, result_type, F(4, 2, 1, 3, 0));

   dag_traverse_bottom_up(dag, output_cb, &actual);

   TEST_CHECK();

   TEST_FINI();
}
