/*
 * Copyright (C) 2022 Alyssa Rosenzweig <alyssa@rosenzweig.io>
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

#include "agx_compiler.h"
#include "agx_builder.h"
#include "util/u_lower_parallel_copy.h"

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
 *
 * We only handles register-register copies, not general agx_index sources. This
 * suffices for its internal use for register allocation.
 */

static struct agx_copy
copy_for_entry(const struct u_copy *entry)
{
   return (struct agx_copy) {
      .dest = entry->dst,
      .src = entry->src,
      .size = (entry->size == 1) ? AGX_SIZE_16 : AGX_SIZE_32,
   };
}

static void
do_copy(const struct u_copy *entry, void *data)
{
   agx_builder *b = data;
   struct agx_copy copy = copy_for_entry(entry);

   agx_mov_to(b, agx_register(copy.dest, copy.size),
                 agx_register(copy.src, copy.size));
}

static void
do_swap(const struct u_copy *entry, void *data)
{
   agx_builder *b = data;
   struct agx_copy copy = copy_for_entry(entry);

   if (copy.dest == copy.src)
      return;

   agx_index x = agx_register(copy.dest, copy.size);
   agx_index y = agx_register(copy.src, copy.size);

   agx_xor_to(b, x, x, y);
   agx_xor_to(b, y, x, y);
   agx_xor_to(b, x, x, y);
}

void
agx_emit_parallel_copies(agx_builder *b, struct agx_copy *copies, unsigned n)
{
   struct lower_parallel_copy_options options = {
      .num_regs = 256,
      .copy = do_copy,
      .swap = do_swap,
      .data = b
   };

   struct u_copy *uc = calloc(sizeof(struct u_copy), n);

   for (unsigned i = 0; i < n; ++i) {
      uc[i] = (struct u_copy) {
         .dst = copies[i].dest,
         .size = agx_size_align_16(copies[i].size),
         .src = copies[i].src
      };
   }

   u_lower_parallel_copy(&options, uc, n);
   free(uc);
}
