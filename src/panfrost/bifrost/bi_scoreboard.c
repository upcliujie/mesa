/*
 * Copyright (C) 2020 Collabora Ltd.
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

#include "compiler.h"

/* Assign dependency slots to each clause and calculate dependencies, This pass
 * must be run after scheduling.
 *
 * 1. A clause that does not produce a message must use the sentinel slot #0
 * 2a. A clause that depends on the results of a previous message-passing
 * instruction must depend on that instruction's dependency slot, unless all
 * reaching code paths already depended on it.
 * 2b. More generally, any dependencies must be encoded. This includes
 * Write-After-Write and Write-After-Read hazards with LOAD/STORE to memory.
 * 3. The shader must wait on slot #6 before running BLEND, ATEST
 * 4. The shader must wait on slot #7 before running BLEND, ST_TILE
 * 5. ATEST, ZS_EMIT must be issued with slot #0
 * 6. BARRIER must be issued with slot #7
 * 7. Only slots #0 through #5 may be used for clauses not otherwise specified.
 * 8. If a clause writes to a read staging register of an unresolved
 * dependency, it must set a staging barrier.
 *
 * Note it _is_ legal to reuse slots for multiple message passing instructions
 * with overlapping liveness, albeit with a slight performance penalty. As such
 * the problem is significantly easier than register allocation, rather than
 * spilling we may simply reuse slots. (TODO: does this have an optimal
 * linear-time solution).
 *
 * Within these constraints we are free to assign slots as we like. This pass
 * attempts to minimize stalls (TODO).
 */

#define BI_NUM_GENERAL_SLOTS 6
#define BI_NUM_SLOTS 8
#define BI_NUM_REGISTERS 64

/* A model for the state of the scoreboard */

struct bi_reg_state {
        /** Instruction that reads/writes this register */
        bi_instr *parent;

        /** Is this register read by a pending message-passing instruction? */
        bool read : 1;

        /** Is this register written by a pending instruction? */
        bool written : 1;

        /** Start register for the vector this is a part */
        unsigned start : 6;

        /** Number of registers for the vector this is a part of. */
        unsigned count : 4;

        /** Slot that produces this register */
        unsigned slot : 3;
};

struct bi_scoreboard_state {
        /** Number of pending instructions on a given slot */
        unsigned pending[BI_NUM_SLOTS];

        /** Map from registers to scoreboard metadata about the producer */
        struct bi_reg_state reg[BI_NUM_REGISTERS];
};

/* Given a scoreboard model, choose a slot for a clause wrapping a given
 * message passing instruction. No side effects. */

static unsigned
bi_choose_scoreboard_slot(struct bi_scoreboard_state *st, bi_instr *message)
{
        /* ATEST, ZS_EMIT must be issued with slot #0 */
        if (message->op == BI_OPCODE_ATEST || message->op == BI_OPCODE_ZS_EMIT)
                return 0;

        /* BARRIER must be issued with slot #7 */
        if (message->op == BI_OPCODE_BARRIER)
                return 7;

        /* Assign the slot with the fewest pending instructions */
        unsigned best = 0;

        for (unsigned i = 0; i < BI_NUM_GENERAL_SLOTS; ++i) {
                unsigned count = st->pending[i];

                if (count == 0) {
                        best = i;
                        break;
                }

                if (count < st->pending[best])
                        best = i;
        }

        return best;
}

static void
bi_push_scoreboard(struct bi_scoreboard_state *st, bi_instr *I, unsigned slot)
{
        /* Update the scoreboard state */
        st->pending[slot]++;

        bool sr_write = bi_opcode_props[I->op].sr_write;
        bool sr_read = bi_opcode_props[I->op].sr_read;

        if (sr_write && !bi_is_null(I->dest[0])) {
                assert(I->dest[0].type == BI_INDEX_REGISTER);

                unsigned reg = I->dest[0].value;
                unsigned count = bi_count_write_registers(I, 0);
                unsigned reads = sr_read ? bi_count_read_registers(I, 0) : 0;

                assert(count < 16);
                assert((reg + count) < BI_NUM_REGISTERS);

                for (unsigned i = reg; i < (reg + count); ++i) {
                        assert(!(st->reg[i].written || st->reg[i].read));
                        st->reg[i].parent = I;
                        st->reg[i].written = true;
                        st->reg[i].start = reg;
                        st->reg[i].count = count;
                        st->reg[i].slot = slot;
                }

                for (unsigned i = reg; i < reads; ++i) {
                        st->reg[i].parent = I;
                        st->reg[i].read = true;
                }
        } else if (sr_read && !bi_is_null(I->src[0])) {
                assert(I->src[0].type == BI_INDEX_REGISTER);

                unsigned reg = I->src[0].value;
                unsigned count = bi_count_read_registers(I, 0);

                assert(count < 16);
                assert((reg + count) < BI_NUM_REGISTERS);

                for (unsigned i = reg; i < (reg + count); ++i) {
                        st->reg[i].parent = I;
                        st->reg[i].read = true;
                        st->reg[i].start = reg;
                        st->reg[i].count = count;
                        st->reg[i].slot = slot;
                }

        }
}

static void
bi_clear_reads(bi_instr *parent, struct bi_scoreboard_state *st)
{
        if (!bi_opcode_props[parent->op].sr_read) return;
        if (bi_is_null(parent->src[0])) return;

        assert(parent->src[0].type == BI_INDEX_REGISTER);

        unsigned reg = parent->src[0].value;
        unsigned count = bi_count_read_registers(parent, 0);

        assert(count < 16);
        assert((reg + count) < BI_NUM_REGISTERS);

        for (unsigned i = reg; i < (reg + count); ++i) {
                assert(st->reg[i].parent == parent);

                st->reg[i].read = false;
        }
}

/** Adds a dependency on a given register. To do so, we must add a dependency
 * on the entire of vector of registers in which it is contained. */

static void
bi_depend_on_writer(bi_clause *clause, struct bi_scoreboard_state *st, bi_index index)
{
        if (index.type != BI_INDEX_REGISTER) return;
        if (!st->reg[index.value].written) return;

        unsigned reg = index.value;
        unsigned slot = st->reg[reg].slot;

        assert(st->pending[slot] > 0);

        /* Update the register state to mark the affected registers as ready */
        for (unsigned i = 0; i < st->reg[reg].count; ++i) {
                unsigned offs_reg = st->reg[reg].start + i;

                assert(st->reg[offs_reg].written);
                assert(st->reg[offs_reg].slot == slot);

                st->reg[offs_reg].written = false;
        }

        /* Any reads from the writer are now finished */
        bi_clear_reads(st->reg[reg].parent, st);

        /* Update the scoreboard state to pop the affected instruction off */
        st->pending[slot]--;

        /* Insert a dependency from the clause on the writer */
        clause->dependencies |= BITFIELD_BIT(slot);
}

static void
bi_depend_on_reader(bi_clause *clause, struct bi_scoreboard_state *st, bi_index index, unsigned count)
{
        if (index.type != BI_INDEX_REGISTER) return;

        for (unsigned i = index.value; i < index.value + count; ++i) {
                if (!st->reg[i].read) continue;

                unsigned slot = st->reg[i].slot;
                assert(st->pending[slot] > 0);

                bi_clear_reads(st->reg[i].parent, st);

                /* Do not set a dependency. Rather, set an outbound staging
                 * register barrier to force the read to finish */
                clause->staging_barrier = true;
        }
}

/* Sets the dependencies for a given clause, updating the model */

static void
bi_set_dependencies(bi_block *block, bi_clause *clause, struct bi_scoreboard_state *st)
{
        bi_foreach_instr_in_clause(block, clause, I) {
                /* Read-after-write */
                bi_foreach_src(I, s)
                        bi_depend_on_writer(clause, st, I->src[s]);

                /* Write-after-write */
                bi_foreach_dest(I, d)
                        bi_depend_on_writer(clause, st, I->dest[d]);

                /* Write-after-read */
                bi_foreach_dest(I, d) {
                        bi_depend_on_reader(clause, st, I->dest[d],
                                        bi_count_write_registers(I, d));
                }
        }
}

/* XXX: For conformance depend on everything at the end of a basic block since
 * we don't do the data flow analysis to scoreboard across branches yet */

static void
bi_depend_all(struct bi_scoreboard_state *st, bi_clause *clause)
{
        for (unsigned i = 0; i < BI_NUM_SLOTS; ++i) {
                if (st->pending[i] > 0) {
                        clause->dependencies |= BITFIELD_BIT(i);
                        st->pending[i] = 0;
                }
        }

        for (unsigned i = 0; i < BI_NUM_REGISTERS; ++i) {
                st->reg[i].read = false;
                st->reg[i].written = false;
        }
}

void
bi_assign_scoreboard(bi_context *ctx)
{
        struct bi_scoreboard_state st = {};

        /* Assign slots */
        bi_foreach_block(ctx, _block) {
                bi_block *block = (bi_block *) _block;

                bi_foreach_clause_in_block(block, clause) {
                        bi_set_dependencies(block, clause, &st);

                        if (clause->message) {
                                unsigned slot = bi_choose_scoreboard_slot(&st, clause->message);
                                bi_push_scoreboard(&st, clause->message, slot);
                                clause->scoreboard_id = slot;
                        }
                }

                /* XXX: Data flow analysis to track across bblocks? */
                bi_depend_all(&st,
                        list_last_entry(&block->clauses, bi_clause, link));
        }
}
