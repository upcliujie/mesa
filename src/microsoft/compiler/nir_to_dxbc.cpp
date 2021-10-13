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
   return &nir_options;
}

struct DxbcModule {
   struct dxil_signature_record inputs[DXIL_SHADER_MAX_IO_ROWS];
   struct dxil_signature_record outputs[DXIL_SHADER_MAX_IO_ROWS];

   struct dxil_features feats;

   D3D10ShaderBinary::CShaderAsm shader;
};

struct ntd_context {
   void* ralloc_ctx;
   const struct nir_to_dxbc_options* opts{};
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

   assert("unimplemented!");
   return false;
}

static bool
emit_instr(struct ntd_context *ctx, struct nir_instr* instr)
{
   switch (instr->type) {
   // case nir_instr_type_alu:
   //    return emit_alu(ctx, nir_instr_as_alu(instr));
   // case nir_instr_type_intrinsic:
   //    return emit_intrinsic(ctx, nir_instr_as_intrinsic(instr));
   // case nir_instr_type_load_const:
   //    return emit_load_const(ctx, nir_instr_as_load_const(instr));
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
}

bool
nir_to_dxbc(struct nir_shader *s, const struct nir_to_dxbc_options *opts,
            struct blob *blob)
{
   assert(opts);
   blob_init(blob);

   // NOTE: do not run scalarization passes
   // TODO:
   // NIR_PASS_V(s, nir_convert_from_ssa, false);

   ntd_context ctx;
   ctx.shader = s;
   ctx.opts = opts;

   nir_function_impl *entry = nir_shader_get_entrypoint(ctx.shader);
   nir_metadata_require(entry, nir_metadata_block_index);
   if (!emit_cf_list(&ctx, &entry->body)) {
      return false;
   }

   DxbcModule& mod = ctx.mod;
   mod.shader.Init(1024);
   mod.shader.StartShader(D3D10_SB_VERTEX_SHADER, 5, 1);
   D3D10ShaderBinary::COperandBase nil;
   D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, nil, nil);
   mod.shader.EmitInstruction(mov);
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
