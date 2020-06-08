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

/**
 * \file combine_constants.c
 * Generate a minimal set of constants to cover all values in an input set.
 *
 * This implementation is independent of the unerlying IR.  Callers of
 * \c util_combine_constants supply a set of values to be "combined" and a
 * description of how the values are used.  This description includes:
 *
 * * The raw value and its bit size.
 *
 * * The instruction and source position of the use of the value.
 *
 * * Description of the ways in which the instruction can interpret the
 *   value.  This will usually be "floating-point only" or "integer only."
 *   Some instructions may allow either interpretation.
 *
 * * Flag selecting whether the instruction allows a single immediate value or
 *   all values must be in registers.  The the discussion about selection
 *   instructions below for more details.
 *
 * See \c struct value for more details.
 *
 * Consider a shader that contains a value -1.2 (0xbf99999a) in a
 * floating-point instruction.  Since the desired source value -1.2 can be
 * derived from either 0xbf99999a or 0x3f99999a (+1.2), one of those two bit
 * patterns must be loaded into a register.  The bit patterns 0xbf99999a and
 * 0x3f99999a are called reaching values because either of these values can
 * reach the value needed by the instruction.  Each use of a bit pattern has a
 * set of reaching values.
 *
 * Each bit pattern also has a set of reachable values.  For -1.2
 * (0xbf99999a), 1.2 (0x3f99999a) and 0x40666666 are reachable.  The extended
 * set of reachable values is the union of the reachable sets of the reaching
 * set.  For -1.2 (0xbf99999a), the extended reachable set is (0x3f99999a,
 * 0x40666666) ∪ (0xbf999999a, 0xc0666666).  The values present are the
 * floating-point negation, the integer negation, the original value, and
 * integer negation of the floating-point negation.  This is the set of values
 * that could be reached from the set of values that can reach -1.2.
 *
 * A value Y might be reachable from another value X if reachable(X) ∩
 * reaching(Y) ≠ Ø.  This is equivalent to Y ⊆ extended_reachable(X).
 *
 * Both the value and the steps to derive it are relevant.  The integer value
 * 0xc0666666 can reach -1.2 but -3.59 (0xc0666666) cannot.  This is because
 * the value 0x3f99999a would be stored.  The integer value 0xc0666666 would
 * be satisfied by taking the integer negation of 0x3f99999a, and -1.2 would
 * be satisfied by taking the floating-point negation of 1.2 (0x3f99999a).  It
 * is not possible to derive -1.2 from -3.59 without violating type rules.
 *
 * As a convenience, an extended reaching set is also defined.  This set is
 * identical to the extended reachable set, but the order of the "negation of
 * negation" values is reversed.  If the extended reachable set has the
 * integer negation of the floating-point, the extended reaching set will have
 * the floating-point negation of the integer negation.  This reversal is
 * because the extended reachable set defines routes from the value to other
 * values, but the extended reaching set defines routes from other values to
 * the value.  If a value X has the integer negation of the floating-point
 * negation in its extended reachable set, and another value Y has the integer
 * negation of the floating-point negation in its extended reaching set, the Y
 * is reachable from X if Y == -(int)(-(float)X).  This makes some aspects of
 * the implementation simpler.
 *
 * For some bit patterns, a particular kind of negation may not exist, so none
 * of those values will be reachable or reaching.  Bit patterns that represent
 * IEEE 754 not-a-number will not have float-point negations.  Likewise, 0 and
 * INT_MIN (for any bit size) will not have integer negations.  This may cause
 * some of the negation or negation-of-negation values to be removed from the
 * various sets.
 *
 * At the maximum, the extended reachable set for a floating-point value will
 * contain the original value, the floating-point negation, the integer
 * negation, and the integer negation of the floating-point negation.
 *
 * At the maximum, the extended reachable set for an integer value will
 * contain the original value, the floating-point negation, the integer
 * negation, and the floating-point negation of the integer negation.
 *
 * For each value there are multiple possible representations, as defined by
 * the reaching set.  A bit pattern in the reaching set of one value may be in
 * the reaching set of a another value.  The choice of loading a particular
 * bit pattern into a register may satisfy other values.  Minimization
 * problems with this-or-that choices are combinitorial optimization problems.
 *
 * This implementation uses branch-and-bound as the combinitorial optimization
 * algorithm (see footnote #1).  An abstarct branch-and-bound implementation
 * requires the user provide two functions: a function to generate a set of
 * branches from a known state to new states, and an heuristic function to
 * estimate the best-case cost achievable in a particular subtree.
 *
 * Two additional factors complicate both the branching and the heuristic.
 *
 * The first complicating factor is that some kinds of instructions can
 * interpret sources as either integers or floating-point.  The hardware
 * instruction used to implement selection on Intel GPUs is one such
 * instruction.  The sources of that hardware instruction can be either
 * integer or floating-point with the appropriate typed negation applied.
 *
 * The second complicating factor is that some instructions only require a
 * subset of constants be loaded in registers.  It is common to use selection
 * to conditionally select one of two constants, but only one of the values is
 * required to be loaded in a register.  On more recent Intel GPUs, one of the
 * sources to a three-source instruction, with some limitations, does not need
 * to be loaded into a register.  For a sequence like,
 *
 *       vec1 32 ssa_3 = load_const (0x40800000 -> 4.0)
 *       vec1 32 ssa_4 = load_const (0x3f000000 -> 0.5)
 *       ...
 *       vec1 32 ssa_30 = ffma ssa_3, ssa_27, ssa_4
 *
 * loading any one of 4.0, -4.0, 0.5, or -0.5 into a register will satisfy
 * both uses.
 *
 * The time complexity of combinatorial optimization algorithms is related to
 * the branching factor of the problem.  Two related steps are taken to reduce
 * the branching factor at the cost of increasing the constant factor of the
 * algorithm.  First, branches are trivially culled based on values that
 * actually remain in the data set.  Second, values with the smallest
 * branching factor are chosen first.
 *
 * As an example, if -1.2 is used but none of the other values in the extended
 * reachable set are used, then there is no reason to explore the benefits of
 * loading 0x40666666 instead of loading -1.2.  By examining the subset of the
 * extended reachable set that remains to be satisfied, a minimum set of
 * branches for each value can be chosen as described in the following table.
 *
 *  float     integer    integer neg.     float neg. of
 * negation   negation   of float neg.    integer neg.   Branches
 *    No         No            No             No         Original value
 *   Yes         No            No             No         Original value
 *    No        Yes            No             No         Original value
 *   Yes        Yes            No             No         Original value
 *    No         No           Yes             No         Floating-point negation
 *   Yes         No           Yes             No         Floating-point negation
 *    No        Yes           Yes             No         Original value and
 *                                                       floating-point negation
 *   Yes        Yes           Yes             No         Original value and
 *                                                       floating-point negation
 *    No         No            No             Yes        Integer negation
 *   Yes         No            No             Yes        Original value and
 *                                                       integer negation
 *    No        Yes            No             Yes        Integer negation
 *   Yes        Yes            No             Yes        Original value and
 *                                                       integer negation
 *    No         No           Yes             Yes        Integer negation and
 *                                                       floating-point negation
 *   Yes         No           Yes             Yes        Integer negation and
 *                                                       floating-point negation
 *    No        Yes           Yes             Yes        Integer negation and
 *                                                       floating-point negation
 *   Yes        Yes           Yes             Yes        Integer negation and
 *                                                       floating-point negation
 *
 * Two branches at most are necessary, and a single branch is sufficient in
 * many cases.  When choosing branches to add to the search tree, values
 * needing only a single branch are evaluated first.
 *
 * For the algorithm to work properly, the heuristic must be less than or
 * equal to the actual best route from a particular state to the goal.  The
 * high-level algorithm performs better when the heuristic is closer to the
 * actual best-case value.  A heuristic function that always evaluates to zero
 * results in worst-case preformance by forcing the algorithm to evaluate all
 * possible state combinations.
 *
 * Crafting a heuristic that is never greater than the actual best-case cost
 * is surprisingly difficult in this case.  Consider a shader that has 10,000
 * selection instructions like:
 *
 *    result_.. = selection(condition_.., 0x00000001, 0x00000002)
 *    result_.. = selection(condition_.., 0xffffffff, 0x00000003)
 *    result_.. = selection(condition_.., 0x00000001, 0x00000004)
 *    ...
 *    result_.. = selection(condition_.., 0xffffffff, 0x00002711)
 *
 * Loading either 1 or -1 into a register satisfy all 10,000 instructions.
 * None of the other 10,000 values need to be emitted.
 *
 * With the addition of a single instruction, loading ±1 will not satisfy all
 * of the values.  Either 0x2711 or 0x2712 would also need to be
 * loaded to satisfy all the instructions.
 *
 *    result_.. = selection(condition_.., 0x00002712, 0x00002711)
 *
 *
 * FINISHME: Finish the description of the heuristic.
 *
 * Footnote 1: This optimization was originally implemented A* (see
 * https://en.wikipedia.org/wiki/A*_search_algorithm).  A* was originally
 * designed for use in route planning.  Much of the terminology used in the
 * implementation (e.g., route_step and path_node) made more sense in that
 * frame of reference.
 */

#include <math.h> /* for isnan() */
#include <limits.h> /* for UINT_MAX */
#include <stdlib.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include "util/bitset.h"
#include "util/branch_and_bound.h"
#include "util/combine_constants.h"
#include "util/u_math.h"

#define SGN_DIFF(a, b) (((a) > (b)) - ((a) < (b)))

struct route_step {
   /**
    * Previous step along the route.
    *
    * Complete routes are stored as the current step and a link to the
    * previous step.  This allows most of the route information to be shared
    * among routes that have common "prefixes."
    */
   struct route_step *prev;

   /** Raw bit pattern of the constant loaded. */
   constant_value value;

   /** Size of the constant in bits. */
   uint8_t bit_size;

   /** Mask of negations that can be generated from this value. */
   uint8_t reachable_mask;
};

struct path_node {
   struct candidate_node base;

   unsigned ref_count;

   /**
    * Location in the graph.
    *
    * This is a bitset where each set bit represents a constant that still
    * needs to be handled.  Location 0 is the goal.  Location
    * (2**number_of_constants)-1 is the start state.
    */
   BITSET_WORD *location;

   /** Sequence of steps taken to get to this location. */
   struct route_step route;

   /**
    * Number of constants that still need to be handled.
    *
    * This must be equal to the number of set bits in the \c location bitset.
    */
   uint16_t remaining_constants;
};

struct combine_constants_state {
   struct value *candidates;
   unsigned num_candidates;
};

static void method_generate_branches(struct candidate_node *,
                                     struct list_head *branches,
                                     void *_state);

static void path_node_dtor(struct candidate_node *);

static const struct candidate_node_vtable path_node_vtable = {
   path_node_dtor,
   method_generate_branches
};

#define VALUE_INDEX                  0
#define FLOAT_NEG_INDEX              1
#define INT_NEG_INDEX                2
#define INT_NEG_OF_FLOAT_NEG_INDEX   3
#define FLOAT_NEG_OF_INT_NEG_INDEX   4

#define VALUE_EXISTS                 (1 << VALUE_INDEX)
#define FLOAT_NEG_EXISTS             (1 << FLOAT_NEG_INDEX)
#define INT_NEG_EXISTS               (1 << INT_NEG_INDEX)
#define FLOAT_NEG_OF_INT_NEG_EXISTS  (1 << FLOAT_NEG_OF_INT_NEG_INDEX)
#define INT_NEG_OF_FLOAT_NEG_EXISTS  (1 << INT_NEG_OF_FLOAT_NEG_INDEX)

static bool
negation_exists(constant_value v, unsigned bit_size,
                enum interpreted_type base_type)
{
   /* either_type does not make sense in this context. */
   assert(base_type == float_only || base_type == integer_only);

   switch (bit_size) {
   case 8:
      if (base_type == float_only)
         return false;
      else
         return v.i8 != 0 && v.i8 != INT8_MIN;

   case 16:
      if (base_type == float_only) {
         /*     !nan                     && !(zero || subnormal) */
         return !util_is_half_nan(v.i16) && (v.u16 & 0x7c00) != 0x0000;
      } else
         return v.i16 != 0 && v.i16 != INT16_MIN;

   case 32:
      if (base_type == float_only) {
         const int c = fpclassify(v.f32);

         return c == FP_NORMAL || c == FP_INFINITE;
      } else
         return v.i32 != 0 && v.i32 != INT32_MIN;

   case 64:
      if (base_type == float_only) {
         const int c = fpclassify(v.f64);

         return c == FP_NORMAL || c == FP_INFINITE;
      } else
         return v.i64 != 0 && v.i64 != INT64_MIN;

   default:
      unreachable("unsupported bit-size should have already been filtered.");
   }
}

static constant_value
negate(constant_value v, unsigned bit_size, enum interpreted_type base_type)
{
   /* either_type does not make sense in this context. */
   assert(base_type == float_only || base_type == integer_only);

   constant_value ret = { 0, };

   switch (bit_size) {
   case 8:
      assert(base_type == integer_only);
      ret.i8 = -v.i8;
      break;

   case 16:
      if (base_type == float_only)
         ret.u16 = v.u16 ^ INT16_MIN;
      else
         ret.i16 = -v.i16;
      break;

   case 32:
      if (base_type == float_only)
         ret.u32 = v.u32 ^ INT32_MIN;
      else
         ret.i32 = -v.i32;
      break;

   case 64:
      if (base_type == float_only)
         ret.u64 = v.u64 ^ INT64_MIN;
      else
         ret.i64 = -v.i64;
      break;

   default:
      unreachable("unsupported bit-size should have already been filtered.");
   }

   return ret;
}

static int
compar_value(const void *_a, const void *_b)
{
   const struct value *a = (const struct value *)_a;
   const struct value *b = (const struct value *)_b;

   if (a->bit_size != b->bit_size)
      return SGN_DIFF(a->bit_size, b->bit_size);

   switch (a->bit_size) {
   case 8:  return SGN_DIFF(a->value.i8,  b->value.i8);
   case 16: return SGN_DIFF(a->value.i16, b->value.i16);
   case 32: return SGN_DIFF(a->value.i32, b->value.i32);
   case 64: return SGN_DIFF(a->value.i64, b->value.i64);

   default:
      unreachable("unsupported bit-size should have already been filtered.");
   }
}

static void
path_node_unref(struct path_node *node)
{
   if (node != NULL) {
      assert(node->ref_count != 0);

      node->ref_count--;

      if (node->ref_count == 0) {
         if (node->route.prev != NULL) {
            path_node_unref((struct path_node *) container_of(node->route.prev,
                                                              node,
                                                              route));
         }

         free(node->location);
         free(node);
      }
   }
}

static void
path_node_dtor(struct candidate_node *_node)
{
   path_node_unref((struct path_node *) _node);
}

static void
calculate_masks(constant_value v, enum interpreted_type type,
                unsigned bit_size, uint8_t *reachable_mask,
                uint8_t *reaching_mask)
{
   *reachable_mask = 0;
   *reaching_mask = 0;

   /* Calculate the extended reachable mask. */
   if (type == float_only || type == either_type) {
      if (negation_exists(v, bit_size, float_only)) {
         const constant_value neg = negate(v, bit_size, float_only);

         *reachable_mask |= FLOAT_NEG_EXISTS;

         if (negation_exists(neg, bit_size, integer_only))
            *reachable_mask |= INT_NEG_OF_FLOAT_NEG_EXISTS;
      }
   }

   if (type == integer_only || type == either_type) {
      if (negation_exists(v, bit_size, integer_only)) {
         const constant_value neg = negate(v, bit_size, integer_only);

         *reachable_mask |= INT_NEG_EXISTS;

         if (negation_exists(neg, bit_size, float_only))
            *reachable_mask |= FLOAT_NEG_OF_INT_NEG_EXISTS;
      }
   }

   /* Calculate the extended reaching mask.  All of the "is this negation
    * possible" was already determined for the reachable_mask, so reuse that
    * data.
    */
   if (type == float_only || type == either_type) {
      if (*reachable_mask & FLOAT_NEG_EXISTS) {
         *reaching_mask |= FLOAT_NEG_EXISTS;

         if (*reachable_mask & INT_NEG_OF_FLOAT_NEG_EXISTS)
            *reaching_mask |= FLOAT_NEG_OF_INT_NEG_EXISTS;
      }
   }

   if (type == integer_only || type == either_type) {
      if (*reachable_mask & INT_NEG_EXISTS) {
         *reaching_mask |= INT_NEG_EXISTS;

         if (*reachable_mask & FLOAT_NEG_OF_INT_NEG_EXISTS)
            *reaching_mask |= INT_NEG_OF_FLOAT_NEG_EXISTS;
      }
   }
}

static void
calculate_reachable_values(constant_value v,
                           unsigned bit_size,
                           unsigned reachable_mask,
                           constant_value *reachable_values)
{
   memset(reachable_values, 0, 5 * sizeof(reachable_values[0]));

   reachable_values[VALUE_INDEX] = v;

   if (reachable_mask & INT_NEG_EXISTS) {
      const constant_value neg = negate(v, bit_size, integer_only);

      reachable_values[INT_NEG_INDEX] = neg;

      if (reachable_mask & FLOAT_NEG_OF_INT_NEG_EXISTS) {
         reachable_values[FLOAT_NEG_OF_INT_NEG_INDEX] = negate(neg,
                                                               bit_size,
                                                               float_only);
      }
   }

   if (reachable_mask & FLOAT_NEG_EXISTS) {
      const constant_value neg = negate(v, bit_size, float_only);

      reachable_values[FLOAT_NEG_INDEX] = neg;

      if (reachable_mask & INT_NEG_OF_FLOAT_NEG_EXISTS) {
         reachable_values[INT_NEG_OF_FLOAT_NEG_INDEX] = negate(neg,
                                                               bit_size,
                                                               integer_only);
      }
   }
}

static bool
value_equal(constant_value a, constant_value b, unsigned bit_size)
{
   switch (bit_size) {
   case 8:
      return a.u8 == b.u8;
   case 16:
      return a.u16 == b.u16;
   case 32:
      return a.u32 == b.u32;
   case 64:
      return a.u64 == b.u64;
   default:
      unreachable("unsupported bit-size should have already been filtered.");
   }
}

static bool
value_could_reach(const constant_value *from, uint8_t reachable_mask,
                  constant_value to, uint8_t reaching_mask,
                  unsigned bit_size)
{
   const uint8_t combined_mask = reachable_mask & reaching_mask;

   return value_equal(from[VALUE_INDEX], to, bit_size) ||
          ((combined_mask & INT_NEG_EXISTS) &&
           value_equal(from[INT_NEG_INDEX], to, bit_size)) ||
          ((combined_mask & FLOAT_NEG_EXISTS) &&
           value_equal(from[FLOAT_NEG_INDEX], to, bit_size)) ||
          ((combined_mask & INT_NEG_OF_FLOAT_NEG_EXISTS) &&
           value_equal(from[INT_NEG_OF_FLOAT_NEG_INDEX], to, bit_size)) ||
          ((combined_mask & FLOAT_NEG_OF_INT_NEG_EXISTS) &&
           value_equal(from[FLOAT_NEG_OF_INT_NEG_INDEX], to, bit_size));
}

static int
compar_unsigned(const void *_a, const void *_b)
{
   const unsigned a = *(const unsigned *) _a;
   const unsigned b = *(const unsigned *) _b;

   return -SGN_DIFF(a, b);
}

static unsigned
evaluate_heuristic(struct path_node *p, struct value *candidates,
                   unsigned num_candidates)
{
   /* FINISHME: As implemented, this is n**2.  A real data structure created
    * at the start would make this linear.  A collection of graphs where each
    * node is a unique sized bit pattern.  Each node contains a list of users
    * (indicies in candidates).  Each edge is a connection to the integer
    * negation and the floating-point negation of the sized bit pattern.
    */
   unsigned *potential = calloc(num_candidates, sizeof(potential[0]));

   unsigned prev_i = UINT_MAX;
   unsigned i;
   unsigned unique_values = 0;

   BITSET_FOREACH_SET(i, p->location, num_candidates) {
      if (prev_i > i ||
          candidates[prev_i].bit_size != candidates[i].bit_size ||
          !value_equal(candidates[prev_i].value, candidates[i].value,
                       candidates[prev_i].bit_size)) {
         const unsigned bit_size = candidates[i].bit_size;

         constant_value reachable_values[5];

         calculate_reachable_values(candidates[i].value,
                                    bit_size,
                                    candidates[i].reachable_mask,
                                    reachable_values);

         /* Count the number of values that are covered.  A value is covered
          * if it is bitwise equal to the test value, it is a (type correct)
          * negation of the test value, or it is a source of an instruction
          * that allows a constant and the other source is covered by the test
          * value.
          */
         unsigned num_covered = 0;
         unsigned j;
         BITSET_FOREACH_SET(j, p->location, num_candidates) {
            if (candidates[j].bit_size != bit_size)
               continue;

            if (value_could_reach(reachable_values,
                                  candidates[i].reachable_mask,
                                  candidates[j].value,
                                  candidates[j].reaching_mask,
                                  bit_size)) {
               num_covered++;
            } else {
               if (candidates[j].allow_one_constant) {
                  const struct value *const other_candidate =
                     candidates[j].next_src;

                  /* Much of this code assumes that there will be at most two
                   * sources in an instruction that are constant.
                   */
                  assert(other_candidate->next_src == &candidates[j]);

                  if (value_could_reach(reachable_values,
                                        candidates[i].reachable_mask,
                                        other_candidate->value,
                                        other_candidate->reaching_mask,
                                        bit_size)) {
                     num_covered++;
                  }
               }
            }
         }

         potential[unique_values++] = num_covered;
      }

      prev_i = i;
   }

   qsort(potential, unique_values, sizeof(unsigned), compar_unsigned);

   unsigned sum = 0;
   for (i = 0; i < unique_values; i++) {
      sum += potential[i];

      if (sum >= p->remaining_constants) {
         free(potential);
         return i + 1;
      }
   }

   /* There are many reasons why the sum of elements of potential must always
    * be greater than or equal to (and usually much greater than) to number of
    * remaining constants.
    */
   unreachable("Should have returned from inside the loop.");
}

static struct path_node *
create_initial_path_node(unsigned num_candidates)
{
   struct path_node *n = calloc(1, sizeof(struct path_node));

   if (n != NULL) {
      n->base.vtable = &path_node_vtable;
      n->location = calloc(BITSET_WORDS(num_candidates), sizeof(BITSET_WORD));
      n->ref_count = 1;
      n->remaining_constants = num_candidates;

      if (n->location == NULL) {
         free(n);
         return NULL;
      }

      /* Mark the bit for every candidate constant. */
      for (unsigned i = 0; i < BITSET_WORDS(num_candidates); i++)
         n->location[i] = ~0;

      /* The last word might not be fully set. */
      if (num_candidates % BITSET_WORDBITS != 0) {
         n->location[BITSET_WORDS(num_candidates) - 1] =
            (1u << (num_candidates % BITSET_WORDBITS)) - 1;
      }
   }

   return n;
}

static struct path_node *
create_new_path_node(struct path_node *base,
                     struct value *candidates,
                     unsigned num_candidates,
                     constant_value v,
                     enum interpreted_type type,
                     unsigned bit_size)
{
   struct path_node *p = create_initial_path_node(num_candidates);

   memcpy(p->location, base->location,
          sizeof(p->location[0]) * BITSET_WORDS(num_candidates));

   base->ref_count++;
   p->base.cost_so_far = base->base.cost_so_far + 1;
   p->route.prev = &base->route;
   p->route.value = v;
   p->route.bit_size = bit_size;
   p->remaining_constants = base->remaining_constants;

   constant_value reachable_values[5];
   uint8_t reachable_mask;
   uint8_t do_not_use;

   calculate_masks(v, type, bit_size, &reachable_mask, &do_not_use);

   /* Once a concrete value is stored, the double negation values aren't
    * actually reachable.  If x is stored, it is possible to derive either
    * -bitsAsInt(x) or -bitsAsFloat(x) using source modifiers in an
    * instruction.
    */
   reachable_mask &= ~(FLOAT_NEG_OF_INT_NEG_EXISTS |
                       INT_NEG_OF_FLOAT_NEG_EXISTS);

   calculate_reachable_values(v, bit_size, reachable_mask, reachable_values);

   p->route.reachable_mask = reachable_mask;

   unsigned i;
   BITSET_FOREACH_SET(i, p->location, num_candidates) {
      /* The BITSET_FOREACH_SET macro does not detect changes to the bitset
       * that occur within the current word.  Since code in this loop may
       * clear bits from the set, re-test here.
       */
      if (!BITSET_TEST(p->location, i))
         continue;

      if (bit_size != candidates[i].bit_size)
         continue;

      if (value_could_reach(reachable_values,
                            reachable_mask,
                            candidates[i].value,
                            candidates[i].reaching_mask,
                            bit_size)) {
         BITSET_CLEAR(p->location, i);
         p->remaining_constants--;

         /* If the value is used by an instruction that allows a constant
          * source, mark the other source as also being handled.
          */
         if (candidates[i].allow_one_constant) {
            assert(candidates[i].next_src != NULL);

            const struct value *const other_candidate = candidates[i].next_src;
            const unsigned j = other_candidate - candidates;

            BITSET_CLEAR(p->location, j);
            p->remaining_constants--;
         }
      }
   }

#ifndef NDEBUG
   unsigned counted_remaining_constants = 0;

   BITSET_FOREACH_SET(i, p->location, num_candidates)
      counted_remaining_constants++;

   assert(p->remaining_constants == counted_remaining_constants);
#endif

   if (p->remaining_constants == 0) {
      p->base.is_solution = true;
      p->base.cost_lower_bound = p->base.cost_so_far;
   } else {
      p->base.is_solution = false;
      p->base.cost_lower_bound = p->base.cost_so_far +
         evaluate_heuristic(p, candidates, num_candidates);
   }

   assert(p->remaining_constants < base->remaining_constants);

   return p;
}

enum PACKED possible_action_mask {
   emit_original_value = VALUE_EXISTS,
   emit_float_negation = FLOAT_NEG_EXISTS,
   emit_int_negation = INT_NEG_EXISTS,

   emit_original_value_and_float_negation = VALUE_EXISTS | FLOAT_NEG_EXISTS,
   emit_original_value_and_int_negation = VALUE_EXISTS | INT_NEG_EXISTS,

   emit_int_negation_and_float_negation = INT_NEG_EXISTS | FLOAT_NEG_EXISTS,
};

static const enum possible_action_mask action_table[16] = {
/* -f  -i  -i(-f)  -f(-i)    Action */
/*  n   n    n       n  */   emit_original_value,
/*  y   n    n       n  */   emit_original_value,
/*  n   y    n       n  */   emit_original_value,
/*  y   y    n       n  */   emit_original_value,
/*  n   n    y       n  */   emit_float_negation,
/*  y   n    y       n  */   emit_float_negation,
/*  n   y    y       n  */   emit_original_value_and_float_negation,
/*  y   y    y       n  */   emit_original_value_and_float_negation,
/*  n   n    n       y  */   emit_int_negation,
/*  y   n    n       y  */   emit_original_value_and_int_negation,
/*  n   y    n       y  */   emit_int_negation,
/*  y   y    n       y  */   emit_original_value_and_int_negation,
/*  n   n    y       y  */   emit_int_negation_and_float_negation,
/*  y   n    y       y  */   emit_int_negation_and_float_negation,
/*  n   y    y       y  */   emit_int_negation_and_float_negation,
/*  y   y    y       y  */   emit_int_negation_and_float_negation,
};

static void
generate_branches(struct list_head *branches, struct path_node *p,
                  struct value *candidates, unsigned num_candidates)
{
   list_inithead(branches);

   unsigned i;

   /* For each remaining value, calculate the reachable value set.  If the
    * reachable set for any value is empty or has a single element, emit that
    * branch and terminate early.
    */
   constant_value prev_v = { 0, };
   unsigned prev_bit_size = 0;

   enum possible_action_mask minimum_action = 0;
   unsigned minimum_action_i;

   BITSET_FOREACH_SET(i, p->location, num_candidates) {
      const constant_value v = candidates[i].value;
      const unsigned bit_size = candidates[i].bit_size;

      /* It is common for the same value to be used repeatedly.  There is no
       * reason to try to add it to the path repeatedly.
       */
      if (bit_size == prev_bit_size && value_equal(v, prev_v, bit_size))
         continue;

      /* Instructions that allow a single constant do not take part in this.
       * The challenge is that the reachable set of both sources must be
       * explored as mutually exclusive branches of the search tree.
       */
      if (candidates[i].allow_one_constant)
         continue;

      prev_v = v;
      prev_bit_size = bit_size;

      if (candidates[i].reachable_mask == 0) {
         /* If there are no negations possible, then this value must be
          * emitted.
          */
         minimum_action = emit_original_value;
         minimum_action_i = i;
         break;
      }

      constant_value reachable_values[5];

      calculate_reachable_values(v, bit_size, candidates[i].reachable_mask,
                                 reachable_values);

      /* Now that the set of reachable values has been generated, scan the set
       * to see which of those values may exist.
       */
      unsigned total_mask = 0;
      unsigned j;
      BITSET_FOREACH_SET(j, p->location, num_candidates) {
         if (bit_size != candidates[j].bit_size)
            continue;

         unsigned matches_mask = 0;
         for (unsigned k = 0; k < ARRAY_SIZE(reachable_values); k++) {
            const bool match = value_equal(reachable_values[k],
                                           candidates[j].value,
                                           bit_size);

            matches_mask |= (int)match << k;
         }

         /* The loop above checks all the values, even the ones that are not
          * reachable from candidates[i] and cannot reach candidates[j].
          * After masking those away, the remaining bits, if any, describe the
          * values that could be used to generate both the value of
          * candidates[i] and the value of candidates[j].
          */
         total_mask |= candidates[i].reachable_mask &
                       candidates[j].reaching_mask &
                       matches_mask;
      }

      /* Reaching / reachable make values for negations start at bit 1, but
       * the table index needs them to start at bit 0.  Shift by 1 to adjust.
       */
      const enum possible_action_mask action = action_table[total_mask >> 1];

      if (action == emit_original_value ||
          action == emit_float_negation ||
          action == emit_int_negation) {
         minimum_action = action;
         minimum_action_i = i;
         break;
      }

      minimum_action = action;
      minimum_action_i = i;
   }

   if (minimum_action != 0) {
      const unsigned bit_size = candidates[minimum_action_i].bit_size;
      constant_value reachable_values[5];

      calculate_reachable_values(candidates[minimum_action_i].value,
                                 candidates[minimum_action_i].bit_size,
                                 candidates[minimum_action_i].reachable_mask,
                                 reachable_values);

      for (unsigned j = 0; j < ARRAY_SIZE(reachable_values); j++) {
         struct path_node *new_path = NULL;

         if (minimum_action & (1u << j)) {
            new_path = create_new_path_node(p, candidates, num_candidates,
                                            reachable_values[j],
                                            candidates[minimum_action_i].type,
                                            bit_size);

            list_addtail(&new_path->base.link, branches);
         }
      }

      return;
   }

   /* If execution reaches this point, all of the remaining candidates must be
    * used by instructions that allow a constant.  This case is similar to the
    * case of regular instructions, but there are more possible choices.  The
    * reaching sets of each source are added to the search tree as mutually
    * exclusive subtrees.  If a value from the reaching set of one source is
    * on the optimal path, the entire reaching set of the other source is
    * disregarded.
    *
    * In the regular instruction case, choosing the constant with the smallest
    * set of options leads to a narrower search tree (i.e., fewer combinations
    * to examine).  This may also be the case here, but it is more annoying to
    * implement.  Instead, pick the first remaining candidate.  Emit all the
    * possible choices for that candidate and the other candidate used by the
    * same instruction.
    */
   i = __bitset_ffs(p->location, BITSET_WORDS(num_candidates)) - 1;

   /* This would occur if __bitset_ffs returned zero for no bits set. */
   assert(i != UINT_MAX);
   assert(BITSET_TEST(p->location, i));

   const unsigned bit_size = candidates[i].bit_size;

   assert(candidates[i].allow_one_constant);

   constant_value values[6];
   unsigned num_values = 0;

   values[num_values++] = candidates[i].value;

   if (candidates[i].reaching_mask & INT_NEG_EXISTS)
      values[num_values++] = negate(values[0], bit_size, integer_only);

   if (candidates[i].reaching_mask & FLOAT_NEG_EXISTS)
      values[num_values++] = negate(values[0], bit_size, float_only);

   assert(candidates[i].next_src != NULL);

   struct value *other_candidate = candidates[i].next_src;

#ifndef NDEBUG
   unsigned other_i = other_candidate - candidates;

   assert(BITSET_TEST(p->location, other_i));
#endif

   values[num_values++] = other_candidate->value;

   if (other_candidate->reaching_mask & INT_NEG_EXISTS)
      values[num_values++] = negate(other_candidate->value, bit_size, integer_only);

   if (other_candidate->reaching_mask & FLOAT_NEG_EXISTS)
      values[num_values++] = negate(other_candidate->value, bit_size, float_only);

   /* num_values must be at least 2.  It can only be 2 for a small set of
    * values.  For example, if src1 is 0x8000000 and src2 is 0x00000000.
    */
   assert(num_values >= 2);

   for (unsigned j = 0; j < num_values; j++) {
      /* Check to make sure this same bit pattern was not already added. */
      bool already_added = false;
      for (unsigned k = 0; k < j; k++) {
         if (value_equal(values[j], values[k], bit_size)) {
            already_added = true;
            break;
         }
      }

      if (already_added)
         continue;

      struct path_node *new_path = create_new_path_node(p,
                                                        candidates,
                                                        num_candidates,
                                                        values[j],
                                                        candidates[i].type,
                                                        bit_size);

      list_addtail(&new_path->base.link, branches);
   }
}

static void
method_generate_branches(struct candidate_node *_node,
                         struct list_head *branches,
                         void *_state)
{
   struct combine_constants_state *state = (struct combine_constants_state *) _state;

   generate_branches(branches, (struct path_node *) _node,
                     state->candidates, state->num_candidates);
}

static void
preprocess_candidates(struct value *candidates, unsigned num_candidates)
{
   qsort(candidates, num_candidates, sizeof(candidates[0]), compar_value);

   /* Calculate the reaching_mask and reachable_mask for each candidate. */
   for (unsigned i = 0; i < num_candidates; i++) {
      calculate_masks(candidates[i].value,
                      candidates[i].type,
                      candidates[i].bit_size,
                      &candidates[i].reachable_mask,
                      &candidates[i].reaching_mask);
   }

   for (unsigned i = 0; i < num_candidates; i++)
      candidates[i].next_src = NULL;

   for (unsigned i = 0; i < num_candidates - 1; i++) {
      if (candidates[i].next_src != NULL)
         continue;

      struct value *prev = &candidates[i];

      for (unsigned j = i + 1; j < num_candidates; j++) {
         if (candidates[i].instr == candidates[j].instr) {
            prev->next_src = &candidates[j];
            prev = prev->next_src;
         }
      }

      /* Close the cycle. */
      if (prev != &candidates[i])
         prev->next_src = &candidates[i];
   }
}

struct combine_constants_result *
util_combine_constants(struct value *candidates, unsigned num_candidates)
{
   preprocess_candidates(candidates, num_candidates);

   struct path_node *initial = create_initial_path_node(num_candidates);

   struct combine_constants_state state = {
      candidates, num_candidates
   };

   struct path_node *best =
      (struct path_node *) _mesa_branch_and_bound_solve(&initial->base, &state);

   assert(best != NULL);

   /* Flatten the linked list into something that the caller can more easily
    * consume.
    */
   unsigned num_values = 0;
   struct route_step *step = &best->route;
   while (step->prev != NULL) {
      step = step->prev;
      num_values++;
   }

   struct combine_constants_result *result;
   const size_t bytes = sizeof(*result) +
                        num_candidates * sizeof(result->user_map[0]);

   /* Since calloc isn't being used "properly," manually check for integer
    * overflow.
    */
   if (bytes < sizeof(*result))
      return NULL;

   result = calloc(1, bytes);
   if (result == NULL)
      return NULL;

   struct combine_constants_value *values =
      calloc(num_values, sizeof(values[0]));

   if (values == NULL) {
      util_combine_constants_result_dtor(result);
      return NULL;
   }

   result->values_to_emit = values;
   result->num_values_to_emit = num_values;

   BITSET_WORD *remain = calloc(BITSET_WORDS(num_candidates), sizeof(remain[0]));

   if (remain == NULL) {
      util_combine_constants_result_dtor(result);
      return NULL;
   }

   memset(remain, 0xff, BITSET_WORDS(num_candidates) * sizeof(remain[0]));

   unsigned total_users = 0;
   unsigned value_idx = 0;

   for (step = &best->route; step->prev != NULL; /* empty */) {
      struct route_step *prev = step->prev;

      values[value_idx].value = step->value;
      values[value_idx].first_user = total_users;
      values[value_idx].num_users = 0;
      values[value_idx].bit_size = step->bit_size;

      constant_value reachable_values[5];

      calculate_reachable_values(step->value, step->bit_size,
                                 step->reachable_mask, reachable_values);

      for (unsigned i = 0; i < num_candidates; i++) {
         bool matched = false;

         if (!BITSET_TEST(remain, i))
            continue;

         if (candidates[i].bit_size != step->bit_size)
            continue;

         if (value_equal(candidates[i].value, step->value, step->bit_size)) {
            result->user_map[total_users].index = i;
            result->user_map[total_users].type = candidates[i].type;
            result->user_map[total_users].negate = false;
            total_users++;

            matched = true;
            BITSET_CLEAR(remain, i);
         } else {
            const uint8_t combined_mask = step->reachable_mask &
                                          candidates[i].reaching_mask;

            enum interpreted_type type = either_type;

            if ((combined_mask & INT_NEG_EXISTS) &&
                value_equal(candidates[i].value,
                            reachable_values[INT_NEG_INDEX],
                            candidates[i].bit_size)) {
               type = integer_only;
            }

            if (type == either_type &&
                (combined_mask & FLOAT_NEG_EXISTS) &&
                value_equal(candidates[i].value,
                            reachable_values[FLOAT_NEG_INDEX],
                            candidates[i].bit_size)) {
               type = float_only;
            }

            if (type != either_type) {
               result->user_map[total_users].index = i;
               result->user_map[total_users].type = type;
               result->user_map[total_users].negate = true;
               total_users++;

               matched = true;
               BITSET_CLEAR(remain, i);
            }
         }

         /* Mark the other source of instructions that can have a constant
          * source.  Selection is the prime example of this, and we want to
          * avoid generating sequences like bcsel(a, fneg(b), ineg(c)).
          *
          * This also makes sure that the assertion (below) that *all* values
          * were processed holds even when some values may be allowed to
          * remain as constants.
          *
          * FINISHME: There may be value in only doing this when type ==
          * either_type.  If both sources are loaded, a register allocator may
          * be able to make a better choice about which value to "spill"
          * (i.e., replace with an immediate) under heavy register pressure.
          */
         if (matched && candidates[i].allow_one_constant) {
            const struct value *const other_src = candidates[i].next_src;
            const unsigned idx = other_src - candidates;

            assert(idx < num_candidates);
            BITSET_CLEAR(remain, idx);
         }
      }

      /* FINISHME: This if-statement is a hack.  Some shaders contain
       * combinations of uses of 1.0, -4.0, and 4.0 that confuse the algorithm
       * into emitting a -1.0 node that has zero uses.  For example, see
       * shaders/closed/UnrealEngine4/ReflectionsSubwayDemo/191.shader_test.
       */
      if (total_users != values[value_idx].first_user) {
         assert(total_users > values[value_idx].first_user);
         values[value_idx].num_users = total_users - values[value_idx].first_user;
         value_idx++;
      }

      /* We want to release memory as we go.  The only reference to step->prev
       * is step->prev.  If we unref step while they are linked, the whole
       * chain will be released.  Break the link so that only step is released.
       */
      step->prev = NULL;
      path_node_unref((struct path_node *) container_of(step, best, route));

      step = prev;
   }

   /* Verify that all of the values were emitted by the loop above.  If any
    * bits are still set in remain, then some value was not emitted.  The use
    * of memset to populate remain prevents the use of a more performant loop.
    */
#ifndef NDEBUG
   unsigned i;
   bool pass = true;

   BITSET_FOREACH_SET(i, remain, num_candidates) {
      fprintf(stderr, "candidate %d was not processed: { "
              ".b = %s, "
              ".f32 = %f, .f64 = %g, "
              ".i8 = %d, .u8 = 0x%02x, "
              ".i16 = %d, .u16 = 0x%04x, "
              ".i32 = %d, .u32 = 0x%08x, "
              ".i64 = %" PRId64 ", .u64 = 0x%016" PRIx64 " }\n",
              i,
              candidates[i].value.b ? "true" : "false",
              candidates[i].value.f32, candidates[i].value.f64,
              candidates[i].value.i8,  candidates[i].value.u8,
              candidates[i].value.i16, candidates[i].value.u16,
              candidates[i].value.i32, candidates[i].value.u32,
              candidates[i].value.i64, candidates[i].value.u64);
      pass = false;
   }

   assert(pass && "All values should have been processed.");
#endif

   free(remain);

   return result;
}

void
util_combine_constants_result_dtor(struct combine_constants_result *result)
{
   if (result != NULL) {
      free(result->values_to_emit);
      free(result);
   }
}
