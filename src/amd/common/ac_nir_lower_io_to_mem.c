/*
 * Copyright © 2021 Valve Corporation
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

/*
 * These NIR passes are used to lower NIR cross-stage I/O intrinsics into the
 * memory accesses that actually happen on the HW.
 *
 * Each input and output has a 16-byte (4 dwords) slot reserved for it, and
 * can have up to 4 components. Each component is 32 bits.
 *
 * ## VS-TCS-TES I/O - Terminology:
 *
 * * patch - Group of vertices, used instead of primitives in tessellation
 * * per-vertex - input or output which can be different for every vertex.
 * * per-patch - input output which applies to a patch (a group of vertices)
 *
 * ## VS-TCS-TES I/O - How it works:
 *
 * * SW VS runs as a HW LS (Local Shader, merged into HS on GFX9+),
 *   and SW TCS runs as HW HS (Hull Shader).
 *   SW TES runs as either HW VS or HW ES (Export Shader).
 * * LS and HS share the same LDS space. LS->HS cross-stage I/O always goes through LDS.
 *   Notable exception:
 *   on GFX9+ when the input and output patch size are the same, the number of LS and HS
 *   invocations are also the same, so some I/O can be passed through temporaries (see tcs_in_out_eq).
 * * HS outputs are stored in LDS if the HS reads them.
 * * TES doesn't use the same LDS space. (Possible on the HW, but not implemented in Mesa,
 *   because that would force the TES waves to run on the same CU as the LS-HS waves.)
 *   So, HS outputs are stored to VRAM if the TES reads them.
 *
 * ### LDS layout used by VS-TCS:
 *
 * ```
 * TCS per-vertex inputs for patch 0  <--- 0
 * TCS per-vertex inputs for patch 1
 * TCS per-vertex inputs for patch 2  <--- hs_per_vertex_input_lds_offset (rel_patch_id = 2)
 * ...
 * TCS per-vertex outputs for patch 0 <--- output_patch0_offset
 * TCS per-patch outputs for patch 0  <--- output_patch0_patch_data_offset
 * TCS per-vertex outputs for patch 1
 * TCS per-patch outputs for patch 1
 * TCS per-vertex outputs for patch 2 <--- hs_output_lds_offset (rel_patch_id = 2, per-vertex)
 * TCS per-patch outputs for patch 2  <--- hs_output_lds_offset (rel_patch_id = 2, per-patch)
 * ...
 * ```
 *
 * ### VRAM layout used by TCS-TES I/O:
 *
 * ```
 * attr 0 of patch 0 vertex 0   <-- "off-chip LDS" offset
 * attr 0 of patch 0 vertex 1
 * attr 0 of patch 0 vertex 2
 * ...
 * attr 0 of patch 1 vertex 0
 * attr 0 of patch 1 vertex 1
 * attr 0 of patch 1 vertex 2   <-- hs_per_vertex_output_vmem_offset (attribute slot = 0, rel_patch_id = 1, vertex index = 1)
 * ...
 * attr 0 of patch 2 vertex 0
 * attr 0 of patch 2 vertex 1
 * attr 0 of patch 2 vertex 2
 * ...
 * attr 1 of patch 0 vertex 0
 * attr 1 of patch 0 vertex 1
 * attr 1 of patch 0 vertex 2
 * ...
 * ...
 * per-patch attr 0 of patch 0
 * per-patch attr 0 of patch 1
 * per-patch attr 0 of patch 2  <-- hs_per_patch_output_vmem_offset (attribute slot = 0, rel_patch_id = 2)
 * ...
 * per-patch attr 1 of patch 0
 * per-patch attr 1 of patch 1
 * per-patch attr 1 of patch 2
 * ...
 * ```
 *
 */

typedef struct {
   /* Which hardware generation we're dealing with */
   enum chip_class chip_class;

   /* True if merged VS+TCS (on GFX9+) has the same number
    * of input and output patch size.
    */
   bool tcs_in_out_eq;

   /* Bit mask of TCS per-vertex inputs (VS outputs) which
    * are passed between the two stages only in temporaries (registers).
    */
   uint64_t tcs_temp_only_inputs;

   /* Bit mask of TCS outputs read by TES. */
   uint64_t tes_inputs_read;
   uint64_t tes_patch_inputs_read;

   /* Whether TES reads the tess factors. */
   bool tes_reads_tessfactors;

   /* Number of inputs for which memory should be reserved.
    * When compacted, this should be the number of linked inputs.
    */
   unsigned tcs_num_reserved_inputs;
   unsigned tcs_num_reserved_outputs;
   unsigned tcs_num_reserved_patch_outputs;

   /* Location (slot) where tessellation levels are stored. */
   unsigned tcs_tess_lvl_in_loc;
   unsigned tcs_tess_lvl_out_loc;

} lower_io_to_mem_state;

static nir_ssa_def *
offset_from_io_intrinsic_with_stride_var(nir_builder *b,
                                         nir_intrinsic_instr *instr,
                                         nir_ssa_def *base_stride,
                                         unsigned component_stride)
{
   assert(instr);
   assert(base_stride);

   nir_ssa_def *base_stride_x4 = nir_umul24_imm(b, base_stride, 4u);

   /* base is the driver_location, which is in slots (1 slot = 4x4 bytes) */
   nir_ssa_def *base_op = nir_umul24_imm(b, base_stride_x4, nir_intrinsic_base(instr));

   /* component is in bytes */
   unsigned const_op = nir_intrinsic_component(instr) * component_stride;

   /* offset should be interpreted in relation to the base,
    * so the instruction effectively reads/writes another input/output
    * when it has an offset
    */
   nir_ssa_def *offset_op = nir_umul24(b, base_stride_x4, nir_ssa_for_src(b, *nir_get_io_offset_src(instr), 1));

   return nir_iadd_imm_nuw(b, nir_iadd_nuw(b, base_op, offset_op), const_op);
}

static nir_ssa_def *
offset_from_io_intrinsic_with_stride(nir_builder *b,
                                     nir_intrinsic_instr *instr,
                                     unsigned base_stride,
                                     unsigned component_stride)
{
   return offset_from_io_intrinsic_with_stride_var(b, instr, nir_imm_int(b, base_stride), component_stride);
}

static nir_ssa_def *
offset_from_io_intrinsic(nir_builder *b,
                         nir_intrinsic_instr *instr)
{
   return offset_from_io_intrinsic_with_stride(b, instr, 1u, 1u);
}

static bool
match_mask(nir_intrinsic_instr *intrin,
           uint64_t mask,
           bool match_indirect)
{
   bool indirect = !nir_src_is_const(*nir_get_io_offset_src(intrin));
   if (indirect)
      return match_indirect;

   uint64_t slot = nir_intrinsic_io_semantics(intrin).location;
   if (intrin->intrinsic != nir_intrinsic_load_per_vertex_input &&
       intrin->intrinsic != nir_intrinsic_store_per_vertex_output)
      slot -= VARYING_SLOT_PATCH0;

   return (1UL << slot) & mask;
}

static bool
tcs_output_needs_vmem(nir_intrinsic_instr *intrin,
                      lower_io_to_mem_state *st)
{
   uint64_t mask = intrin->intrinsic == nir_intrinsic_store_per_vertex_output
                   ? st->tes_inputs_read
                   : st->tes_patch_inputs_read;

   return match_mask(intrin, mask, true);
}

static bool
tcs_output_needs_lds(nir_intrinsic_instr *intrin,
                     nir_shader *shader)
{
   uint64_t mask = intrin->intrinsic == nir_intrinsic_store_per_vertex_output
                   ? shader->info.outputs_read
                   : shader->info.patch_outputs_read;

   return match_mask(intrin, mask, true);
}

static bool
filter_store_output(const nir_instr *instr,
                    UNUSED const void *st)
{
   return instr->type == nir_instr_type_intrinsic &&
          nir_instr_as_intrinsic(instr)->intrinsic ==
            nir_intrinsic_store_output;
}

static bool
filter_any_output_access(const nir_instr *instr,
                         UNUSED const void *st)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   return intrin->intrinsic == nir_intrinsic_store_output ||
          intrin->intrinsic == nir_intrinsic_store_per_vertex_output ||
          intrin->intrinsic == nir_intrinsic_load_output ||
          intrin->intrinsic == nir_intrinsic_load_per_vertex_output;
}

static bool
filter_any_input_access(const nir_instr *instr,
                        UNUSED const void *st)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   return intrin->intrinsic == nir_intrinsic_load_input ||
          intrin->intrinsic == nir_intrinsic_load_per_vertex_input;
}

static bool
lower_ls_output_store(nir_builder *b,
                      nir_instr *instr,
                      void *state)
{
   if (!filter_store_output(instr, state))
      return false;

   lower_io_to_mem_state *st = (lower_io_to_mem_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   /* If this is a temp-only TCS input, we don't need to use shared memory at all. */
   if (match_mask(intrin, st->tcs_temp_only_inputs, false))
      return false;

   b->cursor = nir_before_instr(instr);

   nir_ssa_def *vertex_idx = nir_build_load_tess_vs_rel_id_gcn(b);
   nir_ssa_def *base_off_var = nir_umul24_imm(b, vertex_idx, st->tcs_num_reserved_inputs * 16u);

   nir_ssa_def *io_off = offset_from_io_intrinsic_with_stride(b, intrin, 4u, 4u);
   unsigned write_mask = nir_intrinsic_write_mask(intrin);

   nir_ssa_def *off = nir_iadd_nuw(b, base_off_var, io_off);
   nir_build_store_shared(b, intrin->src[0].ssa, off, .write_mask = write_mask, .align_mul = 16u);

   /* NOTE: don't remove the store_output intrinsic on GFX9+ when tcs_in_out_eq,
    * it will be used by same-invocation TCS input loads.
    */
   if (!st->tcs_in_out_eq)
      nir_instr_remove(instr);

   return true;
}

static bool
filter_load_tcs_per_vertex_input(const nir_instr *instr,
                                 UNUSED const void *state)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   lower_io_to_mem_state *st = (lower_io_to_mem_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   if (intrin->intrinsic != nir_intrinsic_load_per_vertex_input)
      return false;
   if (!st->tcs_in_out_eq)
      return true;

   /* tcs_in_out_eq: a same-invocation input load, without indirect offset,
    * can use temporaries, no need to use shared memory.
    */
   nir_src *off_src = nir_get_io_offset_src(intrin);
   nir_src *vertex_index_src = nir_get_io_vertex_index_src(intrin);
   nir_instr *vertex_index_instr = vertex_index_src->ssa->parent_instr;

   bool can_use_temps = nir_src_is_const(*off_src) &&
                        vertex_index_instr->type == nir_instr_type_intrinsic &&
                        nir_instr_as_intrinsic(vertex_index_instr)->intrinsic == nir_intrinsic_load_invocation_id;

   return !can_use_temps;
}

static nir_ssa_def *
hs_per_vertex_input_lds_offset(nir_builder *b,
                               lower_io_to_mem_state *st,
                               nir_intrinsic_instr *instr)
{
   unsigned tcs_in_vertex_stride = st->tcs_num_reserved_inputs * 4u;
   nir_ssa_def *tcs_in_vtxcnt = nir_build_load_patch_vertices_in(b);
   nir_ssa_def *tcs_in_patch_stride = nir_umul24_imm(b, tcs_in_vtxcnt, tcs_in_vertex_stride);

   nir_ssa_def *off = offset_from_io_intrinsic(b, instr);

   nir_ssa_def *vertex_index = nir_ssa_for_src(b, *nir_get_io_vertex_index_src(instr), 1);
   nir_ssa_def *vertex_index_off = nir_umul24_imm(b, vertex_index, tcs_in_vertex_stride);
   off = nir_iadd_nuw(b, off, vertex_index_off);

   nir_ssa_def *rel_patch_id = nir_build_load_tess_rel_patch_id_gcn(b);
   nir_ssa_def *tcs_in_current_patch_offset = nir_umul24(b, rel_patch_id, tcs_in_patch_stride);
   off = nir_iadd_nuw(b, off, tcs_in_current_patch_offset);

   return nir_umul24_imm(b, off, 4u);
}

static nir_ssa_def *
hs_output_lds_offset(nir_builder *b,
                     lower_io_to_mem_state *st,
                     nir_intrinsic_instr *intrin)
{
   bool per_vertex = intrin &&
                     (intrin->intrinsic == nir_intrinsic_store_per_vertex_output ||
                      intrin->intrinsic == nir_intrinsic_load_per_vertex_output);

   unsigned output_vertex_size = st->tcs_num_reserved_outputs * 16u;
   unsigned pervertex_output_patch_size = b->shader->info.tess.tcs_vertices_out * output_vertex_size;
   unsigned output_patch_stride = pervertex_output_patch_size + st->tcs_num_reserved_patch_outputs * 16u;

   nir_ssa_def *tcs_in_vtxcnt = nir_build_load_patch_vertices_in(b);
   nir_ssa_def *tcs_num_patches = nir_build_load_tcs_num_patches_gcn(b);
   nir_ssa_def *input_patch_size = nir_umul24_imm(b, tcs_in_vtxcnt, st->tcs_num_reserved_inputs * 16u);
   nir_ssa_def *output_patch0_offset = nir_umul24(b, input_patch_size, tcs_num_patches);

   nir_ssa_def *off = intrin
                    ? offset_from_io_intrinsic_with_stride(b, intrin, 4u, 4u)
                    : nir_imm_int(b, 0);

   nir_ssa_def *rel_patch_id = nir_build_load_tess_rel_patch_id_gcn(b);
   nir_ssa_def *patch_offset = nir_umul24_imm(b, rel_patch_id, output_patch_stride);
   nir_ssa_def *output_patch_offset = nir_iadd_nuw(b, patch_offset, output_patch0_offset);

   if (per_vertex) {
      nir_ssa_def *vertex_index = nir_ssa_for_src(b, *nir_get_io_vertex_index_src(intrin), 1);
      nir_ssa_def *vertex_index_off = nir_umul24_imm(b, vertex_index, output_vertex_size);

      off = nir_iadd_nuw(b, off, vertex_index_off);
      return nir_iadd_nuw(b, off, output_patch_offset);
   } else {
      off = nir_iadd_imm_nuw(b, off, pervertex_output_patch_size);
      return nir_iadd_nuw(b, off, output_patch_offset);
   }
}

static nir_ssa_def *
hs_per_vertex_output_vmem_offset(nir_builder *b,
                                 lower_io_to_mem_state *st,
                                 nir_intrinsic_instr *intrin)
{
   nir_ssa_def *out_vertices_per_patch = b->shader->info.stage == MESA_SHADER_TESS_CTRL
                                         ? nir_imm_int(b, b->shader->info.tess.tcs_vertices_out)
                                         : nir_build_load_patch_vertices_in(b);

   nir_ssa_def *tcs_num_patches = nir_build_load_tcs_num_patches_gcn(b);
   nir_ssa_def *attr_stride_x4 = nir_umul24(b, tcs_num_patches, nir_umul24_imm(b, out_vertices_per_patch, 4u));
   nir_ssa_def *off = offset_from_io_intrinsic_with_stride_var(b, intrin, attr_stride_x4, 4u);
   nir_ssa_def *rel_patch_id = nir_build_load_tess_rel_patch_id_gcn(b);
   nir_ssa_def *patch_offset = nir_umul24(b, rel_patch_id, nir_umul24_imm(b, out_vertices_per_patch, 16u));
   off = nir_iadd_nuw(b, off, patch_offset);

   nir_ssa_def *vertex_index = nir_ssa_for_src(b, *nir_get_io_vertex_index_src(intrin), 1);
   nir_ssa_def *vertex_index_off = nir_umul24_imm(b, vertex_index, 16u);
   return nir_iadd_nuw(b, off, vertex_index_off);
}

static nir_ssa_def *
hs_per_patch_output_vmem_offset(nir_builder *b,
                                lower_io_to_mem_state *st,
                                nir_intrinsic_instr *intrin,
                                unsigned const_base_offset)
{
   nir_ssa_def *out_vertices_per_patch = b->shader->info.stage == MESA_SHADER_TESS_CTRL
                                         ? nir_imm_int(b, b->shader->info.tess.tcs_vertices_out)
                                         : nir_build_load_patch_vertices_in(b);

   nir_ssa_def *tcs_num_patches = nir_build_load_tcs_num_patches_gcn(b);
   nir_ssa_def *per_vertex_output_patch_size = nir_umul24_imm(b, out_vertices_per_patch, st->tcs_num_reserved_outputs * 16u);
   nir_ssa_def *per_patch_data_offset = nir_umul24(b, tcs_num_patches, per_vertex_output_patch_size);

   nir_ssa_def * off = intrin
                    ? offset_from_io_intrinsic_with_stride_var(b, intrin, nir_umul24_imm(b, tcs_num_patches, 4u), 4u)
                    : nir_imm_int(b, 0);

   if (const_base_offset)
      off = nir_iadd_nuw(b, off, nir_umul24_imm(b, tcs_num_patches, const_base_offset));

   nir_ssa_def *rel_patch_id = nir_build_load_tess_rel_patch_id_gcn(b);
   nir_ssa_def *patch_offset = nir_umul24_imm(b, rel_patch_id, 16u);
   off = nir_iadd_nuw(b, off, per_patch_data_offset);
   return nir_iadd_nuw(b, off, patch_offset);
}

static nir_ssa_def *
lower_hs_per_vertex_input_load(nir_builder *b,
                               nir_instr *instr,
                               void *state)
{
   lower_io_to_mem_state *st = (lower_io_to_mem_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   nir_ssa_def *off = hs_per_vertex_input_lds_offset(b, st, intrin);
   return nir_build_load_shared(b, intrin->dest.ssa.num_components, intrin->dest.ssa.bit_size, off, .align_mul = 16u);
}

static void
lower_hs_output_store(nir_builder *b,
                      nir_intrinsic_instr *intrin,
                      lower_io_to_mem_state *st)
{
   assert(intrin->intrinsic == nir_intrinsic_store_per_vertex_output ||
          intrin->intrinsic == nir_intrinsic_store_output);

   nir_io_semantics semantics = nir_intrinsic_io_semantics(intrin);
   nir_ssa_def *store_val = intrin->src[0].ssa;
   unsigned write_mask = nir_intrinsic_write_mask(intrin);
   bool is_tess_factor = semantics.location == VARYING_SLOT_TESS_LEVEL_INNER ||
                         semantics.location == VARYING_SLOT_TESS_LEVEL_OUTER;
   bool write_to_vmem = !is_tess_factor && tcs_output_needs_vmem(intrin, st);
   bool write_to_lds = is_tess_factor || tcs_output_needs_lds(intrin, b->shader);

   if (write_to_vmem) {
      nir_ssa_def *vmem_off = intrin->intrinsic == nir_intrinsic_store_per_vertex_output
                            ? hs_per_vertex_output_vmem_offset(b, st, intrin)
                            : hs_per_patch_output_vmem_offset(b, st, intrin, 0);

      nir_ssa_def *hs_ring_tess_offchip = nir_build_load_ring_tess_offchip_gcn(b);
      nir_ssa_def *offchip_offset = nir_build_load_ring_tess_offchip_offset_gcn(b);
      nir_build_store_mubuf_gcn(b, store_val, hs_ring_tess_offchip, vmem_off, offchip_offset, .write_mask = write_mask, .memory_modes = nir_var_shader_out);
   }

   if (write_to_lds) {
      /* Remember driver location of tess factors, so we can read them later */
      if (semantics.location == VARYING_SLOT_TESS_LEVEL_INNER)
         st->tcs_tess_lvl_in_loc = nir_intrinsic_base(intrin) * 16u;
      else if (semantics.location == VARYING_SLOT_TESS_LEVEL_OUTER)
         st->tcs_tess_lvl_out_loc = nir_intrinsic_base(intrin) * 16u;

      nir_ssa_def *lds_off = hs_output_lds_offset(b, st, intrin);
      nir_build_store_shared(b, store_val, lds_off, .write_mask = write_mask, .align_mul = 16u);
   }
}

static nir_ssa_def *
lower_hs_output_load(nir_builder *b,
                     nir_intrinsic_instr *intrin,
                     lower_io_to_mem_state *st)
{
   nir_ssa_def *off = hs_output_lds_offset(b, st, intrin);
   return nir_build_load_shared(b, intrin->dest.ssa.num_components, intrin->dest.ssa.bit_size, off, .align_mul = 16u);
}

static nir_ssa_def *
lower_hs_output_access(nir_builder *b,
                       nir_instr *instr,
                       void *state)
{
   assert(filter_any_output_access(instr, state));

   lower_io_to_mem_state *st = (lower_io_to_mem_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   if (intrin->intrinsic == nir_intrinsic_store_output ||
       intrin->intrinsic == nir_intrinsic_store_per_vertex_output) {
      lower_hs_output_store(b, intrin, st);
      return NIR_LOWER_INSTR_PROGRESS_REPLACE;
   } else {
      return lower_hs_output_load(b, intrin, st);
   }
}

static void
hs_emit_write_tess_factors(nir_shader *shader,
                           lower_io_to_mem_state *st)
{
   unsigned outer_comps;
   unsigned inner_comps;

   switch (shader->info.tess.primitive_mode) {
   case GL_ISOLINES:
      outer_comps = 2;
      inner_comps = 0;
      break;
   case GL_TRIANGLES:
      outer_comps = 3;
      inner_comps = 1;
      break;
   case GL_QUADS:
      outer_comps = 4;
      inner_comps = 2;
      break;
   default:
      unreachable("invalid primitive mode");
      return;
   }

   nir_function_impl *impl = nir_shader_get_entrypoint(shader);
   assert(impl);
   nir_block *last_block = nir_impl_last_block(impl);
   assert(last_block);

   /* We assume there is always a single end block in the shader. */

   nir_builder builder;
   nir_builder *b = &builder; /* This is to avoid the & */
   nir_builder_init(b, impl);
   b->cursor = nir_after_block(last_block);

   nir_scoped_barrier(b, .execution_scope=NIR_SCOPE_WORKGROUP, .memory_scope=NIR_SCOPE_WORKGROUP,
                         .memory_semantics=NIR_MEMORY_ACQ_REL, .memory_modes=nir_var_shader_out|nir_var_mem_shared);

   nir_ssa_def *invocation_id = nir_build_load_invocation_id(b);

   /* Only the 1st invocation of each patch needs to do this. */
   nir_if *invocation_id_zero = nir_push_if(b, nir_ieq_imm(b, invocation_id, 0));

   /* The descriptor where tess factors have to be stored by the shader. */
   nir_ssa_def *tessfactor_ring = nir_build_load_ring_tess_factors_gcn(b);

   /* Base LDS address of per-patch outputs in the current patch. */
   nir_ssa_def *lds_base = hs_output_lds_offset(b, st, NULL);

   /* Load all tessellation factors (aka. tess levels) from LDS. */
   nir_ssa_def *tessfactors_outer = nir_build_load_shared(b, outer_comps, 32, lds_base, .base = st->tcs_tess_lvl_out_loc, .align_mul = 16u, .align_offset = st->tcs_tess_lvl_out_loc % 16u);
   nir_ssa_def *tessfactors_inner = inner_comps
                                    ? nir_build_load_shared(b, inner_comps, 32, lds_base, .base = st->tcs_tess_lvl_in_loc, .align_mul = 16u, .align_offset = st->tcs_tess_lvl_in_loc % 16u)
                                    : NULL;

   if (shader->info.tess.primitive_mode == GL_ISOLINES) {
      /* LINES reversal */
      nir_ssa_def *o0 = nir_vector_extract(b, tessfactors_outer, nir_imm_int(b, 0));
      nir_ssa_def *o1 = nir_vector_extract(b, tessfactors_outer, nir_imm_int(b, 1));
      tessfactors_outer = nir_vec2(b, o1, o0);
   }

   nir_ssa_def *rel_patch_id = nir_build_load_tess_rel_patch_id_gcn(b);
   nir_ssa_def *tess_factors_base = nir_build_load_ring_tess_factors_offset_gcn(b);
   nir_ssa_def *tess_factors_offset = nir_umul24_imm(b, rel_patch_id, (inner_comps + outer_comps) * 4u);
   unsigned tess_factors_const_offset = 0;

   if (st->chip_class <= GFX8) {
      /* Store the dynamic HS control word. */
      nir_if *rel_patch_id_zero = nir_push_if(b, nir_ieq_imm(b, rel_patch_id, 0));
      nir_ssa_def *ctrlw = nir_imm_int(b, 0x80000000u);
      nir_build_store_mubuf_gcn(b, ctrlw, tessfactor_ring, nir_ssa_undef(b, 1, 32), tess_factors_base, .write_mask = 0x1u);
      tess_factors_const_offset += 4;
      nir_pop_if(b, rel_patch_id_zero);
   }

   /* Store tess factors for the tessellator */
   if ((inner_comps + outer_comps) <= 4) {
      /* There is no nir_op_vec6 so we only do this for isolines and triangles. */
      nir_ssa_def *tessfactors[4];
      for (unsigned i = 0; i < outer_comps; ++i)
         tessfactors[i] = nir_vector_extract(b, tessfactors_outer, nir_imm_int(b, i));
      for (unsigned i = 0; i < inner_comps; ++i)
         tessfactors[outer_comps + i] = nir_vector_extract(b, tessfactors_inner, nir_imm_int(b, i));
      nir_ssa_def *tessfactors_all = nir_vec(b, tessfactors, outer_comps + inner_comps);
      nir_build_store_mubuf_gcn(b, tessfactors_all, tessfactor_ring, tess_factors_offset, tess_factors_base, .base = tess_factors_const_offset, .write_mask = 0xfu);
   } else {
      nir_build_store_mubuf_gcn(b, tessfactors_outer, tessfactor_ring, tess_factors_offset, tess_factors_base, .base = tess_factors_const_offset, .write_mask = 0xfu);
      if (inner_comps)
         nir_build_store_mubuf_gcn(b, tessfactors_inner, tessfactor_ring, tess_factors_offset, tess_factors_base, .base = tess_factors_const_offset + 4u * outer_comps, .write_mask = 0xfu);
   }

   if (st->tes_reads_tessfactors) {
      /* Store to offchip for TES to read - only if TES actually reads them */
      nir_ssa_def *hs_ring_tess_offchip = nir_build_load_ring_tess_offchip_gcn(b);
      nir_ssa_def *offchip_offset = nir_build_load_ring_tess_offchip_offset_gcn(b);

      nir_ssa_def *vmem_off_outer = hs_per_patch_output_vmem_offset(b, st, NULL, st->tcs_tess_lvl_out_loc);
      nir_build_store_mubuf_gcn(b, tessfactors_outer, hs_ring_tess_offchip, vmem_off_outer, offchip_offset, .write_mask = 0xfu, .memory_modes = nir_var_shader_out);

      if (inner_comps) {
         nir_ssa_def *vmem_off_inner = hs_per_patch_output_vmem_offset(b, st, NULL, st->tcs_tess_lvl_in_loc);
         nir_build_store_mubuf_gcn(b, tessfactors_inner, hs_ring_tess_offchip, vmem_off_inner, offchip_offset, .write_mask = 0xfu, .memory_modes = nir_var_shader_out);
      }
   }

   nir_pop_if(b, invocation_id_zero);
}

static nir_ssa_def *
lower_tes_input_load(nir_builder *b,
                     nir_instr *instr,
                     void *state)
{
   assert(filter_any_input_access(instr, state));

   lower_io_to_mem_state *st = (lower_io_to_mem_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   nir_ssa_def *offchip_ring = nir_build_load_ring_tess_offchip_gcn(b);
   nir_ssa_def *offchip_offset = nir_build_load_ring_tess_offchip_offset_gcn(b);
   nir_ssa_def *off = intrin->intrinsic == nir_intrinsic_load_per_vertex_input
                    ? hs_per_vertex_output_vmem_offset(b, st, intrin)
                    : hs_per_patch_output_vmem_offset(b, st, intrin, 0);

   return nir_build_load_mubuf_gcn(b, intrin->dest.ssa.num_components, intrin->dest.ssa.bit_size, offchip_ring, off, offchip_offset);
}

void
ac_nir_lower_ls_outputs_to_mem(nir_shader *shader,
                               bool tcs_in_out_eq,
                               uint64_t tcs_temp_only_inputs,
                               unsigned num_reserved_ls_outputs)
{
   assert(shader->info.stage == MESA_SHADER_VERTEX);

   lower_io_to_mem_state state = {
      .tcs_num_reserved_inputs = num_reserved_ls_outputs,
      .tcs_in_out_eq = tcs_in_out_eq,
      .tcs_temp_only_inputs = tcs_in_out_eq ? tcs_temp_only_inputs : 0,
   };

   nir_shader_instructions_pass(shader,
                                lower_ls_output_store,
                                nir_metadata_all,
                                &state);
}

void
ac_nir_lower_hs_inputs_to_mem(nir_shader *shader,
                              bool tcs_in_out_eq,
                              unsigned num_reserved_tcs_inputs)
{
   assert(shader->info.stage == MESA_SHADER_TESS_CTRL);

   lower_io_to_mem_state state = {
      .tcs_in_out_eq = tcs_in_out_eq,
      .tcs_num_reserved_inputs = num_reserved_tcs_inputs,
   };

   nir_shader_lower_instructions(shader,
                                 filter_load_tcs_per_vertex_input,
                                 lower_hs_per_vertex_input_load,
                                 &state);
}

void
ac_nir_lower_hs_outputs_to_mem(nir_shader *shader,
                               enum chip_class chip_class,
                               bool tes_reads_tessfactors,
                               uint64_t tes_inputs_read,
                               uint64_t tes_patch_inputs_read,
                               unsigned num_reserved_tcs_inputs,
                               unsigned num_reserved_tcs_outputs,
                               unsigned num_reserved_tcs_patch_outputs,
                               bool emit_tess_factor_write)
{
   assert(shader->info.stage == MESA_SHADER_TESS_CTRL);

   lower_io_to_mem_state state = {
      .chip_class = chip_class,
      .tes_reads_tessfactors = tes_reads_tessfactors,
      .tes_inputs_read = tes_inputs_read,
      .tes_patch_inputs_read = tes_patch_inputs_read,
      .tcs_num_reserved_inputs = num_reserved_tcs_inputs,
      .tcs_num_reserved_outputs = num_reserved_tcs_outputs,
      .tcs_num_reserved_patch_outputs = num_reserved_tcs_patch_outputs,
   };

   nir_shader_lower_instructions(shader,
                                 filter_any_output_access,
                                 lower_hs_output_access,
                                 &state);

   if (emit_tess_factor_write)
      hs_emit_write_tess_factors(shader, &state);
}

void
ac_nir_lower_tes_inputs_to_mem(nir_shader *shader,
                               unsigned num_reserved_tcs_outputs,
                               unsigned num_reserved_tcs_patch_outputs)
{
   assert(shader->info.stage == MESA_SHADER_TESS_EVAL);

   lower_io_to_mem_state state = {
      .tcs_num_reserved_outputs = num_reserved_tcs_outputs,
      .tcs_num_reserved_patch_outputs = num_reserved_tcs_patch_outputs,
   };

   nir_shader_lower_instructions(shader,
                                 filter_any_input_access,
                                 lower_tes_input_load,
                                 &state);
}

typedef struct
{
   unsigned patch_vtx_in;
   unsigned tcs_num_patches;
   unsigned options;
} lower_tess_to_const_state;

static bool
filter_const_lowerable_tess_intrinsics(const nir_instr *instr,
                                       const void *state)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   lower_tess_to_const_state *st = (lower_tess_to_const_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   return ((st->options & ac_nir_lower_patch_vtx_in) && intrin->intrinsic == nir_intrinsic_load_patch_vertices_in) ||
          ((st->options & ac_nir_lower_num_patches) && intrin->intrinsic == nir_intrinsic_load_tcs_num_patches_gcn);
}

static nir_ssa_def *
lower_tess_intrinsics_to_const(nir_builder *b,
                               nir_instr *instr,
                               void *state)
{
   lower_tess_to_const_state *st = (lower_tess_to_const_state *) state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   switch (intrin->intrinsic) {
   case nir_intrinsic_load_patch_vertices_in:
      return nir_imm_int(b, st->patch_vtx_in);
   case nir_intrinsic_load_tcs_num_patches_gcn:
      return nir_imm_int(b, st->tcs_num_patches);
   default:
      unreachable("Unsupported tess intrinsic.");
   }
}

void
ac_nir_lower_tess_to_const(nir_shader *shader,
                           unsigned patch_vtx_in,
                           unsigned tcs_num_patches,
                           unsigned options)
{
   lower_tess_to_const_state st = {
      .patch_vtx_in = patch_vtx_in,
      .tcs_num_patches = tcs_num_patches,
      .options = options,
   };

   nir_shader_lower_instructions(shader,
                                 filter_const_lowerable_tess_intrinsics,
                                 lower_tess_intrinsics_to_const,
                                 &st);
}