/*
 * Copyright © 2024 Collabora Ltd.
 *
 * Derived from tu_cmd_buffer.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "genxml/gen_macros.h"

#include "panvk_cmd_buffer.h"
#include "panvk_cmd_desc_state.h"
#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_physical_device.h"

#include "pan_desc.h"
#include "pan_encoder.h"
#include "pan_jc.h"
#include "pan_pool.h"
#include "pan_props.h"

#include <vulkan/vulkan_core.h>

struct panvk_dispatch_info {
   struct pan_compute_dim wg_count;
   mali_ptr tsd;
   mali_ptr push_uniforms;
};

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDispatchBase)(VkCommandBuffer commandBuffer,
                                uint32_t baseGroupX, uint32_t baseGroupY,
                                uint32_t baseGroupZ, uint32_t groupCountX,
                                uint32_t groupCountY, uint32_t groupCountZ)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   const struct panvk_shader *shader = cmdbuf->state.compute.shader;

   if (groupCountX == 0 || groupCountY == 0 || groupCountZ == 0)
      return;

   /* If there's no compute shader, we can skip the dispatch. */
   if (!panvk_priv_mem_dev_addr(shader->rsd))
      return;

   struct panvk_device *dev = to_panvk_device(cmdbuf->vk.base.device);
   struct panvk_physical_device *phys_dev =
      to_panvk_physical_device(dev->vk.physical);
   struct panvk_dispatch_info dispatch = {
      .wg_count = {groupCountX, groupCountY, groupCountZ},
   };

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   struct panvk_batch *batch = panvk_per_arch(cmd_open_batch)(cmdbuf);

   struct panvk_descriptor_state *desc_state =
      &cmdbuf->state.compute.desc_state;
   struct panvk_shader_desc_state *cs_desc_state =
      &cmdbuf->state.compute.cs.desc;

   panvk_per_arch(cmd_alloc_tls_desc)(cmdbuf, false);
   dispatch.tsd = batch->tls.gpu;

   panvk_per_arch(cmd_prepare_push_descs)(&cmdbuf->desc_pool.base, desc_state,
                                          shader->desc_info.used_set_mask);

   struct panvk_compute_sysvals *sysvals = &cmdbuf->state.compute.sysvals;
   sysvals->base.x = baseGroupX;
   sysvals->base.y = baseGroupY;
   sysvals->base.z = baseGroupZ;
   sysvals->num_work_groups.x = groupCountX;
   sysvals->num_work_groups.y = groupCountY;
   sysvals->num_work_groups.z = groupCountZ;
   sysvals->local_group_size.x = shader->local_size.x;
   sysvals->local_group_size.y = shader->local_size.y;
   sysvals->local_group_size.z = shader->local_size.z;
   panvk_per_arch(cmd_prepare_dyn_ssbos)(&cmdbuf->desc_pool.base, desc_state,
                                         shader, cs_desc_state);
   sysvals->desc.dyn_ssbos = cs_desc_state->dyn_ssbos;

   for (uint32_t i = 0; i < MAX_SETS; i++) {
      if (shader->desc_info.used_set_mask & BITFIELD_BIT(i))
         sysvals->desc.sets[i] = desc_state->sets[i]->descs.dev;
   }

   cmdbuf->state.compute.push_uniforms = 0;

   if (!cmdbuf->state.compute.push_uniforms) {
      cmdbuf->state.compute.push_uniforms = panvk_cmd_prepare_push_uniforms(
         &cmdbuf->desc_pool.base, &cmdbuf->state.push_constants,
         &cmdbuf->state.compute.sysvals, sizeof(cmdbuf->state.compute.sysvals));
   }

   dispatch.push_uniforms = cmdbuf->state.compute.push_uniforms;

   panvk_per_arch(cmd_prepare_shader_desc_tables)(
      &cmdbuf->desc_pool.base, desc_state, shader, cs_desc_state);

   struct panfrost_ptr copy_desc_job = panvk_per_arch(meta_get_copy_desc_job)(
      dev, &cmdbuf->desc_pool.base, shader, &cmdbuf->state.compute.desc_state,
      cs_desc_state, 0);

   if (copy_desc_job.cpu)
      util_dynarray_append(&batch->jobs, void *, copy_desc_job.cpu);

   struct panfrost_ptr job =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, COMPUTE_JOB);
   util_dynarray_append(&batch->jobs, void *, job.cpu);

   panfrost_pack_work_groups_compute(
      pan_section_ptr(job.cpu, COMPUTE_JOB, INVOCATION), dispatch.wg_count.x,
      dispatch.wg_count.y, dispatch.wg_count.z, shader->local_size.x,
      shader->local_size.y, shader->local_size.z, false, false);

   pan_section_pack(job.cpu, COMPUTE_JOB, PARAMETERS, cfg) {
      cfg.job_task_split = util_logbase2_ceil(shader->local_size.x + 1) +
                           util_logbase2_ceil(shader->local_size.y + 1) +
                           util_logbase2_ceil(shader->local_size.z + 1);
   }

   pan_section_pack(job.cpu, COMPUTE_JOB, DRAW, cfg) {
      cfg.state = panvk_priv_mem_dev_addr(shader->rsd);
      cfg.attributes = cs_desc_state->img_attrib_table;
      cfg.attribute_buffers =
         cs_desc_state->tables[PANVK_BIFROST_DESC_TABLE_IMG];
      cfg.thread_storage = dispatch.tsd;
      cfg.uniform_buffers = cs_desc_state->tables[PANVK_BIFROST_DESC_TABLE_UBO];
      cfg.push_uniforms = dispatch.push_uniforms;
      cfg.textures = cs_desc_state->tables[PANVK_BIFROST_DESC_TABLE_TEXTURE];
      cfg.samplers = cs_desc_state->tables[PANVK_BIFROST_DESC_TABLE_SAMPLER];
   }

   unsigned copy_desc_dep =
      copy_desc_job.gpu
         ? pan_jc_add_job(&batch->vtc_jc, MALI_JOB_TYPE_COMPUTE, false, false,
                          0, 0, &copy_desc_job, false)
         : 0;

   pan_jc_add_job(&batch->vtc_jc, MALI_JOB_TYPE_COMPUTE, false, false, 0,
                  copy_desc_dep, &job, false);

   batch->tlsinfo.tls.size = shader->info.tls_size;
   batch->tlsinfo.wls.size = shader->info.wls_size;
   if (batch->tlsinfo.wls.size) {
      unsigned core_id_range;

      panfrost_query_core_count(&phys_dev->kmod.props, &core_id_range);
      batch->tlsinfo.wls.instances = pan_wls_instances(&dispatch.wg_count);
      batch->wls_total_size = pan_wls_adjust_size(batch->tlsinfo.wls.size) *
                              batch->tlsinfo.wls.instances * core_id_range;
   }

   panvk_per_arch(cmd_close_batch)(cmdbuf);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDispatchIndirect)(VkCommandBuffer commandBuffer,
                                    VkBuffer _buffer, VkDeviceSize offset)
{
   panvk_stub();
}

void
panvk_per_arch(cmd_meta_compute_start)(
   struct panvk_cmd_buffer *cmdbuf,
   struct panvk_cmd_meta_compute_save_ctx *save_ctx)
{
   const struct panvk_descriptor_set *set0 =
      cmdbuf->state.compute.desc_state.sets[0];
   struct panvk_descriptor_set *push_set0 =
      cmdbuf->state.compute.desc_state.push_sets[0];

   save_ctx->set0 = set0;
   if (push_set0 && push_set0 == set0) {
      save_ctx->push_set0.desc_count = push_set0->desc_count;
      save_ctx->push_set0.descs_dev_addr = push_set0->descs.dev;
      memcpy(save_ctx->push_set0.desc_storage, push_set0->descs.host,
             push_set0->desc_count * PANVK_DESCRIPTOR_SIZE);
   }

   save_ctx->push_constants = cmdbuf->state.push_constants;
   save_ctx->cs.shader = cmdbuf->state.compute.shader;
   save_ctx->cs.desc = cmdbuf->state.compute.cs.desc;
}

void
panvk_per_arch(cmd_meta_compute_end)(
   struct panvk_cmd_buffer *cmdbuf,
   const struct panvk_cmd_meta_compute_save_ctx *save_ctx)
{
   struct panvk_descriptor_set *push_set0 =
      cmdbuf->state.gfx.desc_state.push_sets[0];

   cmdbuf->state.gfx.desc_state.sets[0] = save_ctx->set0;
   if (save_ctx->push_set0.desc_count) {
      memcpy(push_set0->descs.host, save_ctx->push_set0.desc_storage,
             save_ctx->push_set0.desc_count * PANVK_DESCRIPTOR_SIZE);
      push_set0->descs.dev = save_ctx->push_set0.descs_dev_addr;
      push_set0->desc_count = save_ctx->push_set0.desc_count;
   }

   if (memcmp(cmdbuf->state.push_constants.data, save_ctx->push_constants.data,
              sizeof(cmdbuf->state.push_constants.data))) {
      cmdbuf->state.push_constants = save_ctx->push_constants;
      cmdbuf->state.compute.push_uniforms = 0;
      cmdbuf->state.gfx.push_uniforms = 0;
   }

   cmdbuf->state.compute.shader = save_ctx->cs.shader;
   cmdbuf->state.compute.cs.desc = save_ctx->cs.desc;
}
