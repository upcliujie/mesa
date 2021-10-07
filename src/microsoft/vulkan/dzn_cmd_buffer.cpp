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
#include "vk_format.h"
#include "vk_util.h"

VkResult
dzn_CreateCommandPool(VkDevice _device,
                      const VkCommandPoolCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkCommandPool *pCmdPool)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   struct dzn_cmd_pool *pool = (struct dzn_cmd_pool *)
      vk_object_alloc(&device->vk, pAllocator, sizeof(*pool),
                      VK_OBJECT_TYPE_COMMAND_POOL);
   if (pool == NULL)
      vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (pAllocator)
      pool->alloc = *pAllocator;
   else
      pool->alloc = device->vk.alloc;

   list_inithead(&pool->cmd_buffers);

   pool->flags = pCreateInfo->flags;

   *pCmdPool = dzn_cmd_pool_to_handle(pool);

   return VK_SUCCESS;
}

static void
dzn_cmd_free_batch(dzn_cmd_buffer *cmd_buffer, dzn_batch *batch)
{
   util_dynarray_fini(&batch->events.wait);
   util_dynarray_fini(&batch->events.signal);

   if (batch->cmdlist)
      batch->cmdlist->Release();

   vk_free(&cmd_buffer->pool->alloc, batch);
}

static void
dzn_cmd_buffer_destroy(struct dzn_cmd_buffer *cmd_buffer)
{
   list_del(&cmd_buffer->pool_link);

   d3d12_descriptor_pool_free(cmd_buffer->rtv_pool);
   cmd_buffer->rtv_pool = NULL;

   util_dynarray_foreach(&cmd_buffer->batches, dzn_batch *, batch)
      dzn_cmd_free_batch(cmd_buffer, *batch);
   util_dynarray_fini(&cmd_buffer->batches);

   util_dynarray_foreach(&cmd_buffer->heaps, ID3D12DescriptorHeap *, heap)
      (*heap)->Release();

   util_dynarray_fini(&cmd_buffer->heaps);
   vk_command_buffer_finish(&cmd_buffer->vk);
   vk_free2(&cmd_buffer->device->vk.alloc, &cmd_buffer->pool->alloc,
            cmd_buffer);
}

void
dzn_DestroyCommandPool(VkDevice _device,
                       VkCommandPool commandPool,
                       const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_cmd_pool, pool, commandPool);

   if (!pool)
      return;

   list_for_each_entry_safe(struct dzn_cmd_buffer, cmd_buffer,
                            &pool->cmd_buffers, pool_link) {
      dzn_cmd_buffer_destroy(cmd_buffer);
   }

   vk_object_free(&device->vk, pAllocator, pool);
}

static void
dzn_cmd_close_batch(dzn_cmd_buffer *cmd_buffer)
{
   dzn_batch *batch = cmd_buffer->batch;

   if (!batch)
      return;

   batch->cmdlist->Close();
   util_dynarray_append(&cmd_buffer->batches, dzn_batch *, batch);
   cmd_buffer->batch = NULL;
}

static VkResult
dzn_cmd_open_batch(dzn_cmd_buffer *cmd_buffer)
{
   dzn_device *device = cmd_buffer->device;
   dzn_batch *batch = (dzn_batch *)
      vk_zalloc(&cmd_buffer->pool->alloc,
                sizeof(*batch), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

   util_dynarray_init(&batch->events.wait, NULL);
   util_dynarray_init(&batch->events.signal, NULL);

   if (!batch)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (FAILED(device->dev->CreateCommandList(0, cmd_buffer->type, cmd_buffer->alloc, NULL,
                                             IID_PPV_ARGS(&batch->cmdlist))))
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   cmd_buffer->batch = batch;
   return VK_SUCCESS;
}

static dzn_batch *
dzn_cmd_get_batch(dzn_cmd_buffer *cmd_buffer, bool signal_event)
{
   dzn_batch *batch = cmd_buffer->batch;

   if (batch) {
      if (batch->events.signal.size == 0 || signal_event)
         return batch;
 
      /* Close the current batch if there are event signaling pending. */
      dzn_cmd_close_batch(cmd_buffer);
   }

   dzn_cmd_open_batch(cmd_buffer);
   assert(cmd_buffer->batch);
   return cmd_buffer->batch;
}

static VkResult
dzn_create_cmd_buffer(struct dzn_device *device,
                      struct dzn_cmd_pool *pool,
                      VkCommandBufferLevel level,
                      VkCommandBuffer *pCommandBuffer)
{
   VkResult result;

   dzn_cmd_buffer *cmd_buffer = (dzn_cmd_buffer *)
      vk_zalloc2(&device->vk.alloc, &pool->alloc, sizeof(*cmd_buffer), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (cmd_buffer == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   result = vk_command_buffer_init(&cmd_buffer->vk, &device->vk);
   if (result != VK_SUCCESS)
      goto fail;

   util_dynarray_init(&cmd_buffer->heaps, NULL);
   cmd_buffer->device = device;
   cmd_buffer->pool = pool;
   cmd_buffer->level = level;

   cmd_buffer->rtv_pool =
      d3d12_descriptor_pool_new(device->dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);

   if (level == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
      cmd_buffer->type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   else
      cmd_buffer->type = D3D12_COMMAND_LIST_TYPE_BUNDLE;

   if (FAILED(device->dev->CreateCommandAllocator(cmd_buffer->type,
                                                  IID_PPV_ARGS(&cmd_buffer->alloc)))) {
      result = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
      goto fail_cmd_buffer;
   }

   result = dzn_cmd_open_batch(cmd_buffer);
   if (result != VK_SUCCESS)
      goto fail_cmd_buffer;

   list_addtail(&cmd_buffer->pool_link, &pool->cmd_buffers);

   *pCommandBuffer = dzn_cmd_buffer_to_handle(cmd_buffer);

   return VK_SUCCESS;

fail_cmd_buffer:
   vk_command_buffer_finish(&cmd_buffer->vk);

 fail:
   if (cmd_buffer->alloc)
      cmd_buffer->alloc->Release();

   vk_free(&cmd_buffer->pool->alloc, cmd_buffer);

   return result;
}

VkResult
dzn_AllocateCommandBuffers(VkDevice _device,
                           const VkCommandBufferAllocateInfo *pAllocateInfo,
                           VkCommandBuffer *pCommandBuffers)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_cmd_pool, pool, pAllocateInfo->commandPool);

   VkResult result = VK_SUCCESS;
   uint32_t i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {
      result = dzn_create_cmd_buffer(device, pool, pAllocateInfo->level,
                                     &pCommandBuffers[i]);
      if (result != VK_SUCCESS)
         break;
   }

   if (result != VK_SUCCESS) {
      dzn_FreeCommandBuffers(_device, pAllocateInfo->commandPool,
                             i, pCommandBuffers);
      for (i = 0; i < pAllocateInfo->commandBufferCount; i++)
         pCommandBuffers[i] = VK_NULL_HANDLE;
   }

   return result;
}

void
dzn_FreeCommandBuffers(VkDevice device,
                       VkCommandPool commandPool,
                       uint32_t commandBufferCount,
                       const VkCommandBuffer *pCommandBuffers)
{
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, pCommandBuffers[i]);

      if (!cmd_buffer)
         continue;

      dzn_cmd_buffer_destroy(cmd_buffer);
   }
}

VkResult
dzn_cmd_buffer_reset(struct dzn_cmd_buffer *cmd_buffer)
{
   /* TODO: Return batches to the pool instead of freeing them. */
   util_dynarray_foreach(&cmd_buffer->batches, dzn_batch *, batch)
      dzn_cmd_free_batch(cmd_buffer, *batch);
   util_dynarray_clear(&cmd_buffer->batches);

   if (cmd_buffer->batch) {
      dzn_cmd_free_batch(cmd_buffer, cmd_buffer->batch);
      cmd_buffer->batch = NULL;
   }

   /* TODO: Return heaps to the command pool instead of freeing them */
   d3d12_descriptor_pool_free(cmd_buffer->rtv_pool);
   cmd_buffer->rtv_pool =
      d3d12_descriptor_pool_new(cmd_buffer->device->dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);

   util_dynarray_foreach(&cmd_buffer->heaps, ID3D12DescriptorHeap *, heap)
      (*heap)->Release();
   util_dynarray_clear(&cmd_buffer->heaps);
   vk_command_buffer_reset(&cmd_buffer->vk);

   return VK_SUCCESS;
}

VkResult
dzn_ResetCommandBuffer(VkCommandBuffer commandBuffer,
                       VkCommandBufferResetFlags flags)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   return dzn_cmd_buffer_reset(cmd_buffer);
}

VkResult
dzn_BeginCommandBuffer(
    VkCommandBuffer commandBuffer,
    const VkCommandBufferBeginInfo *pBeginInfo)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   /* If this is the first vkBeginCommandBuffer, we must *initialize* the
    * command buffer's state. Otherwise, we must *reset* its state. In both
    * cases we reset it.
    *
    * From the Vulkan 1.0 spec:
    *
    *    If a command buffer is in the executable state and the command buffer
    *    was allocated from a command pool with the
    *    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag set, then
    *    vkBeginCommandBuffer implicitly resets the command buffer, behaving
    *    as if vkResetCommandBuffer had been called with
    *    VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT not set. It then puts
    *    the command buffer in the recording state.
    */
   dzn_cmd_buffer_reset(cmd_buffer);

   cmd_buffer->usage_flags = pBeginInfo->flags;

   /* VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT must be ignored for
    * primary level command buffers.
    *
    * From the Vulkan 1.0 spec:
    *
    *    VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT specifies that a
    *    secondary command buffer is considered to be entirely inside a render
    *    pass. If this is a primary command buffer, then this bit is ignored.
    */
   if (cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
      cmd_buffer->usage_flags &= ~VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

   VkResult result = VK_SUCCESS;

#if 0
   if (cmd_buffer->usage_flags &
       VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) {
      assert(pBeginInfo->pInheritanceInfo);
      DZN_FROM_HANDLE(dzn_render_pass, pass,
                      pBeginInfo->pInheritanceInfo->renderPass);
      struct dzn_subpass *subpass =
         &pass->subpasses[pBeginInfo->pInheritanceInfo->subpass];
      DZN_FROM_HANDLE(dzn_framebuffer, framebuffer,
                      pBeginInfo->pInheritanceInfo->framebuffer);

      cmd_buffer->state.pass = pass;
      cmd_buffer->state.subpass = subpass;
   }
#endif

   return result;
}

VkResult
dzn_EndCommandBuffer(VkCommandBuffer commandBuffer)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   dzn_cmd_close_batch(cmd_buffer);

   return VK_SUCCESS;
}

D3D12_RESOURCE_STATES
dzn_get_states(VkImageLayout layout)
{
   switch (layout) {
   case VK_IMAGE_LAYOUT_PREINITIALIZED:
   case VK_IMAGE_LAYOUT_UNDEFINED:
   case VK_IMAGE_LAYOUT_GENERAL:
      /* YOLO! */
   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return D3D12_RESOURCE_STATE_COMMON;

   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return D3D12_RESOURCE_STATE_COPY_DEST;

   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return D3D12_RESOURCE_STATE_COPY_SOURCE;

   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return D3D12_RESOURCE_STATE_RENDER_TARGET;

   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
   case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
      return D3D12_RESOURCE_STATE_DEPTH_WRITE;

   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
      return D3D12_RESOURCE_STATE_DEPTH_READ;

   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

   default:
      unreachable("not implemented");
   }
}

void
dzn_CmdPipelineBarrier(VkCommandBuffer commandBuffer,
                       VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags destStageMask,
                       VkBool32 byRegion,
                       uint32_t memoryBarrierCount,
                       const VkMemoryBarrier *pMemoryBarriers,
                       uint32_t bufferMemoryBarrierCount,
                       const VkBufferMemoryBarrier * pBufferMemoryBarriers,
                       uint32_t imageMemoryBarrierCount,
                       const VkImageMemoryBarrier *pImageMemoryBarriers)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   /* Global memory barriers can be emulated with NULL UAV/Aliasing barriers.
    * Scopes are not taken into account, but that's inherent to the current
    * D3D12 barrier API.
    */
   if (memoryBarrierCount) {
      D3D12_RESOURCE_BARRIER barriers[2];

      barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barriers[0].UAV.pResource = NULL;
      barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
      barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barriers[1].Aliasing.pResourceBefore = NULL;
      barriers[1].Aliasing.pResourceAfter = NULL;
      batch->cmdlist->ResourceBarrier(2, barriers);
   }

   for (uint32_t i = 0; i < bufferMemoryBarrierCount; i++) {
      DZN_FROM_HANDLE(dzn_buffer, buf, pBufferMemoryBarriers[i].buffer);
      D3D12_RESOURCE_BARRIER barrier;

      /* UAV are used only for storage buffers, skip all other buffers. */
      if (!(buf->usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
         continue;

      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.UAV.pResource = buf->res;
      batch->cmdlist->ResourceBarrier(1, &barrier);
   }

   for (uint32_t i = 0; i < imageMemoryBarrierCount; i++) {
      /* D3D12_RESOURCE_BARRIER_TYPE_TRANSITION */
      DZN_FROM_HANDLE(dzn_image, image, pImageMemoryBarriers[i].image);
      const VkImageSubresourceRange *range =
         &pImageMemoryBarriers[i].subresourceRange;

      uint32_t base_layer, layer_count;
      if (image->vk.image_type == VK_IMAGE_TYPE_3D) {
         base_layer = 0;
         layer_count = u_minify(image->vk.extent.depth, range->baseMipLevel);
      } else {
         base_layer = range->baseArrayLayer;
         layer_count = dzn_get_layerCount(image, range);
      }

      D3D12_RESOURCE_BARRIER barriers[2];
      /* We use placed resource's simple model, in which only one resource
       * pointing to a given heap is active at a given time. To make the
       * resource active we need to add an aliasing barrier.
       */
      barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
      barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barriers[0].Aliasing.pResourceBefore = NULL;
      barriers[0].Aliasing.pResourceAfter = image->res;
      barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

      barriers[1].Transition.pResource = image->res;
      assert(base_layer == 0 && layer_count == 1);
      barriers[1].Transition.Subresource = 0; // YOLO

      if (pImageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
          pImageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
         barriers[1].Transition.StateBefore = image->mem->initial_state;
      else
         barriers[1].Transition.StateBefore = dzn_get_states(pImageMemoryBarriers[i].oldLayout);

      barriers[1].Transition.StateAfter = dzn_get_states(pImageMemoryBarriers[i].newLayout);

      /* some layouts map to the states, and NOP-barriers are illegal */
      unsigned nbarriers = 1 + barriers[1].Transition.StateBefore != barriers[1].Transition.StateAfter;
      batch->cmdlist->ResourceBarrier(nbarriers, barriers);
   }
}

void dzn_CmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer,
                                  const VkCopyBufferToImageInfo2KHR *pCopyBufferToImageInfo)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_buffer, src_buffer, pCopyBufferToImageInfo->srcBuffer);
   DZN_FROM_HANDLE(dzn_image, dst_image, pCopyBufferToImageInfo->dstImage);

   D3D12_TEXTURE_COPY_LOCATION src_buf_loc = {
      .pResource = src_buffer->res,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
   };

   D3D12_TEXTURE_COPY_LOCATION dst_img_loc = {
      .pResource = dst_image->res,
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
   };

   ID3D12Device *dev = cmd_buffer->device->dev;

   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist;

   for (int i = 0; i < pCopyBufferToImageInfo->regionCount; i++) {
      const VkBufferImageCopy2KHR *region = &pCopyBufferToImageInfo->pRegions[i];

      const uint32_t buffer_row_length =
         region->bufferRowLength ?
         region->bufferRowLength : region->imageExtent.width;

      const uint32_t buffer_image_height =
         region->bufferImageHeight ?
         region->bufferImageHeight : region->imageExtent.height;

      /* prepare source details */
      dev->GetCopyableFootprints(&dst_image->desc, 0, 1, 0,
                                 &src_buf_loc.PlacedFootprint,
                                 NULL, NULL, NULL);
      src_buf_loc.PlacedFootprint.Footprint.Width = buffer_row_length;
      src_buf_loc.PlacedFootprint.Footprint.Height = buffer_image_height;
      src_buf_loc.PlacedFootprint.Footprint.Depth = 1;
      src_buf_loc.PlacedFootprint.Offset += region->bufferOffset;
      D3D12_BOX src_box = {
         .left = 0,
         .top = 0,
         .front = 0,
         .right = region->imageExtent.width,
         .bottom = region->imageExtent.height,
         .back = region->imageExtent.depth,
      };

      /* prepare destination details */
      dst_img_loc.SubresourceIndex =
         dzn_get_subresource_index(&dst_image->desc,
                                   region->imageSubresource.aspectMask,
                                   region->imageSubresource.mipLevel,
                                   region->imageSubresource.baseArrayLayer);

      assert(region->imageSubresource.layerCount == 1);

      cmdlist->CopyTextureRegion(&dst_img_loc, region->imageOffset.x,
                                 region->imageOffset.y, region->imageOffset.z,
                                 &src_buf_loc, &src_box);
   }
}

void
dzn_CmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer,
                             const VkCopyImageToBufferInfo2KHR *pCopyImageToBufferInfo)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_image, src_image, pCopyImageToBufferInfo->srcImage);
   DZN_FROM_HANDLE(dzn_buffer, dst_buffer, pCopyImageToBufferInfo->dstBuffer);

   D3D12_TEXTURE_COPY_LOCATION dst_buf_loc = {
      .pResource = dst_buffer->res,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
   };

   D3D12_TEXTURE_COPY_LOCATION src_img_loc = {
      .pResource = src_image->res,
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
   };

   ID3D12Device *dev = cmd_buffer->device->dev;
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist;

   for (int i = 0; i < pCopyImageToBufferInfo->regionCount; i++) {
      const VkBufferImageCopy2KHR *region = &pCopyImageToBufferInfo->pRegions[i];

      const uint32_t buffer_row_length =
         region->bufferRowLength ?
         region->bufferRowLength : region->imageExtent.width;

      const uint32_t buffer_image_height =
         region->bufferImageHeight ?
         region->bufferImageHeight : region->imageExtent.height;

      /* prepare destination details */
      dev->GetCopyableFootprints(&src_image->desc, 0, 1, 0,
                                 &dst_buf_loc.PlacedFootprint,
                                 NULL, NULL, NULL);
      dst_buf_loc.PlacedFootprint.Footprint.Width = buffer_row_length;
      dst_buf_loc.PlacedFootprint.Footprint.Height = buffer_image_height;
      dst_buf_loc.PlacedFootprint.Footprint.Depth = 1;
      dst_buf_loc.PlacedFootprint.Offset += region->bufferOffset;
      D3D12_BOX src_box = {
         .left = 0,
         .top = 0,
         .front = 0,
         .right = region->imageExtent.width,
         .bottom = region->imageExtent.height,
         .back = region->imageExtent.depth,
      };

      /* prepare source details */
      src_img_loc.SubresourceIndex =
         dzn_get_subresource_index(&src_image->desc,
                                   region->imageSubresource.aspectMask,
                                   region->imageSubresource.mipLevel,
                                   region->imageSubresource.baseArrayLayer);

      assert(region->imageSubresource.layerCount == 1);

      cmdlist->CopyTextureRegion(&dst_buf_loc, region->imageOffset.x,
                                 region->imageOffset.y, region->imageOffset.z,
                                 &src_img_loc, &src_box);
   }
}

static void
dzn_fill_image_copy_loc(const dzn_image *img,
                        const VkImageSubresourceLayers *subres,
                        D3D12_TEXTURE_COPY_LOCATION *loc)
{
   loc->pResource = img->res;
   if (img->desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
      assert(subres->baseArrayLayer == 0);
      assert(subres->mipLevel == 0);
      loc->Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      loc->PlacedFootprint.Offset = 0;
      loc->PlacedFootprint.Footprint.Format = dzn_get_format(img->vk.format);
      loc->PlacedFootprint.Footprint.Width = img->vk.extent.width;
      loc->PlacedFootprint.Footprint.Height = img->vk.extent.height;
      loc->PlacedFootprint.Footprint.Depth = img->vk.extent.depth;
      loc->PlacedFootprint.Footprint.RowPitch = img->linear.row_stride;
   } else {
      loc->Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      loc->SubresourceIndex =
         dzn_get_subresource_index(&img->desc,
                                   subres->aspectMask,
                                   subres->mipLevel,
                                   subres->baseArrayLayer);
   }
}

void
dzn_CmdCopyImage2KHR(VkCommandBuffer commandBuffer,
                     const VkCopyImageInfo2KHR *pCopyImageInfo)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_image, src, pCopyImageInfo->srcImage);
   DZN_FROM_HANDLE(dzn_image, dst, pCopyImageInfo->dstImage);

   ID3D12Device *dev = cmd_buffer->device->dev;
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist;

   assert(src->vk.samples == dst->vk.samples);

   /* TODO: MS copies */
   assert(src->vk.samples == 1);

   for (int i = 0; i < pCopyImageInfo->regionCount; i++) {
      const VkImageCopy2KHR *region = &pCopyImageInfo->pRegions[i];
      VkImageSubresourceLayers src_subres = region->srcSubresource;
      VkImageSubresourceLayers dst_subres = region->dstSubresource;

      assert(src_subres.layerCount == dst_subres.layerCount);

      for (uint32_t l = 0; l < src_subres.layerCount; l++) {
         D3D12_TEXTURE_COPY_LOCATION dst_loc = { 0 }, src_loc = { 0 };

         dzn_fill_image_copy_loc(src, &src_subres, &src_loc);
         dzn_fill_image_copy_loc(dst, &dst_subres, &dst_loc);

         D3D12_BOX src_box = {
            .left = (UINT)MAX2(region->srcOffset.x, 0),
            .top = (UINT)MAX2(region->srcOffset.y, 0),
            .front = (UINT)MAX2(region->srcOffset.z, 0),
            .right = (UINT)region->srcOffset.x + region->extent.width,
            .bottom = (UINT)region->srcOffset.y + region->extent.height,
            .back = (UINT)region->srcOffset.z + region->extent.depth,
         };

         cmdlist->CopyTextureRegion(&dst_loc, region->dstOffset.x,
                                    region->dstOffset.y, region->dstOffset.z,
                                    &src_loc, &src_box);
      }
   }
}

void
dzn_CmdClearColorImage(VkCommandBuffer commandBuffer,
                       VkImage image,
                       VkImageLayout imageLayout,
                       const VkClearColorValue *pColor,
                       uint32_t rangeCount,
                       const VkImageSubresourceRange *pRanges)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);
   dzn_device *device = cmd_buffer->device;
   DZN_FROM_HANDLE(dzn_image, img, image);
   D3D12_RENDER_TARGET_VIEW_DESC desc = {
      .Format = img->desc.Format,
   };

   switch (img->vk.image_type) {
   case VK_IMAGE_TYPE_1D:
      desc.ViewDimension =
         img->vk.array_layers > 1 ?
         D3D12_RTV_DIMENSION_TEXTURE1DARRAY : D3D12_RTV_DIMENSION_TEXTURE1D;
      break;
   case VK_IMAGE_TYPE_2D:
      if (img->vk.array_layers > 1) {
         desc.ViewDimension =
            img->vk.samples > 1 ?
            D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY :
            D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
      } else {
         desc.ViewDimension =
            img->vk.samples > 1 ?
            D3D12_RTV_DIMENSION_TEXTURE2DMS :
            D3D12_RTV_DIMENSION_TEXTURE2D;
      }
      break;
   case VK_IMAGE_TYPE_3D:
      desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
      break;
   default: unreachable("Invalid image type\n");
   }

   for (uint32_t r = 0; r < rangeCount; r++) {
      const VkImageSubresourceRange *range = &pRanges[r];

      for (uint32_t l = 0; l < range->levelCount; l++) {
         switch (desc.ViewDimension) {
         case D3D12_RTV_DIMENSION_TEXTURE1D:
            desc.Texture1D.MipSlice = range->baseMipLevel + l;
            break;
         case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
            desc.Texture1DArray.MipSlice = range->baseMipLevel + l;
            desc.Texture1DArray.FirstArraySlice = range->baseArrayLayer;
            desc.Texture1DArray.ArraySize = range->layerCount;
            break;
         case D3D12_RTV_DIMENSION_TEXTURE2D:
            desc.Texture2D.MipSlice = range->baseMipLevel + l;
            if (range->aspectMask & VK_IMAGE_ASPECT_PLANE_1_BIT)
               desc.Texture2D.PlaneSlice = 1;
            else if (range->aspectMask & VK_IMAGE_ASPECT_PLANE_2_BIT)
               desc.Texture2D.PlaneSlice = 2;
            else
               desc.Texture2D.PlaneSlice = 0;
            break;
         case D3D12_RTV_DIMENSION_TEXTURE2DMS:
            break;
         case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
            desc.Texture2DArray.MipSlice = range->baseMipLevel + l;
            desc.Texture2DArray.FirstArraySlice = range->baseArrayLayer;
            desc.Texture2DArray.ArraySize = range->layerCount;
            if (range->aspectMask & VK_IMAGE_ASPECT_PLANE_1_BIT)
               desc.Texture2DArray.PlaneSlice = 1;
            else if (range->aspectMask & VK_IMAGE_ASPECT_PLANE_2_BIT)
               desc.Texture2DArray.PlaneSlice = 2;
            else
               desc.Texture2DArray.PlaneSlice = 0;
            break;
         case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
            desc.Texture2DMSArray.FirstArraySlice = range->baseArrayLayer;
            desc.Texture2DMSArray.ArraySize = range->layerCount;
            break;
         case D3D12_RTV_DIMENSION_TEXTURE3D:
            desc.Texture3D.MipSlice = range->baseMipLevel + l;
            desc.Texture3D.FirstWSlice = range->baseArrayLayer;
            desc.Texture3D.WSize = range->layerCount;
            break;
         }

         struct d3d12_descriptor_handle handle;
         d3d12_descriptor_pool_alloc_handle(cmd_buffer->rtv_pool, &handle);
         device->dev->CreateRenderTargetView(img->res, &desc,
                                            handle.cpu_handle);
         batch->cmdlist->ClearRenderTargetView(handle.cpu_handle,
                                               pColor->float32, 0, NULL);
      }
   }
}

void
dzn_CmdBeginRenderPass2(VkCommandBuffer commandBuffer,
                        const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                        const VkSubpassBeginInfoKHR *pSubpassBeginInfo)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_render_pass, pass, pRenderPassBeginInfo->renderPass);
   DZN_FROM_HANDLE(dzn_framebuffer, framebuffer, pRenderPassBeginInfo->framebuffer);
   const struct dzn_subpass *subpass = &pass->subpasses[0];
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   cmd_buffer->state.framebuffer = framebuffer;
   cmd_buffer->state.pass = pass;
   cmd_buffer->state.subpass = 0;

   D3D12_CPU_DESCRIPTOR_HANDLE rt_handles[MAX_RTS] = { };
   D3D12_CPU_DESCRIPTOR_HANDLE zs_handle = { 0 };

   for (uint32_t i = 0; i < subpass->color_count; i++) {
      if (subpass->colors[i].idx == VK_ATTACHMENT_UNUSED) continue;

      if (!i)
         cmd_buffer->rt0 = framebuffer->attachments[subpass->colors[i].idx]->image->res;
      rt_handles[i] = framebuffer->attachments[subpass->colors[i].idx]->rt_handle.cpu_handle;
   }

   if (subpass->zs.idx != VK_ATTACHMENT_UNUSED) {
      zs_handle = framebuffer->attachments[subpass->zs.idx]->zs_handle.cpu_handle;
   }

   assert(pass->attachment_count == framebuffer->attachment_count);

   for (uint32_t i = 0; i < pass->attachment_count; i++) {
      const dzn_attachment *att = &pass->attachments[i];
      const dzn_image *image = framebuffer->attachments[i]->image;

      if (att->before == att->during)
         continue;

      D3D12_RESOURCE_BARRIER barrier;
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

      barrier.Transition.pResource = image->res;
      barrier.Transition.Subresource = 0; // YOLO

      barrier.Transition.StateBefore = att->before;
      barrier.Transition.StateAfter = att->during;

      batch->cmdlist->ResourceBarrier(1, &barrier);
   }

   assert(subpass->color_count > 0);
   batch->cmdlist->OMSetRenderTargets(subpass->color_count, rt_handles, FALSE, zs_handle.ptr ? &zs_handle : NULL);

   D3D12_RECT rect = {
      pRenderPassBeginInfo->renderArea.offset.x,
      pRenderPassBeginInfo->renderArea.offset.y,
      pRenderPassBeginInfo->renderArea.offset.x + pRenderPassBeginInfo->renderArea.extent.width,
      pRenderPassBeginInfo->renderArea.offset.y + pRenderPassBeginInfo->renderArea.extent.height
   };

   assert(pRenderPassBeginInfo->clearValueCount <= framebuffer->attachment_count);
   for (int i = 0; i < pRenderPassBeginInfo->clearValueCount; ++i) {
      if (vk_format_is_depth_or_stencil(framebuffer->attachments[i]->vk_format)) {
         D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;

         if (pass->attachments[i].clear.depth)
            flags |= D3D12_CLEAR_FLAG_DEPTH;
         if (pass->attachments[i].clear.stencil)
            flags |= D3D12_CLEAR_FLAG_STENCIL;

         if (flags != 0)
            batch->cmdlist->ClearDepthStencilView(framebuffer->attachments[i]->zs_handle.cpu_handle,
                                                  flags,
                                                  pRenderPassBeginInfo->pClearValues[i].depthStencil.depth,
                                                  pRenderPassBeginInfo->pClearValues[i].depthStencil.stencil,
                                                  1, &rect);
      } else if (pass->attachments[i].clear.color) {
         batch->cmdlist->ClearRenderTargetView(framebuffer->attachments[i]->rt_handle.cpu_handle,
                                               pRenderPassBeginInfo->pClearValues[i].color.float32,
                                               1, &rect);
      }
   }
}

void
dzn_CmdEndRenderPass2(VkCommandBuffer commandBuffer,
                      const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   assert(cmd_buffer->state.pass->attachment_count ==
          cmd_buffer->state.framebuffer->attachment_count);
   for (uint32_t i = 0; i < cmd_buffer->state.pass->attachment_count; i++) {
      const dzn_attachment *att = &cmd_buffer->state.pass->attachments[i];
      const dzn_image *image = cmd_buffer->state.framebuffer->attachments[i]->image;

      if (att->during == att->after)
         continue;

      D3D12_RESOURCE_BARRIER barrier;
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

      barrier.Transition.pResource = image->res;
      barrier.Transition.Subresource = 0; // YOLO

      barrier.Transition.StateBefore = att->during;
      barrier.Transition.StateAfter = att->after;

      batch->cmdlist->ResourceBarrier(1, &barrier);
   }

   cmd_buffer->state.framebuffer = NULL;
   cmd_buffer->state.pass = NULL;

#if 0
   cmd_buffer->state.subpass = NULL;
#endif
}

void
dzn_CmdBindPipeline(VkCommandBuffer commandBuffer,
                    VkPipelineBindPoint pipelineBindPoint,
                    VkPipeline _pipeline)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_pipeline, pipeline, _pipeline);

   cmd_buffer->state.bindpoint[pipelineBindPoint].pipeline = pipeline;
   cmd_buffer->state.bindpoint[pipelineBindPoint].dirty |= DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
   if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      const dzn_graphics_pipeline *gfx = (const dzn_graphics_pipeline *)pipeline;

      memcpy(cmd_buffer->state.viewports, gfx->vp.desc,
             gfx->vp.count * sizeof(cmd_buffer->state.viewports[0]));
      memcpy(cmd_buffer->state.scissors, gfx->scissor.desc,
             gfx->scissor.count * sizeof(cmd_buffer->state.scissors[0]));
      cmd_buffer->state.dirty |= DZN_CMD_DIRTY_VIEWPORTS | DZN_CMD_DIRTY_SCISSORS;

      for (uint32_t vb = 0; vb < gfx->vb.count; vb++)
         cmd_buffer->state.vb.views[vb].StrideInBytes = gfx->vb.strides[vb];

      if (gfx->vb.count > 0)
         BITSET_SET_RANGE(cmd_buffer->state.vb.dirty, 0, gfx->vb.count - 1);
   }
}

static void
update_pipeline(dzn_cmd_buffer *cmd_buffer, uint32_t bindpoint)
{
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   if (cmd_buffer->state.bindpoint[bindpoint].pipeline &&
       cmd_buffer->state.bindpoint[bindpoint].pipeline != cmd_buffer->state.pipeline) {
      cmd_buffer->state.pipeline = cmd_buffer->state.bindpoint[bindpoint].pipeline;
      batch->cmdlist->SetGraphicsRootSignature(cmd_buffer->state.pipeline->layout->root.sig);
      batch->cmdlist->SetPipelineState(cmd_buffer->state.pipeline->state);
      if (bindpoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
         struct dzn_graphics_pipeline *gfx =
            container_of(cmd_buffer->state.pipeline, struct dzn_graphics_pipeline, base);
         batch->cmdlist->IASetPrimitiveTopology(gfx->ia.topology);
      }
   }
}

static void
update_heaps(dzn_cmd_buffer *cmd_buffer, uint32_t bindpoint)
{
   ID3D12DescriptorHeap **heaps = cmd_buffer->state.bindpoint[bindpoint].heaps;
   const struct dzn_pipeline *pipeline = cmd_buffer->state.bindpoint[bindpoint].pipeline;
   uint32_t view_desc_sz =
      cmd_buffer->device->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   uint32_t sampler_desc_sz =
      cmd_buffer->device->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   if (!(cmd_buffer->state.bindpoint[bindpoint].dirty & DZN_CMD_BINDPOINT_DIRTY_HEAPS))
      goto set_heaps;

   for (uint32_t type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; type++) {
      if (heaps[type]) {
         util_dynarray_append(&cmd_buffer->heaps, ID3D12DescriptorHeap *,
                              cmd_buffer->state.bindpoint[bindpoint].heaps[type]);
         heaps[type] = NULL;
      }

      uint32_t desc_count = cmd_buffer->state.pipeline->layout->desc_count[type];
      if (!desc_count)
         continue;

      D3D12_DESCRIPTOR_HEAP_DESC desc = {
         .Type = (D3D12_DESCRIPTOR_HEAP_TYPE)type,
         .NumDescriptors = desc_count,
         .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      };

      HRESULT ret =
         cmd_buffer->device->dev->CreateDescriptorHeap(&desc,
                                                       IID_PPV_ARGS(&heaps[type]));
      assert(!FAILED(ret));

      D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = {
         .ptr = heaps[type]->GetCPUDescriptorHandleForHeapStart().ptr,
      };

      for (uint32_t s = 0; s < MAX_SETS; s++) {
         const struct dzn_descriptor_set *set =
            cmd_buffer->state.bindpoint[bindpoint].sets[s];

         if (!set) continue;

         uint32_t set_desc_count =
            type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ?
            set->layout->view_desc_count : set->layout->sampler_desc_count;
         uint32_t desc_sz =
            type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ?
            view_desc_sz : sampler_desc_sz;

         if (!set_desc_count) continue;

         D3D12_CPU_DESCRIPTOR_HANDLE src_handle = {
            .ptr = set->heaps[type]->GetCPUDescriptorHandleForHeapStart().ptr,
         };

         cmd_buffer->device->dev->CopyDescriptorsSimple(set_desc_count,
                                                        dst_handle,
                                                        src_handle,
						        desc.Type);
         dst_handle.ptr += (desc_sz * set_desc_count);
      }
   }

set_heaps:
   if (heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] != cmd_buffer->state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] ||
       heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] != cmd_buffer->state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]) {
      if (heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] && heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER])
         batch->cmdlist->SetDescriptorHeaps(2, heaps);
      else if (heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV])
         batch->cmdlist->SetDescriptorHeaps(1, &heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]);
      else if (heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER])
         batch->cmdlist->SetDescriptorHeaps(1, &heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]);
      memcpy(cmd_buffer->state.heaps, heaps, sizeof(cmd_buffer->state.heaps));

      for (uint32_t r = 0; r < pipeline->layout->root.param_count; r++) {
         if (bindpoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
            D3D12_DESCRIPTOR_HEAP_TYPE type = pipeline->layout->root.type[r];
            D3D12_GPU_DESCRIPTOR_HANDLE handle = {
               .ptr = heaps[type]->GetGPUDescriptorHandleForHeapStart().ptr,
            };

            batch->cmdlist->SetGraphicsRootDescriptorTable(r, handle);
         }
      }
   }
}

void
dzn_CmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                          VkPipelineBindPoint pipelineBindPoint,
                          VkPipelineLayout _layout,
                          uint32_t firstSet,
                          uint32_t descriptorSetCount,
                          const VkDescriptorSet *pDescriptorSets,
                          uint32_t dynamicOffsetCount,
                          const uint32_t *pDynamicOffsets)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_pipeline_layout, layout, _layout);

   for (uint32_t i = 0; i < descriptorSetCount; i++) {
      DZN_FROM_HANDLE(dzn_descriptor_set, set, pDescriptorSets[i]);
      cmd_buffer->state.bindpoint[pipelineBindPoint].sets[firstSet + i] = set;
   }

   cmd_buffer->state.bindpoint[pipelineBindPoint].dirty |= DZN_CMD_BINDPOINT_DIRTY_HEAPS;
}

static void
update_viewports(dzn_cmd_buffer *cmd_buffer)
{
   struct dzn_graphics_pipeline *pipeline =
      container_of(cmd_buffer->state.pipeline, struct dzn_graphics_pipeline, base);
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   if (!(cmd_buffer->state.dirty & DZN_CMD_DIRTY_VIEWPORTS) ||
       !pipeline->vp.count)
      return;

   batch->cmdlist->RSSetViewports(pipeline->vp.count,
                                  cmd_buffer->state.viewports);
}

void
dzn_CmdSetViewport(VkCommandBuffer commandBuffer,
                   uint32_t firstViewport,
                   uint32_t viewportCount,
                   const VkViewport *pViewports)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   for (uint32_t i = firstViewport; i < firstViewport + viewportCount; i++)
      dzn_translate_viewport(&cmd_buffer->state.viewports[i], &pViewports[i]);

   if (viewportCount)
      cmd_buffer->state.dirty |= DZN_CMD_DIRTY_VIEWPORTS;
}

static void
update_scissors(dzn_cmd_buffer *cmd_buffer)
{
   struct dzn_graphics_pipeline *pipeline =
      container_of(cmd_buffer->state.pipeline, struct dzn_graphics_pipeline, base);
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   if (!(cmd_buffer->state.dirty & DZN_CMD_DIRTY_SCISSORS) ||
       !pipeline->scissor.count)
      return;

   batch->cmdlist->RSSetScissorRects(pipeline->scissor.count,
                                     cmd_buffer->state.scissors);
}

void
dzn_CmdSetScissor(VkCommandBuffer commandBuffer,
                  uint32_t firstScissor,
                  uint32_t scissorCount,
                  const VkRect2D *pScissors)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   for (uint32_t i = firstScissor; i < firstScissor + scissorCount; i++)
      dzn_translate_scissor(&cmd_buffer->state.scissors[i], &pScissors[i]);

   if (scissorCount)
      cmd_buffer->state.dirty |= DZN_CMD_DIRTY_SCISSORS;
}

static void
update_vbviews(dzn_cmd_buffer *cmd_buffer)
{
   struct dzn_graphics_pipeline *pipeline =
      container_of(cmd_buffer->state.pipeline, struct dzn_graphics_pipeline, base);
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);
   unsigned start, end;

   BITSET_FOREACH_RANGE(start, end, cmd_buffer->state.vb.dirty, MAX_VBS)
      batch->cmdlist->IASetVertexBuffers(start, end - start, cmd_buffer->state.vb.views);

   BITSET_CLEAR_RANGE(cmd_buffer->state.vb.dirty, 0, MAX_VBS);
}

void
dzn_CmdDraw(VkCommandBuffer commandBuffer,
            uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   update_pipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
   update_heaps(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
   update_viewports(cmd_buffer);
   update_scissors(cmd_buffer);
   update_vbviews(cmd_buffer);
   cmd_buffer->state.dirty = 0;

   batch->cmdlist->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void
dzn_CmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                         uint32_t firstBinding,
                         uint32_t bindingCount,
                         const VkBuffer *pBuffers,
                         const VkDeviceSize *pOffsets)
{
   if (!bindingCount)
      return;

   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   D3D12_VERTEX_BUFFER_VIEW *vbviews = cmd_buffer->state.vb.views;

   for (uint32_t i = 0; i < bindingCount; i++) {
      DZN_FROM_HANDLE(dzn_buffer, buf, pBuffers[i]);

      vbviews[firstBinding + i].BufferLocation = buf->res->GetGPUVirtualAddress() + pOffsets[i];
      vbviews[firstBinding + i].SizeInBytes = buf->size - pOffsets[i];
   }

   BITSET_SET_RANGE(cmd_buffer->state.vb.dirty, firstBinding,
                    firstBinding + bindingCount - 1);
}

void
dzn_CmdResetEvent(VkCommandBuffer commandBuffer,
                  VkEvent _event,
                  VkPipelineStageFlags stageMask)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_event, event, _event);

   struct dzn_cmd_event_signal signal = {
      .event = event,
      .value = false,
   };

   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, true);

   util_dynarray_append(&batch->events.signal,
                        struct dzn_cmd_event_signal, signal);
}

void
dzn_CmdSetEvent(VkCommandBuffer commandBuffer,
                VkEvent _event,
                VkPipelineStageFlags stageMask)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   DZN_FROM_HANDLE(dzn_event, event, _event);

   struct dzn_cmd_event_signal signal = {
      .event = event,
      .value = true,
   };

   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, true);

   util_dynarray_append(&batch->events.signal,
                        struct dzn_cmd_event_signal, signal);
}

void
dzn_CmdWaitEvents(VkCommandBuffer commandBuffer,
                  uint32_t eventCount,
                  const VkEvent *pEvents,
                  VkPipelineStageFlags srcStageMask,
                  VkPipelineStageFlags dstStageMask,
                  uint32_t memoryBarrierCount,
                  const VkMemoryBarrier *pMemoryBarriers,
                  uint32_t bufferMemoryBarrierCount,
                  const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                  uint32_t imageMemoryBarrierCount,
                  const VkImageMemoryBarrier *pImageMemoryBarriers)
{
   DZN_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   dzn_batch *batch = dzn_cmd_get_batch(cmd_buffer, false);

   for (uint32_t i = 0; i < eventCount; i++) {
      DZN_FROM_HANDLE(dzn_event, event, pEvents[i]);

      util_dynarray_append(&batch->events.signal,
                           dzn_event *, event);
   }
}
