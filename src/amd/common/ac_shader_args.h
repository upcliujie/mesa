/*
 * Copyright 2019 Valve Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef AC_SHADER_ARGS_H
#define AC_SHADER_ARGS_H

#include <stdbool.h>
#include <stdint.h>

#define AC_MAX_INLINE_PUSH_CONSTS 8

enum ac_arg_regfile
{
   AC_ARG_SGPR,
   AC_ARG_VGPR,
};

enum ac_arg_type
{
   AC_ARG_FLOAT,
   AC_ARG_INT,
   AC_ARG_CONST_PTR,       /* Pointer to i8 array */
   AC_ARG_CONST_FLOAT_PTR, /* Pointer to f32 array */
   AC_ARG_CONST_PTR_PTR,   /* Pointer to pointer to i8 array */
   AC_ARG_CONST_DESC_PTR,  /* Pointer to v4i32 array */
   AC_ARG_CONST_IMAGE_PTR, /* Pointer to v8i32 array */
};

struct ac_arg {
   uint16_t arg_index;
   bool used;
};

#define AC_MAX_ARGS 384 /* including all VS->TCS IO */

struct ac_shader_args {
   /* Info on how to declare arguments */
   struct {
      enum ac_arg_type type;
      enum ac_arg_regfile file;
      uint8_t offset;
      uint8_t size;
      bool skip;
   } args[AC_MAX_ARGS];

   uint16_t arg_count;
   uint16_t num_sgprs_used;
   uint16_t num_vgprs_used;

   uint16_t return_count;
   uint16_t num_sgprs_returned;
   uint16_t num_vgprs_returned;

   struct ac_arg base_vertex;
   struct ac_arg start_instance;
   struct ac_arg draw_id;
   struct ac_arg vertex_id;
   struct ac_arg instance_id;
   struct ac_arg tcs_patch_id;
   struct ac_arg tcs_rel_ids;
   struct ac_arg tes_patch_id;
   struct ac_arg gs_prim_id;
   struct ac_arg gs_invocation_id;

   /* PS */
   struct ac_arg frag_pos[4];
   struct ac_arg front_face;
   struct ac_arg ancillary;
   struct ac_arg sample_coverage;
   struct ac_arg prim_mask;
   struct ac_arg persp_sample;
   struct ac_arg persp_center;
   struct ac_arg persp_centroid;
   struct ac_arg pull_model;
   struct ac_arg linear_sample;
   struct ac_arg linear_center;
   struct ac_arg linear_centroid;

   /* CS */
   struct ac_arg local_invocation_ids;
   struct ac_arg num_work_groups;
   struct ac_arg workgroup_ids[3];
   struct ac_arg tg_size;

   /* Vulkan only */
   struct ac_arg push_constants;
   struct ac_arg inline_push_consts[AC_MAX_INLINE_PUSH_CONSTS];
   unsigned num_inline_push_consts;
   unsigned base_inline_push_consts;
   struct ac_arg view_index;

   /*************************/
   /*  RadeonSI args begin  */
   /*************************/

   struct ac_arg const_and_shader_buffers;
   struct ac_arg samplers_and_images;

   /* For merged shaders, the per-stage descriptors for the stage other
    * than the one we're processing, used to pass them from the
    * first stage to the second.
    */
   struct ac_arg other_const_and_shader_buffers;
   struct ac_arg other_samplers_and_images;

   struct ac_arg rw_buffers;
   struct ac_arg bindless_samplers_and_images;
   /* Common inputs for merged shaders. */
   struct ac_arg merged_wave_info;
   struct ac_arg merged_scratch_offset;
   struct ac_arg small_prim_cull_info;
   /* API VS */
   struct ac_arg vertex_buffers;
   struct ac_arg vb_descriptors[5];
   struct ac_arg rel_auto_id;
   struct ac_arg vs_prim_id;
   struct ac_arg vertex_index0;
   /* VS states and layout of LS outputs / TCS inputs at the end
    *   [0] = clamp vertex color
    *   [1] = indexed
    *   [2:3] = NGG: output primitive type
    *   [4:5] = NGG: provoking vertex index
    *   [6]   = NGG: streamout queries enabled
    *   [7:10] = NGG: small prim filter precision = num_samples / quant_mode,
    *            but in reality it's: 1/2^n, from 1/16 to 1/4096 = 1/2^4 to 1/2^12
    *            Only the first 4 bits of the exponent are stored.
    *            Set it like this: (fui(num_samples / quant_mode) >> 23)
    *            Expand to FP32 like this: ((0x70 | value) << 23);
    *            With 0x70 = 112, we get 2^(112 + value - 127) = 2^(value - 15)
    *            = 1/2^(15 - value) in FP32
    *   [11:23] = stride between patches in DW = num_inputs * num_vertices * 4
    *             max = 32*32*4 + 32*4
    *   [24:31] = stride between vertices in DW = num_inputs * 4
    *             max = 32*4
    */
   struct ac_arg vs_state_bits;
   struct ac_arg vs_blit_inputs;
   /* HW VS */
   struct ac_arg streamout_config;
   struct ac_arg streamout_write_index;
   struct ac_arg streamout_offset[4];

   /* API TCS & TES */
   /* Layout of TCS outputs in the offchip buffer
    * # 6 bits
    *   [0:5] = the number of patches per threadgroup - 1, max = 63
    * # 5 bits
    *   [6:10] = the number of output vertices per patch - 1, max = 31
    * # 21 bits
    *   [11:31] = the offset of per patch attributes in the buffer in bytes.
    *             max = NUM_PATCHES*32*32*16 = 1M
    */
   struct ac_arg tcs_offchip_layout;

   /* API TCS */
   /* Offsets where TCS outputs and TCS patch outputs live in LDS:
    *   [0:15] = TCS output patch0 offset / 16, max = NUM_PATCHES * 32 * 32 = 64K (TODO: not enough bits)
    *   [16:31] = TCS output patch0 offset for per-patch / 16
    *             max = (NUM_PATCHES + 1) * 32*32 = 66624 (TODO: not enough bits)
    */
   struct ac_arg tcs_out_lds_offsets;
   /* Layout of TCS outputs / TES inputs:
    *   [0:12] = stride between output patches in DW, num_outputs * num_vertices * 4
    *            max = 32*32*4 + 32*4 = 4224
    *   [13:18] = gl_PatchVerticesIn, max = 32
    *   [19:31] = high 13 bits of the 32-bit address of tessellation ring buffers
    */
   struct ac_arg tcs_out_lds_layout;
   struct ac_arg tcs_offchip_offset;
   struct ac_arg tcs_factor_offset;

   /* API TES */
   struct ac_arg tes_offchip_addr;
   struct ac_arg tes_u;
   struct ac_arg tes_v;
   struct ac_arg tes_rel_patch_id;
   /* HW ES */
   struct ac_arg es2gs_offset;
   /* HW GS */
   /* On gfx10:
    *  - bits 0..11: ordered_wave_id
    *  - bits 12..20: number of vertices in group
    *  - bits 22..30: number of primitives in group
    */
   struct ac_arg gs_tg_info;
   /* API GS */
   struct ac_arg gs2vs_offset;
   struct ac_arg gs_wave_id;       /* GFX6 */
   struct ac_arg gs_vtx_offset[6]; /* in dwords (GFX6) */
   struct ac_arg gs_vtx01_offset;  /* in dwords (GFX9) */
   struct ac_arg gs_vtx23_offset;  /* in dwords (GFX9) */
   struct ac_arg gs_vtx45_offset;  /* in dwords (GFX9) */
   /* PS */
   struct ac_arg pos_fixed_pt;
   /* CS */
   struct ac_arg block_size;
   struct ac_arg cs_user_data;
   struct ac_arg cs_shaderbuf[3];
   struct ac_arg cs_image[3];

   /***********************/
   /*  RadeonSI args end  */
   /***********************/
};

void ac_add_arg(struct ac_shader_args *info, enum ac_arg_regfile regfile, unsigned registers,
                enum ac_arg_type type, struct ac_arg *arg);
void ac_add_return(struct ac_shader_args *info, enum ac_arg_regfile regfile);

#endif
