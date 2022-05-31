/*
 * Copyright © 2022 Valve Corporation
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

#include "nir.h"
#include "nir_builder.h"
#include "ac_nir.h"
#include "radv_constants.h"
#include "radv_private.h"
#include "radv_shader.h"
#include "radv_shader_args.h"

typedef struct {
   enum amd_gfx_level gfx_level;
   uint32_t address32_hi;
   const struct radv_shader_args *args;
   const struct radv_shader_info *info;
   const struct radv_pipeline_key *pl_key;
} lower_vs_input_state;

static nir_ssa_def *
lower_load_vs_input_from_prolog(nir_builder *b,
                                nir_intrinsic_instr *intrin,
                                lower_vs_input_state *s)
{
   nir_src *offset_src = nir_get_io_offset_src(intrin);
   assert(nir_src_is_const(*offset_src));

   unsigned base = nir_intrinsic_base(intrin);
   unsigned base_offset = nir_src_as_uint(*offset_src);
   unsigned location = base + base_offset - VERT_ATTRIB_GENERIC0;
   unsigned component = nir_intrinsic_component(intrin);
   unsigned bit_size = intrin->dest.ssa.bit_size;
   unsigned num_components = intrin->dest.ssa.num_components;

   nir_ssa_def *input_arg = ac_nir_load_arg(b, &s->args->ac, s->args->vs_inputs[location]);
   return nir_extract_bits(b, &input_arg, 1, component * bit_size, num_components, bit_size);
}

static nir_ssa_def *
calc_vs_input_index_instance_rate(nir_builder *b,
                                  unsigned location,
                                  lower_vs_input_state *s)
{
   nir_ssa_def *start_instance = nir_load_base_instance(b);
   uint32_t divisor = s->pl_key->vs.instance_rate_divisors[location];
   if (!divisor)
      return start_instance;

   nir_ssa_def *instance_id = nir_load_instance_id(b);
   if (divisor == 1)
      return nir_iadd(b, start_instance, instance_id);

   nir_ssa_def *divided = nir_udiv_imm(b, instance_id, divisor);
   return nir_iadd(b, start_instance, divided);
}

static nir_ssa_def *
calc_vs_input_index(nir_builder *b,
                    unsigned location,
                    lower_vs_input_state *s)
{
   if (s->pl_key->vs.instance_rate_inputs & BITFIELD_BIT(location))
      return calc_vs_input_index_instance_rate(b, location, s);

   return nir_iadd(b, nir_load_first_vertex(b), nir_load_vertex_id_zero_base(b));
}

static nir_ssa_def *
lower_load_vs_input(nir_builder *b,
                    nir_intrinsic_instr *intrin,
                    lower_vs_input_state *s)
{
   nir_src *offset_src = nir_get_io_offset_src(intrin);
   assert(nir_src_is_const(*offset_src));

   const unsigned base = nir_intrinsic_base(intrin);
   const unsigned base_offset = nir_src_as_uint(*offset_src);
   const unsigned location = base + base_offset - VERT_ATTRIB_GENERIC0;
   const unsigned component = nir_intrinsic_component(intrin);
   const unsigned bit_size = intrin->dest.ssa.bit_size;
   const unsigned num_components = intrin->dest.ssa.num_components;

   const uint32_t attrib_binding = s->pl_key->vs.vertex_attribute_bindings[location];
   const uint32_t attrib_offset = s->pl_key->vs.vertex_attribute_offsets[location];
   const uint32_t attrib_stride = s->pl_key->vs.vertex_attribute_strides[location];
   const uint32_t attrib_format = s->pl_key->vs.vertex_attribute_formats[location];
   const unsigned binding_index = s->info->vs.use_per_attribute_vb_descs ? location : attrib_binding;
   const unsigned desc_index = util_bitcount(s->info->vs.vb_desc_usage_mask & u_bit_consecutive(0, binding_index));

   const unsigned dfmt = attrib_format & 0xf;
   const unsigned nfmt = (attrib_format >> 4) & 0x7;
   const struct ac_data_format_info* vtx_info = ac_get_data_format_info(dfmt);

   nir_ssa_def *vertex_buffers_arg = ac_nir_load_arg(b, &s->args->ac, s->args->ac.vertex_buffers);
   nir_ssa_def *vertex_buffers = nir_pack_64_2x32_split(b, vertex_buffers_arg, nir_imm_int(b, s->address32_hi));
   nir_ssa_def *descriptor = nir_load_smem_amd(b, 4, vertex_buffers, nir_imm_int(b, desc_index * 16));
   nir_ssa_def *index = calc_vs_input_index(b, location, s);
   nir_ssa_def *zero = nir_imm_int(b, 0);

   const bool use_buffer_load =
      vtx_info->chan_byte_size == 4 &&
      (nfmt == V_008F0C_BUF_NUM_FORMAT_FLOAT || nfmt == V_008F0C_BUF_NUM_FORMAT_UINT ||
       nfmt == V_008F0C_BUF_NUM_FORMAT_SINT);

   if (use_buffer_load) {
      unsigned const_off = attrib_offset + vtx_info->chan_byte_size * component;
      nir_ssa_def *addr = zero;
      if (attrib_stride && const_off > attrib_stride) {
         addr = nir_imm_int(b, const_off / attrib_stride);
         const_off %= attrib_stride;
      }

      return nir_load_buffer_amd(b, num_components, bit_size, descriptor, addr, zero, index, .base = const_off);
   }

   return NULL;
}

static nir_ssa_def *
lower_vs_input_instr(nir_builder *b, nir_instr *instr, void *state)
{
   lower_vs_input_state *s = (lower_vs_input_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   assert(intrin->intrinsic == nir_intrinsic_load_input);

   if (s->info->vs.dynamic_inputs)
      return lower_load_vs_input_from_prolog(b, intrin, s);

   return lower_load_vs_input(b, intrin, s);
}

static bool
filter_vs_input_instr(const nir_instr *instr,
                 UNUSED const void *state)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   return intrin->intrinsic == nir_intrinsic_load_input;
}

void
radv_nir_lower_vs_input(nir_shader *shader,
                        const struct radv_device *device,
                        const struct radv_shader_info *info,
                        const struct radv_shader_args *args,
                        const struct radv_pipeline_key *pl_key)
{
   if (shader->info.stage != MESA_SHADER_VERTEX)
      return;

   lower_vs_input_state state = {
      .gfx_level = device->physical_device->rad_info.gfx_level,
      .address32_hi = device->physical_device->rad_info.address32_hi,
      .info = info,
      .args = args,
      .pl_key = pl_key,
   };

   nir_shader_lower_instructions(shader, filter_vs_input_instr, lower_vs_input_instr, &state);
}
