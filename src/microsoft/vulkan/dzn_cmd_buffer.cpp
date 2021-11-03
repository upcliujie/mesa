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

dzn_cmd_pool::dzn_cmd_pool(dzn_device *device,
                           const VkCommandPoolCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator) :
                          flags(pCreateInfo->flags)
{
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_COMMAND_POOL);
   alloc = pAllocator ? *pAllocator : device->vk.alloc;
}

dzn_cmd_pool::~dzn_cmd_pool()
{
   vk_object_base_finish(&base);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateCommandPool(VkDevice device,
                      const VkCommandPoolCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkCommandPool *pCmdPool)
{
   return dzn_cmd_pool_factory::create(device,
                                       pCreateInfo,
                                       pAllocator,
                                       pCmdPool);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyCommandPool(VkDevice device,
                       VkCommandPool commandPool,
                       const VkAllocationCallbacks *pAllocator)
{
   dzn_cmd_pool_factory::destroy(device,
                                 commandPool,
                                 pAllocator);
}

dzn_batch::dzn_batch(dzn_cmd_buffer *cmd_buffer):
                    wait(wait_allocator(&cmd_buffer->pool->alloc)),
                    signal(signal_allocator(&cmd_buffer->pool->alloc))
{
   pool = cmd_buffer->pool;
   if (FAILED(cmd_buffer->device->dev->CreateCommandList(0, cmd_buffer->type,
                                                         cmd_buffer->alloc.Get(), NULL,
                                                         IID_PPV_ARGS(&cmdlist))))
      throw vk_error(cmd_buffer->device, VK_ERROR_OUT_OF_HOST_MEMORY);
}

dzn_batch::~dzn_batch()
{
}

const VkAllocationCallbacks *
dzn_batch::get_vk_allocator()
{
   return &pool->alloc;
}

dzn_batch *
dzn_batch::create(dzn_cmd_buffer *cmd_buffer)
{
   dzn_batch *batch = (dzn_batch *)
      vk_zalloc(&cmd_buffer->pool->alloc,
                sizeof(*batch), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (!batch)
      throw vk_error(cmd_buffer->device, VK_ERROR_OUT_OF_HOST_MEMORY);

   try {
      std::construct_at(batch, cmd_buffer);
   } catch (VkResult result) {
      vk_free(&cmd_buffer->pool->alloc, batch);
      throw result;
   }

   return batch;
}

void
dzn_batch::destroy(dzn_batch *batch, struct dzn_cmd_buffer *cmd_buffer)
{
   std::destroy_at(batch);
   vk_free(&cmd_buffer->pool->alloc, batch);
}

dzn_cmd_buffer::dzn_cmd_buffer(dzn_device *dev,
                               dzn_cmd_pool *cmd_pool,
                               VkCommandBufferLevel lvl,
                               const VkAllocationCallbacks *pAllocator) :
                              sysval_bufs(sysval_bufs_allocator(pAllocator ? pAllocator : &cmd_pool->alloc)),
                              heaps(heaps_allocator(pAllocator ? pAllocator : &cmd_pool->alloc)),
                              batches(batches_allocator(pAllocator ? pAllocator : &cmd_pool->alloc))
{
   device = dev;
   level = lvl;
   pool = cmd_pool;

   VkResult result =
      vk_command_buffer_init(&vk, &device->vk);

   if (result != VK_SUCCESS)
      throw vk_error(device, result);

   struct d3d12_descriptor_pool *pool =
      d3d12_descriptor_pool_new(device->dev.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);

   rtv_pool = std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter>(pool);

   if (level == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
      type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   else
      type = D3D12_COMMAND_LIST_TYPE_BUNDLE;

   if (FAILED(device->dev->CreateCommandAllocator(type,
                                                  IID_PPV_ARGS(&alloc)))) {
      vk_command_buffer_finish(&vk);
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   try {
      open_batch();
   } catch (VkResult error) {
      vk_command_buffer_finish(&vk);
      throw error;
   }
}

dzn_cmd_buffer::~dzn_cmd_buffer()
{
   if (state.sysval_mem.res) {
      state.sysval_mem.res->Unmap(0, NULL);
      state.sysval_mem.res = NULL;
      state.sysval_mem.cpu = NULL;
      state.sysval_mem.offset = 0;
      state.sysval_mem.size = 0;
   }

   vk_command_buffer_finish(&vk);
}

void
dzn_cmd_buffer::close_batch()
{
   if (!batch.get())
      return;

   batch->cmdlist->Close();
   batches.push_back(dzn_object_unique_ptr<dzn_batch>(batch.release()));
   assert(batch.get() == NULL);
}

void
dzn_cmd_buffer::open_batch()
{
   batch = dzn_object_unique_ptr<dzn_batch>(dzn_batch::create(this));
}

dzn_batch *
dzn_cmd_buffer::get_batch(bool signal_event)
{
   if (batch.get()) {
      if (batch->signal.size() == 0 || signal_event)
         return batch.get();
 
      /* Close the current batch if there are event signaling pending. */
      close_batch();
   }

   open_batch();
   assert(batch.get());
   return batch.get();
}

void
dzn_cmd_buffer::reset()
{
   /* TODO: Return heaps to the command pool instead of freeing them */
   struct d3d12_descriptor_pool *pool =
      d3d12_descriptor_pool_new(device->dev.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);

   rtv_pool.reset(pool);

   /* TODO: Return sysval buffers to the pool instead of freeing them. */
   if (state.sysval_mem.res) {
      state.sysval_mem.res->Unmap(0, NULL);
      state.sysval_mem.res = NULL;
      state.sysval_mem.cpu = NULL;
      state.sysval_mem.offset = 0;
      state.sysval_mem.size = 0;
   }
   sysval_bufs.clear();

   /* TODO: Return batches to the pool instead of freeing them. */
   batches.clear();
   batch.reset(NULL);

   vk_command_buffer_reset(&vk);
}

VkResult
dzn_cmd_pool::allocate_cmd_buffers(VkDevice device,
                                   const VkCommandBufferAllocateInfo *pAllocateInfo,
                                   VkCommandBuffer *pCommandBuffers)
{
   VkResult result = VK_SUCCESS;
   uint32_t i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {
      result = dzn_cmd_buffer_factory::create(device, this,
                                              pAllocateInfo->level,
                                              &alloc,
                                              &pCommandBuffers[i]);
      if (result != VK_SUCCESS)
         break;
   }

   if (result != VK_SUCCESS) {
      dzn_cmd_pool::free_cmd_buffers(device, i, pCommandBuffers);
      for (i = 0; i < pAllocateInfo->commandBufferCount; i++)
         pCommandBuffers[i] = VK_NULL_HANDLE;
   }

   return result;
}

void
dzn_cmd_pool::free_cmd_buffers(VkDevice device,
                               uint32_t commandBufferCount,
                               const VkCommandBuffer *pCommandBuffers)
{
   for (uint32_t i = 0; i < commandBufferCount; i++)
      dzn_cmd_buffer_factory::destroy(device, pCommandBuffers[i], &alloc);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_AllocateCommandBuffers(VkDevice device,
                           const VkCommandBufferAllocateInfo *pAllocateInfo,
                           VkCommandBuffer *pCommandBuffers)
{
   VK_FROM_HANDLE(dzn_cmd_pool, pool, pAllocateInfo->commandPool);

   return pool->allocate_cmd_buffers(device, pAllocateInfo, pCommandBuffers);
}

VKAPI_ATTR void VKAPI_CALL
dzn_FreeCommandBuffers(VkDevice device,
                       VkCommandPool commandPool,
                       uint32_t commandBufferCount,
                       const VkCommandBuffer *pCommandBuffers)
{
   VK_FROM_HANDLE(dzn_cmd_pool, pool, commandPool);

   pool->free_cmd_buffers(device, commandBufferCount, pCommandBuffers);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_ResetCommandBuffer(VkCommandBuffer commandBuffer,
                       VkCommandBufferResetFlags flags)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->reset();
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_BeginCommandBuffer(
    VkCommandBuffer commandBuffer,
    const VkCommandBufferBeginInfo *pBeginInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

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
   cmd_buffer->reset();

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
      VK_FROM_HANDLE(dzn_render_pass, pass,
                      pBeginInfo->pInheritanceInfo->renderPass);
      struct dzn_subpass *subpass =
         &pass->subpasses[pBeginInfo->pInheritanceInfo->subpass];
      VK_FROM_HANDLE(dzn_framebuffer, framebuffer,
                      pBeginInfo->pInheritanceInfo->framebuffer);

      cmd_buffer->state.pass = pass;
      cmd_buffer->state.subpass = subpass;
   }
#endif

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_EndCommandBuffer(VkCommandBuffer commandBuffer)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->close_batch();

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

VKAPI_ATTR void VKAPI_CALL
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
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   dzn_batch *batch = cmd_buffer->get_batch();

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
      VK_FROM_HANDLE(dzn_buffer, buf, pBufferMemoryBarriers[i].buffer);
      D3D12_RESOURCE_BARRIER barrier;

      /* UAV are used only for storage buffers, skip all other buffers. */
      if (!(buf->usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
         continue;

      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.UAV.pResource = buf->res.Get();
      batch->cmdlist->ResourceBarrier(1, &barrier);
   }

   for (uint32_t i = 0; i < imageMemoryBarrierCount; i++) {
      /* D3D12_RESOURCE_BARRIER_TYPE_TRANSITION */
      VK_FROM_HANDLE(dzn_image, image, pImageMemoryBarriers[i].image);
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
      barriers[0].Aliasing.pResourceAfter = image->res.Get();
      barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

      barriers[1].Transition.pResource = image->res.Get();
      assert(base_layer == 0 && layer_count == 1);
      barriers[1].Transition.Subresource = 0; // YOLO

      if (pImageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
          pImageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
         barriers[1].Transition.StateBefore = image->mem->initial_state;
      else
         barriers[1].Transition.StateBefore = dzn_get_states(pImageMemoryBarriers[i].oldLayout);

      barriers[1].Transition.StateAfter = dzn_get_states(pImageMemoryBarriers[i].newLayout);

      /* some layouts map to the states, and NOP-barriers are illegal */
      unsigned nbarriers = 1 + (barriers[1].Transition.StateBefore != barriers[1].Transition.StateAfter);
      batch->cmdlist->ResourceBarrier(nbarriers, barriers);
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyBuffer2KHR(VkCommandBuffer commandBuffer,
                      const VkCopyBufferInfo2KHR* pCopyBufferInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_buffer, src_buffer, pCopyBufferInfo->srcBuffer);
   VK_FROM_HANDLE(dzn_buffer, dst_buffer, pCopyBufferInfo->dstBuffer);

   dzn_batch *batch = cmd_buffer->get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

   for (int i = 0; i < pCopyBufferInfo->regionCount; i++) {
      const VkBufferCopy2KHR *region = &pCopyBufferInfo->pRegions[i];

      cmdlist->CopyBufferRegion(dst_buffer->res.Get(), region->dstOffset,
                                src_buffer->res.Get(), region->srcOffset,
                                region->size);
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer,
                             const VkCopyBufferToImageInfo2KHR *pCopyBufferToImageInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_buffer, src_buffer, pCopyBufferToImageInfo->srcBuffer);
   VK_FROM_HANDLE(dzn_image, dst_image, pCopyBufferToImageInfo->dstImage);

   D3D12_TEXTURE_COPY_LOCATION src_buf_loc = {
      .pResource = src_buffer->res.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
   };

   D3D12_TEXTURE_COPY_LOCATION dst_img_loc = {
      .pResource = dst_image->res.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
   };

   ID3D12Device *dev = cmd_buffer->device->dev.Get();

   dzn_batch *batch = cmd_buffer->get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

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

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer,
                             const VkCopyImageToBufferInfo2KHR *pCopyImageToBufferInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_image, src_image, pCopyImageToBufferInfo->srcImage);
   VK_FROM_HANDLE(dzn_buffer, dst_buffer, pCopyImageToBufferInfo->dstBuffer);

   D3D12_TEXTURE_COPY_LOCATION dst_buf_loc = {
      .pResource = dst_buffer->res.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
   };

   D3D12_TEXTURE_COPY_LOCATION src_img_loc = {
      .pResource = src_image->res.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
   };

   ID3D12Device *dev = cmd_buffer->device->dev.Get();
   dzn_batch *batch = cmd_buffer->get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

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
   loc->pResource = img->res.Get();
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

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyImage2KHR(VkCommandBuffer commandBuffer,
                     const VkCopyImageInfo2KHR *pCopyImageInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_image, src, pCopyImageInfo->srcImage);
   VK_FROM_HANDLE(dzn_image, dst, pCopyImageInfo->dstImage);

   ID3D12Device *dev = cmd_buffer->device->dev.Get();
   dzn_batch *batch = cmd_buffer->get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

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

VKAPI_ATTR void VKAPI_CALL
dzn_CmdClearColorImage(VkCommandBuffer commandBuffer,
                       VkImage image,
                       VkImageLayout imageLayout,
                       const VkClearColorValue *pColor,
                       uint32_t rangeCount,
                       const VkImageSubresourceRange *pRanges)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = cmd_buffer->get_batch();
   dzn_device *device = cmd_buffer->device;
   VK_FROM_HANDLE(dzn_image, img, image);
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
         d3d12_descriptor_pool_alloc_handle(cmd_buffer->rtv_pool.get(), &handle);
         device->dev->CreateRenderTargetView(img->res.Get(), &desc,
                                             handle.cpu_handle);
         batch->cmdlist->ClearRenderTargetView(handle.cpu_handle,
                                               pColor->float32, 0, NULL);
      }
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdBeginRenderPass2(VkCommandBuffer commandBuffer,
                        const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                        const VkSubpassBeginInfoKHR *pSubpassBeginInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_render_pass, pass, pRenderPassBeginInfo->renderPass);
   VK_FROM_HANDLE(dzn_framebuffer, framebuffer, pRenderPassBeginInfo->framebuffer);
   const struct dzn_subpass *subpass = &pass->subpasses[0];
   dzn_batch *batch = cmd_buffer->get_batch();

   cmd_buffer->state.framebuffer = framebuffer;
   cmd_buffer->state.pass = pass;
   cmd_buffer->state.subpass = 0;

   D3D12_CPU_DESCRIPTOR_HANDLE rt_handles[MAX_RTS] = { };
   D3D12_CPU_DESCRIPTOR_HANDLE zs_handle = { 0 };

   for (uint32_t i = 0; i < subpass->color_count; i++) {
      if (subpass->colors[i].idx == VK_ATTACHMENT_UNUSED) continue;

      if (!i)
         cmd_buffer->rt0 = framebuffer->attachments[subpass->colors[i].idx]->image->res.Get();
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

      barrier.Transition.pResource = image->res.Get();
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

VKAPI_ATTR void VKAPI_CALL
dzn_CmdEndRenderPass2(VkCommandBuffer commandBuffer,
                      const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = cmd_buffer->get_batch();

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

      barrier.Transition.pResource = image->res.Get();
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

VKAPI_ATTR void VKAPI_CALL
dzn_CmdBindPipeline(VkCommandBuffer commandBuffer,
                    VkPipelineBindPoint pipelineBindPoint,
                    VkPipeline _pipeline)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_pipeline, pipeline, _pipeline);

   cmd_buffer->state.bindpoint[pipelineBindPoint].pipeline = pipeline;
   cmd_buffer->state.bindpoint[pipelineBindPoint].dirty |= DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
   if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      const dzn_graphics_pipeline *gfx = (const dzn_graphics_pipeline *)pipeline;

      if (!gfx->vp.dynamic) {
         memcpy(cmd_buffer->state.viewports, gfx->vp.desc,
                gfx->vp.count * sizeof(cmd_buffer->state.viewports[0]));
         cmd_buffer->state.dirty |= DZN_CMD_DIRTY_VIEWPORTS;
      }

      if (!gfx->scissor.dynamic) {
         memcpy(cmd_buffer->state.scissors, gfx->scissor.desc,
                gfx->scissor.count * sizeof(cmd_buffer->state.scissors[0]));
         cmd_buffer->state.dirty |= DZN_CMD_DIRTY_SCISSORS;
      }

      for (uint32_t vb = 0; vb < gfx->vb.count; vb++)
         cmd_buffer->state.vb.views[vb].StrideInBytes = gfx->vb.strides[vb];

      if (gfx->vb.count > 0)
         BITSET_SET_RANGE(cmd_buffer->state.vb.dirty, 0, gfx->vb.count - 1);
   }
}

void
dzn_cmd_buffer::update_pipeline(uint32_t bindpoint)
{
   const dzn_pipeline *pipeline = state.bindpoint[bindpoint].pipeline;
   dzn_batch *batch = get_batch();

   if (!pipeline)
      return;

   if (state.bindpoint[bindpoint].dirty & DZN_CMD_BINDPOINT_DIRTY_PIPELINE) {
      if (bindpoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
         const dzn_graphics_pipeline *gfx =
            reinterpret_cast<const dzn_graphics_pipeline *>(pipeline);
         batch->cmdlist->SetGraphicsRootSignature(pipeline->layout->root.sig.Get());
         batch->cmdlist->IASetPrimitiveTopology(gfx->ia.topology);
      } else {
         batch->cmdlist->SetComputeRootSignature(pipeline->layout->root.sig.Get());
      }
   }

   if (state.pipeline != pipeline) {
      batch->cmdlist->SetPipelineState(pipeline->state.Get());
      state.pipeline = pipeline;
   }
}

void
dzn_cmd_buffer::update_heaps(uint32_t bindpoint)
{
   ComPtr<ID3D12DescriptorHeap> *new_heaps = state.bindpoint[bindpoint].heaps;
   const struct dzn_pipeline *pipeline = state.bindpoint[bindpoint].pipeline;
   uint32_t view_desc_sz =
      device->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   uint32_t sampler_desc_sz =
      device->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
   dzn_batch *batch = get_batch();

   if (!(state.bindpoint[bindpoint].dirty & DZN_CMD_BINDPOINT_DIRTY_HEAPS))
      goto set_heaps;

   for (uint32_t type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; type++) {
      if (new_heaps[type]) {
         heaps.push_back(new_heaps[type]);
         new_heaps[type] = ComPtr<ID3D12DescriptorHeap>(NULL);
      }

      uint32_t desc_count = state.pipeline->layout->desc_count[type];
      if (!desc_count)
         continue;

      D3D12_DESCRIPTOR_HEAP_DESC desc = {
         .Type = (D3D12_DESCRIPTOR_HEAP_TYPE)type,
         .NumDescriptors = desc_count,
         .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      };

      HRESULT ret =
         device->dev->CreateDescriptorHeap(&desc,
                                           IID_PPV_ARGS(&new_heaps[type]));
      assert(!FAILED(ret));

      D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = {
         .ptr = new_heaps[type]->GetCPUDescriptorHandleForHeapStart().ptr,
      };

      for (uint32_t s = 0; s < MAX_SETS; s++) {
         const struct dzn_descriptor_set *set =
            state.bindpoint[bindpoint].sets[s];

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

         device->dev->CopyDescriptorsSimple(set_desc_count,
                                            dst_handle,
                                            src_handle,
                                            desc.Type);
         dst_handle.ptr += (desc_sz * set_desc_count);
      }
   }

set_heaps:
   if (new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Get() != state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Get() ||
       new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Get() != state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Get()) {
      ID3D12DescriptorHeap *desc_heaps[2];
      uint32_t num_desc_heaps = 0;
      if (new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Get())
         desc_heaps[num_desc_heaps++] = new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Get();
      if (new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Get())
         desc_heaps[num_desc_heaps++] = new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Get();
      batch->cmdlist->SetDescriptorHeaps(num_desc_heaps, desc_heaps);

      for (unsigned h = 0; h < ARRAY_SIZE(state.heaps); h++)
         state.heaps[h] = new_heaps[h];

      for (uint32_t r = 0; r < pipeline->layout->root.sets_param_count; r++) {
         D3D12_DESCRIPTOR_HEAP_TYPE type = pipeline->layout->root.type[r];
         D3D12_GPU_DESCRIPTOR_HANDLE handle = {
            .ptr = new_heaps[type]->GetGPUDescriptorHandleForHeapStart().ptr,
         };

         if (bindpoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
            batch->cmdlist->SetGraphicsRootDescriptorTable(r, handle);
         else
            batch->cmdlist->SetComputeRootDescriptorTable(r, handle);
      }
   }
}

void *
dzn_cmd_buffer::alloc_sysval_mem(uint32_t size, uint32_t align,
                                 D3D12_GPU_VIRTUAL_ADDRESS *gpu_ptr)
{
   uint32_t offset = ALIGN_POT(state.sysval_mem.offset, align);
   void *cpu_ptr;

   if (state.sysval_mem.offset + size > state.sysval_mem.size) {
      ComPtr<ID3D12Resource> res = state.sysval_mem.res;

      if (res.Get()) {
         res->Unmap(0, NULL);
         res = ComPtr<ID3D12Resource>(NULL);
      }

      /* Align size on 64k (the default alignment) */
      size = ALIGN_POT(size, 64 * 1024);

      D3D12_HEAP_PROPERTIES hprops =
         device->dev->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
      D3D12_RESOURCE_DESC rdesc = {
         .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
         .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
         .Width = size,
         .Height = 1,
         .DepthOrArraySize = 1,
         .MipLevels = 1,
         .Format = DXGI_FORMAT_UNKNOWN,
         .SampleDesc = { .Count = 1, .Quality = 0 },
         .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
         .Flags = D3D12_RESOURCE_FLAG_NONE,
      };

      HRESULT hres =
         device->dev->CreateCommittedResource(&hprops, D3D12_HEAP_FLAG_NONE, &rdesc,
			                      D3D12_RESOURCE_STATE_GENERIC_READ,
			                      NULL, IID_PPV_ARGS(&res));
      assert(!FAILED(hres));

      sysval_bufs.push_back(res);
      state.sysval_mem.res = res.Get();
      state.sysval_mem.offset = 0;
      state.sysval_mem.size = size;
      hres = res->Map(0, NULL, &state.sysval_mem.cpu);
      assert(!FAILED(hres));

      dzn_batch *batch = get_batch();

      /* Transition the buffer to CBV usage */
      D3D12_RESOURCE_BARRIER barrier = {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Transition = {
            .pResource = res.Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ,
            .StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
         },
      };

      batch->cmdlist->ResourceBarrier(1, &barrier);
   }

   *gpu_ptr = state.sysval_mem.res->GetGPUVirtualAddress() + state.sysval_mem.offset;
   cpu_ptr = (uint8_t *)state.sysval_mem.cpu + state.sysval_mem.offset;
   state.sysval_mem.offset += size;
   return cpu_ptr;
}

void
dzn_cmd_buffer::update_sysvals(uint32_t bindpoint)
{
   if (!(state.bindpoint[bindpoint].dirty & DZN_CMD_BINDPOINT_DIRTY_SYSVALS))
      return;

   const struct dzn_pipeline *pipeline = state.bindpoint[bindpoint].pipeline;
   uint32_t sysval_cbv_param_idx = pipeline->layout->root.sysval_cbv_param_idx;
   dzn_batch *batch = get_batch();
   D3D12_GPU_VIRTUAL_ADDRESS gpu_ptr = 0;

   if (bindpoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      void *sysvals =
         alloc_sysval_mem(sizeof(state.sysvals.gfx), 256, &gpu_ptr);

      memcpy(sysvals, &state.sysvals.gfx, sizeof(state.sysvals.gfx));
      batch->cmdlist->SetGraphicsRootConstantBufferView(sysval_cbv_param_idx,
                                                        gpu_ptr);
   } else {
      void *sysvals =
         alloc_sysval_mem(sizeof(state.sysvals.compute), 256, &gpu_ptr);

      memcpy(sysvals, &state.sysvals.compute, sizeof(state.sysvals.compute));
      batch->cmdlist->SetComputeRootConstantBufferView(sysval_cbv_param_idx,
                                                       gpu_ptr);
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                          VkPipelineBindPoint pipelineBindPoint,
                          VkPipelineLayout _layout,
                          uint32_t firstSet,
                          uint32_t descriptorSetCount,
                          const VkDescriptorSet *pDescriptorSets,
                          uint32_t dynamicOffsetCount,
                          const uint32_t *pDynamicOffsets)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_pipeline_layout, layout, _layout);

   for (uint32_t i = 0; i < descriptorSetCount; i++) {
      VK_FROM_HANDLE(dzn_descriptor_set, set, pDescriptorSets[i]);
      cmd_buffer->state.bindpoint[pipelineBindPoint].sets[firstSet + i] = set;
   }

   cmd_buffer->state.bindpoint[pipelineBindPoint].dirty |= DZN_CMD_BINDPOINT_DIRTY_HEAPS;
}

void
dzn_cmd_buffer::update_viewports()
{
   const dzn_graphics_pipeline *pipeline =
      reinterpret_cast<const dzn_graphics_pipeline *>(state.pipeline);
   dzn_batch *batch = get_batch();

   if (!(state.dirty & DZN_CMD_DIRTY_VIEWPORTS) ||
       !pipeline->vp.count)
      return;

   batch->cmdlist->RSSetViewports(pipeline->vp.count, state.viewports);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdSetViewport(VkCommandBuffer commandBuffer,
                   uint32_t firstViewport,
                   uint32_t viewportCount,
                   const VkViewport *pViewports)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   for (uint32_t i = 0; i < viewportCount; i++)
      dzn_translate_viewport(&cmd_buffer->state.viewports[firstViewport + i], &pViewports[i]);

   if (viewportCount)
      cmd_buffer->state.dirty |= DZN_CMD_DIRTY_VIEWPORTS;
}

void
dzn_cmd_buffer::update_scissors()
{
   const dzn_graphics_pipeline *pipeline =
      reinterpret_cast<const dzn_graphics_pipeline *>(state.pipeline);
   dzn_batch *batch = get_batch();

   if (!(state.dirty & DZN_CMD_DIRTY_SCISSORS) || !pipeline->scissor.count)
      return;

   batch->cmdlist->RSSetScissorRects(pipeline->scissor.count, state.scissors);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdSetScissor(VkCommandBuffer commandBuffer,
                  uint32_t firstScissor,
                  uint32_t scissorCount,
                  const VkRect2D *pScissors)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   for (uint32_t i = 0; i < scissorCount; i++)
      dzn_translate_scissor(&cmd_buffer->state.scissors[i + firstScissor], &pScissors[i]);

   if (scissorCount)
      cmd_buffer->state.dirty |= DZN_CMD_DIRTY_SCISSORS;
}

void
dzn_cmd_buffer::update_vbviews()
{
   const dzn_graphics_pipeline *pipeline =
      reinterpret_cast<const dzn_graphics_pipeline *>(state.pipeline);
   dzn_batch *batch = get_batch();
   unsigned start, end;

   BITSET_FOREACH_RANGE(start, end, state.vb.dirty, MAX_VBS)
      batch->cmdlist->IASetVertexBuffers(start, end - start, state.vb.views);

   BITSET_CLEAR_RANGE(state.vb.dirty, 0, MAX_VBS);
}

void
dzn_cmd_buffer::update_ibview()
{
   if (!(state.dirty & DZN_CMD_DIRTY_IB))
      return;

   dzn_batch *batch = get_batch();

   batch->cmdlist->IASetIndexBuffer(&state.ib.view);
}

void
dzn_cmd_buffer::update_push_constants(uint32_t bindpoint)
{
   assert(bindpoint == VK_PIPELINE_BIND_POINT_GRAPHICS);

   dzn_batch *batch = get_batch();

   if (!(state.push_constant.stages & VK_SHADER_STAGE_ALL_GRAPHICS))
      return;

   uint32_t slot = state.pipeline->layout->root.push_constant_cbv_param_idx;
   uint32_t offset = state.push_constant.offset / 4;
   uint32_t end = ALIGN(state.push_constant.end, 4) / 4;

   batch->cmdlist->SetGraphicsRoot32BitConstants(slot, end - offset,
      state.push_constant.values + offset, offset);
   state.push_constant.stages = 0;
   state.push_constant.offset = 0;
   state.push_constant.end = 0;
}

void
dzn_CmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
                     VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size,
                     const void *pValues)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   memcpy(((char *)cmd_buffer->state.push_constant.values) + offset, pValues, size);
   cmd_buffer->state.push_constant.stages |= stageFlags;

   uint32_t current_offset = cmd_buffer->state.push_constant.offset;
   uint32_t current_end = cmd_buffer->state.push_constant.end;
   uint32_t end = offset + size;
   if (current_end != 0) {
      offset = MIN2(current_offset, offset);
      end = MAX2(current_end, end);
   }
   cmd_buffer->state.push_constant.offset = offset;
   cmd_buffer->state.push_constant.end = end;
}

void
dzn_cmd_buffer::prepare_draw(bool indexed)
{
   update_pipeline(VK_PIPELINE_BIND_POINT_GRAPHICS);
   update_heaps(VK_PIPELINE_BIND_POINT_GRAPHICS);
   update_sysvals(VK_PIPELINE_BIND_POINT_GRAPHICS);
   update_viewports();
   update_scissors();
   update_vbviews();
   update_push_constants(VK_PIPELINE_BIND_POINT_GRAPHICS);

   if (indexed)
      update_ibview();

   /* Reset the dirty states */
   state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty = 0;
   state.dirty = 0;
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdDraw(VkCommandBuffer commandBuffer,
            uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = cmd_buffer->get_batch();

   if (!cmd_buffer->state.sysval_mem.res ||
       (cmd_buffer->state.sysvals.gfx.first_vertex != firstVertex ||
        cmd_buffer->state.sysvals.gfx.base_instance != firstInstance ||
        cmd_buffer->state.sysvals.gfx.is_indexed_draw != (instanceCount > 1))) {
      cmd_buffer->state.sysvals.gfx.first_vertex = firstVertex;
      cmd_buffer->state.sysvals.gfx.base_instance = firstInstance;
      cmd_buffer->state.sysvals.gfx.is_indexed_draw = instanceCount > 1;
      cmd_buffer->state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_SYSVALS;
   }

   cmd_buffer->prepare_draw(false);

   batch->cmdlist->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdDrawIndexed(VkCommandBuffer commandBuffer,
                   uint32_t indexCount,
                   uint32_t instanceCount,
                   uint32_t firstIndex,
                   int32_t vertexOffset,
                   uint32_t firstInstance)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = cmd_buffer->get_batch();

   if (cmd_buffer->state.sysvals.gfx.first_vertex != vertexOffset ||
       cmd_buffer->state.sysvals.gfx.base_instance != firstInstance ||
       cmd_buffer->state.sysvals.gfx.is_indexed_draw != (instanceCount > 1)) {
      cmd_buffer->state.sysvals.gfx.first_vertex = vertexOffset;
      cmd_buffer->state.sysvals.gfx.base_instance = firstInstance;
      cmd_buffer->state.sysvals.gfx.is_indexed_draw = instanceCount > 1;
      cmd_buffer->state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_SYSVALS;
   }

   cmd_buffer->prepare_draw(true);

   batch->cmdlist->DrawIndexedInstanced(indexCount, instanceCount, firstIndex,
                                        vertexOffset, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                         uint32_t firstBinding,
                         uint32_t bindingCount,
                         const VkBuffer *pBuffers,
                         const VkDeviceSize *pOffsets)
{
   if (!bindingCount)
      return;

   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   D3D12_VERTEX_BUFFER_VIEW *vbviews = cmd_buffer->state.vb.views;

   for (uint32_t i = 0; i < bindingCount; i++) {
      VK_FROM_HANDLE(dzn_buffer, buf, pBuffers[i]);

      vbviews[firstBinding + i].BufferLocation = buf->res->GetGPUVirtualAddress() + pOffsets[i];
      vbviews[firstBinding + i].SizeInBytes = buf->size - pOffsets[i];
   }

   BITSET_SET_RANGE(cmd_buffer->state.vb.dirty, firstBinding,
                    firstBinding + bindingCount - 1);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdBindIndexBuffer(VkCommandBuffer commandBuffer,
                       VkBuffer buffer,
                       VkDeviceSize offset,
                       VkIndexType indexType)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_buffer, buf, buffer);

   cmd_buffer->state.ib.view.BufferLocation = buf->res->GetGPUVirtualAddress() + offset;
   cmd_buffer->state.ib.view.SizeInBytes = buf->size - offset;
   switch (indexType) {
   case VK_INDEX_TYPE_UINT16:
      cmd_buffer->state.ib.view.Format = DXGI_FORMAT_R16_UINT;
      break;
   case VK_INDEX_TYPE_UINT32:
      cmd_buffer->state.ib.view.Format = DXGI_FORMAT_R32_UINT;
      break;
   case VK_INDEX_TYPE_UINT8_EXT:
      cmd_buffer->state.ib.view.Format = DXGI_FORMAT_R8_UINT;
      break;
   default: unreachable("Invalid index type");
   }

   cmd_buffer->state.dirty |= DZN_CMD_DIRTY_IB;
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdResetEvent(VkCommandBuffer commandBuffer,
                  VkEvent _event,
                  VkPipelineStageFlags stageMask)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_event, event, _event);

   struct dzn_cmd_event_signal signal = {
      .event = event,
      .value = false,
   };

   dzn_batch *batch = cmd_buffer->get_batch(true);

   batch->signal.push_back(signal);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdSetEvent(VkCommandBuffer commandBuffer,
                VkEvent _event,
                VkPipelineStageFlags stageMask)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_event, event, _event);

   struct dzn_cmd_event_signal signal = {
      .event = event,
      .value = true,
   };

   dzn_batch *batch = cmd_buffer->get_batch(true);

   batch->signal.push_back(signal);
}

VKAPI_ATTR void VKAPI_CALL
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
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   dzn_batch *batch = cmd_buffer->get_batch();

   for (uint32_t i = 0; i < eventCount; i++) {
      VK_FROM_HANDLE(dzn_event, event, pEvents[i]);

      batch->wait.push_back(event);
   }
}

void
dzn_cmd_buffer::prepare_dispatch()
{
   update_pipeline(VK_PIPELINE_BIND_POINT_COMPUTE);
   update_heaps(VK_PIPELINE_BIND_POINT_COMPUTE);
   update_sysvals(VK_PIPELINE_BIND_POINT_COMPUTE);

   /* Reset the dirty states */
   state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].dirty = 0;
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdDispatch(VkCommandBuffer commandBuffer,
                uint32_t groupCountX,
                uint32_t groupCountY,
                uint32_t groupCountZ)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   dzn_batch *batch = cmd_buffer->get_batch();

   if (groupCountX != cmd_buffer->state.sysvals.compute.group_count_x ||
       groupCountY != cmd_buffer->state.sysvals.compute.group_count_y ||
       groupCountZ != cmd_buffer->state.sysvals.compute.group_count_z) {
      cmd_buffer->state.sysvals.compute.group_count_x = groupCountX;
      cmd_buffer->state.sysvals.compute.group_count_y = groupCountY;
      cmd_buffer->state.sysvals.compute.group_count_z = groupCountZ;
      cmd_buffer->state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_SYSVALS;
   }

   cmd_buffer->prepare_dispatch();
   batch->cmdlist->Dispatch(groupCountX, groupCountY, groupCountZ);
}
