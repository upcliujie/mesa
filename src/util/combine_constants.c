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
 *   all values must be in registers.
 *
 * See \c struct value for more details.
 */

#include <math.h> /* for isnan() */
#include <limits.h> /* for UINT_MAX */
#include <stdlib.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include "util/bitset.h"
#include "util/combine_constants.h"
#include "util/u_math.h"

#define VALUE_INDEX                  0
#define FLOAT_NEG_INDEX              1
#define INT_NEG_INDEX                2
#define MAX_NUM_REACHABLE            3

#define VALUE_EXISTS                 (1 << VALUE_INDEX)
#define FLOAT_NEG_EXISTS             (1 << FLOAT_NEG_INDEX)
#define INT_NEG_EXISTS               (1 << INT_NEG_INDEX)

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
      if (base_type == float_only)
         return !util_is_half_nan(v.i16);
      else
         return v.i16 != 0 && v.i16 != INT16_MIN;

   case 32:
      if (base_type == float_only)
         return !isnan(v.f32);
      else
         return v.i32 != 0 && v.i32 != INT32_MIN;

   case 64:
      if (base_type == float_only)
         return !isnan(v.f64);
      else
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

static constant_value
absolute(constant_value v, unsigned bit_size, enum interpreted_type base_type)
{
   /* either_type does not make sense in this context. */
   assert(base_type == float_only || base_type == integer_only);

   constant_value ret = { 0, };

   switch (bit_size) {
   case 8:
      assert(base_type == integer_only);
      ret.i8 = abs(v.i8);
      break;

   case 16:
      if (base_type == float_only)
         ret.u16 = v.u16 & 0x7fff;
      else
         ret.i16 = abs(v.i16);
      break;

   case 32:
      if (base_type == float_only)
         ret.f32 = fabs(v.f32);
      else
         ret.i32 = abs(v.i32);
      break;

   case 64:
      if (base_type == float_only)
         ret.f64 = fabs(v.f64);
      else {
         if (sizeof(v.i64) == sizeof(long int)) {
            ret.i64 = labs((long int) v.i64);
         } else {
            assert(sizeof(v.i64) == sizeof(long long int));
            ret.i64 = llabs((long long int) v.i64);
         }
      }
      break;

   default:
      unreachable("unsupported bit-size should have already been filtered.");
   }

   return ret;
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
      if (negation_exists(v, bit_size, float_only))
         *reachable_mask |= FLOAT_NEG_EXISTS;
   }

   if (type == integer_only || type == either_type) {
      if (negation_exists(v, bit_size, integer_only))
         *reachable_mask |= INT_NEG_EXISTS;
   }

   /* Calculate the extended reaching mask.  All of the "is this negation
    * possible" was already determined for the reachable_mask, so reuse that
    * data.
    */
   if (type == float_only || type == either_type) {
      if (*reachable_mask & FLOAT_NEG_EXISTS)
         *reaching_mask |= FLOAT_NEG_EXISTS;
   }

   if (type == integer_only || type == either_type) {
      if (*reachable_mask & INT_NEG_EXISTS)
         *reaching_mask |= INT_NEG_EXISTS;
   }
}

static void
calculate_reachable_values(constant_value v,
                           unsigned bit_size,
                           unsigned reachable_mask,
                           constant_value *reachable_values)
{
   memset(reachable_values, 0, MAX_NUM_REACHABLE * sizeof(reachable_values[0]));

   reachable_values[VALUE_INDEX] = v;

   if (reachable_mask & INT_NEG_EXISTS) {
      const constant_value neg = negate(v, bit_size, integer_only);

      reachable_values[INT_NEG_INDEX] = neg;
   }

   if (reachable_mask & FLOAT_NEG_EXISTS) {
      const constant_value neg = negate(v, bit_size, float_only);

      reachable_values[FLOAT_NEG_INDEX] = neg;
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

/** Can these values be the same with one level of negation? */
static bool
value_can_equal(const constant_value *from, uint8_t reachable_mask,
                constant_value to, uint8_t reaching_mask,
                unsigned bit_size)
{
   const uint8_t combined_mask = reachable_mask & reaching_mask;

   return value_equal(from[VALUE_INDEX], to, bit_size) ||
          ((combined_mask & INT_NEG_EXISTS) &&
           value_equal(from[INT_NEG_INDEX], to, bit_size)) ||
          ((combined_mask & FLOAT_NEG_EXISTS) &&
           value_equal(from[FLOAT_NEG_INDEX], to, bit_size));
}

static void
preprocess_candidates(struct value *candidates, unsigned num_candidates)
{
   /* Calculate the reaching_mask and reachable_mask for each candidate. */
   for (unsigned i = 0; i < num_candidates; i++) {
      calculate_masks(candidates[i].value,
                      candidates[i].type,
                      candidates[i].bit_size,
                      &candidates[i].reachable_mask,
                      &candidates[i].reaching_mask);

      /* If negations are not allowed, then only the original value is
       * reaching.
       */
      if (candidates[i].no_negations)
         candidates[i].reaching_mask = 0;
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

static bool
reaching_value_exists(const struct value *c,
                      const struct combine_constants_value *values,
                      unsigned num_values)
{
   constant_value reachable_values[MAX_NUM_REACHABLE];

   calculate_reachable_values(c->value, c->bit_size, c->reaching_mask,
                              reachable_values);

   /* Check to see if the value is already in the result set. */
   for (unsigned j = 0; j < num_values; j++) {
      if (c->bit_size == values[j].bit_size &&
          value_can_equal(reachable_values, c->reaching_mask,
                          values[j].value, c->reaching_mask,
                          c->bit_size)) {
         return true;
      }
   }

   return false;
}

static struct combine_constants_result *
combine_constants_greedy(struct value *candidates, unsigned num_candidates)
{
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

   /* In the worst case, the number of output values will be equal to the
    * number of input values.  Allocate a buffer that is known to be large
    * enough now, and it can be reduced later.
    */
   struct combine_constants_value *values =
      calloc(num_candidates, sizeof(values[0]));

   if (values == NULL) {
      util_combine_constants_result_dtor(result);
      return NULL;
   }

   result->values_to_emit = values;
   result->num_values_to_emit = 0;

   BITSET_WORD *remain = calloc(BITSET_WORDS(num_candidates), sizeof(remain[0]));

   if (remain == NULL) {
      util_combine_constants_result_dtor(result);
      return NULL;
   }

   memset(remain, 0xff, BITSET_WORDS(num_candidates) * sizeof(remain[0]));

   unsigned num_values = 0;

   /* Operate in three passes.  The first pass handles all values that must be
    * emitted and for which a negation cannot exist.
    */
   unsigned i;
   for (i = 0; i < num_candidates; i++) {
      if (candidates[i].allow_one_constant ||
          (candidates[i].reaching_mask & (FLOAT_NEG_EXISTS | INT_NEG_EXISTS))) {
         continue;
      }

      /* Check to see if the value is already in the result set. */
      bool found = false;
      for (unsigned j = 0; j < num_values; j++) {
         if (candidates[i].bit_size == values[j].bit_size &&
             value_equal(candidates[i].value,
                         values[j].value,
                         candidates[i].bit_size)) {
            found = true;
            break;
         }
      }

      if (!found) {
         values[num_values].value = candidates[i].value;
         values[num_values].first_user = 0;
         values[num_values].num_users = 0;
         values[num_values].bit_size = candidates[i].bit_size;
         num_values++;
      }

      BITSET_CLEAR(remain, i);
   }

   /* The second pass handles all values that must be emitted and for which a
    * negation can exist.
    */
   BITSET_FOREACH_SET(i, remain, num_candidates) {
      if (candidates[i].allow_one_constant)
         continue;

      assert(candidates[i].reaching_mask & (FLOAT_NEG_EXISTS | INT_NEG_EXISTS));

      if (!reaching_value_exists(&candidates[i], values, num_values)) {
         values[num_values].value =
            absolute(candidates[i].value, candidates[i].bit_size, candidates[i].type);
         values[num_values].first_user = 0;
         values[num_values].num_users = 0;
         values[num_values].bit_size = candidates[i].bit_size;
         num_values++;
      }

      BITSET_CLEAR(remain, i);
   }

   /* The third pass handles all of the values that may not have to be
    * emitted.  These are the values where allow_one_constant is set.
    */
   BITSET_FOREACH_SET(i, remain, num_candidates) {
      assert(candidates[i].allow_one_constant);

      /* The BITSET_FOREACH_SET macro does not detect changes to the bitset
       * that occur within the current word.  Since code in this loop may
       * clear bits from the set, re-test here.
       */
      if (!BITSET_TEST(remain, i))
         continue;

      assert(candidates[i].next_src != NULL);

      const struct value *const other_candidate = candidates[i].next_src;
      const unsigned j = other_candidate - candidates;

      if (!reaching_value_exists(&candidates[i], values, num_values)) {
         /* Before emitting a value, see if a match for the other source of
          * the instruction exists.
          */
         if (!reaching_value_exists(&candidates[j], values, num_values)) {
            values[num_values].value = candidates[i].value;
            values[num_values].first_user = 0;
            values[num_values].num_users = 0;
            values[num_values].bit_size = candidates[i].bit_size;
            num_values++;
         }
      }

      /* Mark both sources as handled. */
      BITSET_CLEAR(remain, i);
      BITSET_CLEAR(remain, j);
   }

   /* As noted above, there will never be more values in the output than in
    * the input.  If there are fewer values, reduce the size of the
    * allocation.
    */
   if (num_values < num_candidates) {
      values = realloc(values, sizeof(values[0]) * num_values);

      /* Is it even possible for a reducing realloc to fail? */
      assert(values != NULL);
   }

   /* Create the mapping from "combined" constants to list of candidates
    * passed in by the caller.
    */
   result->values_to_emit = values;
   result->num_values_to_emit = num_values;

   memset(remain, 0xff, BITSET_WORDS(num_candidates) * sizeof(remain[0]));

   unsigned total_users = 0;

   for (unsigned value_idx = 0; value_idx < num_values; value_idx++) {
      values[value_idx].first_user = total_users;

      uint8_t reachable_mask;
      uint8_t unused_mask;

      calculate_masks(values[value_idx].value, either_type,
                      values[value_idx].bit_size,
                      &reachable_mask, &unused_mask);

      constant_value reachable_values[MAX_NUM_REACHABLE];

      calculate_reachable_values(values[value_idx].value,
                                 values[value_idx].bit_size,
                                 reachable_mask, reachable_values);

      for (unsigned i = 0; i < num_candidates; i++) {
         bool matched = false;

         if (!BITSET_TEST(remain, i))
            continue;

         if (candidates[i].bit_size != values[value_idx].bit_size)
            continue;

         if (value_equal(candidates[i].value, values[value_idx].value,
                         values[value_idx].bit_size)) {
            result->user_map[total_users].index = i;
            result->user_map[total_users].type = candidates[i].type;
            result->user_map[total_users].negate = false;
            total_users++;

            matched = true;
            BITSET_CLEAR(remain, i);
         } else {
            const uint8_t combined_mask = reachable_mask &
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
               /* Finding a match on this path implies that the user must
                * allow source negations.
                */
               assert(!candidates[i].no_negations);

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

      assert(total_users > values[value_idx].first_user);
      values[value_idx].num_users = total_users - values[value_idx].first_user;
   }

   /* Verify that all of the values were emitted by the loop above.  If any
    * bits are still set in remain, then some value was not emitted.  The use
    * of memset to populate remain prevents the use of a more performant loop.
    */
#ifndef NDEBUG
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

struct combine_constants_result *
util_combine_constants(struct value *candidates, unsigned num_candidates)
{
   preprocess_candidates(candidates, num_candidates);

   return combine_constants_greedy(candidates, num_candidates);
}

void
util_combine_constants_result_dtor(struct combine_constants_result *result)
{
   if (result != NULL) {
      free(result->values_to_emit);
      free(result);
   }
}
