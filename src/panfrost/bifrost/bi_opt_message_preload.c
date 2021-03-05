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
#include "midgard_pack.h"

/* Bifrost v7 can preload up to two messages of the form:
 *
 * 1. +LD_VAR_IMM, register_format f32/f16, sample mode
 * 2. +VAR_TEX, register format f32/f16, sample mode (TODO)
 *
 * Analyze the shader for these instructions and push accordingly.
 */

static bool
bi_is_regfmt_float(enum bi_register_format regfmt)
{
        return (regfmt == BI_REGISTER_FORMAT_F32) ||
                (regfmt == BI_REGISTER_FORMAT_F16);
}

static enum mali_message_preload_register_format
bi_map_regfmt(enum bi_register_format regfmt)
{
        return (regfmt == BI_REGISTER_FORMAT_F32) ?
                MALI_MESSAGE_PRELOAD_REGISTER_FORMAT_F32 :
                MALI_MESSAGE_PRELOAD_REGISTER_FORMAT_F16;
}

static bool
bi_can_preload_ld_var(bi_instr *instr)
{
        return (instr->op == BI_OPCODE_LD_VAR_IMM) &&
                (instr->sample == BI_SAMPLE_SAMPLE) &&
                bi_is_regfmt_float(instr->register_format);
}

static uint16_t
bi_preload_ld_var(bi_instr *instr)
{
        uint32_t raw = 0;

        pan_pack(&raw, MESSAGE_PRELOAD, msg) {
                msg.type = MALI_MESSAGE_TYPE_LD_VAR;
                msg.ld_var.varying_index = instr->varying_index;
                msg.ld_var.register_format =
                        bi_map_regfmt(instr->register_format);
                msg.ld_var.num_components = instr->vecsize + 1;
        }

        return raw;
}

void
bi_opt_message_preload(bi_context *ctx)
{
        unsigned nr_preload = 0;
        bi_index preload[2] = { bi_null() };

        bi_foreach_instr_global(ctx, ins) {
                bi_foreach_src(ins, s) {
                        bi_index use = ins->src[s];
                        if (!bi_is_ssa(use)) continue;

                        for (unsigned i = 0; i < nr_preload; ++i) {
                                if (bi_is_equiv(use, preload[i])) {
                                        ins->src[s] = bi_replace_index(use,
                                                        bi_register(4*i + use.offset));
                                        break;
                                }
                        }
                }

                /* TODO: generalize? */
                if (nr_preload == 2) continue;
                if (!bi_is_ssa(ins->dest[0])) continue;
                if (ins->dest[0].offset) continue;

                uint16_t msg = 0;

                if (bi_can_preload_ld_var(ins))
                        msg = bi_preload_ld_var(ins);
                else
                        continue;

                /* Report the preloading */
                ctx->info->bifrost.messages[nr_preload] = msg;
                preload[nr_preload++] = ins->dest[0];
        }
}
