/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 * Copyright 2021 Valve Corporation
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
 *
 */

#include "ac_nir.h"
#include "nir_builder.h"

/* This code is adapted from ac_llvm_cull.c, hence the copyright to AMD. */

typedef struct
{
   nir_ssa_def *w_reflection;
   nir_ssa_def *w_accepted;
   nir_ssa_def *all_w_positive;
   nir_ssa_def *any_w_negative;
} position_w_info;

static void
analyze_position_w(nir_builder *b, nir_ssa_def *pos[3][4], position_w_info *w_info)
{
   nir_ssa_def *all_w_negative = nir_imm_bool(b, true);

   w_info->w_reflection = nir_imm_bool(b, false);
   w_info->any_w_negative = nir_imm_bool(b, false);

   for (unsigned i = 0; i < 3; ++i) {
      nir_ssa_def *neg_w = nir_flt(b, pos[i][3], nir_imm_float(b, 0.0f));
      w_info->w_reflection = nir_ixor(b, neg_w, w_info->w_reflection);
      w_info->any_w_negative = nir_ior(b, neg_w, w_info->any_w_negative);
      all_w_negative = nir_iand(b, neg_w, all_w_negative);
   }

   w_info->all_w_positive = nir_inot(b, w_info->any_w_negative);
   w_info->w_accepted = nir_inot(b, all_w_negative);
}

static nir_ssa_def *
cull_face(nir_builder *b, nir_ssa_def *pos[3][4], const position_w_info *w_info)
{
   nir_ssa_def *det_t0 = nir_fsub(b, pos[2][0], pos[0][0]);
   nir_ssa_def *det_t1 = nir_fsub(b, pos[1][1], pos[0][1]);
   nir_ssa_def *det_t2 = nir_fsub(b, pos[0][0], pos[1][0]);
   nir_ssa_def *det_t3 = nir_fsub(b, pos[0][1], pos[2][1]);
   nir_ssa_def *det_p0 = nir_fmul(b, det_t0, det_t1);
   nir_ssa_def *det_p1 = nir_fmul(b, det_t2, det_t3);
   nir_ssa_def *det = nir_fsub(b, det_p0, det_p1);

   det = nir_bcsel(b, w_info->w_reflection, nir_fneg(b, det), det);

   nir_ssa_def *cull_front = nir_build_load_cull_front_face_enabled_amd(b);
   nir_ssa_def *front_facing = nir_flt(b, det, nir_imm_float(b, 0.0f));
   nir_ssa_def *front_accepted = nir_bcsel(b, cull_front, nir_imm_false(b), front_facing);

   nir_ssa_def *cull_back = nir_build_load_cull_back_face_enabled_amd(b);
   nir_ssa_def *back_facing = nir_flt(b, nir_imm_float(b, 0.0f), det);
   nir_ssa_def *back_accepted = nir_bcsel(b, cull_back, nir_imm_false(b), back_facing);

   return nir_ior(b, front_accepted, back_accepted);
}

nir_ssa_def *
ac_nir_cull_triangle(nir_builder *b,
                     nir_ssa_def *initially_accepted,
                     nir_ssa_def *pos[3][4])
{
   position_w_info w_info = {0};
   analyze_position_w(b, pos, &w_info);

   nir_ssa_def *accepted = initially_accepted;
   accepted = nir_iand(b, accepted, w_info.w_accepted);
   accepted = nir_iand(b, accepted, cull_face(b, pos, &w_info));

   /* TODO: copy cull_bbox to implement other culling options */

   return accepted;
}
