/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "dzn_private.h"

#include "vk_alloc.h"

dzn_shader_module::dzn_shader_module(dzn_device *device,
                                     const VkShaderModuleCreateInfo *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);
   assert(pCreateInfo->codeSize % 4 == 0);

   code_size = pCreateInfo->codeSize;
   memcpy(code, pCreateInfo->pCode, pCreateInfo->codeSize);
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_SHADER_MODULE);
}


dzn_shader_module::~dzn_shader_module()
{
   vk_object_base_finish(&base);
}

dzn_shader_module *
dzn_shader_module_factory::allocate(dzn_device *device,
                                    const VkShaderModuleCreateInfo *pCreateInfo,
                                    const VkAllocationCallbacks *pAllocator)
{
   return (dzn_shader_module *)
      vk_alloc2(&device->vk.alloc, pAllocator,
                sizeof(dzn_shader_module) + pCreateInfo->codeSize,
                alignof(dzn_shader_module),
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateShaderModule(VkDevice device,
                       const VkShaderModuleCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator,
                       VkShaderModule *pShaderModule)
{
   return dzn_shader_module_factory::create(device, pCreateInfo,
                                            pAllocator, pShaderModule);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyShaderModule(VkDevice device,
                        VkShaderModule mod,
                        const VkAllocationCallbacks *pAllocator)
{
   return dzn_shader_module_factory::destroy(device, mod, pAllocator);
}

