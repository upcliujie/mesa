/*
 * Copyright © 2019 Broadcom
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

/** @file
 *
 * Implementation of the Near Optimal Linear-Time Instruction Selection
 * algorithm.
 *
 * http://www.cs.cmu.edu/~dkoes/research/CGO08-NOLTIS.pdf
 */

#include "nir_noltis.h"
#include "nir_worklist.h"
#include "util/hash_table.h"
#include "util/set.h"

nir_noltis_node *
nir_noltis_get_node(nir_noltis *noltis, nir_instr *instr)
{
   struct hash_entry *entry = _mesa_hash_table_search(noltis->ht, instr);
   if (!entry)
      return NULL;
   return entry->data;
}

/**
 * Creates a new tile for NOLTIS to select on.
 *
 * The driver should store its private state (the instruction sequence or how
 * to generate it) in *data.
 *
 * nir_noltis_tile_add_interior() should be called on any other instructions
 * that the tile is implementing, and nir_noltis_tile_add_edge() should be
 * called on any NIR instructions whose results the tile needs.
 *
 * The driver should set up tile->cost when it's done (the number being
 * minimized by the algorithm).
 */
nir_noltis_tile *nir_noltis_tile_create(nir_noltis *noltis,
                                        nir_instr *instr,
                                        void *data)
{
   nir_noltis_tile *tile = rzalloc(noltis, nir_noltis_tile);
   nir_noltis_node *node = nir_noltis_get_node(noltis, instr);

   /* Only give us tiles for NIR instructions we're selecting on, please. */
   assert(node);

   tile->noltis = noltis;
   tile->node = node;
   tile->data = data;

   util_dynarray_init(&tile->edge_nodes, tile);
   util_dynarray_init(&tile->interior_nodes, tile);

   util_dynarray_append(&node->matching_tiles, nir_noltis_tile *, tile);

   return tile;
}

void
nir_noltis_tile_add_edge(nir_noltis_tile *tile, nir_instr *instr)
{
   if (!tile)
      return;

   /* Make sure that we don't trivially violate that the NIR SSA tree is a
    * DAG.
    */
   assert(tile->node->instr != instr);

   nir_noltis_node *node = nir_noltis_get_node(tile->noltis, instr);

   if (node)
      util_dynarray_append(&tile->edge_nodes, nir_noltis_node *, node);
}

void
nir_noltis_tile_add_interior(nir_noltis_tile *tile, nir_instr *instr)
{
   if (!tile || tile->node->instr == instr)
      return;

   nir_noltis_node *node = nir_noltis_get_node(tile->noltis, instr);

   /* XXX: Is it dangerous to have an interior node also in the edge list? */
   if (node)
      util_dynarray_append(&tile->interior_nodes, nir_noltis_node *, node);
}

static bool
has_interior_fixed_node(nir_noltis *noltis, nir_noltis_tile *tile)
{
   util_dynarray_foreach(&tile->interior_nodes, nir_noltis_node *, nodep) {
      nir_noltis_node *node = *nodep;
      if (!node || node->fixed)
         return true;
   }

   return false;
}

/* Walks the instruction sequence going from each NIR SSA def to its uses,
 * finding the best tile for each NIR instruction node, counting the costs of
 * the nodes at the tile's edges.
 */
static void
nir_noltis_bottom_up_dp(nir_noltis *noltis)
{
   /* Note: The reverse topological sort is just walking forwards through the
    * instruction sequence.
    */
   nir_foreach_instr(instr, noltis->block) {
      nir_noltis_node *node = nir_noltis_get_node(noltis, instr);

      node->best_cost = ~0;
      util_dynarray_foreach(&node->matching_tiles, nir_noltis_tile *, tilep) {
         nir_noltis_tile *tile = *tilep;

         if (has_interior_fixed_node(noltis, tile))
            continue;

         uint32_t cost = tile->cost;
         util_dynarray_foreach(&tile->edge_nodes, nir_noltis_node *, edge) {
            cost += (*edge)->best_cost;
         }

         if (cost < node->best_cost) {
            node->best_cost = cost;
            node->best_choice = tile;
         }
      }
      /* The driver must always provide a choice for each node. */
      if (!node->best_choice) {
         fprintf(stderr, "Couldn't find a tile for NIR instr: ");
         nir_print_instr(node->instr, stderr);
         fprintf(stderr, "\n");
         assert(node->best_choice);
      }
   }
}

struct nir_noltis_remove_src_from_roots_state {
   nir_noltis *noltis;
   struct set *roots;
};

static bool
remove_src_from_roots(nir_src *src, void *in_state)
{
   struct nir_noltis_remove_src_from_roots_state *state = in_state;

   if (src->is_ssa) {
      nir_instr *parent_instr = src->ssa->parent_instr;
      nir_noltis_node *src_node =
         nir_noltis_get_node(state->noltis, parent_instr);

      /* Keep fixed nodes as DAG heads.  The paper doesn't note this because
       * they don't initialize nodes with fixed state like we do.  This makes
       * sure that a load_const used both inside this block (like in a
       * load_uniform() offset that will probably be covered by the
       * load_uniform) and outside this block actually gets an instruction
       * selected.
       */
      if (src_node && !src_node->fixed)
         _mesa_set_remove_key(state->roots, parent_instr);
   }

   return true;
}

/* Returns the roots of the DAG as a NIR worklist.  There might be a better
 * way, but we can just walk the instructions in order adding it to the set,
 * and removing any SSA nodes that it references.
 *
 * Note that the resulting roots worklist will include any instructions
 * writing NIR regs, since they are implicitly fixed nodes.
 */
static nir_instr_worklist *
nir_noltis_get_dag_roots(nir_noltis *noltis)
{

   struct set *roots = _mesa_pointer_set_create(noltis);
   struct nir_noltis_remove_src_from_roots_state remove_state = {
      .noltis = noltis,
      .roots = roots,
   };

   nir_foreach_instr(instr, noltis->block) {
      nir_noltis_node *node = nir_noltis_get_node(noltis, instr);

      /* Reset the state, given we only want an instr to be visited in the
       * worklist once, but top_down_select is called twice.
       */
      node->selected = false;

      _mesa_set_add(roots, instr);

      nir_foreach_src(instr, remove_src_from_roots, &remove_state);
   }
   nir_instr_worklist *q = nir_instr_worklist_create();
   set_foreach(roots, entry) {
      nir_instr *instr = (void *)entry->key;
      nir_instr_worklist_push_tail(q, instr);
   }

   _mesa_set_destroy(roots, NULL);

   return q;
}

/* Walks the instruction sequence from the bottom up (starting from block
 * outputs and proceeding toward the leaf SSA defs), picking the actual tiles
 * that should be emitted.  NIR instructions completely covered by other tiles
 * will end up with no tile selected by them.
 */
static void
nir_noltis_top_down_select(nir_noltis *noltis)
{
   noltis->matched_tiles = _mesa_pointer_hash_table_create(noltis);

   nir_instr_worklist *q = nir_noltis_get_dag_roots(noltis);

   nir_foreach_instr_in_worklist(instr, q) {
      nir_noltis_node *node = nir_noltis_get_node(noltis, instr);

      if (node->selected)
         continue;

      nir_noltis_tile *best_tile = node->best_choice;
      _mesa_hash_table_insert(noltis->matched_tiles, instr, best_tile);

      util_dynarray_foreach(&best_tile->interior_nodes, nir_noltis_node *,
                            interior_nodep) {
         util_dynarray_append(&(*interior_nodep)->covering_tiles,
                              nir_noltis_tile *, best_tile);
      }

      util_dynarray_foreach(&best_tile->edge_nodes, nir_noltis_node *, edge) {
         nir_instr_worklist_push_tail(q, (*edge)->instr);
      }

      node->selected = true;
   }

   nir_instr_worklist_destroy(q);
}


struct nir_noltis_path_to_root_state {
   nir_noltis_node *leaf;
   nir_noltis_node *node;
   nir_noltis_tile *tile;
   bool found_leaf_through_node;
   bool node_in_path;
};

static bool
nir_noltis_path_to_root_cb(nir_src *src, void *in_state)
{
   struct nir_noltis_path_to_root_state *state = in_state;

   bool saved_node_in_path = state->node_in_path;

   if (!src->is_ssa)
      return true;
   nir_instr *instr = src->ssa->parent_instr;

   /* Make sure that this SSA use is still within the tile. */
   bool in_tile = false;
   util_dynarray_foreach(&state->tile->interior_nodes, nir_noltis_node *,
                         nodep) {
      if ((*nodep)->instr == instr) {
         in_tile = true;
         break;
      }
   }
   if (!in_tile)
      return true;

   if (instr == state->node->instr)
      state->node_in_path = true;
   if (instr == state->leaf->instr) {
      if (state->node_in_path)
         state->found_leaf_through_node = true;
      return true;
   }

   nir_foreach_src(instr, nir_noltis_path_to_root_cb, state);

   state->node_in_path = saved_node_in_path;

   return true;
}

/**
 * Walks from @tile's root through the SSA uses toward the edges of the tile,
 * seeing if we find @node on the way to @leaf.
 */
static bool
nir_noltis_path_to_tile_root_contains(nir_noltis_node *leaf,
                                      nir_noltis_tile *tile,
                                      nir_noltis_node *node)
{
   struct nir_noltis_path_to_root_state state = {
      .leaf = leaf,
      .node = node,
      .tile = tile,
      .found_leaf_through_node = false,
      .node_in_path = false,
   };

   nir_foreach_src(tile->node->instr, nir_noltis_path_to_root_cb, &state);

   return state.found_leaf_through_node;
}

/**
 * For a node with overlapping tiles, count up the cost of the tiles starting
 * from the nodes
 */
static uint32_t
nir_noltis_get_overlap_cost(nir_noltis_node *node)
{
   uint32_t cost = 0;
   struct set *seen = _mesa_pointer_set_create(NULL);
   struct util_dynarray q;
   util_dynarray_init(&q, NULL);

   util_dynarray_foreach(&node->covering_tiles, nir_noltis_tile *, tilep) {
      nir_noltis_tile *tile = *tilep;
      util_dynarray_append(&q, nir_noltis_tile *, tile);
      _mesa_set_add(seen, tile);
   }

   while (util_dynarray_num_elements(&q, nir_noltis_tile *) != 0) {
      nir_noltis_tile *tile = util_dynarray_pop(&q, nir_noltis_tile *);
      cost += tile->cost;

      util_dynarray_foreach(&tile->edge_nodes, nir_noltis_node *, edgep) {
         nir_noltis_node *edge = *edgep;
         nir_noltis_tile *edge_tile = edge->best_choice;

         /* The "reachable" test in the pseudocode is this simple, because we
          * don't put tiles past the shared node into the worklist.
          */
         if (nir_noltis_path_to_tile_root_contains(edge, tile, node)) {
            if (util_dynarray_num_elements(&edge->covering_tiles,
                                           nir_noltis_tile *) == 1)
               cost += edge_tile->cost;
         } else if (!_mesa_set_search(seen, edge_tile)) {
            /* Note that in the pseudocode of the paper, this is indented to the
             * level above.  However, the description says we're trying to count
             * the cost of the tree of tiles overlapping node without double
             * counting areas where the tile trees do *not* overlap, while the
             * block above is for the overlap.
             */
            util_dynarray_append(&q, nir_noltis_tile *, edge_tile);
            _mesa_set_add(seen, edge_tile);
         }
      }
   }

   ralloc_free(seen);
   util_dynarray_fini(&q);

   return cost;
}

/* Find the minimum cost for replacing the given tile with a second-choice
 * tile that turns *node into an edge instead of an interior node.
 */
static uint32_t
nir_noltis_get_tile_cut_cost(nir_noltis_tile *tile, nir_noltis_node *node)
{
   uint32_t best_cost = ~0;

   nir_noltis_node *r = node;
   util_dynarray_foreach(&r->matching_tiles, nir_noltis_tile *, tilep) {
      nir_noltis_tile *tile = *tilep;

      bool has_node_as_edge = false;
      util_dynarray_foreach(&tile->edge_nodes, nir_noltis_node *, edge) {
         if (*edge == node)
            has_node_as_edge = true;
      }
      if (!has_node_as_edge)
         continue;

      uint32_t cost = tile->cost;
      util_dynarray_foreach(&tile->edge_nodes, nir_noltis_node *, edge) {
         if (*edge != node)
            cost += (*edge)->best_cost;
      }

      cost = MIN2(cost, best_cost);
   }

   util_dynarray_foreach(&tile->edge_nodes, nir_noltis_node *, edgep) {
      nir_noltis_node *edge = *edgep;
      if (!nir_noltis_path_to_tile_root_contains(edge, tile, node)) {
         best_cost -= edge->best_cost;
      }
   }

   return best_cost;
}

static void
nir_noltis_improve_cse_decisions(nir_noltis *noltis)
{
   hash_table_foreach(noltis->ht, entry) {
      nir_noltis_node *node = entry->data;

      if (!node->shared)
         continue;

      if (util_dynarray_num_elements(&node->covering_tiles,
                                     nir_noltis_tile *) <= 1)
         continue;

      uint32_t overlap_cost = nir_noltis_get_overlap_cost(node);
      uint32_t cse_cost = node->best_cost;

      util_dynarray_foreach(&node->covering_tiles, nir_noltis_tile *, tile)
         cse_cost += nir_noltis_get_tile_cut_cost(*tile, node);

      if (cse_cost < overlap_cost)
         node->fixed = true;
   }
}

static void
nir_noltis_check_def_fixed(nir_noltis *noltis, nir_ssa_def *ssa)
{
   nir_noltis_node *node = nir_noltis_get_node(noltis, ssa->parent_instr);
   nir_foreach_use(src, ssa) {
      nir_instr *use_instr = src->parent_instr;
      if (!nir_noltis_get_node(noltis, use_instr))
         node->fixed = true;
   }

   /* XXX: At some point we probably want to handle the IF ending a block in
    * NOLTIS.
    */
   nir_foreach_if_use(src, ssa)
      node->fixed = true;
}

/* Sets the initial "fixed" state if the dest is a NIR reg (so it must be
 * written, not folded in other instructions), or involves a use outside of
 * the block covered by this NOLTIS invocation.
 */
static bool
nir_noltis_dest_check_fixed(nir_dest *dest, void *state)
{
   nir_noltis *noltis = state;

   if (!dest->is_ssa) {
      nir_noltis_node *node = nir_noltis_get_node(noltis,
                                                  dest->reg.parent_instr);
      node->fixed = true;
      return true;
   }

   nir_noltis_check_def_fixed(noltis, &dest->ssa);

   return true;
}

static bool
nir_noltis_def_check_fixed_shared(nir_ssa_def *def, void *state)
{
   nir_noltis *noltis = state;
   nir_noltis_node *node = nir_noltis_get_node(noltis, def->parent_instr);
   int uses = 0;

   nir_noltis_check_def_fixed(noltis, def);

   nir_foreach_use(src, def)
      uses++;

   nir_foreach_if_use(src, def)
      uses++;

   if (uses > 1)
      node->shared = true;

   return true;
}

/**
 * Initialize the tracking structure for this NOLTIS invocation.
 *
 * After this, the driver should add matching tiles for the instructions in
 * the block, then call nir_noltis_solve(), then walk over the NIR
 * instructions and emit the ones in noltis->matched_tiles.
 *
 * When you're done, the struct may be freed with ralloc_free().
 */
nir_noltis *
nir_noltis_create(void *mem_ctx, nir_block *block)
{
   nir_noltis *noltis = rzalloc(mem_ctx, nir_noltis);

   noltis->block = block;
   noltis->ht = _mesa_pointer_hash_table_create(noltis);

   /* Create the tracking node for each instruction */
   nir_foreach_instr(instr, noltis->block) {
      nir_noltis_node *node = rzalloc(noltis, nir_noltis_node);

      node->instr = instr;
      util_dynarray_init(&node->matching_tiles, node);
      util_dynarray_init(&node->covering_tiles, node);

      _mesa_hash_table_insert(noltis->ht, instr, node);
   }

   /* Find the nodes that are used outside of this block and mark their
    * initial fixed and shared state.  We don't do instruction selection
    * across blocks.
    */
   nir_foreach_instr(instr, noltis->block) {
      /* There should be only 0 or 1 dest, but this is how to generically get
       * at it.
       */
      nir_foreach_dest(instr, nir_noltis_dest_check_fixed, noltis);
      nir_foreach_ssa_def(instr, nir_noltis_def_check_fixed_shared, noltis);
   }

   return noltis;
}

static bool
nir_noltis_validate_src_cb(nir_src *src, void *in_state)
{
   struct set *set = in_state;

   if (!src->is_ssa)
      return true;

   _mesa_set_add(set, src->ssa->parent_instr);

   return true;
}

static void
nir_noltis_validate(nir_noltis *noltis)
{
#ifdef NDEBUG
   return;
#endif

   struct set *reachable_srcs = _mesa_pointer_set_create(NULL);

   nir_foreach_instr(instr, noltis->block) {
      nir_noltis_node *node = nir_noltis_get_node(noltis, instr);

      util_dynarray_foreach(&node->matching_tiles, nir_noltis_tile *, tilep) {
         nir_noltis_tile *tile = *tilep;

         /* Sanity-check the interior nodes list: If an edge isn't reachable
          * from the instr or an interior node of the tile, then you've
          * definitely forgotten one.  This won't catch missing interior nodes
          * deeper in the tree, but will probably cover the common case.
          */
         nir_foreach_src(instr, nir_noltis_validate_src_cb, reachable_srcs);
         util_dynarray_foreach(&tile->interior_nodes, nir_noltis_node *,
                               interiorp) {
            nir_noltis_node *interior = *interiorp;
            nir_foreach_src(interior->instr, nir_noltis_validate_src_cb,
                            reachable_srcs);
         }

         util_dynarray_foreach(&tile->edge_nodes, nir_noltis_node *, edgep) {
            assert(_mesa_set_search(reachable_srcs, (*edgep)->instr) != NULL);
         }

         _mesa_set_clear(reachable_srcs, NULL);
      }
   }

   _mesa_set_destroy(reachable_srcs, NULL);
}

void
nir_noltis_select(nir_noltis *noltis)
{
   nir_noltis_validate(noltis);

   nir_noltis_bottom_up_dp(noltis);
   nir_noltis_top_down_select(noltis);
   nir_noltis_improve_cse_decisions(noltis);
   nir_noltis_bottom_up_dp(noltis);
   nir_noltis_top_down_select(noltis);
}

nir_noltis_tile *
nir_noltis_get_tile(nir_noltis *noltis, nir_instr *instr)
{
   struct hash_entry *entry = _mesa_hash_table_search(noltis->matched_tiles,
                                                      instr);
   if (entry)
      return entry->data;
   return NULL;
}
