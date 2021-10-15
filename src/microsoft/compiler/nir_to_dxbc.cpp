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

#include "d3d12TokenizedProgramFormat.hpp"
#include "ShaderBinary.h"

#include <stdint.h>
#include <vector>

#define NIR_INSTR_UNSUPPORTED(instr) \
   if (true) \
   do { \
      fprintf(stderr, "Unsupported instruction:"); \
      nir_print_instr(instr, stderr); \
      fprintf(stderr, "\n"); \
   } while (0)

#define TRACE_CONVERSION(instr) \
   if (true) \
      do { \
         fprintf(stderr, "Convert '"); \
         nir_print_instr(instr, stderr); \
         fprintf(stderr, "'\n"); \
      } while (0)

const nir_shader_compiler_options*
dxbc_get_nir_compiler_options(void)
{
   static  nir_shader_compiler_options nir_options{};
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

   // TODO: fxc turns `mul(float4,float4)` into `dp4`, but we're running a pass somewhere that converts the `fdot` from SPIR-V's `OpDot` to `fmul`/`fadd`.
   return &nir_options;
}

struct DxbcModule {
   struct dxil_signature_record inputs[DXIL_SHADER_MAX_IO_ROWS];
   struct dxil_signature_record outputs[DXIL_SHADER_MAX_IO_ROWS];

   struct dxil_features feats;

   D3D10_SB_TOKENIZED_PROGRAM_TYPE shader_kind;
   uint32_t major_version;
   uint32_t minor_version;
   D3D10ShaderBinary::CShaderAsm shader;
};

struct ntd_context {
   void* ralloc_ctx;
   const struct nir_to_dxil_options* opts{};
   struct nir_shader* shader{};
   DxbcModule mod{};
   dxil_module dxil_mod{};

   ntd_context() { 
      ralloc_ctx = ralloc_context(NULL); 
      if(ralloc_ctx)
         dxil_module_init(&dxil_mod, ralloc_ctx);
   }
   ~ntd_context() { 
      dxil_module_release(&dxil_mod);
      ralloc_free(ralloc_ctx); 
   }
};

class ScopedDxilContainer {
public:
   ScopedDxilContainer() { dxil_container_init(&inner); }
   ~ScopedDxilContainer() { dxil_container_finish(&inner); }
   dxil_container* get() { return &inner; }

private:
   dxil_container inner{};
};

static D3D10ShaderBinary::COperandDst
nir_dest_as_register(nir_dest dest, uint32_t write_mask)
{
  assert(!dest.is_ssa);
   return D3D10ShaderBinary::COperandDst(D3D10_SB_OPERAND_TYPE_TEMP,
                                       dest.reg.reg->index,
                                    write_mask << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT);
}

// After running `nir_convert_from_ssa`, we're out of SSA land, with the exception of literal values. This function converts a `nir_src` to either a temporary register value or a literal, based on the `ssa`-ness of it.
static D3D10ShaderBinary::COperandBase
nir_src_as_const_value_or_register(nir_src src, uint32_t num_write_components, uint8_t swizzle[NIR_MAX_VEC_COMPONENTS])
{
  uint32_t num_components =
      std::min(num_write_components, nir_src_num_components(src));

  if (src.is_ssa) {
    // TODO immediate operand type?
    switch (num_components) {
      case 1:
        return D3D10ShaderBinary::COperand(
            static_cast<float>(nir_src_comp_as_float(src, 0)));

      case 2:
        return D3D10ShaderBinary::COperand(
            static_cast<float>(nir_src_comp_as_float(src, 0)),
            static_cast<float>(nir_src_comp_as_float(src, 1)));

      case 4:
        return D3D10ShaderBinary::COperand(
            static_cast<float>(nir_src_comp_as_float(src, 0)),
            static_cast<float>(nir_src_comp_as_float(src, 1)),
            static_cast<float>(nir_src_comp_as_float(src, 2)),
            static_cast<float>(nir_src_comp_as_float(src, 3)));

      default:
        unreachable("unhandled number of components");
    }
  } else {
    // nir doesn't ever have a direct load from an input to a usage, so in
    // load_input we move the value to a temp, so we can assume all `nir_src`s
    // that point to registers are to temps.
    D3D10_SB_OPERAND_TYPE temp = D3D10_SB_OPERAND_TYPE_TEMP;
    switch (num_components) {
      case 1: {
        if (swizzle) {
          return D3D10ShaderBinary::COperand4(
              temp, src.reg.reg->index,
              static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[0]));
        } else {
          return D3D10ShaderBinary::COperand4(temp, src.reg.reg->index,
                                              D3D10_SB_4_COMPONENT_X);
        }
      }

      case 4:
        if (swizzle) {
          return D3D10ShaderBinary::COperand(
              temp, src.reg.reg->index,
              static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[0]),
              static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[1]),
              static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[2]),
              static_cast<D3D10_SB_4_COMPONENT_NAME>(swizzle[3]));
        } else {
          return D3D10ShaderBinary::COperand4(temp, src.reg.reg->index);
        }

      default:
        unreachable("unhandled number of components");
    }
  }
}

static uint32_t
count_write_components(uint32_t write_mask) {
   uint32_t count = 0;
   for (int i = 0; i < NIR_MAX_VEC_COMPONENTS; i++) {
      if ((write_mask & (1<<i)) != 0) {
         count += 1;
      }
   }
   return count;
}

static D3D10ShaderBinary::CInstruction
get_intr_1_args(D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu) {
   uint32_t num_write_components = count_write_components(alu->dest.write_mask);
   D3D10ShaderBinary::COperandBase dst = nir_dest_as_register(alu->dest.dest, alu->dest.write_mask);
   D3D10ShaderBinary::COperandBase a = nir_src_as_const_value_or_register(alu->src[0].src, num_write_components, alu->src[0].swizzle);
   return D3D10ShaderBinary::CInstruction(opcode, dst, a);
}

static D3D10ShaderBinary::CInstruction
get_intr_2_args(D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu) {
   uint32_t num_write_components = count_write_components(alu->dest.write_mask);
   D3D10ShaderBinary::COperandBase dst = nir_dest_as_register(alu->dest.dest, alu->dest.write_mask);
   D3D10ShaderBinary::COperandBase lhs = nir_src_as_const_value_or_register(alu->src[0].src, num_write_components, alu->src[0].swizzle);
   D3D10ShaderBinary::COperandBase rhs = nir_src_as_const_value_or_register(alu->src[1].src, num_write_components, alu->src[1].swizzle);
   return D3D10ShaderBinary::CInstruction(opcode, dst, lhs, rhs);
}

static D3D10ShaderBinary::CInstruction
get_intr_3_args(D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu) {
   uint32_t num_write_components = count_write_components(alu->dest.write_mask);
   D3D10ShaderBinary::COperandBase dst = nir_dest_as_register(alu->dest.dest, alu->dest.write_mask);
   D3D10ShaderBinary::COperandBase arg0 = nir_src_as_const_value_or_register(alu->src[0].src, num_write_components, alu->src[0].swizzle);
   D3D10ShaderBinary::COperandBase arg1 = nir_src_as_const_value_or_register(alu->src[1].src, num_write_components, alu->src[1].swizzle);
   D3D10ShaderBinary::COperandBase arg2 = nir_src_as_const_value_or_register(alu->src[2].src, num_write_components, alu->src[2].swizzle);
   return D3D10ShaderBinary::CInstruction(opcode, dst, arg0, arg1, arg2);
}

static bool
emit_alu(struct ntd_context *ctx, nir_alu_instr *alu) {
  D3D10ShaderBinary::COperand null_operand(D3D10_SB_OPERAND_TYPE_NULL);

  switch (alu->op) {
    case nir_op_vec2:
    case nir_op_vec3:
    case nir_op_vec4: {
      // TODO: coalesce like-sourced `mov`s together
      for (int chan = 0; chan < alu->dest.dest.reg.reg->num_components;
           chan++) {
        D3D10ShaderBinary::COperand4 dst(
            D3D10_SB_OPERAND_TYPE_TEMP, alu->dest.dest.reg.reg->index,
            static_cast<D3D10_SB_4_COMPONENT_NAME>(chan));
        D3D10ShaderBinary::COperandBase src =
            nir_src_as_const_value_or_register(
                alu->src[chan].src,
                count_write_components(alu->dest.write_mask),
                alu->src[chan].swizzle);
        D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
        ctx->mod.shader.EmitInstruction(mov);
      }
      return true;
    }

    case nir_op_fmax:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_MAX, alu));
      return true;

    case nir_op_fsat: {
      D3D10ShaderBinary::CInstruction intr =
          get_intr_1_args(D3D10_SB_OPCODE_MOV, alu);
      intr.m_bSaturate = true;
      ctx->mod.shader.EmitInstruction(intr);
      return true;
    }

    case nir_op_fmul:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_MUL, alu));
      return true;

    case nir_op_fdiv:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_DIV, alu));
      return true;

    case nir_op_fadd:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_ADD, alu));
      return true;

    case nir_op_fsub: {
      D3D10ShaderBinary::CInstruction intr =
          get_intr_2_args(D3D10_SB_OPCODE_ADD, alu);
      intr.m_Operands[2].SetModifier(D3D10_SB_OPERAND_MODIFIER_NEG);
      ctx->mod.shader.EmitInstruction(intr);
      return true;
    }

    case nir_op_bcsel:
      ctx->mod.shader.EmitInstruction(
          get_intr_3_args(D3D10_SB_OPCODE_MOVC, alu));
      return true;

    case nir_op_feq:
      ctx->mod.shader.EmitInstruction(get_intr_2_args(D3D10_SB_OPCODE_EQ, alu));
      return true;

    case nir_op_mov:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_MOV, alu));
      return true;

    case nir_op_ige:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_IGE, alu));
      return true;

    case nir_op_ieq:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_IEQ, alu));
      return true;

    case nir_op_ior:
      ctx->mod.shader.EmitInstruction(get_intr_2_args(D3D10_SB_OPCODE_OR, alu));
      return true;

    case nir_op_iand:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_AND, alu));
      return true;

    case nir_op_iadd:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_IADD, alu));
      return true;

    case nir_op_ushr:
      ctx->mod.shader.EmitInstruction(
          get_intr_2_args(D3D10_SB_OPCODE_USHR, alu));
      return true;

    case nir_op_flt:
      ctx->mod.shader.EmitInstruction(get_intr_2_args(D3D10_SB_OPCODE_LT, alu));
      return true;

    case nir_op_fsqrt:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_SQRT, alu));
      return true;

    case nir_op_ffloor:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_ROUND_NI, alu));
      return true;

    case nir_op_fceil:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_ROUND_PI, alu));
      return true;

    case nir_op_fround_even:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_ROUND_NE, alu));
      return true;

    case nir_op_ffract:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_FRC, alu));
      return true;

    case nir_op_ftrunc:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_ROUND_Z, alu));
      return true;

    case nir_op_fabs: {
      D3D10ShaderBinary::CInstruction intr =
          get_intr_1_args(D3D10_SB_OPCODE_MOV, alu);
      intr.m_Operands[1].SetModifier(D3D10_SB_OPERAND_MODIFIER_ABS);
      ctx->mod.shader.EmitInstruction(intr);
      return true;
    }

    case nir_op_f2i32:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_FTOI, alu));
      return true;

    case nir_op_b2f32:
      // TODO: is a bool an int in this instruction's eyes?
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_ITOF, alu));
      return true;

    case nir_op_fcos: {
      uint32_t num_write_components =
          count_write_components(alu->dest.write_mask);
      D3D10ShaderBinary::COperandBase dst =
          nir_dest_as_register(alu->dest.dest, alu->dest.write_mask);
      D3D10ShaderBinary::COperandBase a = nir_src_as_const_value_or_register(
          alu->src[0].src, num_write_components, alu->src[0].swizzle);
      ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(
          D3D10_SB_OPCODE_SINCOS, null_operand, dst, a));
      return true;
    }

    case nir_op_fsin: {
      uint32_t num_write_components =
          count_write_components(alu->dest.write_mask);
      D3D10ShaderBinary::COperandBase dst =
          nir_dest_as_register(alu->dest.dest, alu->dest.write_mask);
      D3D10ShaderBinary::COperandBase a = nir_src_as_const_value_or_register(
          alu->src[0].src, num_write_components, alu->src[0].swizzle);
      ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(
          D3D10_SB_OPCODE_SINCOS, dst, null_operand, a));
      return true;
    }

    case nir_op_fddx:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_DERIV_RTX, alu));
      return true;

    case nir_op_fddy:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D10_SB_OPCODE_DERIV_RTY, alu));
      return true;

    case nir_op_fddx_fine:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D11_SB_OPCODE_DERIV_RTX_FINE, alu));
      return true;

    case nir_op_fddy_fine:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D11_SB_OPCODE_DERIV_RTY_FINE, alu));
      return true;

    case nir_op_fddx_coarse:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D11_SB_OPCODE_DERIV_RTX_COARSE, alu));
      return true;

    case nir_op_fddy_coarse:
      ctx->mod.shader.EmitInstruction(
          get_intr_1_args(D3D11_SB_OPCODE_DERIV_RTY_COARSE, alu));
      return true;

    default:
      NIR_INSTR_UNSUPPORTED(&alu->instr);
      assert(!"Unimplemented ALU instruction");
      return false;
  }
  return true;
}

static bool
emit_store_output(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
  D3D10ShaderBinary::COperandBase src = nir_src_as_const_value_or_register(intr->src[0], 4,nullptr);
  D3D10ShaderBinary::COperandDst dst(D3D10_SB_OPERAND_TYPE_OUTPUT,
                                 //   nir_src_as_uint(intr->src[1]) // TODO is `base` the output we care about?
                                 nir_intrinsic_base(intr)
                                   );
  D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
  ctx->mod.shader.EmitInstruction(mov);

  return true;
}

static bool
emit_load_input(struct ntd_context* ctx,
                nir_intrinsic_instr* intr) {
  D3D10ShaderBinary::COperand4 src(D3D10_SB_OPERAND_TYPE_INPUT,
                                 //   nir_src_as_uint(intr->src[0]) // TODO is `base` the input we care about?
                                 nir_intrinsic_base(intr)
                                   );

  D3D10ShaderBinary::COperandBase dst = nir_dest_as_register(intr->dest, 0b1111);

  D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
  ctx->mod.shader.EmitInstruction(mov);

  return true;
}

static bool
emit_load_frag_coord(struct ntd_context* ctx,
                nir_intrinsic_instr* intr) {
   // ctx->dxil_mod.inputs->sysvalue
   UINT pos_reg_index = 0; /* todo: search input signature for POS register */
  D3D10ShaderBinary::COperand4 src(D3D10_SB_OPERAND_TYPE_INPUT, pos_reg_index);
  D3D10ShaderBinary::COperandBase dst = nir_dest_as_register(intr->dest, 0b1111);
  D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
  ctx->mod.shader.EmitInstruction(mov);
  return true;
}


static bool
emit_intrinsic(struct ntd_context *ctx, nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_input:
      return emit_load_input(ctx, intr);
   case nir_intrinsic_store_output:
      return emit_store_output(ctx, intr);

   case nir_intrinsic_load_frag_coord:
      return emit_load_frag_coord(ctx, intr);

   default:
      NIR_INSTR_UNSUPPORTED(&intr->instr);
      assert(!"Unimplemented intrinsic instruction");
      return false;
   }
}

static bool
emit_load_const(struct ntd_context *ctx, nir_load_const_instr *load_const)
{
   // No-op since we can always chase SSAs in concrete instructions.
   return true;
}

static bool
emit_jump(struct ntd_context *ctx, nir_jump_instr *instr)
{
   switch (instr->type) {
   case nir_jump_break:
      assert(instr->instr.block->successors[0]);
      assert(!instr->instr.block->successors[1]);
      ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(D3D10_SB_OPCODE_BREAK));
      return true;

   case nir_jump_continue:
      assert(instr->instr.block->successors[0]);
      assert(!instr->instr.block->successors[1]);
      ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(D3D10_SB_OPCODE_CONTINUE));
      return true;

   default:
      unreachable("Unsupported jump type\n");
   }
}


static bool
emit_instr(struct ntd_context *ctx, struct nir_instr* instr)
{
   switch (instr->type) {
   case nir_instr_type_alu:
      return emit_alu(ctx, nir_instr_as_alu(instr));
   case nir_instr_type_intrinsic:
      return emit_intrinsic(ctx, nir_instr_as_intrinsic(instr));
   case nir_instr_type_load_const:
      return emit_load_const(ctx, nir_instr_as_load_const(instr));
   // case nir_instr_type_deref:
   //    return emit_deref(ctx, nir_instr_as_deref(instr));
   case nir_instr_type_jump:
      return emit_jump(ctx, nir_instr_as_jump(instr));
   // case nir_instr_type_phi:
   //    return emit_phi(ctx, nir_instr_as_phi(instr));
   // case nir_instr_type_tex:
   //    return emit_tex(ctx, nir_instr_as_tex(instr));
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
   // assert(block->index < ctx->mod.num_basic_block_ids);
   // ctx->mod.basic_block_ids[block->index] = ctx->mod.curr_block;

   nir_foreach_instr(instr, block) {
      TRACE_CONVERSION(instr);

      if (!emit_instr(ctx, instr))  {
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
   D3D10ShaderBinary::COperandBase cond = nir_src_as_const_value_or_register(if_stmt->condition, 4, nullptr);
   ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(D3D10_SB_OPCODE_IF, cond, /* todo? is it ever ZERO? */D3D10_SB_INSTRUCTION_TEST_NONZERO));

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

   // if (!emit_cond_branch(ctx, cond, then_block->index,
   //                       else_block ? else_block->index : then_succ))
   //    return false;

   /* handle then-block */
   if (!emit_cf_list(ctx, &if_stmt->then_list) 
      // ||
      // (!nir_block_ends_in_jump(nir_if_last_then_block(if_stmt)) &&
      // !emit_branch(ctx, then_succ))
      )
      return false;

   if (else_block) {
      /* handle else-block */
      if (!emit_cf_list(ctx, &if_stmt->else_list)
         // ||
         // (!nir_block_ends_in_jump(nir_if_last_else_block(if_stmt)) &&
         // !emit_branch(ctx, else_succ))
         )
         return false;
   }


   ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(D3D10_SB_OPCODE_ENDIF));
   return true;
}

static bool
emit_loop(struct ntd_context *ctx, nir_loop *loop)
{
   nir_block *first_block = nir_loop_first_block(loop);

   assert(nir_loop_last_block(loop)->successors[0]);
   assert(!nir_loop_last_block(loop)->successors[1]);

   ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(D3D10_SB_OPCODE_LOOP));

   // if (!emit_branch(ctx, first_block->index))
   //    return false;

   if (!emit_cf_list(ctx, &loop->body))
      return false;

   // if (!emit_branch(ctx, first_block->index))
   //    return false;

   ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(D3D10_SB_OPCODE_ENDLOOP));

   return true;
}

static bool
emit_cf_list(struct ntd_context *ctx, struct exec_list *list)
{
   foreach_list_typed(nir_cf_node, node, node, list) {
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

static bool
emit_dcl(struct ntd_context *ctx)
{
   // TODO?
   ctx->mod.shader.EmitGlobalFlagsDecl(
      D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED);

   for (int i = 0; i < ctx->dxil_mod.num_sig_inputs; i++) {
     struct dxil_signature_record& input = ctx->dxil_mod.inputs[i];
     for (int e = 0; e < input.num_elements; e++) {
       struct dxil_signature_element& elem = input.elements[e];
       UINT write_mask = elem.mask << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT;

       // TODO: If this trips it's probably because we need to handle SIVs or SGVs
       assert(elem.system_value == 0);

       if (ctx->mod.shader_kind == D3D10_SB_VERTEX_SHADER) {
         ctx->mod.shader.EmitInputDecl(D3D10_SB_OPERAND_TYPE_INPUT, elem.reg,
                                       write_mask);
       } else {
         ctx->mod.shader.EmitPSInputDecl(elem.reg, write_mask,
                                         D3D10_SB_INTERPOLATION_LINEAR  // TODO?
         );
       }
     }
   }

   for (int i = 0; i < ctx->dxil_mod.num_sig_outputs; i++) {
     struct dxil_signature_record& output = ctx->dxil_mod.outputs[i];
     for (int e = 0; e < output.num_elements; e++) {
       struct dxil_signature_element& elem = output.elements[e];
       UINT write_mask = elem.mask << D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT;

       // elem.system_value is `enum dxil_prog_sig_semantic`
       // https://gitlab.freedesktop.org/mesa/mesa/blob/4a3395f35aeeb90f4613922dfe761dae62572f4b/src/microsoft/compiler/dxil_signature.c#L405
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

         default:
           unreachable("unhandled dxil_prog_sig_semantic");
           return false;
       }
     }
   }

   uint32_t num_temps = 0;
   nir_foreach_register(reg, &nir_shader_get_entrypoint(ctx->shader)->registers) {
      num_temps = std::max(num_temps, reg->index + 1);
   }
   if (num_temps > 0) {
     ctx->mod.shader.EmitTempsDecl(num_temps);
   }

   return true;
}

static bool
emit_module(struct ntd_context *ctx) {
  if (!emit_dcl(ctx)) {
    return false;
   }

  nir_function_impl* entry = nir_shader_get_entrypoint(ctx->shader);
  nir_metadata_require(entry, nir_metadata_block_index);
  if (!emit_cf_list(ctx, &entry->body)) {
    return false;
   }

   ctx->mod.shader.EmitInstruction(D3D10ShaderBinary::CInstruction(D3D10_SB_OPCODE_RET));

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
get_glsl_type_size(const struct glsl_type * type, bool is_bindless)
{
   return glsl_count_attribute_slots(type, false);
}

bool
nir_to_dxbc(struct nir_shader *s, const struct nir_to_dxil_options *opts,
            struct blob *blob)
{
   assert(opts);
   blob_init(blob);

   NIR_PASS_V(s, nir_lower_pack);
   NIR_PASS_V(s, nir_lower_frexp);
   NIR_PASS_V(s, nir_lower_flrp, 16 | 32 | 64, true);

   // NOTE: do not run scalarization passes
   optimize_nir(s, opts, false);

   NIR_PASS_V(s, nir_remove_dead_variables,
              nir_var_function_temp | nir_var_shader_temp, NULL);

   nir_lower_io_options options{};
   NIR_PASS_V(s, nir_lower_io, nir_var_shader_in | nir_var_shader_out | nir_var_uniform, get_glsl_type_size, options);
   // NIR_PASS_V(s, nir_lower_locals_to_regs);
   // NIR_PASS_V(s, nir_move_vec_src_uses_to_dest);
   // NIR_PASS_V(s, nir_lower_vec_to_movs, NULL, NULL);
   NIR_PASS_V(s, nir_opt_dce);
   // NIR_PASS_V(s, nir_remove_dead_variables, nir_var_function_temp, NULL);
   NIR_PASS_V(s, nir_convert_from_ssa, false);

   ntd_context ctx;
   ctx.shader = s;
   ctx.opts = opts;
   ctx.mod.shader_kind = get_dxbc_shader_kind(s);
   ctx.mod.major_version = 5;
   ctx.mod.minor_version = 1;

   // TODO SV_Position doesn't make it into dxil_module's inputs?
   get_signatures(&ctx.dxil_mod, s, ctx.opts->vulkan_environment);

   DxbcModule& mod = ctx.mod;
   mod.shader.Init(1024);
   mod.shader.StartShader(mod.shader_kind, mod.major_version, mod.minor_version);

   emit_module(&ctx);

   mod.shader.EndShader();

   ScopedDxilContainer container;

   // RDEF is only used for reflection?

   if (!dxil_container_add_io_signature(container.get(), DXIL_ISG1, ctx.dxil_mod.num_sig_inputs,
                                          ctx.dxil_mod.inputs)) {
      debug_printf("D3D12: dxil_container_add_io_signature failed\n");
      return false;
   }

   if (!dxil_container_add_io_signature(container.get(), DXIL_OSG1, ctx.dxil_mod.num_sig_outputs,
                                          ctx.dxil_mod.outputs)) {
      debug_printf("D3D12: dxil_container_add_io_signature failed\n");
      return false;
   }

   // if (!dxil_container_add_features(container.get(), &ctx.dxil_mod.feats)) {
   //    debug_printf("D3D12: dxil_container_add_features failed\n");
   //    return false;
   // }

   if (!dxil_container_add_shader_blob(
            container.get(), static_cast<const void*>(mod.shader.GetShader()),
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

   return true;
}
