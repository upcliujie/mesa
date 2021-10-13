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

bool
nir_to_dxbc(struct nir_shader *s, const struct nir_to_dxbc_options *opts,
            struct blob *blob)
{
   assert(opts);
   bool retval = true;
   blob_init(blob);

   // TODO: Lower SSAs to registers (nir_convert_from_ssa)
   // NOTE: do not run scalarization passes

   // NIR_PASS_V(s, nir_convert_from_ssa, false);

   DxbcModule module;
   module.shader.Init(1024);
   module.shader.StartShader(D3D10_SB_VERTEX_SHADER, 5, 1);
   D3D10ShaderBinary::COperandBase nil;
   D3D10ShaderBinary::CInstruction mov(D3D10_SB_OPCODE_MOV, nil, nil);
   module.shader.EmitInstruction(mov);
   module.shader.EndShader();

   module.inputs[0].elements[0].stream = 0;
   module.inputs[0].elements[0].semantic_name_offset = 11;
   module.inputs[0].elements[0].semantic_index = 0;
   module.inputs[0].elements[0].system_value = DXIL_PROG_SEM_POSITION;
   module.inputs[0].elements[0].comp_type = DXIL_PROG_SIG_COMP_TYPE_FLOAT32;
   module.inputs[0].elements[0].reg = 0;
   module.inputs[0].elements[0].mask = 0b1111;
   module.inputs[0].elements[0].always_reads_mask = 0b1111;
   module.inputs[0].elements[0].pad = 0;
   module.inputs[0].elements[0].min_precision = DXIL_MIN_PREC_DEFAULT;
   module.inputs[0].num_elements = 1;
   module.inputs[0].name = const_cast<char*>("SV_Position");
   module.inputs[0].sysvalue = "POS";

// never_writes_mask

   module.outputs[0].num_elements = 1;
   module.outputs[0].name = const_cast<char*>("TARGET");
   module.outputs[0].sysvalue = "TARGET";

   dxil_container c;
   dxil_container_init(&c);

   if (!dxil_container_add_io_signature(&c, DXIL_ISG1, 1, module.inputs)) {
     debug_printf("D3D12: dxil_container_add_io_signature failed\n");
     retval = false;
     goto out;
   }

   if (!dxil_container_add_io_signature(&c, DXIL_OSG1, 1, module.outputs)) {
     debug_printf("D3D12: dxil_container_add_io_signature failed\n");
     retval = false;
     goto out;
   }

   if (!dxil_container_add_features(&c, &module.feats)) {
     debug_printf("D3D12: dxil_container_add_features failed\n");
     retval = false;
     goto out;
   }

   if (!dxil_container_add_shader_blob(
           &c, static_cast<const void*>(module.shader.GetShader()),
           static_cast<uint32_t>(module.shader.ShaderSizeInDWORDs() *
                                 sizeof(UINT)))) {
     debug_printf("D3D12: dxil_container_add_shader_blob failed\n");
     retval = false;
     goto out;
   }

   if (!dxil_container_write(&c, blob)) {
     debug_printf("D3D12: dxil_container_write failed\n");
     retval = false;
     goto out;
   }

   dxil_container_finish(&c);

   // retval = false;
   // assert("nir_to_dxbc unimplemented");

out:
   // ralloc_free(ctx->ralloc_ctx);
   // free(ctx);
   return retval;
}
