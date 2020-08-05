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

#ifndef UTIL_COMBINE_CONSTANTS_H
#define UTIL_COMBINE_CONSTANTS_H

#include <inttypes.h>
#include <stdbool.h>
#include "util/macros.h"

enum PACKED interpreted_type {
   float_only = 0,
   integer_only,
   either_type
};

typedef union {
   bool b;
   float f32;
   double f64;
   int8_t i8;
   uint8_t u8;
   int16_t i16;
   uint16_t u16;
   int32_t i32;
   uint32_t u32;
   int64_t i64;
   uint64_t u64;
} constant_value;

struct abstract_instruction;

struct value {
   /** Raw bit pattern of the value. */
   constant_value value;

   /** Instruction that uses this instance of the value. */
   struct abstract_instruction *instr;

   /** Size, in bits, of the value. */
   uint8_t bit_size;

   /**
    * Which source of instr is this value?
    *
    * \note This field is not actually used by \c util_combine_constants, but
    * it is generally very useful to callers.
    */
   uint8_t src;

   /**
    * In what ways can instr interpret this value?
    *
    * Choices are floating-point only, integer only, or either type.
    */
   enum interpreted_type type;

   /**
    * Only try to make a single source non-constant.
    *
    * On some architectures, some instructions require that all sources be
    * non-constant.  For example, the multiply-accumulate instruction on Intel
    * GPUs upto Gen11 require that all sources be non-constant.  Other
    * instructions, like the selection instruction, allow one constant source.
    *
    * If a single constant source is allowed, set this flag to true.
    *
    * If an instruction allows a single constant and it has only a signle
    * constant to begin, it should be included.  Various places in
    * \c combine_constants will assume that there are multiple constants if
    * \c ::allow_one_constant is set.  This may even be enforced by in-code
    * assertions.
    */
   bool allow_one_constant;

   /**
    * Restrict values that can reach this value to not include negations.
    *
    * This is useful for instructions that cannot have source modifiers.  For
    * example, on Intel GPUs the integer source of a shift instruction (e.g.,
    * SHL) can have a source modifier, but the integer source of the bitfield
    * insertion instruction (i.e., BFI2) cannot.  A pair of these instructions
    * might have sources that are negations of each other.  Using this flag
    * will ensure that the BFI2 does not have a negated source, but the SHL
    * might.
    */
   bool no_negations;

   /**
    * \name UtilCombineConstantsPrivate
    * Private data used only by util_combine_constants
    *
    * Any data stored in these fields will be overwritten by the call to
    * \c util_combine_constants.  No assumptions should be made about the
    * state of these fields after that function returns.
    */
   /**@{*/
   /** Mask of negations that can be generated from this value. */
   uint8_t reachable_mask;

   /** Mask of negations that can generate this value. */
   uint8_t reaching_mask;

   /**
    * Value with the next source from the same instruction.
    *
    * This pointer may be \c NULL.  If it is not \c NULL, it will form a
    * singly-linked circular list of values.  The list is unorderd.  That is,
    * as the list is iterated, the \c ::src values will be in arbitrary order.
    *
    * \todo Is it even possible for there to be more than two elements in this
    * list?  This pass does not operate on vecN instructions or intrinsics, so
    * the theoretical limit should be three.  However, instructions will all
    * constant sources should have been folded away.
    */
   struct value *next_src;
   /**@}*/
};

struct combine_constants_value {
   /** Raw bit pattern of the constant loaded. */
   constant_value value;

   /**
    * Index of the first user.
    *
    * This is the offset into \c combine_constants_result::user_map of the
    * first user of this value.
    */
   unsigned first_user;

   /** Number of users of this value. */
   unsigned num_users;

   /** Size, in bits, of the value. */
   uint8_t bit_size;
};

struct combine_constants_user {
   /** Index into the array of values passed to util_combine_constants. */
   unsigned index;

   /**
    * Manner in which the value should be interpreted in the instruction.
    *
    * This is only useful when ::negate is set.  Unless the corresponding
    * value::type is \c either_type, this field must have the same value as
    * value::type.
    */
   enum interpreted_type type;

   /** Should this value be negated to generate the original value? */
   bool negate;
};

struct combine_constants_result {

   unsigned num_values_to_emit;
   struct combine_constants_value *values_to_emit;

   struct combine_constants_user user_map[];
};

#ifdef __cplusplus
extern "C" {
#endif

struct combine_constants_result *util_combine_constants(struct value *values,
                                                        unsigned num_values);

void util_combine_constants_result_dtor(struct combine_constants_result *);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UTIL_COMBINE_CONSTANTS_H */
