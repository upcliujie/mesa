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

#define ARRAY_SIZE(a) (sizeof (a) / sizeof *(a))

extern const struct isa_bitset *__instruction[];

/**
 * Current decode state
 *
 * TODO maybe we end up wanting something similar(ish) for encode?
 */
struct decode_state {
	FILE *out;

	/**
	 * We allow a limited amount of expression evaluation recursion, but
	 * not recursive evaluation of any given expression, to prevent infinite
	 * recursion.
	 */
	const struct isa_expr *expr_stack[4];
	int expr_sp;

	/**
	 * Conditionals in nested bitset decoding can refer back out to a higher
	 * level to resolve fields (symbols) used in conditionals, we we have a
	 * stack of value+bitset
	 */
	struct {
		uint64_t val;
		const struct isa_bitset *bitset;
	} stack[4];

	int sp;
};

static void display(struct decode_state *state);
static uint64_t decode_field(struct decode_state *state, const char *field_name, int up);

static bool
push_expr(struct decode_state *state, const struct isa_expr *expr)
{
	for (int i = state->expr_sp - 1; i > 0; i--) {
		if (state->expr_stack[i] == expr) {
			return false;
		}
	}
	state->expr_stack[state->expr_sp++] = expr;
	return true;
}

static void
pop_expr(struct decode_state *state)
{
	assert(state->expr_sp > 0);
	state->expr_sp--;
}

static void
push_bitset(struct decode_state *state, const struct isa_bitset *bitset, uint64_t val)
{
	state->stack[state->sp].bitset = bitset;
	state->stack[state->sp].val = val;
	state->sp++;
}

static void
pop_bitset(struct decode_state *state)
{
	assert(state->sp > 0);
	state->sp--;
}

static const struct isa_bitset *
current_bitset(struct decode_state *state, int up)
{
	int idx = state->sp - up;
	assert(idx > 0);
	return state->stack[idx - 1].bitset;
}

static const uint64_t
current_val(struct decode_state *state, int up)
{
	int idx = state->sp - up;
	assert(idx > 0);
	return state->stack[idx - 1].val;
}

/**
 * Evaluate an expression, returning it's resulting value
 */
static uint64_t
evaluate_expr(struct decode_state *state, const struct isa_expr *expr)
{
	int64_t stack[8], tmp;
	int sp = 0;
#define eval_dbg(...)
	//printf(__VA_ARGS__)
#define push(v) do { \
			assert(sp < ARRAY_SIZE(stack)); \
			eval_dbg("EVAL: %s", #v); \
			stack[sp] = (v); \
			sp++; \
			eval_dbg(" -> %"PRId64"\n", stack[sp-1]); \
		} while (0)
#define peek() ({ \
			assert(sp < ARRAY_SIZE(stack)); \
			stack[sp - 1]; \
		})
#define pop() ({ \
			assert(sp > 0); \
			--sp; \
			stack[sp]; \
		})

	if (!push_expr(state, expr))
		return 0;

	eval_dbg("EVAL: %p (%u)\n", expr, expr->num_instructions);
	for (int pc = 0; pc < expr->num_instructions; pc++) {
		switch (expr->instructions[pc].opc) {
		case ISA_INSTR_LITERAL:
			push(expr->instructions[pc].literal);
			break;
		case ISA_INSTR_VAR:
			eval_dbg("EVAL: variable=%s\n", expr->instructions[pc].variable);
			push(decode_field(state, expr->instructions[pc].variable, 0));
			break;
		case ISA_INSTR_DUP:
			push(peek());
			break;
		case ISA_INSTR_JMP:
			pc += pop();
			assert(pc > 0);
			break;
		case ISA_INSTR_RET:
			goto out;
		case ISA_INSTR_RETLIT:
			push(expr->instructions[pc].literal);
			goto out;
		case ISA_INSTR_RETIF:
			tmp = pop();
			if (tmp) {
				push(tmp);
				goto out;
			}
			break;
		case ISA_INSTR_NE:
			push(pop() != pop());
			break;
		case ISA_INSTR_EQ:
			push(pop() == pop());
			break;
		case ISA_INSTR_GT:
			push(pop() > pop());
			break;
		case ISA_INSTR_NOT:
			push(!pop());
			break;
		case ISA_INSTR_OR:
			push(pop() | pop());
			break;
		case ISA_INSTR_AND:
			push(pop() & pop());
			break;
		case ISA_INSTR_LSH:
			push(pop() << pop());
			break;
		case ISA_INSTR_RSH:
			push(pop() >> pop());
			break;
		}
	}

out:
	pop_expr(state);

	return pop();
#undef pop
#undef peek
#undef push
}

/**
 * Find the bitset in NULL terminated bitset hiearchy root table which
 * matches against 'val'
 */
static const struct isa_bitset *
find_bitset(struct decode_state *state, const struct isa_bitset **bitsets,
		uint64_t val)
{
	const struct isa_bitset *match = NULL;
	for (int n = 0; bitsets[n]; n++) {
		uint64_t m = (val & bitsets[n]->mask) & ~bitsets[n]->dontcare;

		if (m != bitsets[n]->match) {
			continue;
		}

		/* We should only have exactly one match
		 *
		 * TODO more complete/formal way to validate that any given
		 * bit pattern will only have a single match?
		 */
		if (match) {
			printf("bitset conflict: %s vs %s\n", match->name,
					bitsets[n]->name);
			return NULL;
		}

		match = bitsets[n];
	}

	return match;
}

static const struct isa_field *
find_field(struct decode_state *state, const struct isa_bitset *bitset,
		const char *name)
{
	for (unsigned i = 0; i < bitset->num_cases; i++) {
		const struct isa_case *c = bitset->cases[i];
		if (c->expr && !evaluate_expr(state, c->expr))
			continue;
		for (unsigned i = 0; i < c->num_fields; i++) {
			if (!strcmp(name, c->fields[i].name)) {
				return &c->fields[i];
			}
		}
	}

	if (bitset->parent) {
		const struct isa_field *f = find_field(state, bitset->parent, name);
		if (f) {
			return f;
		}
	}

	return NULL;
}

static const char *
get_display(struct decode_state *state, const struct isa_bitset *bitset)
{
	for (unsigned i = 0; i < bitset->num_cases; i++) {
		const struct isa_case *c = bitset->cases[i];
		if (!c->display)
			continue;
		if (c->expr && !evaluate_expr(state, c->expr))
			continue;
		return c->display;
	}

	if (bitset->parent) {
		return get_display(state, bitset->parent);
	}

	return NULL;
}

/**
 * Decode a field that is itself another bitset type
 */
static void
display_bitset_field(struct decode_state *state, const struct isa_field *field, uint64_t val)
{
	const struct isa_bitset *b = find_bitset(state, field->bitsets, val);
	if (!b) {
		printf("no match: BITSET: '%s': 0x%"PRIx64"\n", field->name, val);
		return;
	}
	push_bitset(state, b, val);
	display(state);
	pop_bitset(state);
}

static void
display_enum_field(struct decode_state *state, const struct isa_field *field, uint64_t val)
{
	const struct isa_enum *e = field->enums;
	for (unsigned i = 0; i < e->num_values; i++) {
		if (e->values[i].val == val) {
			fprintf(state->out, "%s", e->values[i].display);
			return;
		}
	}

	fprintf(state->out, "%u", (unsigned)val);
}

static uint64_t
decode_field(struct decode_state *state, const char *field_name, int up)
{
	if (up >= state->sp) {
		printf("no field '%s'\n", field_name);
		return 0;
	}

	const struct isa_bitset *bitset = current_bitset(state, up);
	const struct isa_field *field = find_field(state, bitset, field_name);

	if (!field) {
		return decode_field(state, field_name, up+1);
	}

	/* extract out raw field value: */
	uint64_t val;
	if (field->expr) {
		val = evaluate_expr(state, field->expr);
	} else {
		val = current_val(state, up);
		val = (val >> field->low) & ((1ul << (1 + field->high - field->low)) - 1);
	}

	return val;
}

static void
display_field(struct decode_state *state, const char *field_name, int up)
{
	if (up >= state->sp) {
		printf("no field '%s'\n", field_name);
		return;
	}

	const struct isa_bitset *bitset = current_bitset(state, up);

	/* Special case 'NAME' maps to instruction/bitset name: */
	if (!strcmp("NAME", field_name)) {
		fprintf(state->out, "%s", bitset->name);
		return;
	}

	const struct isa_field *field = find_field(state, bitset, field_name);

	if (!field) {
		display_field(state, field_name, up+1);
		return;
	}

	/* extract out raw field value: */
	uint64_t val;
	if (field->expr) {
		val = evaluate_expr(state, field->expr);
	} else {
		val = current_val(state, up);
		val = (val >> field->low) & ((1ul << (1 + field->high - field->low)) - 1);
	}

	//printf("%s: %"PRIu64"\n", field->name, val);
	switch (field->type) {
	/* Basic types: */
	case TYPE_INT:
		// TODO sign extension:
		fprintf(state->out, "%d", (int)val);
		break;
	case TYPE_UINT:
		fprintf(state->out, "%u", (unsigned)val);
		break;
	case TYPE_BOOL:
		if (field->display) {
			if (val) {
				fprintf(state->out, "%s", field->display);
			}
		} else {
			fprintf(state->out, "%u", (unsigned)val);
		}
		break;
	case TYPE_ENUM:
		display_enum_field(state, field, val);
		break;

	/* For fields that are decoded with another bitset hierarchy: */
	case TYPE_BITSET:
		display_bitset_field(state, field, val);
		break;
	}
}

static void
display(struct decode_state *state)
{
	const struct isa_bitset *bitset = current_bitset(state, 0);
	const char *display = get_display(state, bitset);

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
			display_field(state, field_name, 0);
			free(field_name);

			p = e;
		} else {
			fputc(*p, state->out);
		}
		p++;
	}
}

void
isa_decode(void *bin, int sz, FILE *out)
{
	struct decode_state state = {
			.out = out,
	};
	int num_instr = sz / 8;
	uint64_t *instrs = bin;

	for (int i = 0; i < num_instr; i++) {
		const struct isa_bitset *b =
				find_bitset(&state, __instruction, instrs[i]);
		if (!b) {
			printf("no match: %016"PRIx64"\n", instrs[i]);
			continue;
		}

		push_bitset(&state, b, instrs[i]);

		//fprintf(out, "%016"PRIx64": ", instrs[i]);
		display(&state);
		fprintf(out, "\n");

		pop_bitset(&state);
	}

}
