/*
 * Copyright © 2014 Intel Corporation
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
 *
 * Authors:
 *    Connor Abbott (cwabbott0@gmail.com)
 *
 */

#ifndef NIR_CONTROL_FLOW_H
#define NIR_CONTROL_FLOW_H

#include "nir.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NIR Control Flow Modification
 *
 * This file contains various APIs that make modifying control flow in NIR,
 * while maintaining the invariants checked by the validator, much easier.
 * There are two parts to this:
 *
 * 1. Inserting control flow (ifs and loops) in various places, for creating
 *    IR either from scratch or as part of some lowering pass.
 * 2. Taking existing pieces of the IR and either moving them around or
 *    deleting them.
 */

/* Control flow insertion. */

/** puts a control flow node where the cursor is */
void nir_cf_node_insert(nir_cursor cursor, nir_cf_node *node);

/** puts a control flow node immediately after another control flow node */
static inline void
nir_cf_node_insert_after(nir_cf_node *node, nir_cf_node *after)
{
   nir_cf_node_insert(nir_after_cf_node(node), after);
}

/** puts a control flow node immediately before another control flow node */
static inline void
nir_cf_node_insert_before(nir_cf_node *node, nir_cf_node *before)
{
   nir_cf_node_insert(nir_before_cf_node(node), before);
}

/** puts a control flow node at the beginning of a list from an if, loop, or function */
static inline void
nir_cf_node_insert_begin(struct exec_list *list, nir_cf_node *node)
{
   nir_cf_node_insert(nir_before_cf_list(list), node);
}

/** puts a control flow node at the end of a list from an if, loop, or function */
static inline void
nir_cf_node_insert_end(struct exec_list *list, nir_cf_node *node)
{
   nir_cf_node_insert(nir_after_cf_list(list), node);
}


/* Control flow motion.
 *
 * These functions let you take a part of a control flow list (basically
 * equivalent to a series of statement in GLSL) and "extract" it from the IR,
 * so that it's a free-floating piece of IR that can be either re-inserted
 * somewhere else or deleted entirely.
 *
 * There are several caveats on these functions, see the docs for more
 * information.
 */

/**
 * An opaque wrapper for a portion of a CF list that has been extracted from
 * a function.
 */
typedef struct {
   struct exec_list list;
   nir_function_impl *impl; /* for cleaning up if the list is deleted */
} nir_cf_list;

/** Extract a piece of control flow from a function.
 *
 * \p begin and \p end must be inside blocks in the same CF list, and
 * \p begin must be before \p end. If a NIR CF list corresponds to a list
 * of statements in GLSL, then the portion between \p begin and \p end
 * then corresponds to a sub-list within that list, which is extracted into
 * \p extracted which is a free-floating piece of IR that can later be
 * deleted, cloned, or re-inserted.
 *
 * This function splits up the basic blocks at both \p begin and \p end, and
 * it is left unspecified how they are split up. This means that any pointers
 * to those blocks are invalid after the function is called.
 */
void nir_cf_extract(nir_cf_list *extracted, nir_cursor begin, nir_cursor end);

/**
 * Re-insert a ::nir_cf_list which has been extracted by nir_cf_extract() at
 * the cursor. Any pointer to the block that \p cursor is in is similarly
 * invalidated.
 */
void nir_cf_reinsert(nir_cf_list *cf_list, nir_cursor cursor);

/** Delete a ::nir_cf_list which has been extracted by nir_cf_extract(). */
void nir_cf_delete(nir_cf_list *cf_list);

/** Clone a ::nir_cf_list which has been extracted by nir_cf_extract().
 *
 * @param dst The cloned ::nir_cf_list.
 * @param src The ::nir_cf_list to clone.
 * @param parent The ::nir_cf_node \p dst will be inserted under.
 * @param remap_table A table of SSA values used to rewrite uses of values
 *                    when cloning. If a value is in this table, uses of it
 *                    will be rewritten. Otherwise, values outside \p src
 *                    will be kept as-is.
 */
void nir_cf_list_clone(nir_cf_list *dst, nir_cf_list *src, nir_cf_node *parent,
                       struct hash_table *remap_table);

static inline void
nir_cf_list_clone_and_reinsert(nir_cf_list *src_list, nir_cf_node *parent,
                               nir_cursor cursor,
                               struct hash_table *remap_table)
{
   nir_cf_list list;
   nir_cf_list_clone(&list, src_list, parent, remap_table);
   nir_cf_reinsert(&list, cursor);
}

/** Extract an entire CF list. */
static inline void
nir_cf_list_extract(nir_cf_list *extracted, struct exec_list *cf_list)
{
   nir_cf_extract(extracted, nir_before_cf_list(cf_list),
                  nir_after_cf_list(cf_list));
}

/** Removes a control flow node, doing any cleanup necessary. */
static inline void
nir_cf_node_remove(nir_cf_node *node)
{
   nir_cf_list list;
   nir_cf_extract(&list, nir_before_cf_node(node), nir_after_cf_node(node));
   nir_cf_delete(&list);
}

/** inserts undef phi sources from predcessor into phis of the block */
void nir_insert_phi_undef(nir_block *block, nir_block *pred);

#ifdef __cplusplus
}
#endif

#endif /* NIR_CONTROL_FLOW_H */
