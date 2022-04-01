/*
 * © Copyright 2018 Alyssa Rosenzweig
 * Copyright (C) 2019-2020 Collabora, Ltd.
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pan_bo.h"
#include "pan_context.h"
#include "pan_shader.h"
#include "pan_util.h"

#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "nir/tgsi_to_nir.h"
#include "util/u_dynarray.h"
#include "util/u_upload_mgr.h"

#include "tgsi/tgsi_dump.h"

static void
panfrost_nir_lower_xfb_output(struct nir_builder *b, nir_intrinsic_instr *intr,
                             unsigned output, unsigned num_components,
                             unsigned buffer, unsigned offset_words)
{
        assert(buffer < MAX_XFB_BUFFERS);

        assert(nir_intrinsic_component(intr) == 0); // TODO

        /* Transform feedback info in units of words, convert to bytes. */
        uint16_t stride = b->shader->info.xfb_stride[buffer] * 4;
        assert(stride != 0);

        uint16_t offset = offset_words * 4;

        nir_ssa_def *index = nir_load_vertex_id_zero_base(b);
        nir_ssa_def *buf = nir_load_xfb_address(b, 1, 64, .base = buffer);
        nir_ssa_def *addr =
                nir_iadd(b, buf, nir_u2u64(b,
                                    nir_iadd_imm(b,
                                                 nir_imul_imm(b, index, stride),
                                                 offset)));

        nir_ssa_def *value = nir_ssa_for_src(b, intr->src[0], num_components);
        nir_store_global(b, addr, 4, value, BITFIELD_MASK(num_components));
}

static bool
panfrost_nir_lower_xfb(struct nir_builder *b, nir_instr *instr, UNUSED void *data)
{
        if (instr->type != nir_instr_type_intrinsic)
                return false;

        nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
        if (intr->intrinsic != nir_intrinsic_store_output)
                return false;

        bool progress = false;

        b->cursor = nir_before_instr(&intr->instr);

        for (unsigned i = 0; i < 2; ++i) {
                nir_io_xfb xfb = i ? nir_intrinsic_io_xfb2(intr) : nir_intrinsic_io_xfb(intr);
                for (unsigned j = 0; j < 2; ++j) {
                        if (!xfb.out[j].num_components) continue;

                        panfrost_nir_lower_xfb_output(b, intr, i*2 + j,
                                                     xfb.out[j].num_components,
                                                     xfb.out[j].buffer,
                                                     xfb.out[j].offset);
                        progress = true;
                }
        }

        nir_instr_remove(instr);
        return progress;
}

void
panfrost_shader_compile(struct pipe_screen *pscreen,
                        struct panfrost_pool *shader_pool,
                        struct panfrost_pool *desc_pool,
                        enum pipe_shader_ir ir_type,
                        const void *ir,
                        gl_shader_stage stage,
                        struct panfrost_shader_state *state)
{
        struct panfrost_screen *screen = pan_screen(pscreen);
        struct panfrost_device *dev = pan_device(pscreen);

        nir_shader *s;

        if (ir_type == PIPE_SHADER_IR_NIR) {
                s = nir_shader_clone(NULL, ir);
        } else {
                assert (ir_type == PIPE_SHADER_IR_TGSI);
                s = tgsi_to_nir(ir, pscreen, false);
        }

        static bool hack = false;

        if (stage == MESA_SHADER_VERTEX && s->info.has_transform_feedback_varyings && !hack) {
                /* Create compute shader doing transform feedback */
                nir_shader *xfb = nir_shader_clone(NULL, s);
                xfb->info.name = ralloc_asprintf(xfb, "%s@xfb", xfb->info.name);

                NIR_PASS_V(xfb, nir_shader_instructions_pass,
                                panfrost_nir_lower_xfb,
                                nir_metadata_block_index | nir_metadata_dominance,
                                NULL);

                state->xfb = rzalloc(NULL, struct panfrost_shader_state); // XXX: leaks
                hack = true;
                panfrost_shader_compile(pscreen, shader_pool, desc_pool, PIPE_SHADER_IR_NIR, xfb, stage, state->xfb);
                hack = false;
        }

        /* Lower this early so the backends don't have to worry about it */
        if (stage == MESA_SHADER_FRAGMENT)
                NIR_PASS_V(s, nir_lower_fragcolor, state->nr_cbufs);

        s->info.stage = stage;

        /* Call out to Midgard compiler given the above NIR */
        struct panfrost_compile_inputs inputs = {
                .gpu_id = dev->gpu_id,
                .shaderdb = !!(dev->debug & PAN_DBG_PRECOMPILE),
        };

        memcpy(inputs.rt_formats, state->rt_formats, sizeof(inputs.rt_formats));

        struct util_dynarray binary;

        util_dynarray_init(&binary, NULL);
        screen->vtbl.compile_shader(s, &inputs, &binary, &state->info);

        if (binary.size) {
                state->bin = panfrost_pool_take_ref(shader_pool,
                        pan_pool_upload_aligned(&shader_pool->base,
                                binary.data, binary.size, 128));
        }


        /* Don't upload RSD for fragment shaders since they need draw-time
         * merging for e.g. depth/stencil/alpha. RSDs are replaced by simpler
         * shader program descriptors on Valhall, which can be preuploaded even
         * for fragment shaders. */
        bool upload = !(stage == MESA_SHADER_FRAGMENT && dev->arch <= 7);
        screen->vtbl.prepare_shader(state, desc_pool, upload);

        panfrost_analyze_sysvals(state);

        util_dynarray_fini(&binary);

        /* In both clone and tgsi_to_nir paths, the shader is ralloc'd against
         * a NULL context */
        ralloc_free(s);
}
