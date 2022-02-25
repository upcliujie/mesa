/*
 * Copyright © 2022 Intel Corporation
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

#ifndef GENX_CMD_GENERATED_INDIRECT_DRAW_H
#define GENX_CMD_GENERATED_INDIRECT_DRAW_H

#include <assert.h>
#include <stdbool.h>

#include "anv_private.h"

#if GFX_VER < 12
#error "Generated draws optimization not supported prior to Gfx12"
#endif

static void
genX(cmd_buffer_init_generate_draws)(struct anv_cmd_buffer *cmd_buffer)
{
   struct anv_batch *gen_batch = &cmd_buffer->generation_batch;
   struct anv_device *device = cmd_buffer->device;
   struct intel_device_info *devinfo = &device->info;

   /* Disable all shader stages */
   const struct brw_vs_prog_data *vs_prog_data =
      brw_vs_prog_data_const(device->generated_draw_kernel->prog_data);

   anv_batch_emit(gen_batch, GENX(3DSTATE_VS), vs) {
      vs.Enable               = true;
      vs.KernelStartPointer   = device->generated_draw_kernel->kernel.offset;
#if GFX_VER >= 8
      vs.SIMD8DispatchEnable  =
         vs_prog_data->base.dispatch_mode == DISPATCH_MODE_SIMD8;
#endif

#if GFX_VER < 11
      vs.SingleVertexDispatch       = false;
#endif
      vs.VectorMaskEnable           = false;
      vs.FloatingPointMode          = IEEE754;
      vs.MaximumNumberofThreads     = devinfo->max_vs_threads - 1;

      vs.VertexURBEntryReadLength      = vs_prog_data->base.urb_read_length;
      vs.VertexURBEntryReadOffset      = 0;
      vs.DispatchGRFStartRegisterForURBData =
         vs_prog_data->base.base.dispatch_grf_start_reg;
   }
   anv_batch_emit(gen_batch, GENX(3DSTATE_HS), hs);
   anv_batch_emit(gen_batch, GENX(3DSTATE_TE), te);
   anv_batch_emit(gen_batch, GENX(3DSTATE_DS), DS);
   anv_batch_emit(gen_batch, GENX(3DSTATE_GS), gs);
   anv_batch_emit(gen_batch, GENX(3DSTATE_PS), gs);

   anv_batch_emit(gen_batch, GENX(3DSTATE_SBE), sbe);

   /* Emit URB setup.  We tell it that the VS is active because we want it to
    * allocate space for the VS.  Even though one isn't run, we need VUEs to
    * store the data that VF is going to pass to SOL.
    */
   const unsigned entry_size[4] = { DIV_ROUND_UP(32, 64), 1, 1, 1 };

   genX(emit_l3_config)(gen_batch, device, device->generated_draw_l3_config);

   cmd_buffer->state.current_l3_config = device->generated_draw_l3_config;

   genX(emit_urb_setup)(device, gen_batch, device->generated_draw_l3_config,
                        VK_SHADER_STAGE_VERTEX_BIT, entry_size, NULL);

#if GFX_VER >= 12
   /* Disable Primitive Replication. */
   anv_batch_emit(gen_batch, GENX(3DSTATE_PRIMITIVE_REPLICATION), pr);
#endif

   anv_batch_emit(gen_batch, GENX(3DSTATE_VF_TOPOLOGY), topo) {
      topo.PrimitiveTopologyType = _3DPRIM_POINTLIST;
   }
   anv_batch_emit(gen_batch, GENX(3DSTATE_VF_STATISTICS), vf) {
      vf.StatisticsEnable = false;
   }

   anv_batch_emit(gen_batch, GENX(3DSTATE_PUSH_CONSTANT_ALLOC_VS), alloc) {
      alloc.ConstantBufferOffset = 0;
      alloc.ConstantBufferSize   = cmd_buffer->device->info.max_constant_urb_size_kb;
   }

   uint32_t *dw = anv_batch_emitn(gen_batch,
                                  1 + 3 * GENX(VERTEX_ELEMENT_STATE_length),
                                  GENX(3DSTATE_VERTEX_ELEMENTS));
   GENX(VERTEX_ELEMENT_STATE_pack)(gen_batch, dw + 1,
      &(struct GENX(VERTEX_ELEMENT_STATE)) {
         .VertexBufferIndex   = 0,
         .Valid               = true,
         .SourceElementFormat = ISL_FORMAT_R32G32B32A32_UINT,
         .SourceElementOffset = 0,
         .Component0Control   = VFCOMP_STORE_SRC,
         .Component1Control   = VFCOMP_STORE_SRC,
         .Component2Control   = VFCOMP_STORE_SRC,
         .Component3Control   = VFCOMP_STORE_SRC,
      });
   GENX(VERTEX_ELEMENT_STATE_pack)(gen_batch, dw + 1 + GENX(VERTEX_ELEMENT_STATE_length),
      &(struct GENX(VERTEX_ELEMENT_STATE)) {
         .VertexBufferIndex   = 1,
         .Valid               = true,
         .SourceElementFormat = ISL_FORMAT_R32_UINT,
         .SourceElementOffset = 0,
         .Component0Control   = VFCOMP_STORE_SRC,
         .Component1Control   = VFCOMP_STORE_0,
         .Component2Control   = VFCOMP_STORE_0,
         .Component3Control   = VFCOMP_STORE_0,
      });
   GENX(VERTEX_ELEMENT_STATE_pack)(gen_batch, dw + 1 + 2 * GENX(VERTEX_ELEMENT_STATE_length),
      &(struct GENX(VERTEX_ELEMENT_STATE)) {
         .VertexBufferIndex   = ANV_DRAWID_VB_INDEX,
         .Valid               = true,
         .SourceElementFormat = ISL_FORMAT_R32_UINT,
         .SourceElementOffset = 0,
         .Component0Control   = VFCOMP_STORE_SRC,
         .Component1Control   = VFCOMP_STORE_0,
         .Component2Control   = VFCOMP_STORE_0,
         .Component3Control   = VFCOMP_STORE_0,
      });
   anv_batch_emit(gen_batch, GENX(3DSTATE_VF_INSTANCING), vfi) {
      vfi.InstancingEnable   = false;
      vfi.VertexElementIndex = 0;
   }
   anv_batch_emit(gen_batch, GENX(3DSTATE_VF_INSTANCING), vfi) {
      vfi.InstancingEnable   = false;
      vfi.VertexElementIndex = 1;
   }
   anv_batch_emit(gen_batch, GENX(3DSTATE_VF_SGVS), sgvs) {
      assert(vs_prog_data->uses_vertexid);
      sgvs.VertexIDEnable              = true;
      sgvs.VertexIDElementOffset       = 2;
      sgvs.VertexIDComponentNumber     = 0;
   }
   anv_batch_emit(gen_batch, GENX(3DSTATE_VF_SGVS_2), sgvs);

   anv_batch_emit(gen_batch, GENX(MI_ARB_CHECK), arb) {
      arb.PreParserDisableMask = true;
      arb.PreParserDisable = true;
   }
}

#if GFX_VER >= 12
#define NULL_VERTEX_BUFFER(idx) \
   (struct GENX(VERTEX_BUFFER_STATE)) { \
      .VertexBufferIndex     = idx, \
      .MOCS                  = anv_mocs(device, NULL, 0), \
      .L3BypassDisable       = true, \
      .NullVertexBuffer      = true, \
   }
#else
#define NULL_VERTEX_BUFFER(idx) \
   (struct GENX(VERTEX_BUFFER_STATE)) { \
      .VertexBufferIndex     = idx, \
      .MOCS                  = anv_mocs(device, NULL, 0), \
      .NullVertexBuffer      = true, \
   }
#endif


static struct anv_generated_indirect_draw_params *
genX(cmd_buffer_emit_generate_draws)(struct anv_cmd_buffer *cmd_buffer,
                                     struct anv_address indirect_data_addr,
                                     uint32_t indirect_data_stride,
                                     uint32_t draw_count,
                                     bool indexed)
{
   struct anv_device *device = cmd_buffer->device;
   struct anv_batch *gen_batch = &cmd_buffer->generation_batch;

   if (anv_address_is_null(cmd_buffer->generation_return_addr)) {
      anv_batch_emit_ensure_space(gen_batch, 4);

      trace_intel_begin_generate_draws(&cmd_buffer->trace, cmd_buffer);

      anv_batch_emit(&cmd_buffer->batch, GENX(MI_BATCH_BUFFER_START), bbs) {
         bbs.AddressSpaceIndicator = ASI_PPGTT;
         bbs.BatchBufferStartAddress = anv_batch_current_address(gen_batch);
      }

      cmd_buffer->generation_return_addr = anv_batch_current_address(&cmd_buffer->batch);

      trace_intel_end_generate_draws(&cmd_buffer->trace, cmd_buffer);

      /* Mark dirty all the states we're going to touch in this function. */
      cmd_buffer->state.gfx.dirty |= ANV_CMD_DIRTY_PIPELINE | ANV_CMD_DIRTY_DYNAMIC_ALL;
      cmd_buffer->state.push_constants_dirty |= VK_SHADER_STAGE_VERTEX_BIT;
      genX(cmd_buffer_init_generate_draws)(cmd_buffer);
   }

   uint32_t *dw = anv_batch_emitn(gen_batch,
                                  1 + 3 * GENX(VERTEX_BUFFER_STATE_length),
                                  GENX(3DSTATE_VERTEX_BUFFERS));
   GENX(VERTEX_BUFFER_STATE_pack)(gen_batch,
                                  dw + 1 + 0 * GENX(VERTEX_BUFFER_STATE_length),
                                  &(struct GENX(VERTEX_BUFFER_STATE)) {
         .VertexBufferIndex     = 0, /* Reserved for this */
         .AddressModifyEnable   = true,
         .BufferStartingAddress = indirect_data_addr,
         .BufferPitch           = indirect_data_stride,
         .MOCS                  = anv_mocs(device, indirect_data_addr.bo, 0),
#if GFX_VER >= 12
         .L3BypassDisable       = true,
#endif
         .BufferSize            = draw_count * indirect_data_stride,
      });

   /* The second vertex buffer is either null or points to the same buffer as
    * the first vertex buffer only offset by 16 bytes to capture the
    * VkDrawIndexedIndirectCommand::firstInstance value.
    */
   struct GENX(VERTEX_BUFFER_STATE) second_buffer;
   if (indexed) {
      second_buffer = (struct GENX(VERTEX_BUFFER_STATE)) {
         .VertexBufferIndex     = 1, /* Reserved for this */
         .AddressModifyEnable   = true,
         .BufferStartingAddress = anv_address_add(indirect_data_addr, 16),
         .BufferPitch           = indirect_data_stride,
         .MOCS                  = anv_mocs(device, indirect_data_addr.bo, 0),
#if GFX_VER >= 12
         .L3BypassDisable       = true,
#endif
         .BufferSize            = draw_count * indirect_data_stride,
      };
   } else {
      second_buffer = NULL_VERTEX_BUFFER(1);
   }
   GENX(VERTEX_BUFFER_STATE_pack)(gen_batch,
                                  dw + 1 + 1 * GENX(VERTEX_BUFFER_STATE_length),
                                  &second_buffer);
   GENX(VERTEX_BUFFER_STATE_pack)(gen_batch,
                                  dw + 1 + 2 * GENX(VERTEX_BUFFER_STATE_length),
                                  &NULL_VERTEX_BUFFER(2));

   struct anv_state push_data_state =
      anv_cmd_buffer_alloc_dynamic_state(
         cmd_buffer,
         sizeof(struct anv_generated_indirect_draw_params), 32);

   UNUSED uint32_t mocs = anv_mocs(device, NULL, 0);
   anv_batch_emit(gen_batch, GENX(3DSTATE_CONSTANT_VS), c) {
      c.MOCS = mocs;
      c.ConstantBody.ReadLength[3] = 1;
      c.ConstantBody.Buffer[3] = (struct anv_address) {
         .bo = cmd_buffer->device->dynamic_state_pool.block_pool.bo,
         .offset = push_data_state.offset,
      };
   }

   anv_batch_emit(gen_batch, GENX(3DPRIMITIVE), prim) {
      prim.VertexAccessType         = SEQUENTIAL;
      prim.PrimitiveTopologyType    = _3DPRIM_POINTLIST;
      prim.VertexCountPerInstance   = draw_count;
      prim.StartVertexLocation      = 0;
      prim.InstanceCount            = 1;
      prim.StartInstanceLocation    = 0;
      prim.BaseVertexLocation       = 0;
   }

   return push_data_state.map;
}

static void
genX(cmd_buffer_emit_indirect_generated_draws)(struct anv_cmd_buffer *cmd_buffer,
                                               struct anv_address indirect_data_addr,
                                               uint32_t indirect_data_stride,
                                               uint32_t draw_count,
                                               bool indexed)
{
   genX(flush_pipeline_select_3d)(cmd_buffer);

   /* In order to have the vertex fetch gather the data we need to have a non
    * 0 stride. It's possible to have a 0 stride given by the application when
    * draw_count is 1, but we need a correct value for the
    * VERTEX_BUFFER_STATE::BufferPitch, so ensure the caller set this
    * correctly :
    *
    * Vulkan spec, vkCmdDrawIndirect:
    *
    *   "If drawCount is less than or equal to one, stride is ignored."
    */
   assert(indirect_data_stride > 0);

   struct anv_generated_indirect_draw_params *generate_data =
      genX(cmd_buffer_emit_generate_draws)(
         cmd_buffer, indirect_data_addr,
         indirect_data_stride,
         draw_count,
         indexed);

   /* Emit the 3D in the main batch. */
   genX(cmd_buffer_flush_state)(cmd_buffer);

   if (cmd_buffer->state.conditional_render_enabled)
      genX(cmd_emit_conditional_render_predicate)(cmd_buffer);

   const uint32_t draw_cmd_stride = 4 * GENX(3DPRIMITIVE_EXTENDED_length);

   /* Ensure we have enough contiguous space for all the draws so that the
    * compute shader can edit all the 3DPRIMITIVEs from a single base
    * address.
    *
    * TODO: we might have to split that if the amount of space is to large (at
    *       1Mb?).
    */
   VkResult result = anv_batch_emit_ensure_space(&cmd_buffer->batch,
                                                 draw_count * draw_cmd_stride);
   if (result != VK_SUCCESS)
      return;

   struct anv_address draw_cmds_addr =
      anv_batch_current_address(&cmd_buffer->batch);

   for (uint32_t i = 0; i < draw_count; i++) {
      anv_batch_emit(&cmd_buffer->batch, GENX(3DPRIMITIVE_EXTENDED), prim) {
         prim.IndirectParameterEnable   = false;
         prim.PredicateEnable           = cmd_buffer->state.conditional_render_enabled;
         prim.VertexAccessType          = indexed ? RANDOM : SEQUENTIAL;
         prim.PrimitiveTopologyType     = cmd_buffer->state.gfx.primitive_topology;
         prim.ExtendedParametersPresent = true;
      }
   }

   *generate_data = (struct anv_generated_indirect_draw_params) {
      .generated_cmd_addr   = anv_address_physical(draw_cmds_addr),
      .generated_cmd_stride = draw_cmd_stride,
      .indexed              = indexed,
      .multiview_multiplier = anv_cmd_buffer_get_view_count(cmd_buffer),
   };
}

static void
genX(cmd_buffer_flush_generated_draws)(struct anv_cmd_buffer *cmd_buffer)
{
   /* No return address setup means we don't have to do anything */
   if (anv_address_is_null(cmd_buffer->generation_return_addr))
      return;

   /* Wait for all the generation vertex shader to generate the commands. */
   genX(emit_apply_pipe_flushes)(&cmd_buffer->generation_batch,
                                 cmd_buffer->device,
                                 _3D,
                                 ANV_PIPE_DATA_CACHE_FLUSH_BIT |
                                 ANV_PIPE_CS_STALL_BIT);

   anv_batch_emit(&cmd_buffer->generation_batch, GENX(MI_ARB_CHECK), arb) {
      arb.PreParserDisableMask = true;
      arb.PreParserDisable = false;
   }

   /* Return to the main batch. */
   anv_batch_emit(&cmd_buffer->generation_batch,
                  GENX(MI_BATCH_BUFFER_START), bbs) {
      bbs.AddressSpaceIndicator = ASI_PPGTT;
      bbs.BatchBufferStartAddress = cmd_buffer->generation_return_addr;
   }

   cmd_buffer->generation_return_addr = ANV_NULL_ADDRESS;
}

#endif /* GENX_CMD_GENERATED_INDIRECT_DRAW_H */
