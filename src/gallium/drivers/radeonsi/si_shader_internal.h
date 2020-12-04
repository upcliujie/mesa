/*
 * Copyright 2016 Advanced Micro Devices, Inc.
 * All Rights Reserved.
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

#ifndef SI_SHADER_PRIVATE_H
#define SI_SHADER_PRIVATE_H

#include "ac_shader_abi.h"
#include "si_shader.h"

struct pipe_debug_callback;

#define RADEON_LLVM_MAX_INPUTS 32 * 4

/* Ideally pass the sample mask input to the PS epilog as v14, which
 * is its usual location, so that the shader doesn't have to add v_mov.
 */
#define PS_EPILOG_SAMPLEMASK_MIN_LOC 14

struct si_shader_output_values {
   LLVMValueRef values[4];
   ubyte vertex_stream[4];
   ubyte semantic;
};

struct si_shader_context {
   struct ac_llvm_context ac;
   struct si_shader *shader;
   struct si_shader_selector *next_shader_sel;
   struct si_screen *screen;

   gl_shader_stage stage;

   /* For clamping the non-constant index in resource indexing: */
   unsigned num_const_buffers;
   unsigned num_shader_buffers;
   unsigned num_images;
   unsigned num_samplers;

   struct ac_shader_args args;
   struct ac_shader_abi abi;

   LLVMValueRef inputs[RADEON_LLVM_MAX_INPUTS];

   LLVMBasicBlockRef merged_wrap_if_entry_block;
   int merged_wrap_if_label;

   LLVMValueRef main_fn;
   LLVMTypeRef return_type;

   struct ac_llvm_compiler *compiler;

   /* Preloaded descriptors. */
   LLVMValueRef esgs_ring;
   LLVMValueRef gsvs_ring[4];
   LLVMValueRef tess_offchip_ring;

   LLVMValueRef invoc0_tess_factors[6]; /* outer[4], inner[2] */
   LLVMValueRef gs_next_vertex[4];
   LLVMValueRef gs_curprim_verts[4];
   LLVMValueRef gs_generated_prims[4];
   LLVMValueRef gs_ngg_emit;
   LLVMValueRef gs_ngg_scratch;
   LLVMValueRef return_value;
};

static inline struct si_shader_context *si_shader_context_from_abi(struct ac_shader_abi *abi)
{
   struct si_shader_context *ctx = NULL;
   return container_of(abi, ctx, abi);
}

/* si_shader.c */
bool si_is_multi_part_shader(struct si_shader *shader);
bool si_is_merged_shader(struct si_shader *shader);
void si_add_arg_checked(struct ac_shader_args *args, enum ac_arg_regfile file, unsigned registers,
                        enum ac_arg_type type, struct ac_arg *arg, unsigned idx);
void si_init_shader_args(struct si_shader_context *ctx, bool ngg_cull_shader);
unsigned si_get_max_workgroup_size(const struct si_shader *shader);
bool si_vs_needs_prolog(const struct si_shader_selector *sel,
                        const struct si_vs_prolog_bits *prolog_key,
                        const struct si_shader_key *key, bool ngg_cull_shader);
void si_get_vs_prolog_key(const struct si_shader_info *info, unsigned num_input_sgprs,
                          bool ngg_cull_shader, const struct si_vs_prolog_bits *prolog_key,
                          struct si_shader *shader_out, union si_shader_part_key *key);
struct nir_shader *si_get_nir_shader(struct si_shader_selector *sel,
                                     const struct si_shader_key *key,
                                     bool *free_nir);
bool si_need_ps_prolog(const union si_shader_part_key *key);
void si_get_ps_prolog_key(struct si_shader *shader, union si_shader_part_key *key,
                          bool separate_prolog);
void si_get_ps_epilog_key(struct si_shader *shader, union si_shader_part_key *key);
void si_fix_resource_usage(struct si_screen *sscreen, struct si_shader *shader);

/* gfx10_shader_ngg.c */
bool gfx10_ngg_export_prim_early(struct si_shader *shader);
void gfx10_ngg_build_sendmsg_gs_alloc_req(struct si_shader_context *ctx);
void gfx10_ngg_build_export_prim(struct si_shader_context *ctx, LLVMValueRef user_edgeflags[3],
                                 LLVMValueRef prim_passthrough);
void gfx10_emit_ngg_culling_epilogue(struct ac_shader_abi *abi, unsigned max_outputs,
                                     LLVMValueRef *addrs);
void gfx10_emit_ngg_epilogue(struct ac_shader_abi *abi, unsigned max_outputs, LLVMValueRef *addrs);
void gfx10_ngg_gs_emit_vertex(struct si_shader_context *ctx, unsigned stream, LLVMValueRef *addrs);
void gfx10_ngg_gs_emit_prologue(struct si_shader_context *ctx);
void gfx10_ngg_gs_emit_epilogue(struct si_shader_context *ctx);
unsigned gfx10_ngg_get_scratch_dw_size(struct si_shader *shader);
bool gfx10_ngg_calculate_subgroup_info(struct si_shader *shader);

/* si_shader_llvm.c */
bool si_compile_llvm(struct si_screen *sscreen, struct si_shader_binary *binary,
                     struct ac_shader_config *conf, struct ac_llvm_compiler *compiler,
                     struct ac_llvm_context *ac, struct pipe_debug_callback *debug,
                     gl_shader_stage stage, const char *name, bool less_optimized);
void si_llvm_context_init(struct si_shader_context *ctx, struct si_screen *sscreen,
                          struct ac_llvm_compiler *compiler, unsigned wave_size);
void si_llvm_create_func(struct si_shader_context *ctx, const char *name, LLVMTypeRef *return_types,
                         unsigned num_return_elems, unsigned max_workgroup_size);
void si_llvm_create_main_func(struct si_shader_context *ctx, bool ngg_cull_shader);
void si_llvm_optimize_module(struct si_shader_context *ctx);
void si_llvm_dispose(struct si_shader_context *ctx);
LLVMValueRef si_buffer_load_const(struct si_shader_context *ctx, LLVMValueRef resource,
                                  LLVMValueRef offset);
void si_llvm_build_ret(struct si_shader_context *ctx, LLVMValueRef ret);
LLVMValueRef si_insert_input_ret(struct si_shader_context *ctx, LLVMValueRef ret,
                                 struct ac_arg param, unsigned return_index);
LLVMValueRef si_insert_input_ret_float(struct si_shader_context *ctx, LLVMValueRef ret,
                                       struct ac_arg param, unsigned return_index);
LLVMValueRef si_insert_input_ptr(struct si_shader_context *ctx, LLVMValueRef ret,
                                 struct ac_arg param, unsigned return_index);
LLVMValueRef si_prolog_get_rw_buffers(struct si_shader_context *ctx);
void si_llvm_emit_barrier(struct si_shader_context *ctx);
void si_llvm_declare_esgs_ring(struct si_shader_context *ctx);
void si_init_exec_from_input(struct si_shader_context *ctx, struct ac_arg param,
                             unsigned bitoffset);
LLVMValueRef si_unpack_param(struct si_shader_context *ctx, struct ac_arg param, unsigned rshift,
                             unsigned bitwidth);
LLVMValueRef si_get_primitive_id(struct si_shader_context *ctx, unsigned swizzle);
void si_build_wrapper_function(struct si_shader_context *ctx, LLVMValueRef *parts,
                               unsigned num_parts, unsigned main_part,
                               unsigned next_shader_first_part, bool same_thread_count);
bool si_llvm_translate_nir(struct si_shader_context *ctx, struct si_shader *shader,
                           struct nir_shader *nir, bool free_nir, bool ngg_cull_shader);
bool si_llvm_compile_shader(struct si_screen *sscreen, struct ac_llvm_compiler *compiler,
                            struct si_shader *shader, struct pipe_debug_callback *debug,
                            struct nir_shader *nir, bool free_nir);

/* si_shader_llvm_gs.c */
LLVMValueRef si_is_es_thread(struct si_shader_context *ctx);
LLVMValueRef si_is_gs_thread(struct si_shader_context *ctx);
void si_llvm_emit_es_epilogue(struct ac_shader_abi *abi, unsigned max_outputs, LLVMValueRef *addrs);
void si_preload_esgs_ring(struct si_shader_context *ctx);
void si_preload_gs_rings(struct si_shader_context *ctx);
void si_llvm_build_gs_prolog(struct si_shader_context *ctx, union si_shader_part_key *key);
void si_llvm_init_gs_callbacks(struct si_shader_context *ctx);

/* si_shader_llvm_tess.c */
void si_llvm_preload_tes_rings(struct si_shader_context *ctx);
void si_llvm_emit_ls_epilogue(struct ac_shader_abi *abi, unsigned max_outputs, LLVMValueRef *addrs);
void si_llvm_build_tcs_epilog(struct si_shader_context *ctx, union si_shader_part_key *key);
void si_llvm_init_tcs_callbacks(struct si_shader_context *ctx);
void si_llvm_init_tes_callbacks(struct si_shader_context *ctx, bool ngg_cull_shader);

/* si_shader_llvm_ps.c */
LLVMValueRef si_get_sample_id(struct si_shader_context *ctx);
void si_llvm_build_ps_prolog(struct si_shader_context *ctx, union si_shader_part_key *key);
void si_llvm_build_ps_epilog(struct si_shader_context *ctx, union si_shader_part_key *key);
void si_llvm_build_monolithic_ps(struct si_shader_context *ctx, struct si_shader *shader);
void si_llvm_init_ps_callbacks(struct si_shader_context *ctx);

/* si_shader_llvm_resources.c */
void si_llvm_init_resource_callbacks(struct si_shader_context *ctx);

/* si_shader_llvm_vs.c */
void si_llvm_load_vs_inputs(struct si_shader_context *ctx, struct nir_shader *nir);
void si_llvm_streamout_store_output(struct si_shader_context *ctx, LLVMValueRef const *so_buffers,
                                    LLVMValueRef const *so_write_offsets,
                                    struct pipe_stream_output *stream_out,
                                    struct si_shader_output_values *shader_out);
void si_llvm_emit_streamout(struct si_shader_context *ctx, struct si_shader_output_values *outputs,
                            unsigned noutput, unsigned stream);
void si_llvm_build_vs_exports(struct si_shader_context *ctx,
                              struct si_shader_output_values *outputs, unsigned noutput);
void si_llvm_emit_vs_epilogue(struct ac_shader_abi *abi, unsigned max_outputs, LLVMValueRef *addrs);
void si_llvm_build_vs_prolog(struct si_shader_context *ctx, union si_shader_part_key *key);
void si_llvm_init_vs_callbacks(struct si_shader_context *ctx, bool ngg_cull_shader);

#endif
