/*
 * Copyright © 2021 Google, Inc.
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

#include "freedreno_autotune.h"
#include "freedreno_batch.h"
#include "freedreno_util.h"


/**
 * Tracks, for a given batch key (which maps to a FBO/framebuffer state),
 *
 * ralloc parent is fd_autotune::ht
 */
struct fd_batch_history {
	struct fd_batch_key *key;

	unsigned num_results;

	/**
	 * List of recent fd_batch_result's
	 */
	struct list_head results;
#define MAX_RESULTS 5
};


static struct fd_batch_history *
get_history(struct fd_autotune *at, struct fd_batch *batch)
{
	if (!batch->key)
		return NULL;

	struct hash_entry *entry =
		_mesa_hash_table_search_pre_hashed(at->ht, batch->hash, batch->key);

	if (entry)
		return entry->data;

	struct fd_batch_history *history = rzalloc_size(at->ht, sizeof(*history));

	history->key = fd_batch_key_clone(history, batch->key);
	list_inithead(&history->results);

	_mesa_hash_table_insert_pre_hashed(at->ht, batch->hash, history->key, history);

	return history;
}

static struct fd_batch_result *
get_result(struct fd_autotune *at, struct fd_batch_history *history)
{
	struct fd_batch_result *result = rzalloc_size(history, sizeof(*result));

	// TODO if we have more pending results than there are result slots,
	// we need to pop one of the tail of at->pending_results

	result->fence = ++at->fence_counter; /* pre-increment so zero isn't valid fence */
	result->idx   = at->idx_counter++;

	if (at->idx_counter >= ARRAY_SIZE(at->results->result))
		at->idx_counter = 0;

	result->history = history;
	list_addtail(&result->node, &at->pending_results);

	return result;
}

static void
process_results(struct fd_autotune *at)
{
	uint32_t current_fence = at->results->fence;

	list_for_each_entry_safe (struct fd_batch_result, result, &at->pending_results, node) {
		if (result->fence > current_fence)
			break;

		struct fd_batch_history *history = result->history;

		result->samples_passed = at->results->result[result->idx].samples_end -
				at->results->result[result->idx].samples_start;

		list_delinit(&result->node);
		list_add(&result->node, &history->results);

		if (history->num_results < MAX_RESULTS) {
			history->num_results++;
		} else {
			/* Once above a limit, start popping old results off the
			 * tail of the list:
			 */
			struct fd_batch_result *old_result =
				list_last_entry(&history->results, struct fd_batch_result, node);
			list_del(&old_result->node);
			ralloc_free(old_result);
		}
	}
}

static bool
fallback_use_bypass(struct fd_batch *batch)
{
	struct pipe_framebuffer_state *pfb = &batch->framebuffer;

	/* Fallback logic if we have no historical data about the rendertarget: */
	// TODO we probably want a per-gen mask of gmem_reason's that actually
	// matter.. for ex, some gens can do depth/stencil in bypass and others
	// cannot

	if (batch->cleared || batch->gmem_reason ||
			((batch->num_draws > 5) && !batch->blit) ||
			(pfb->samples > 1)) {
		return false;
	}

	return true;
}

/**
 * A magic 8-ball that tells the gmem code whether we should do bypass mode
 * for moar fps.
 */
bool
fd_autotune_use_bypass(struct fd_autotune *at, struct fd_batch *batch)
{
	struct fd_batch_history *history = get_history(at, batch);

	process_results(at);

	if (!history)
		return fallback_use_bypass(batch);

	batch->autotune_result = get_result(at, history);
	batch->autotune_result->cost = batch->cost;

	bool use_bypass = fallback_use_bypass(batch);

	if (use_bypass)
		return true;

	if (history->num_results > 0) {
		uint32_t total_samples = 0;

		// TODO we should account for clears somehow
		// TODO should we try to notice if there is a drastic change from
		// frame to frame?
		list_for_each_entry (struct fd_batch_result, result, &history->results, node) {
			total_samples += result->samples_passed;
		}

		float avg_samples = (float)total_samples / (float)history->num_results;

		/* Low sample count could mean there was only a clear.. or there was
		 * a clear plus draws that touch no or few samples
		 */
		if (avg_samples < 500.0)
			return true;

		/* Cost-per-sample is an estimate for the average number of reads+
		 * writes for a given passed sample.
		 */
		float sample_cost = batch->cost;
		sample_cost /= batch->num_draws;

		float total_draw_cost = (avg_samples * sample_cost) / batch->num_draws;
		DBG("%08x:%u\ttotal_samples=%u, avg_samples=%f, sample_cost=%f, total_draw_cost=%f\n",
				batch->hash, batch->num_draws, total_samples, avg_samples, sample_cost, total_draw_cost);

		if (total_draw_cost < 3000.0)
			return true;
	}

	return use_bypass;
}

void
fd_autotune_init(struct fd_autotune *at, struct fd_device *dev)
{
	at->ht = _mesa_hash_table_create(NULL, fd_batch_key_hash, fd_batch_key_equals);
	at->results_mem = fd_bo_new(dev, sizeof(struct fd_autotune_results),
			DRM_FREEDRENO_GEM_TYPE_KMEM, "autotune");
	at->results = fd_bo_map(at->results_mem);
	list_inithead(&at->pending_results);
}

void
fd_autotune_fini(struct fd_autotune *at)
{
	_mesa_hash_table_destroy(at->ht, NULL);
	fd_bo_del(at->results_mem);
}
