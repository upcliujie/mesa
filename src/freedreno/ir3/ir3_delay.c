/*
 * Copyright (C) 2019 Google, Inc.
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
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#include "ir3.h"

/*
 * Helpers to figure out the necessary delay slots between instructions.  Used
 * both in scheduling pass(es) and the final pass to insert any required nop's
 * so that the shader program is valid.
 *
 * Note that this needs to work both pre and post RA, so we can't assume ssa
 * src iterators work.
 */

/* calculate required # of delay slots between the instruction that
 * assigns a value and the one that consumes
 */
int
ir3_delayslots(struct ir3_instruction *assigner,
		struct ir3_instruction *consumer, unsigned n, bool soft)
{
	/* generally don't count false dependencies, since this can just be
	 * something like a barrier, or SSBO store.
	 */
	if (__is_false_dep(consumer, n))
		return 0;

	/* worst case is cat1-3 (alu) -> cat4/5 needing 6 cycles, normal
	 * alu -> alu needs 3 cycles, cat4 -> alu and texture fetch
	 * handled with sync bits
	 */

	if (is_meta(assigner) || is_meta(consumer))
		return 0;

	if (writes_addr0(assigner) || writes_addr1(assigner))
		return 6;

	/* On a6xx, it takes the number of delay slots to get a SFU result
	 * back (ie. using nop's instead of (ss) is:
	 *
	 *     8 - single warp
	 *     9 - two warps
	 *    10 - four warps
	 *
	 * and so on.  Not quite sure where it tapers out (ie. how many
	 * warps share an SFU unit).  But 10 seems like a reasonable #
	 * to choose:
	 */
	if (soft && is_sfu(assigner))
		return 10;

	/* handled via sync flags: */
	if (is_sfu(assigner) || is_tex(assigner) || is_mem(assigner))
		return 0;

	if (assigner->opc == OPC_MOVMSK)
		return 4;

	/* As far as we know, shader outputs don't need any delay. */
	if (consumer->opc == OPC_END || consumer->opc == OPC_CHMASK)
		return 0;

	/* assigner must be alu: */
	if (is_flow(consumer) || is_sfu(consumer) || is_tex(consumer) ||
			is_mem(consumer)) {
		return 6;
	} else {
		/* assigner and consumer are both alu */
		assert(n > 0);

		/* In mergedregs mode, there is an extra 2-cycle penalty when half of
		 * a full-reg is read as a half-reg, and a 1-cycle penalty when a
		 * half-reg is read as a full-reg.
		 */
		bool half_to_full =
			(assigner->regs[0]->flags & IR3_REG_HALF) &&
			!(consumer->regs[n - 1]->flags & IR3_REG_HALF);
		bool full_to_half =
			!(assigner->regs[0]->flags & IR3_REG_HALF) &&
			(consumer->regs[n - 1]->flags & IR3_REG_HALF);
		unsigned penalty = 0;
		if (half_to_full)
			penalty = 1;
		else if (full_to_half)
			penalty = 2;
		if ((is_mad(consumer->opc) || is_madsh(consumer->opc)) &&
			(n == 3)) {
			/* special case, 3rd src to cat3 not required on first cycle */
			return 1 + penalty;
		} else {
			return 3 + penalty;
		}
	}
}

/* Post-RA, we don't have arrays any more, so we have to be a bit careful here
 * and have to handle relative accesses specially.
 */

static unsigned
post_ra_reg_elems(struct ir3_register *reg)
{
	if (reg->flags & IR3_REG_RELATIV)
		return reg->size;
	return reg_elems(reg);
}

static unsigned
post_ra_reg_num(struct ir3_register *reg)
{
	if (reg->flags & IR3_REG_RELATIV)
		return reg->array.base;
	return reg->num;
}

static bool
regs_interfere(struct ir3_register *assigner, struct ir3_register *consumer,
			   unsigned consumer_n, bool mergedregs)
{
	if (consumer->flags & IR3_REG_SSA) {
		return consumer->def == assigner;
	}

	if (!mergedregs &&
		(consumer->flags & IR3_REG_HALF) != (assigner->flags & IR3_REG_HALF))
		return false;

	unsigned consumer_start = post_ra_reg_num(consumer) * ir3_reg_elem_size(consumer);
	unsigned consumer_end = consumer_start + post_ra_reg_elems(consumer) * ir3_reg_elem_size(consumer);
	unsigned assigner_start = post_ra_reg_num(assigner) * ir3_reg_elem_size(assigner);
	unsigned assigner_end = assigner_start + post_ra_reg_elems(assigner) * ir3_reg_elem_size(assigner);

	if (assigner_start >= consumer_end || consumer_start >= assigner_end)
		return false;

	/* TODO compute delayslot offset due to repeat here */

	return true;
}

static bool
count_instruction(struct ir3_instruction *n)
{
	/* NOTE: don't count branch/jump since we don't know yet if they will
	 * be eliminated later in resolve_jumps().. really should do that
	 * earlier so we don't have this constraint.
	 */
	return is_alu(n) || (is_flow(n) && (n->opc != OPC_JUMP) && (n->opc != OPC_B));
}

static unsigned
delay_calc(struct ir3_block *block,
		   struct ir3_instruction *start,
		   struct ir3_instruction *orig_consumer,
		   unsigned orig_consumer_n,
		   struct ir3_instruction *cur_consumer,
		   unsigned distance, bool soft, bool pred, bool mergedregs)
{
	unsigned delay = 0;
	/* Search backwards starting at the instruction before start, unless it's
	 * NULL then search backwards from the block end.
	 */
	struct list_head *start_list = start ? start->node.prev : block->instr_list.prev;
	list_for_each_entry_from_rev(struct ir3_instruction, assigner, start_list, &block->instr_list, node) {
		if (count_instruction(assigner))
			distance += assigner->nop;

		if (distance >= (soft ? 10 : 6))
			return delay;

		if (assigner->opc == OPC_META_SPLIT || assigner->opc == OPC_META_COLLECT) {
			struct ir3_instruction *consumer = orig_consumer ? orig_consumer : cur_consumer;
			foreach_src_n (src, n, cur_consumer) {
				if ((src->flags & IR3_REG_SSA) && src->def->instr == assigner) {
					unsigned consumer_n = orig_consumer ? orig_consumer_n : n;
					unsigned new_delay =
						delay_calc(block, assigner, consumer, consumer_n,
								assigner, distance, soft, pred, mergedregs);
					delay = MAX2(delay, new_delay);
				}
			}
		} else if (!is_meta(assigner)) {
			unsigned new_delay = 0;
			struct ir3_instruction *consumer = orig_consumer ? orig_consumer : cur_consumer;

			if (consumer->address == assigner)
				new_delay = MAX2(new_delay, ir3_delayslots(assigner, consumer, 0, soft));

			if (dest_regs(assigner) != 0) {
				foreach_src_n (src, n, cur_consumer) {
					if (src->flags & (IR3_REG_IMMED | IR3_REG_CONST))
						continue;

					if (!regs_interfere(assigner->regs[0], src, n, mergedregs))
						continue;

					unsigned consumer_n = orig_consumer ? orig_consumer_n : n;
					new_delay = MAX2(new_delay, ir3_delayslots(assigner, consumer, consumer_n + 1, soft));
				}
			}

			new_delay = new_delay > distance ? new_delay - distance : 0;
			delay = MAX2(delay, new_delay);
		}

		if (count_instruction(assigner))
			distance += 1 + assigner->repeat;

		if (distance >= (soft ? 10 : 6))
			return delay;
	}

	/* Note: this allows recursion into "block" if it has already been
	 * visited, but *not* recursion into its predecessors. We may have to
	 * visit the original block twice, for the loop case where we have to
	 * consider definititons in an earlier iterations of the same loop:
	 *
	 * while (...) {
	 *		mov.u32u32 ..., r0.x
	 *		...
	 *		mov.u32u32 r0.x, ...
	 * }
	 *
	 * However any other recursion would be unnecessary.
	 */

	if (pred && block->data != block) {
		block->data = block;

		for (unsigned i = 0; i < block->predecessors_count; i++) {
			struct ir3_block *pred = block->predecessors[i];
			unsigned pred_delay =
				delay_calc(pred, NULL, orig_consumer, orig_consumer_n,
						   cur_consumer, distance, soft, pred, mergedregs);
			delay = MAX2(delay, pred_delay);
		}

		block->data = NULL;
	}

	return delay;
}

/**
 * Calculate delay for instruction (maximum of delay for all srcs):
 *
 * @soft:  If true, add additional delay for situations where they
 *    would not be strictly required because a sync flag would be
 *    used (but scheduler would prefer to schedule some other
 *    instructions first to avoid stalling on sync flag)
 * @pred:  If true, recurse into predecessor blocks
 */
unsigned
ir3_delay_calc(struct ir3_block *block, struct ir3_instruction *instr,
		bool soft, bool pred, bool mergedregs)
{
	return delay_calc(block, NULL, NULL, 0, instr, 0, soft, pred, mergedregs);
}

/**
 * Remove nop instructions.  The scheduler can insert placeholder nop's
 * so that ir3_delay_calc() can account for nop's that won't be needed
 * due to nop's triggered by a previous instruction.  However, before
 * legalize, we want to remove these.  The legalize pass can insert
 * some nop's if needed to hold (for example) sync flags.  This final
 * remaining nops are inserted by legalize after this.
 */
void
ir3_remove_nops(struct ir3 *ir)
{
	foreach_block (block, &ir->block_list) {
		foreach_instr_safe (instr, &block->instr_list) {
			if (instr->opc == OPC_NOP) {
				list_del(&instr->node);
			}
		}
	}

}
