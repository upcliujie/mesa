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

#include "dxil_nir.h"
#include "dxil_nir_lower_int_samplers.h"

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

   nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));

   if ((instance->debug_flags & DZN_DEBUG_NIR) &&
       (instance->debug_flags & DZN_DEBUG_INTERNAL))
      nir_print_shader(nir, stderr);

   struct nir_to_dxil_options opts = { .environment = DXIL_ENVIRONMENT_VULKAN };
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

#define DZN_META_INDIRECT_DRAW_MAX_PARAM_COUNT 4

dzn_meta_indirect_draw::dzn_meta_indirect_draw(dzn_device *dev,
                                               enum dzn_indirect_draw_type type) :
   dzn_meta(dev)
{
   glsl_type_singleton_init_or_ref();

   nir_shader *nir = dzn_nir_indirect_draw_shader(type);
   bool triangle_fan = type == DZN_INDIRECT_DRAW_TRIANGLE_FAN ||
                       type == DZN_INDIRECT_INDEXED_DRAW_TRIANGLE_FAN;
   uint32_t shader_params_size =
      triangle_fan ?
      sizeof(struct dzn_indirect_draw_triangle_fan_rewrite_params) :
      sizeof(struct dzn_indirect_draw_rewrite_params);

   uint32_t root_param_count = 0;
   D3D12_ROOT_PARAMETER1 root_params[DZN_META_INDIRECT_DRAW_MAX_PARAM_COUNT];

   root_params[root_param_count++] = D3D12_ROOT_PARAMETER1 {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
      .Constants = {
         .ShaderRegister = 0,
         .RegisterSpace = 0,
         .Num32BitValues = shader_params_size / 4,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
   };

   root_params[root_param_count++] = D3D12_ROOT_PARAMETER1 {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
      .Descriptor = {
         .ShaderRegister = 1,
         .RegisterSpace = 0,
         .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
   };

   root_params[root_param_count++] = D3D12_ROOT_PARAMETER1 {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
      .Descriptor = {
         .ShaderRegister = 2,
         .RegisterSpace = 0,
         .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
   };


   if (triangle_fan) {
      root_params[root_param_count++] = D3D12_ROOT_PARAMETER1 {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
         .Descriptor = {
            .ShaderRegister = 3,
            .RegisterSpace = 0,
            .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
         },
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
      };
   }

   assert(root_param_count <= ARRAY_SIZE(root_params));

   D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {
      .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
      .Desc_1_1 = {
         .NumParameters = root_param_count,
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
   assert(desc.CS.pShaderBytecode);

   HRESULT hres =
      device->dev->CreateComputePipelineState(&desc,
                                              IID_PPV_ARGS(&pipeline_state));
   assert(!FAILED(hres));

   free((void *)desc.CS.pShaderBytecode);
   ralloc_free(nir);
   glsl_type_singleton_decref();
}

#define DZN_META_TRIANGLE_FAN_REWRITE_IDX_MAX_PARAM_COUNT 3

dzn_meta_triangle_fan_rewrite_index::dzn_meta_triangle_fan_rewrite_index(dzn_device *dev,
                                                                         enum index_type old_index_type) :
   dzn_meta(dev)
{
   glsl_type_singleton_init_or_ref();

   uint8_t old_index_size = get_index_size(old_index_type);

   nir_shader *nir = dzn_nir_triangle_fan_rewrite_index_shader(old_index_size);

   uint32_t root_param_count = 0;
   D3D12_ROOT_PARAMETER1 root_params[DZN_META_TRIANGLE_FAN_REWRITE_IDX_MAX_PARAM_COUNT];

   root_params[root_param_count++] = D3D12_ROOT_PARAMETER1 {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
      .Descriptor = {
         .ShaderRegister = 1,
         .RegisterSpace = 0,
         .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
   };

   root_params[root_param_count++] = D3D12_ROOT_PARAMETER1 {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
      .Constants = {
         .ShaderRegister = 0,
         .RegisterSpace = 0,
         .Num32BitValues = sizeof(struct dzn_triangle_fan_rewrite_index_params) / 4,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
   };

   if (old_index_type != NO_INDEX) {
      root_params[root_param_count++] = D3D12_ROOT_PARAMETER1 {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {
            .ShaderRegister = 2,
            .RegisterSpace = 0,
            .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
         },
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
      };
   }

   assert(root_param_count <= ARRAY_SIZE(root_params));

   D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {
      .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
      .Desc_1_1 = {
         .NumParameters = root_param_count,
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

   compile_shader(device, nir, &desc.CS);

   HRESULT hres =
      device->dev->CreateComputePipelineState(&desc,
                                              IID_PPV_ARGS(&pipeline_state));
   assert(!FAILED(hres));

   free((void *)desc.CS.pShaderBytecode);
   ralloc_free(nir);
   glsl_type_singleton_decref();

   D3D12_INDIRECT_ARGUMENT_DESC cmd_args[] = {
      {
         .Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW,
         .UnorderedAccessView = {
            .RootParameterIndex = 0,
         },
      },
      {
         .Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT,
         .Constant = {
            .RootParameterIndex = 1,
            .DestOffsetIn32BitValues = 0,
            .Num32BitValuesToSet = sizeof(struct dzn_triangle_fan_rewrite_index_params) / 4,
         },
      },
      {
         .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH,
      },
   };

   D3D12_COMMAND_SIGNATURE_DESC cmd_sig_desc = {
      .ByteStride = sizeof(struct dzn_indirect_triangle_fan_rewrite_index_exec_params),
      .NumArgumentDescs = ARRAY_SIZE(cmd_args),
      .pArgumentDescs = cmd_args,
   };

   assert((cmd_sig_desc.ByteStride & 7) == 0);

   hres = device->dev->CreateCommandSignature(&cmd_sig_desc,
                                              root_sig.Get(),
                                              IID_PPV_ARGS(&cmd_sig));
   assert(!FAILED(hres));
}

enum dzn_meta_triangle_fan_rewrite_index::index_type
dzn_meta_triangle_fan_rewrite_index::get_index_type(uint8_t index_size)
{
   switch (index_size) {
   case 0: return NO_INDEX;
   case 2: return INDEX_2B;
   case 4: return INDEX_4B;
   default: unreachable("Invalid index size");
   }
}

enum dzn_meta_triangle_fan_rewrite_index::index_type
dzn_meta_triangle_fan_rewrite_index::get_index_type(DXGI_FORMAT format)
{
   switch (format) {
   case DXGI_FORMAT_UNKNOWN: return NO_INDEX;
   case DXGI_FORMAT_R16_UINT: return INDEX_2B;
   case DXGI_FORMAT_R32_UINT: return INDEX_4B;
   default: unreachable("Invalid index format");
   }
}

uint8_t
dzn_meta_triangle_fan_rewrite_index::get_index_size(enum index_type type)
{
   switch (type) {
   case NO_INDEX: return 0;
   case INDEX_2B: return 2;
   case INDEX_4B: return 4;
   default: unreachable("Invalid index type");
   }
}

dzn_meta_blit::shader::shader(struct dzn_device *dev) :
   device(dev)
{
}

dzn_meta_blit::shader::shader(struct dzn_device *dev, const D3D12_SHADER_BYTECODE &in) :
   device(dev)
{
   code.pShaderBytecode = vk_alloc(&dev->vk.alloc, in.BytecodeLength, 8,
                                 VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   memcpy((void *)code.pShaderBytecode, in.pShaderBytecode, in.BytecodeLength);
   code.BytecodeLength = in.BytecodeLength;
}

dzn_meta_blit::shader::~shader()
{
   vk_free(&device->vk.alloc, (void *)code.pShaderBytecode);
}

const VkAllocationCallbacks *
dzn_meta_blit::shader::get_vk_allocator()
{
   return &device->vk.alloc;
}

const dzn_meta_blit::shader *
dzn_meta_blits::get_vs()
{
   std::lock_guard<std::mutex> lock(shaders_lock);

   if (vs.get() == NULL) {
      nir_shader *nir = dzn_nir_blit_vs();

      NIR_PASS_V(nir, nir_lower_system_values);

      gl_system_value system_values[] = {
         SYSTEM_VALUE_FIRST_VERTEX,
         SYSTEM_VALUE_BASE_VERTEX,
      };

      NIR_PASS_V(nir, dxil_nir_lower_system_values_to_zero, system_values,
                ARRAY_SIZE(system_values));

      D3D12_SHADER_BYTECODE bc;

      dzn_meta::compile_shader(device, nir, &bc);
      vs = dzn_private_object_create<dzn_meta_blit::shader>(&device->vk.alloc,
                                                            device, bc);
      free((void *)bc.pShaderBytecode);
      ralloc_free(nir);
   }

   return vs.get();
}

const dzn_meta_blit::shader *
dzn_meta_blits::get_fs(const struct dzn_nir_blit_info &info)
{
   std::lock_guard<std::mutex> lock(shaders_lock);
   const dzn_meta_blit::shader *out = NULL;

   auto iter = fs.find(info.hash_key);

   STATIC_ASSERT(sizeof(struct dzn_nir_blit_info) == sizeof(uint32_t));

   if (iter != fs.end()) {
      out = iter->second.get();
   } else {
      nir_shader *nir = dzn_nir_blit_fs(&info);

      if (info.out_type != GLSL_TYPE_FLOAT) {
         dxil_wrap_sampler_state wrap_state = {
            .is_int_sampler = 1,
            .is_linear_filtering = 0,
            .skip_boundary_conditions = 1,
         };
         dxil_lower_sample_to_txf_for_integer_tex(nir, &wrap_state, NULL, 0);
      }

      D3D12_SHADER_BYTECODE bc;

      dzn_meta::compile_shader(device, nir, &bc);

      auto s =
         dzn_private_object_create<dzn_meta_blit::shader>(&device->vk.alloc,
                                                          device, bc);
      out = s.get();
      fs[info.hash_key].swap(s);
      free((void *)bc.pShaderBytecode);
      ralloc_free(nir);
   }

   assert(out);
   return out;
}

dzn_meta_blit::dzn_meta_blit(struct dzn_device *dev,
                             const key &key) :
   dzn_meta(dev)
{
   glsl_type_singleton_init_or_ref();

   D3D12_DESCRIPTOR_RANGE1 ranges[] = {
      {
         .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
         .NumDescriptors = 1,
         .BaseShaderRegister = 0,
         .RegisterSpace = 0,
         .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS,
         .OffsetInDescriptorsFromTableStart = 0,
      },
   };

   D3D12_STATIC_SAMPLER_DESC samplers[] = {
      {
         .Filter = key.linear_filter ?
                   D3D12_FILTER_MIN_MAG_MIP_LINEAR :
                   D3D12_FILTER_MIN_MAG_MIP_POINT,
         .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .MipLODBias = 0,
         .MaxAnisotropy = 0,
         .MinLOD = 0,
         .MaxLOD = D3D12_FLOAT32_MAX,
         .ShaderRegister = 0,
         .RegisterSpace = 0,
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
      },
   };

   D3D12_ROOT_PARAMETER1 root_params[] = {
      {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
         .DescriptorTable = {
            .NumDescriptorRanges = ARRAY_SIZE(ranges),
            .pDescriptorRanges = ranges,
         },
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
      },
      {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
         .Constants = {
            .ShaderRegister = 0,
            .RegisterSpace = 0,
            .Num32BitValues = 17,
         },
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX,
      },
   };

   D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {
      .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
      .Desc_1_1 = {
         .NumParameters = ARRAY_SIZE(root_params),
         .pParameters = root_params,
         .NumStaticSamplers = ARRAY_SIZE(samplers),
         .pStaticSamplers = samplers,
         .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
      },
   };

   root_sig = device->create_root_sig(root_sig_desc);
   if (!root_sig.Get())
      throw vk_error(device->instance, VK_ERROR_UNKNOWN);

   D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
      .pRootSignature = root_sig.Get(),
      .SampleMask = key.resolve ? 1 : (1ULL << key.samples) - 1,
      .RasterizerState = {
         .FillMode = D3D12_FILL_MODE_SOLID,
         .CullMode = D3D12_CULL_MODE_NONE,
         .DepthClipEnable = TRUE,
      },
      .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
      .SampleDesc = {
         .Count = key.resolve ? 1 : key.samples,
         .Quality = 0,
      },
      .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
   };

   auto *vs = device->blits->get_vs();
   desc.VS = vs->code;
   assert(desc.VS.pShaderBytecode);

   struct dzn_nir_blit_info blit_fs_info = {
      .src_samples = key.samples,
      .loc = key.loc,
      .out_type = key.out_type,
      .sampler_dim = key.sampler_dim,
      .src_is_array = key.src_is_array,
      .resolve = key.resolve,
   };
   auto *fs = device->blits->get_fs(blit_fs_info);
   desc.PS = fs->code;
   assert(desc.PS.pShaderBytecode);

   assert(key.loc == FRAG_RESULT_DATA0 ||
          key.loc == FRAG_RESULT_DEPTH ||
          key.loc == FRAG_RESULT_STENCIL);

   if (key.loc == FRAG_RESULT_DATA0) {
      desc.NumRenderTargets = 1;
      desc.RTVFormats[0] = key.out_format;
      desc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
   } else {
      desc.DSVFormat = key.out_format;
      if (key.loc == FRAG_RESULT_DEPTH) {
         desc.DepthStencilState.DepthEnable = TRUE;
         desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
         desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      } else {
         assert(key.loc == FRAG_RESULT_STENCIL);
         desc.DepthStencilState.StencilEnable = TRUE;
         desc.DepthStencilState.StencilWriteMask = 0xff;
         desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_REPLACE;
         desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_REPLACE;
         desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
         desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
         desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
      }
   }

   HRESULT hres =
      device->dev->CreateGraphicsPipelineState(&desc,
                                               IID_PPV_ARGS(&pipeline_state));
   assert(!FAILED(hres));

   glsl_type_singleton_decref();
}

const dzn_meta_blit *
dzn_meta_blits::get_context(const dzn_meta_blit::key &key)
{
   std::lock_guard<std::mutex> lock(contexts_lock);
   const dzn_meta_blit *out = NULL;

   STATIC_ASSERT(sizeof(key) == sizeof(uint64_t));

   auto iter = contexts.find(key.u64);
   if (iter != contexts.end()) {
      out = iter->second.get();
   } else {
      auto context =
         dzn_private_object_create<dzn_meta_blit>(&device->vk.alloc,
                                                  device, key);
      out = context.get();
      contexts[key.u64].swap(context);
   }

   return out;
}

dzn_meta_blits::dzn_meta_blits(struct dzn_device *dev) :
   device(dev), fs(fs_allocator(&dev->vk.alloc)),
   contexts(contexts_allocator(&dev->vk.alloc))
{
}

const VkAllocationCallbacks *
dzn_meta_blits::get_vk_allocator()
{
   return &device->vk.alloc;
}
