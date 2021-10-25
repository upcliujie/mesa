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

#include "vk_alloc.h"
#include "vk_util.h"

#include <directx/d3d12.h>
#include <dxguids/dxguids.h>

#include <dxcapi.h>
#include <wrl/client.h>

#include "util/u_debug.h"

using Microsoft::WRL::ComPtr;

class ShaderBlob : public IDxcBlob {
public:
   ShaderBlob(void *buf, size_t sz) : data(buf), size(sz) {}

   LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override { return data; }

   SIZE_T STDMETHODCALLTYPE GetBufferSize() override { return size; }

   HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }

   ULONG STDMETHODCALLTYPE AddRef() override { return 1; }

   ULONG STDMETHODCALLTYPE Release() override { return 0; }

   void *data;
   size_t size;
};

static dxil_spirv_shader_stage
to_dxil_shader_stage(VkShaderStageFlagBits in)
{
   switch (in) {
   case VK_SHADER_STAGE_VERTEX_BIT: return DXIL_SPIRV_SHADER_VERTEX;
   case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return DXIL_SPIRV_SHADER_TESS_CTRL;
   case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return DXIL_SPIRV_SHADER_TESS_EVAL;
   case VK_SHADER_STAGE_GEOMETRY_BIT: return DXIL_SPIRV_SHADER_GEOMETRY;
   case VK_SHADER_STAGE_FRAGMENT_BIT: return DXIL_SPIRV_SHADER_FRAGMENT;
   case VK_SHADER_STAGE_COMPUTE_BIT: return DXIL_SPIRV_SHADER_COMPUTE;
   default: unreachable("Unsupported stage");
   }
}

static VkResult
dzn_pipeline_compile_shader(dzn_device *device,
                            const VkPipelineShaderStageCreateInfo *stage_info,
                            bool apply_yflip,
                            D3D12_SHADER_BYTECODE *slot)
{
   IDxcValidator *validator = device->instance->dxc.validator.Get();
   IDxcLibrary *library = device->instance->dxc.library.Get();
   IDxcCompiler *compiler = device->instance->dxc.compiler.Get();
   const VkSpecializationInfo *spec_info = stage_info->pSpecializationInfo;
   const struct dzn_shader_module *module =
      dzn_shader_module_from_handle(stage_info->module);
   struct dxil_spirv_runtime_conf conf;
   struct dxil_spirv_object dxil_object;

   /* convert VkSpecializationInfo */
   struct dxil_spirv_specialization *spec = NULL;
   uint32_t num_spec = 0;

   if (spec_info && spec_info->mapEntryCount) {
      spec = (struct dxil_spirv_specialization *)
         malloc(sizeof(*spec) * spec_info->mapEntryCount);
      if (!spec)
         return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

      for (uint32_t i = 0; i < spec_info->mapEntryCount; i++) {
         const VkSpecializationMapEntry *entry = &spec_info->pMapEntries[i];
         const uint8_t *data = (const uint8_t *)spec_info->pData + entry->offset;
         assert(data + entry->size <= (const uint8_t *)spec_info->pData + spec_info->dataSize);
         spec[i].id = entry->constantID;
         switch (entry->size) {
         case 8:
            spec[i].value.u64 = *(const uint64_t *)data;
            break;
         case 4:
            spec[i].value.u32 = *(const uint32_t *)data;
            break;
         case 2:
            spec[i].value.u16 = *(const uint16_t *)data;
            break;
         case 1:
            spec[i].value.u8 = *(const uint8_t *)data;
            break;
         default:
            assert(!"Invalid spec constant size");
            break;
         }

         spec[i].defined_on_module = false;
      }

      num_spec = spec_info->mapEntryCount;
   }

   memset(&conf, 0, sizeof(conf));
   conf.zero_based_vertex_instance_id = true;
   conf.y_flip = apply_yflip ?
                 DXIL_SPIRV_YFLIP_UNCONDITIONAL :
                 DXIL_SPIRV_YFLIP_NONE;

   struct dxil_spirv_debug_options dbg_opts = {
      .dump_nir = !!(device->instance->debug_flags & DZN_DEBUG_NIR),
   };

   /* TODO: Extend spirv_to_dxil() to allow passing a custom allocator */
   if (!spirv_to_dxil(module->code, module->code_size / sizeof(uint32_t),
                      spec, num_spec,
                      to_dxil_shader_stage(stage_info->stage),
                      stage_info->pName, &dbg_opts, &conf, &dxil_object))
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   ShaderBlob blob(dxil_object.binary.buffer, dxil_object.binary.size);
   ComPtr<IDxcOperationResult> result;
   validator->Validate(&blob, DxcValidatorFlags_InPlaceEdit, &result);

   if (device->physical_device->instance->debug_flags & DZN_DEBUG_DXIL) {
      IDxcBlobEncoding *disassembly;
      compiler->Disassemble(&blob, &disassembly);
      ComPtr<IDxcBlobEncoding> blobUtf8;
      library->GetBlobAsUtf8(disassembly, blobUtf8.GetAddressOf());
      char *disasm = reinterpret_cast<char*>(blobUtf8->GetBufferPointer());
      disasm[blobUtf8->GetBufferSize() - 1] = 0;
      fprintf(stderr, "== BEGIN SHADER ============================================\n"
              "%s\n"
              "== END SHADER ==============================================\n",
              disasm);
      disassembly->Release();
   }

   HRESULT validationStatus;
   result->GetStatus(&validationStatus);
   if (FAILED(validationStatus)) {
      if (device->physical_device->instance->debug_flags & DZN_DEBUG_DXIL) {
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

      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   slot->pShaderBytecode = dxil_object.binary.buffer;
   slot->BytecodeLength = dxil_object.binary.size;
   return VK_SUCCESS;
}

static D3D12_SHADER_BYTECODE *
dzn_pipeline_get_gfx_shader_slot(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
                                 VkShaderStageFlagBits in)
{
   switch (in) {
   case VK_SHADER_STAGE_VERTEX_BIT: return &desc->VS;
   case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return &desc->DS;
   case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return &desc->HS;
   case VK_SHADER_STAGE_GEOMETRY_BIT: return &desc->GS;
   case VK_SHADER_STAGE_FRAGMENT_BIT: return &desc->PS;
   default: unreachable("Unsupported stage");
   }
}

static VkResult
dzn_pipeline_translate_vi(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
                          struct dzn_graphics_pipeline *pipeline,
                          const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineVertexInputStateCreateInfo *vi =
      pCreateInfo->pVertexInputState;

   if (!vi->vertexAttributeDescriptionCount) {
      desc->InputLayout.pInputElementDescs = NULL;
      desc->InputLayout.NumElements = 0;
      return VK_SUCCESS;
   }

   D3D12_INPUT_ELEMENT_DESC *inputs = (D3D12_INPUT_ELEMENT_DESC *)
      calloc(vi->vertexAttributeDescriptionCount, sizeof(*inputs));
   if (!inputs)
      return vk_error(pipeline, VK_ERROR_OUT_OF_HOST_MEMORY);

   D3D12_INPUT_CLASSIFICATION slot_class[MAX_VBS];

   pipeline->vb.count = 0;
   for (uint32_t i = 0; i < vi->vertexBindingDescriptionCount; i++) {
      const struct VkVertexInputBindingDescription *bdesc =
         &vi->pVertexBindingDescriptions[i];

      pipeline->vb.count = MAX2(pipeline->vb.count, bdesc->binding + 1);
      pipeline->vb.strides[bdesc->binding] = bdesc->stride;
      if (bdesc->inputRate == VK_VERTEX_INPUT_RATE_INSTANCE) {
         slot_class[bdesc->binding] = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
      } else {
         assert(bdesc->inputRate == VK_VERTEX_INPUT_RATE_VERTEX);
         slot_class[bdesc->binding] = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      }
   }

   for (uint32_t i = 0; i < vi->vertexAttributeDescriptionCount; i++) {
      const VkVertexInputAttributeDescription *attr =
         &vi->pVertexAttributeDescriptions[i];

      /* nir_to_dxil() name all vertex inputs as TEXCOORDx */
      inputs[i].SemanticName = "TEXCOORD";
      inputs[i].SemanticIndex = attr->location;
      inputs[i].Format = dzn_get_format(attr->format);
      inputs[i].InputSlot = slot_class[attr->binding];
      inputs[i].InstanceDataStepRate =
         inputs[i].InputSlot == VK_VERTEX_INPUT_RATE_INSTANCE ? 1 : 0;
      inputs[i].AlignedByteOffset = attr->offset;
   }

   desc->InputLayout.pInputElementDescs = inputs;
   desc->InputLayout.NumElements = vi->vertexAttributeDescriptionCount;
   return VK_SUCCESS;
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE
to_prim_topology_type(VkPrimitiveTopology in)
{
   switch (in) {
   case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
   case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
   default: unreachable("Invalid primitive topology");
   }
}

static D3D12_PRIMITIVE_TOPOLOGY
to_prim_topology(VkPrimitiveTopology in, unsigned patch_control_points)
{
   switch (in) {
   case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY: return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
      assert(patch_control_points);
      return (D3D12_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + patch_control_points - 1);
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: 
   default: unreachable("Invalid primitive topology");
   }
}

static void
dzn_pipeline_translate_ia(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
                          struct dzn_graphics_pipeline *pipeline,
                          const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineInputAssemblyStateCreateInfo *ia =
      pCreateInfo->pInputAssemblyState;
   const VkPipelineTessellationStateCreateInfo *tes =
      (desc->DS.pShaderBytecode && desc->HS.pShaderBytecode) ?
      pCreateInfo->pTessellationState : NULL;

   desc->PrimitiveTopologyType = to_prim_topology_type(ia->topology);
   pipeline->ia.topology =
      to_prim_topology(ia->topology, tes ? tes->patchControlPoints : 0);

   /* FIXME: does that work for u16 index buffers? */
   if (ia->primitiveRestartEnable)
      desc->IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;
   else
      desc->IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
}

static D3D12_FILL_MODE
translate_polygon_mode(VkPolygonMode in)
{
   switch (in) {
   case VK_POLYGON_MODE_FILL: return D3D12_FILL_MODE_SOLID;
   case VK_POLYGON_MODE_LINE: return D3D12_FILL_MODE_WIREFRAME;
   default: unreachable("Unsupported polygon mode");
   }
}

static D3D12_CULL_MODE
translate_cull_mode(VkCullModeFlags in)
{
   switch (in) {
   case VK_CULL_MODE_NONE: return D3D12_CULL_MODE_NONE;
   case VK_CULL_MODE_FRONT_BIT: return D3D12_CULL_MODE_FRONT;
   case VK_CULL_MODE_BACK_BIT: return D3D12_CULL_MODE_BACK;
   default: unreachable("Unsupported cull mode");
   }
}

static void
dzn_pipeline_translate_rast(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
                            struct dzn_graphics_pipeline *pipeline,
                            const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineRasterizationStateCreateInfo *rast =
      pCreateInfo->pRasterizationState;
   const VkPipelineViewportStateCreateInfo *vp =
      pCreateInfo->pViewportState;

   if (vp) {
      pipeline->vp.count = vp->viewportCount;
      if (vp->pViewports) {
         for (uint32_t i = 0; vp->pViewports && i < vp->viewportCount; i++)
            dzn_translate_viewport(&pipeline->vp.desc[i], &vp->pViewports[i]);
      }
 
      pipeline->scissor.count = vp->scissorCount;
      if (vp->pScissors) {
         for (uint32_t i = 0; i < vp->scissorCount; i++)
            dzn_translate_scissor(&pipeline->scissor.desc[i], &vp->pScissors[i]);
      }
   }

   /* TODO: rasterizerDiscardEnable */
   desc->RasterizerState.DepthClipEnable = !rast->depthClampEnable;
   desc->RasterizerState.FillMode = translate_polygon_mode(rast->polygonMode);
   desc->RasterizerState.CullMode = translate_cull_mode(rast->cullMode);
   desc->RasterizerState.FrontCounterClockwise =
      rast->frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE;
   if (rast->depthBiasEnable) {
      desc->RasterizerState.DepthBias = rast->depthBiasConstantFactor;
      desc->RasterizerState.SlopeScaledDepthBias = rast->depthBiasSlopeFactor;
      desc->RasterizerState.DepthBiasClamp = rast->depthBiasClamp;
   }

   assert(rast->lineWidth == 1.0f);
}

static void
dzn_pipeline_translate_ms(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
                          const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineMultisampleStateCreateInfo *ms =
      pCreateInfo->pMultisampleState;

   /* TODO: sampleShadingEnable, minSampleShading,
    *       alphaToOneEnable
    */
   desc->SampleDesc.Count = ms->rasterizationSamples;
   desc->SampleDesc.Quality = 0;
   desc->SampleMask = ms->pSampleMask ? *ms->pSampleMask : 1;
}

static D3D12_STENCIL_OP
translate_stencil_op(VkStencilOp in)
{
   switch (in) {
   case VK_STENCIL_OP_KEEP: return D3D12_STENCIL_OP_KEEP;
   case VK_STENCIL_OP_ZERO: return D3D12_STENCIL_OP_ZERO;
   case VK_STENCIL_OP_REPLACE: return D3D12_STENCIL_OP_REPLACE;
   case VK_STENCIL_OP_INCREMENT_AND_CLAMP: return D3D12_STENCIL_OP_INCR_SAT;
   case VK_STENCIL_OP_DECREMENT_AND_CLAMP: return D3D12_STENCIL_OP_DECR_SAT;
   case VK_STENCIL_OP_INCREMENT_AND_WRAP: return D3D12_STENCIL_OP_INCR;
   case VK_STENCIL_OP_DECREMENT_AND_WRAP: return D3D12_STENCIL_OP_DECR;
   case VK_STENCIL_OP_INVERT: return D3D12_STENCIL_OP_INVERT;
   default: unreachable("Invalid stencil op");
   }
}

static void
dzn_pipeline_translate_zsa(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
                           struct dzn_graphics_pipeline *pipeline,
                           const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineDepthStencilStateCreateInfo *zsa =
      pCreateInfo->pDepthStencilState;

   /* TODO: depthBoundsTestEnable */

   desc->DepthStencilState.DepthEnable = zsa->depthTestEnable;
   desc->DepthStencilState.DepthWriteMask =
      zsa->depthWriteEnable ?
      D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
   desc->DepthStencilState.DepthFunc =
      dzn_translate_compare_op(zsa->depthCompareOp);
   desc->DepthStencilState.StencilEnable = zsa->stencilTestEnable;
   if (zsa->stencilTestEnable && 0) {
      desc->DepthStencilState.FrontFace.StencilFailOp =
        translate_stencil_op(zsa->front.failOp);
      desc->DepthStencilState.FrontFace.StencilDepthFailOp =
        translate_stencil_op(zsa->front.depthFailOp);
      desc->DepthStencilState.FrontFace.StencilPassOp =
        translate_stencil_op(zsa->front.passOp);
      desc->DepthStencilState.FrontFace.StencilFunc =
        dzn_translate_compare_op(zsa->front.compareOp);
      desc->DepthStencilState.BackFace.StencilFailOp =
        translate_stencil_op(zsa->back.failOp);
      desc->DepthStencilState.BackFace.StencilDepthFailOp =
        translate_stencil_op(zsa->back.depthFailOp);
      desc->DepthStencilState.BackFace.StencilPassOp =
        translate_stencil_op(zsa->back.passOp);
      desc->DepthStencilState.BackFace.StencilFunc =
        dzn_translate_compare_op(zsa->back.compareOp);

      /* FIXME: In vulkan, front/back readmask/writemask/ref can be
       * different.
       */
      desc->DepthStencilState.StencilReadMask =
        zsa->back.compareMask | zsa->front.compareMask;
      desc->DepthStencilState.StencilWriteMask =
        zsa->back.writeMask | zsa->front.writeMask;
      pipeline->zsa.stencil_ref = zsa->back.reference | zsa->front.reference;
   }
}

static D3D12_BLEND
translate_blend_factor(VkBlendFactor in)
{
   switch (in) {
   case VK_BLEND_FACTOR_ZERO: return D3D12_BLEND_ZERO;
   case VK_BLEND_FACTOR_ONE: return D3D12_BLEND_ONE;
   case VK_BLEND_FACTOR_SRC_COLOR: return D3D12_BLEND_SRC_COLOR;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return D3D12_BLEND_INV_SRC_COLOR;
   case VK_BLEND_FACTOR_DST_COLOR: return D3D12_BLEND_DEST_COLOR;
   case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return D3D12_BLEND_INV_DEST_COLOR;
   case VK_BLEND_FACTOR_SRC_ALPHA: return D3D12_BLEND_SRC_ALPHA;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return D3D12_BLEND_INV_SRC_ALPHA;
   case VK_BLEND_FACTOR_DST_ALPHA: return D3D12_BLEND_DEST_ALPHA;
   case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return D3D12_BLEND_INV_DEST_ALPHA;
   /* FIXME: no way to isolate the alpla and color constants */
   case VK_BLEND_FACTOR_CONSTANT_COLOR:
   case VK_BLEND_FACTOR_CONSTANT_ALPHA:
      return D3D12_BLEND_BLEND_FACTOR;
   case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
   case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
      return D3D12_BLEND_INV_BLEND_FACTOR;
   case VK_BLEND_FACTOR_SRC1_COLOR: return D3D12_BLEND_SRC1_COLOR;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return D3D12_BLEND_INV_SRC1_COLOR;
   case VK_BLEND_FACTOR_SRC1_ALPHA: return D3D12_BLEND_SRC1_ALPHA;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return D3D12_BLEND_INV_SRC1_ALPHA;
   case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return D3D12_BLEND_SRC_ALPHA_SAT;
   default: unreachable("Invalid blend factor");
   }
}

static D3D12_BLEND_OP
translate_blend_op(VkBlendOp in)
{
   switch (in) {
   case VK_BLEND_OP_ADD: return D3D12_BLEND_OP_ADD;
   case VK_BLEND_OP_SUBTRACT: return D3D12_BLEND_OP_SUBTRACT;
   case VK_BLEND_OP_REVERSE_SUBTRACT: return D3D12_BLEND_OP_REV_SUBTRACT;
   case VK_BLEND_OP_MIN: return D3D12_BLEND_OP_MIN;
   case VK_BLEND_OP_MAX: return D3D12_BLEND_OP_MAX;
   default: unreachable("Invalid blend op");
   }
}

static D3D12_LOGIC_OP
translate_logic_op(VkLogicOp in)
{
   switch (in) {
   case VK_LOGIC_OP_CLEAR: return D3D12_LOGIC_OP_CLEAR;
   case VK_LOGIC_OP_AND: return D3D12_LOGIC_OP_AND;
   case VK_LOGIC_OP_AND_REVERSE: return D3D12_LOGIC_OP_AND_REVERSE;
   case VK_LOGIC_OP_COPY: return D3D12_LOGIC_OP_COPY;
   case VK_LOGIC_OP_AND_INVERTED: return D3D12_LOGIC_OP_AND_INVERTED;
   case VK_LOGIC_OP_NO_OP: return D3D12_LOGIC_OP_NOOP;
   case VK_LOGIC_OP_XOR: return D3D12_LOGIC_OP_XOR;
   case VK_LOGIC_OP_OR: return D3D12_LOGIC_OP_OR;
   case VK_LOGIC_OP_NOR: return D3D12_LOGIC_OP_NOR;
   case VK_LOGIC_OP_EQUIVALENT: return D3D12_LOGIC_OP_EQUIV;
   case VK_LOGIC_OP_INVERT: return D3D12_LOGIC_OP_INVERT;
   case VK_LOGIC_OP_OR_REVERSE: return D3D12_LOGIC_OP_OR_REVERSE;
   case VK_LOGIC_OP_COPY_INVERTED: return D3D12_LOGIC_OP_COPY_INVERTED;
   case VK_LOGIC_OP_OR_INVERTED: return D3D12_LOGIC_OP_OR_INVERTED;
   case VK_LOGIC_OP_NAND: return D3D12_LOGIC_OP_NAND;
   case VK_LOGIC_OP_SET: return D3D12_LOGIC_OP_SET;
   default: unreachable("Invalid logic op");
   }
}

static void
dzn_pipeline_translate_blend(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
                             struct dzn_graphics_pipeline *pipeline,
                             const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
   const VkPipelineColorBlendStateCreateInfo *blend =
      pCreateInfo->pColorBlendState;
   const VkPipelineMultisampleStateCreateInfo *ms =
      pCreateInfo->pMultisampleState;

   D3D12_LOGIC_OP logicop =
      blend->logicOpEnable ?
      translate_logic_op(blend->logicOp) : D3D12_LOGIC_OP_NOOP;
   desc->BlendState.AlphaToCoverageEnable = ms->alphaToCoverageEnable;
   for (uint32_t i = 0; i < blend->attachmentCount; i++) {
      if (i > 0 &&
          !memcmp(&blend->pAttachments[i - 1], &blend->pAttachments[i],
                  sizeof(*blend->pAttachments)))
         desc->BlendState.IndependentBlendEnable = true;

      desc->BlendState.RenderTarget[i].BlendEnable =
         blend->pAttachments[i].blendEnable;
         blend->logicOpEnable;
      desc->BlendState.RenderTarget[i].RenderTargetWriteMask =
         blend->pAttachments[i].colorWriteMask;
      if (blend->logicOpEnable) {
         desc->BlendState.RenderTarget[i].LogicOpEnable = true;
         desc->BlendState.RenderTarget[i].LogicOp = logicop;
      } else {
         desc->BlendState.RenderTarget[i].SrcBlend =
            translate_blend_factor(blend->pAttachments[i].srcColorBlendFactor);
         desc->BlendState.RenderTarget[i].DestBlend =
            translate_blend_factor(blend->pAttachments[i].dstColorBlendFactor);
         desc->BlendState.RenderTarget[i].BlendOp =
            translate_blend_op(blend->pAttachments[i].colorBlendOp);
         desc->BlendState.RenderTarget[i].SrcBlendAlpha =
            translate_blend_factor(blend->pAttachments[i].srcAlphaBlendFactor);
         desc->BlendState.RenderTarget[i].DestBlendAlpha =
            translate_blend_factor(blend->pAttachments[i].dstAlphaBlendFactor);
         desc->BlendState.RenderTarget[i].BlendOpAlpha =
            translate_blend_op(blend->pAttachments[i].alphaBlendOp);
      }
   }
}

static void
dzn_pipeline_destroy(struct dzn_device *device,
                     struct dzn_pipeline *pipeline,
                     const VkAllocationCallbacks *pAllocator)
{
   if (!pipeline)
      return;

   if (pipeline->state)
      pipeline->state->Release();

   vk_object_free(&device->vk, pAllocator, pipeline);
}

static VkResult
graphics_pipeline_create(dzn_device *device,
                         dzn_pipeline_cache *cache,
                         const VkGraphicsPipelineCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkPipeline *pPipeline)
{
   VK_FROM_HANDLE(dzn_render_pass, pass, pCreateInfo->renderPass);
   VK_FROM_HANDLE(dzn_pipeline_layout, layout, pCreateInfo->layout);
   const dzn_subpass *subpass = &pass->subpasses[pCreateInfo->subpass];
   VkResult ret;
   HRESULT hres = 0;

   dzn_graphics_pipeline *pipeline = (dzn_graphics_pipeline *)
      vk_object_zalloc(&device->vk, pAllocator,
                       sizeof(*pipeline), VK_OBJECT_TYPE_PIPELINE);
   if (pipeline == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   pipeline->base.layout = layout;

   D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
      .pRootSignature = layout->root.sig.Get(),
      .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
   };

   uint32_t stage_mask = 0;
   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++)
      stage_mask |= pCreateInfo->pStages[i].stage;

   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      D3D12_SHADER_BYTECODE *slot =
         dzn_pipeline_get_gfx_shader_slot(&desc, pCreateInfo->pStages[i].stage);
      bool apply_yflip =
         pCreateInfo->pStages[i].stage == VK_SHADER_STAGE_GEOMETRY_BIT ||
         (pCreateInfo->pStages[i].stage == VK_SHADER_STAGE_VERTEX_BIT &&
          !(stage_mask & VK_SHADER_STAGE_GEOMETRY_BIT));

      ret = dzn_pipeline_compile_shader(device, &pCreateInfo->pStages[i],
                                        apply_yflip, slot);
      if (ret != VK_SUCCESS)
         goto out;
   }

   ret = dzn_pipeline_translate_vi(&desc, pipeline, pCreateInfo);
   if (ret != VK_SUCCESS)
      goto out;

   dzn_pipeline_translate_ia(&desc, pipeline, pCreateInfo);
   dzn_pipeline_translate_rast(&desc, pipeline, pCreateInfo);
   dzn_pipeline_translate_ms(&desc, pCreateInfo);
   dzn_pipeline_translate_zsa(&desc, pipeline, pCreateInfo);
   dzn_pipeline_translate_blend(&desc, pipeline, pCreateInfo);

   desc.NumRenderTargets = subpass->color_count;
   for (uint32_t i = 0; i < subpass->color_count; i++) {
      uint32_t idx = subpass->colors[i].idx;

      if (idx == VK_ATTACHMENT_UNUSED) continue;

      const struct dzn_attachment *attachment = &pass->attachments[idx];

      desc.RTVFormats[i] = attachment->format;
   }

   if (subpass->zs.idx != VK_ATTACHMENT_UNUSED) {
      const struct dzn_attachment *attachment =
         &pass->attachments[subpass->zs.idx];

      desc.DSVFormat = attachment->format;
   }

   hres = device->dev->CreateGraphicsPipelineState(&desc,
                                                   IID_PPV_ARGS(&pipeline->base.state));
   if (FAILED(hres)) {
      ret = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto out;
   }

   *pPipeline = dzn_pipeline_to_handle(&pipeline->base);
   ret = VK_SUCCESS;

out:
   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      D3D12_SHADER_BYTECODE *slot =
         dzn_pipeline_get_gfx_shader_slot(&desc, pCreateInfo->pStages[i].stage);
      free((void *)slot->pShaderBytecode);
   }

   free((void *)desc.InputLayout.pInputElementDescs);

   if (ret != VK_SUCCESS)
      dzn_pipeline_destroy(device, &pipeline->base, pAllocator);

   return ret;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateGraphicsPipelines(VkDevice _device,
                            VkPipelineCache pipelineCache,
                            uint32_t count,
                            const VkGraphicsPipelineCreateInfo *pCreateInfos,
                            const VkAllocationCallbacks *pAllocator,
                            VkPipeline *pPipelines)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_pipeline_cache, pipeline_cache, pipelineCache);

   VkResult result = VK_SUCCESS;

   unsigned i;
   for (i = 0; i < count; i++) {
      VkResult res = graphics_pipeline_create(device, pipeline_cache,
                                              &pCreateInfos[i],
                                              pAllocator, &pPipelines[i]);

      if (res == VK_SUCCESS)
         continue;

      /* Bail out on the first error != VK_PIPELINE_COMPILE_REQUIRED_EX as it
       * is not obvious what error should be report upon 2 different failures.
       * */
      result = res;
      if (res != VK_PIPELINE_COMPILE_REQUIRED_EXT)
         break;

      if (pCreateInfos[i].flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
         break;
   }

   for (; i < count; i++)
      pPipelines[i] = VK_NULL_HANDLE;

   return result;
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyPipeline(VkDevice _device,
                    VkPipeline _pipeline,
                    const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_pipeline, pipeline, _pipeline);

   dzn_pipeline_destroy(device, pipeline, pAllocator);
}
