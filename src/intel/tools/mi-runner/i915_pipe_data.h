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

#ifndef I915_PIPE_DATA_H
#define I915_PIPE_DATA_H

#include <stdint.h>

enum i915_pipe_msg_type {
   I915_PIPE_MSG_TYPE_BO,
   I915_PIPE_MSG_TYPE_EXECBUF,
   I915_PIPE_MSG_TYPE_EXECBUF_RESULT,

};

struct i915_pipe_base_msg {
   uint32_t type;
   uint32_t size;
};

struct i915_pipe_bo_msg {
   struct i915_pipe_base_msg base;
   uint64_t mem_addr;
   uint64_t gtt_offset;
   uint64_t size;
};

struct i915_pipe_execbuf_msg {
   struct i915_pipe_base_msg base;
   uint64_t gtt_offset;
   uint32_t ctx_id;
};

struct i915_pipe_execbuf_result_msg {
   struct i915_pipe_base_msg base;
   int32_t result;
};

#endif /* I915_PIPE_DATA_H */
