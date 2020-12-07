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

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isa.h"

extern const struct isa_bitset *__instruction[];

static void decode(const struct isa_bitset *bitset, uint64_t val, FILE *out);


/**
 * Find the bitset in NULL terminated bitset hiearchy root table which
 * matches against 'val'
 */
static const struct isa_bitset *
find_bitset(const struct isa_bitset **bitsets, uint64_t val)
{
	const struct isa_bitset *match = NULL;
	for (int n = 0; bitsets[n]; n++) {
		uint64_t m = (val & bitsets[n]->mask) & ~bitsets[n]->dontcare;

		if (m != bitsets[n]->match)
			continue;

		/* We should only have exactly one match
		 *
		 * TODO more complete/formal way to validate that any given
		 * bit pattern will only have a single match?
		 */
		assert(!match);

		match = bitsets[n];
	}

	return match;
}

static const struct isa_field *
find_field(const struct isa_bitset *bitset, const char *name)
{
	for (int i = 0; i < bitset->num_fields; i++) {
		if (!strcmp(name, bitset->fields[i].name)) {
			return &bitset->fields[i];
		}
	}

	if (bitset->parent) {
		return find_field(bitset->parent, name);
	}

	return NULL;
}

static const char *
get_display(const struct isa_bitset *bitset)
{
	while (bitset) {
		if (bitset->display)
			return bitset->display;
		bitset = bitset->parent;
	}
	return NULL;
}

/**
 * Decode a field that is itself another bitset type
 */
static void
decode_bitset_field(const struct isa_field *field, uint64_t val, FILE *out)
{
	const struct isa_bitset *b = find_bitset(field->bitsets, val);
	if (!b) {
		printf("no-match: BITSET: '%s': 0x%"PRIx64"\n", field->name, val);
		return;
	}
	decode(b, val, out);
}

static void
decode_enum_field(const struct isa_field *field, uint64_t val, FILE *out)
{
	for (unsigned i = 0; i < field->num_enums; i++) {
		if (field->enums[i].val == val) {
			fprintf(out, "%s", field->enums[i].display);
			return;
		}
	}

	fprintf(out, "%u", (unsigned)val);
}

static void
decode_field(const struct isa_bitset *bitset, const char *field_name,
		uint64_t val, FILE *out)
{
	/* Special case 'NAME' maps to instruction/bitset name: */
	if (!strcmp("NAME", field_name)) {
		fprintf(out, "%s", bitset->name);
		return;
	}

	const struct isa_field *field = find_field(bitset, field_name);

	if (!field) {
		printf("no field '%s'\n", field_name);
		return;
	}

	/* extract out raw field value: */
	val = (val >> field->low) & ((1ul << (1 + field->high - field->low)) - 1);

	//printf("%s: %"PRIu64"\n", field->name, val);
	switch (field->type) {
	/* Basic types: */
	case TYPE_INT:
		// TODO sign extension:
		fprintf(out, "%d", (int)val);
		break;
	case TYPE_UINT:
		fprintf(out, "%u", (unsigned)val);
		break;
	case TYPE_BOOL:
		if (field->display) {
			if (val) {
				fprintf(out, "%s", field->display);
			}
		} else {
			fprintf(out, "%u", (unsigned)val);
		}
		break;
	case TYPE_ENUM:
		decode_enum_field(field, val, out);
		break;

	/* For fields that are decoded with another bitset hierarchy: */
	case TYPE_BITSET:
		decode_bitset_field(field, val, out);
		break;
	}
}

static void
decode(const struct isa_bitset *bitset, uint64_t val, FILE *out)
{
	const char *display = get_display(bitset);

	if (!display) {
		printf("%s: no display", bitset->name);
		return;
	}

	//printf("%s '%s'\n", bitset->name, display);

	const char *p = display;

	while (*p != '\0') {
		if (*p == '{') {
			const char *e = ++p;
			while (*e != '}') {
				e++;
			}

			char *field_name = strndup(p, e-p);
			decode_field(bitset, field_name, val, out);
			free(field_name);

			p = e;
		} else {
			fputc(*p, out);
		}
		p++;
	}
}

void
isa_decode(void *bin, int sz, FILE *out)
{
	int num_instr = sz / 8;
	uint64_t *instrs = bin;

	for (int i = 0; i < num_instr; i++) {
		const struct isa_bitset *b = find_bitset(__instruction, instrs[i]);
		if (!b) {
			printf("no match: %016"PRIx64"\n", instrs[i]);
			continue;
		}
		//fprintf(out, "%016"PRIx64": ", instrs[i]);
		decode(b, instrs[i], out);
		fprintf(out, "\n");
	}

}
