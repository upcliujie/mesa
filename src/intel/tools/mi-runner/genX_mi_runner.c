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

#include "gen_mi_runner.h"

#define __gen_address_type uint64_t

#include "genxml/gen_macros.h"
#include "genxml/genX_unpack.h"
#include "common/gen_gem.h"
#include "util/os_time.h"

#define GPR_OFFSET 0x2600
#define GPR_REG(i) (GPR_OFFSET + (i) * 8)

#define U64_1 (0xffffffffffffffff)

#define PREDICATE_SRC0 0x2400
#define PREDICATE_SRC1 0x2408
#define PREDICATE_RESULT 0x2418

#define INST(name) _inst_##name

static uint32_t null_reg;
static uint32_t unknown_reg = 0xdeaddead;

static uint32_t *reg_ptr(struct gen_mi_context *ctx, uint32_t offset, bool write)
{
   if (offset >= GPR_REG(0) &&
       offset < GPR_REG(16)) {
      return &ctx->gpr32[(offset - GPR_OFFSET) / 4];
   } else {
      bool off = (offset & 0x7ull) != 0;
      switch (offset & ~0x7ull) {
      case PREDICATE_SRC0:
         return ((uint32_t *) &ctx->predicate.src0) + (off ? 1 : 0);
      case PREDICATE_SRC1:
         return ((uint32_t *) &ctx->predicate.src1) + (off ? 1 : 0);
      case PREDICATE_RESULT:
         return ((uint32_t *) &ctx->predicate.result) + (off ? 1 : 0);
      default:
         return write ? &null_reg : &unknown_reg;
      }
   }

   return NULL;
}

static bool
INST(MI_LOAD_REGISTER_IMM)(struct gen_mi_context *ctx,
                           struct GENX(MI_LOAD_REGISTER_IMM) *v)
{
   for (uint32_t i = 0; i < (v->__variable_length + 1); i++) {
      uint32_t offset, dword;

      if (i == 0) {
         offset = v->RegisterOffset;
         dword = v->DataDWord;
      } else {
         offset = v->__variable[i - 1].RegisterOffset;
         dword = v->__variable[i - 1].DataDWord;
      }

      uint32_t *ptr = reg_ptr(ctx, offset, true);
      if (ptr)
         *ptr = dword;
   }

   return false;
}

static bool
INST(MI_LOAD_REGISTER_MEM)(struct gen_mi_context *ctx,
                           struct GENX(MI_LOAD_REGISTER_MEM) *v)
{
   uint64_t addr = gen_48b_address(v->MemoryAddress);
   struct gen_mi_bo bo = ctx->get_bo(ctx->user_data, !v->UseGlobalGTT, addr);
   if (!bo.map)
      return false;

   uint32_t *mem = bo.map + addr - bo.gtt_offset;
   uint32_t *ptr = reg_ptr(ctx, v->RegisterAddress, true);
   if (ptr)
      *ptr = *mem;

   return false;
}

#if GEN_GEN >= 8 || GEN_IS_HASWELL
static bool
INST(MI_LOAD_REGISTER_REG)(struct gen_mi_context *ctx,
                           struct GENX(MI_LOAD_REGISTER_REG) *v)
{
   uint32_t *src = reg_ptr(ctx, v->SourceRegisterAddress, false);
   uint32_t *dst = reg_ptr(ctx, v->DestinationRegisterAddress, true);
   if (src && dst)
      *dst = *src;

   return false;
}
#endif

static bool
INST(MI_STORE_DATA_IMM)(struct gen_mi_context *ctx,
                        struct GENX(MI_STORE_DATA_IMM) *v)
{
   uint64_t addr = gen_48b_address(v->Address);
   struct gen_mi_bo bo = ctx->get_bo(ctx->user_data, !v->UseGlobalGTT, addr);
   if (!bo.map)
      return false;

   if (v->DWordLength == 3) {
      uint64_t *mem = bo.map + addr - bo.gtt_offset;
      *mem = v->ImmediateData;
   } else {
      uint32_t *mem = bo.map + addr - bo.gtt_offset;
      *mem = v->ImmediateData;
   }

   return false;
}

static bool
INST(MI_STORE_REGISTER_MEM)(struct gen_mi_context *ctx,
                            struct GENX(MI_STORE_REGISTER_MEM) *v)
{
#if GEN_GEN >= 8 || GEN_IS_HASWELL
   if (v->PredicateEnable && !(ctx->predicate.result >> 32))
      return false;
#endif

   uint64_t addr = gen_48b_address(v->MemoryAddress);
   struct gen_mi_bo bo = ctx->get_bo(ctx->user_data, !v->UseGlobalGTT, addr);
   if (!bo.map)
      return false;

   uint32_t *mem = bo.map + addr - bo.gtt_offset;
   uint32_t *reg = reg_ptr(ctx, v->RegisterAddress, false);
   *mem = reg ? *reg : 0;

   return false;
}

#if GEN_GEN >= 8
static bool
INST(MI_COPY_MEM_MEM)(struct gen_mi_context *ctx,
                      struct GENX(MI_COPY_MEM_MEM) *v)
{

   uint64_t src_addr = gen_48b_address(v->SourceMemoryAddress);
   uint64_t dst_addr = gen_48b_address(v->DestinationMemoryAddress);
   struct gen_mi_bo src_bo = ctx->get_bo(ctx->user_data,
                                         !v->UseGlobalGTTSource, src_addr);
   struct gen_mi_bo dst_bo = ctx->get_bo(ctx->user_data,
                                         !v->UseGlobalGTTDestination, dst_addr);
   if (!src_bo.map || !dst_bo.map)
      return false;

   uint32_t *src_mem = src_bo.map + src_addr - src_bo.gtt_offset;
   uint32_t *dst_mem = dst_bo.map + dst_addr - dst_bo.gtt_offset;
   *dst_mem = *src_mem;

   return false;
}
#endif

static bool
INST(MI_BATCH_BUFFER_START)(struct gen_mi_context *ctx,
                            struct GENX(MI_BATCH_BUFFER_START) *v)
{
   uint64_t addr = gen_48b_address(v->BatchBufferStartAddress);

#if GEN_GEN >= 8 || GEN_IS_HASWELL
   if (v->PredicationEnable && !(ctx->predicate.result >> 32))
      return false;

   if (v->SecondLevelBatchBuffer && ctx->pc_depth == 1) {
      ctx->pc[ctx->pc_depth] += 4 * (GENX(MI_BATCH_BUFFER_START_length_bias) + v->DWordLength);
      ctx->pc_depth++;
   }
#endif

   ctx->pc[ctx->pc_depth] = addr;

   return true;
}

static bool
INST(MI_BATCH_BUFFER_END)(struct gen_mi_context *ctx,
                          struct GENX(MI_BATCH_BUFFER_END) *v)
{
   if (ctx->pc_depth == 0)
      return true;

   ctx->pc_depth--;

   return true;
}

#if GEN_GEN >= 8 || GEN_IS_HASWELL
static bool
INST(MI_PREDICATE)(struct gen_mi_context *ctx,
                   struct GENX(MI_PREDICATE) *v)
{
   uint64_t compare_res = 0;
   switch (v->CompareOperation) {
   case COMPARE_TRUE:
      compare_res = U64_1;
      break;
   case COMPARE_FALSE:
      compare_res = 0;
      break;
   case COMPARE_SRCS_EQUAL:
      ctx->predicate.data = ctx->predicate.src0 - ctx->predicate.src1;
      compare_res = ctx->predicate.src0 == ctx->predicate.src1 ? U64_1 : 0;
      break;
   case COMPARE_DELTAS_EQUAL:
      compare_res = (ctx->predicate.src0 - ctx->predicate.src1) == ctx->predicate.data ? U64_1 : 0;
      break;
   }

   uint64_t predicate_bit = ctx->predicate.result != 0 ? U64_1 : 0;
   uint64_t predicate_res = 0;
   switch (v->CombineOperation) {
   case COMBINE_SET:
      predicate_res = compare_res;
      break;
   case COMBINE_AND:
      predicate_res = predicate_bit & compare_res;
      break;
   case COMBINE_OR:
      predicate_res = predicate_bit | compare_res;
      break;
   case COMBINE_XOR:
      predicate_res = predicate_bit ^ compare_res;
      break;
   }

   switch (v->LoadOperation) {
   case LOAD_KEEP:
      break;
   case LOAD_LOAD:
      ctx->predicate.result = predicate_res;
      break;
   case LOAD_LOADINV:
      ctx->predicate.result = ~predicate_res;
      break;
   }

   return false;
}

static uint64_t *
operand_ptr(struct gen_mi_context *ctx, uint32_t name)
{
   if (name >= MI_ALU_REG0 &&
       name <= MI_ALU_REG15)
      return &ctx->gpr64[name];
   switch (name) {
   case MI_ALU_SRCA:
      return &ctx->alu.src0;
   case MI_ALU_SRCB:
      return &ctx->alu.src1;
   case MI_ALU_ACCU:
      return &ctx->alu.accu;
   case MI_ALU_ZF:
      return &ctx->alu.zf;
   case MI_ALU_CF:
      return &ctx->alu.cf;
   }
   return NULL;
}

static bool
INST(MI_MATH)(struct gen_mi_context *ctx,
              struct GENX(MI_MATH) *v)
{
   assert(ctx->alu.inst_idx < v->__variable_length);

   ctx->alu.inst_count = v->__variable_length;

   struct GENX(MI_MATH_ALU_INSTRUCTION) *inst =
      &v->__variable[ctx->alu.inst_idx++].Instruction;

   uint64_t *op0 = operand_ptr(ctx, inst->Operand1);
   uint64_t *op1 = operand_ptr(ctx, inst->Operand2);

   switch (inst->ALUOpcode) {
   case MI_ALU_NOOP:
      break;
   case MI_ALU_LOAD:
      *op0 = *op1;
      break;
   case MI_ALU_LOADINV:
      *op0 = ~(*op1);
      break;
   case MI_ALU_LOAD0:
      *op0 = 0;
      break;
   case MI_ALU_LOAD1:
      *op0 = 0xffffffffffffffff;
      break;
   case MI_ALU_ADD:
      ctx->alu.accu = ctx->alu.src0 + ctx->alu.src1;
      ctx->alu.cf = (ctx->alu.accu < ctx->alu.src0 ||
                     ctx->alu.accu < ctx->alu.src1) ? U64_1 : 0;
      ctx->alu.zf = ctx->alu.accu == 0;
      break;
   case MI_ALU_SUB:
      ctx->alu.accu = ctx->alu.src0 - ctx->alu.src1;
      ctx->alu.cf = (ctx->alu.src0 < ctx->alu.src1) ? U64_1 : 0;
      ctx->alu.zf = ctx->alu.accu == 0;
      break;
   case MI_ALU_AND:
      ctx->alu.accu = ctx->alu.src0 & ctx->alu.src1;
      ctx->alu.cf = 0;
      ctx->alu.zf = ctx->alu.accu == 0;
      break;
   case MI_ALU_OR:
      ctx->alu.accu = ctx->alu.src0 | ctx->alu.src1;
      ctx->alu.cf = 0;
      ctx->alu.zf = ctx->alu.accu == 0;
      break;
   case MI_ALU_XOR:
      ctx->alu.accu = ctx->alu.src0 ^ ctx->alu.src1;
      ctx->alu.cf = 0;
      ctx->alu.zf = ctx->alu.accu == 0;
      break;
   case MI_ALU_STORE:
      *op0 = *op1;
      break;
   case MI_ALU_STOREINV:
      *op0 = ~(*op1);
      break;
   }

   if (ctx->alu.inst_idx < v->__variable_length)
      return true;

   ctx->alu.inst_idx = 0;
   ctx->alu.inst_count = 0;
   return false;
}
#endif

static bool
INST(MI_REPORT_PERF_COUNT)(struct gen_mi_context *ctx,
                           struct GENX(MI_REPORT_PERF_COUNT) *v)
{
   uint64_t dst_addr = gen_48b_address(v->MemoryAddress);
   struct gen_mi_bo dst_bo =
      ctx->get_bo(ctx->user_data, !v->UseGlobalGTT, dst_addr);

   if (!dst_bo.map)
      return false;

   /* Assume the HW has been configured with 256bytes report size (only size
    * we use in Mesa).
    */
   uint32_t *dw = dst_bo.map + dst_addr - dst_bo.gtt_offset;
   dw[0] = v->ReportID;
   dw[1] = os_time_get_nano(); /* Timestamp */
   dw[2] = 0x42; /* HW ID */
   for (uint32_t i = 3; i < 64; i++)
      dw[i] = 0xdeadbee;

   return false;
}

static bool
INST(PIPE_CONTROL)(struct gen_mi_context *ctx,
                   struct GENX(PIPE_CONTROL) *v)
{
   if (v->PostSyncOperation != NoWrite) {
      uint64_t dst_addr = gen_48b_address(v->Address);
      struct gen_mi_bo dst_bo =
         ctx->get_bo(ctx->user_data, !v->DestinationAddressType, dst_addr);

      if (dst_bo.map) {
         switch (v->PostSyncOperation) {
         case WriteImmediateData:
            *(uint64_t *) (dst_bo.map + dst_addr - dst_bo.gtt_offset) = v->ImmediateData;
            break;
         case WritePSDepthCount:
            *(uint64_t *) (dst_bo.map + dst_addr - dst_bo.gtt_offset) = 0;
            break;
         case WriteTimestamp:
            *(uint64_t *) (dst_bo.map + dst_addr - dst_bo.gtt_offset) = os_time_get_nano();
            break;
         }
      }
   }

   return false;
}

typedef uint32_t (*mi_inst_read_length)(const void *src);
typedef void (*mi_inst_unpack)(void *decoded_data, const void *src);
typedef bool (*mi_inst_exec)(struct gen_mi_context *ctx, void *decoded_data);

#define INST_ENTRY(name) \
   { GENX(name##_opcode), \
     GENX(name##_opcode_mask), \
     GENX(name##_read_length), \
     (mi_inst_unpack) GENX(name##_unpack), \
     (mi_inst_exec) INST(name) }

static struct gen_mi_inst {
   uint32_t opcode;
   uint32_t opcode_mask;
   mi_inst_read_length read_length;
   mi_inst_unpack unpack;
   mi_inst_exec exec;
} mi_insts[] = {
   INST_ENTRY(MI_LOAD_REGISTER_IMM),
   INST_ENTRY(MI_LOAD_REGISTER_MEM),
   INST_ENTRY(MI_STORE_DATA_IMM),
   INST_ENTRY(MI_STORE_REGISTER_MEM),
   INST_ENTRY(MI_BATCH_BUFFER_START),
   INST_ENTRY(MI_BATCH_BUFFER_END),
#if GEN_GEN >= 8 || GEN_IS_HASWELL
   INST_ENTRY(MI_LOAD_REGISTER_REG),
   INST_ENTRY(MI_MATH),
   INST_ENTRY(MI_PREDICATE),
#endif
#if GEN_GEN >= 8
   INST_ENTRY(MI_COPY_MEM_MEM),
#endif
   INST_ENTRY(MI_REPORT_PERF_COUNT),
   INST_ENTRY(PIPE_CONTROL),
};

/* Executes one instruction and modify the context accordingly. Return false
 * in case of error.
 */
enum gen_mi_runner_status
genX(mi_runner_execute_one_inst)(struct gen_mi_context *ctx)
{
   struct gen_mi_bo bo = ctx->get_bo(ctx->user_data,
                                     ctx->pc_as[ctx->pc_depth],
                                     ctx->pc[ctx->pc_depth]);

   if (!bo.map)
      return GEN_MI_RUNNER_STATUS_ERROR;

   uint32_t *p = bo.map + (ctx->pc[ctx->pc_depth] - bo.gtt_offset);

   struct gen_mi_inst *inst = NULL;
   for (uint32_t i = 0; i < ARRAY_SIZE(mi_insts); i++) {
      if (mi_insts[i].opcode == (*p & mi_insts[i].opcode_mask)) {
         inst = &mi_insts[i];
         break;
      }
   }

   bool move_forward = true;

   if (inst) {
      uint32_t decoded_data_len = inst->read_length(p);
      if (decoded_data_len > ctx->decoded_data_len) {
         ctx->decoded_data = realloc(ctx->decoded_data, decoded_data_len);
         ctx->decoded_data_len = decoded_data_len;
      }

      inst->unpack(ctx->decoded_data, p);
      move_forward = !inst->exec(ctx, ctx->decoded_data);
   }

   /* MI_BATCH_BUFFER_START/END won't require updating as they change the
    * context directly.
    */
   if (move_forward) {
      struct gen_group *group = gen_spec_find_instruction(ctx->spec, ctx->engine, p);
      if (!group)
         return GEN_MI_RUNNER_STATUS_ERROR;
      uint32_t len = gen_group_get_length(group, p);
      ctx->pc[ctx->pc_depth] += 4 * len;
   }

   return GEN_MI_RUNNER_STATUS_OK;
}
