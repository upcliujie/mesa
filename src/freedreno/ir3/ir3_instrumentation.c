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
#include "util/u_vector.h"

#include "ir3_shader.h"
#include "ir3_compiler.h"
#include "ir3_nir.h"
#include "ir3_assembler.h"
#include "ir3_parser.h"

#include "isa/isa.h"

#include <regex.h>

struct reg_meta {
	struct ir3_register reg;
	uint32_t data_offset;
	bool is_dst;
};

struct instruction_meta {
	char *disasm;
	uint32_t regs_count;
	struct reg_meta *regs_meta;
};

struct instrumentation_ctx {
	struct list_head link;

	uint32_t instrumentation_start_reg;

	uint32_t current_instr_n;

	uint32_t instr_meta_size;
	struct instruction_meta *instr_meta;

	struct iova_func_table iova_func;
	void *opaque_iova;
	void *iova_map;
};

struct cf_retarget_info {
	struct ir3_instruction *instr_cf;
	struct ir3_instruction *instr_before_target;
};

static struct list_head contexts = {
   .next = &contexts,
   .prev = &contexts,
};

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
	fclose(stream);

	instr_disasm = ralloc_size(ctx, stream_size + 1);
	memcpy(instr_disasm, stream_data, stream_size);
	instr_disasm[stream_size] = 0;

	free(stream_data);

	return instr_disasm;
}

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
raw_asm_prepend(struct instrumentation_ctx* ctx, struct ir3_shader_variant *v,
	struct ir3_instruction *before, char *asm_str, ...)
{
	static char buf[4096] = {'\0'};
	va_list va;
	va_start(va, asm_str);
	vsnprintf(buf, sizeof(buf), asm_str, va);
	va_end(va);

	struct ir3_kernel_info info = {};
	info.numwg = INVALID_REG;

	struct ir3_shader_variant *tmp_v = rzalloc_size(ctx, sizeof(*tmp_v));
	tmp_v->type = MESA_SHADER_COMPUTE;
	tmp_v->shader = v->shader;

	FILE *in = fmemopen((void *)buf, strlen(buf), "r");
	tmp_v->ir = ir3_parse(tmp_v, &info, in);
	fclose(in);

	if (!tmp_v->ir) {
		fprintf(stderr, "raw_asm_prepend: unable to parse \"%s\"", asm_str);
		exit(1);
	}

	foreach_block (block, &tmp_v->ir->block_list) {
		foreach_instr_safe (instr, &block->instr_list) {
			for (int i = 0; i < instr->regs_count; i++) {
				struct ir3_register *reg = instr->regs[i];

				if ((reg_num(reg) > (v->info.max_reg + 1)) && reg_num(reg) < 48) {
					reg->num -= ctx->instrumentation_start_reg << 2;
					reg->num += (v->info.max_reg + 1) << 2;
				}
			}

			ir3_instr_move_before(instr, before);
		}
	}
}

static void
write_single_reg(struct instrumentation_ctx* ctx, struct ir3_shader_variant *v,
				 struct ir3_instruction *instr, struct ir3_register *reg,
				int offset, bool is_dst)
{
	struct ir3_instruction *before =
		is_dst ? list_container_of(instr->node.next, instr, node) : instr;
	raw_asm_prepend(ctx, v, before,
		"(ss)mov.u32u32 r47.y, %d"
		"(rpt3)nop"
		"(sy)stg.%s g[r46.z+r47.y], %s%u.%c, 1",
		offset * ((reg->flags & IR3_REG_HALF) ? 2 : 1),
		(reg->flags & IR3_REG_HALF) ? "u16" : "u32",
		(reg->flags & IR3_REG_HALF) ? "hr" : "r",
		reg_num(reg), "xyzw"[reg_comp(reg)]
	);
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
	util_dynarray_init(&src_regs, ctx);

	struct util_dynarray dst_regs;
	util_dynarray_init(&dst_regs, ctx);

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
	raw_asm_prepend(ctx, v, instr,
		"(ss)nop\n"
		"mov.u32u32 r46.z, %d\n"
		"(rpt3)nop\n"
		"atomic.g.add.untyped.1d.u32.1.g r46.z, r46.x, r46.z\n",
		(src_count + dst_count + 2) * 4
	);

	// Write header
	raw_asm_prepend(ctx, v, instr,
		"mov.u32u32 r47.y, %d\n"
		"(rpt3)nop\n"
		"(sy)stg.u32 g[r46.z], r47.x, 2\n",
		ctx->current_instr_n
	);

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

	const unsigned wave_to_dump = env_var_as_unsigned("IR3_SHADER_INSTRUMENT_WAVE", -1);

	int8_t instrumentation_reg = v->info.max_reg + 1;

	struct instrumentation_ctx *ctx = ralloc(NULL, struct instrumentation_ctx);

	/* Clone the shader in order to conditionally jump to it for invocations
	 * we are not interested in.
	 */
	struct list_head cloned_shader;
	list_inithead(&cloned_shader);
	{
		struct ir3_block *last_block = list_last_entry(&v->ir->block_list, struct ir3_block, node);
		struct ir3_instruction *last_instr = list_last_entry(&last_block->instr_list, struct ir3_instruction, node);

		foreach_block (block, &v->ir->block_list) {
			foreach_instr_safe (instr, &block->instr_list) {
				struct ir3_instruction *cloned = ir3_instr_clone(instr);
				list_delinit(&cloned->node);
				list_addtail(&cloned->node, &cloned_shader);
			}
		}
	}

	ctx->instrumentation_start_reg = 46;

	ctx->current_instr_n = 0;
	ctx->instr_meta_size = v->info.instrs_count;
	ctx->instr_meta = rzalloc_array(ctx, struct instruction_meta, ctx->instr_meta_size);

	ctx->iova_func = v->shader->iova_func;
	ctx->opaque_iova = ctx->iova_func.create_iova(ctx->iova_func.data, ctx);
	ctx->iova_map = ctx->iova_func.map(ctx->iova_func.data, ctx->opaque_iova);
	uint64_t iova = ctx->iova_func.get_iova(ctx->opaque_iova);

	((uint32_t*)ctx->iova_map)[0] = (iova & 0xFFFFFFFF) + 4 * 3; // Lower part of the address
	((uint32_t*)ctx->iova_map)[1] = 0; // invocations counts
	((uint32_t*)ctx->iova_map)[2] = 0; // waves counts

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

	foreach_block (block, &v->ir->block_list) {
		foreach_instr_safe (instr, &block->instr_list) {
			dump_instruction_regs(ctx, v, instr_regex, instr);
		}
	}

	struct ir3_block *first_block = list_first_entry(&v->ir->block_list, struct ir3_block, node);
	struct ir3_instruction *first_instr = list_first_entry(&first_block->instr_list, struct ir3_instruction, node);

	struct ir3_block *last_block = list_last_entry(&v->ir->block_list, struct ir3_block, node);
	struct ir3_instruction *last_instr = list_last_entry(&last_block->instr_list, struct ir3_instruction, node);

	struct ir3_instruction *jump_to_normal_shader = NULL;
	unsigned initial_last_instr_ip = last_instr->ip;

	/* Get current invocation number, pre-init address registers */

	uint32_t iova_high = iova >> 32;
	uint32_t iova_low = (iova & 0xFFFFFFFF);

	/* Why do we even need this nops? Otherwise hangs compute shader */
	{
		struct ir3_instruction *nop1 = ir3_instr_create(first_block, OPC_NOP, 0);
		ir3_instr_move_before(nop1, first_instr);
		struct ir3_instruction *nop2 = ir3_instr_create(first_block, OPC_NOP, 0);
		ir3_instr_move_before(nop2, first_instr);
	}

	if (wave_to_dump != -1) {
		/* Increment wave_id once in a wave, jump to uninstrumented shader
		 * if it's not the wave we are interested in.
		 */
		raw_asm_prepend(ctx, v, first_instr,
			"mov.u32u32 r46.y, %#08x\n"
			"mov.u32u32 r46.x, %#08x\n"
			"(rpt3)nop\n"
			"(sy)(ss)getone #3\n"
			"atomic.g.inc.untyped.1d.u32.1.g r46.z, r46.x, r46.x\n"
			"(sy)(ss)mov.u32u32 r48.x, r46.z\n"
			"(sy)(ss)(jp)cmps.s.ne p0.x, r48.x, %d\n"
			"(rpt2)nop\n",
			iova_high, iova_low + 8,
			wave_to_dump
		);

		jump_to_normal_shader = ir3_instr_create(first_block, OPC_B, 1);
		jump_to_normal_shader->cat0.brtype = BRANCH_PLAIN;
		jump_to_normal_shader->regs[0] = reg_create(v->ir, regid(REG_P0, 0), 0);

		ir3_instr_move_before(jump_to_normal_shader, first_instr);
	}

	raw_asm_prepend(ctx, v, first_instr,
		"mov.u32u32 r46.y, %#08x\n"
		"mov.u32u32 r46.x, %#08x\n"
		"(rpt3)nop\n"
		"atomic.g.inc.untyped.1d.u32.1.g r47.x, r46.x, r46.x\n"
		"(ss)nop\n"
		"mov.u32u32 r46.x, %#08x\n"
		"mov.u32u32 r46.w, 0\n",
		iova_high, iova_low + 4, iova_low
	);

	unsigned last_instr_ip = 0;

	/* Retarget all jumps */
	{
		last_instr_ip = ir3_count_instructions(v->ir);

		util_dynarray_foreach(&instrs_retarget_info, struct cf_retarget_info, retarget_info) {
			retarget_info->instr_cf->cat0.immed =
				(int)retarget_info->instr_before_target->ip - (int)retarget_info->instr_cf->ip + 1;
		}
	}

	util_dynarray_fini(&instrs_retarget_info);

	if (wave_to_dump != -1) {
		jump_to_normal_shader->cat0.immed = last_instr_ip - jump_to_normal_shader->ip;

		foreach_instr_rev(instr, &last_block->instr_list) {
			if (instr->opc == OPC_END) {
				struct ir3_instruction *jump_to_end = ir3_instr_create(last_block, OPC_JUMP, 0);
				jump_to_end->cat0.immed = initial_last_instr_ip;

				ir3_instr_move_before(jump_to_end, instr);
				list_delinit(&instr->node);
				break;
			}
		}

		last_instr = list_last_entry(&last_block->instr_list, struct ir3_instruction, node);

		list_first_entry(&cloned_shader, struct ir3_instruction, node)->flags = IR3_INSTR_JP;

		foreach_instr_safe(instr, &cloned_shader) {
			list_addtail(&instr->node, &last_block->instr_list);
		}
	}

	v->info.max_reg = v->info.max_reg + 2;

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
	uint32_t total_waves = map[2];
	uint32_t invocations = map[1];

	printf("Data Written %d\n", data_written);
	printf("Total Waves %d\n", total_waves);
	printf("Invocations Written %d\n", invocations);

	map += 3;

	struct u_vector *invocation_to_instrs =
		rzalloc_array(ctx, struct u_vector, invocations);

	for (int i = 0; i < invocations; i++) {
		u_vector_init(&invocation_to_instrs[i], sizeof(uint32_t), 128);
	}

	while (map < (((uint32_t *)ctx->iova_map) + data_written)) {
		uint32_t invocation_n = map[0];
		uint32_t instruction_n = map[1];

		struct instruction_meta *instr_meta = &ctx->instr_meta[instruction_n];

		uint32_t *elem = u_vector_add(&invocation_to_instrs[invocation_n]);
		*elem = map - (uint32_t *)ctx->iova_map;

		map += 2 + instr_meta->regs_count;
	}

	for (int i = 0; i < invocations; i++) {
		printf("\nShader invocation #%d\n", i);

		uint32_t *offset;
		u_vector_foreach(offset, &invocation_to_instrs[i]) {
			uint32_t *instr_map = ((uint32_t *)ctx->iova_map) + *offset;
			uint32_t instruction_n = instr_map[1];
			instr_map += 2;

			struct instruction_meta *instr_meta = &ctx->instr_meta[instruction_n];

			printf("[%d/%d]: %s", i, instruction_n, instr_meta->disasm);
			printf("\t");

			for (int j = 0; j < instr_meta->regs_count; j++)
			{
				struct reg_meta *reg = &instr_meta->regs_meta[j];

				const uint32_t int_val = instr_map[reg->data_offset / 4];
				const float float_val = ((float*) instr_map)[reg->data_offset / 4];

				printf("%s(%s%u.%c)=%#08x /* %f */  ",
					reg->is_dst ? "dst" : "src",
					(reg->reg.flags & IR3_REG_HALF) ? "hr" : "r",
					reg_num(&reg->reg), "xyzw"[reg_comp(&reg->reg)],
					int_val, float_val);
			}

			printf("\n");
		}
	}

	printf("Data Written %d\n", data_written);
	printf("Total Waves %d\n", total_waves);
	printf("Invocations Written %d\n", invocations);

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