/*
 * Copyright (C) 2022 Collabora Ltd
 * Copyright (C) 2021 Valve Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <assert.h>
#include "u_lower_parallel_copy.h"

/*
 * Emits code for
 *
 *    for (int i = 0; i < n; ++i)
 *       registers[dests[i]] = registers[srcs[i]];
 *
 * ...with all copies happening in parallel.
 *
 * That is, emit machine instructions equivalent to a parallel copy. This is
 * used to lower not only parallel copies but also collects and splits, which
 * also have parallel copy semantics.
 */

#define RA_MAX_FILE_SIZE (384) // XXX: generalize

struct copy_ctx {
   /* Number of copies being processed */
   unsigned entry_count;

   /* For each physreg, the number of pending copy entries that use it as a
    * source. Once this drops to zero, then the physreg is unblocked and can
    * be moved to.
    */
   unsigned physreg_use_count[RA_MAX_FILE_SIZE];

   /* For each physreg, the pending copy_entry that uses it as a dest. */
   struct u_copy *physreg_dst[RA_MAX_FILE_SIZE];

   struct u_copy entries[RA_MAX_FILE_SIZE];
};

static bool
entry_blocked(struct u_copy *entry, struct copy_ctx *ctx)
{
   for (unsigned i = 0; i < entry->size; i++) {
      if (ctx->physreg_use_count[entry->dst + i] != 0)
         return true;
   }

   return false;
}

static bool
is_real(struct u_copy *entry)
{
   return entry->src >= 0;
}

/* TODO: Generalize to other bit sizes */
static void
split_32bit_copy(struct copy_ctx *ctx, struct u_copy *entry)
{
   assert(!entry->done);
   assert(is_real(entry));
   assert(entry->size == 2);
   struct u_copy *new_entry = &ctx->entries[ctx->entry_count++];

   new_entry->dst = entry->dst + 1;
   new_entry->src = entry->src + 1;
   new_entry->done = false;
   entry->size = 1;
   new_entry->size = 1;
   ctx->physreg_dst[entry->dst + 1] = new_entry;
}

void
u_lower_parallel_copy(struct lower_parallel_copy_options *options,
                      struct u_copy *copies,
                      unsigned num_copies)
{
   struct copy_ctx _ctx = {
      .entry_count = num_copies
   };

   struct copy_ctx *ctx = &_ctx;

   /* Set up the bookkeeping */
   memset(ctx->physreg_dst, 0, sizeof(ctx->physreg_dst));
   memset(ctx->physreg_use_count, 0, sizeof(ctx->physreg_use_count));

   for (unsigned i = 0; i < ctx->entry_count; i++) {
      struct u_copy *entry = &copies[i];

      ctx->entries[i] = *entry;

      for (unsigned j = 0; j < entry->size; j++) {
         if (is_real(entry))
            ctx->physreg_use_count[entry->src + j]++;

         /* Copies should not have overlapping destinations. */
         assert(!ctx->physreg_dst[entry->dst + j]);
         ctx->physreg_dst[entry->dst + j] = entry;
      }
   }

   bool progress = true;
   while (progress) {
      progress = false;

      /* Step 1: resolve paths in the transfer graph. This means finding
       * copies whose destination aren't blocked by something else and then
       * emitting them, continuing this process until every copy is blocked
       * and there are only cycles left.
       *
       * TODO: We should note that src is also available in dst to unblock
       * cycles that src is involved in.
       */

      for (unsigned i = 0; i < ctx->entry_count; i++) {
         struct u_copy *entry = &ctx->entries[i];
         if (!entry->done && !entry_blocked(entry, ctx)) {
            entry->done = true;
            progress = true;
            options->copy(entry, options->data);
            for (unsigned j = 0; j < entry->size; j++) {
               if (is_real(entry))
                  ctx->physreg_use_count[entry->src + j]--;
               ctx->physreg_dst[entry->dst + j] = NULL;
            }
         }
      }

      if (progress)
         continue;

      /* Step 2: Find partially blocked copies and split them. In the
       * mergedregs case, we can 32-bit copies which are only blocked on one
       * 16-bit half, and splitting them helps get things moving.
       *
       * We can skip splitting copies if the source isn't a register,
       * however, because it does not unblock anything and therefore doesn't
       * contribute to making forward progress with step 1. These copies
       * should still be resolved eventually in step 1 because they can't be
       * part of a cycle.
       */
      for (unsigned i = 0; i < ctx->entry_count; i++) {
         struct u_copy *entry = &ctx->entries[i];
         if (entry->done || (entry->size != 2))
            continue;

         if (((ctx->physreg_use_count[entry->dst] == 0 ||
               ctx->physreg_use_count[entry->dst + 1] == 0)) &&
             is_real(entry)) {
            split_32bit_copy(ctx, entry);
            progress = true;
         }
      }
   }

   /* Step 3: resolve cycles through swapping.
    *
    * At this point, the transfer graph should consist of only cycles.
    * The reason is that, given any physreg n_1 that's the source of a
    * remaining entry, it has a destination n_2, which (because every
    * copy is blocked) is the source of some other copy whose destination
    * is n_3, and so we can follow the chain until we get a cycle. If we
    * reached some other node than n_1:
    *
    *  n_1 -> n_2 -> ... -> n_i
    *          ^             |
    *          |-------------|
    *
    *  then n_2 would be the destination of 2 copies, which is illegal
    *  (checked above in an assert). So n_1 must be part of a cycle:
    *
    *  n_1 -> n_2 -> ... -> n_i
    *  ^                     |
    *  |---------------------|
    *
    *  and this must be only cycle n_1 is involved in, because any other
    *  path starting from n_1 would also have to end in n_1, resulting in
    *  a node somewhere along the way being the destination of 2 copies
    *  when the 2 paths merge.
    *
    *  The way we resolve the cycle is through picking a copy (n_1, n_2)
    *  and swapping n_1 and n_2. This moves n_1 to n_2, so n_2 is taken
    *  out of the cycle:
    *
    *  n_1 -> ... -> n_i
    *  ^              |
    *  |--------------|
    *
    *  and we can keep repeating this until the cycle is empty.
    */

   for (unsigned i = 0; i < ctx->entry_count; i++) {
      struct u_copy *entry = &ctx->entries[i];
      if (entry->done)
         continue;

      assert(is_real(entry));

      /* catch trivial copies */
      if (entry->dst == entry->src) {
         entry->done = true;
         continue;
      }

      options->swap(entry, options->data);

      /* Split any blocking copies whose sources are only partially
       * contained within our destination.
       */
      if (entry->size == 1) {
         for (unsigned j = 0; j < ctx->entry_count; j++) {
            struct u_copy *blocking = &ctx->entries[j];

            if (blocking->done)
               continue;

            if (blocking->src <= entry->dst &&
                blocking->src + 1 >= entry->dst &&
                blocking->size == 2) {
               split_32bit_copy(ctx, blocking);
            }
         }
      }

      /* Update sources of blocking copies.
       *
       * Note: at this point, every blocking copy's source should be
       * contained within our destination.
       */
      for (unsigned j = 0; j < ctx->entry_count; j++) {
         struct u_copy *blocking = &ctx->entries[j];
         if (blocking->src >= entry->dst &&
             blocking->src < entry->dst + entry->size) {
            blocking->src = entry->src + (blocking->src - entry->dst);
         }
      }

      entry->done = true;
   }
}
