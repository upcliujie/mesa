/*
 * Copyright © 2020 Google, Inc.
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

#ifndef _ISA_H_
#define _ISA_H_

#include <stdint.h>

/* TODO we could maybe make this a uint8_t array, with some helpers, to
 * support arbitrary sized patterns.. or add AND/OR/SHIFT support to
 * util/bitset.h?
 */
typedef uint64_t bitmask_t;

struct isa_bitset;

struct isa_enum {
	unsigned val;
	const char *display;
};

struct isa_field {
	const char *name;
	const char *display;
	unsigned low;
	unsigned high;
	enum {
		/* Basic types: */
		TYPE_INT,
		TYPE_UINT,
		TYPE_BOOL,
		TYPE_ENUM,

		/* Register types: */
		TYPE_REG_GPR,
		TYPE_REG_CONST,
		TYPE_REG_REL_GPR,
		TYPE_REG_REL_CONST,

		/* For fields that are decoded with another bitset hierarchy: */
		TYPE_BITSET,
	} type;
	union {
		const struct isa_bitset **bitsets;     /* if type==TYPE_BITSET */
		struct {
			unsigned num_enums;
			const struct isa_enum *enums;
		};
	};
};

struct isa_bitset {
	const struct isa_bitset *parent;
	const char *name;
	const char *display;
	bitmask_t match;
	bitmask_t dontcare;
	bitmask_t mask;
	unsigned num_fields;
	struct isa_field fields[];
};

#endif /* _ISA_H_ */
