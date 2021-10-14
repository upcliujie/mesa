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
#include "nir.h"
#include "nir/nir_builder.h"
#include "nir_intrinsics.h"
#include "ralloc.h"
#include "util/u_debug.h"
#include "util/u_dynarray.h"
#include "util/u_math.h"

#include "git_sha1.h"

#include "vulkan/vulkan_core.h"

#include "d3d12TokenizedProgramFormat.hpp"
#include "DxbcBuilder.hpp"
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
   ntd_context() : ralloc_ctx(nullptr) {}
   ~ntd_context() { ralloc_free(ralloc_ctx); }
};

class ScopedDxilContainer {
public:
   ScopedDxilContainer() { dxil_container_init(&inner); }
   ~ScopedDxilContainer() { dxil_container_finish(&inner); }
   dxil_container* get() { return &inner; }

private:
   dxil_container inner{};
};

// After running `nir_convert_from_ssa`, we're out of SSA land, with the exception of literal values. This function converts a `nir_src` to either a temporary register value or a literal, based on the `ssa`-ness of it.
static D3D10ShaderBinary::COperandBase
nir_src_as_const_value_or_register(nir_src src, uint8_t swizzle[NIR_MAX_VEC_COMPONENTS])
{
  if (src.is_ssa) {
    // TODO immediate operand type?
    switch (src.ssa->num_components) {
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
    switch (src.reg.reg->num_components) {
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
          return D3D10ShaderBinary::COperand(
              temp, src.reg.reg->index, D3D10_SB_4_COMPONENT_X,
              D3D10_SB_4_COMPONENT_Y, D3D10_SB_4_COMPONENT_Z,
              D3D10_SB_4_COMPONENT_W);
        }

      default:
        unreachable("unhandled number of components");
    }
  }
}

static D3D10ShaderBinary::CInstruction
get_binop_intr(D3D10_SB_OPCODE_TYPE opcode, nir_alu_instr *alu) {
  D3D10ShaderBinary::COperand4 dst(D3D10_SB_OPERAND_TYPE_TEMP,
                                   alu->dest.dest.reg.reg->index,
                                   D3D10_SB_4_COMPONENT_X);
  D3D10ShaderBinary::COperandBase lhs = nir_src_as_const_value_or_register(alu->src[0].src,alu->src[0].swizzle);
  D3D10ShaderBinary::COperandBase rhs = nir_src_as_const_value_or_register(alu->src[1].src,alu->src[0].swizzle);
  return D3D10ShaderBinary::CInstruction(opcode, dst, lhs, rhs);
}

static bool
emit_alu(struct ntd_context *ctx, nir_alu_instr *alu) {
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

               D3D10ShaderBinary::COperandBase src = nir_src_as_const_value_or_register(alu->src[chan].src, alu->src[chan].swizzle);
               D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
               ctx->mod.shader.EmitInstruction(mov);
         }
         return true;
      }
   
   case nir_op_fmul: {
     ctx->mod.shader.EmitInstruction(get_binop_intr(D3D10_SB_OPCODE_MUL, alu));
     return true;
   }

    case nir_op_fadd: {
     ctx->mod.shader.EmitInstruction(get_binop_intr(D3D10_SB_OPCODE_ADD, alu));
     return true;
   }

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
  D3D10ShaderBinary::COperandBase src = nir_src_as_const_value_or_register(intr->src[0], nullptr);
  D3D10ShaderBinary::COperand4 dst(D3D10_SB_OPERAND_TYPE_OUTPUT,
                                   nir_src_as_uint(intr->src[1]));
  D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
  ctx->mod.shader.EmitInstruction(mov);

  return true;
}

static bool
emit_load_input(struct ntd_context* ctx,
                nir_intrinsic_instr* intr) {
  D3D10ShaderBinary::COperand4 src(D3D10_SB_OPERAND_TYPE_INPUT,
                                   nir_src_as_uint(intr->src[0]));

  assert(!intr->dest.is_ssa);

  D3D10ShaderBinary::COperand4 dst(D3D10_SB_OPERAND_TYPE_TEMP,
                                   intr->dest.reg.reg->index);

  D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, dst, src);
  ctx->mod.shader.EmitInstruction(mov);

  return true;
}

static bool
emit_load_frag_coord(struct ntd_context* ctx,
                nir_intrinsic_instr* intr) {
  D3D10ShaderBinary::COperand4 src(D3D10_SB_OPERAND_TYPE_INPUT,
                                   0 /* todo: search input signature for POS register */);

  assert(!intr->dest.is_ssa);
  D3D10ShaderBinary::COperand4 dst(D3D10_SB_OPERAND_TYPE_TEMP,
                                   intr->dest.reg.reg->index);

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
emit_deref(struct ntd_context* ctx, nir_deref_instr* instr)
{
   assert(instr->deref_type == nir_deref_type_var ||
          instr->deref_type == nir_deref_type_array);

   /* In the non-Vulkan environment, there's nothing to emit. Any references to
    * derefs will emit the necessary logic to handle scratch/shared GEP addressing
    */
   if (!ctx->opts->vulkan_environment)
      return true;

   /* In the Vulkan environment, we don't have cached handles for textures or
    * samplers, so let's use the opportunity of walking through the derefs to
    * emit those.
    */
   nir_variable *var = nir_deref_instr_get_variable(instr);
   assert(var);

   if (!glsl_type_is_sampler(glsl_without_array(var->type)) &&
       !glsl_type_is_image(glsl_without_array(var->type)))
      return true;

   assert(!"todo: unimplemented!");
   return false;
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
   case nir_instr_type_deref:
      return emit_deref(ctx, nir_instr_as_deref(instr));
   // case nir_instr_type_jump:
   //    return emit_jump(ctx, nir_instr_as_jump(instr));
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

   D3D10ShaderBinary::CInstruction ret(D3D10_SB_OPCODE_RET);
   ctx->mod.shader.EmitInstruction(ret);

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

      // case nir_cf_node_if:
      //    if (!emit_if(ctx, nir_cf_node_as_if(node)))
      //       return false;
      //    break;

      // case nir_cf_node_loop:
      //    if (!emit_loop(ctx, nir_cf_node_as_loop(node)))
      //       return false;
      //    break;

      default:
         unreachable("unsupported cf-list node");
         break;
      }
   }
   return true;
}

static bool
emit_module(struct ntd_context *ctx) {
   // TODO
   ctx->mod.shader.EmitGlobalFlagsDecl(
      D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED);
   ctx->mod.shader.EmitInputDecl(D3D10_SB_OPERAND_TYPE_INPUT, 0,
                                 D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL);
   ctx->mod.shader.EmitOutputSystemInterpretedValueDecl(
      0, D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL, D3D10_SB_NAME_POSITION);

  nir_function_impl* entry = nir_shader_get_entrypoint(ctx->shader);
  nir_metadata_require(entry, nir_metadata_block_index);
  if (!emit_cf_list(ctx, &entry->body)) {
    return false;
   }

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

   DxbcModule& mod = ctx.mod;
   mod.shader.Init(1024);
   mod.shader.StartShader(mod.shader_kind, mod.major_version, mod.minor_version);

   emit_module(&ctx);

   mod.shader.EndShader();

   mod.inputs[0].elements[0].stream = 0;
   mod.inputs[0].elements[0].semantic_name_offset = 11;
   mod.inputs[0].elements[0].semantic_index = 0;
   mod.inputs[0].elements[0].system_value = DXIL_PROG_SEM_POSITION;
   mod.inputs[0].elements[0].comp_type = DXIL_PROG_SIG_COMP_TYPE_FLOAT32;
   mod.inputs[0].elements[0].reg = 0;
   mod.inputs[0].elements[0].mask = 0b1111;
   mod.inputs[0].elements[0].pad = 0;
   mod.inputs[0].elements[0].min_precision = DXIL_MIN_PREC_DEFAULT;
   mod.inputs[0].num_elements = 1;
   mod.inputs[0].name = const_cast<char*>("SV_Position");
   mod.inputs[0].sysvalue = "POS";

   // never_writes_mask

   mod.outputs[0].num_elements = 1;
   mod.outputs[0].name = const_cast<char*>("TARGET");
   mod.outputs[0].sysvalue = "TARGET";

   ScopedDxilContainer container;

   if (!dxil_container_add_io_signature(container.get(), DXIL_ISG1, 1,
                                          mod.inputs)) {
      debug_printf("D3D12: dxil_container_add_io_signature failed\n");
      return false;
   }

   if (!dxil_container_add_io_signature(container.get(), DXIL_OSG1, 1,
                                          mod.outputs)) {
      debug_printf("D3D12: dxil_container_add_io_signature failed\n");
      return false;
   }

   if (!dxil_container_add_features(container.get(), &mod.feats)) {
      debug_printf("D3D12: dxil_container_add_features failed\n");
      return false;
   }

   if (!dxil_container_add_shader_blob(
            container.get(), static_cast<const void*>(mod.shader.GetShader()),
            static_cast<uint32_t>(mod.shader.ShaderSizeInDWORDs() *
                                 sizeof(UINT)))) {
      debug_printf("D3D12: dxil_container_add_shader_blob failed\n");
      return false;
   }

   if (!dxil_container_write(container.get(), blob)) {
      debug_printf("D3D12: dxil_container_write failed\n");
      return false;
   }

   return true;
}
