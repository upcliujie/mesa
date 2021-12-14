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
#include "bi_builder.h"

/* This optimization pass, intended to run once after code emission but before
 * copy propagation, analyzes direct word-aligned UBO reads and promotes a
 * subset to moves from FAU. It is the sole populator of the UBO push data
 * structure returned back to the command stream. */

static bool
bi_is_ubo(bi_instr *ins)
{
        return (bi_opcode_props[ins->op].message == BIFROST_MESSAGE_LOAD) &&
                (ins->seg == BI_SEG_UBO);
}

static bool
bi_is_direct_aligned_ubo(bi_instr *ins)
{
        return bi_is_ubo(ins) &&
                (ins->src[0].type == BI_INDEX_CONSTANT) &&
                (ins->src[1].type == BI_INDEX_CONSTANT) &&
                ((ins->src[0].value & 0x3) == 0);
}

static enum bi_opcode
bi_word_sized_load(unsigned words)
{
        switch (words) {
        case 1: return BI_OPCODE_LOAD_I32;
        case 2: return BI_OPCODE_LOAD_I64;
        case 3: return BI_OPCODE_LOAD_I96;
        case 4: return BI_OPCODE_LOAD_I128;
        default: unreachable("Invalid number of words");
        }
}

/* Represents use data for a single UBO */

#define MAX_UBO_WORDS (65536 / 4)

struct bi_ubo_block {
        BITSET_DECLARE(pushed, MAX_UBO_WORDS);
        BITSET_DECLARE(accessed, MAX_UBO_WORDS);
};

struct bi_ubo_analysis {
        /* Per block analysis */
        unsigned nr_blocks;
        struct bi_ubo_block *blocks;
};

static struct bi_ubo_analysis
bi_analyze_ranges(bi_context *ctx)
{
        struct bi_ubo_analysis res = {
                .nr_blocks = ctx->nir->info.num_ubos + 1,
        };

        res.blocks = calloc(res.nr_blocks, sizeof(struct bi_ubo_block));

        bi_foreach_instr_global(ctx, ins) {
                if (!bi_is_direct_aligned_ubo(ins)) continue;

                unsigned ubo = ins->src[1].value;
                unsigned word = ins->src[0].value / 4;
                unsigned channels = bi_opcode_props[ins->op].sr_count;

                assert(ubo < res.nr_blocks);
                assert(channels > 0 && channels <= 4);

                if ((word + channels) > MAX_UBO_WORDS) continue;

                for (unsigned i = 0; i < channels; ++i)
                        BITSET_SET(res.blocks[ubo].accessed, word + i);
        }

        return res;
}

/* Select UBO words to push. A sophisticated implementation would consider the
 * number of uses and perhaps the control flow to estimate benefit. This is not
 * sophisticated. Select from the last UBO first to prioritize sysvals. */

static void
bi_pick_ubo(struct panfrost_ubo_push *push, struct bi_ubo_analysis *analysis)
{
        /* When IDVS is used, the push analysis runs for each variant, first
         * for position shading and second for varying shading. On Bifrost, the
         * same push buffer is used for both position and varying shading. We
         * don't want to push a uniform twice if it is used in both position
         * and varying shaders, so we first iterate over what was already
         * pushed and mark it as pushed to be ignored in our analysis.
         */
        for (unsigned i = 0; i < push->count; ++i) {
                unsigned ubo = push->words[i].ubo;
                unsigned offset = push->words[i].offset;

                assert(ubo < analysis->nr_blocks);
                assert((offset & 3) == 0);
                assert((offset / 4) < MAX_UBO_WORDS);

                BITSET_SET(analysis->blocks[ubo].pushed, offset / 4);
        }

        for (signed ubo = analysis->nr_blocks - 1; ubo >= 0; --ubo) {
                struct bi_ubo_block *block = &analysis->blocks[ubo];

                for (unsigned r = 0; r < MAX_UBO_WORDS; ++r) {
                        /* Don't push more than possible */
                        if (push->count == PAN_MAX_PUSH)
                                return;

                        if (!BITSET_TEST(block->accessed, r))
                                continue;

                        if (BITSET_TEST(block->pushed, r))
                                continue;

                        push->words[push->count++] = (struct panfrost_ubo_word) {
                                .ubo = ubo,
                                .offset = r * 4
                        };

                        /* Mark it as pushed so we can rewrite */
                        BITSET_SET(block->pushed, r);
                }
        }
}

/**
 * Given a load from <ubo, [offset, offset + channels)>, determine which
 * components are pushed. If no components are pushed, returns 0 and no
 * rewriting should proceed. If all components are pushed, returns
 * BITFIELD_MASK(channels) and the load should be removed. Other values
 * correpsond to partial pushes.
 */
static uint8_t
bi_push_mask(struct bi_ubo_analysis *analysis, unsigned ubo, unsigned offset,
             unsigned channels)
{
        uint8_t mask = 0;

        for (unsigned i = 0; i < channels; ++i) {
                if (BITSET_TEST(analysis->blocks[ubo].pushed, (offset / 4) + i))
                        mask |= BITFIELD_BIT(i);
        }

        return mask;
}

void
bi_opt_push_ubo(bi_context *ctx)
{
        struct bi_ubo_analysis analysis = bi_analyze_ranges(ctx);
        bi_pick_ubo(&ctx->info->push, &analysis);

        ctx->ubo_mask = 0;

        bi_foreach_instr_global_safe(ctx, ins) {
                if (!bi_is_ubo(ins)) continue;

                unsigned ubo = ins->src[1].value;
                unsigned offset = ins->src[0].value;

                if (!bi_is_direct_aligned_ubo(ins)) {
                        /* The load can't be pushed, so this UBO needs to be
                         * uploaded conventionally */
                        if (ins->src[1].type == BI_INDEX_CONSTANT)
                                ctx->ubo_mask |= BITSET_BIT(ubo);
                        else
                                ctx->ubo_mask = ~0;

                        continue;
                }

                assert(ubo < analysis.nr_blocks);

                unsigned channels = bi_opcode_props[ins->op].sr_count;
                unsigned push_mask = bi_push_mask(&analysis, ubo, offset, channels);
                unsigned load_mask = push_mask ^ BITFIELD_MASK(channels);

                /* Skip unpushed instructions */
                if (!push_mask) {
                        ctx->ubo_mask |= BITSET_BIT(ubo);
                        continue;
                }

                /* Replace the UBO load with moves from FAU */
                bi_builder b = bi_init_builder(ctx, bi_after_instr(ins));

                /* Replace pushed components with moves from FAU */
                u_foreach_bit(w, push_mask) {
                        unsigned base =
                                pan_lookup_pushed_ubo(&ctx->info->push, ubo,
                                                      (offset + 4 * w));

                        /* FAU is grouped in pairs (2 x 4-byte) */
                        unsigned fau_idx = (base >> 1);
                        unsigned fau_hi = (base & 1);

                        bi_mov_i32_to(&b,
                                bi_word(ins->dest[0], w),
                                bi_fau(BIR_FAU_UNIFORM | fau_idx, fau_hi));
                }

                if (!load_mask) {
                        bi_remove_instruction(ins);
                        continue;
                }

                /* Shrink the original load */
                unsigned first_channel = ffs(load_mask) - 1;
                unsigned last_channel = util_last_bit(load_mask);
                unsigned new_channels = last_channel - first_channel;

                ins->op = bi_word_sized_load(new_channels);
                ins->src[0].value += (first_channel * 4);

                /* Copy unpushed components to maintain SSA form */
                bi_index dest = ins->dest[0];
                ins->dest[0] = bi_temp(ctx);

                u_foreach_bit(w, load_mask) {
                        bi_mov_i32_to(&b,
                                bi_word(dest, w),
                                bi_word(ins->dest[0], w - first_channel));
                }
        }

        free(analysis.blocks);
}
