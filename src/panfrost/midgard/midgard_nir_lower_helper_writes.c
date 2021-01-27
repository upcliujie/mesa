/*
 * Copyright (C) 2020-2021 Collabora, Ltd.
 * Copyright © 2020 Valve Corporation
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

#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "midgard_nir.h"

static bool
nir_lower_helper_writes(nir_builder *b, nir_instr *instr, UNUSED void *data)
{
        if (instr->type != nir_instr_type_intrinsic)
                return false;

        nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

        switch (intr->intrinsic) {
        case nir_intrinsic_atomic_counter_inc:
        case nir_intrinsic_atomic_counter_inc_deref:
        case nir_intrinsic_atomic_counter_add:
        case nir_intrinsic_atomic_counter_add_deref:
        case nir_intrinsic_atomic_counter_pre_dec:
        case nir_intrinsic_atomic_counter_pre_dec_deref:
        case nir_intrinsic_atomic_counter_post_dec:
        case nir_intrinsic_atomic_counter_post_dec_deref:
        case nir_intrinsic_atomic_counter_min:
        case nir_intrinsic_atomic_counter_min_deref:
        case nir_intrinsic_atomic_counter_max:
        case nir_intrinsic_atomic_counter_max_deref:
        case nir_intrinsic_atomic_counter_and:
        case nir_intrinsic_atomic_counter_and_deref:
        case nir_intrinsic_atomic_counter_or:
        case nir_intrinsic_atomic_counter_or_deref:
        case nir_intrinsic_atomic_counter_xor:
        case nir_intrinsic_atomic_counter_xor_deref:
        case nir_intrinsic_atomic_counter_exchange:
        case nir_intrinsic_atomic_counter_exchange_deref:
        case nir_intrinsic_atomic_counter_comp_swap:
        case nir_intrinsic_atomic_counter_comp_swap_deref:
        case nir_intrinsic_bindless_image_atomic_add:
        case nir_intrinsic_bindless_image_atomic_and:
        case nir_intrinsic_bindless_image_atomic_comp_swap:
        case nir_intrinsic_bindless_image_atomic_dec_wrap:
        case nir_intrinsic_bindless_image_atomic_exchange:
        case nir_intrinsic_bindless_image_atomic_fadd:
        case nir_intrinsic_bindless_image_atomic_imax:
        case nir_intrinsic_bindless_image_atomic_imin:
        case nir_intrinsic_bindless_image_atomic_inc_wrap:
        case nir_intrinsic_bindless_image_atomic_or:
        case nir_intrinsic_bindless_image_atomic_umax:
        case nir_intrinsic_bindless_image_atomic_umin:
        case nir_intrinsic_bindless_image_atomic_xor:
        case nir_intrinsic_bindless_image_store:
        case nir_intrinsic_bindless_image_store_raw_intel:
        case nir_intrinsic_global_atomic_add:
        case nir_intrinsic_global_atomic_and:
        case nir_intrinsic_global_atomic_comp_swap:
        case nir_intrinsic_global_atomic_exchange:
        case nir_intrinsic_global_atomic_fadd:
        case nir_intrinsic_global_atomic_fcomp_swap:
        case nir_intrinsic_global_atomic_fmax:
        case nir_intrinsic_global_atomic_fmin:
        case nir_intrinsic_global_atomic_imax:
        case nir_intrinsic_global_atomic_imin:
        case nir_intrinsic_global_atomic_or:
        case nir_intrinsic_global_atomic_umax:
        case nir_intrinsic_global_atomic_umin:
        case nir_intrinsic_global_atomic_xor:
        case nir_intrinsic_image_atomic_add:
        case nir_intrinsic_image_atomic_and:
        case nir_intrinsic_image_atomic_comp_swap:
        case nir_intrinsic_image_atomic_dec_wrap:
        case nir_intrinsic_image_atomic_exchange:
        case nir_intrinsic_image_atomic_fadd:
        case nir_intrinsic_image_atomic_imax:
        case nir_intrinsic_image_atomic_imin:
        case nir_intrinsic_image_atomic_inc_wrap:
        case nir_intrinsic_image_atomic_or:
        case nir_intrinsic_image_atomic_umax:
        case nir_intrinsic_image_atomic_umin:
        case nir_intrinsic_image_atomic_xor:
        case nir_intrinsic_image_deref_atomic_add:
        case nir_intrinsic_image_deref_atomic_and:
        case nir_intrinsic_image_deref_atomic_comp_swap:
        case nir_intrinsic_image_deref_atomic_dec_wrap:
        case nir_intrinsic_image_deref_atomic_exchange:
        case nir_intrinsic_image_deref_atomic_fadd:
        case nir_intrinsic_image_deref_atomic_imax:
        case nir_intrinsic_image_deref_atomic_imin:
        case nir_intrinsic_image_deref_atomic_inc_wrap:
        case nir_intrinsic_image_deref_atomic_or:
        case nir_intrinsic_image_deref_atomic_umax:
        case nir_intrinsic_image_deref_atomic_umin:
        case nir_intrinsic_image_deref_atomic_xor:
        case nir_intrinsic_image_deref_store:
        case nir_intrinsic_image_deref_store_raw_intel:
        case nir_intrinsic_image_store:
        case nir_intrinsic_image_store_raw_intel:
        case nir_intrinsic_ssbo_atomic_add:
        case nir_intrinsic_ssbo_atomic_add_ir3:
        case nir_intrinsic_ssbo_atomic_and:
        case nir_intrinsic_ssbo_atomic_and_ir3:
        case nir_intrinsic_ssbo_atomic_comp_swap:
        case nir_intrinsic_ssbo_atomic_comp_swap_ir3:
        case nir_intrinsic_ssbo_atomic_exchange:
        case nir_intrinsic_ssbo_atomic_exchange_ir3:
        case nir_intrinsic_ssbo_atomic_fadd:
        case nir_intrinsic_ssbo_atomic_fcomp_swap:
        case nir_intrinsic_ssbo_atomic_fmax:
        case nir_intrinsic_ssbo_atomic_fmin:
        case nir_intrinsic_ssbo_atomic_imax:
        case nir_intrinsic_ssbo_atomic_imax_ir3:
        case nir_intrinsic_ssbo_atomic_imin:
        case nir_intrinsic_ssbo_atomic_imin_ir3:
        case nir_intrinsic_ssbo_atomic_or:
        case nir_intrinsic_ssbo_atomic_or_ir3:
        case nir_intrinsic_ssbo_atomic_umax:
        case nir_intrinsic_ssbo_atomic_umax_ir3:
        case nir_intrinsic_ssbo_atomic_umin:
        case nir_intrinsic_ssbo_atomic_umin_ir3:
        case nir_intrinsic_ssbo_atomic_xor:
        case nir_intrinsic_ssbo_atomic_xor_ir3:
        case nir_intrinsic_store_global:
        case nir_intrinsic_store_global_ir3:
        case nir_intrinsic_store_ssbo:
        case nir_intrinsic_store_ssbo_ir3:
                break;
        default:
                return false;
        }

        b->cursor = nir_before_instr(instr);

        nir_ssa_def *helper = nir_load_helper_invocation(b, 1);
        nir_push_if(b, nir_inot(b, helper));
        nir_instr_remove(instr);
        nir_builder_instr_insert(b, instr);
        nir_pop_if(b, NULL);

        return true;
}

bool
midgard_nir_lower_helper_writes(nir_shader *shader)
{
        if (shader->info.stage != MESA_SHADER_FRAGMENT)
                return false;

        return nir_shader_instructions_pass(shader,
                        nir_lower_helper_writes,
                        nir_metadata_none,
                        NULL);
}
