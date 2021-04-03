/*
 * Copyright (C) 2021 Collabora, Ltd.
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

#include "compiler.h"
#include "compiler/nir/nir_builder.h"

/* Divergent attribute access is undefined behaviour. To avoid divergence,
 * lower to an if-chain like:
 *
 *   value = 0;
 *   if (lane == 0)
 *      value = ld()
 *   else if (lane == 1)
 *      value = ld()
 *   ...
 *   else if (lane == MAX_LANE)
 *      value = ld()
 */

static bool
bi_lower_divergent_indirects_impl(nir_builder *b, nir_instr *instr, void *data)
{
        if (instr->type != nir_instr_type_intrinsic)
                return false;

        nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
        nir_src *offset;

        /* Not all indirect access needs this workaround */
        switch (intr->intrinsic) {
        case nir_intrinsic_load_input:
        case nir_intrinsic_load_interpolated_input:
                /* Attributes and varyings */
                offset = nir_get_io_offset_src(intr);
                if (!nir_src_is_divergent(*offset))
                        return false;

                break;

        default:
                return false;
        }

        /* This indirect does need it */

        b->cursor = nir_before_instr(instr);
        nir_ssa_def *lane = nir_load_subgroup_invocation(b);
        unsigned *lanes = data;

        nir_ssa_def *res = nir_imm_zero(b, nir_dest_num_components(intr->dest),
                        nir_dest_bit_size(intr->dest));

        for (unsigned i = 0; i < (*lanes); ++i) {
                nir_push_if(b, nir_ieq_imm(b, lane, i));

                nir_instr *c = nir_instr_clone(b->shader, instr);
                nir_intrinsic_instr *c_intr = nir_instr_as_intrinsic(c);
                assert(c_intr->dest.is_ssa);
                nir_ssa_def *c_ssa = &c_intr->dest.ssa;

                nir_builder_instr_insert(b, c);
                nir_pop_if(b, NULL);

                res = nir_if_phi(b, c_ssa, res);
        }

        nir_ssa_def_rewrite_uses(&intr->dest.ssa, res);
        nir_instr_remove(instr);
        return true;
}

bool
bi_lower_divergent_indirects(nir_shader *shader, unsigned lanes)
{
        return nir_shader_instructions_pass(shader,
                        bi_lower_divergent_indirects_impl,
                        nir_metadata_none, &lanes);
}
