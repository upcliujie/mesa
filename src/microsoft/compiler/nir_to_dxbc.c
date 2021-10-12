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

#include "d3d12TokenizedProgramFormat.hpp"
#include "nir/nir_builder.h"
#include "util/u_debug.h"
#include "util/u_dynarray.h"
#include "util/u_math.h"

#include "git_sha1.h"

#include "vulkan/vulkan_core.h"

#include <stdint.h>

static const nir_shader_compiler_options
nir_options = {
   .lower_ineg = true,
   .lower_fneg = true,
   .lower_ffma16 = true,
   .lower_ffma32 = true,
   .lower_isign = true,
   .lower_fsign = true,
   .lower_iabs = true,
   .lower_fmod = true,
   .lower_fpow = true,
   .lower_scmp = true,
   .lower_ldexp = true,
   .lower_flrp16 = true,
   .lower_flrp32 = true,
   .lower_flrp64 = true,
   .lower_bitfield_extract_to_shifts = true,
   .lower_extract_word = true,
   .lower_extract_byte = true,
   .lower_insert_word = true,
   .lower_insert_byte = true,
   .lower_all_io_to_elements = true,
   .lower_all_io_to_temps = true,
   .lower_hadd = true,
   .lower_uadd_sat = true,
   .lower_iadd_sat = true,
   .lower_uadd_carry = true,
   .lower_mul_high = true,
   .lower_rotate = true,
   .lower_pack_64_2x32_split = true,
   .lower_pack_32_2x16_split = true,
   .lower_unpack_64_2x32_split = true,
   .lower_unpack_32_2x16_split = true,
   .has_fsub = true,
   .has_isub = true,
   .use_scoped_barrier = true,
   .vertex_id_zero_based = true,
   .lower_base_vertex = true,
   .has_cs_global_id = true,
   .has_txs = true,
};

const nir_shader_compiler_options*
dxbc_get_nir_compiler_options(void)
{
   return &nir_options;
}

bool
nir_to_dxbc(struct nir_shader *s, const struct nir_to_dxbc_options *opts,
            struct blob *blob)
{
   assert(opts);
   bool retval = true;
   blob_init(blob);

   // TODO: Lower SSAs to registers (nir_lower_vars_to_ssa)
   // NOTE: do not run scalarization passes

   debug_printf("test = %d", ENCODE_D3D10_SB_TOKENIZED_PROGRAM_VERSION_TOKEN(D3D10_SB_VERTEX_SHADER, 5, 1));

   retval = false;
   assert("nir_to_dxbc unimplemented");

out:
   // ralloc_free(ctx->ralloc_ctx);
   // free(ctx);
   return retval;
}
