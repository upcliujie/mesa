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

#include "spirv_to_dxil.h"
#include "nir_to_dxil.h"

dzn_meta::dzn_meta(struct dzn_device *dev) : device(dev)
{
}

const VkAllocationCallbacks *
dzn_meta::get_vk_allocator()
{
   return &device->vk.alloc;
}

void
dzn_meta::compile_shader(dzn_device *device, nir_shader *nir,
                         D3D12_SHADER_BYTECODE *slot)
{
   dzn_instance *instance = device->instance;
   IDxcValidator *validator = instance->dxc.validator.Get();
   IDxcLibrary *library = instance->dxc.library.Get();
   IDxcCompiler *compiler = instance->dxc.compiler.Get();

   if ((instance->debug_flags & DZN_DEBUG_NIR) &&
       (instance->debug_flags & DZN_DEBUG_INTERNAL))
      nir_print_shader(nir, stderr);

   struct nir_to_dxil_options opts = {};
   struct blob dxil_blob;
   bool ret = nir_to_dxil(nir, &opts, &dxil_blob);
   assert(ret);


   dzn_shader_blob blob(dxil_blob.data, dxil_blob.size);
   ComPtr<IDxcOperationResult> result;
   validator->Validate(&blob, DxcValidatorFlags_InPlaceEdit, &result);
   if ((instance->debug_flags & DZN_DEBUG_DXIL) &&
       (instance->debug_flags & DZN_DEBUG_INTERNAL)) {
      IDxcBlobEncoding *disassembly;
      compiler->Disassemble(&blob, &disassembly);
      ComPtr<IDxcBlobEncoding> blobUtf8;
      library->GetBlobAsUtf8(disassembly, blobUtf8.GetAddressOf());
      char *disasm = reinterpret_cast<char*>(blobUtf8->GetBufferPointer());
      disasm[blobUtf8->GetBufferSize() - 1] = 0;
      fprintf(stderr,
              "== BEGIN SHADER ============================================\n"
              "%s\n"
              "== END SHADER ==============================================\n",
              disasm);
      disassembly->Release();
   }

   HRESULT validationStatus;
   result->GetStatus(&validationStatus);
   if (FAILED(validationStatus)) {
      if ((instance->debug_flags & DZN_DEBUG_DXIL) &&
          (instance->debug_flags & DZN_DEBUG_INTERNAL)) {
         ComPtr<IDxcBlobEncoding> printBlob, printBlobUtf8;
         result->GetErrorBuffer(&printBlob);
         library->GetBlobAsUtf8(printBlob.Get(), printBlobUtf8.GetAddressOf());

         char *errorString;
         if (printBlobUtf8) {
            errorString = reinterpret_cast<char*>(printBlobUtf8->GetBufferPointer());
            errorString[printBlobUtf8->GetBufferSize() - 1] = 0;
            fprintf(stderr,
                    "== VALIDATION ERROR =============================================\n"
                    "%s\n"
                    "== END ==========================================================\n",
                    errorString);
         }
      }
   }
   assert(!FAILED(validationStatus));

   void *data;
   size_t size;
   blob_finish_get_buffer(&dxil_blob, &data, &size);
   slot->pShaderBytecode = data;
   slot->BytecodeLength = size;
}

dzn_meta_indirect_draw::dzn_meta_indirect_draw(dzn_device *dev,
                                               enum dzn_indirect_draw_type type) :
   dzn_meta(dev)
{
   glsl_type_singleton_init_or_ref();

   nir_shader *nir = dzn_nir_indirect_draw_shader(type);

   D3D12_ROOT_PARAMETER1 root_params[] = {
      {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
         .Constants = {
            .ShaderRegister = 0,
            .RegisterSpace = 0,
            .Num32BitValues = 1,
         },
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
      },
      {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {
            .ShaderRegister = 1,
            .RegisterSpace = 0,
            .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
         },
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
      },
      {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
         .Descriptor = {
            .ShaderRegister = 2,
            .RegisterSpace = 0,
            .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
         },
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
      },
   };

   D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {
      .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
      .Desc_1_1 = {
         .NumParameters = ARRAY_SIZE(root_params),
         .pParameters = root_params,
         .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
      },
   };

   root_sig = device->create_root_sig(root_sig_desc);
   if (!root_sig.Get())
      throw vk_error(device->instance, VK_ERROR_UNKNOWN);

   D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {
      .pRootSignature = root_sig.Get(),
      .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
   };

   dzn_meta::compile_shader(device, nir, &desc.CS);

   HRESULT hres =
      device->dev->CreateComputePipelineState(&desc,
                                              IID_PPV_ARGS(&pipeline_state));
   assert(!FAILED(hres));

   free((void *)desc.CS.pShaderBytecode);
   ralloc_free(nir);
   glsl_type_singleton_decref();
}
