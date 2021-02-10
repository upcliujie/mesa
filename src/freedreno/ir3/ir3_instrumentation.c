/*
 * Copyright © 2021 Igalia S.L.
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

#include "util/u_memory.h"
#include "util/u_dynarray.h"

#include "ir3_shader.h"
#include "ir3_compiler.h"
#include "ir3_nir.h"

#include "isa/isa.h"

#include <regex.h>

struct reg_meta
{
	struct ir3_register reg;
	uint32_t data_offset;
	bool is_dst;
};

struct instruction_meta
{
	char *disasm;
	struct reg_meta *regs_meta;
	uint32_t regs_count;
};

struct instrumentation_ctx
{
	struct list_head link;

	/* */
	struct ir3_register *inv_counter_high;
	struct ir3_register *inv_acounter_low;
	struct ir3_register *buffer_high;
	struct ir3_register *buffer_low;

	struct ir3_register *header[2];

	uint32_t current_instr_n;

	struct instruction_meta *instr_meta;
	uint32_t instr_meta_size;

	struct iova_func_table iova_func;
	void *opaque_iova;
	void *iova_map;
};

struct cf_retarget_info
{
	struct ir3_instruction *instr_cf;
	struct ir3_instruction *instr_before_target;
};

static struct list_head contexts = {
   .next = &contexts,
   .prev = &contexts,
};

static struct ir3_register *
reg_create(struct ir3 *shader, int num, int flags)
{
	struct ir3_register *reg =
			ir3_alloc(shader, sizeof(struct ir3_register));
	reg->wrmask = 1;
	reg->flags = flags;
	reg->num = num;
	return reg;
}

static void
write_single_reg(struct instrumentation_ctx* ctx, struct ir3_shader_variant *v,
				 struct ir3_instruction *instr, struct ir3_register *reg, int offset, bool is_dst)
{
	struct ir3_register *const_stg_offset = reg_create(v->ir, 0, IR3_REG_IMMED);
	/* The incoming offset is in 32b while instruction's offset is in size of cat6.type */
	const_stg_offset->iim_val = offset * ((reg->flags & IR3_REG_HALF) ? 2 : 1);

	struct ir3_register *const_stg_comp = reg_create(v->ir, 0, IR3_REG_IMMED);
	const_stg_comp->iim_val = 1;

	struct ir3_register *stg_offset = ctx->header[1];

	struct ir3_instruction *mov_offset = ir3_instr_create(instr->block, OPC_MOV, 2);
	mov_offset->cat1.src_type = TYPE_U32;
	mov_offset->cat1.dst_type = TYPE_U32;
	mov_offset->regs[0] = stg_offset;
	mov_offset->regs[1] = const_stg_offset;
	mov_offset->flags = IR3_INSTR_SS;

	if (is_dst) {
		ir3_instr_move_after(mov_offset, instr);
	} else {
		ir3_instr_move_before(mov_offset, instr);
	}

	struct ir3_instruction *nop = ir3_instr_create(instr->block, OPC_NOP, 0);
	nop->repeat = 3;
	ir3_instr_move_after(nop, mov_offset);

	struct ir3_instruction *dump_reg = ir3_instr_create(instr->block, OPC_STG, 5);
	dump_reg->cat6.type = (reg->flags & IR3_REG_HALF) ? TYPE_U16 : TYPE_U32;
	dump_reg->flags = IR3_INSTR_G | IR3_INSTR_SY;
	dump_reg->regs[0] = reg_create(v->ir, 0, IR3_REG_IMMED);
	dump_reg->regs[1] = ctx->buffer_low;
	dump_reg->regs[2] = reg;
	dump_reg->regs[3] = const_stg_comp;
	dump_reg->regs[4] = stg_offset;

	ir3_instr_move_after(dump_reg, nop);
}

static char *
disasm_instr(struct instrumentation_ctx* ctx, struct ir3_compiler *compiler,
				struct ir3_instruction *instr)
{
	char *instr_disasm = NULL;
	char *stream_data = NULL;
	size_t stream_size = 0;
	FILE *stream = open_memstream(&stream_data, &stream_size);

	uint64_t bin = isa_assemble_instruction(compiler, instr);

	isa_decode(&bin, 8, stream, &(struct isa_decode_options){
		.gpu_id = compiler->gpu_id,
		.show_errors = true,
	});
	fflush(stream);

	instr_disasm = ralloc_size(ctx, stream_size + 1);
	memcpy(instr_disasm, stream_data, stream_size);
	instr_disasm[stream_size] = 0;

	free(stream_data);

	return instr_disasm;
}

static void
dump_instruction_regs(struct instrumentation_ctx* ctx, struct ir3_shader_variant *v,
						regex_t *instr_filter, struct ir3_instruction *instr)
{
	if (instr->regs_count == 0)
		return;

	char *instr_disasm = disasm_instr(ctx, v->ir->compiler, instr);

	if (instr_filter) {
		if (regexec(instr_filter, instr_disasm, 0, NULL, 0) == REG_NOMATCH) {
			return;
		}
	}

	bool has_dest = dest_regs(instr);

	struct util_dynarray src_regs;
	util_dynarray_init(&src_regs, NULL);

	struct util_dynarray dst_regs;
	util_dynarray_init(&dst_regs, NULL);

	for (int i = (has_dest ? 1 : 0); i < instr->regs_count; i++)
	{
		if (!reg_gpr(instr->regs[i]))
			continue;

		/* Some opcodes consume more than one register from one SRC */
		int sub_regs = 1;

		if (i == 1 &&
			(is_global_a6xx_atomic(instr->opc) ||
			 instr->opc == OPC_LDG)) {
			sub_regs = 2;
		}

		for (int s = 0; s < sub_regs; s++)
		{
			struct ir3_register reg = *instr->regs[i];
			reg.num += s;
			util_dynarray_append(&src_regs, struct ir3_register, reg);
		}
	}

	if (has_dest && reg_gpr(instr->regs[0])) {
		struct ir3_register reg = *instr->regs[0];
		// reg.num += s;
		util_dynarray_append(&dst_regs, struct ir3_register, reg);
	}

	uint32_t src_count = util_dynarray_num_elements(&src_regs, struct ir3_register);
	uint32_t dst_count = util_dynarray_num_elements(&dst_regs, struct ir3_register);

	// Write meta information about the instruction
	{
		struct instruction_meta* meta = &ctx->instr_meta[ctx->current_instr_n];

		meta->disasm = instr_disasm;

		meta->regs_count = src_count + dst_count;
		meta->regs_meta = rzalloc_array(NULL, struct reg_meta, meta->regs_count);

		int idx = 0;
		util_dynarray_foreach(&dst_regs, struct ir3_register, reg) {
			meta->regs_meta[idx].reg = *reg;
			meta->regs_meta[idx].data_offset = 4 * idx;
			meta->regs_meta[idx].is_dst = true;
			idx++;
		}

		util_dynarray_foreach(&src_regs, struct ir3_register, reg) {
			meta->regs_meta[idx].reg = *reg;
			meta->regs_meta[idx].data_offset = 4 * idx;
			meta->regs_meta[idx].is_dst = false;
			idx++;
		}
	}

	// Allocate enough space in global buffer for header + all registers to dump
	{
		// We reuse some registers so we want to wait for previous register dump to consume sources
		struct ir3_instruction *nop_wait_for_src = ir3_instr_create(instr->block, OPC_NOP, 0);
		nop_wait_for_src->flags = IR3_INSTR_SS;
		ir3_instr_move_before(nop_wait_for_src, instr);

		struct ir3_register *const_add = reg_create(v->ir, 0, IR3_REG_IMMED);
		const_add->iim_val = (src_count + dst_count + 2) * 4;

		struct ir3_instruction *mov = ir3_instr_create(instr->block, OPC_MOV, 2);
		mov->cat1.src_type = TYPE_U32;
		mov->cat1.dst_type = TYPE_U32;
		mov->regs[0] = ctx->buffer_low;
		mov->regs[1] = const_add;
		ir3_instr_move_before(mov, instr);

		struct ir3_instruction *nop_wait_for_movs = ir3_instr_create(instr->block, OPC_NOP, 0);
		nop_wait_for_movs->repeat = 3;
		ir3_instr_move_before(nop_wait_for_movs, instr);

		struct ir3_instruction *get_mem_addr = ir3_instr_create(instr->block, OPC_ATOMIC_G_ADD, 3);
		get_mem_addr->cat6.iim_val = 1;
		get_mem_addr->cat6.d = 1;
		get_mem_addr->cat6.type = TYPE_U32;

		get_mem_addr->regs_count = 3;
		get_mem_addr->regs[0] = ctx->buffer_low;
		get_mem_addr->regs[1] = ctx->inv_acounter_low;
		get_mem_addr->regs[2] = ctx->buffer_low;

		ir3_instr_move_before(get_mem_addr, instr);
	}

	// Write header
	{
		struct ir3_register *const_instr_n = reg_create(v->ir, 0, IR3_REG_IMMED);
		const_instr_n->iim_val = ctx->current_instr_n;

		struct ir3_instruction *mov_header_instruction = ir3_instr_create(instr->block, OPC_MOV, 2);
		mov_header_instruction->cat1.src_type = TYPE_U32;
		mov_header_instruction->cat1.dst_type = TYPE_U32;
		mov_header_instruction->regs[0] = ctx->header[1];
		mov_header_instruction->regs[1] = const_instr_n;
		ir3_instr_move_before(mov_header_instruction, instr);

		struct ir3_instruction *nop = ir3_instr_create(instr->block, OPC_NOP, 0);
		nop->repeat = 3;
		ir3_instr_move_before(nop, instr);

		struct ir3_register *const_stg_offset = reg_create(v->ir, 0, IR3_REG_IMMED);
		const_stg_offset->iim_val = 0;

		struct ir3_register *const_stg_comp = reg_create(v->ir, 0, IR3_REG_IMMED);
		const_stg_comp->iim_val = 2;

		struct ir3_instruction *write_value = ir3_instr_create(instr->block, OPC_STG, 5);
		write_value->cat6.type = TYPE_U32;
		write_value->flags = IR3_INSTR_G | IR3_INSTR_SY;

		write_value->regs[0] = reg_create(v->ir, 0, IR3_REG_IMMED);
		write_value->regs[1] = ctx->buffer_low;
		write_value->regs[2] = ctx->header[0];
		write_value->regs[3] = const_stg_comp;
		write_value->regs[4] = const_stg_offset;

		ir3_instr_move_before(write_value, instr);
	}

	/* We could write 4 regs at once but it would consume 4 more registers
	 * to place them continuously.
	 */

	int idx = 0;
	util_dynarray_foreach(&dst_regs, struct ir3_register, reg) {
		write_single_reg(ctx, v, instr, reg, 2 + idx, true);
		idx++;
	}

	util_dynarray_foreach(&src_regs, struct ir3_register, reg) {
		write_single_reg(ctx, v, instr, reg, 2 + idx, false);
		idx++;
	}

	ctx->current_instr_n++;
}

bool
ir3_instrument_shader(struct ir3_shader_variant *v)
{
	regex_t *instr_regex = NULL;
	char *instr_regex_str = getenv("IR3_SHADER_INSTRUMENT_INSTR_REGEX");

	if (instr_regex_str) {
	  instr_regex = ralloc(NULL, regex_t);
	  if (!regcomp(instr_regex, instr_regex_str, REG_EXTENDED|REG_NOSUB) == 0) {
		fprintf(stderr, "Unable to compile regexp: %s", instr_regex_str);
		ralloc_free(instr_regex);
		return false;
	  }
	}

	if ((v->info.max_reg + 2) >= REG_A0) {
		if (instr_regex)
			ralloc_free(instr_regex);

		fprintf(stderr, "Unable to instrument shader, not enough registers available!");
		return false;
	}

	int8_t instrumentation_reg = v->info.max_reg + 1;

	struct instrumentation_ctx *ctx = ralloc(NULL, struct instrumentation_ctx);

	ctx->current_instr_n = 0;
	ctx->instr_meta_size = v->info.instrs_count;
	ctx->instr_meta = rzalloc_array(ctx, struct instruction_meta, ctx->instr_meta_size);

	ctx->inv_counter_high = reg_create(v->ir, regid(instrumentation_reg, 1), 0);
	ctx->inv_acounter_low = reg_create(v->ir, regid(instrumentation_reg, 0), 0);

	ctx->buffer_high = reg_create(v->ir, regid(instrumentation_reg, 3), 0);
	ctx->buffer_low = reg_create(v->ir, regid(instrumentation_reg, 2), 0);

	ctx->header[0] = reg_create(v->ir, regid(instrumentation_reg + 1, 0), 0);
	ctx->header[1] = reg_create(v->ir, regid(instrumentation_reg + 1, 1), 0);

	ctx->iova_func = v->shader->iova_func;
	ctx->opaque_iova = ctx->iova_func.create_iova(ctx->iova_func.data, ctx);
	ctx->iova_map = ctx->iova_func.map(ctx->iova_func.data, ctx->opaque_iova);
	uint64_t iova = ctx->iova_func.get_iova(ctx->opaque_iova);

	((uint32_t*)ctx->iova_map)[0] = (iova & 0xFFFFFFFF) + 8; // Lower part of the address
	((uint32_t*)ctx->iova_map)[1] = 0; // invocations counts

	v->info.max_reg = v->info.max_reg + 2;

	/* Save jump instruction and the instruction before its target.
	 * This will make possible to retarget the jump to the start of
	 * dumping of the regsiters used by jump target.
	 */

	struct util_dynarray instrs_retarget_info;
	util_dynarray_init(&instrs_retarget_info, ctx);
	{
		/* To correctly reassign jump target we need an indexable array */
		struct ir3_instruction **linear_instrs =
			ralloc_array(ctx, struct ir3_instruction *, ctx->instr_meta_size);

		int total_instrs = 0;
		foreach_block (block, &v->ir->block_list) {
			foreach_instr_safe (instr, &block->instr_list) {
				linear_instrs[total_instrs] = instr;
				total_instrs++;
			}
		}

		for (int i = 0; i < total_instrs; i++) {
			struct ir3_instruction *instr = linear_instrs[i];
			if (is_flow(instr) && instr->cat0.immed != 0) {
				struct cf_retarget_info retarget_info;
				retarget_info.instr_cf = instr;
				retarget_info.instr_before_target =
					linear_instrs[i + instr->cat0.immed - 1];

				util_dynarray_append(&instrs_retarget_info, struct cf_retarget_info, retarget_info);
			}
		}

		ralloc_free(linear_instrs);
	}

	struct ir3_register *const_iova_high = reg_create(v->ir, 0, IR3_REG_IMMED);
	const_iova_high->iim_val = iova >> 32;
	struct ir3_register *const_iova_low = reg_create(v->ir, 0, IR3_REG_IMMED);
	const_iova_low->iim_val = iova & 0xFFFFFFFF;
	struct ir3_register *const_iova_invocations_low = reg_create(v->ir, 0, IR3_REG_IMMED);
	const_iova_invocations_low->iim_val = (iova & 0xFFFFFFFF) + 4;

	foreach_block (block, &v->ir->block_list) {
		foreach_instr_safe (instr, &block->instr_list) {
			dump_instruction_regs(ctx, v, instr_regex, instr);
		}
	}

	struct ir3_block *first_block = list_first_entry(&v->ir->block_list, struct ir3_block, node);
	struct ir3_instruction *first_instr = list_first_entry(&first_block->instr_list, struct ir3_instruction, node);

	/* Get current invocation number, pre-init address registers */
	{
		/* Why do we even need this nops? Otherwise hangs compute shader */
		{
			struct ir3_instruction *nop1 = ir3_instr_create(first_block, OPC_NOP, 0);
			ir3_instr_move_before(nop1, first_instr);
			struct ir3_instruction *nop2 = ir3_instr_create(first_block, OPC_NOP, 0);
			ir3_instr_move_before(nop2, first_instr);
		}

		{
			struct ir3_instruction *mov = ir3_instr_create(first_block, OPC_MOV, 2);
			mov->cat1.src_type = TYPE_U32;
			mov->cat1.dst_type = TYPE_U32;
			mov->regs[0] = ctx->inv_counter_high;
			mov->regs[1] = const_iova_high;
			ir3_instr_move_before(mov, first_instr);
		}

		{
			struct ir3_instruction *mov = ir3_instr_create(first_block, OPC_MOV, 2);
			mov->cat1.src_type = TYPE_U32;
			mov->cat1.dst_type = TYPE_U32;
			mov->regs[0] = ctx->inv_acounter_low;
			mov->regs[1] = const_iova_invocations_low;
			ir3_instr_move_before(mov, first_instr);
		}

		struct ir3_instruction *nop = ir3_instr_create(first_block, OPC_NOP, 0);
		nop->repeat = 3;
		ir3_instr_move_before(nop, first_instr);

		struct ir3_instruction *inc_invocations = ir3_instr_create(first_block, OPC_ATOMIC_G_INC, 3);
		inc_invocations->cat6.iim_val = 1;
		inc_invocations->cat6.d = 1;
		inc_invocations->cat6.type = TYPE_U32;
		inc_invocations->regs_count = 3;
		inc_invocations->regs[0] = ctx->header[0];
		inc_invocations->regs[1] = ctx->inv_acounter_low;
		inc_invocations->regs[2] = ctx->inv_acounter_low;

		ir3_instr_move_before(inc_invocations, first_instr);

		nop = ir3_instr_create(first_block, OPC_NOP, 0);
		nop->flags = IR3_INSTR_SS;
		ir3_instr_move_before(nop, first_instr);

		{
			struct ir3_instruction *mov = ir3_instr_create(first_block, OPC_MOV, 2);
			mov->cat1.src_type = TYPE_U32;
			mov->cat1.dst_type = TYPE_U32;
			mov->regs[0] = ctx->inv_acounter_low;
			mov->regs[1] = const_iova_low;
			ir3_instr_move_before(mov, first_instr);
		}

		{
			struct ir3_instruction *mov = ir3_instr_create(first_block, OPC_MOV, 2);
			mov->cat1.src_type = TYPE_U32;
			mov->cat1.dst_type = TYPE_U32;
			mov->regs[0] = ctx->buffer_high;
			mov->regs[1] = const_iova_high;
			ir3_instr_move_before(mov, first_instr);
		}
	}

	/* Retarget all jumps */
	{
		ir3_count_instructions(v->ir);

		util_dynarray_foreach(&instrs_retarget_info, struct cf_retarget_info, retarget_info) {
			retarget_info->instr_cf->cat0.immed =
				(int)retarget_info->instr_before_target->ip - (int)retarget_info->instr_cf->ip + 1;
		}
	}

	util_dynarray_fini(&instrs_retarget_info);

	ir3_validate(v->ir);

	if (instr_regex)
		regfree(instr_regex);

	list_addtail(&ctx->link, &contexts);

	return true;
}

static void
ir3_dump_instrumentation_results(struct instrumentation_ctx *ctx)
{
	uint64_t iova = ctx->iova_func.get_iova(ctx->opaque_iova);

	uint32_t *map = (uint32_t *)ctx->iova_map;
	uint32_t data_written = (map[0] - (iova & 0xFFFFFFFF)) / 4;

	fprintf(stderr, "Data written %d\n", data_written);
	fprintf(stderr, "Invocations %d\n", map[1]);

	map += 2;

	while (map < (((uint32_t *)ctx->iova_map) + data_written))
	{
		uint32_t invocation_n = map[0];
		uint32_t instruction_n = map[1];
		map += 2;

		struct instruction_meta *instr_meta = &ctx->instr_meta[instruction_n];

		bool print = invocation_n == 0;

		if (print) {
			printf("[%d/%d]: %s", invocation_n, instruction_n, instr_meta->disasm);
			printf("\t");
		}

		for (int j = 0; j < instr_meta->regs_count; j++)
		{
			struct reg_meta *reg = &instr_meta->regs_meta[j];

			const uint32_t int_val = map[reg->data_offset / 4];
			const float float_val = ((float*) map)[reg->data_offset / 4];

			if (print)
				printf("%s(%s%u.%c)=%#08x /* %f */  ",
					reg->is_dst ? "dst" : "src",
					(reg->reg.flags & IR3_REG_HALF) ? "hr" : "r",
					reg_num(&reg->reg), "xyzw"[reg_comp(&reg->reg)],
					int_val, float_val);
		}

		if (print)
			printf("\n");

		map += instr_meta->regs_count;
	}

	ctx->iova_func.destroy_iova(ctx->iova_func.data, ctx->opaque_iova, ctx);

	list_del(&ctx->link);

	ralloc_free(ctx);
}

void
ir3_dump_all_instrumentation_results()
{
	list_for_each_entry_safe(struct instrumentation_ctx, ctx, &contexts, link) {
		ir3_dump_instrumentation_results(ctx);
	}
}