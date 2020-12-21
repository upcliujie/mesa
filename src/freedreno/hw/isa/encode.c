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

#include "util/log.h"

#include "ir3/ir3.h"
#include "ir3/ir3_shader.h"
#include "ir3/instr-a3xx.h"  // TODO move opc's and other useful things to ir3-instr.h or so

struct encode_state {
	/**
	 * The instruction which is currently being encoded
	 */
	struct ir3_instruction *instr;
};

/*
 * Helpers defining how to map from ir3_instruction/ir3_register/etc to fields
 * to be encoded:
 */

static inline bool
extract_SRC1_R(struct ir3_instruction *instr)
{
	if (instr->nop) {
		assert(!instr->repeat);
		return instr->nop & 0x1;
	}
	return !!(instr->regs[1]->flags & IR3_REG_R);
}

static inline bool
extract_SRC2_R(struct ir3_instruction *instr)
{
	if (instr->nop) {
		assert(!instr->repeat);
		return (instr->nop >> 1) & 0x1;
	}
	/* src2 does not appear in all cat2, but SRC2_R does (for nop encoding) */
	if (instr->regs_count > 2)
		return !!(instr->regs[2]->flags & IR3_REG_R);
	return 0;
}

static inline opc_t
__instruction_case(struct encode_state *s, struct ir3_instruction *instr)
{
	return instr->opc;
}

static inline unsigned
extract_ABSNEG(struct ir3_register *reg)
{
	// TODO generate enums for this:
	if (reg->flags & (IR3_REG_FNEG | IR3_REG_SNEG | IR3_REG_BNOT)) {
		if (reg->flags & (IR3_REG_FABS | IR3_REG_SABS)) {
			return 3; // ABSNEG
		} else {
			return 1; // NEG
		}
	} else if (reg->flags & (IR3_REG_FABS | IR3_REG_SABS)) {
		return 2; // ABS
	} else {
		return 0;
	}
}

typedef enum {
	REG_MULITSRC_IMMED,
	REG_MULTISRC_IMMED_FLUT_FULL,
	REG_MULTISRC_IMMED_FLUT_HALF,
	REG_MULTISRC_GPR,
	REG_MULTISRC_CONST,
	REG_MULTISRC_RELATIVE_GPR,
	REG_MULTISRC_RELATIVE_CONST,
} reg_multisrc_t;

static inline reg_multisrc_t
__multisrc_case(struct encode_state *s, struct ir3_register *reg)
{
	if (reg->flags & IR3_REG_IMMED) {
		assert(opc_cat(s->instr->opc) == 2);
		if (ir3_cat2_int(s->instr->opc)) {
			return REG_MULITSRC_IMMED;
		} else if (reg->flags & IR3_REG_HALF) {
			return REG_MULTISRC_IMMED_FLUT_HALF;
		} else {
			return REG_MULTISRC_IMMED_FLUT_FULL;
		}
	} else if (reg->flags & IR3_REG_RELATIV) {
		if (reg->flags & IR3_REG_CONST) {
			return REG_MULTISRC_RELATIVE_CONST;
		} else {
			return REG_MULTISRC_RELATIVE_GPR;
		}
	} else if (reg->flags & IR3_REG_CONST) {
		return REG_MULTISRC_CONST;
	} else {
		return REG_MULTISRC_GPR;
	}
}

#include "encode.h"


void * isa_assemble(struct ir3_shader_variant *v);

void *
isa_assemble(struct ir3_shader_variant *v)
{
	uint64_t *ptr, *instrs;
	struct ir3_info *info = &v->info;
	struct ir3 *shader = v->ir;
	const struct ir3_compiler *compiler = v->shader->compiler;

	memset(info, 0, sizeof(*info));
	info->data          = v;
	info->max_reg       = -1;
	info->max_half_reg  = -1;
	info->max_const     = -1;
	info->multi_dword_ldp_stp = false;

	uint32_t instr_count = 0;
	foreach_block (block, &shader->block_list) {
		foreach_instr (instr, &block->instr_list) {
			instr_count++;
		}
	}

	v->instrlen = DIV_ROUND_UP(instr_count, compiler->instr_align);

	/* Pad out with NOPs to instrlen, including at least 4 so that cffdump
	 * doesn't try to decode the following data as instructions (such as the
	 * next stage's shader in turnip)
	 */
	info->size = MAX2(v->instrlen * compiler->instr_align, instr_count + 4) * 8;
	info->sizedwords = info->size / 4;

	if (v->constant_data_size) {
		/* Make sure that where we're about to place the constant_data is safe
		 * to indirectly upload from.
		 */
		info->constant_data_offset = ALIGN_POT(info->size,
				v->shader->compiler->const_upload_unit * 16);
		info->size = info->constant_data_offset + v->constant_data_size;
	}

	/* Pad out the size so that when turnip uploads the shaders in
	 * sequence, the starting offset of the next one is properly aligned.
	 */
	info->size = ALIGN_POT(info->size, compiler->instr_align * 8);

	ptr = instrs = rzalloc_size(v, info->size);

	foreach_block (block, &shader->block_list) {
		unsigned sfu_delay = 0;

		foreach_instr (instr, &block->instr_list) {
			struct encode_state s = {
					.instr = instr,
			};

			*instrs = encode__instruction(&s, instr);

			if ((instr->opc == OPC_BARY_F) && (instr->regs[0]->flags & IR3_REG_EI))
				info->last_baryf = info->instrs_count;

			unsigned instrs_count = 1 + instr->repeat + instr->nop;
			unsigned nops_count = instr->nop;

			if (instr->opc == OPC_NOP) {
				nops_count = 1 + instr->repeat;
				info->instrs_per_cat[0] += nops_count;
			} else {
				info->instrs_per_cat[opc_cat(instr->opc)] += instrs_count;
				info->instrs_per_cat[0] += nops_count;
			}

			if (instr->opc == OPC_MOV) {
				if (instr->cat1.src_type == instr->cat1.dst_type) {
					info->mov_count += 1 + instr->repeat;
				} else {
					info->cov_count += 1 + instr->repeat;
				}
			}

			info->instrs_count += instrs_count;
			info->nops_count += nops_count;

			instrs++;

			if (instr->flags & IR3_INSTR_SS) {
				info->ss++;
				info->sstall += sfu_delay;
			}

			if (instr->flags & IR3_INSTR_SY)
				info->sy++;

			if (is_sfu(instr)) {
				sfu_delay = 10;
			} else if (sfu_delay > 0) {
				sfu_delay--;
			}
		}
	}

	/* Append the immediates after the end of the program.  This lets us emit
	 * the immediates as an indirect load, while avoiding creating another BO.
	 */
	if (v->constant_data_size)
		memcpy(&ptr[info->constant_data_offset / 4], v->constant_data, v->constant_data_size);
	ralloc_free(v->constant_data);
	v->constant_data = NULL;

	return ptr;
}
