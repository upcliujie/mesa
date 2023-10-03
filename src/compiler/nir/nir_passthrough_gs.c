/*
 * Copyright © 2022 Collabora Ltc.
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

#include "util/u_memory.h"
#include "util/u_prim.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_xfb_info.h"

static unsigned int
gs_in_prim_for_topology(enum mesa_prim prim)
{
   switch (prim) {
   case MESA_PRIM_QUADS:
      return MESA_PRIM_LINES_ADJACENCY;
   default:
      return prim;
   }
}

static enum mesa_prim
gs_out_prim_for_topology(enum mesa_prim prim)
{
   switch (prim) {
   case MESA_PRIM_POINTS:
      return MESA_PRIM_POINTS;
   case MESA_PRIM_LINES:
   case MESA_PRIM_LINE_LOOP:
   case MESA_PRIM_LINES_ADJACENCY:
   case MESA_PRIM_LINE_STRIP_ADJACENCY:
   case MESA_PRIM_LINE_STRIP:
      return MESA_PRIM_LINE_STRIP;
   case MESA_PRIM_TRIANGLES:
   case MESA_PRIM_TRIANGLE_STRIP:
   case MESA_PRIM_TRIANGLE_FAN:
   case MESA_PRIM_TRIANGLES_ADJACENCY:
   case MESA_PRIM_TRIANGLE_STRIP_ADJACENCY:
   case MESA_PRIM_POLYGON:
      return MESA_PRIM_TRIANGLE_STRIP;
   case MESA_PRIM_QUADS:
   case MESA_PRIM_QUAD_STRIP:
   case MESA_PRIM_PATCHES:
   default:
      return MESA_PRIM_QUADS;
   }
}

static unsigned int
vertices_for_prim(enum mesa_prim prim)
{
   switch (prim) {
   case MESA_PRIM_POINTS:
      return 1;
   case MESA_PRIM_LINES:
   case MESA_PRIM_LINE_LOOP:
   case MESA_PRIM_LINES_ADJACENCY:
   case MESA_PRIM_LINE_STRIP_ADJACENCY:
   case MESA_PRIM_LINE_STRIP:
      return 2;
   case MESA_PRIM_TRIANGLES:
   case MESA_PRIM_TRIANGLE_STRIP:
   case MESA_PRIM_TRIANGLE_FAN:
   case MESA_PRIM_TRIANGLES_ADJACENCY:
   case MESA_PRIM_TRIANGLE_STRIP_ADJACENCY:
   case MESA_PRIM_POLYGON:
      return 3;
   case MESA_PRIM_QUADS:
   case MESA_PRIM_QUAD_STRIP:
      return 4;
   case MESA_PRIM_PATCHES:
   default:
      unreachable("unsupported primitive for gs input");
   }
}

struct store_instr_info {
   nir_alu_type alu_type;
   nir_io_semantics io_semantics;
   nir_io_xfb xfb_info[2];
   uint8_t num_components;
};

struct scan_stores_state {
   struct store_instr_info store_instrs[VARYING_SLOT_MAX];
   bool written[VARYING_SLOT_MAX];
};

static bool
filter_io_instr(nir_intrinsic_instr *intr, bool *is_load, bool *is_input, bool *is_interp)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_interpolated_input:
      *is_interp = true;
      FALLTHROUGH;
   case nir_intrinsic_load_input:
   case nir_intrinsic_load_per_vertex_input:
      *is_input = true;
      FALLTHROUGH;
   case nir_intrinsic_load_output:
   case nir_intrinsic_load_per_vertex_output:
   case nir_intrinsic_load_per_primitive_output:
      *is_load = true;
      FALLTHROUGH;
   case nir_intrinsic_store_output:
   case nir_intrinsic_store_per_primitive_output:
   case nir_intrinsic_store_per_vertex_output:
      break;
   default:
      return false;
   }
   return true;
}

static bool
scan_stores_instr(nir_builder *b, nir_instr *instr, void *data)
{
   struct scan_stores_state *state = data;
   if (instr->type != nir_instr_type_intrinsic)
      return false;
   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
   bool is_load = false;
   bool is_input = false;
   bool is_interp = false;
   if (!filter_io_instr(intr, &is_load, &is_input, &is_interp))
      return false;

   if (is_input)
      return false;

   unsigned component = nir_intrinsic_component(intr);
   nir_io_semantics io_semantics = nir_intrinsic_io_semantics(intr);

   state->written[io_semantics.location] = true;
   state->store_instrs[io_semantics.location] = (struct store_instr_info){
      .alu_type = nir_intrinsic_src_type(intr),
      .io_semantics = io_semantics,
      .xfb_info[0] = nir_intrinsic_io_xfb(intr),
      .xfb_info[1] = nir_intrinsic_io_xfb2(intr),
      .num_components = intr->num_components,
   };

   return true;
}

static bool
scan_stores(nir_shader *shader, struct scan_stores_state *state)
{
   return nir_shader_instructions_pass(shader, scan_stores_instr,
                                       nir_metadata_loop_analysis |
                                          nir_metadata_block_index |
                                          nir_metadata_dominance,
                                       state);
}

/*
 * A helper to create a passthrough GS shader for drivers that needs to lower
 * some rendering tasks to the GS.
 */

nir_shader *
nir_create_passthrough_gs(const nir_shader_compiler_options *options,
                          const nir_shader *prev_stage,
                          enum mesa_prim primitive_type,
                          bool emulate_edgeflags,
                          bool force_line_strip_out)
{
   unsigned int vertices_out = vertices_for_prim(primitive_type);
   bool needs_closing = (force_line_strip_out || emulate_edgeflags) && vertices_out >= 3;
   enum mesa_prim original_our_prim = gs_out_prim_for_topology(primitive_type);
   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_GEOMETRY,
                                                  options,
                                                  "gs passthrough");

   nir_shader *nir = b.shader;
   nir->info.gs.input_primitive = gs_in_prim_for_topology(primitive_type);
   nir->info.gs.output_primitive = (force_line_strip_out || emulate_edgeflags) ? MESA_PRIM_LINE_STRIP : original_our_prim;
   nir->info.gs.vertices_in = u_vertices_per_prim(primitive_type);
   nir->info.gs.vertices_out = needs_closing ? vertices_out + 1 : vertices_out;
   nir->info.gs.invocations = 1;
   nir->info.gs.active_stream_mask = 1;

   nir->info.has_transform_feedback_varyings = prev_stage->info.has_transform_feedback_varyings;
   memcpy(nir->info.xfb_stride, prev_stage->info.xfb_stride, sizeof(prev_stage->info.xfb_stride));
   if (prev_stage->xfb_info) {
      nir->xfb_info = mem_dup(prev_stage->xfb_info, nir_xfb_info_size(prev_stage->xfb_info->output_count));
   }

   bool handle_flat = nir->info.gs.output_primitive == MESA_PRIM_LINE_STRIP &&
                      nir->info.gs.output_primitive != original_our_prim;
   nir_variable *in_vars[VARYING_SLOT_MAX * 4];
   nir_variable *out_vars[VARYING_SLOT_MAX * 4];
   unsigned num_inputs = 0, num_outputs = 0;

   struct scan_stores_state scan_state = {0};
   NIR_PASS_V((nir_shader *)prev_stage, scan_stores, &scan_state);

   /* Create input/output variables. */
   nir_foreach_shader_out_variable(var, prev_stage) {
      assert(!var->data.patch);

      /* input vars can't be created for those */
      if (var->data.location == VARYING_SLOT_LAYER ||
          var->data.location == VARYING_SLOT_VIEW_INDEX)
         continue;

      char name[100];
      if (var->name)
         snprintf(name, sizeof(name), "in_%s", var->name);
      else
         snprintf(name, sizeof(name), "in_%d", var->data.driver_location);

      nir_variable *in = nir_variable_clone(var, nir);
      ralloc_free(in->name);
      in->name = ralloc_strdup(in, name);
      in->type = glsl_array_type(var->type, 6, false);
      in->data.mode = nir_var_shader_in;
      nir_shader_add_variable(nir, in);

      in_vars[num_inputs++] = in;

      nir->num_inputs++;
      if (in->data.location == VARYING_SLOT_EDGE)
         continue;

      if (var->data.location != VARYING_SLOT_POS)
         nir->num_outputs++;

      if (var->name)
         snprintf(name, sizeof(name), "out_%s", var->name);
      else
         snprintf(name, sizeof(name), "out_%d", var->data.driver_location);

      nir_variable *out = nir_variable_clone(var, nir);
      ralloc_free(out->name);
      out->name = ralloc_strdup(out, name);
      out->data.mode = nir_var_shader_out;
      nir_shader_add_variable(nir, out);

      out_vars[num_outputs++] = out;
   }

   unsigned int start_vert = 0;
   unsigned int end_vert = vertices_out;
   unsigned int vert_step = 1;
   switch (primitive_type) {
   case MESA_PRIM_LINES_ADJACENCY:
   case MESA_PRIM_LINE_STRIP_ADJACENCY:
      start_vert = 1;
      end_vert += 1;
      break;
   case MESA_PRIM_TRIANGLES_ADJACENCY:
   case MESA_PRIM_TRIANGLE_STRIP_ADJACENCY:
      end_vert = 5;
      vert_step = 2;
      break;
   default:
      break;
   }

   nir_def *flat_interp_mask_def = nir_load_flat_mask(&b);
   nir_def *last_pv_vert_def = nir_load_provoking_last(&b);
   last_pv_vert_def = nir_ine_imm(&b, last_pv_vert_def, 0);
   nir_def *start_vert_index = nir_imm_int(&b, start_vert);
   nir_def *end_vert_index = nir_imm_int(&b, end_vert - 1);
   nir_def *pv_vert_index = nir_bcsel(&b, last_pv_vert_def, end_vert_index, start_vert_index);
   for (unsigned i = start_vert; i < end_vert || needs_closing; i += vert_step) {
      int idx = i < end_vert ? i : start_vert;
      /* Copy inputs to outputs. */
      for (unsigned j = 0, oj = 0, of = 0; j < VARYING_SLOT_MAX; ++j) {
         if (!scan_state.written[j])
            continue;
         /* those can't be taken as an input from the GS */
         if (j == VARYING_SLOT_LAYER ||
             j == VARYING_SLOT_VIEW_INDEX)
            continue;
         if (j == VARYING_SLOT_EDGE) {
            continue;
         }
         /* no need to use copy_var to save a lower pass */
         nir_def *index;
         if (j == VARYING_SLOT_POS || !handle_flat)
            index = nir_imm_int(&b, idx);
         else {
            unsigned mask = 1u << (of++);
            index = nir_bcsel(&b, nir_ieq_imm(&b, nir_iand_imm(&b, flat_interp_mask_def, mask), 0), nir_imm_int(&b, idx), pv_vert_index);
         }

         struct store_instr_info store_info = scan_state.store_instrs[j];
         nir_def *value = nir_load_per_vertex_input(&b, store_info.num_components, 32, index, nir_imm_int(&b, 0),
                                                    .dest_type = store_info.alu_type,
                                                    .io_semantics = store_info.io_semantics);
         nir_store_per_vertex_output(&b, value, index, nir_imm_int(&b, 0),
                                     .write_mask = nir_component_mask(store_info.num_components),
                                     .src_type = store_info.alu_type,
                                     .io_semantics = store_info.io_semantics);
         ++oj;
      }
      nir_emit_vertex(&b, 0);
      if (emulate_edgeflags) {
         assert(scan_state.written[VARYING_SLOT_EDGE]);
         struct store_instr_info store_info = scan_state.store_instrs[VARYING_SLOT_EDGE];
         nir_def *edge_value = nir_load_per_vertex_input(&b, store_info.num_components, 32,
                                                         nir_imm_int(&b, idx), nir_imm_int(&b, 0),
                                                         .dest_type = store_info.alu_type,
                                                         .io_semantics = store_info.io_semantics);
         edge_value = nir_channel(&b, edge_value, 0);
         nir_if *edge_if = nir_push_if(&b, nir_fneu_imm(&b, edge_value, 1.0));
         nir_end_primitive(&b, 0);
         nir_pop_if(&b, edge_if);
      }
      if (i >= end_vert)
         break;
   }

   nir_end_primitive(&b, 0);
   nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));
   nir_validate_shader(nir, "in nir_create_passthrough_gs");

   return nir;
}
