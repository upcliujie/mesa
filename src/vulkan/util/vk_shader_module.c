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

#include "util/log.h"
#include "util/mesa-sha1.h"
#include "vk_alloc.h"
#include "vk_common_entrypoints.h"
#include "vk_debug_report.h"
#include "vk_device.h"
#include "vk_physical_device.h"
#include "vk_shader_module.h"
#include "vk_util.h"

#include "vk_enum_to_str.h"

///////// TEMPORARY

static inline struct vk_instance *
vk_device_instance_or_null(const struct vk_device *device)
{
   return device ? device->physical->instance : NULL;
}

/* Whenever we generate an error, pass it through this function. Useful for
 * debugging, where we can break on it. Only call at error site, not when
 * propagating errors. Might be useful to plug in a stack trace here.
 */


#ifdef DEBUG
#define vk_error(error) __vk_errorf(NULL, NULL, error, __FILE__, __LINE__, NULL)
#define vk_errorfi(instance, obj, error, format, ...)\
    __vk_errorf(instance, obj, error,\
                __FILE__, __LINE__, format, ## __VA_ARGS__)
#define vk_errorf(device, obj, error, format, ...)\
   vk_errorfi(vk_device_instance_or_null(device),\
              obj, error, format, ## __VA_ARGS__)
#else

static inline VkResult __dummy_vk_error(VkResult error, UNUSED const void *ignored)
{
   return error;
}

#define vk_error(error) __dummy_vk_error(error, NULL)
#define vk_errorfi(instance, obj, error, format, ...) __dummy_vk_error(error, instance)
#define vk_errorf(device, obj, error, format, ...) __dummy_vk_error(error, device)
#endif

static VkResult
__vk_errorv(struct vk_instance *instance,
            const struct vk_object_base *object, VkResult error,
            const char *file, int line, const char *format, va_list ap)
{
   char buffer[256];
   char report[512];

   const char *error_str = vk_Result_to_str(error);

   if (format) {
      vsnprintf(buffer, sizeof(buffer), format, ap);

      snprintf(report, sizeof(report), "%s:%d: %s (%s)", file, line, buffer,
               error_str);
   } else {
      snprintf(report, sizeof(report), "%s:%d: %s", file, line, error_str);
   }

   if (instance) {
      vk_debug_report(instance, VK_DEBUG_REPORT_ERROR_BIT_EXT,
                      object, line, 0, "anv", report);
   }

   mesa_loge("%s", report);

   return error;
}

static VkResult
__vk_errorf(struct vk_instance *instance,
            const struct vk_object_base *object, VkResult error,
            const char *file, int line, const char *format, ...)
{
   va_list ap;

   va_start(ap, format);
   __vk_errorv(instance, object, error, file, line, format, ap);
   va_end(ap);

   return error;
}
///////// END TEMPORARY

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

    module = vk_alloc2(&device->alloc, pAllocator,
                        sizeof(*module) + pCreateInfo->codeSize, 8,
                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (module == NULL)
       return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

    vk_object_base_init(device, &module->base,
                        VK_OBJECT_TYPE_SHADER_MODULE);
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

   vk_object_base_finish(&module->base);
   vk_free2(&device->alloc, pAllocator, module);
}
