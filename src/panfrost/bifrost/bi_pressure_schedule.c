/*
 * Copyright (C) 2022 Collabora Ltd.
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
 * Authors (Collabora):
 *      Alyssa Rosenzweig <alyssa.rosenzweig@collabora.com>
 */

/* Bottom-up local scheduler to reduce register pressure */

#include "compiler.h"
#include "util/dag.h"

struct sched_ctx {
        /* Dependency graph */
        struct dag *dag;

        /* Live set */
        uint8_t *live;

        /* Size of the live set */
        unsigned max;
};

struct sched_node {
        struct dag_node dag;

        /* Link in temporary schedule */
        struct list_head link;
        bi_instr *instr;
};

static unsigned
label_index(bi_context *ctx, bi_index idx)
{
        if (idx.reg)
                return idx.value + ctx->ssa_alloc;
        else
                return idx.value;
}

static void
add_dep(struct sched_node *a, struct sched_node *b)
{
        if (a && b)
                dag_add_edge(&a->dag, &b->dag, 0);
}

static struct dag *
create_dag(bi_context *ctx, bi_block *block)
{
        struct dag *dag = dag_create(ctx);

        unsigned count = ctx->ssa_alloc + ctx->reg_alloc;
        struct sched_node **last_read =
                rzalloc_array(ctx, struct sched_node *, count);
        struct sched_node **last_write =
                rzalloc_array(ctx, struct sched_node *, count);
        struct sched_node *coverage = NULL;
        struct sched_node *memory = NULL;
        struct sched_node *preload = NULL;

        bi_foreach_instr_in_block(block, I) {
                /* Leave branches at the end */
                bool is_jump = false;

                switch (I->op) {
                case BI_OPCODE_BRANCHZ_I16:
                case BI_OPCODE_JUMP:
                        is_jump = true;
                        break;
                default:
                        assert(I->branch_target == NULL);
                }

                if (is_jump)
                        break;

                struct sched_node *node = rzalloc(ctx, struct sched_node);
                node->instr = I;
                dag_init_node(dag, &node->dag);

                /* Reads depend on writes */
                bi_foreach_src(I, s) {
                        bi_index src = I->src[s];

                        if (src.type == BI_INDEX_NORMAL) {
                                add_dep(node, last_write[label_index(ctx, src)]);

                                /* Serialize access to nir_register for
                                 * simplicity. We could do better.
                                 */
                                if (src.reg)
                                        add_dep(node, last_read[label_index(ctx, src)]);
                        }
                }

                /* Writes depend on reads and writes */
                bi_foreach_dest(I, s) {
                        bi_index dest = I->dest[s];

                        if (dest.type == BI_INDEX_NORMAL) {
                                add_dep(node, last_read[label_index(ctx, dest)]);
                                add_dep(node, last_write[label_index(ctx, dest)]);
                        }
                }

                bi_foreach_dest(I, s) {
                        bi_index dest = I->dest[s];

                        if (dest.type == BI_INDEX_NORMAL) {
                                last_write[label_index(ctx, dest)] = node;
                        }
                }

                bi_foreach_src(I, s) {
                        bi_index src = I->src[s];

                        if (src.type == BI_INDEX_NORMAL) {
                                last_read[label_index(ctx, src)] = node;
                        }
                }

                switch (bi_opcode_props[I->op].message) {
                case BIFROST_MESSAGE_LOAD:
                case BIFROST_MESSAGE_STORE:
                case BIFROST_MESSAGE_ATOMIC:
                case BIFROST_MESSAGE_BARRIER:
                        add_dep(node, memory);
                        memory = node;
                        break;

                case BIFROST_MESSAGE_BLEND:
                case BIFROST_MESSAGE_Z_STENCIL:
                case BIFROST_MESSAGE_TILE:
                        add_dep(node, coverage);
                        coverage = node;
                        break;

                case BIFROST_MESSAGE_ATEST:
                        /* If early fragment tests are forced, we can move ATEST
                         * before memory access, potentially skipping the memory
                         * access if the pixel is killed.
                         *
                         * If early fragment tests are *not* forced, memory
                         * access needs to stay before the ATEST to happen.
                         */
                        if (!ctx->nir->info.fs.early_fragment_tests) {
                                add_dep(node, memory);
                                memory = node;
                        }

                        /* ATEST also updates coverage */
                        add_dep(node, coverage);
                        coverage = node;
                        break;
                default:
                        break;
                }

                if (I->op == BI_OPCODE_DISCARD_F32) {
                        add_dep(node, coverage);
                        coverage = node;
                } else if (I->op == BI_OPCODE_MOV_I32 && I->src[0].type == BI_INDEX_REGISTER) {
                        add_dep(node, preload);
                        preload = node;
                }

                if (preload != node)
                        add_dep(node, preload);
        }

        return dag;
}

static signed
estimate_pressure_delta(bi_instr *I, uint8_t *live, unsigned max)
{
        signed estimate = 0;

        /* live_in[s] = GEN[s] + (live_out[s] - KILL[s]) */

        /* Destinations must be unique */
        bi_foreach_dest(I, d) {
                unsigned node = bi_get_node(I->dest[d]);
                if (node >= max)
                        continue;

                if (live[node])
                        estimate -= bi_count_write_registers(I, d);
        }

        bi_foreach_src(I, src) {
                unsigned node = bi_get_node(I->src[src]);
                if (node >= max)
                        continue;

                /* Filter duplicates */
                bool dupe = false;

                for (unsigned i = 0; i < src; ++i) {
                        if (bi_get_node(I->src[i]) == node) {
                                dupe = true;
                                break;
                        }
                }

                if (dupe)
                        continue;

                if (!live[node])
                        estimate += bi_count_read_registers(I, src);
        }

        return estimate;
}

static struct sched_node *
choose_instr(struct sched_ctx *s)
{
        int32_t min_delta = INT32_MAX;
        struct sched_node *best = NULL;

        list_for_each_entry(struct sched_node, n, &s->dag->heads, dag.link) {
                /* Estimate impact on liveness */
                int16_t delta = estimate_pressure_delta(n->instr, s->live, s->max);

                if ((delta < min_delta) || !best) {
                        best = n;
                        min_delta = delta;
                }
        }

        return best;
}

static void
pressure_schedule_block(bi_context *ctx, bi_block *block, struct sched_ctx *s)
{
        struct list_head schedule;
        list_inithead(&schedule);

        /* off by a constant, that's ok */
        signed pressure = 0;
        signed orig_max_pressure = 0;

        memcpy(s->live, block->live_out, s->max);

        bi_foreach_instr_in_block_rev(block, I) {
                pressure += estimate_pressure_delta(I, s->live, s->max);
                orig_max_pressure = MAX2(pressure, orig_max_pressure);
                bi_liveness_ins_update(s->live, I, s->max);
        }

        memcpy(s->live, block->live_out, s->max);

        /* off by a constant, that's ok */
        signed max_pressure = 0;
        pressure = 0;

        while (!list_is_empty(&s->dag->heads)) {
                struct sched_node *node = choose_instr(s);
                pressure += estimate_pressure_delta(node->instr, s->live, s->max);
                max_pressure = MAX2(pressure, max_pressure);
                dag_prune_head(s->dag, &node->dag);

                /* Add to start */
                list_addtail(&node->link, &schedule);
                bi_liveness_ins_update(s->live, node->instr, s->max);
        }

        /* Bail if it looks like it's worse */
        if (max_pressure >= orig_max_pressure)
                return;

        /* Apply the schedule */
        list_for_each_entry(struct sched_node, n, &schedule, link) {
                bi_remove_instruction(n->instr);
                list_add(&n->instr->link, &block->instructions);
        }
}

void
bi_pressure_schedule(bi_context *ctx)
{
        bi_compute_liveness(ctx);
        unsigned temp_count = bi_max_temp(ctx);
        uint8_t *live = rzalloc_array(ctx, uint8_t, temp_count);

        bi_foreach_block(ctx, block) {
                struct sched_ctx sctx = {
                        .dag = create_dag(ctx, block),
                        .max = temp_count,
                        .live = live
                };

                pressure_schedule_block(ctx, block, &sctx);
        }
}
