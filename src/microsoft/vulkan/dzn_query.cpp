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
#include "vk_debug_report.h"
#include "vk_util.h"

D3D12_QUERY_HEAP_TYPE
dzn_query_pool::get_heap_type(VkQueryType in)
{
   switch (in) {
   case VK_QUERY_TYPE_OCCLUSION: return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
   case VK_QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
   case VK_QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
   default: unreachable("Unsupported query type");
   }
}

D3D12_QUERY_TYPE
dzn_query_pool::get_query_type(VkQueryControlFlags flags)
{
   switch (heap_type) {
   case D3D12_QUERY_HEAP_TYPE_OCCLUSION:
      return flags & VK_QUERY_CONTROL_PRECISE_BIT ?
             D3D12_QUERY_TYPE_OCCLUSION : D3D12_QUERY_TYPE_BINARY_OCCLUSION;
   case D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
   case D3D12_QUERY_HEAP_TYPE_TIMESTAMP: return D3D12_QUERY_TYPE_TIMESTAMP;
   default: unreachable("Unsupported query type");
   }
}

dzn_query_pool::dzn_query_pool(dzn_device *device,
                               const VkQueryPoolCreateInfo *info,
                               const VkAllocationCallbacks *alloc) :
   queries(info->queryCount, query(), dzn_allocator<query>(alloc))
{
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_QUERY_POOL);

   D3D12_QUERY_HEAP_DESC desc = { 0 };
   heap_type = desc.Type = get_heap_type(info->queryType);
   desc.Count = info->queryCount;
   desc.NodeMask = 0;

   HRESULT hres =
      device->dev->CreateQueryHeap(&desc, IID_PPV_ARGS(&heap));
   if (FAILED(hres))
      throw vk_error(device, VK_ERROR_OUT_OF_DEVICE_MEMORY);

   switch (info->queryType) {
   case VK_QUERY_TYPE_OCCLUSION:
   case VK_QUERY_TYPE_TIMESTAMP:
      query_size = sizeof(uint64_t);
      break;
   case VK_QUERY_TYPE_PIPELINE_STATISTICS:
      pipeline_statistics = info->pipelineStatistics;
      query_size = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
      break;
   default: unreachable("Unsupported query type");
   }

   D3D12_HEAP_PROPERTIES hprops =
      device->dev->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
   D3D12_RESOURCE_DESC rdesc = {
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
      .Width = info->queryCount * query_size,
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .Format = DXGI_FORMAT_UNKNOWN,
      .SampleDesc = { .Count = 1, .Quality = 0 },
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      .Flags = D3D12_RESOURCE_FLAG_NONE,
   };

   hres = device->dev->CreateCommittedResource(&hprops,
                                               D3D12_HEAP_FLAG_NONE,
                                               &rdesc,
                                               D3D12_RESOURCE_STATE_COPY_DEST,
                                               NULL, IID_PPV_ARGS(&resolve_buffer));
   if (FAILED(hres))
      throw vk_error(device, VK_ERROR_OUT_OF_DEVICE_MEMORY);

   hprops = device->dev->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_READBACK);
   rdesc.Width = info->queryCount * (query_size + sizeof(uint64_t));
   hres = device->dev->CreateCommittedResource(&hprops,
                                               D3D12_HEAP_FLAG_NONE,
                                               &rdesc,
                                               D3D12_RESOURCE_STATE_COPY_DEST,
                                               NULL, IID_PPV_ARGS(&collect_buffer));
   if (FAILED(hres))
      throw vk_error(device, VK_ERROR_OUT_OF_DEVICE_MEMORY);

   hres = collect_buffer->Map(0, NULL, (void **)&collect_map);
   if (FAILED(hres))
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   memset(collect_map, 0, rdesc.Width);
}

dzn_query_pool::~dzn_query_pool()
{
   vk_object_base_finish(&base);
}

uint32_t
dzn_query_pool::get_result_offset(uint32_t query)
{
   return query * query_size;
}

uint32_t
dzn_query_pool::get_result_size(uint32_t query_count)
{
   return query_count * query_size;
}

uint32_t
dzn_query_pool::get_availability_offset(uint32_t query)
{
   return (queries.size() * query_size) + (sizeof(uint64_t) * query);
}

uint32_t
collect_buffer_get_result_offset(uint32_t query);
         uint32_t collect_buffer_get_availability_offset(uint32_t query);

void
dzn_query_pool::reset(uint32_t first_query, uint32_t query_count)
{
   for (uint32_t q = 0; q < query_count; q++) {
      auto &query = queries[first_query + q];
      query.fence = ComPtr<ID3D12Fence>(NULL);
      query.fence_value = 0;
      query.status = dzn_query_pool::query::status::RESET;
   }

   memset((uint8_t *)collect_map + get_result_offset(first_query),
          0, query_count * query_size);
   memset((uint8_t *)collect_map + get_availability_offset(first_query),
          0, query_count * sizeof(uint64_t));
}

VkResult
dzn_query_pool::get_results(uint32_t first_query,
                            uint32_t query_count,
                            size_t data_size,
                            void *data,
                            VkDeviceSize stride,
                            VkQueryResultFlags flags)
{
   uint32_t step = (flags & VK_QUERY_RESULT_64_BIT) ?
                   sizeof(uint64_t) : sizeof(uint32_t);
   VkResult result = VK_SUCCESS;

   for (uint32_t q = 0; q < query_count; q++) {
      auto &query = queries[q + first_query];
      uint8_t *dst_ptr = (uint8_t *)data + (stride * q);
      uint8_t *src_ptr =
         (uint8_t *)collect_map + get_result_offset(first_query + q);
      uint64_t available = 0;

      if (flags & VK_QUERY_RESULT_WAIT_BIT) {
         ComPtr<ID3D12Fence> query_fence;
	 uint64_t query_fence_val = 0;

         while (true) {
            query_fence = query.fence;
            query_fence_val = query.fence_value.load();
            if (query_fence.Get() && query_fence_val > 0)
               break;

            /* Check again in 10ms */
            Sleep(10);
         }

         query_fence->SetEventOnCompletion(query_fence_val, NULL);
         available = UINT64_MAX;
      } else {
         ComPtr<ID3D12Fence> query_fence = query.fence;
	 uint64_t query_fence_val = query.fence_value.load();

         if (query_fence.Get() && query_fence_val > 0 &&
             query_fence->GetCompletedValue() >= query_fence_val)
            available = UINT64_MAX;
      }

      if (heap_type != D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS) {
         if (available)
            memcpy(dst_ptr, src_ptr, step);
         else if (flags & VK_QUERY_RESULT_PARTIAL_BIT)
            memset(dst_ptr, 0, step);

         dst_ptr += step;
      } else {
         for (uint32_t c = 0; c < sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) / sizeof(uint64_t); c++) {
            if (!(BITFIELD_BIT(c) & pipeline_statistics))
               continue;

            if (available)
               memcpy(dst_ptr, src_ptr + (c * sizeof(uint64_t)), step);
            else if (flags & VK_QUERY_RESULT_PARTIAL_BIT)
               memset(dst_ptr, 0, step);

            dst_ptr += step;
         }
      }

      if (flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
         memcpy(dst_ptr, &available, step);

      if (!available && !(flags & VK_QUERY_RESULT_PARTIAL_BIT))
         result = VK_NOT_READY;
   }

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateQueryPool(VkDevice device,
                    const VkQueryPoolCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkQueryPool *pQueryPool)
{
   return dzn_query_pool_factory::create(device, pCreateInfo, pAllocator, pQueryPool);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyQueryPool(VkDevice device,
                     VkQueryPool queryPool,
                     const VkAllocationCallbacks *pAllocator)
{
   dzn_query_pool_factory::destroy(device, queryPool, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL
dzn_ResetQueryPool(VkDevice device,
                   VkQueryPool queryPool,
                   uint32_t firstQuery,
                   uint32_t queryCount)
{
   VK_FROM_HANDLE(dzn_query_pool, qpool, queryPool);

   qpool->reset(firstQuery, queryCount);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetQueryPoolResults(VkDevice device,
                        VkQueryPool queryPool,
                        uint32_t firstQuery,
                        uint32_t queryCount,
                        size_t dataSize,
                        void *pData,
                        VkDeviceSize stride,
                        VkQueryResultFlags flags)
{
   VK_FROM_HANDLE(dzn_query_pool, qpool, queryPool);

   return qpool->get_results(firstQuery, queryCount, dataSize, pData, stride, flags);
}
