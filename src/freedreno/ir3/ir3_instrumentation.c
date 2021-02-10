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
#include "ir3_ra.h"

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

	struct hash_table *jump_target_for_instr;

	struct ir3_instrumentation_funcs iova_funcs;
	struct ir3_instrumentation_iova iova;
};

struct jump_info {
	struct ir3_instruction *instr_jump;
	struct ir3_instruction *instr_target;
};

typedef struct {
	uint32_t buffer_current_pos_iova_lo;
	uint32_t invocation_count;
	uint32_t wave_count;
} __attribute__((packed)) gpu_global_header;

typedef union {
	uint32_t uint_val;
	float float_val;
} gpu_reg_value;

typedef struct {
	uint32_t invocation_serial_n;
	uint32_t instruction_serial_n;

	gpu_reg_value values[];
} __attribute__((packed)) gpu_instruction;

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

enum instr_insert_direction
{
	INSTR_INSERT_PREPEND,
	INSTR_INSERT_APPEND,
};

static struct ir3_instruction *
raw_asm_insert(struct instrumentation_ctx* ctx, struct ir3_shader_variant *v,
	enum instr_insert_direction direction,
	struct ir3_instruction *target, char *asm_str, ...)
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
		fprintf(stderr, "raw_asm_insert: unable to parse \"%s\"\n", buf);
		exit(1);
	}

	struct ir3_instruction *before = direction == INSTR_INSERT_APPEND ?
		list_container_of(target->node.next, target, node) : target;

	struct ir3_instruction *first_instr = NULL;

	/* Remap temporary registers to be right after the ones used by the shader */
	foreach_block (block, &tmp_v->ir->block_list) {
		foreach_instr_safe (instr, &block->instr_list) {
			for (int i = 0; i < instr->regs_count; i++) {
				struct ir3_register *reg = instr->regs[i];

				if (reg->flags & IR3_REG_CONST)
					continue;

				if ((reg_num(reg) > (v->info.max_reg + 1)) && reg->num < FIRST_SHARED_REG) {
					reg->num -= ctx->instrumentation_start_reg << 2;
					reg->num += (v->info.max_reg + 1) << 2;
				}
			}

			if (first_instr == NULL)
				first_instr = instr;

			ir3_instr_move_before(instr, before);
		}
	}

	return first_instr;
}

static void
get_reg_name(char *buf, struct ir3_register *reg)
{
	char *reg_prefix = "";

	if (reg->flags & IR3_REG_HALF)
		reg_prefix = "h";

	if (reg->flags & IR3_REG_RELATIV) {
		if (reg->flags & IR3_REG_CONST)
			sprintf(buf, "%sc<a0.x + %d>", reg_prefix, reg->array.offset);
		else
			sprintf(buf, "%sr<a0.x + %d>", reg_prefix, reg->array.offset);
	} else {
		if (reg->flags & IR3_REG_CONST)
			sprintf(buf, "%sc%u.%c", reg_prefix, reg_num(reg), "xyzw"[reg_comp(reg)]);
		else
			sprintf(buf, "%sr%u.%c", reg_prefix, reg_num(reg), "xyzw"[reg_comp(reg)]);
	}
}

static void
write_single_reg(struct instrumentation_ctx* ctx, struct ir3_shader_variant *v,
				 struct ir3_instruction *instr, struct ir3_register *reg,
				int offset, bool is_dst)
{
	enum instr_insert_direction direction =
		is_dst ? INSTR_INSERT_APPEND : INSTR_INSERT_PREPEND;

	char reg_name[16] = {'\0'};
	get_reg_name(reg_name, reg);

	char *reg_type = (reg->flags & IR3_REG_HALF) ? "u16" : "u32";

	raw_asm_insert(ctx, v,
		direction, instr,
		"(ss)mov.u32u32 r47.y, %d\n"
		"mov.%su32 r47.z, %s\n"
		"(rpt5)nop\n"
		"stg.u32 g[r46.z+r47.y], r47.z, 1\n",
		offset, reg_type, reg_name
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
		if (!reg_gpr(instr->regs[i]) && !(instr->regs[i]->flags & IR3_REG_CONST))
			continue;

		/* Some opcodes consume more than one register from one SRC */
		int sub_regs = 1;

		if (i == 1 &&
			(is_global_a6xx_atomic(instr->opc) ||
			 instr->opc == OPC_LDG)) {
			// Global address is 64b
			sub_regs = 2;
		}

		if (i == 2 && (instr->opc == OPC_STL ||
						instr->opc == OPC_STP ||
						instr->opc == OPC_STLW)) {
			sub_regs = instr->regs[3]->uim_val;
		}

		for (int s = 0; s < sub_regs; s++) {
			struct ir3_register reg = *instr->regs[i];
			reg.num += s;
			util_dynarray_append(&src_regs, struct ir3_register, reg);
		}
	}

	if (has_dest && reg_gpr(instr->regs[0])) {
		int sub_regs = 1;

		if (instr->opc == OPC_LDG ||
			instr->opc == OPC_LDL ||
			instr->opc == OPC_LDP ||
			instr->opc == OPC_LDL) {
			sub_regs = instr->regs[3]->uim_val;
		}

		if (instr->opc == OPC_LDLV) {
			sub_regs = instr->regs[2]->uim_val;
		}

		for (int s = 0; s < sub_regs; s++) {
			struct ir3_register reg = *instr->regs[0];
			util_dynarray_append(&dst_regs, struct ir3_register, reg);
		}
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
	struct ir3_instruction *first_instr = raw_asm_insert(ctx, v,
		INSTR_INSERT_PREPEND, instr,
		"(ss)nop\n"
		"mov.u32u32 r46.z, %d\n"
		"(rpt5)nop\n"
		"atomic.g.add.untyped.1d.u32.1.g r46.z, r46.x, r46.z\n",
		(src_count + dst_count + 2) * 4
	);

	first_instr->flags |= instr->flags & IR3_INSTR_JP;

	_mesa_hash_table_insert(ctx->jump_target_for_instr, instr, first_instr);

	// Write header
	raw_asm_insert(ctx, v,
		INSTR_INSERT_PREPEND, instr,
		"mov.u32u32 r47.y, %d\n"
		"(rpt5)nop\n"
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

	if (is_tex(instr) || is_mem(instr)) {
		raw_asm_insert(ctx, v,
			INSTR_INSERT_APPEND, instr,
			"(sy)nop\n",
			ctx->current_instr_n
		);
	} else {
		raw_asm_insert(ctx, v,
			INSTR_INSERT_APPEND, instr,
			"(rpt5)nop\n",
			ctx->current_instr_n
		);
	}

	util_dynarray_foreach(&src_regs, struct ir3_register, reg) {
		write_single_reg(ctx, v, instr, reg, 2 + idx, false);
		idx++;
	}

	/* Avoid WAR hazard */
	instr->flags |= IR3_INSTR_SS;

	ctx->current_instr_n++;
}

/* Save instructions with jump and their jump targets in order to
 * restore proper jump target later on, since adding instrumentation
 * messes with them.
 */
static struct util_dynarray
save_instrs_jump_target(struct instrumentation_ctx *ctx, struct ir3 *ir) {
	struct util_dynarray jump_targets;
	util_dynarray_init(&jump_targets, ctx);

	struct ir3_instruction **linear_instrs =
		ralloc_array(ctx, struct ir3_instruction *, ctx->instr_meta_size);

	int total_instrs = 0;
	foreach_block (block, &ir->block_list) {
		foreach_instr_safe (instr, &block->instr_list) {
			linear_instrs[total_instrs] = instr;
			total_instrs++;
		}
	}

	for (int i = 0; i < total_instrs; i++) {
		struct ir3_instruction *instr = linear_instrs[i];
		if (is_flow(instr) && instr->cat0.immed != 0) {
			struct jump_info jump;
			jump.instr_jump = instr;
			jump.instr_target = linear_instrs[i + instr->cat0.immed];

			util_dynarray_append(&jump_targets, struct jump_info, jump);
		}
	}

	ralloc_free(linear_instrs);

	return jump_targets;
}

static void
restore_jump_targets(struct instrumentation_ctx *ctx, struct util_dynarray *instrs_jump_info) {
	util_dynarray_foreach(instrs_jump_info, struct jump_info, jump) {
		struct hash_entry *entry =
			_mesa_hash_table_search(ctx->jump_target_for_instr, jump->instr_target);
		struct ir3_instruction *new_target =
			entry ? (struct ir3_instruction *) entry->data : jump->instr_target;

		jump->instr_jump->cat0.immed = (int)new_target->ip - (int)jump->instr_jump->ip;
	}

	util_dynarray_fini(instrs_jump_info);
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

	ctx->instrumentation_start_reg = (NUM_REGS >> 2) - 2;

	ctx->current_instr_n = 0;
	ctx->instr_meta_size = v->info.instrs_count;
	ctx->instr_meta = rzalloc_array(ctx, struct instruction_meta, ctx->instr_meta_size);
	ctx->jump_target_for_instr = _mesa_pointer_hash_table_create(ctx);

	ctx->iova_funcs = v->shader->iova_func;
	ctx->iova = ctx->iova_funcs.create_iova(ctx->iova_funcs.ctx, 128 * 1024 * 1024);
	uint64_t iova = ctx->iova.iova;

	assert(iova != 0 && ctx->iova.map != NULL);

	gpu_global_header* global_header = ctx->iova.map;
	global_header->buffer_current_pos_iova_lo =
		(iova & 0xFFFFFFFF) + sizeof(gpu_global_header);
	global_header->invocation_count = 0;
	global_header->wave_count = 0;

	struct util_dynarray instrs_jump_info = save_instrs_jump_target(ctx, v->ir);

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
		raw_asm_insert(ctx, v,
			INSTR_INSERT_PREPEND, first_instr,
			"(ss)(sy)mov.u32u32 r46.y, %#08x\n"
			"mov.u32u32 r46.x, %#08x\n"
			"mov.u32u32 r46.w, r48.x\n"  // Save value of shared reg
			"(rpt3)nop\n"
			"getone #3\n"
			"atomic.g.inc.untyped.1d.u32.1.g r46.z, r46.x, r46.x\n"
			"(sy)mov.u32u32 r48.x, r46.z\n"
			"(jp)(rpt3)nop\n"
			"cmps.s.ne p0.x, r48.x, %d\n"
			"getone #2\n"
			"mov.u32u32 r48.x, r46.w\n"  // Restore value of shared reg
			"(jp)(rpt2)nop\n",
			iova_high, iova_low + offsetof(gpu_global_header, wave_count),
			wave_to_dump
		);

		jump_to_normal_shader = ir3_instr_create(first_block, OPC_B, 1);
		jump_to_normal_shader->cat0.brtype = BRANCH_PLAIN;
		jump_to_normal_shader->regs[0] = reg_create(v->ir, regid(REG_P0, 0), 0);

		ir3_instr_move_before(jump_to_normal_shader, first_instr);
	}

	raw_asm_insert(ctx, v,
		INSTR_INSERT_PREPEND, first_instr,
		"mov.u32u32 r46.y, %#08x\n"
		"mov.u32u32 r46.x, %#08x\n"
		"(rpt5)nop\n"
		"atomic.g.inc.untyped.1d.u32.1.g r47.x, r46.x, r46.x\n"
		"(ss)mov.u32u32 r46.x, %#08x\n"
		"mov.u32u32 r46.w, 0\n"
		"nop\n",
		iova_high, iova_low + offsetof(gpu_global_header, invocation_count),
		iova_low
	);

	unsigned last_instr_ip = ir3_count_instructions(v->ir);

	restore_jump_targets(ctx, &instrs_jump_info);

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

		/* We wrapped the whole shader in if */
		v->branchstack++;
	}

	ir3_validate(v->ir);

	if (instr_regex)
		regfree(instr_regex);

	list_addtail(&ctx->link, &contexts);

	return true;
}

static void
ir3_dump_instrumentation_results(struct instrumentation_ctx *ctx, FILE *out)
{
	uint64_t iova = ctx->iova.iova;

	gpu_global_header* global_header = ctx->iova.map;
	uint32_t data_written =
		(global_header->buffer_current_pos_iova_lo - (iova & 0xFFFFFFFF)) / 4;

	if (global_header->invocation_count == 0) {
		fprintf(out, "[IR3RegDump] No invocations dumped!\n");
		goto end;
	}

	struct u_vector *invocation_to_instrs =
		rzalloc_array(ctx, struct u_vector, global_header->invocation_count);

	for (int i = 0; i < global_header->invocation_count; i++) {
		u_vector_init(&invocation_to_instrs[i], sizeof(uint32_t), 128);
	}

	uint32_t *map = ctx->iova.map + sizeof(gpu_global_header);
	while (map < (((uint32_t *)ctx->iova.map) + data_written)) {
		gpu_instruction *instr = (gpu_instruction *) map;

		if (instr->instruction_serial_n >= ctx->instr_meta_size) {
			fprintf(out, "[IR3RegDump] Instruction id is out of bounds %d >= %d\n",
				instr->instruction_serial_n, ctx->instr_meta_size);
			break;
		}

		struct instruction_meta *instr_meta =
			&ctx->instr_meta[instr->instruction_serial_n];

		if (instr->invocation_serial_n >= global_header->invocation_count) {
			fprintf(out, "[IR3RegDump] Invocation id is out of bounds %d >= %d\n",
				instr->invocation_serial_n, global_header->invocation_count);
			break;
		}

		uint32_t *elem = u_vector_add(&invocation_to_instrs[instr->invocation_serial_n]);
		*elem = map - (uint32_t *)ctx->iova.map;

		map += sizeof(gpu_instruction) / sizeof(uint32_t) + instr_meta->regs_count;
	}

	for (uint32_t i = 0; i < global_header->invocation_count; i++) {
		fprintf(out, "\n[IR3RegDump] Shader invocation #%u\n", i);

		uint32_t *offset;
		u_vector_foreach(offset, &invocation_to_instrs[i]) {
			uint32_t *instr_map = ((uint32_t *)ctx->iova.map) + *offset;
			gpu_instruction *instr = (gpu_instruction *) instr_map;
			instr_map += sizeof(gpu_instruction) / sizeof(uint32_t);

			struct instruction_meta *instr_meta = &ctx->instr_meta[instr->instruction_serial_n];

			fprintf(out, "[%d/%d]: %s", i, instr->instruction_serial_n, instr_meta->disasm);
			fprintf(out, "\t");

			for (int j = 0; j < instr_meta->regs_count; j++) {
				struct reg_meta *reg = &instr_meta->regs_meta[j];

				gpu_reg_value val = instr->values[reg->data_offset / 4];

				char reg_name[16];
				get_reg_name(reg_name, &reg->reg);

				fprintf(out,
					"%s(%s)=%#08x /* %f */  ",
					reg->is_dst ? "dst" : "src",
					reg_name,
					val.uint_val, val.float_val);
			}

			fprintf(out, "\n");
		}
	}

	fprintf(out, "[IR3RegDump] Data Written %d\n", data_written);
	fprintf(out, "[IR3RegDump] Total Waves %d\n", global_header->wave_count);
	fprintf(out, "[IR3RegDump] Invocations Written %d\n", global_header->invocation_count);


end:
	ctx->iova_funcs.destroy_iova(ctx->iova_funcs.ctx, &ctx->iova);

	list_del(&ctx->link);

	ralloc_free(ctx);
}

void
ir3_dump_all_instrumentation_results(FILE *out)
{
	list_for_each_entry_safe(struct instrumentation_ctx, ctx, &contexts, link) {
		ir3_dump_instrumentation_results(ctx, out);
	}
}