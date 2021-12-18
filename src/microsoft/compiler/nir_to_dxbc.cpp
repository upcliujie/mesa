/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "nir_to_dxbc.h"

#include "blob.h"
#include "dxil_container.h"
#include "dxil_enums.h"
#include "dxil_module.h"
#include "dxil_signature.h"
#include "dxil_nir.h"
#include "nir.h"
#include "nir/nir_builder.h"
#include "nir_intrinsics.h"
#include "nir_opcodes.h"
#include "ralloc.h"
#include "util/u_debug.h"
#include "util/u_dynarray.h"
#include "util/u_math.h"

#include "git_sha1.h"

#include "vulkan/vulkan_core.h"

#include "ShaderBinary.h"
#include "d3d12TokenizedProgramFormat.hpp"

#include <stdint.h>
#include <vector>
#include <memory>

#ifdef _WIN32
#include <Unknwn.h>
#include <dxcapi.h>
#endif

using namespace D3D10ShaderBinary;

int debug_dxbc = 0;

enum dxbc_debug_flags {
   DXBC_DEBUG_VERBOSE = 1 << 0,
   DXBC_DEBUG_DUMP_BLOB = 1 << 1,
   DXBC_DEBUG_TRACE = 1 << 2,
   DXBC_DEBUG_DISASSEMBLE = 1 << 3,
};

static const struct debug_named_value
dxbc_debug_options[] = {
   { "verbose", DXBC_DEBUG_VERBOSE, NULL },
   { "dump_blob",  DXBC_DEBUG_DUMP_BLOB , "Write shader blobs" },
   { "trace",  DXBC_DEBUG_TRACE , "Trace instruction conversion" },
   { "disassemble", DXBC_DEBUG_DISASSEMBLE, "Use d3dcompiler to disassemble shaders to stderr"},
   DEBUG_NAMED_VALUE_END
};

DEBUG_GET_ONCE_FLAGS_OPTION(debug_dxbc, "DXBC_DEBUG", dxbc_debug_options, 0)

#define NIR_INSTR_UNSUPPORTED(instr)                                         \
   if (debug_dxbc & DXBC_DEBUG_VERBOSE)                                      \
      do {                                                                   \
         fprintf(stderr, "Unsupported instruction:");                        \
         nir_print_instr(instr, stderr);                                     \
         fprintf(stderr, "\n");                                              \
   } while (0)

#define TRACE_CONVERSION(instr)                                              \
   if (debug_dxbc & DXBC_DEBUG_TRACE)                                        \
      do {                                                                   \
         fprintf(stderr, "Convert '");                                       \
         nir_print_instr(instr, stderr);                                     \
         fprintf(stderr, "'\n");                                             \
   } while (0)

const nir_shader_compiler_options *
dxbc_get_nir_compiler_options(void)
{
   static nir_shader_compiler_options nir_options{};
   nir_options.lower_ffma16 = true;
   nir_options.lower_ffma32 = true;
   nir_options.lower_flrp16 = true;
   nir_options.lower_flrp32 = true;
   nir_options.lower_flrp64 = true;
   nir_options.lower_fpow = true;
   nir_options.lower_fmod = true;
   nir_options.lower_bitfield_extract_to_shifts = true;
   nir_options.lower_uadd_carry = true;
   nir_options.lower_mul_high = true;
   nir_options.lower_fneg = true;
   nir_options.lower_ineg = true;
   nir_options.lower_scmp = true;
   nir_options.lower_isign = true;
   nir_options.lower_fsign = true;
   nir_options.lower_iabs = true;
   nir_options.lower_ldexp = true;
   nir_options.lower_pack_64_2x32_split = true;
   nir_options.lower_pack_32_2x16_split = true;
   nir_options.lower_unpack_64_2x32_split = true;
   nir_options.lower_unpack_32_2x16_split = true;
   nir_options.lower_extract_byte = true;
   nir_options.lower_extract_word = true;
   nir_options.lower_insert_byte = true;
   nir_options.lower_insert_word = true;
   nir_options.lower_all_io_to_temps = true;
   nir_options.lower_all_io_to_elements = true;
   nir_options.vertex_id_zero_based = true;
   nir_options.lower_base_vertex = true;
   nir_options.has_cs_global_id = true;
   nir_options.lower_hadd = true;
   nir_options.lower_uadd_sat = true;
   nir_options.lower_iadd_sat = true;
   nir_options.lower_rotate = true;
   nir_options.has_fsub = true;
   nir_options.has_isub = true;
   nir_options.has_txs = true;
   nir_options.use_scoped_barrier = true;

   // TODO: fxc turns `mul(float4,float4)` into `dp4`, but we're running a
   // pass somewhere that converts the `fdot` from SPIR-V's `OpDot` to
   // `fmul`/`fadd`.
   return &nir_options;
}

struct ntd_ssa {
   COperandBase operand;
   int instruction_index = -1;
};

struct DxbcModule {
   D3D10_SB_TOKENIZED_PROGRAM_TYPE shader_kind;
   uint32_t major_version;
   uint32_t minor_version;
   CShaderAsm shader;

   std::unique_ptr<ntd_ssa[]> ssa_operands;
   std::vector<CInstruction> instructions;

   uint32_t reg_alloc;
};

struct ntd_context {
   void *ralloc_ctx;
   const struct nir_to_dxil_options *opts{};
   struct nir_shader *shader{};
   DxbcModule mod{};
   dxil_module dxil_mod{};
   nir_variable *system_value[SYSTEM_VALUE_MAX];

   ntd_context()
   {
      ralloc_ctx = ralloc_context(NULL);
      if (ralloc_ctx)
         dxil_module_init(&dxil_mod, ralloc_ctx);
   }
   ~ntd_context()
   {
      dxil_module_release(&dxil_mod);
      ralloc_free(ralloc_ctx);
   }
};

class ScopedDxilContainer {
 public:
   ScopedDxilContainer() { dxil_container_init(&inner); }
   ~ScopedDxilContainer() { dxil_container_finish(&inner); }
   dxil_container *
   get()
   {
      return &inner;
   }

 private:
   dxil_container inner{};
};

static COperandDst
nir_dest_as_register(ntd_context *ctx, nir_dest dest, uint32_t write_mask)
{
   unsigned index = dest.is_ssa ? ctx->mod.reg_alloc++ : dest.reg.reg->index;
   return COperandDst(D3D10_SB_OPERAND_TYPE_TEMP, index,
       write_mask << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT);
}

static COperandBase
nir_src_as_const_value_or_register(ntd_context *ctx, nir_src src, uint32_t num_write_components,
                                   uint8_t swizzle[4])
{
   uint32_t num_components =
       std::min(num_write_components, nir_src_num_components(src));

   if (src.is_ssa) {
      COperandBase ret = ctx->mod.ssa_operands[src.ssa->index].operand;

      // For derefs, the num_components is about the resulting resource access,
      // not about the number of components in the deref itself.
      nir_deref_instr *deref = nir_src_as_deref(src);
      if (deref)
         num_components = 4;

      if (num_components == 1) {
         // Already a single component
         if (ret.m_NumComponents == D3D10_SB_OPERAND_1_COMPONENT) {
            assert(!swizzle || swizzle[0] == 0);
            return ret;
         }

         D3D10_SB_4_COMPONENT_NAME component = D3D10_SB_4_COMPONENT_X;
         if (swizzle)
            component = static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[0]);

         // Re-swizzle the constant to be a single component
         if (ret.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 || ret.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
            ret.m_NumComponents = D3D10_SB_OPERAND_1_COMPONENT;
            if (ret.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 && component > 0)
               ret.m_Value[0] = ret.m_Value[component];
            else if (component > 0)
               ret.m_Value64[0] = ret.m_Value[component];
            return ret;
         }

         // Switch to single-component selection
         ret.SelectComponent(static_cast<D3D10_SB_4_COMPONENT_NAME>(ret.SwizzleComponent(component)));
         return ret;
      }

      // Don't know how to promote from 1 to 4
      assert(ret.m_NumComponents != D3D10_SB_OPERAND_1_COMPONENT);

      if (swizzle) {
         uint8_t final_swizzle[4];
         for (unsigned i = 0; i < 4; ++i)
            final_swizzle[i] = ret.SwizzleComponent(swizzle[i]);
         ret.SetSwizzle(final_swizzle[0], final_swizzle[1], final_swizzle[2], final_swizzle[3]);
      }

      return ret;
   }

   // nir doesn't ever have a direct load from an input to a usage, so in
   // load_input we move the value to a temp, so we can assume all
   // `nir_src`s that point to registers are to temps.
   D3D10_SB_OPERAND_TYPE temp = D3D10_SB_OPERAND_TYPE_TEMP;
   unsigned index = src.reg.reg->index;
   switch (num_components) {
   case 1: {
      if (swizzle) {
         return COperand4(
               temp, index,
               static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[0]));
      } else {
         return COperand4(temp, index, D3D10_SB_4_COMPONENT_X);
      }
   }

   case 2:
   case 3:
   case 4:
      if (swizzle) {
         return COperand(
               temp, index,
               static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[0]),
               static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[1]),
               static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[2]),
               static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[3]));
      } else {
         return COperand4(temp, index);
      }

   default:
      unreachable("unhandled number of components");
   }
}

static uint32_t
count_write_components(uint32_t write_mask)
{
   uint32_t count = 0;
   for (int i = 0; i < 4; i++) {
      if ((write_mask & (1 << i)) != 0) {
         count += 1;
      }
   }
   return count;
}

static COperandBase
get_alu_src_as_const_value_or_register(ntd_context *ctx, nir_alu_instr *alu, int num_components, int src_index)
{
   return nir_src_as_const_value_or_register(ctx, alu->src[src_index].src,
      num_components, alu->src[src_index].swizzle);
}

static CInstruction
get_intr_1_args(ntd_context *ctx, D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu)
{
   uint32_t num_write_components =
       count_write_components(alu->dest.write_mask);
   COperandBase dst =
       nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
   COperandBase a = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 0);
   return CInstruction(opcode, dst, a);
}

static CInstruction
get_intr_2_args(ntd_context *ctx, D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu)
{
   uint32_t num_write_components =
       count_write_components(alu->dest.write_mask);
   COperandBase dst =
       nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
   COperandBase lhs = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 0);
   COperandBase rhs = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 1);
   return CInstruction(opcode, dst, lhs, rhs);
}

static CInstruction
get_intr_dot_product(ntd_context *ctx, D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu)
{
   uint32_t num_write_components =
      count_write_components(alu->dest.write_mask);
   uint32_t num_src_components = alu->op == nir_op_fdot4 ?
      4 : (alu->op == nir_op_fdot3 ? 3 : 2);
   COperandBase dst =
      nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
   COperandBase lhs = get_alu_src_as_const_value_or_register(ctx, alu, num_src_components, 0);
   COperandBase rhs = get_alu_src_as_const_value_or_register(ctx, alu, num_src_components, 1);
   return CInstruction(opcode, dst, lhs, rhs);
}

static CInstruction
get_intr_3_args(ntd_context *ctx, D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu)
{
   uint32_t num_write_components =
       count_write_components(alu->dest.write_mask);
   COperandBase dst =
       nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
   COperandBase arg0 = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 0);
   COperandBase arg1 = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 1);
   COperandBase arg2 = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 2);
   return CInstruction(opcode, dst, arg0, arg1, arg2);
}

static void
store_ssa_instruction(ntd_context *ctx, nir_ssa_def *dest, CInstruction const& instr)
{
   ctx->mod.ssa_operands[dest->index].instruction_index = (int)ctx->mod.instructions.size();
   ctx->mod.instructions.push_back(instr);
   for (unsigned i = 0; i < instr.m_NumOperands; ++i) {
      const COperandBase &instr_dest = instr.Operand(i);
      if (instr_dest.m_Type == D3D10_SB_OPERAND_TYPE_NULL)
         continue;
      assert(instr_dest.m_Type == D3D10_SB_OPERAND_TYPE_TEMP);
      assert(instr_dest.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
      assert(instr_dest.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
      assert(instr_dest.m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE);
      ctx->mod.ssa_operands[dest->index].operand = COperand4(instr_dest.m_Type, instr_dest.RegIndex(0));
      return;
   }
   unreachable("Instruction has no destination");
}

static void
store_instruction(ntd_context *ctx, nir_dest &dest, CInstruction const& instr)
{
   if (dest.is_ssa)
      store_ssa_instruction(ctx, &dest.ssa, instr);
   else
      ctx->mod.instructions.push_back(instr);
}

static bool
modify_ssa_instruction(ntd_context *ctx, nir_alu_instr *alu, void (*callback)(CInstruction&))
{
   if (alu->src[0].src.is_ssa && alu->dest.dest.is_ssa &&
      list_length(&alu->src[0].src.ssa->uses) + list_length(&alu->src[0].src.ssa->if_uses) == 1) {
      // Modify the previous instruction to saturate
      auto& prev_ssa_src = ctx->mod.ssa_operands[alu->src[0].src.ssa->index];
      if (prev_ssa_src.instruction_index >= 0) {
         CInstruction& prev_intr = ctx->mod.instructions[prev_ssa_src.instruction_index];
         callback(prev_intr);
         ctx->mod.ssa_operands[alu->dest.dest.ssa.index].operand =
            get_alu_src_as_const_value_or_register(ctx, alu, count_write_components(alu->dest.write_mask), 0);
         return true;
      }
   }
   return false;
}

static bool
emit_alu(struct ntd_context *ctx, nir_alu_instr *alu)
{
   COperand null_operand(D3D10_SB_OPERAND_TYPE_NULL);

   switch (alu->op) {
   case nir_op_vec2:
   case nir_op_vec3:
   case nir_op_vec4: {
      // TODO: coalesce like-sourced `mov`s together
      COperandDst dst = nir_dest_as_register(ctx, alu->dest.dest, 0);
      for (int chan = 0; chan < nir_dest_num_components(alu->dest.dest); chan++) {
         dst.SetMask((1 << chan) << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT);
         COperandBase src = get_alu_src_as_const_value_or_register(ctx, alu, 1, chan);
         CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
         ctx->mod.instructions.push_back(mov);
         if (alu->dest.dest.is_ssa)
            ctx->mod.ssa_operands[alu->dest.dest.ssa.index].operand =
               COperand4(D3D10_SB_OPERAND_TYPE_TEMP, dst.RegIndex(0));
      }
      return true;
   }

   case nir_op_fmax:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_MAX, alu));
      return true;

   case nir_op_fmin:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_MIN, alu));
      return true;

   case nir_op_fsat: {
      if (!modify_ssa_instruction(ctx, alu, [](CInstruction &prev_intr) { prev_intr.m_bSaturate = true; })) {
         // Previous instruction unsaturated value is used elsewhere, or something's not ssa,
         // or it wasn't just one DXBC instruction to produce value, just generate a saturating mov
         CInstruction intr =
            get_intr_1_args(ctx, D3D10_SB_OPCODE_MOV, alu);
         intr.m_bSaturate = true;
         store_instruction(ctx, alu->dest.dest, intr);
      }
      return true;
   }

   case nir_op_fmul:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_MUL, alu));
      return true;

   case nir_op_fdiv:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_DIV, alu));
      return true;

   case nir_op_fadd:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_ADD, alu));
      return true;

   case nir_op_fsub: {
      CInstruction intr =
          get_intr_2_args(ctx, D3D10_SB_OPCODE_ADD, alu);
      if (intr.m_Operands[2].m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
         unsigned num_components = intr.m_Operands[2].NumComponents();
         if (intr.m_Operands[2].NumComponents() == D3D10_SB_OPERAND_4_COMPONENT)
            num_components = 4;
         for (unsigned i = 0; i < num_components; ++i)
            intr.m_Operands[i].m_Valuef[i] = -intr.m_Operands[i].m_Valuef[i];
      } else
         intr.m_Operands[2].SetModifier(D3D10_SB_OPERAND_MODIFIER_NEG);
      store_instruction(ctx, alu->dest.dest, intr);
      return true;
   }

   case nir_op_bcsel:
      store_instruction(ctx, alu->dest.dest,
          get_intr_3_args(ctx, D3D10_SB_OPCODE_MOVC, alu));
      return true;

   case nir_op_feq:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_EQ, alu));
      return true;

   case nir_op_fneu:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_NE, alu));
      return true;

   case nir_op_mov:
      if (alu->dest.dest.is_ssa) {
         ctx->mod.ssa_operands[alu->dest.dest.ssa.index].operand =
            get_alu_src_as_const_value_or_register(ctx, alu, count_write_components(alu->dest.write_mask), 0);
      } else {
         store_instruction(ctx, alu->dest.dest,
             get_intr_1_args(ctx, D3D10_SB_OPCODE_MOV, alu));
      }
      return true;

   case nir_op_imax:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_IMAX, alu));
      return true;

   case nir_op_imin:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_IMIN, alu));
      return true;

   case nir_op_ige:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_IGE, alu));
      return true;

   case nir_op_ilt:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_ILT, alu));
      return true;

   case nir_op_ieq:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_IEQ, alu));
      return true;

   case nir_op_ine:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_INE, alu));
      return true;

   case nir_op_ior:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_OR, alu));
      return true;

   case nir_op_iand:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_AND, alu));
      return true;

   case nir_op_ixor:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_XOR, alu));
      return true;

   case nir_op_iadd:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_IADD, alu));
      return true;

   case nir_op_imul: {
      COperandBase dest = nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
      unsigned num_dest_comps = count_write_components(alu->dest.write_mask);
      COperandBase src0 = get_alu_src_as_const_value_or_register(ctx, alu, num_dest_comps, 0);
      COperandBase src1 = get_alu_src_as_const_value_or_register(ctx, alu, num_dest_comps, 1);
      store_instruction(ctx, alu->dest.dest,
         CInstruction(D3D10_SB_OPCODE_IMUL, null_operand, dest, src0, src1));
      return true;
   }

   case nir_op_isub: {
      CInstruction intr =
         get_intr_2_args(ctx, D3D10_SB_OPCODE_IADD, alu);
      if (intr.m_Operands[2].m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
         unsigned num_components = intr.m_Operands[2].NumComponents();
         if (intr.m_Operands[2].NumComponents() == D3D10_SB_OPERAND_4_COMPONENT)
            num_components = 4;
         for (unsigned i = 0; i < num_components; ++i)
            intr.m_Operands[i].m_Value[i] = -intr.m_Operands[i].m_Value[i];
      } else
         intr.m_Operands[2].SetModifier(D3D10_SB_OPERAND_MODIFIER_NEG);
      store_instruction(ctx, alu->dest.dest, intr);
      return true;
   }

   case nir_op_umax:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_UMAX, alu));
      return true;

   case nir_op_umin:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_UMIN, alu));
      return true;

   case nir_op_udiv:
   case nir_op_umod: {
      COperandBase dst0 = nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
      COperandBase dst1 = null_operand;
      if (alu->op == nir_op_umod)
         std::swap(dst0, dst1);
      unsigned num_write_components = count_write_components(alu->dest.write_mask);
      COperandBase src0 = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 0);
      COperandBase src1 = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 1);
      store_instruction(ctx, alu->dest.dest, CInstruction(D3D10_SB_OPCODE_UDIV,
            dst0, dst1, src0, src1));
      return true;
   }

   case nir_op_ushr:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_USHR, alu));
      return true;

   case nir_op_ishr:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_ISHR, alu));
      return true;

   case nir_op_ishl:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_ISHL, alu));
      return true;

   case nir_op_flt:
      store_instruction(ctx, alu->dest.dest,
          get_intr_2_args(ctx, D3D10_SB_OPCODE_LT, alu));
      return true;

   case nir_op_fsqrt:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_SQRT, alu));
      return true;

   case nir_op_ffloor:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_ROUND_NI, alu));
      return true;

   case nir_op_fceil:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_ROUND_PI, alu));
      return true;

   case nir_op_fround_even:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_ROUND_NE, alu));
      return true;

   case nir_op_ffract:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_FRC, alu));
      return true;

   case nir_op_ftrunc:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_ROUND_Z, alu));
      return true;

   case nir_op_fabs: {
      if (alu->dest.dest.is_ssa) {
         auto& operand = ctx->mod.ssa_operands[alu->dest.dest.ssa.index].operand;
         operand = get_alu_src_as_const_value_or_register(ctx, alu, count_write_components(alu->dest.write_mask), 0);
         operand.SetModifier(D3D10_SB_OPERAND_MODIFIER_ABS);
      } else {
         CInstruction intr =
             get_intr_1_args(ctx, D3D10_SB_OPCODE_MOV, alu);
         intr.m_Operands[1].SetModifier(D3D10_SB_OPERAND_MODIFIER_ABS);
         store_instruction(ctx, alu->dest.dest, intr);
      }
      return true;
   }

   case nir_op_f2i32:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_FTOI, alu));
      return true;

   case nir_op_i2f32:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_ITOF, alu));
      return true;

   case nir_op_u2f32:
      store_instruction(ctx, alu->dest.dest,
         get_intr_1_args(ctx, D3D10_SB_OPCODE_UTOF, alu));
      return true;

   case nir_op_f2u32:
      store_instruction(ctx, alu->dest.dest,
         get_intr_1_args(ctx, D3D10_SB_OPCODE_FTOU, alu));
      return true;

   case nir_op_b2f32:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_ITOF, alu));
      return true;

   case nir_op_b2i32:
      store_instruction(ctx, alu->dest.dest,
         get_intr_1_args(ctx, D3D10_SB_OPCODE_MOV, alu));
      return true;

   case nir_op_fcos: {
      uint32_t num_write_components =
          count_write_components(alu->dest.write_mask);
      COperandBase dst =
          nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
      COperandBase a = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 0);
      store_instruction(ctx, alu->dest.dest, CInstruction(
          D3D10_SB_OPCODE_SINCOS, null_operand, dst, a));
      return true;
   }

   case nir_op_fsin: {
      uint32_t num_write_components =
          count_write_components(alu->dest.write_mask);
      COperandBase dst =
          nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
      COperandBase a = get_alu_src_as_const_value_or_register(ctx, alu, num_write_components, 0);
      store_instruction(ctx, alu->dest.dest, CInstruction(
          D3D10_SB_OPCODE_SINCOS, dst, null_operand, a));
      return true;
   }

   case nir_op_fddx:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_DERIV_RTX, alu));
      return true;

   case nir_op_fddy:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D10_SB_OPCODE_DERIV_RTY, alu));
      return true;

   case nir_op_fddx_fine:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D11_SB_OPCODE_DERIV_RTX_FINE, alu));
      return true;

   case nir_op_fddy_fine:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D11_SB_OPCODE_DERIV_RTY_FINE, alu));
      return true;

   case nir_op_fddx_coarse:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D11_SB_OPCODE_DERIV_RTX_COARSE, alu));
      return true;

   case nir_op_fddy_coarse:
      store_instruction(ctx, alu->dest.dest,
          get_intr_1_args(ctx, D3D11_SB_OPCODE_DERIV_RTY_COARSE, alu));
      return true;

   case nir_op_fdot4:
      store_instruction(ctx, alu->dest.dest,
         get_intr_dot_product(ctx, D3D10_SB_OPCODE_DP4, alu));
      return true;

   case nir_op_fdot3:
      store_instruction(ctx, alu->dest.dest,
         get_intr_dot_product(ctx, D3D10_SB_OPCODE_DP3, alu));
      return true;

   case nir_op_fdot2:
      store_instruction(ctx, alu->dest.dest,
         get_intr_dot_product(ctx, D3D10_SB_OPCODE_DP2, alu));
      return true;

   case nir_op_frsq:
      store_instruction(ctx, alu->dest.dest,
         get_intr_1_args(ctx, D3D10_SB_OPCODE_RSQ, alu));
      return true;

   case nir_op_ult:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_ULT, alu));
      return true;

   case nir_op_uge:
      store_instruction(ctx, alu->dest.dest,
         get_intr_2_args(ctx, D3D10_SB_OPCODE_UGE, alu));
      return true;

   case nir_op_inot: {
      COperandBase dst = nir_dest_as_register(ctx, alu->dest.dest, alu->dest.write_mask);
      COperandBase src = get_alu_src_as_const_value_or_register(ctx, alu, count_write_components(alu->dest.write_mask), 0);
      COperand imm(UINT_MAX);
      std::fill(imm.m_Value, std::end(imm.m_Value), UINT_MAX);
      imm.m_NumComponents = src.m_NumComponents;
      imm.m_ComponentSelection = src.m_ComponentSelection;
      store_instruction(ctx, alu->dest.dest, CInstruction(D3D10_SB_OPCODE_XOR, dst, src, imm));
      break;
   }

   default:
      NIR_INSTR_UNSUPPORTED(&alu->instr);
      assert(!"Unimplemented ALU instruction");
      return false;
   }
   return true;
}

static void
src_to_index(ntd_context *ctx, unsigned const_base, nir_src src, COperandBase& op, int index_to_set)
{
   D3D10_SB_4_COMPONENT_NAME component = D3D10_SB_4_COMPONENT_X;
   if (!src.is_ssa) {
      op.SetIndex(index_to_set, const_base, D3D10_SB_OPERAND_TYPE_TEMP, src.reg.reg->index, 0, component);
      return;
   }

   COperandBase& src_operand = ctx->mod.ssa_operands[src.ssa->index].operand;
   if (src_operand.m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE)
      component = src_operand.m_ComponentName;
   else if (src_operand.m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)
      component = static_cast<D3D10_SB_4_COMPONENT_NAME>(src_operand.SwizzleComponent(0));
   if (src_operand.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
      op.SetIndex(index_to_set, const_base + src_operand.m_Value[component]);
   } else if (src_operand.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D && 
              src_operand.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
      op.SetIndex(index_to_set, const_base, src_operand.m_Type, src_operand.RegIndex(0), 0, component);
   } else if (src_operand.m_Type == D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP &&
              src_operand.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32 &&
              src_operand.m_IndexType[1] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
      op.SetIndex(index_to_set, const_base, src_operand.m_Type, src_operand.RegIndex(0), src_operand.RegIndex(1), component);
   } else {
      // Load from the source into a temp, we can't use the value as an index directly
      COperandDst dst(D3D10_SB_OPERAND_TYPE_TEMP, ctx->mod.reg_alloc++, D3D10_SB_OPERAND_4_COMPONENT_MASK_X);
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_MOV, dst, src_operand));
      op.SetIndex(index_to_set, const_base, dst.m_Type, dst.RegIndex(0), 0, D3D10_SB_4_COMPONENT_X);
   }
}

static COperandBase
get_output_operand_type(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   unsigned write_mask = nir_intrinsic_write_mask(intr) << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT;
   nir_io_semantics semantics = nir_intrinsic_io_semantics(intr);
   if (ctx->mod.shader_kind == D3D10_SB_PIXEL_SHADER) {
      switch (semantics.location) {
      case FRAG_RESULT_DEPTH:
         return COperandDst(write_mask, D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH);
      case FRAG_RESULT_STENCIL:
         return COperandDst(write_mask, D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF);
      default:
         break;
      }
   }

   COperandDst dst(D3D10_SB_OPERAND_TYPE_OUTPUT, 0, write_mask);
   src_to_index(ctx, nir_intrinsic_base(intr), intr->src[1], dst, 0);
   return dst;
}

static bool
emit_store_output(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase src =
       nir_src_as_const_value_or_register(ctx, intr->src[0], 4, nullptr);
   COperandBase dst = get_output_operand_type(ctx, intr);
   CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
   ctx->mod.instructions.push_back(mov);

   return true;
}

static bool
emit_load_input(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperand4 src(D3D10_SB_OPERAND_TYPE_INPUT, 0);
   src_to_index(ctx, nir_intrinsic_base(intr), intr->src[0], src, 0);

   unsigned num_components = nir_dest_num_components(intr->dest);
   unsigned base_component = nir_intrinsic_component(intr);

   if (num_components == 1)
      src.SelectComponent(static_cast<D3D10_SB_4_COMPONENT_NAME>(base_component));
   else {
      uint8_t swizzle[4] = {};
      for (unsigned i = 0; i < 4; ++i)
         swizzle[i] = base_component + i < 4 ? base_component + i : 0;
      src.SetSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
   }

   assert(intr->dest.is_ssa);
   ctx->mod.ssa_operands[intr->dest.ssa.index].operand = src;

   return true;
}

static bool
emit_load_per_vertex_input(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperand2D src(D3D10_SB_OPERAND_TYPE_INPUT, 0, 0);
   src_to_index(ctx, 0, intr->src[0], src, 0);
   src_to_index(ctx, nir_intrinsic_base(intr), intr->src[1], src, 1);

   assert(intr->dest.is_ssa);
   ctx->mod.ssa_operands[intr->dest.ssa.index].operand = src;

   return true;
}

static bool
emit_vulkan_resource_index(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   assert(intr->dest.is_ssa);
   COperandBase input = nir_src_as_const_value_or_register(ctx, intr->src[0], 2, nullptr);
   COperandBase output;
   assert(input.m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE ||
          input.m_NumComponents == D3D10_SB_OPERAND_1_COMPONENT);
   if (input.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
      output = COperand(input.m_Value[input.m_ComponentName], 0, 0, 0);
   } else {
      // Dynamic index, compose 2-vector
      COperandDst copy_dest(D3D10_SB_OPERAND_TYPE_TEMP, ctx->mod.reg_alloc++);
      copy_dest.SetMask(D3D10_SB_OPERAND_4_COMPONENT_MASK_X);
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_MOV, copy_dest, input));
      COperand single_zero(0u);
      copy_dest.SetMask(D3D10_SB_OPERAND_4_COMPONENT_MASK_Y);
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_MOV, copy_dest, single_zero));
      output = COperand4(D3D10_SB_OPERAND_TYPE_TEMP, copy_dest.RegIndex(0), 0, 1, 0, 0);
   }

   ctx->mod.ssa_operands[intr->dest.ssa.index].operand = output;
   return true;
}

static bool
emit_load_vulkan_descriptor(struct ntd_context *ctx,
                            nir_intrinsic_instr *intr)
{
   assert(intr->dest.is_ssa);
   ctx->mod.ssa_operands[intr->dest.ssa.index].operand =
      nir_src_as_const_value_or_register(ctx, intr->src[0], 2, nullptr);
   return true;
}

static bool
emit_load_ubo_dxil(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase src;
   src.m_Type = D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER;
   src.SetSwizzle();
   src.m_NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
   if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
      src.m_IndexDimension = D3D10_SB_OPERAND_INDEX_3D;
      nir_binding binding = nir_chase_binding(intr->src[0]);
      nir_variable *var = nir_get_binding_variable(ctx->shader, binding);
      src.SetIndex(0, var->data.driver_location);
      src_to_index(ctx, ctx->opts->vulkan_environment ? binding.binding : 0, intr->src[0], src, 1);
      src_to_index(ctx, 0, intr->src[1], src, 2);
   } else {
      src.m_IndexDimension = D3D10_SB_OPERAND_INDEX_2D;
      src_to_index(ctx, 0, intr->src[0], src, 0);
      src_to_index(ctx, 0, intr->src[1], src, 1);
   }

   assert(intr->dest.is_ssa);
   if (list_length(&intr->dest.ssa.uses) + list_length(&intr->dest.ssa.if_uses) == 1)
      ctx->mod.ssa_operands[intr->dest.ssa.index].operand = src;
   else {
      COperandDst dst = nir_dest_as_register(ctx, intr->dest, 0b1111);
      store_instruction(ctx, intr->dest, CInstruction(D3D10_SB_OPCODE_MOV, dst, src));
   }
   return true;
}

static COperandBase
get_ssbo_operand(struct ntd_context *ctx, nir_src src, bool is_src)
{
   COperandBase op;
   nir_binding binding = nir_chase_binding(src);
   nir_variable *var = nir_get_binding_variable(ctx->shader, binding);
   op.m_Type = (var->data.access & ACCESS_NON_WRITEABLE) ?
      D3D10_SB_OPERAND_TYPE_RESOURCE : D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW;
   if (is_src)
      op.SetSwizzle();
   else
      op.SetMask();
   op.m_NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
   if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
      op.m_IndexDimension = D3D10_SB_OPERAND_INDEX_2D;
      op.SetIndex(0, var->data.driver_location);
      src_to_index(ctx, ctx->opts->vulkan_environment ? binding.binding : 0, src, op, 1);
   } else {
      op.m_IndexDimension = D3D10_SB_OPERAND_INDEX_1D;
      src_to_index(ctx, 0, src, op, 0);
   }

   return op;
}

static bool
emit_load_ssbo(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase src = get_ssbo_operand(ctx, intr->src[0], true);
   unsigned num_components = nir_dest_num_components(intr->dest);
   COperandBase dst = nir_dest_as_register(ctx, intr->dest, (1 << num_components) - 1);
   COperandBase offset = nir_src_as_const_value_or_register(ctx, intr->src[1], 1, nullptr);
   store_instruction(ctx, intr->dest, CInstruction(D3D11_SB_OPCODE_LD_RAW, dst, offset, src));

   return true;
}

static bool
emit_store_ssbo(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase dst = get_ssbo_operand(ctx, intr->src[1], false);
   unsigned num_components = nir_src_num_components(intr->src[0]);
   dst.SetMask(((1 << num_components) - 1) << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT);
   COperandBase value = nir_src_as_const_value_or_register(ctx, intr->src[0], num_components, nullptr);
   COperandBase offset = nir_src_as_const_value_or_register(ctx, intr->src[2], 1, nullptr);
   store_instruction(ctx, intr->dest, CInstruction(D3D11_SB_OPCODE_STORE_RAW, dst, offset, value));

   return true;
}

static bool
emit_get_ssbo_size(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase src = get_ssbo_operand(ctx, intr->src[0], true);
   COperandBase dest = nir_dest_as_register(ctx, intr->dest, 0b1111);
   store_instruction(ctx, intr->dest, CInstruction(D3D11_SB_OPCODE_BUFINFO, dest, src));
   return true;
}

static bool
emit_image_deref_size(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase dest = nir_dest_as_register(ctx, intr->dest, 0b1111);
   COperandBase src = nir_src_as_const_value_or_register(ctx, intr->src[0], 4, nullptr);
   COperand lod(0u);
   CInstruction inst =
      glsl_get_sampler_dim(glsl_without_array(nir_deref_instr_get_variable(
         nir_src_as_deref(intr->src[0]))->type)) == GLSL_SAMPLER_DIM_BUF ?
            CInstruction(D3D11_SB_OPCODE_BUFINFO, dest, src) :
            CInstruction(D3D10_SB_OPCODE_RESINFO, dest, lod, src);
   inst.m_ResInfoReturnType = D3D10_SB_RESINFO_INSTRUCTION_RETURN_UINT;
   store_instruction(ctx, intr->dest, inst);
   return true;
}

static bool
emit_image_deref_store(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase dest = nir_src_as_const_value_or_register(ctx, intr->src[0], 4, nullptr);
   dest.SetMask();
   COperandBase coords = nir_src_as_const_value_or_register(ctx, intr->src[1], 4, nullptr);
   COperandBase value = nir_src_as_const_value_or_register(ctx, intr->src[3], 4, nullptr);
   CInstruction inst(D3D11_SB_OPCODE_STORE_UAV_TYPED, dest, coords, value);
   store_instruction(ctx, intr->dest, inst);
   return true;
}

static bool
emit_scoped_barrier(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   CInstruction inst(D3D11_SB_OPCODE_SYNC);

   if (nir_intrinsic_execution_scope(intr) == NIR_SCOPE_WORKGROUP)
      inst.m_SyncFlags.bThreadsInGroup = true;

   nir_variable_mode modes = nir_intrinsic_memory_modes(intr);
   nir_scope mem_scope = nir_intrinsic_memory_scope(intr);

   /* Currently vtn uses uniform to indicate image memory, which DXIL considers global */
   if (modes & nir_var_uniform)
      modes |= nir_var_mem_global;

   if (modes & (nir_var_mem_ssbo | nir_var_mem_global)) {
      if (mem_scope > NIR_SCOPE_WORKGROUP)
         inst.m_SyncFlags.bUnorderedAccessViewMemoryGlobal = true;
      else
         inst.m_SyncFlags.bUnorderedAccessViewMemoryGroup = true;
   }

   if (modes & nir_var_mem_shared)
      inst.m_SyncFlags.bThreadGroupSharedMemory = true;

   ctx->mod.instructions.push_back(inst);
   return true;
}

static bool
emit_load_shared(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase dest = nir_dest_as_register(ctx, intr->dest, (1 << nir_dest_num_components(intr->dest)) - 1);
   COperandBase offset = nir_src_as_const_value_or_register(ctx, intr->src[0], 1, nullptr);
   COperand4 tgsm(D3D11_SB_OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY, 0);
   if (nir_intrinsic_base(intr)) {
      COperandDst temp_dest(D3D10_SB_OPERAND_TYPE_TEMP, ctx->mod.reg_alloc++, D3D10_SB_OPERAND_4_COMPONENT_MASK_X);
      COperand offset_addend(nir_intrinsic_base(intr));
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_IADD, temp_dest, offset, offset_addend));
      offset = COperand4(D3D10_SB_OPERAND_TYPE_TEMP, temp_dest.RegIndex(0), D3D10_SB_4_COMPONENT_X);
   }
   if (nir_dest_num_components(intr->dest) == 1)
      tgsm.SelectComponent();
   else
      tgsm.SetSwizzle();
   store_instruction(ctx, intr->dest, CInstruction(D3D11_SB_OPCODE_LD_RAW, dest, offset, tgsm));
   return true;
}

static bool
emit_store_shared(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   COperandBase value = nir_src_as_const_value_or_register(ctx, intr->src[0], 4, nullptr);
   COperandBase offset = nir_src_as_const_value_or_register(ctx, intr->src[1], 1, nullptr);
   COperandDst tgsm(D3D11_SB_OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY, 0,
      nir_intrinsic_write_mask(intr) << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT);
   if (nir_intrinsic_base(intr)) {
      COperandDst temp_dest(D3D10_SB_OPERAND_TYPE_TEMP, ctx->mod.reg_alloc++, D3D10_SB_OPERAND_4_COMPONENT_MASK_X);
      COperand offset_addend(nir_intrinsic_base(intr));
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_IADD, temp_dest, offset, offset_addend));
      offset = COperand4(D3D10_SB_OPERAND_TYPE_TEMP, temp_dest.RegIndex(0), D3D10_SB_4_COMPONENT_X);
   }
   store_instruction(ctx, intr->dest, CInstruction(D3D11_SB_OPCODE_STORE_RAW, tgsm, offset, value));
   return true;
}

static bool
emit_load_system_value(ntd_context *ctx, nir_intrinsic_instr *intr, D3D10_SB_OPERAND_TYPE operand)
{
   COperand op(operand);
   if (op.m_NumComponents == D3D10_SB_OPERAND_4_COMPONENT)
      op.SetSwizzle();
   else {
      // Looks like the helper and at least WARP's DXBC reader disagree here, so
      // follow WARP's expected behavior which probably matches FXC
      op.m_NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
      op.SelectComponent();
   }
   if (intr->dest.is_ssa && list_length(&intr->dest.ssa.uses) + list_length(&intr->dest.ssa.if_uses) == 1)
      ctx->mod.ssa_operands[intr->dest.ssa.index].operand = op;
   else {
      COperandDst dst = nir_dest_as_register(ctx, intr->dest, (1 << nir_dest_num_components(intr->dest)) - 1);
      store_instruction(ctx, intr->dest, CInstruction(D3D10_SB_OPCODE_MOV, dst, op));
   }
   return true;
}

static bool
emit_intrinsic(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_input:
      return emit_load_input(ctx, intr);
   case nir_intrinsic_load_per_vertex_input:
      return emit_load_per_vertex_input(ctx, intr);
   case nir_intrinsic_store_output:
      return emit_store_output(ctx, intr);

   case nir_intrinsic_load_global_invocation_id:
      return emit_load_system_value(ctx, intr, D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID);
   case nir_intrinsic_load_workgroup_id:
      return emit_load_system_value(ctx, intr, D3D11_SB_OPERAND_TYPE_INPUT_THREAD_GROUP_ID);
   case nir_intrinsic_load_local_invocation_id:
      return emit_load_system_value(ctx, intr, D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP);
   case nir_intrinsic_load_local_invocation_index:
      return emit_load_system_value(ctx, intr, D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED);
   case nir_intrinsic_load_primitive_id:
      return emit_load_system_value(ctx, intr, D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID);

   case nir_intrinsic_vulkan_resource_index:
      return emit_vulkan_resource_index(ctx, intr);
   case nir_intrinsic_load_vulkan_descriptor:
      return emit_load_vulkan_descriptor(ctx, intr);

   case nir_intrinsic_load_ubo_dxil:
      return emit_load_ubo_dxil(ctx, intr);
   case nir_intrinsic_load_ssbo:
      return emit_load_ssbo(ctx, intr);
   case nir_intrinsic_store_ssbo:
      return emit_store_ssbo(ctx, intr);
   case nir_intrinsic_get_ssbo_size:
      return emit_get_ssbo_size(ctx, intr);

   case nir_intrinsic_image_deref_size:
      return emit_image_deref_size(ctx, intr);
   case nir_intrinsic_image_deref_store:
      return emit_image_deref_store(ctx, intr);

   case nir_intrinsic_scoped_barrier:
      return emit_scoped_barrier(ctx, intr);

   case nir_intrinsic_load_shared:
      return emit_load_shared(ctx, intr);
   case nir_intrinsic_store_shared:
      return emit_store_shared(ctx, intr);

   case nir_intrinsic_end_primitive:
   case nir_intrinsic_emit_vertex: {
      COperand src(D3D11_SB_OPERAND_TYPE_STREAM, nir_intrinsic_stream_id(intr));
      ctx->mod.instructions.push_back(CInstruction(
         intr->intrinsic == nir_intrinsic_emit_vertex ?
            D3D11_SB_OPCODE_EMIT_STREAM : D3D11_SB_OPCODE_CUT_STREAM, src));
      return true;
   }

   case nir_intrinsic_discard_if:
   case nir_intrinsic_discard: {
      COperandBase src = intr->intrinsic == nir_intrinsic_discard_if ?
         nir_src_as_const_value_or_register(ctx, intr->src[0], 1, nullptr) :
         COperand(UINT_MAX);
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_DISCARD, src));
      return true;
   }

   default:
      NIR_INSTR_UNSUPPORTED(&intr->instr);
      assert(!"Unimplemented intrinsic instruction");
      return false;
   }
}

static bool
emit_load_const(struct ntd_context *ctx, nir_load_const_instr *instr)
{
   COperand val;
   if (instr->def.num_components == 1)
      val.m_NumComponents = D3D10_SB_OPERAND_1_COMPONENT;
   else
      val.m_NumComponents = D3D10_SB_OPERAND_4_COMPONENT;

   if (instr->def.bit_size == 64) {
      val.m_Type = D3D10_SB_OPERAND_TYPE_IMMEDIATE64;
      assert(instr->def.num_components <= 2);
      memcpy(&val.m_Value64, &instr->value->i64, 8 * instr->def.num_components);
   } else if (instr->def.bit_size == 1) {
      val.m_Type = D3D10_SB_OPERAND_TYPE_IMMEDIATE32;
      assert(instr->def.num_components <= 4);
      for (unsigned i = 0; i < instr->def.num_components; ++i)
         val.m_Value[i] = instr->value[i].b ? UINT_MAX : 0;
   } else {
      val.m_Type = D3D10_SB_OPERAND_TYPE_IMMEDIATE32;
      assert(instr->def.bit_size == 32);
      assert(instr->def.num_components <= 4);
      for (unsigned i = 0; i < instr->def.num_components; ++i)
         memcpy(&val.m_Value[i], &instr->value[i].u32, 4);
   }

   auto& ntd_ssa = ctx->mod.ssa_operands[instr->def.index];
   ntd_ssa.operand = val;
   return true;
}

static bool
emit_jump(struct ntd_context *ctx, nir_jump_instr *instr)
{
   switch (instr->type) {
   case nir_jump_break:
      assert(instr->instr.block->successors[0]);
      assert(!instr->instr.block->successors[1]);
      ctx->mod.instructions.push_back(
          CInstruction(D3D10_SB_OPCODE_BREAK));
      return true;

   case nir_jump_continue:
      assert(instr->instr.block->successors[0]);
      assert(!instr->instr.block->successors[1]);
      ctx->mod.instructions.push_back(
          CInstruction(D3D10_SB_OPCODE_CONTINUE));
      return true;

   default:
      unreachable("Unsupported jump type\n");
   }
}

static bool
emit_deref(struct ntd_context* ctx, nir_deref_instr* instr)
{
   assert(instr->deref_type == nir_deref_type_var ||
          instr->deref_type == nir_deref_type_array);

   // In the non-Vulkan environment, there's nothing to emit.
   if (!ctx->opts->vulkan_environment)
      return true;

   nir_variable *var = nir_deref_instr_get_variable(instr);
   assert(var);

   if (!glsl_type_is_sampler(glsl_without_array(var->type)) &&
       !glsl_type_is_image(glsl_without_array(var->type)) &&
       !glsl_type_is_texture(glsl_without_array(var->type)))
      return true;

   const struct glsl_type *type = instr->type;
   COperandBase src;

   if (instr->deref_type == nir_deref_type_var) {
      D3D10_SB_OPERAND_TYPE op_type = glsl_type_is_sampler(glsl_without_array(var->type)) ?
         D3D10_SB_OPERAND_TYPE_SAMPLER : glsl_type_is_image(glsl_without_array(var->type)) ?
         D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW : D3D10_SB_OPERAND_TYPE_RESOURCE;
      src = COperand2D(op_type, var->data.driver_location, var->data.binding);
   } else {
      COperandBase previous = nir_src_as_const_value_or_register(ctx, instr->parent, 4, nullptr);
      src = previous;

      if (previous.m_IndexType[1] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
         src_to_index(ctx, previous.RegIndex(1), instr->arr.index, src, 1);
      } else {
         unreachable("Multiple array dimensions unsupported");
      }
   }

   ctx->mod.ssa_operands[instr->dest.ssa.index].operand = src;
   return true;
}

static bool
emit_tex(ntd_context *ctx, nir_tex_instr *instr)
{
   COperandBase tex;
   COperandBase sampler;
   COperandBase bias, lod = COperand(0u), min_lod;
   COperandBase coord, offset, dx, dy;
   COperandBase cmp, sample_idx;

   bool is_zero_lod = false;
   bool lod_needs_to_be_merged_to_coord_alpha = false;
   bool has_offset = false;
   bool has_lod_clamp = false;

   if (!ctx->opts->vulkan_environment) {
      if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
         nir_binding tex_binding = { true, nullptr, 0, instr->texture_index };
         nir_binding sampler_binding = { true, nullptr, 0, instr->sampler_index };
         nir_variable *tex_var = nir_get_binding_variable(ctx->shader, tex_binding);
         nir_variable *sampler_var = nir_get_binding_variable(ctx->shader, sampler_binding);
         if (tex_var)
            tex = COperand2D(D3D10_SB_OPERAND_TYPE_RESOURCE, tex_var->data.driver_location, instr->texture_index);
         if (sampler_var)
            sampler = COperand2D(D3D10_SB_OPERAND_TYPE_SAMPLER, sampler_var->data.driver_location, instr->sampler_index);
      } else {
         tex = COperand(D3D10_SB_OPERAND_TYPE_RESOURCE, instr->texture_index);
         sampler = COperand(D3D10_SB_OPERAND_TYPE_SAMPLER, instr->sampler_index);
      }
   }

   for (unsigned i = 0; i < instr->num_srcs; i++) {
      nir_alu_type type = nir_tex_instr_src_type(instr, i);

      switch (instr->src[i].src_type) {
      case nir_tex_src_coord:
         coord = nir_src_as_const_value_or_register(ctx, instr->src[i].src, instr->coord_components, nullptr);
         break;

      case nir_tex_src_offset:
         has_offset = nir_src_is_const(instr->src[i].src);
         offset = nir_src_as_const_value_or_register(ctx, instr->src[i].src, instr->coord_components, nullptr);
         break;

      case nir_tex_src_bias:
         assert(instr->op == nir_texop_txb);
         assert(nir_src_num_components(instr->src[i].src) == 1);
         bias = nir_src_as_const_value_or_register(ctx, instr->src[i].src, 1, nullptr);
         break;

      case nir_tex_src_lod:
         assert(nir_src_num_components(instr->src[i].src) == 1);
         lod = nir_src_as_const_value_or_register(ctx, instr->src[i].src, 1, nullptr);
         lod_needs_to_be_merged_to_coord_alpha = instr->op == nir_texop_txf;
         is_zero_lod = nir_src_is_const(instr->src[i].src) && nir_src_as_uint(instr->src[i].src) == 0;
         break;

      case nir_tex_src_min_lod:
         assert(nir_src_num_components(instr->src[i].src) == 1);
         min_lod = nir_src_as_const_value_or_register(ctx, instr->src[i].src, 1, nullptr);
         break;

      case nir_tex_src_comparator:
         assert(instr->op == nir_texop_tex && instr->is_shadow);
         assert(nir_src_num_components(instr->src[i].src) == 1);
         cmp = nir_src_as_const_value_or_register(ctx, instr->src[i].src, 1, nullptr);
         break;

      case nir_tex_src_ddx:
         dx = nir_src_as_const_value_or_register(ctx, instr->src[i].src, instr->coord_components, nullptr);
         break;

      case nir_tex_src_ddy:
         dy = nir_src_as_const_value_or_register(ctx, instr->src[i].src, instr->coord_components, nullptr);
         break;

      case nir_tex_src_ms_index:
         assert(instr->op == nir_texop_txf_ms);
         assert(nir_src_num_components(instr->src[i].src) == 1);
         sample_idx = nir_src_as_const_value_or_register(ctx, instr->src[i].src, 1, nullptr);
         break;

      case nir_tex_src_texture_deref:
         assert(ctx->opts->vulkan_environment);
         tex = nir_src_as_const_value_or_register(ctx, instr->src[i].src, 4, nullptr);
         break;

      case nir_tex_src_sampler_deref:
         assert(ctx->opts->vulkan_environment);
         sampler = nir_src_as_const_value_or_register(ctx, instr->src[i].src, 4, nullptr);
         break;

      case nir_tex_src_projector:
         unreachable("Texture projector should have been lowered");

      default:
         fprintf(stderr, "texture source: %d\n", instr->src[i].src_type);
         unreachable("unknown texture source");
      }
   }

   tex.m_NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
   tex.SetSwizzle(0, 1, 2, 3);
   sampler.m_NumComponents = D3D10_SB_OPERAND_0_COMPONENT;

   if (lod_needs_to_be_merged_to_coord_alpha) {
      COperandDst temp_coords(D3D10_SB_OPERAND_TYPE_TEMP, ctx->mod.reg_alloc++);
      temp_coords.SetMask(0b111 << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT);
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_MOV, temp_coords, coord));
      temp_coords.SetMask(D3D10_SB_OPERAND_4_COMPONENT_MASK_W);
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_MOV, temp_coords, lod));
      coord = COperand4(D3D10_SB_OPERAND_TYPE_TEMP, temp_coords.RegIndex(0));
   }

   COperandBase dest = nir_dest_as_register(ctx, instr->dest, 0b1111);
   CInstruction inst;
   switch (instr->op) {
   case nir_texop_txb:
      inst = CInstruction(D3D10_SB_OPCODE_SAMPLE_B, dest, coord, tex, sampler, bias);
      break;

   case nir_texop_tex:
      if (instr->is_shadow) {
         D3D10_SB_OPCODE_TYPE opcode = ctx->mod.shader_kind != DXIL_PIXEL_SHADER || is_zero_lod ?
            D3D10_SB_OPCODE_SAMPLE_C_LZ : D3D10_SB_OPCODE_SAMPLE_C;
         inst = CInstruction(opcode, dest, coord, tex, sampler, cmp);
         break;
      }
      else if (ctx->mod.shader_kind == DXIL_PIXEL_SHADER) {
         inst = CInstruction(D3D10_SB_OPCODE_SAMPLE, dest, coord, tex, sampler);
         break;
      }
      lod = COperand(0u);
      FALLTHROUGH;
   case nir_texop_txl:
      inst = CInstruction(D3D10_SB_OPCODE_SAMPLE_L, dest, coord, tex, sampler, lod);
      break;

   case nir_texop_txd:
      inst = CInstruction(D3D10_SB_OPCODE_SAMPLE_D, dest, coord, tex, sampler, dx, dy);
      break;

   case nir_texop_txf:
      inst = CInstruction(D3D10_SB_OPCODE_LD, dest, coord, tex);
      break;

   case nir_texop_txf_ms:
      inst = CInstruction(D3D10_SB_OPCODE_LD_MS, dest, coord, tex, sample_idx);
      break;

   case nir_texop_txs:
      if (instr->sampler_dim == GLSL_SAMPLER_DIM_BUF)
         inst = CInstruction(D3D11_SB_OPCODE_BUFINFO, dest, tex);
      else {
         inst = CInstruction(D3D10_SB_OPCODE_RESINFO, dest, lod, tex);
         inst.m_ResInfoReturnType = D3D10_SB_RESINFO_INSTRUCTION_RETURN_UINT;
      }
      break;

   case nir_texop_lod:
      inst = CInstruction(D3D10_1_SB_OPCODE_LOD, dest, coord, tex, sampler);
      break;

   case nir_texop_query_levels:
      tex.SetSwizzle(3, 3, 3, 3);
      lod = COperand(0u);
      inst = CInstruction(D3D10_SB_OPCODE_RESINFO, dest, lod, tex);
      break;

   default:
      fprintf(stderr, "texture op: %d\n", instr->op);
      unreachable("unknown texture op");
   }

   if (has_offset) {
      int *offsets = reinterpret_cast<int*>(offset.m_Value);
      inst.SetTexelOffset(offsets[0], offsets[1], offsets[2]);
   }

   if (has_lod_clamp) {
      bool modified = true;
      switch (inst.OpCode()) {
      case D3D10_SB_OPCODE_SAMPLE:
         inst.m_OpCode = D3DWDDM1_3_SB_OPCODE_SAMPLE_CLAMP_FEEDBACK;
         break;
      case D3D10_SB_OPCODE_SAMPLE_B:
         inst.m_OpCode = D3DWDDM1_3_SB_OPCODE_SAMPLE_B_CLAMP_FEEDBACK;
         break;
      case D3D10_SB_OPCODE_SAMPLE_C:
         inst.m_OpCode = D3DWDDM1_3_SB_OPCODE_SAMPLE_C_CLAMP_FEEDBACK;
         break;
      case D3D10_SB_OPCODE_SAMPLE_D:
         inst.m_OpCode = D3DWDDM1_3_SB_OPCODE_SAMPLE_D_CLAMP_FEEDBACK;
         break;
      default:
         modified = false;
         break;
      }
      if (modified) {
         inst.SetNumOperands(inst.NumOperands() + 2);
         inst.m_Operands[inst.NumOperands() - 1] = COperand(D3D10_SB_OPERAND_TYPE_NULL);
         inst.m_Operands[inst.NumOperands() - 2] = min_lod;
      }
   }

   store_instruction(ctx, instr->dest, inst);

   return true;
}

static bool
emit_instr(struct ntd_context *ctx, struct nir_instr *instr)
{
   switch (instr->type) {
   case nir_instr_type_alu:
      return emit_alu(ctx, nir_instr_as_alu(instr));
   case nir_instr_type_intrinsic:
      return emit_intrinsic(ctx, nir_instr_as_intrinsic(instr));
   case nir_instr_type_load_const:
      return emit_load_const(ctx, nir_instr_as_load_const(instr));
    case nir_instr_type_deref:
       return emit_deref(ctx, nir_instr_as_deref(instr));
   case nir_instr_type_jump:
      return emit_jump(ctx, nir_instr_as_jump(instr));
    case nir_instr_type_tex:
       return emit_tex(ctx, nir_instr_as_tex(instr));
   // case nir_instr_type_ssa_undef:
   //    return emit_undefined(ctx, nir_instr_as_ssa_undef(instr));
   default:
      NIR_INSTR_UNSUPPORTED(instr);
      unreachable("Unimplemented instruction type");
      return false;
   }
}

static bool
emit_block(struct ntd_context *ctx, struct nir_block *block)
{
   nir_foreach_instr(instr, block)
   {
      TRACE_CONVERSION(instr);

      if (!emit_instr(ctx, instr)) {
         return false;
      }
   }

   return true;
}

static bool
emit_cf_list(struct ntd_context *ctx, struct exec_list *list);


static bool
emit_if(struct ntd_context *ctx, struct nir_if *if_stmt)
{
   COperandBase cond =
       nir_src_as_const_value_or_register(ctx, if_stmt->condition, 4, nullptr);
   ctx->mod.instructions.push_back(CInstruction(
       D3D10_SB_OPCODE_IF, cond,
       D3D10_SB_INSTRUCTION_TEST_NONZERO));

   nir_block *then_block = nir_if_first_then_block(if_stmt);
   assert(nir_if_last_then_block(if_stmt)->successors[0]);
   assert(!nir_if_last_then_block(if_stmt)->successors[1]);
   int then_succ = nir_if_last_then_block(if_stmt)->successors[0]->index;

   nir_block *else_block = NULL;
   int else_succ = -1;
   if (!exec_list_is_empty(&if_stmt->else_list)) {
      else_block = nir_if_first_else_block(if_stmt);
      assert(nir_if_last_else_block(if_stmt)->successors[0]);
      assert(!nir_if_last_else_block(if_stmt)->successors[1]);
      else_succ = nir_if_last_else_block(if_stmt)->successors[0]->index;
   }

   if (!emit_cf_list(ctx, &if_stmt->then_list))
      return false;

   if (else_block) {
      ctx->mod.instructions.push_back(CInstruction(D3D10_SB_OPCODE_ELSE));
      if (!emit_cf_list(ctx, &if_stmt->else_list))
         return false;
   }

   ctx->mod.instructions.push_back(
       CInstruction(D3D10_SB_OPCODE_ENDIF));
   return true;
}

static bool
emit_loop(struct ntd_context *ctx, nir_loop *loop)
{
   nir_block *first_block = nir_loop_first_block(loop);

   assert(nir_loop_last_block(loop)->successors[0]);
   assert(!nir_loop_last_block(loop)->successors[1]);

   ctx->mod.instructions.push_back(
       CInstruction(D3D10_SB_OPCODE_LOOP));

   if (!emit_cf_list(ctx, &loop->body))
      return false;

   ctx->mod.instructions.push_back(
       CInstruction(D3D10_SB_OPCODE_ENDLOOP));

   return true;
}

static bool
emit_cf_list(struct ntd_context *ctx, struct exec_list *list)
{
   foreach_list_typed(nir_cf_node, node, node, list)
   {
      switch (node->type) {
      case nir_cf_node_block:
         if (!emit_block(ctx, nir_cf_node_as_block(node)))
            return false;
         break;

      case nir_cf_node_if:
         if (!emit_if(ctx, nir_cf_node_as_if(node)))
            return false;
         break;

      case nir_cf_node_loop:
         if (!emit_loop(ctx, nir_cf_node_as_loop(node)))
            return false;
         break;

      default:
         unreachable("unsupported cf-list node");
         break;
      }
   }
   return true;
}

static unsigned get_dword_size(const struct glsl_type *type)
{
   return glsl_count_attribute_slots(type, false);
}

static D3D10_SB_PRIMITIVE
dxbc_get_input_primitive(unsigned primitive)
{
   switch (primitive) {
   case GL_POINTS:
      return D3D10_SB_PRIMITIVE_POINT;
   case GL_LINES:
      return D3D10_SB_PRIMITIVE_LINE;
   case GL_LINES_ADJACENCY:
      return D3D10_SB_PRIMITIVE_LINE_ADJ;
   case GL_TRIANGLES:
      return D3D10_SB_PRIMITIVE_TRIANGLE;
   case GL_TRIANGLES_ADJACENCY:
      return D3D10_SB_PRIMITIVE_TRIANGLE_ADJ;
   default:
      unreachable("unhandled primitive topology");
   }
}

static D3D10_SB_PRIMITIVE_TOPOLOGY
dxbc_get_primitive_topology(unsigned topology)
{
   switch (topology) {
   case GL_POINTS:
      return D3D10_SB_PRIMITIVE_TOPOLOGY_POINTLIST;
   case GL_LINES:
      return D3D10_SB_PRIMITIVE_TOPOLOGY_LINELIST;
   case GL_LINE_STRIP:
      return D3D10_SB_PRIMITIVE_TOPOLOGY_LINESTRIP;
   case GL_TRIANGLE_STRIP:
      return D3D10_SB_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
   default:
      unreachable("unhandled primitive topology");
   }
}

static D3D10_SB_INTERPOLATION_MODE
dxil_to_dxbc_interpolation_mode(dxil_interpolation_mode mode)
{
   switch (mode) {
   case DXIL_INTERP_CONSTANT: return D3D10_SB_INTERPOLATION_CONSTANT;
   case DXIL_INTERP_LINEAR: return D3D10_SB_INTERPOLATION_LINEAR;
   case DXIL_INTERP_LINEAR_CENTROID: return D3D10_SB_INTERPOLATION_LINEAR_CENTROID;
   case DXIL_INTERP_LINEAR_NOPERSPECTIVE: return D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE;
   case DXIL_INTERP_LINEAR_NOPERSPECTIVE_CENTROID: return D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID;
   case DXIL_INTERP_LINEAR_NOPERSPECTIVE_SAMPLE: return D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE;
   case DXIL_INTERP_LINEAR_SAMPLE: return D3D10_SB_INTERPOLATION_LINEAR_SAMPLE;
   default: return D3D10_SB_INTERPOLATION_UNDEFINED;
   }
}

static void
count_resources(struct ntd_context *ctx)
{
   if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
      unsigned num_ubos = 0;
      nir_foreach_variable_with_modes(ubo, ctx->shader, nir_var_mem_ubo) {
         ubo->data.driver_location = num_ubos++;
      }

      unsigned num_samplers = 0;
      nir_foreach_variable_with_modes(var, ctx->shader, nir_var_uniform) {
         unsigned count = glsl_type_get_sampler_count(var->type);
         assert(count == 0 || glsl_type_is_bare_sampler(glsl_without_array(var->type)));
         if (count > 0)
            var->data.driver_location = num_samplers++;
      }

      unsigned num_textures = 0;
      nir_foreach_variable_with_modes(var, ctx->shader, nir_var_uniform) {
         unsigned count = glsl_type_get_texture_count(var->type);
         assert(count == 0 || glsl_type_is_texture(glsl_without_array(var->type)));
         if (count > 0)
            var->data.driver_location = num_textures++;
      }

      unsigned num_uavs = 0;
      nir_foreach_variable_with_modes(var, ctx->shader, nir_var_mem_ssbo) {
         if ((var->data.access & ACCESS_NON_WRITEABLE) != 0)
            var->data.driver_location = num_textures++;
         else
            var->data.driver_location = num_uavs++;
      }

      nir_foreach_image_variable(var, ctx->shader) {
         var->data.driver_location = num_uavs++;
      }
   }

   nir_function_impl *entrypoint = nir_shader_get_entrypoint(ctx->shader);
   ctx->mod.ssa_operands.reset(new ntd_ssa[entrypoint->ssa_alloc]);
}

static D3D10_SB_RESOURCE_DIMENSION
glsl_sampler_dim_to_dxbc(glsl_sampler_dim dim, bool is_array)
{
   switch (dim) {
   case GLSL_SAMPLER_DIM_1D:
      return is_array ?
         D3D10_SB_RESOURCE_DIMENSION_TEXTURE1DARRAY :
         D3D10_SB_RESOURCE_DIMENSION_TEXTURE1D;
   case GLSL_SAMPLER_DIM_2D:
   case GLSL_SAMPLER_DIM_EXTERNAL:
      return is_array ?
         D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DARRAY :
         D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
   case GLSL_SAMPLER_DIM_3D:
      return D3D10_SB_RESOURCE_DIMENSION_TEXTURE3D;
   case GLSL_SAMPLER_DIM_CUBE:
      return is_array ?
         D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBEARRAY :
         D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBE;
   case GLSL_SAMPLER_DIM_BUF:
      return D3D10_SB_RESOURCE_DIMENSION_BUFFER;
   case GLSL_SAMPLER_DIM_MS:
      return is_array ?
         D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMSARRAY :
         D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMS;
   default:
      assert(false);
      return D3D10_SB_RESOURCE_DIMENSION_UNKNOWN;
   }
}

static D3D10_SB_RESOURCE_RETURN_TYPE
glsl_base_type_to_dxbc_ret_type(glsl_base_type type)
{
   switch (type) {
   case GLSL_TYPE_UINT: return D3D10_SB_RETURN_TYPE_UINT;
   case GLSL_TYPE_INT: return D3D10_SB_RETURN_TYPE_SINT;
   default:
   case GLSL_TYPE_FLOAT: return D3D10_SB_RETURN_TYPE_FLOAT;
   }
}

static bool
emit_dcl(struct ntd_context *ctx)
{
   // TODO pare the write_masks down to just what is used in the shader?

   // TODO?
   ctx->mod.shader.EmitGlobalFlagsDecl(
       D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED);

   nir_foreach_variable_with_modes(ubo, ctx->shader, nir_var_mem_ubo) {
      if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
         unsigned upper_bound = ctx->opts->vulkan_environment ?
            (ubo->data.binding + (glsl_type_is_array(ubo->type) ? glsl_get_aoa_size(ubo->type) - 1 : 0)) :
            ubo->data.binding;
         unsigned size = get_dword_size(ctx->opts->vulkan_environment ?
            glsl_without_array(ubo->type) : ubo->type);

         ctx->mod.shader.EmitIndexableConstantBufferDecl(ubo->data.driver_location,
            ubo->data.binding, upper_bound, size,
            D3D10_SB_CONSTANT_BUFFER_DYNAMIC_INDEXED, ubo->data.descriptor_set);
      } else {
         ctx->mod.shader.EmitConstantBufferDecl(ubo->data.binding, get_dword_size(ubo->type), D3D10_SB_CONSTANT_BUFFER_DYNAMIC_INDEXED);
      }
   }

   nir_foreach_variable_with_modes(var, ctx->shader, nir_var_uniform) {
      unsigned count = glsl_type_get_sampler_count(var->type);
      assert(count == 0 || glsl_type_is_bare_sampler(glsl_without_array(var->type)));
      if (count > 0) {
         D3D10_SB_SAMPLER_MODE mode = glsl_sampler_type_is_shadow(glsl_without_array(var->type)) ?
            D3D10_SB_SAMPLER_MODE_COMPARISON : D3D10_SB_SAMPLER_MODE_DEFAULT;
         if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
            ctx->mod.shader.EmitIndexableSamplerDecl(var->data.driver_location,
               var->data.binding, var->data.binding + count - 1, mode, var->data.descriptor_set);
         } else {
            for (unsigned i = 0; i < count; ++i) {
               ctx->mod.shader.EmitSamplerDecl(var->data.binding + i, mode);
            }
         }
      }
   }

   nir_foreach_variable_with_modes(var, ctx->shader, nir_var_uniform) {
      unsigned count = glsl_type_get_texture_count(var->type);
      assert(count == 0 || glsl_type_is_texture(glsl_without_array(var->type)));
      if (count > 0) {
         const glsl_type *type = glsl_without_array(var->type);
         D3D10_SB_RESOURCE_DIMENSION dim = glsl_sampler_dim_to_dxbc(glsl_get_sampler_dim(type), glsl_sampler_type_is_array(type));
         D3D10_SB_RESOURCE_RETURN_TYPE ret_type = glsl_base_type_to_dxbc_ret_type(glsl_get_sampler_result_type(type));
         if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
            ctx->mod.shader.EmitIndexableResourceDecl(var->data.driver_location,
               var->data.binding, var->data.binding + count - 1,
               dim, ret_type, ret_type, ret_type, ret_type,
               var->data.descriptor_set);
         } else {
            for (unsigned i = 0; i < count; ++i) {
               ctx->mod.shader.EmitResourceDecl(dim, var->data.binding + i,
                  ret_type, ret_type, ret_type, ret_type);
            }
         }
      }
   }

   nir_foreach_variable_with_modes(var, ctx->shader, nir_var_mem_ssbo) {
      if ((var->data.access & ACCESS_NON_WRITEABLE) != 0) {
         unsigned count = 1;
         if (glsl_type_is_array(var->type))
            count = glsl_get_length(var->type);
         if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
            ctx->mod.shader.EmitIndexableRawShaderResourceViewDecl(var->data.driver_location,
               var->data.binding, var->data.binding + count - 1, var->data.descriptor_set);
         } else {
            for (unsigned i = 0; i < count; ++i) {
               ctx->mod.shader.EmitRawShaderResourceViewDecl(var->data.binding + i);
            }
         }
      }
   }

   nir_foreach_variable_with_modes(var, ctx->shader, nir_var_mem_ssbo) {
      if ((var->data.access & ACCESS_NON_WRITEABLE) == 0) {
         unsigned count = 1;
         if (glsl_type_is_array(var->type))
            count = glsl_get_length(var->type);
         if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
            ctx->mod.shader.EmitIndexableRawUnorderedAccessViewDecl(var->data.driver_location,
               var->data.binding, var->data.binding + count - 1, 0, var->data.descriptor_set);
         } else {
            for (unsigned i = 0; i < count; ++i) {
               ctx->mod.shader.EmitRawUnorderedAccessViewDecl(var->data.binding + i, 0);
            }
         }
      }
   }

   nir_foreach_image_variable(var, ctx->shader) {
      unsigned count = glsl_type_get_image_count(var->type);
      const glsl_type *type = glsl_without_array(var->type);
      D3D10_SB_RESOURCE_DIMENSION dim = glsl_sampler_dim_to_dxbc(glsl_get_sampler_dim(type), glsl_sampler_type_is_array(type));
      D3D10_SB_RESOURCE_RETURN_TYPE ret_type = glsl_base_type_to_dxbc_ret_type(glsl_get_sampler_result_type(type));
      if (ctx->mod.major_version == 5 && ctx->mod.minor_version == 1) {
         ctx->mod.shader.EmitIndexableTypedUnorderedAccessViewDecl(var->data.driver_location,
            var->data.binding, var->data.binding + count - 1,
            dim, ret_type, ret_type, ret_type, ret_type,
            0, var->data.descriptor_set);
      } else {
         for (unsigned i = 0; i < count; ++i) {
            ctx->mod.shader.EmitTypedUnorderedAccessViewDecl(dim, var->data.binding + i,
               ret_type, ret_type, ret_type, ret_type, 0);
         }
      }
   }

   for (int i = 0; i < ctx->dxil_mod.num_sig_inputs; i++) {
      struct dxil_signature_record &input = ctx->dxil_mod.inputs[i];
      struct dxil_psv_signature_element &psv_input = ctx->dxil_mod.psv_inputs[i];
      for (int e = 0; e < input.num_elements; e++) {
         struct dxil_signature_element &elem = input.elements[e];
         UINT write_mask = elem.mask
                           << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT;

         // elem.system_value is `enum dxil_prog_sig_semantic`
         // https://gitlab.freedesktop.org/mesa/mesa/blob/4a3395f35aeeb90f4613922dfe761dae62572f4b/src/microsoft/compiler/dxil_signature.c#L405
         enum dxil_prog_sig_semantic sem =
            static_cast<enum dxil_prog_sig_semantic>(elem.system_value);
         switch (sem) {
         case DXIL_PROG_SEM_UNDEFINED:
         case DXIL_PROG_SEM_TARGET:
            // emit as normal
            if (ctx->mod.shader_kind == D3D10_SB_PIXEL_SHADER) {
               ctx->mod.shader.EmitPSInputDecl(
                  elem.reg, write_mask,
                  dxil_to_dxbc_interpolation_mode((dxil_interpolation_mode)psv_input.interpolation_mode)
               );
            } else {
               ctx->mod.shader.EmitInputDecl(D3D10_SB_OPERAND_TYPE_INPUT,
                  elem.reg, write_mask);
            }
            break;

         case DXIL_PROG_SEM_POSITION:
            if (ctx->mod.shader_kind == D3D10_SB_PIXEL_SHADER) {
               ctx->mod.shader.EmitPSInputSystemInterpretedValueDecl(
                  elem.reg, write_mask,
                  D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE,
                  D3D10_SB_NAME_POSITION);
            } else {
               ctx->mod.shader.EmitInputSystemInterpretedValueDecl(D3D10_SB_OPERAND_TYPE_INPUT,
                  elem.reg, write_mask, D3D10_SB_NAME_POSITION);
            }
            break;

         case DXIL_PROG_SEM_PRIMITIVE_ID:
            ctx->mod.shader.EmitInputPrimIdDecl();
            break;

         case DXIL_PROG_SEM_VERTEX_ID:
         case DXIL_PROG_SEM_INSTANCE_ID:
         case DXIL_PROG_SEM_IS_FRONTFACE:
         case DXIL_PROG_SEM_SAMPLE_INDEX:
            ctx->mod.shader.EmitInputSystemGeneratedValueDecl(
               elem.reg, write_mask, static_cast<D3D10_SB_NAME>(sem));
            break;

         default:
            unreachable("unhandled dxil_prog_sig_semantic");
            return false;
         }
      }
   }

   uint32_t num_temps = ctx->mod.reg_alloc;
   if (num_temps > 0) {
      ctx->mod.shader.EmitTempsDecl(num_temps);
   }

   if (ctx->mod.shader_kind == D3D10_SB_GEOMETRY_SHADER) {
      ctx->mod.shader.EmitGSInputPrimitiveDecl(dxbc_get_input_primitive(ctx->shader->info.gs.input_primitive));

      for (unsigned i = 0; i < 4; ++i) {
         if (ctx->shader->info.gs.active_stream_mask & (1 << i)) {
            ctx->mod.shader.EmitStreamDecl(i);
         }
      }

      ctx->mod.shader.EmitGSOutputTopologyDecl(dxbc_get_primitive_topology(ctx->shader->info.gs.output_primitive));
   }

   for (int i = 0; i < ctx->dxil_mod.num_sig_outputs; i++) {
      struct dxil_signature_record &output = ctx->dxil_mod.outputs[i];
      for (int e = 0; e < output.num_elements; e++) {
         struct dxil_signature_element &elem = output.elements[e];
         UINT write_mask = elem.mask
                           << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT;

         enum dxil_prog_sig_semantic sem =
             static_cast<enum dxil_prog_sig_semantic>(elem.system_value);
         switch (sem) {
         case DXIL_PROG_SEM_UNDEFINED:
         case DXIL_PROG_SEM_TARGET:
            // emit as normal
            ctx->mod.shader.EmitOutputDecl(elem.reg, write_mask);
            break;

         case DXIL_PROG_SEM_POSITION:
            ctx->mod.shader.EmitOutputSystemInterpretedValueDecl(
                elem.reg, write_mask, D3D10_SB_NAME_POSITION);
            break;

         case DXIL_PROG_SEM_DEPTH:
            ctx->mod.shader.EmitODepthDecl();
            break;

         case DXIL_PROG_SEM_DEPTH_GE:
            ctx->mod.shader.EmitODepthDeclGE();
            break;

         case DXIL_PROG_SEM_DEPTH_LE:
            ctx->mod.shader.EmitODepthDeclLE();
            break;

         case DXIL_PROG_SEM_STENCIL_REF:
            ctx->mod.shader.EmitOStencilRefDecl();
            break;

         default:
            unreachable("unhandled dxil_prog_sig_semantic");
            return false;
         }
      }
   }

   if (ctx->mod.shader_kind == D3D10_SB_GEOMETRY_SHADER) {
      ctx->mod.shader.EmitGSMaxOutputVertexCountDecl(ctx->shader->info.gs.vertices_out);
   }

   if (ctx->mod.shader_kind == D3D11_SB_COMPUTE_SHADER) {
      const unsigned mask_3comp = 0b111 << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT;
      if (BITSET_TEST(ctx->shader->info.system_values_read, SYSTEM_VALUE_GLOBAL_INVOCATION_ID))
         ctx->mod.shader.EmitInputThreadIDDecl(mask_3comp);
      if (BITSET_TEST(ctx->shader->info.system_values_read, SYSTEM_VALUE_WORKGROUP_ID))
         ctx->mod.shader.EmitInputThreadGroupIDDecl(mask_3comp);
      if (BITSET_TEST(ctx->shader->info.system_values_read, SYSTEM_VALUE_LOCAL_INVOCATION_ID))
         ctx->mod.shader.EmitInputThreadIDInGroupDecl(mask_3comp);
      if (BITSET_TEST(ctx->shader->info.system_values_read, SYSTEM_VALUE_LOCAL_INVOCATION_INDEX))
         ctx->mod.shader.EmitInputThreadIDInGroupFlattenedDecl();

      if (ctx->shader->info.shared_size)
         ctx->mod.shader.EmitRawThreadGroupSharedMemoryDecl(0, ctx->shader->info.shared_size);

      ctx->mod.shader.EmitThreadGroupDecl(
         ctx->shader->info.workgroup_size[0],
         ctx->shader->info.workgroup_size[1],
         ctx->shader->info.workgroup_size[2]);
   }

   return true;
}

static bool
emit_module(struct ntd_context *ctx)
{
   nir_function_impl *entry = nir_shader_get_entrypoint(ctx->shader);
   nir_metadata_require(entry, nir_metadata_block_index);

   count_resources(ctx);

   if (!emit_cf_list(ctx, &entry->body)) {
      return false;
   }

   if (!emit_dcl(ctx)) {
      return false;
   }

   for (auto& instr : ctx->mod.instructions)
      ctx->mod.shader.EmitInstruction(instr);

   ctx->mod.shader.EmitInstruction(
       CInstruction(D3D10_SB_OPCODE_RET));

   return true;
}

static D3D10_SB_TOKENIZED_PROGRAM_TYPE
get_dxbc_shader_kind(struct nir_shader *s)
{
   switch (s->info.stage) {
   case MESA_SHADER_VERTEX:
      return D3D10_SB_VERTEX_SHADER;
   case MESA_SHADER_GEOMETRY:
      return D3D10_SB_GEOMETRY_SHADER;
   case MESA_SHADER_FRAGMENT:
      return D3D10_SB_PIXEL_SHADER;
   case MESA_SHADER_KERNEL:
   case MESA_SHADER_COMPUTE:
      return D3D11_SB_COMPUTE_SHADER;
   default:
      unreachable("unknown shader stage in nir_to_dxbc");
      return D3D11_SB_COMPUTE_SHADER;
   }
}

static int
get_glsl_type_size(const struct glsl_type *type, bool is_bindless)
{
   return glsl_count_attribute_slots(type, false);
}

bool
nir_to_dxbc(struct nir_shader *s, const struct nir_to_dxil_options *opts,
            struct blob *blob)
{
   assert(opts);
   blob_init(blob);
   debug_dxbc = (int)debug_get_option_debug_dxbc();

   if (opts->shader_model_max < SHADER_MODEL_5_0) {
      debug_printf("D3D12: invalid shader model requested");
      return false;
   }

   ntd_context ctx;
   ctx.shader = s;
   ctx.opts = opts;
   ctx.mod.shader_kind = get_dxbc_shader_kind(s);
   ctx.mod.major_version = 5;
   ctx.mod.minor_version = opts->shader_model_max >= SHADER_MODEL_5_1 ? 1 : 0;

   NIR_PASS_V(s, nir_lower_pack);
   NIR_PASS_V(s, nir_lower_frexp);
   NIR_PASS_V(s, nir_lower_flrp, 16 | 32 | 64, true);
   nir_lower_idiv_options idiv_opts = { .keep_unsigned = true };
   NIR_PASS_V(s, nir_lower_idiv, &idiv_opts);
   NIR_PASS_V(s, dxil_nir_lower_loads_stores_to_dxil, nir_var_all & ~nir_var_mem_shared);

   // NOTE: do not run scalarization passes
   optimize_nir(s, opts, false);

   NIR_PASS_V(s, nir_remove_dead_variables,
              nir_var_function_temp | nir_var_shader_temp, NULL);

   nir_lower_io_options options{};
   NIR_PASS_V(s, nir_lower_io,
              nir_var_shader_in | nir_var_shader_out | nir_var_uniform,
              get_glsl_type_size, options);

   if (!dxil_allocate_sysvalues(s, ctx.system_value))
      return false;

   dxil_lower_sysval_options sysval_options = {};
   sysval_options.sample_id_is_sysval = true;
   NIR_PASS_V(s, dxil_nir_lower_sysval_to_load_input, ctx.system_value, &sysval_options);
   // NIR_PASS_V(s, nir_lower_locals_to_regs);
   // NIR_PASS_V(s, nir_move_vec_src_uses_to_dest);
   // NIR_PASS_V(s, nir_lower_vec_to_movs, NULL, NULL);
   NIR_PASS_V(s, nir_opt_dce);
   // NIR_PASS_V(s, nir_remove_dead_variables, nir_var_function_temp, NULL);

   // TODO register allocator that can reuse registers?
   NIR_PASS_V(s, nir_convert_from_ssa, true);
   nir_function_impl *entrypoint = nir_shader_get_entrypoint(s);
   nir_index_ssa_defs(entrypoint);
   nir_index_local_regs(entrypoint);

   if (debug_dxbc & DXBC_DEBUG_VERBOSE)
      nir_print_shader(s, stderr);

   get_signatures(&ctx.dxil_mod, s, ctx.opts->vulkan_environment, true /* dxbc */);

   DxbcModule &mod = ctx.mod;
   mod.shader.Init(1024);
   mod.shader.StartShader(mod.shader_kind, mod.major_version,
                          mod.minor_version);
   mod.reg_alloc = entrypoint->reg_alloc;

   emit_module(&ctx);

   mod.shader.EndShader();

   ScopedDxilContainer container;

   // RDEF is only used for reflection?

   if (!dxil_container_add_io_signature(container.get(), DXIL_ISG1,
                                        ctx.dxil_mod.num_sig_inputs,
                                        ctx.dxil_mod.inputs)) {
      debug_printf("D3D12: dxil_container_add_io_signature failed\n");
      return false;
   }

   if (!dxil_container_add_io_signature(container.get(), DXIL_OSG1,
                                        ctx.dxil_mod.num_sig_outputs,
                                        ctx.dxil_mod.outputs)) {
      debug_printf("D3D12: dxil_container_add_io_signature failed\n");
      return false;
   }

   dxil_features empty_feats = {};
   if (memcmp(&ctx.dxil_mod.feats, &empty_feats, sizeof(empty_feats)) != 0 &&
       !dxil_container_add_features(container.get(), &ctx.dxil_mod.feats)) {
      debug_printf("D3D12: dxil_container_add_features failed\n");
      return false;
   }

   if (!dxil_container_add_shader_blob(
           container.get(), static_cast<const void *>(mod.shader.GetShader()),
           static_cast<uint32_t>(mod.shader.ShaderSizeInDWORDs() *
                                 sizeof(UINT)))) {
      debug_printf("D3D12: dxil_container_add_shader_blob failed\n");
      return false;
   }

   // STAT is only used for reflection?

   if (!dxil_container_write(container.get(), blob)) {
      debug_printf("D3D12: dxil_container_write failed\n");
      return false;
   }

#ifdef _WIN32
   if (debug_dxbc & DXBC_DEBUG_DISASSEMBLE) {
      HMODULE signer = LoadLibraryA("dxbcSigner.dll");
      auto sign = reinterpret_cast<HRESULT(APIENTRY *)(BYTE *, UINT32)>(GetProcAddress(signer, "SignDxbc"));
      
      if (SUCCEEDED(sign(blob->data, blob->size))) {
         HMODULE d3dcompiler = LoadLibraryA("d3dcompiler_47.dll");
         auto disassemble = reinterpret_cast<HRESULT(APIENTRY *)(LPCVOID, SIZE_T, UINT, LPCSTR, IDxcBlob **)>(
            GetProcAddress(d3dcompiler, "D3DDisassemble"));
         assert(d3dcompiler && disassemble);
         IDxcBlob *result = nullptr;
         if (SUCCEEDED(disassemble(blob->data, blob->size, 0, nullptr, &result))) {
            fprintf(stderr, "%s\n", (char *)result->GetBufferPointer());
            result->Release();
         } else
            debug_printf("D3D12: failed to disassemble shader");
         FreeLibrary(d3dcompiler);
      } else
         debug_printf("D3D12: failed to sign shader for disassembly");

      FreeLibrary(signer);
   }
#endif

   return true;
}
