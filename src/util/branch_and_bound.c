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
 * \file branch_and_bound.c
 * Implementation of the branch-and-bound algorithm for combinatorial
 * optimization.  See https://en.wikipedia.org/wiki/Branch_and_bound.
 *
 * This implementation deviates from the straght forward implementation
 * slightly.  In the "typical" implementation, the list of nodes is either a
 * priority queue sorted by heuristic or a FIFO queue.  When the FIFO strategy
 * is used, the algorithm behaves as depth-first search.  When the priority
 * queue strategy is used and the heuristic is not very accurate, the
 * algorithm devolves into breadth-first search.
 *
 * Start in FIFO mode until a solution is found, then switch to priority mode.
 * If a "large" number of steps have occured without reaching another
 * solution, assume the search is behaving more like BFS and switch back to
 * FIFO for awhile.
 *
 * Finally, the algorithm will halt and return the best yet known solution
 * after a fixed number of iterations.
 *
 * There may be better solutions to this problem, and there are some papers on
 * this topic.  For a survey of related work, see
 * https://www.sciencedirect.com/science/article/pii/S1572528616000062.
 */
//#define BNB_DEBUG

#include <stdlib.h>
#ifdef BNB_DEBUG
#include <stdio.h>
#endif
#include "branch_and_bound.h"

/**
 * Add a node to a list before some item already in the list.
 *
 * \p after_item is the item already in the list.  This is functionally the
 * same as \c list_addtail, but sometimes the other name makes more sense.
 */
static void
list_addbefore(struct list_head *item, struct list_head *after_item)
{
   list_addtail(item, after_item);
}

/**
 * Remove and return the first item in the list.
 *
 * \returns \c NULL if the list is empty.
 */
static struct list_head *
list_pophead(struct list_head *list)
{
   if (list_is_empty(list))
      return NULL;

   struct list_head *n = list->next;

   list_del(n);

   return n;
}

static void
add_candidate_node(struct list_head *candidate_queue,
                   struct candidate_node *cand,
                   bool fifo_queue)
{
   if (fifo_queue || list_is_empty(candidate_queue)) {
      list_add(&cand->link, candidate_queue);
      return;
   }

   /* This is loop is structured this way instead of using LIST_FOR_EACH_ENTRY
    * because when the loop terminates n might point to candidate_queue.
    * Since candidate_queue is not a 'struct candidate_node', that would lead
    * to undefined behavior.
    */
   struct list_head *n = candidate_queue->next;
   while (n != candidate_queue) {
      struct candidate_node *curr = (struct candidate_node *) n;

      if (curr->cost_lower_bound >= cand->cost_lower_bound)
         break;

      n = n->next;
   }

   list_addbefore(&cand->link, n);
}

#define SGN_DIFF(a, b) (((a) > (b)) - ((a) < (b)))

static int
compar_cost(const void *_a, const void *_b)
{
   const struct candidate_node *a = *(const struct candidate_node **) _a;
   const struct candidate_node *b = *(const struct candidate_node **) _b;

   return SGN_DIFF(a->cost_lower_bound, b->cost_lower_bound);
}

static void
sort_queue(struct list_head *candidate_queue, unsigned length)
{
   assert(length == list_length(candidate_queue));

   struct candidate_node **candidate_array =
      calloc(length, sizeof(struct candidate_node *));

   unsigned i = 0;
   struct candidate_node *n;

   LIST_FOR_EACH_ENTRY(n, candidate_queue, link)
      candidate_array[i++] = n;

   qsort(candidate_array, length, sizeof(candidate_array[0]), compar_cost);

   list_inithead(candidate_queue);
   for (i = 0; i < length; i++)
      list_addtail(&candidate_array[i]->link, candidate_queue);

   free(candidate_array);
}

struct candidate_node *
_mesa_branch_and_bound_solve(struct candidate_node *start, void *state)
{
   struct list_head candidate_queue;
   unsigned candidate_queue_length = 0;

   list_inithead(&candidate_queue);

   add_candidate_node(&candidate_queue, start, true);
   candidate_queue_length++;

   struct candidate_node *best = NULL;
   uint64_t problem_upper_bound = UINT64_MAX;

   bool fifo = true;
   unsigned transition_countdown = ~0;

   unsigned iterations = 0;
   struct list_head *n;
   while ((n = list_pophead(&candidate_queue)) != NULL) {
      candidate_queue_length--;

      struct candidate_node *cand = (struct candidate_node *) n;
      bool need_to_sort_queue = false;

      /* A partial solution that may have been reasonable to evaluate when it
       * was added to the queue may not be reasonable to evaluate now.  Check
       * against the bounds and possibly discard.
       */
      if (cand->cost_lower_bound >= problem_upper_bound) {
         cand->vtable->dtor(cand);
         continue;
      }

      struct list_head branches;
      cand->vtable->generate_branches(cand, &branches, state);

      const uint64_t previous_cost_lower_bound = cand->cost_lower_bound;

      cand->vtable->dtor(cand);

      struct list_head *nn;
      while ((nn = list_pophead(&branches)) != NULL) {
         struct candidate_node *new_candidate = (struct candidate_node *) nn;

         if (new_candidate->is_solution) {
            /* Does the new solution improve on the best known solution?  If
             * not, drop it.
             */
            if (new_candidate->cost_so_far < problem_upper_bound) {
               problem_upper_bound = new_candidate->cost_so_far;

#ifdef BNB_DEBUG
               if (best != NULL) {
                  fprintf(stderr, "Changing best from %p to %p @ interation %u w/cost from %u to %u\n",
                          best->cost_so_far, new_candidate->cost_so_far,
                          best, new_candidate, iterations);
               }
#endif

               /* The first solution has been found.  This triggers the
                * transition from FIFO-queue mode to priority-queue mode.  On
                * that transition, the unsorted queue must be priority sorted.
                */
               if (best == NULL)
                  transition_countdown = 20;
               else
                  best->vtable->dtor(best);

               best = new_candidate;
            } else {
               new_candidate->vtable->dtor(new_candidate);
            }
         } else {
            /* The estimated lower bound cost must be less than or equal to
             * the actual cost.  As the problem is evaluated, the estimated
             * lower bound cost of the partial solution must move closer to
             * the actual cost.  By induction, the estimated lower bound of
             * the new partial solution must be greater than or equal to the
             * estimated lower bound of the previous partial solution.
             */
            assert(new_candidate->cost_lower_bound >= previous_cost_lower_bound);

            if (new_candidate->cost_lower_bound < problem_upper_bound) {
               add_candidate_node(&candidate_queue, new_candidate, fifo);
               candidate_queue_length++;
            } else
               new_candidate->vtable->dtor(new_candidate);
         }
      }

      transition_countdown--;
      if (transition_countdown == 0) {
         if (fifo) {
            sort_queue(&candidate_queue, candidate_queue_length);
            transition_countdown = 733;
         } else
            transition_countdown = 97;

         fifo = !fifo;
      }

      iterations++;

      if (best != NULL && iterations > 8000)
         break;
   }

#ifdef BNB_DEBUG
   fprintf(stderr, "%u MAXIMUM ITERATIONS\n", iterations);
#endif
   return best;
}
