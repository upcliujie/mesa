/*
 * Copyright © 2021 Intel Corporation
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

#include "brw_compiler.h"
#include "brw_fs.h"
#include "brw_nir.h"
#include "brw_private.h"
#include "compiler/nir/nir_builder.h"
#include "dev/intel_debug.h"

using namespace brw;

static inline int
type_size_scalar_dwords(const struct glsl_type *type, bool bindless)
{
   return glsl_count_dword_slots(type, bindless);
}

static void
brw_nir_lower_tue_outputs(nir_shader *nir, const brw_tue_map *map)
{
   nir_foreach_shader_out_variable(var, nir) {
      int location = var->data.location;
      assert(location >= 0);
      assert(map->start_dw[location] != -1);
      var->data.driver_location = map->start_dw[location];
   }

   nir_lower_io(nir, nir_var_shader_out, type_size_scalar_dwords,
                nir_lower_io_lower_64bit_to_32);
}

static void
brw_compute_tue_map(struct nir_shader *nir, struct brw_tue_map *map)
{
   memset(map, 0, sizeof(*map));

   map->start_dw[VARYING_SLOT_TASK_COUNT] = 0;

   /* Words 1-3 are used for "Dispatch Dimensions" feature, to allow mapping a
    * 3D dispatch into the 1D dispatch supported by HW.  So ignore those.
    */

   /* From bspec: "It is suggested that SW reserve the 16 bytes following the
    * TUE Header, and therefore start the SW-defined data structure at 32B
    * alignment.  This allows the TUE Header to always be written as 32 bytes
    * with 32B alignment, the most optimal write performance case."
    */
   map->per_task_data_start_dw = 8;


   /* Compact the data: find the size associated with each location... */
   nir_foreach_shader_out_variable(var, nir) {
      const int location = var->data.location;
      if (location == VARYING_SLOT_TASK_COUNT)
         continue;
      assert(location >= VARYING_SLOT_VAR0);
      assert(location < VARYING_SLOT_MAX);

      map->start_dw[location] += type_size_scalar_dwords(var->type, false);
   }

   /* ...then assign positions using those sizes. */
   unsigned next = map->per_task_data_start_dw;
   for (unsigned i = 0; i < VARYING_SLOT_MAX; i++) {
      if (i == VARYING_SLOT_TASK_COUNT)
         continue;
      if (map->start_dw[i] == 0) {
         map->start_dw[i] = -1;
      } else {
         const unsigned size = map->start_dw[i];
         map->start_dw[i] = next;
         next += size;
      }
   }

   map->size_dw = ALIGN(next, 8);
}

static void
brw_print_tue_map(FILE *fp, const struct brw_tue_map *map)
{
   fprintf(fp, "TUE map (%d dwords)\n", map->size_dw);
   fprintf(fp, "  %4d: VARYING_SLOT_TASK_COUNT\n",
           map->start_dw[VARYING_SLOT_TASK_COUNT]);

   for (int i = VARYING_SLOT_VAR0; i < VARYING_SLOT_MAX; i++) {
      if (map->start_dw[i] != -1) {
         fprintf(fp, "  %4d: VARYING_SLOT_VAR%d\n", map->start_dw[i],
                 i - VARYING_SLOT_VAR0);
      }
   }

   fprintf(fp, "\n");
}

const unsigned *
brw_compile_task(const struct brw_compiler *compiler,
                 void *mem_ctx,
                 struct brw_compile_task_params *params)
{
   struct nir_shader *nir = params->nir;
   const struct brw_task_prog_key *key = params->key;
   struct brw_task_prog_data *prog_data = params->prog_data;
   const bool debug_enabled = INTEL_DEBUG(DEBUG_TASK);

   prog_data->base.base.stage = MESA_SHADER_TASK;
   prog_data->base.base.total_shared = nir->info.shared_size;

   prog_data->base.local_size[0] = nir->info.workgroup_size[0];
   prog_data->base.local_size[1] = nir->info.workgroup_size[1];
   prog_data->base.local_size[2] = nir->info.workgroup_size[2];

   brw_compute_tue_map(nir, &prog_data->map);

   const unsigned required_dispatch_width =
      brw_required_dispatch_width(&nir->info, key->base.subgroup_size_type);

   fs_visitor *v[3]     = {0};
   const char *error[3] = {0};

   for (unsigned simd = 0; simd < 3; simd++) {
      if (!brw_simd_should_compile(mem_ctx, simd, compiler->devinfo, &prog_data->base,
                                   required_dispatch_width, &error[simd]))
         continue;

      const unsigned dispatch_width = 8 << simd;

      nir_shader *shader = nir_shader_clone(mem_ctx, nir);
      brw_nir_apply_key(shader, compiler, &key->base, dispatch_width, true /* is_scalar */);

      NIR_PASS_V(shader, brw_nir_lower_tue_outputs, &prog_data->map);
      NIR_PASS_V(shader, brw_nir_lower_simd, dispatch_width);

      brw_postprocess_nir(shader, compiler, true /* is_scalar */, debug_enabled,
                          key->base.robust_buffer_access);

      v[simd] = new fs_visitor(compiler, params->log_data, mem_ctx, &key->base,
                               &prog_data->base.base, shader, dispatch_width,
                               -1 /* shader_time_index */, debug_enabled);

      if (prog_data->base.prog_mask) {
         unsigned first = ffs(prog_data->base.prog_mask) - 1;
         v[simd]->import_uniforms(v[first]);
      }

      const bool allow_spilling = !prog_data->base.prog_mask;

      if (v[simd]->run_task(allow_spilling))
         brw_simd_mark_compiled(simd, &prog_data->base, v[simd]->spilled_any_registers);
      else
         error[simd] = ralloc_strdup(mem_ctx, v[simd]->fail_msg);
   }

   int selected_simd = brw_simd_select(&prog_data->base);
   if (selected_simd < 0) {
      params->error_str = ralloc_asprintf(mem_ctx, "Can't compile shader: %s, %s and %s.\n",
                                          error[0], error[1], error[2]);;
      return NULL;
   }

   fs_visitor *selected = v[selected_simd];
   prog_data->base.prog_mask = 1 << selected_simd;

   if (unlikely(debug_enabled)) {
      fprintf(stderr, "Task Output ");
      brw_print_tue_map(stderr, &prog_data->map);
   }

   fs_generator g(compiler, params->log_data, mem_ctx,
                  &prog_data->base.base, false, MESA_SHADER_TASK);
   if (unlikely(debug_enabled)) {
      g.enable_debug(ralloc_asprintf(mem_ctx,
                                     "%s task shader %s",
                                     nir->info.label ? nir->info.label
                                                     : "unnamed",
                                     nir->info.name));
   }

   g.generate_code(selected->cfg, selected->dispatch_width, selected->shader_stats,
                   selected->performance_analysis.require(), params->stats);

   delete v[0];
   delete v[1];
   delete v[2];

   return g.get_assembly();
}

static void
brw_nir_lower_tue_inputs(nir_shader *nir, const brw_tue_map *map)
{
   if (!map)
      return;

   nir_foreach_shader_in_variable(var, nir) {
      int location = var->data.location;
      assert(location >= 0);
      assert(map->start_dw[location] != -1);
      var->data.driver_location = map->start_dw[location];
   }

   nir_lower_io(nir, nir_var_shader_in, type_size_scalar_dwords,
                nir_lower_io_lower_64bit_to_32);
}

const unsigned *
brw_compile_mesh(const struct brw_compiler *compiler,
                 void *mem_ctx,
                 struct brw_compile_mesh_params *params)
{
   struct nir_shader *nir = params->nir;
   const struct brw_mesh_prog_key *key = params->key;
   struct brw_mesh_prog_data *prog_data = params->prog_data;
   const bool debug_enabled = INTEL_DEBUG(DEBUG_MESH);

   prog_data->base.base.stage = MESA_SHADER_MESH;
   prog_data->base.base.total_shared = nir->info.shared_size;

   prog_data->base.local_size[0] = nir->info.workgroup_size[0];
   prog_data->base.local_size[1] = nir->info.workgroup_size[1];
   prog_data->base.local_size[2] = nir->info.workgroup_size[2];

   prog_data->max_vertices_out = nir->info.mesh.max_vertices_out;
   prog_data->max_primitives_out = nir->info.mesh.max_primitives_out;
   prog_data->primitive_type = nir->info.mesh.primitive_type;

   /* TODO(mesh): Use other index formats (that are more compact) for optimization. */
   prog_data->index_format = BRW_INDEX_FORMAT_U32;

   const unsigned required_dispatch_width =
      brw_required_dispatch_width(&nir->info, key->base.subgroup_size_type);

   fs_visitor *v[3]     = {0};
   const char *error[3] = {0};

   for (int simd = 0; simd < 3; simd++) {
      if (!brw_simd_should_compile(mem_ctx, simd, compiler->devinfo, &prog_data->base,
                                   required_dispatch_width, &error[simd]))
         continue;

      const unsigned dispatch_width = 8 << simd;

      nir_shader *shader = nir_shader_clone(mem_ctx, nir);
      brw_nir_apply_key(shader, compiler, &key->base, dispatch_width, true /* is_scalar */);

      NIR_PASS_V(shader, brw_nir_lower_tue_inputs, params->tue_map);
      NIR_PASS_V(shader, brw_nir_lower_simd, dispatch_width);

      brw_postprocess_nir(shader, compiler, true /* is_scalar */, debug_enabled,
                          key->base.robust_buffer_access);

      v[simd] = new fs_visitor(compiler, params->log_data, mem_ctx, &key->base,
                               &prog_data->base.base, shader, dispatch_width,
                               -1 /* shader_time_index */, debug_enabled);

      if (prog_data->base.prog_mask) {
         unsigned first = ffs(prog_data->base.prog_mask) - 1;
         v[simd]->import_uniforms(v[first]);
      }

      const bool allow_spilling = !prog_data->base.prog_mask;

      if (v[simd]->run_mesh(allow_spilling))
         brw_simd_mark_compiled(simd, &prog_data->base, v[simd]->spilled_any_registers);
      else
         error[simd] = ralloc_strdup(mem_ctx, v[simd]->fail_msg);
   }

   int selected_simd = brw_simd_select(&prog_data->base);
   if (selected_simd < 0) {
      params->error_str = ralloc_asprintf(mem_ctx, "Can't compile shader: %s, %s and %s.\n",
                                          error[0], error[1], error[2]);;
      return NULL;
   }

   fs_visitor *selected = v[selected_simd];
   prog_data->base.prog_mask = 1 << selected_simd;

   if (unlikely(debug_enabled)) {
      if (params->tue_map) {
         fprintf(stderr, "Mesh Input ");
         brw_print_tue_map(stderr, params->tue_map);
      }
   }

   fs_generator g(compiler, params->log_data, mem_ctx,
                  &prog_data->base.base, false, MESA_SHADER_MESH);
   if (unlikely(debug_enabled)) {
      g.enable_debug(ralloc_asprintf(mem_ctx,
                                     "%s mesh shader %s",
                                     nir->info.label ? nir->info.label
                                                     : "unnamed",
                                     nir->info.name));
   }

   g.generate_code(selected->cfg, selected->dispatch_width, selected->shader_stats,
                   selected->performance_analysis.require(), params->stats);

   delete v[0];
   delete v[1];
   delete v[2];

   return g.get_assembly();
}

static fs_reg
get_urb_handles(const fs_builder &bld, nir_intrinsic_op op)
{
   const unsigned subreg = op == nir_intrinsic_load_input ? 7 : 6;

   fs_builder ubld8 = bld.group(8, 0).exec_all();

   fs_reg h = ubld8.vgrf(BRW_REGISTER_TYPE_UD, 1);
   ubld8.MOV(h, retype(brw_vec1_grf(0, subreg), BRW_REGISTER_TYPE_UD));
   ubld8.AND(h, h, brw_imm_ud(0xFFFF));

   return h;
}

static void
emit_urb_direct_writes(const fs_builder &bld, unsigned dispatch_width,
                       nir_intrinsic_instr *instr, const fs_reg &src)
{
   assert(nir_src_bit_size(instr->src[0]) == 32);

   nir_src *offset_nir_src = nir_get_io_offset_src(instr);
   assert(nir_src_is_const(*offset_nir_src));

   fs_reg urb_handles = get_urb_handles(bld, instr->intrinsic);

   const unsigned comps = nir_src_num_components(instr->src[0]);
   assert(comps <= 4);

   const unsigned mask = nir_intrinsic_write_mask(instr);
   const unsigned offset_in_dwords =
      nir_intrinsic_base(instr) + nir_src_as_uint(*offset_nir_src);

   const unsigned offsets[] = {
      offset_in_dwords / 4,
      (offset_in_dwords + comps) / 4,
   };

   const unsigned comp_shift = offset_in_dwords % 4;
   const unsigned masks[] = {
      (mask << comp_shift) & 0xF,
      (mask >> (4 - comp_shift)) & 0xF,
   };

   const unsigned first_comps = MIN2(comps, 4 - comp_shift);
   const unsigned second_comps = comps - first_comps;

   const unsigned all_comps[] = {
      first_comps,
      second_comps,
   };

   if (masks[1])
      assert(offsets[0] != offsets[1]);

   for (unsigned i = 0; i < 2; i++) {
      if (!masks[i])
         continue;

      fs_reg payload_srcs[6];
      int prefix = 0;
      payload_srcs[prefix++] = urb_handles;
      payload_srcs[prefix++] = brw_imm_ud(masks[i] << 16);

      if (i == 0) {
         for (unsigned j = 0; j < comp_shift; j++)
            payload_srcs[prefix++] = reg_undef;
      }

      for (unsigned q = 0; q < dispatch_width / 8; q++) {
         fs_builder bld8 = bld.group(8, q);
         int x = prefix;

         const unsigned adjust = i == 1 ? all_comps[0] : 0;
         for (unsigned j = 0; j < all_comps[i]; j++) {
            fs_reg src_comp = offset(src, bld, j + adjust);
            payload_srcs[x++] = byte_offset(src_comp, q * 8 * 4);
         }

         fs_reg payload = bld8.vgrf(BRW_REGISTER_TYPE_UD, x);
         bld8.LOAD_PAYLOAD(payload, payload_srcs, x, 2);

         fs_inst *inst = bld8.emit(SHADER_OPCODE_URB_WRITE_SIMD8_MASKED, reg_undef, payload);
         inst->mlen = x;
         inst->offset = offsets[i];
      }
   }
}

static void
emit_urb_indirect_writes(const fs_builder &bld, unsigned dispatch_width,
                         nir_intrinsic_instr *instr, const fs_reg &src,
                         const fs_reg &offset_src)
{
   assert(nir_src_bit_size(instr->src[0]) == 32);

   const unsigned comps = nir_src_num_components(instr->src[0]);
   assert(comps <= 4);

   fs_reg urb_handles = get_urb_handles(bld, instr->intrinsic);

   for (unsigned i = 0; i < comps; i++) {
      if (((1 << i) & nir_intrinsic_write_mask(instr)) == 0)
         continue;

      for (unsigned q = 0; q < dispatch_width / 8; q++) {
         fs_builder bld8 = bld.group(8, q);

         fs_reg off = bld8.vgrf(BRW_REGISTER_TYPE_UD, 1);
         bld8.MOV(off, byte_offset(offset_src, 8 * q * 4));
         bld8.ADD(off, off, brw_imm_ud(i + nir_intrinsic_base(instr)));

         fs_reg mask = bld8.vgrf(BRW_REGISTER_TYPE_UD, 1);
         bld8.AND(mask, off, brw_imm_ud(0x3));

         fs_reg one = bld8.vgrf(BRW_REGISTER_TYPE_UD, 1);
         bld8.MOV(one, brw_imm_ud(1));
         bld8.SHL(mask, one, mask);
         bld8.SHL(mask, mask, brw_imm_ud(16));

         bld8.SHR(off, off, brw_imm_ud(2));

         fs_reg payload_srcs[7];
         int x = 0;
         payload_srcs[x++] = urb_handles;
         payload_srcs[x++] = off;
         payload_srcs[x++] = mask;

         fs_reg src_comp = byte_offset(offset(src, bld, i), 8 * q * 4);
         for (unsigned j = 0; j < 4; j++)
            payload_srcs[x++] = src_comp;

         fs_reg payload = bld8.vgrf(BRW_REGISTER_TYPE_UD, x);
         bld8.LOAD_PAYLOAD(payload, payload_srcs, x, 3);

         fs_inst *inst = bld8.emit(SHADER_OPCODE_URB_WRITE_SIMD8_MASKED_PER_SLOT, reg_undef, payload);
         inst->mlen = x;
         inst->offset = 0;
      }
   }
}

static void
emit_urb_direct_reads(const fs_builder &bld, unsigned dispatch_width,
                      nir_intrinsic_instr *instr, const fs_reg &dest)
{
   assert(nir_dest_bit_size(instr->dest) == 32);

   nir_src *offset_nir_src = nir_get_io_offset_src(instr);
   assert(nir_src_is_const(*offset_nir_src));

   fs_reg urb_handles = get_urb_handles(bld, instr->intrinsic);

   unsigned comps = MAX2(nir_dest_num_components(instr->dest), 1);
   unsigned offset_in_dwords = nir_intrinsic_base(instr) + nir_src_as_uint(*offset_nir_src);

   const unsigned comp_offset = offset_in_dwords % 4;
   const unsigned num_regs = comp_offset + comps;

   for (unsigned q = 0; q < dispatch_width / 8; q++) {
      fs_builder bld8 = bld.group(8, q);
      fs_reg data = bld8.vgrf(BRW_REGISTER_TYPE_UD, num_regs);

      fs_inst *inst = bld8.emit(SHADER_OPCODE_URB_READ_SIMD8, data, urb_handles);
      inst->mlen = 1;
      inst->offset = offset_in_dwords / 4;
      inst->size_written = num_regs * REG_SIZE;

      for (unsigned j = 0; j < comps; j++) {
         fs_reg dest_comp = offset(dest, bld, j);
         bld8.MOV(retype(byte_offset(dest_comp, q * 8 * 4), BRW_REGISTER_TYPE_UD),
                  offset(data, bld8, comp_offset + j));
      }
   }
}

static void
emit_urb_indirect_reads(const fs_builder &bld, unsigned dispatch_width,
                        nir_intrinsic_instr *instr, const fs_reg &dest,
                        const fs_reg &offset_src)
{
   assert(nir_dest_bit_size(instr->dest) == 32);

   fs_reg seq_ud;
   {
      fs_builder ubld8 = bld.group(8, 0).exec_all();
      seq_ud = ubld8.vgrf(BRW_REGISTER_TYPE_UD, 1);
      fs_reg seq_uw = ubld8.vgrf(BRW_REGISTER_TYPE_UW, 1);
      ubld8.MOV(seq_uw, fs_reg(brw_imm_v(0x76543210)));
      ubld8.MOV(seq_ud, seq_uw);
      ubld8.MUL(seq_ud, seq_ud, brw_imm_ud(4));
   }

   fs_reg urb_handles = get_urb_handles(bld, instr->intrinsic);

   unsigned comps = MAX2(nir_dest_num_components(instr->dest), 1);

   for (unsigned i = 0; i < comps; i++) {
      for (unsigned q = 0; q < dispatch_width / 8; q++) {
         fs_builder bld8 = bld.group(8, q);

         fs_reg off = bld8.vgrf(BRW_REGISTER_TYPE_UD, 1);
         bld8.MOV(off, byte_offset(offset_src, 8 * q * 4));
         bld8.ADD(off, off, brw_imm_ud(i + nir_intrinsic_base(instr)));

         fs_reg comp = bld8.vgrf(BRW_REGISTER_TYPE_UD, 1);
         bld8.AND(comp, off, brw_imm_ud(0x3));
         bld8.MUL(comp, comp, brw_imm_ud(REG_SIZE));
         bld8.ADD(comp, comp, seq_ud);

         bld8.SHR(off, off, brw_imm_ud(2));

         fs_reg payload_srcs[2];
         payload_srcs[0] = urb_handles;
         payload_srcs[1] = off;

         fs_reg payload = bld8.vgrf(BRW_REGISTER_TYPE_UD, 2);
         bld8.LOAD_PAYLOAD(payload, payload_srcs, 2, 2);

         fs_reg data = bld8.vgrf(BRW_REGISTER_TYPE_UD, 4);

         fs_inst *inst = bld8.emit(SHADER_OPCODE_URB_READ_SIMD8_PER_SLOT, data, payload);
         inst->mlen = 2;
         inst->offset = 0;
         inst->size_written = 4 * REG_SIZE;

         fs_reg dest_comp = offset(dest, bld, i);
         bld8.emit(SHADER_OPCODE_MOV_INDIRECT,
                   retype(byte_offset(dest_comp, q * 8 * 4), BRW_REGISTER_TYPE_UD),
                   data,
                   comp,
                   brw_imm_ud(4));
      }
   }
}

void
fs_visitor::emit_task_mesh_store(const fs_builder &bld, nir_intrinsic_instr *instr)
{
   fs_reg src = get_nir_src(instr->src[0]);
   nir_src *offset_nir_src = nir_get_io_offset_src(instr);

   if (nir_src_is_const(*offset_nir_src))
      emit_urb_direct_writes(bld, dispatch_width, instr, src);
   else
      emit_urb_indirect_writes(bld, dispatch_width, instr, src,
                               get_nir_src(*offset_nir_src));
}

void
fs_visitor::emit_task_mesh_load(const fs_builder &bld, nir_intrinsic_instr *instr)
{
   fs_reg dest = get_nir_dest(instr->dest);
   nir_src *offset_nir_src = nir_get_io_offset_src(instr);

   if (nir_src_is_const(*offset_nir_src))
      emit_urb_direct_reads(bld, dispatch_width, instr, dest);
   else
      emit_urb_indirect_reads(bld, dispatch_width, instr, dest,
                              get_nir_src(*offset_nir_src));
}

void
fs_visitor::nir_emit_task_intrinsic(const fs_builder &bld,
                                    nir_intrinsic_instr *instr)
{
   assert(stage == MESA_SHADER_TASK);

   switch (instr->intrinsic) {
   case nir_intrinsic_store_output:
      emit_task_mesh_store(bld, instr);
      break;

   case nir_intrinsic_load_output:
      emit_task_mesh_load(bld, instr);
      break;

   default:
      nir_emit_task_mesh_intrinsic(bld, instr);
      break;
   }
}

void
fs_visitor::nir_emit_mesh_intrinsic(const fs_builder &bld,
                                    nir_intrinsic_instr *instr)
{
   assert(stage == MESA_SHADER_MESH);

   switch (instr->intrinsic) {
   case nir_intrinsic_store_per_primitive_output:
   case nir_intrinsic_store_per_vertex_output:
   case nir_intrinsic_store_output:
   case nir_intrinsic_load_per_vertex_output:
   case nir_intrinsic_load_per_primitive_output:
   case nir_intrinsic_load_output:
      /* TODO(mesh): Mesh Output. */
      break;

   case nir_intrinsic_load_input:
      emit_task_mesh_load(bld, instr);
      break;

   default:
      nir_emit_task_mesh_intrinsic(bld, instr);
      break;
   }
}

void
fs_visitor::nir_emit_task_mesh_intrinsic(const fs_builder &bld,
                                         nir_intrinsic_instr *instr)
{
   assert(stage == MESA_SHADER_MESH || stage == MESA_SHADER_TASK);

   fs_reg dest;
   if (nir_intrinsic_infos[instr->intrinsic].has_dest)
      dest = get_nir_dest(instr->dest);

   switch (instr->intrinsic) {
   case nir_intrinsic_load_local_invocation_index:
   case nir_intrinsic_load_local_invocation_id:
      /* Local_ID.X is given by the HW in the shader payload. */
      dest = retype(dest, BRW_REGISTER_TYPE_UD);
      bld.MOV(dest, retype(brw_vec8_grf(1, 0), BRW_REGISTER_TYPE_UW));
      /* Task/Mesh only use one dimension. */
      if (instr->intrinsic == nir_intrinsic_load_local_invocation_id) {
         bld.MOV(offset(dest, bld, 1), brw_imm_uw(0));
         bld.MOV(offset(dest, bld, 2), brw_imm_uw(0));
      }
      break;

   default:
      nir_emit_cs_intrinsic(bld, instr);
      break;
   }
}
