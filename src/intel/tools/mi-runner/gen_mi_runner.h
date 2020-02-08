/*
 * Copyright © 2020 Intel Corporation
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

#ifndef GEN_MI_RUNNER_H
#define GEN_MI_RUNNER_H

#include <stdint.h>

#include "common/gen_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gen_mi_bo {
   void *map;
   uint64_t gtt_offset;
   uint64_t size;
};

struct gen_mi_context {
   /* Ring/primary/secondary tracking. */
   uint64_t pc[3];
   bool     pc_as[3]; /* Address space (0 GGTT, 1 PPGTT) */
   uint32_t pc_depth; /* Index in the arrays above */

   struct {
      uint64_t src0;
      uint64_t src1;
      uint64_t data;
      uint64_t result;
   } predicate;

   struct {
      uint64_t src0;
      uint64_t src1;
      uint64_t accu;
      uint64_t cf;
      uint64_t zf;

      uint32_t inst_idx;
      uint32_t inst_count;
   } alu;

   union {
      uint64_t gpr64[16];
      uint32_t gpr32[16 * 2];
   };

   void *decoded_data;
   uint32_t decoded_data_len;

   /* Below are fields to be filled by caller. */
   struct gen_spec *spec;
   enum drm_i915_gem_engine_class engine;

   void *user_data;
   struct gen_mi_bo (*get_bo)(void *user_data, bool ppgtt, uint64_t address);
};

enum gen_mi_runner_status {
   GEN_MI_RUNNER_STATUS_OK,
   GEN_MI_RUNNER_STATUS_ERROR,
   GEN_MI_RUNNER_STATUS_FINISHED,
};

typedef enum gen_mi_runner_status (*mi_runner_exec)(struct gen_mi_context *);

enum gen_mi_runner_status gen7_mi_runner_execute_one_inst(struct gen_mi_context *ctx);
enum gen_mi_runner_status gen75_mi_runner_execute_one_inst(struct gen_mi_context *ctx);
enum gen_mi_runner_status gen8_mi_runner_execute_one_inst(struct gen_mi_context *ctx);
enum gen_mi_runner_status gen9_mi_runner_execute_one_inst(struct gen_mi_context *ctx);
enum gen_mi_runner_status gen10_mi_runner_execute_one_inst(struct gen_mi_context *ctx);
enum gen_mi_runner_status gen11_mi_runner_execute_one_inst(struct gen_mi_context *ctx);
enum gen_mi_runner_status gen12_mi_runner_execute_one_inst(struct gen_mi_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* GEN_MI_RUNNER_H */
