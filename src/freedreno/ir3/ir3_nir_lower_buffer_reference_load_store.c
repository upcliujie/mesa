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

#include "ir3_nir.h"
#include "compiler/nir/nir_builder.h"

/**
 * IR3 does not support load/store of 64 bit values.
 * This pass lowers load/store of 64 bit buffer references, and since that is
 * the only 64 bit values we support, we could always expect a single 64 bit
 * value. Otherwise, we would need to be able to divide load/store in two
 * (see sfn_nir_lower_64bit.cpp for such lowering).
 */

static bool
lower_buffer_reference_load(nir_builder *b, nir_intrinsic_instr *load) {
	if (load->dest.ssa.bit_size != 64)
		return false;
	assert(load->dest.ssa.num_components == 1);

	load->num_components *= 2;
	load->dest.ssa.bit_size = 32;
	load->dest.ssa.num_components *= 2;

	if (nir_intrinsic_has_dest_type(load)) {
		nir_intrinsic_set_dest_type(load, nir_type_int32);
	}

	if (nir_intrinsic_has_component(load)) {
		nir_intrinsic_set_component(load, nir_intrinsic_component(load) * 2);
	}

	b->cursor = nir_after_instr(&load->instr);

	nir_ssa_def *packed = nir_pack_64_2x32_split(b,
			nir_channel(b, &load->dest.ssa, 0),
			nir_channel(b, &load->dest.ssa, 1));

	nir_ssa_def_rewrite_uses_after(&load->dest.ssa,
			nir_src_for_ssa(packed), packed->parent_instr);

	return true;
}

static bool
lower_buffer_reference_store(nir_builder *b, nir_intrinsic_instr *store) {
	nir_src *src0 = &store->src[0];

	if (nir_src_bit_size(*src0) != 64)
		return false;
	assert(src0->is_ssa && src0->ssa->num_components == 1);

	b->cursor = nir_before_instr(&store->instr);

	nir_ssa_def *unpacked = nir_vec2(b,
			nir_unpack_64_2x32_split_x(b, src0->ssa),
			nir_unpack_64_2x32_split_y(b, src0->ssa));

	nir_instr_rewrite_src(&store->instr, src0, nir_src_for_ssa(unpacked));

	if (nir_intrinsic_has_write_mask(store)) {
		unsigned wm = nir_intrinsic_write_mask(store);
		nir_intrinsic_set_write_mask(store, (wm == 1) ? 3 : 0xf);
	}

	store->num_components *= 2;

	return true;
}

static bool
lower_64bit_const(nir_builder *b, nir_instr *instr) {
	nir_load_const_instr *load = nir_instr_as_load_const(instr);

	if (load->def.bit_size != 64)
		return false;
	assert(load->def.num_components == 1);

	nir_const_value val[2] = {0};
	uint64_t v = load->value[0].u64;
	val[0].u32 = v & 0xffffffff;
	val[1].u32 = (v >> 32) & 0xffffffff;

	b->cursor = nir_after_instr(instr);

	nir_ssa_def *unpacked_const = nir_build_imm(b,
			2 * load->def.num_components, 32, val);
	nir_ssa_def *packed = nir_pack_64_2x32(b, nir_vec2(b,
			nir_channel(b, unpacked_const, 0),
			nir_channel(b, unpacked_const, 1)));

	nir_ssa_def_rewrite_uses_after(&load->def,
			nir_src_for_ssa(packed), packed->parent_instr);

	return true;
}

static bool
lower_64bit_undef(nir_instr *instr) {
	nir_ssa_undef_instr *undef = nir_instr_as_ssa_undef(instr);

	if (undef->def.bit_size != 64)
		return false;
	assert(undef->def.num_components == 1);

	undef->def.num_components *= 2;
	undef->def.bit_size = 32;

	return true;
}

static bool
lower_buffer_reference_load_store_impl(nir_function_impl *impl)
{
	nir_builder b;
	nir_builder_init(&b, impl);
	bool progress = false;

	nir_foreach_block(block, impl) {
		nir_foreach_instr_safe(instr, block) {
			if (instr->type == nir_instr_type_intrinsic) {
				nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
				switch (intr->intrinsic) {
				case nir_intrinsic_load_input:
				case nir_intrinsic_load_scratch:
				case nir_intrinsic_load_uniform:
				case nir_intrinsic_load_ssbo:
				case nir_intrinsic_load_ubo:
				case nir_intrinsic_load_global:
				case nir_intrinsic_load_global_ir3: {
					progress = lower_buffer_reference_load(&b, intr);
					break;
				}
				case nir_intrinsic_store_output:
				case nir_intrinsic_store_scratch:
				case nir_intrinsic_store_ssbo:
				case nir_intrinsic_store_global:
				case nir_intrinsic_store_global_ir3: {
					progress = lower_buffer_reference_store(&b, intr);
					break;
				}
				default:
					break;
				}
			} else if (instr->type == nir_instr_type_load_const) {
				progress = lower_64bit_const(&b, instr);
			} else if (instr->type == nir_instr_type_ssa_undef) {
				progress = lower_64bit_undef(instr);
			}
		}
	}

	if (progress) {
		nir_metadata_preserve(impl, nir_metadata_block_index |
									nir_metadata_dominance);
	} else {
		nir_metadata_preserve(impl, nir_metadata_all);
	}

	return progress;
}

bool
ir3_nir_lower_buffer_reference_load_store(nir_shader *shader)
{
	bool progress = false;

	nir_foreach_function(function, shader) {
		if (function->impl)
			progress |= lower_buffer_reference_load_store_impl(function->impl);
	}

	return progress;
}
