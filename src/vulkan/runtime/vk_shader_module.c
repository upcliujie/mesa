/*
 * Copyright © 2017 Intel Corporation
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

#include "vk_shader_module.h"

#include "compiler/spirv/nir_spirv.h"
#include "util/mesa-sha1.h"
#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_log.h"
#include "vk_util.h"

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateShaderModule(VkDevice _device,
                             const VkShaderModuleCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator,
                             VkShaderModule *pShaderModule)
{
    VK_FROM_HANDLE(vk_device, device, _device);
    struct vk_shader_module *module;

    assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    assert(pCreateInfo->flags == 0);

    module = vk_object_alloc(device, pAllocator,
                             sizeof(*module) + pCreateInfo->codeSize,
                             VK_OBJECT_TYPE_SHADER_MODULE);
    if (module == NULL)
       return VK_ERROR_OUT_OF_HOST_MEMORY;

    module->size = pCreateInfo->codeSize;
    module->nir = NULL;
    memcpy(module->data, pCreateInfo->pCode, module->size);

    _mesa_sha1_compute(module->data, module->size, module->sha1);

    *pShaderModule = vk_shader_module_to_handle(module);

    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_DestroyShaderModule(VkDevice _device,
                              VkShaderModule _module,
                              const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_shader_module, module, _module);

   if (!module)
      return;

   /* NIR modules (which are only created internally by the driver) are not
    * dynamically allocated so we should never call this for them.
    * Instead the driver is responsible for freeing the NIR code when it is
    * no longer needed.
    */
   assert(module->nir == NULL);

   vk_object_free(device, pAllocator, module);
}

#define SPIR_V_MAGIC_NUMBER 0x07230203

uint32_t
vk_shader_module_spirv_version(const struct vk_shader_module *mod)
{
   if (mod->nir != NULL)
      return 0;

   uint32_t *spirv = (uint32_t *) mod->data;
   assert(mod->size >= 8);
   assert(spirv[0] == SPIR_V_MAGIC_NUMBER);
   return spirv[1];
}

static void
spirv_nir_debug(void *private_data,
                enum nir_spirv_debug_level level,
                size_t spirv_offset,
                const char *message)
{
   const struct vk_shader_module *mod = private_data;

   switch (level) {
   case NIR_SPIRV_DEBUG_LEVEL_INFO:
      //vk_logi(VK_LOG_OBJS(mode), "SPIR-V offset %lu: %s",
      //        (unsigned long) spirv_offset, message);
      break;
   case NIR_SPIRV_DEBUG_LEVEL_WARNING:
      vk_logw(VK_LOG_OBJS(mod), "SPIR-V offset %lu: %s",
              (unsigned long) spirv_offset, message);
      break;
   case NIR_SPIRV_DEBUG_LEVEL_ERROR:
      vk_loge(VK_LOG_OBJS(mod), "SPIR-V offset %lu: %s",
              (unsigned long) spirv_offset, message);
      break;
   default:
      break;
   }
}

VkResult
vk_shader_module_to_nir(struct vk_device *device,
                        const struct vk_shader_module *mod,
                        gl_shader_stage stage,
                        const char *entrypoint_name,
                        const VkSpecializationInfo *spec_info,
                        const struct spirv_to_nir_options *spirv_options,
                        const nir_shader_compiler_options *nir_options,
                        void *mem_ctx, nir_shader **nir_out)
{
   if (mod->nir != NULL) {
      assert(mod->nir->info.stage == stage);
      assert(exec_list_length(&mod->nir->functions) == 1);
      ASSERTED const char *nir_name =
         nir_shader_get_entrypoint(mod->nir)->function->name;
      assert(strcmp(nir_name, entrypoint_name) == 0);

      nir_validate_shader(mod->nir, "internal shader");

      nir_shader *clone = nir_shader_clone(mem_ctx, mod->nir);
      if (clone == NULL)
         return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

      assert(clone->options == NULL || clone->options == nir_options);
      clone->options = nir_options;

      *nir_out = clone;
      return VK_SUCCESS;
   }

   uint32_t *spirv = (uint32_t *) mod->data;
   assert(spirv[0] == SPIR_V_MAGIC_NUMBER);
   assert(mod->size % 4 == 0);

   struct spirv_to_nir_options spirv_options_local = *spirv_options;
   spirv_options_local.debug.func = spirv_nir_debug;
   spirv_options_local.debug.private_data = (void *)mod;

   uint32_t num_spec_entries = 0;
   struct nir_spirv_specialization *spec_entries =
      vk_spec_info_to_nir_spirv(spec_info, &num_spec_entries);

   nir_shader *nir = spirv_to_nir(spirv, mod->size / 4,
                                  spec_entries, num_spec_entries,
                                  stage, entrypoint_name,
                                  &spirv_options_local, nir_options);
   free(spec_entries);

   if (nir == NULL)
      return vk_errorf(device, VK_ERROR_UNKNOWN, "spirv_to_nir failed");

   assert(nir->info.stage == stage);
   nir_validate_shader(nir, "after spirv_to_nir");
   nir_validate_ssa_dominance(nir, "after spirv_to_nir");
   if (mem_ctx != NULL)
      ralloc_steal(mem_ctx, nir);

   /* We have to lower away local constant initializers right before we
    * inline functions.  That way they get properly initialized at the top
    * of the function and not at the top of its caller.
    */
   NIR_PASS_V(nir, nir_lower_variable_initializers, nir_var_function_temp);
   NIR_PASS_V(nir, nir_lower_returns);
   NIR_PASS_V(nir, nir_inline_functions);
   NIR_PASS_V(nir, nir_copy_prop);
   NIR_PASS_V(nir, nir_opt_deref);

   /* Pick off the single entrypoint that we want */
   foreach_list_typed_safe(nir_function, func, node, &nir->functions) {
      if (!func->is_entrypoint)
         exec_node_remove(&func->node);
   }
   assert(exec_list_length(&nir->functions) == 1);

   /* Now that we've deleted all but the main function, we can go ahead and
    * lower the rest of the constant initializers.  We do this here so that
    * nir_remove_dead_variables and split_per_member_structs below see the
    * corresponding stores.
    */
   NIR_PASS_V(nir, nir_lower_variable_initializers, ~0);

   /* Split member structs.  We do this before lower_io_to_temporaries so that
    * it doesn't lower system values to temporaries by accident.
    */
   NIR_PASS_V(nir, nir_split_var_copies);
   NIR_PASS_V(nir, nir_split_per_member_structs);

   NIR_PASS_V(nir, nir_remove_dead_variables,
              nir_var_shader_in | nir_var_shader_out | nir_var_system_value |
              nir_var_shader_call_data | nir_var_ray_hit_attrib,
              NULL);

   NIR_PASS_V(nir, nir_propagate_invariant, false);
   NIR_PASS_V(nir, nir_lower_io_to_temporaries,
              nir_shader_get_entrypoint(nir), true, false);

   *nir_out = nir;
   return VK_SUCCESS;
}
