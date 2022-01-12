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
   flags(pCreateInfo->flags),
   bufs(bufs_allocator(pAllocator ? pAllocator : &device->vk.alloc, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
{
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_COMMAND_POOL);
   alloc = pAllocator ? *pAllocator : device->vk.alloc;
}

dzn_cmd_pool::~dzn_cmd_pool()
{
   vk_object_base_finish(&base);
}

VkResult
dzn_cmd_pool::allocate_cmd_buffers(dzn_device *device,
                                   const VkCommandBufferAllocateInfo *pAllocateInfo,
                                   VkCommandBuffer *pCommandBuffers)
{
   VkResult result = VK_SUCCESS;
   uint32_t i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {
      dzn_cmd_buffer *cmd_buffer;
      result = dzn_cmd_buffer_factory::create(device, this,
                                              pAllocateInfo->level,
                                              &alloc,
                                              &cmd_buffer);
      if (result != VK_SUCCESS)
         break;

      cmd_buffer->index = bufs.size();
      dzn_object_unique_ptr<dzn_cmd_buffer> cmd_buf_ptr(cmd_buffer);

      try {
         bufs.resize(bufs.size() + 1);
      } catch (VkResult error) {
         result = error;
         break;
      }

      bufs[cmd_buffer->index].swap(cmd_buf_ptr);
      pCommandBuffers[i] = dzn_cmd_buffer_to_handle(cmd_buffer);
   }

   if (result != VK_SUCCESS) {
      dzn_cmd_pool::free_cmd_buffers(device, i, pCommandBuffers);
      for (i = 0; i < pAllocateInfo->commandBufferCount; i++)
         pCommandBuffers[i] = VK_NULL_HANDLE;
   }

   return result;
}

void
dzn_cmd_pool::free_cmd_buffers(dzn_device *device,
                               uint32_t commandBufferCount,
                               const VkCommandBuffer *pCommandBuffers)
{
   // TODO: keep resources around
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      VK_FROM_HANDLE(dzn_cmd_buffer, buf, pCommandBuffers[i]);

      if (buf) {
         assert(bufs.size() > 0);

         if (buf->index != bufs.size() - 1) {
            auto &tmp_buf = bufs[buf->index];
            tmp_buf.swap(bufs[bufs.size() - 1]);
	    tmp_buf->index = buf->index;
	 }

         bufs.pop_back();
      }
   }
}

VkResult
dzn_cmd_pool::reset(dzn_device *device)
{
   // TODO: keep resources around
   for (auto &buf : bufs)
      buf->reset();

   return VK_SUCCESS;
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

VKAPI_ATTR VkResult VKAPI_CALL
dzn_ResetCommandPool(VkDevice device,
                     VkCommandPool commandPool,
                     VkCommandPoolResetFlags flags)
{
   VK_FROM_HANDLE(dzn_cmd_pool, pool, commandPool);
   VK_FROM_HANDLE(dzn_device, dev, device);

   return pool->reset(dev);
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
                              internal_bufs(bufs_allocator(pAllocator ? pAllocator : &cmd_pool->alloc)),
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
      d3d12_descriptor_pool_new(device->dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);

   rtv_pool = std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter>(pool);

   pool = d3d12_descriptor_pool_new(device->dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16);
   dsv_pool = std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter>(pool);

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
   vk_command_buffer_finish(&vk);
}

const VkAllocationCallbacks *
dzn_cmd_buffer::get_vk_allocator()
{
   return &pool->alloc;
}

void
dzn_cmd_buffer::close_batch()
{
   if (!batch.get())
      return;

   batch->cmdlist->Close();

   batches.resize(batches.size() + 1);
   batches[batches.size() - 1].swap(batch);
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

      /* We need to make sure the current state is re-applied on the new
       * cmdlist, so mark things as dirty.
       */
      const dzn_graphics_pipeline * gfx_pipeline =
         reinterpret_cast<const dzn_graphics_pipeline *>(state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].pipeline);

      if (gfx_pipeline) {
         if (gfx_pipeline->vp.count)
            state.dirty |= DZN_CMD_DIRTY_VIEWPORTS;
         if (gfx_pipeline->scissor.count)
            state.dirty |= DZN_CMD_DIRTY_SCISSORS;

         state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
            DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
      }

      if (state.ib.view.SizeInBytes)
         state.dirty |= DZN_CMD_DIRTY_IB;

      const dzn_pipeline *compute_pipeline =
         state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].pipeline;

      if (compute_pipeline) {
         state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].dirty |=
            DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
      }

      state.pipeline = NULL;
   }

   open_batch();
   assert(batch.get());
   return batch.get();
}

void
dzn_cmd_buffer::reset()
{
   /* TODO: Return heaps to the command pool instead of freeing them */
   struct d3d12_descriptor_pool *new_rtv_pool =
      d3d12_descriptor_pool_new(device->dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);
   struct d3d12_descriptor_pool *new_dsv_pool =
      d3d12_descriptor_pool_new(device->dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16);

   rtv_pool.reset(new_rtv_pool);
   dsv_pool.reset(new_dsv_pool);

   /* TODO: Return batches to the pool instead of freeing them. */
   batches.clear();
   batch.reset(NULL);

   internal_bufs.clear();

   /* Reset the state */
   memset(&state, 0, sizeof(state));

   vk_command_buffer_reset(&vk);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_AllocateCommandBuffers(VkDevice device,
                           const VkCommandBufferAllocateInfo *pAllocateInfo,
                           VkCommandBuffer *pCommandBuffers)
{
   VK_FROM_HANDLE(dzn_cmd_pool, pool, pAllocateInfo->commandPool);
   VK_FROM_HANDLE(dzn_device, dev, device);

   return pool->allocate_cmd_buffers(dev, pAllocateInfo, pCommandBuffers);
}

VKAPI_ATTR void VKAPI_CALL
dzn_FreeCommandBuffers(VkDevice device,
                       VkCommandPool commandPool,
                       uint32_t commandBufferCount,
                       const VkCommandBuffer *pCommandBuffers)
{
   VK_FROM_HANDLE(dzn_cmd_pool, pool, commandPool);
   VK_FROM_HANDLE(dzn_device, dev, device);

   pool->free_cmd_buffers(dev, commandBufferCount, pCommandBuffers);
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
      D3D12_RESOURCE_BARRIER barriers[2] = {};

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
      D3D12_RESOURCE_BARRIER barrier = {};

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

      /* We use placed resource's simple model, in which only one resource
       * pointing to a given heap is active at a given time. To make the
       * resource active we need to add an aliasing barrier.
       */
      D3D12_RESOURCE_BARRIER aliasing_barrier = {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Aliasing = {
            .pResourceBefore = NULL,
            .pResourceAfter = image->res.Get(),
         },
      };

      batch->cmdlist->ResourceBarrier(1, &aliasing_barrier);

      D3D12_RESOURCE_BARRIER transition_barrier = {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Transition = {
            .pResource = image->res.Get(),
            .StateAfter = dzn_image::get_state(pImageMemoryBarriers[i].newLayout),
         },
      };

      if (pImageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
          pImageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
         transition_barrier.Transition.StateBefore = image->mem->initial_state;
      else
         transition_barrier.Transition.StateBefore = dzn_image::get_state(pImageMemoryBarriers[i].oldLayout);

      if (transition_barrier.Transition.StateBefore == transition_barrier.Transition.StateAfter)
         continue;

      /* some layouts map to the same states, and NOP-barriers are illegal */
      uint32_t layer_count = dzn_get_layer_count(image, range);
      uint32_t level_count = dzn_get_level_count(image, range);
      for (uint32_t layer = 0; layer < layer_count; layer++) {
         for (uint32_t lvl = 0; lvl < level_count; lvl++) {
            dzn_foreach_aspect(aspect, range->aspectMask) {
               transition_barrier.Transition.Subresource =
                  image->get_subresource_index(*range, aspect, lvl, layer);
               batch->cmdlist->ResourceBarrier(1, &transition_barrier);
            }
         }
      }
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyBuffer2KHR(VkCommandBuffer commandBuffer,
                      const VkCopyBufferInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->copy(info);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer,
                             const VkCopyBufferToImageInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->copy(info);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer,
                             const VkCopyImageToBufferInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->copy(info);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdCopyImage2KHR(VkCommandBuffer commandBuffer,
                     const VkCopyImageInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->copy(info);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdBlitImage2KHR(VkCommandBuffer commandBuffer,
                     const VkBlitImageInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->blit(info);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdResolveImage2KHR(VkCommandBuffer commandBuffer,
                        const VkResolveImageInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->resolve(info);
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
   VK_FROM_HANDLE(dzn_image, img, image);

   cmd_buffer->clear(img, imageLayout, pColor, rangeCount, pRanges);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdClearDepthStencilImage(VkCommandBuffer commandBuffer,
                              VkImage image,
                              VkImageLayout imageLayout,
                              const VkClearDepthStencilValue *pDepthStencil,
                              uint32_t rangeCount,
                              const VkImageSubresourceRange *pRanges)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_image, img, image);

   cmd_buffer->clear(img, imageLayout, pDepthStencil, rangeCount, pRanges);
}

void
dzn_cmd_buffer::clear_attachment(uint32_t idx,
                                 const VkClearValue *pClearValue,
                                 VkImageAspectFlags aspectMask,
                                 uint32_t rectCount,
                                 D3D12_RECT *rects)
{
   if (idx == VK_ATTACHMENT_UNUSED)
      return;

   dzn_image_view *view = state.framebuffer->attachments[idx];
   dzn_batch *batch = get_batch();

   if (vk_format_is_depth_or_stencil(view->vk.format)) {
      D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;

      if (aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
         flags |= D3D12_CLEAR_FLAG_DEPTH;
      if (aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
         flags |= D3D12_CLEAR_FLAG_STENCIL;

      if (flags != 0)
         batch->cmdlist->ClearDepthStencilView(view->zs_handle.cpu_handle,
                                                flags,
                                                pClearValue->depthStencil.depth,
                                                pClearValue->depthStencil.stencil,
                                                rectCount, rects);
   } else if (aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      batch->cmdlist->ClearRenderTargetView(view->rt_handle.cpu_handle,
                                             pClearValue->color.float32,
                                             rectCount, rects);
   }
}

void
dzn_cmd_buffer::clear(const dzn_image *image,
                      VkImageLayout layout,
                      const VkClearColorValue *color,
                      uint32_t range_count,
                      const VkImageSubresourceRange *ranges)
{
   dzn_batch *batch = get_batch();

   assert(image->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

   float clear_vals[4];

   enum pipe_format pfmt = vk_format_to_pipe_format(image->vk.format);

   /* FIXME: precision loss for all 32-bit integer formats */
   if (util_format_is_pure_sint(pfmt)) {
      for (uint32_t c = 0; c < ARRAY_SIZE(clear_vals); c++)
         clear_vals[c] = color->int32[c];
   } else if (util_format_is_pure_uint(pfmt)) {
      for (uint32_t c = 0; c < ARRAY_SIZE(clear_vals); c++)
         clear_vals[c] = color->uint32[c];
   } else {
      memcpy(clear_vals, color->float32, sizeof(clear_vals));
   }

   for (uint32_t r = 0; r < range_count; r++) {
      const VkImageSubresourceRange &range = ranges[r];
      uint32_t layer_count = dzn_get_layer_count(image, &range);
      uint32_t level_count = dzn_get_level_count(image, &range);

      for (uint32_t lvl = 0; lvl < level_count; lvl++) {
         struct d3d12_descriptor_handle handle;

         D3D12_RESOURCE_BARRIER barrier = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
               .pResource = image->res.Get(),
               .StateBefore = dzn_image::get_state(layout),
               .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
            },
         };

         if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
            for (uint32_t layer = 0; layer < layer_count; layer++) {
               barrier.Transition.Subresource =
                  image->get_subresource_index(range, VK_IMAGE_ASPECT_COLOR_BIT,
                                               lvl, layer);
               batch->cmdlist->ResourceBarrier(1, &barrier);
            }
         }

         d3d12_descriptor_pool_alloc_handle(rtv_pool.get(), &handle);
         image->create_rtv(device, range, lvl, handle.cpu_handle);
         batch->cmdlist->ClearRenderTargetView(handle.cpu_handle,
                                               clear_vals, 0, NULL);

         if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

            for (uint32_t layer = 0; layer < layer_count; layer++) {
               barrier.Transition.Subresource =
                  image->get_subresource_index(range, VK_IMAGE_ASPECT_COLOR_BIT, lvl, layer);
               batch->cmdlist->ResourceBarrier(1, &barrier);
            }
         }
      }
   }
}

void
dzn_cmd_buffer::clear(const dzn_image *image,
                      VkImageLayout layout,
                      const VkClearDepthStencilValue *zs,
                      uint32_t range_count,
                      const VkImageSubresourceRange *ranges)
{
   dzn_batch *batch = get_batch();

   assert(image->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

   for (uint32_t r = 0; r < range_count; r++) {
      const VkImageSubresourceRange &range = ranges[r];
      uint32_t layer_count = dzn_get_layer_count(image, &range);
      uint32_t level_count = dzn_get_level_count(image, &range);

      D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;

      if (range.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
         flags |= D3D12_CLEAR_FLAG_DEPTH;
      if (range.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
         flags |= D3D12_CLEAR_FLAG_STENCIL;

      for (uint32_t lvl = 0; lvl < level_count; lvl++) {
         struct d3d12_descriptor_handle handle;

         D3D12_RESOURCE_BARRIER barrier = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
               .pResource = image->res.Get(),
               .StateBefore = dzn_image::get_state(layout),
               .StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE,
            },
         };

         if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
            for (uint32_t layer = 0; layer < layer_count; layer++) {
               dzn_foreach_aspect(aspect, range.aspectMask) {
                  barrier.Transition.Subresource =
                     image->get_subresource_index(range, aspect, lvl, layer);
                  batch->cmdlist->ResourceBarrier(1, &barrier);
               }
            }
         }

         d3d12_descriptor_pool_alloc_handle(dsv_pool.get(), &handle);
         image->create_dsv(device, range, lvl, handle.cpu_handle);
         batch->cmdlist->ClearDepthStencilView(handle.cpu_handle, flags,
                                               zs->depth, zs->stencil,
                                               0, NULL);

         if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

            for (uint32_t layer = 0; layer < layer_count; layer++) {
               dzn_foreach_aspect(aspect, range.aspectMask) {
                  barrier.Transition.Subresource =
                     image->get_subresource_index(range, aspect, lvl, layer);
                  batch->cmdlist->ResourceBarrier(1, &barrier);
               }
            }
         }
      }
   }
}

void
dzn_cmd_buffer::copy(const VkCopyBufferInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_buffer, src_buffer, info->srcBuffer);
   VK_FROM_HANDLE(dzn_buffer, dst_buffer, info->dstBuffer);

   dzn_batch *batch = get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

   for (int i = 0; i < info->regionCount; i++) {
      auto &region = info->pRegions[i];

      cmdlist->CopyBufferRegion(dst_buffer->res.Get(), region.dstOffset,
                                src_buffer->res.Get(), region.srcOffset,
                                region.size);
   }
}

void
dzn_cmd_buffer::copy(const VkCopyBufferToImageInfo2KHR *info,
                     uint32_t r,
                     VkImageAspectFlagBits aspect,
                     uint32_t l)
{
   VK_FROM_HANDLE(dzn_buffer, src_buffer, info->srcBuffer);
   VK_FROM_HANDLE(dzn_image, dst_image, info->dstImage);

   ID3D12Device *dev = device->dev;

   dzn_batch *batch = get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

   const VkBufferImageCopy2KHR &region = info->pRegions[r];
   enum pipe_format pfmt = vk_format_to_pipe_format(dst_image->vk.format);
   uint32_t blkh = util_format_get_blockheight(pfmt);
   uint32_t blkd = util_format_get_blockdepth(pfmt);

   D3D12_TEXTURE_COPY_LOCATION dst_img_loc =
      dst_image->get_copy_loc(region.imageSubresource, aspect, l);
   D3D12_TEXTURE_COPY_LOCATION src_buf_loc =
      src_buffer->get_copy_loc(dst_image->vk.format, region, aspect, l);

   if (src_buffer->supports_region_copy(src_buf_loc)) {
      /* RowPitch and Offset are properly aligned, we can copy
       * the whole thing in one call.
       */
      D3D12_BOX src_box = {
         .left = 0,
         .top = 0,
         .front = 0,
         .right = region.imageExtent.width,
         .bottom = region.imageExtent.height,
         .back = region.imageExtent.depth,
      };

      cmdlist->CopyTextureRegion(&dst_img_loc, region.imageOffset.x,
                                 region.imageOffset.y, region.imageOffset.z,
                                 &src_buf_loc, &src_box);
      return;
   }

   /* Copy line-by-line if things are not properly aligned. */
   D3D12_BOX src_box = {
      .top = 0,
      .front = 0,
      .bottom = blkh,
      .back = blkd,
   };

   for (uint32_t z = 0; z < region.imageExtent.depth; z += blkd) {
      for (uint32_t y = 0; y < region.imageExtent.height; y += blkh) {
         uint32_t src_x;

         D3D12_TEXTURE_COPY_LOCATION src_buf_line_loc =
            src_buffer->get_line_copy_loc(dst_image->vk.format,
                                          region, src_buf_loc,
                                          y, z, src_x);

         src_box.left = src_x;
         src_box.right = src_x + region.imageExtent.width;
         cmdlist->CopyTextureRegion(&dst_img_loc,
                                    region.imageOffset.x,
                                    region.imageOffset.y + y,
                                    region.imageOffset.z + z,
                                    &src_buf_line_loc, &src_box);
      }
   }
}

void
dzn_cmd_buffer::copy(const VkCopyBufferToImageInfo2KHR *info)
{
   for (int i = 0; i < info->regionCount; i++) {
      const VkBufferImageCopy2KHR &region = info->pRegions[i];

      dzn_foreach_aspect(aspect, region.imageSubresource.aspectMask) {
         for (uint32_t l = 0; l < region.imageSubresource.layerCount; l++)
            copy(info, i, aspect, l);
      }
   }
}

void
dzn_cmd_buffer::copy(const VkCopyImageToBufferInfo2KHR *info,
                     uint32_t r,
                     VkImageAspectFlagBits aspect,
                     uint32_t l)
{
   VK_FROM_HANDLE(dzn_image, src_image, info->srcImage);
   VK_FROM_HANDLE(dzn_buffer, dst_buffer, info->dstBuffer);

   ID3D12Device *dev = device->dev;
   dzn_batch *batch = get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

   const VkBufferImageCopy2KHR &region = info->pRegions[r];
   enum pipe_format pfmt = vk_format_to_pipe_format(src_image->vk.format);
   uint32_t blkh = util_format_get_blockheight(pfmt);
   uint32_t blkd = util_format_get_blockdepth(pfmt);

   D3D12_TEXTURE_COPY_LOCATION src_img_loc =
      src_image->get_copy_loc(region.imageSubresource, aspect, l);
   D3D12_TEXTURE_COPY_LOCATION dst_buf_loc =
      dst_buffer->get_copy_loc(src_image->vk.format, region, aspect, l);

   if (dst_buffer->supports_region_copy(dst_buf_loc)) {
      /* RowPitch and Offset are properly aligned on 256 bytes, we can copy
       * the whole thing in one call.
       */
      D3D12_BOX src_box = {
         .left = (UINT)region.imageOffset.x,
         .top = (UINT)region.imageOffset.y,
         .front = (UINT)region.imageOffset.z,
         .right = (UINT)(region.imageOffset.x + region.imageExtent.width),
         .bottom = (UINT)(region.imageOffset.y + region.imageExtent.height),
         .back = (UINT)(region.imageOffset.z + region.imageExtent.depth),
      };

      cmdlist->CopyTextureRegion(&dst_buf_loc, 0, 0, 0,
                                 &src_img_loc, &src_box);
      return;
   }

   D3D12_BOX src_box = {
      .left = (UINT)region.imageOffset.x,
      .right = (UINT)(region.imageOffset.x + region.imageExtent.width),
   };

   /* Copy line-by-line if things are not properly aligned. */
   for (uint32_t z = 0; z < region.imageExtent.depth; z += blkd) {
      src_box.front = region.imageOffset.z + z;
      src_box.back = src_box.front + blkd;

      for (uint32_t y = 0; y < region.imageExtent.height; y += blkh) {
         uint32_t dst_x;

         D3D12_TEXTURE_COPY_LOCATION dst_buf_line_loc =
            dst_buffer->get_line_copy_loc(src_image->vk.format,
                                          region, dst_buf_loc,
                                          y, z, dst_x);

         src_box.top = region.imageOffset.y + y;
         src_box.bottom = src_box.top + blkh;

         cmdlist->CopyTextureRegion(&dst_buf_line_loc, dst_x, 0, 0,
                                    &src_img_loc, &src_box);
      }
   }
}

void
dzn_cmd_buffer::copy(const VkCopyImageToBufferInfo2KHR *info)
{
   for (int i = 0; i < info->regionCount; i++) {
      const VkBufferImageCopy2KHR &region = info->pRegions[i];

      dzn_foreach_aspect(aspect, region.imageSubresource.aspectMask) {
         for (uint32_t l = 0; l < region.imageSubresource.layerCount; l++)
            copy(info, i, aspect, l);
      }
   }
}

void
dzn_cmd_buffer::copy(const VkCopyImageInfo2KHR *info,
                     D3D12_RESOURCE_DESC &tmp_desc,
                     D3D12_TEXTURE_COPY_LOCATION &tmp_loc,
                     uint32_t r,
                     VkImageAspectFlagBits aspect,
                     uint32_t l)
{
   VK_FROM_HANDLE(dzn_image, src, info->srcImage);
   VK_FROM_HANDLE(dzn_image, dst, info->dstImage);

   ID3D12Device *dev = device->dev;
   dzn_batch *batch = get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

   const VkImageCopy2KHR &region = info->pRegions[r];
   const VkImageSubresourceLayers &src_subres = region.srcSubresource;
   const VkImageSubresourceLayers &dst_subres = region.dstSubresource;
   VkFormat src_format =
      dzn_image::get_plane_format(src->vk.format, aspect);
   VkFormat dst_format =
      dzn_image::get_plane_format(dst->vk.format, aspect);

   enum pipe_format src_pfmt = vk_format_to_pipe_format(src_format);
   uint32_t src_blkw = util_format_get_blockwidth(src_pfmt);
   uint32_t src_blkh = util_format_get_blockheight(src_pfmt);
   uint32_t src_blkd = util_format_get_blockdepth(src_pfmt);
   enum pipe_format dst_pfmt = vk_format_to_pipe_format(dst_format);
   uint32_t dst_blkw = util_format_get_blockwidth(dst_pfmt);
   uint32_t dst_blkh = util_format_get_blockheight(dst_pfmt);
   uint32_t dst_blkd = util_format_get_blockdepth(dst_pfmt);

   assert(src_subres.layerCount == dst_subres.layerCount);
   assert(src_subres.aspectMask == dst_subres.aspectMask);

   auto dst_loc = dst->get_copy_loc(dst_subres, aspect, l);
   auto src_loc = src->get_copy_loc(src_subres, aspect, l);

   D3D12_BOX src_box = {
      .left = (UINT)MAX2(region.srcOffset.x, 0),
      .top = (UINT)MAX2(region.srcOffset.y, 0),
      .front = (UINT)MAX2(region.srcOffset.z, 0),
      .right = (UINT)region.srcOffset.x + region.extent.width,
      .bottom = (UINT)region.srcOffset.y + region.extent.height,
      .back = (UINT)region.srcOffset.z + region.extent.depth,
   };

   if (!tmp_loc.pResource) {
      cmdlist->CopyTextureRegion(&dst_loc, region.dstOffset.x,
                                 region.dstOffset.y, region.dstOffset.z,
                                 &src_loc, &src_box);
      return;
   }

   tmp_desc.Format =
      dzn_image::get_placed_footprint_format(src->vk.format, aspect);
   tmp_desc.Width = region.extent.width;
   tmp_desc.Height = region.extent.height;

   dev->GetCopyableFootprints(&tmp_desc,
                              0, 1, 0,
                              &tmp_loc.PlacedFootprint,
                              NULL, NULL, NULL);

   tmp_loc.PlacedFootprint.Footprint.Depth = region.extent.depth;

   D3D12_RESOURCE_BARRIER barrier = {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
         .pResource = tmp_loc.pResource,
         .Subresource = 0,
         .StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
         .StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
      },
   };

   if (r > 0 || l > 0)
      batch->cmdlist->ResourceBarrier(1, &barrier);

   cmdlist->CopyTextureRegion(&tmp_loc, 0, 0, 0, &src_loc, &src_box);

   std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
   batch->cmdlist->ResourceBarrier(1, &barrier);

   tmp_desc.Format =
      dzn_image::get_placed_footprint_format(dst->vk.format, aspect);
   if (src_blkw != dst_blkw)
      tmp_desc.Width = DIV_ROUND_UP(region.extent.width, src_blkw) * dst_blkw;
   if (src_blkh != dst_blkh)
      tmp_desc.Height = DIV_ROUND_UP(region.extent.height, src_blkh) * dst_blkh;

   device->dev->GetCopyableFootprints(&tmp_desc,
                                      0, 1, 0,
                                      &tmp_loc.PlacedFootprint,
                                      NULL, NULL, NULL);

   if (src_blkd != dst_blkd) {
      tmp_loc.PlacedFootprint.Footprint.Depth =
      DIV_ROUND_UP(region.extent.depth, src_blkd) * dst_blkd;
   } else {
      tmp_loc.PlacedFootprint.Footprint.Depth = region.extent.depth;
   }

   D3D12_BOX tmp_box = {
      .left = 0,
      .top = 0,
      .front = 0,
      .right = tmp_loc.PlacedFootprint.Footprint.Width,
      .bottom = tmp_loc.PlacedFootprint.Footprint.Height,
      .back = tmp_loc.PlacedFootprint.Footprint.Depth,
   };

   cmdlist->CopyTextureRegion(&dst_loc,
                              region.dstOffset.x,
                              region.dstOffset.y,
                              region.dstOffset.z,
                              &tmp_loc, &tmp_box);
}

void
dzn_cmd_buffer::copy(const VkCopyImageInfo2KHR *info)
{
   VK_FROM_HANDLE(dzn_image, src, info->srcImage);
   VK_FROM_HANDLE(dzn_image, dst, info->dstImage);

   assert(src->vk.samples == dst->vk.samples);

   bool requires_temp_res = src->vk.format != dst->vk.format &&
                            src->vk.tiling != VK_IMAGE_TILING_LINEAR &&
                            dst->vk.tiling != VK_IMAGE_TILING_LINEAR;

   /* FIXME: multisample copies only work if we copy the entire subresource
    * and if the the copy doesn't require a temporary linear resource. When
    * these conditions are not met we should use a blit shader.
    */
   if (src->vk.samples > 1) {
      assert(requires_temp_res == false);

      for (uint32_t i = 0; i < info->regionCount; i++) {
         const VkImageCopy2KHR &region = info->pRegions[i];
         uint32_t src_w = u_minify(src->vk.extent.width, region.srcSubresource.mipLevel);
         uint32_t src_h = u_minify(src->vk.extent.width, region.srcSubresource.mipLevel);

         assert(region.srcOffset.x == 0 && region.srcOffset.y == 0);
         assert(region.extent.width == u_minify(src->vk.extent.width, region.srcSubresource.mipLevel));
         assert(region.extent.height == u_minify(src->vk.extent.height, region.srcSubresource.mipLevel));
         assert(region.dstOffset.x == 0 && region.dstOffset.y == 0);
         assert(region.extent.width == u_minify(dst->vk.extent.width, region.dstSubresource.mipLevel));
         assert(region.extent.height == u_minify(dst->vk.extent.height, region.dstSubresource.mipLevel));
      }
   }

   D3D12_TEXTURE_COPY_LOCATION tmp_loc = {};
   D3D12_RESOURCE_DESC tmp_desc = {
      .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .Format = src->desc.Format,
      .SampleDesc = { .Count = 1, .Quality = 0 },
      .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
      .Flags = D3D12_RESOURCE_FLAG_NONE,
   };

   if (requires_temp_res) {
      ID3D12Device *dev = device->dev;
      VkImageAspectFlags aspect = 0;
      uint64_t max_size = 0;

      if (vk_format_has_depth(src->vk.format))
         aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
      else if (vk_format_has_stencil(src->vk.format))
         aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
      else
         aspect = VK_IMAGE_ASPECT_COLOR_BIT;

      for (uint32_t i = 0; i < info->regionCount; i++) {
         const VkImageCopy2KHR &region = info->pRegions[i];
         uint64_t region_size = 0;

         tmp_desc.Format =
            dzn_image::get_dxgi_format(src->vk.format,
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                       aspect);
         tmp_desc.Width = region.extent.width;
         tmp_desc.Height = region.extent.height;

         dev->GetCopyableFootprints(&src->desc,
                                    0, 1, 0,
                                    NULL, NULL, NULL,
                                    &region_size);
         max_size = MAX2(max_size, region_size * region.extent.depth);
      }

      tmp_loc.pResource =
         dzn_cmd_buffer::alloc_internal_buf(max_size,
                                            D3D12_HEAP_TYPE_DEFAULT,
                                            D3D12_RESOURCE_STATE_COPY_DEST);
      tmp_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   }

   for (int i = 0; i < info->regionCount; i++) {
      const VkImageCopy2KHR &region = info->pRegions[i];

      dzn_foreach_aspect(aspect, region.srcSubresource.aspectMask) {
         for (uint32_t l = 0; l < region.srcSubresource.layerCount; l++)
            copy(info, tmp_desc, tmp_loc, i, aspect, l);
      }
   }
}

void
dzn_cmd_buffer::copy(ID3D12Resource *src,
                     dzn_buffer *dst,
                     VkDeviceSize dst_offset,
                     VkDeviceSize size)
{
   D3D12_TEXTURE_COPY_LOCATION src_loc = {
      .pResource = src,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = {
         .Offset = 0,
         .Footprint = {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .Width = (UINT)(size / 4),
            .Height = 1,
            .Depth = 1,
            .RowPitch = (UINT)ALIGN(size, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT),
         },
      },
   };

   D3D12_BOX src_box = { 0, 0, 0, size / 4, 1, 1 };

   uint32_t dst_x = (dst_offset & (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1)) / 4;

   D3D12_TEXTURE_COPY_LOCATION dst_loc = {
      .pResource = dst->res.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = {
         .Offset = dst_offset & ~(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1),
         .Footprint = {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .Width = (UINT)(dst_x + (size / 4)),
            .Height = 1,
            .Depth = 1,
            .RowPitch = (UINT)ALIGN((dst_x * 4) + size, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT),
         },
      },
   };

   dzn_batch *batch = get_batch();
   ID3D12GraphicsCommandList *cmdlist = batch->cmdlist.Get();

   D3D12_RESOURCE_BARRIER barriers[] = {
      {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Transition = {
            .pResource = src,
            .StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ,
            .StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
         },
      },
   };

   cmdlist->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
   cmdlist->CopyTextureRegion(&dst_loc, dst_x, 0, 0,
                              &src_loc, &src_box);
}

void
dzn_cmd_buffer::fill(dzn_buffer *buf,
                     VkDeviceSize offset,
                     VkDeviceSize size,
                     uint32_t data)
{
   if (size == VK_WHOLE_SIZE)
      size = buf->size - offset;

   size &= ~3ULL;

   auto src_res = alloc_internal_buf(size,
                                     D3D12_HEAP_TYPE_UPLOAD,
                                     D3D12_RESOURCE_STATE_GENERIC_READ);
   uint32_t *cpu_ptr;
   src_res->Map(0, NULL, (void **)&cpu_ptr);
   for (uint32_t i = 0; i < size / 4; i++)
      cpu_ptr[i] = data;

   src_res->Unmap(0, NULL);

   copy(src_res, buf, offset, size);
}

void
dzn_cmd_buffer::update(dzn_buffer *buf,
                       VkDeviceSize offset,
                       VkDeviceSize size,
                       const void *data)
{
   if (size == VK_WHOLE_SIZE)
      size = buf->size - offset;

   /*
    * The spec says:
    *  "size is the number of bytes to fill, and must be either a multiple of
    *   4, or VK_WHOLE_SIZE to fill the range from offset to the end of the
    *   buffer. If VK_WHOLE_SIZE is used and the remaining size of the buffer
    *   is not a multiple of 4, then the nearest smaller multiple is used."
    */
   size &= ~3ULL;

   auto src_res = alloc_internal_buf(size,
                                     D3D12_HEAP_TYPE_UPLOAD,
                                     D3D12_RESOURCE_STATE_GENERIC_READ);
   void *cpu_ptr;
   src_res->Map(0, NULL, &cpu_ptr);
   memcpy(cpu_ptr, data, size),
   src_res->Unmap(0, NULL);

   copy(src_res, buf, offset, size);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdFillBuffer(VkCommandBuffer commandBuffer,
                  VkBuffer dstBuffer,
                  VkDeviceSize dstOffset,
                  VkDeviceSize size,
                  uint32_t data)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_buffer, buffer, dstBuffer);

   cmd_buffer->fill(buffer, dstOffset, size, data);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdUpdateBuffer(VkCommandBuffer commandBuffer,
                    VkBuffer dstBuffer,
                    VkDeviceSize dstOffset,
                    VkDeviceSize size,
                    const void *data)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_buffer, buffer, dstBuffer);

   cmd_buffer->update(buffer, dstOffset, size, data);
}

void
dzn_cmd_buffer::blit_prepare_src_view(VkImage image,
                                      VkImageAspectFlagBits aspect,
                                      const VkImageSubresourceLayers &subres,
                                      dzn_descriptor_heap &heap,
                                      uint32_t heap_offset)
{
   VK_FROM_HANDLE(dzn_image, img, image);
   VkImageViewCreateInfo iview_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .format = img->vk.format,
      .subresourceRange = {
         .aspectMask = (VkImageAspectFlags)aspect,
         .baseMipLevel = subres.mipLevel,
         .levelCount = 1,
         .baseArrayLayer = subres.baseArrayLayer,
         .layerCount = subres.layerCount,
      },
   };

   if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT) {
      iview_info.components.r = VK_COMPONENT_SWIZZLE_G;
      iview_info.components.g = VK_COMPONENT_SWIZZLE_G;
      iview_info.components.b = VK_COMPONENT_SWIZZLE_G;
      iview_info.components.a = VK_COMPONENT_SWIZZLE_G;
   } else if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT) {
      iview_info.components.r = VK_COMPONENT_SWIZZLE_R;
      iview_info.components.g = VK_COMPONENT_SWIZZLE_R;
      iview_info.components.b = VK_COMPONENT_SWIZZLE_R;
      iview_info.components.a = VK_COMPONENT_SWIZZLE_R;
   }

   switch (img->vk.image_type) {
   case VK_IMAGE_TYPE_1D:
      iview_info.viewType = img->vk.array_layers > 1 ?
                            VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
      break;
   case VK_IMAGE_TYPE_2D:
      iview_info.viewType = img->vk.array_layers > 1 ?
                            VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
      break;
   case VK_IMAGE_TYPE_3D:
      iview_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
      break;
   default:
      unreachable("Invalid type");
   }

   dzn_image_view iview(device, &iview_info, NULL);
   heap.write_desc(heap_offset, false, &iview);

   dzn_batch *batch = get_batch();
   batch->cmdlist->SetGraphicsRootDescriptorTable(0, heap.get_gpu_handle(heap_offset));
}

void
dzn_cmd_buffer::blit_prepare_dst_view(dzn_image *img,
                                      VkImageAspectFlagBits aspect,
                                      uint32_t level, uint32_t layer)
{
   bool ds = aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
   VkImageSubresourceRange range = {
      .aspectMask = (VkImageAspectFlags)aspect,
      .baseMipLevel = level,
      .levelCount = 1,
      .baseArrayLayer = layer,
      .layerCount = 1,
   };
   struct d3d12_descriptor_handle handle;
   dzn_batch *batch = get_batch();

   if (ds) {
      d3d12_descriptor_pool_alloc_handle(dsv_pool.get(), &handle);
      img->create_dsv(device, range, 0, handle.cpu_handle);
      batch->cmdlist->OMSetRenderTargets(0, NULL, TRUE, &handle.cpu_handle);
   } else {
      d3d12_descriptor_pool_alloc_handle(rtv_pool.get(), &handle);
      img->create_rtv(device, range, 0, handle.cpu_handle);
      batch->cmdlist->OMSetRenderTargets(1, &handle.cpu_handle, FALSE, NULL);
   }
}

void
dzn_cmd_buffer::blit_set_pipeline(const dzn_image *src,
                                  const dzn_image *dst,
                                  VkImageAspectFlagBits aspect,
                                  VkFilter filter, bool resolve)
{
   enum pipe_format pfmt = vk_format_to_pipe_format(dst->vk.format);
   VkImageUsageFlags usage =
      vk_format_is_depth_or_stencil(dst->vk.format) ?
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT :
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   struct dzn_meta_blit::key ctx_key = {
      .out_format = dzn_image::get_dxgi_format(dst->vk.format, usage, aspect),
      .samples = (uint32_t)src->vk.samples,
      .loc = (uint32_t)(aspect == VK_IMAGE_ASPECT_DEPTH_BIT ?
                        FRAG_RESULT_DEPTH :
                        aspect == VK_IMAGE_ASPECT_STENCIL_BIT ?
                        FRAG_RESULT_STENCIL :
                        FRAG_RESULT_DATA0),
      .out_type = (uint32_t)(util_format_is_pure_uint(pfmt) ? GLSL_TYPE_UINT :
                             util_format_is_pure_sint(pfmt) ? GLSL_TYPE_INT :
                             aspect == VK_IMAGE_ASPECT_STENCIL_BIT ? GLSL_TYPE_UINT :
                             GLSL_TYPE_FLOAT),
      .sampler_dim = (uint32_t)(src->vk.image_type == VK_IMAGE_TYPE_1D ? GLSL_SAMPLER_DIM_1D :
                                src->vk.image_type == VK_IMAGE_TYPE_2D && src->vk.samples == 1 ? GLSL_SAMPLER_DIM_2D :
                                src->vk.image_type == VK_IMAGE_TYPE_2D && src->vk.samples > 1 ? GLSL_SAMPLER_DIM_MS :
                                GLSL_SAMPLER_DIM_3D),
      .src_is_array = src->vk.array_layers > 1,
      .resolve = resolve,
      .linear_filter = filter == VK_FILTER_LINEAR,
   };

   const dzn_meta_blit *ctx = device->blits->get_context(ctx_key);
   assert(ctx);

   dzn_batch *batch = get_batch();

   batch->cmdlist->SetGraphicsRootSignature(ctx->root_sig.Get());
   batch->cmdlist->SetPipelineState(ctx->pipeline_state.Get());
}

void
dzn_cmd_buffer::blit_set_2d_region(const dzn_image *src,
                                   const VkImageSubresourceLayers &src_subres,
                                   const VkOffset3D *src_offsets,
                                   const dzn_image *dst,
                                   const VkImageSubresourceLayers &dst_subres,
                                   const VkOffset3D *dst_offsets,
                                   bool normalize_src_coords)
{
   uint32_t dst_w = u_minify(dst->vk.extent.width, dst_subres.mipLevel);
   uint32_t dst_h = u_minify(dst->vk.extent.height, dst_subres.mipLevel);
   uint32_t src_w = u_minify(src->vk.extent.width, src_subres.mipLevel);
   uint32_t src_h = u_minify(src->vk.extent.height, src_subres.mipLevel);

   float dst_pos[4] = {
      (2 * (float)dst_offsets[0].x / (float)dst_w) - 1.0f, -((2 * (float)dst_offsets[0].y / (float)dst_h) - 1.0f),
      (2 * (float)dst_offsets[1].x / (float)dst_w) - 1.0f, -((2 * (float)dst_offsets[1].y / (float)dst_h) - 1.0f),
   };

   float src_pos[4] = {
      (float)src_offsets[0].x, (float)src_offsets[0].y,
      (float)src_offsets[1].x, (float)src_offsets[1].y,
   };

   if (normalize_src_coords) {
      src_pos[0] /= src_w;
      src_pos[1] /= src_h;
      src_pos[2] /= src_w;
      src_pos[3] /= src_h;
   }

   float coords[] = {
      dst_pos[0], dst_pos[1], src_pos[0], src_pos[1],
      dst_pos[2], dst_pos[1], src_pos[2], src_pos[1],
      dst_pos[0], dst_pos[3], src_pos[0], src_pos[3],
      dst_pos[2], dst_pos[3], src_pos[2], src_pos[3],
   };

   batch->cmdlist->SetGraphicsRoot32BitConstants(1, ARRAY_SIZE(coords), coords, 0);

   D3D12_VIEWPORT vp = {
      .TopLeftX = 0,
      .TopLeftY = 0,
      .Width = (float)dst_w,
      .Height = (float)dst_h,
      .MinDepth = 0,
      .MaxDepth = 1,
   };
   batch->cmdlist->RSSetViewports(1, &vp);

   D3D12_RECT scissor = {
      .left = MIN2(dst_offsets[0].x, dst_offsets[1].x),
      .top = MIN2(dst_offsets[0].y, dst_offsets[1].y),
      .right = MAX2(dst_offsets[0].x, dst_offsets[1].x),
      .bottom = MAX2(dst_offsets[0].y, dst_offsets[1].y),
   };
   batch->cmdlist->RSSetScissorRects(1, &scissor);
}

void
dzn_cmd_buffer::blit_issue_barriers(dzn_image *src, VkImageLayout src_layout,
                                    const VkImageSubresourceLayers &src_subres,
                                    dzn_image *dst, VkImageLayout dst_layout,
                                    const VkImageSubresourceLayers &dst_subres,
                                    VkImageAspectFlagBits aspect,
                                    bool post)
{
   bool ds = aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
   D3D12_RESOURCE_BARRIER barriers[2] = {
      {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Transition = {
            .pResource = src->res.Get(),
            .StateBefore = dzn_image::get_state(src_layout),
            .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
         },
      },
      {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Transition = {
            .pResource = dst->res.Get(),
            .StateBefore = dzn_image::get_state(dst_layout),
            .StateAfter = ds ?
                          D3D12_RESOURCE_STATE_DEPTH_WRITE :
                          D3D12_RESOURCE_STATE_RENDER_TARGET,
         },
      },
   };

   if (post) {
      std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
      std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
   }

   uint32_t layer_count = dzn_get_layer_count(src, &src_subres);
   uint32_t src_level = src_subres.mipLevel;
   uint32_t dst_level = dst_subres.mipLevel;

   assert(dzn_get_layer_count(dst, &dst_subres) == layer_count);
   assert(src_level < src->vk.mip_levels);
   assert(dst_level < dst->vk.mip_levels);

   for (uint32_t layer = 0; layer < layer_count; layer++) {
      barriers[0].Transition.Subresource =
         src->get_subresource_index(src_subres, aspect, layer);
      barriers[1].Transition.Subresource =
         dst->get_subresource_index(dst_subres, aspect, layer);
      batch->cmdlist->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
   }
}

void
dzn_cmd_buffer::blit(const VkBlitImageInfo2KHR *info,
                     dzn_descriptor_heap &heap,
                     uint32_t &heap_offset,
                     uint32_t r)
{
   VK_FROM_HANDLE(dzn_image, src, info->srcImage);
   VK_FROM_HANDLE(dzn_image, dst, info->dstImage);

   ID3D12Device *dev = device->dev;
   dzn_batch *batch = get_batch();
   const VkImageBlit2KHR &region = info->pRegions[r];

   dzn_foreach_aspect(aspect, region.srcSubresource.aspectMask) {
      blit_set_pipeline(src, dst, aspect, info->filter, false);
      blit_issue_barriers(src, info->srcImageLayout, region.srcSubresource,
                          dst, info->dstImageLayout, region.dstSubresource,
                          aspect, false);
      blit_prepare_src_view(info->srcImage, aspect, region.srcSubresource, heap, heap_offset++);
      blit_set_2d_region(src, region.srcSubresource, region.srcOffsets,
                         dst, region.dstSubresource, region.dstOffsets,
                         src->vk.samples == 1);

      uint32_t dst_depth =
         region.dstOffsets[1].z > region.dstOffsets[0].z ?
         region.dstOffsets[1].z - region.dstOffsets[0].z :
         region.dstOffsets[0].z - region.dstOffsets[1].z;
      uint32_t src_depth =
         region.srcOffsets[1].z > region.srcOffsets[0].z ?
         region.srcOffsets[1].z - region.srcOffsets[0].z :
         region.srcOffsets[0].z - region.srcOffsets[1].z;

      uint32_t layer_count = dzn_get_layer_count(src, &region.srcSubresource);
      uint32_t dst_level = region.dstSubresource.mipLevel;

      float src_slice_step = layer_count > 1 ? 1 : (float)src_depth / dst_depth;
      if (region.srcOffsets[0].z > region.srcOffsets[1].z)
         src_slice_step = -src_slice_step;
      float src_z_coord = layer_count > 1 ?
                          0 : (float)region.srcOffsets[0].z + (src_slice_step * 0.5f);
      uint32_t slice_count = layer_count > 1 ? layer_count : dst_depth;
      uint32_t dst_z_coord = layer_count > 1 ?
                             region.dstSubresource.baseArrayLayer :
                             region.dstOffsets[0].z;
      if (region.dstOffsets[0].z > region.dstOffsets[1].z)
         dst_z_coord--;

      uint32_t dst_slice_step = region.dstOffsets[0].z < region.dstOffsets[1].z ?
                                1 : -1;

      /* Normalize the src coordinates/step */
      if (layer_count == 1 && src->vk.samples == 1) {
         src_z_coord /= src->vk.extent.depth;
         src_slice_step /= src->vk.extent.depth;
      }

      for (uint32_t slice = 0; slice < slice_count; slice++) {
         blit_prepare_dst_view(dst, aspect, dst_level, dst_z_coord);
         batch->cmdlist->SetGraphicsRoot32BitConstants(1, 1, &src_z_coord, 16);
         batch->cmdlist->DrawInstanced(4, 1, 0, 0);
         src_z_coord += src_slice_step;
         dst_z_coord += dst_slice_step;
      }

      blit_issue_barriers(src, info->srcImageLayout, region.srcSubresource,
                          dst, info->dstImageLayout, region.dstSubresource,
                          aspect, true);
   }
}

void
dzn_cmd_buffer::blit(const VkBlitImageInfo2KHR *info)
{
   if (info->regionCount == 0)
      return;

   uint32_t desc_count = 0;
   for (uint32_t r = 0; r < info->regionCount; r++)
      desc_count += util_bitcount(info->pRegions[r].srcSubresource.aspectMask);

   auto heap =
      dzn_descriptor_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, desc_count, true);

   heaps.push_back(heap);

   ID3D12DescriptorHeap * const heaps[] = { heap };
   batch->cmdlist->SetDescriptorHeaps(ARRAY_SIZE(heaps), heaps);
   batch->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   uint32_t heap_offset = 0;
   for (uint32_t r = 0; r < info->regionCount; r++)
      blit(info, heap, heap_offset, r);

   state.pipeline = NULL;
   state.dirty |= DZN_CMD_DIRTY_VIEWPORTS | DZN_CMD_DIRTY_SCISSORS;
   state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = heap;
   if (state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].pipeline) {
      state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
   }
}

void
dzn_cmd_buffer::resolve(const VkResolveImageInfo2KHR *info,
                        dzn_descriptor_heap &heap,
                        uint32_t &heap_offset,
                        uint32_t r)
{
   VK_FROM_HANDLE(dzn_image, src, info->srcImage);
   VK_FROM_HANDLE(dzn_image, dst, info->dstImage);

   ID3D12Device *dev = device->dev;
   dzn_batch *batch = get_batch();
   const VkImageResolve2KHR &region = info->pRegions[r];

   dzn_foreach_aspect(aspect, region.srcSubresource.aspectMask) {
      blit_set_pipeline(src, dst, aspect, VK_FILTER_NEAREST, true);
      blit_issue_barriers(src, info->srcImageLayout, region.srcSubresource,
                          dst, info->dstImageLayout, region.dstSubresource,
                          aspect, false);
      blit_prepare_src_view(info->srcImage, aspect, region.srcSubresource, heap, heap_offset++);

      VkOffset3D src_offset[2] = {
         {
            .x = region.srcOffset.x,
            .y = region.srcOffset.y,
         },
         {
            .x = (int32_t)(region.srcOffset.x + region.extent.width),
            .y = (int32_t)(region.srcOffset.y + region.extent.height),
         },
      };
      VkOffset3D dst_offset[2] = {
         {
            .x = region.dstOffset.x,
            .y = region.dstOffset.y,
         },
         {
            .x = (int32_t)(region.dstOffset.x + region.extent.width),
            .y = (int32_t)(region.dstOffset.y + region.extent.height),
         },
      };

      blit_set_2d_region(src, region.srcSubresource, src_offset,
                         dst, region.dstSubresource, dst_offset,
                         false);

      uint32_t layer_count = dzn_get_layer_count(src, &region.srcSubresource);
      for (uint32_t layer = 0; layer < layer_count; layer++) {
         float src_z_coord = layer;

         blit_prepare_dst_view(dst, aspect, region.dstSubresource.mipLevel,
                               region.dstSubresource.baseArrayLayer + layer);
         batch->cmdlist->SetGraphicsRoot32BitConstants(1, 1, &src_z_coord, 16);
         batch->cmdlist->DrawInstanced(4, 1, 0, 0);
      }

      blit_issue_barriers(src, info->srcImageLayout, region.srcSubresource,
                          dst, info->dstImageLayout, region.dstSubresource,
                          aspect, true);
   }
}
void
dzn_cmd_buffer::resolve(const VkResolveImageInfo2KHR *info)
{
   if (info->regionCount == 0)
      return;

   uint32_t desc_count = 0;
   for (uint32_t r = 0; r < info->regionCount; r++)
      desc_count += util_bitcount(info->pRegions[r].srcSubresource.aspectMask);

   auto heap =
      dzn_descriptor_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, desc_count, true);

   heaps.push_back(heap);

   ID3D12DescriptorHeap * const heaps[] = { heap };
   batch->cmdlist->SetDescriptorHeaps(ARRAY_SIZE(heaps), heaps);
   batch->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   uint32_t heap_offset = 0;
   for (uint32_t r = 0; r < info->regionCount; r++)
      resolve(info, heap, heap_offset, r);

   state.pipeline = NULL;
   state.dirty |= DZN_CMD_DIRTY_VIEWPORTS | DZN_CMD_DIRTY_SCISSORS;
   state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = heap;
   if (state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].pipeline) {
      state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdClearAttachments(VkCommandBuffer commandBuffer,
                        uint32_t attachmentCount,
                        const VkClearAttachment *pAttachments,
                        uint32_t rectCount,
                        const VkClearRect *pRects)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   struct dzn_render_pass *pass = cmd_buffer->state.pass;
   const struct dzn_subpass *subpass = &pass->subpasses[cmd_buffer->state.subpass];
   dzn_batch *batch = cmd_buffer->get_batch();

   auto rects_elems =
      dzn_transient_zalloc<D3D12_RECT>(rectCount, &cmd_buffer->device->vk.alloc);
   D3D12_RECT *rects = rects_elems.get();
   for (unsigned i = 0; i < rectCount; i++) {
      assert(pRects[i].baseArrayLayer == 0 && pRects[i].layerCount == 1);
      dzn_translate_rect(&rects[i], &pRects[i].rect);
   }

   for (unsigned i = 0; i < attachmentCount; i++) {
      uint32_t idx;
      if (pAttachments[i].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
         idx = subpass->colors[pAttachments[i].colorAttachment].idx;
      else
         idx = subpass->zs.idx;

      cmd_buffer->clear_attachment(idx, &pAttachments[i].clearValue,
                                   pAttachments[i].aspectMask,
                                   rectCount, rects);
   }
}

void
dzn_cmd_buffer::attachment_transition(const dzn_attachment_ref &att)
{
   dzn_batch *batch = get_batch();
   const dzn_image *image = state.framebuffer->attachments[att.idx]->get_image();

   if (att.before == att.during)
      return;

   D3D12_RESOURCE_BARRIER barrier = {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
         .pResource = image->res.Get(),
         .Subresource = 0, // YOLO
         .StateBefore = att.before,
         .StateAfter = att.during,
      },
   };
   batch->cmdlist->ResourceBarrier(1, &barrier);
}

void
dzn_cmd_buffer::attachment_transition(const dzn_attachment &att)
{
   dzn_batch *batch = get_batch();
   const dzn_image *image = state.framebuffer->attachments[att.idx]->get_image();

   if (att.last == att.after)
      return;

   D3D12_RESOURCE_BARRIER barrier = {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
         .pResource = image->res.Get(),
         .Subresource = 0, // YOLO
         .StateBefore = att.last,
         .StateAfter = att.after,
      },
   };
   batch->cmdlist->ResourceBarrier(1, &barrier);
}

void
dzn_cmd_buffer::resolve_attachment(uint32_t i)
{
   const struct dzn_subpass *subpass = &state.pass->subpasses[state.subpass];

   if (subpass->resolve[i].idx == VK_ATTACHMENT_UNUSED)
      return;

   dzn_batch *batch = get_batch();
   const dzn_framebuffer *framebuffer = state.framebuffer;
   struct dzn_image_view *src = framebuffer->attachments[subpass->colors[i].idx];
   struct dzn_image_view *dst = framebuffer->attachments[subpass->resolve[i].idx];
   D3D12_RESOURCE_BARRIER barriers[2];
   uint32_t barrier_count = 0;

   /* TODO: 2DArrays/3D */
   if (subpass->colors[i].during != D3D12_RESOURCE_STATE_RESOLVE_SOURCE) {
      barriers[barrier_count++] = D3D12_RESOURCE_BARRIER {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Transition = {
            .pResource = src->get_image()->res.Get(),
            .Subresource = 0,
            .StateBefore = subpass->colors[i].during,
            .StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
         },
      };
   }

   if (subpass->resolve[i].during != D3D12_RESOURCE_STATE_RESOLVE_DEST) {
      barriers[barrier_count++] = D3D12_RESOURCE_BARRIER {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
         .Transition = {
            .pResource = dst->get_image()->res.Get(),
            .Subresource = 0,
            .StateBefore = subpass->resolve[i].during,
            .StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST,
         },
      };
   }

   if (barrier_count)
      batch->cmdlist->ResourceBarrier(barrier_count, barriers);

   batch->cmdlist->ResolveSubresource(dst->get_image()->res.Get(), 0,
                                      src->get_image()->res.Get(), 0,
                                      dst->desc.Format);

   for (uint32_t b = 0; b < barrier_count; b++)
      std::swap(barriers[b].Transition.StateBefore, barriers[b].Transition.StateAfter);

   if (barrier_count)
      batch->cmdlist->ResourceBarrier(barrier_count, barriers);
}

void
dzn_cmd_buffer::begin_subpass()
{
   struct dzn_framebuffer *framebuffer = state.framebuffer;
   struct dzn_render_pass *pass = state.pass;
   const struct dzn_subpass *subpass = &pass->subpasses[state.subpass];
   dzn_batch *batch = get_batch();

   D3D12_CPU_DESCRIPTOR_HANDLE rt_handles[MAX_RTS] = { };
   D3D12_CPU_DESCRIPTOR_HANDLE zs_handle = { 0 };

   for (uint32_t i = 0; i < subpass->color_count; i++) {
      if (subpass->colors[i].idx == VK_ATTACHMENT_UNUSED) continue;

      rt_handles[i] = framebuffer->attachments[subpass->colors[i].idx]->rt_handle.cpu_handle;
   }

   if (subpass->zs.idx != VK_ATTACHMENT_UNUSED) {
      zs_handle = framebuffer->attachments[subpass->zs.idx]->zs_handle.cpu_handle;
   }

   batch->cmdlist->OMSetRenderTargets(subpass->color_count,
                                      subpass->color_count ? rt_handles : NULL,
                                      FALSE, zs_handle.ptr ? &zs_handle : NULL);

   for (uint32_t i = 0; i < subpass->color_count; i++)
      attachment_transition(subpass->colors[i]);
   for (uint32_t i = 0; i < subpass->input_count; i++)
      attachment_transition(subpass->inputs[i]);
   if (subpass->zs.idx != VK_ATTACHMENT_UNUSED)
      attachment_transition(subpass->zs);
}

void
dzn_cmd_buffer::end_subpass()
{
   const dzn_subpass *subpass = &state.pass->subpasses[state.subpass];

   for (uint32_t i = 0; i < subpass->color_count; i++)
      resolve_attachment(i);
}

void
dzn_cmd_buffer::next_subpass()
{
   end_subpass();
   assert(state.subpass + 1 < state.pass->subpass_count);
   state.subpass++;
   begin_subpass();
}

void
dzn_cmd_buffer::begin_pass(const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                           const VkSubpassBeginInfoKHR *pSubpassBeginInfo)
{
   VK_FROM_HANDLE(dzn_render_pass, pass, pRenderPassBeginInfo->renderPass);
   VK_FROM_HANDLE(dzn_framebuffer, framebuffer, pRenderPassBeginInfo->framebuffer);

   assert(pass->attachment_count == framebuffer->attachment_count);

   state.framebuffer = framebuffer;
   state.render_area = D3D12_RECT {
      .left = pRenderPassBeginInfo->renderArea.offset.x,
      .top = pRenderPassBeginInfo->renderArea.offset.y,
      .right = (LONG)(pRenderPassBeginInfo->renderArea.offset.x + pRenderPassBeginInfo->renderArea.extent.width),
      .bottom = (LONG)(pRenderPassBeginInfo->renderArea.offset.y + pRenderPassBeginInfo->renderArea.extent.height),
   };

   // The render area has an impact on the scissor state.
   state.dirty |= DZN_CMD_DIRTY_SCISSORS;
   state.pass = pass;
   state.subpass = 0;
   begin_subpass();

   uint32_t clear_count =
      MIN2(pRenderPassBeginInfo->clearValueCount, framebuffer->attachment_count);
   for (int i = 0; i < clear_count; ++i) {
      VkImageAspectFlags aspectMask =
         (pass->attachments[i].clear.color ? VK_IMAGE_ASPECT_COLOR_BIT : 0) |
         (pass->attachments[i].clear.depth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
         (pass->attachments[i].clear.stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
      clear_attachment(i, &pRenderPassBeginInfo->pClearValues[i], aspectMask,
                       1, &state.render_area);
   }
}

void
dzn_cmd_buffer::end_pass(const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
   end_subpass();

   for (uint32_t i = 0; i < state.pass->attachment_count; i++)
      attachment_transition(state.pass->attachments[i]);

   state.framebuffer = NULL;
   state.pass = NULL;
   state.subpass = 0;
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdBeginRenderPass2(VkCommandBuffer commandBuffer,
                        const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                        const VkSubpassBeginInfoKHR *pSubpassBeginInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->begin_pass(pRenderPassBeginInfo, pSubpassBeginInfo);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdEndRenderPass2(VkCommandBuffer commandBuffer,
                      const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->end_pass(pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdNextSubpass2(VkCommandBuffer commandBuffer,
                    const VkSubpassBeginInfo *pSubpassBeginInfo,
                    const VkSubpassEndInfo *pSubpassEndInfo)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->next_subpass();
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
   struct dzn_descriptor_state *desc_state = &state.bindpoint[bindpoint].desc_state;
   ID3D12DescriptorHeap **new_heaps = desc_state->heaps;
   const struct dzn_pipeline *pipeline = state.bindpoint[bindpoint].pipeline;
   dzn_batch *batch = get_batch();

   if (!(state.bindpoint[bindpoint].dirty & DZN_CMD_BINDPOINT_DIRTY_HEAPS))
      goto set_heaps;

   dzn_foreach_pool_type (type) {
      uint32_t desc_count = pipeline->layout->desc_count[type];
      if (!desc_count)
         continue;

      uint32_t dst_offset = 0;
      auto dst_heap =
         dzn_descriptor_heap(device, type, desc_count, true);

      for (uint32_t s = 0; s < MAX_SETS; s++) {
         const struct dzn_descriptor_set *set = desc_state->sets[s].set;
         if (!set) continue;

         uint32_t set_heap_offset = pipeline->layout->sets[s].heap_offsets[type];
         uint32_t set_desc_count = set->layout->range_desc_count[type];
         if (set_desc_count)
            dst_heap.copy(set_heap_offset, set->heaps[type], 0, set_desc_count);

         if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
            for (uint32_t o = 0, elem = 0; o < set->layout->dynamic_buffers.count; o++, elem++) {
               uint32_t b = set->layout->dynamic_buffers.bindings[o];

               if (o > 0 && set->layout->dynamic_buffers.bindings[o - 1] != b)
                  elem = 0;

	       uint32_t desc_heap_offset = set->layout->get_heap_offset(b, type, false) + elem;

               dst_heap.write_desc(set_heap_offset + desc_heap_offset,
                                   false,
                                   set->dynamic_buffers[o] +
                                   desc_state->sets[s].dynamic_offsets[o]);

               if (dzn_descriptor_heap::type_depends_on_shader_usage(set->dynamic_buffers[o].type)) {
                  desc_heap_offset = set->layout->get_heap_offset(b, type, true) + elem;
                  dst_heap.write_desc(set_heap_offset + desc_heap_offset,
                                      true,
                                      set->dynamic_buffers[o] +
                                      desc_state->sets[s].dynamic_offsets[o]);
               }
            }
         }
      }

      new_heaps[type] = dst_heap;
      heaps.push_back(dst_heap);
   }

set_heaps:
   if (new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] != state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] ||
       new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] != state.heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]) {
      ID3D12DescriptorHeap *desc_heaps[2];
      uint32_t num_desc_heaps = 0;
      if (new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV])
         desc_heaps[num_desc_heaps++] = new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
      if (new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER])
         desc_heaps[num_desc_heaps++] = new_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
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

void
dzn_cmd_buffer::update_sysvals(uint32_t bindpoint)
{
   if (!(state.bindpoint[bindpoint].dirty & DZN_CMD_BINDPOINT_DIRTY_SYSVALS))
      return;

   const struct dzn_pipeline *pipeline = state.bindpoint[bindpoint].pipeline;
   uint32_t sysval_cbv_param_idx = pipeline->layout->root.sysval_cbv_param_idx;
   dzn_batch *batch = get_batch();

   if (bindpoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      batch->cmdlist->SetGraphicsRoot32BitConstants(sysval_cbv_param_idx,
                                                    sizeof(state.sysvals.gfx) / 4,
                                                    &state.sysvals.gfx, 0);
   } else {
      batch->cmdlist->SetComputeRoot32BitConstants(sysval_cbv_param_idx,
                                                   sizeof(state.sysvals.compute) / 4,
                                                   &state.sysvals.compute, 0);
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
   struct dzn_descriptor_state *desc_state =
      &cmd_buffer->state.bindpoint[pipelineBindPoint].desc_state;
   uint32_t dirty = 0;

   for (uint32_t i = 0; i < descriptorSetCount; i++) {
      uint32_t idx = firstSet + i;
      VK_FROM_HANDLE(dzn_descriptor_set, set, pDescriptorSets[i]);

      if (desc_state->sets[idx].set != set) {
         desc_state->sets[idx].set = set;
         dirty |= DZN_CMD_BINDPOINT_DIRTY_HEAPS;
      }

      if (set->layout->dynamic_buffers.count) {
         assert(dynamicOffsetCount >= set->layout->dynamic_buffers.count);

         for (uint32_t j = 0; j < set->layout->dynamic_buffers.count; j++)
            desc_state->sets[idx].dynamic_offsets[j] = pDynamicOffsets[j];

         dynamicOffsetCount -= set->layout->dynamic_buffers.count;
         pDynamicOffsets += set->layout->dynamic_buffers.count;
         dirty |= DZN_CMD_BINDPOINT_DIRTY_HEAPS;
      }
   }

   cmd_buffer->state.bindpoint[pipelineBindPoint].dirty |= dirty;
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

   STATIC_ASSERT(MAX_VP <= DXIL_SPIRV_MAX_VIEWPORT);

   for (uint32_t i = 0; i < viewportCount; i++) {
      uint32_t vp = i + firstViewport;

      dzn_translate_viewport(&cmd_buffer->state.viewports[vp], &pViewports[i]);

      if (pViewports[i].minDepth > pViewports[i].maxDepth)
         cmd_buffer->state.sysvals.gfx.yz_flip_mask |= BITFIELD_BIT(vp + DXIL_SPIRV_Z_FLIP_SHIFT);
      else
         cmd_buffer->state.sysvals.gfx.yz_flip_mask &= ~BITFIELD_BIT(vp + DXIL_SPIRV_Z_FLIP_SHIFT);

      if (pViewports[i].height > 0)
         cmd_buffer->state.sysvals.gfx.yz_flip_mask |= BITFIELD_BIT(vp);
      else
         cmd_buffer->state.sysvals.gfx.yz_flip_mask &= ~BITFIELD_BIT(vp);
   }

   if (viewportCount) {
      cmd_buffer->state.dirty |= DZN_CMD_DIRTY_VIEWPORTS;
      cmd_buffer->state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_SYSVALS;
   }
}

void
dzn_cmd_buffer::update_scissors()
{
   const dzn_graphics_pipeline *pipeline =
      reinterpret_cast<const dzn_graphics_pipeline *>(state.pipeline);
   dzn_batch *batch = get_batch();

   if (!(state.dirty & DZN_CMD_DIRTY_SCISSORS))
      return;

   if (!pipeline->scissor.count) {
      /* Apply a scissor delimiting the render area. */
      batch->cmdlist->RSSetScissorRects(1, &state.render_area);
      return;
   }

   D3D12_RECT scissors[MAX_SCISSOR];
   uint32_t scissor_count = pipeline->scissor.count;

   memcpy(scissors, state.scissors, sizeof(D3D12_RECT) * pipeline->scissor.count);
   for (uint32_t i = 0; i < pipeline->scissor.count; i++) {
      scissors[i].left = MAX2(scissors[i].left, state.render_area.left);
      scissors[i].top = MAX2(scissors[i].top, state.render_area.top);
      scissors[i].right = MIN2(scissors[i].right, state.render_area.right);
      scissors[i].bottom = MIN2(scissors[i].bottom, state.render_area.bottom);
   }

   batch->cmdlist->RSSetScissorRects(pipeline->scissor.count, scissors);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdSetScissor(VkCommandBuffer commandBuffer,
                  uint32_t firstScissor,
                  uint32_t scissorCount,
                  const VkRect2D *pScissors)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   for (uint32_t i = 0; i < scissorCount; i++)
      dzn_translate_rect(&cmd_buffer->state.scissors[i + firstScissor], &pScissors[i]);

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
dzn_cmd_buffer::triangle_fan_create_index(uint32_t &vertex_count)
{
   uint8_t index_size = vertex_count <= 0xffff ? 2 : 4;
   uint32_t triangle_count = MAX2(vertex_count, 2) - 2;

   vertex_count = triangle_count * 3;
   if (!vertex_count)
      return;

   ID3D12Resource *index_buf =
      dzn_cmd_buffer::alloc_internal_buf(vertex_count * index_size,
                                         D3D12_HEAP_TYPE_UPLOAD,
                                         D3D12_RESOURCE_STATE_GENERIC_READ);
   void *cpu_ptr;
   index_buf->Map(0, NULL, &cpu_ptr);

   /* TODO: VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT */
   if (index_size == 2) {
      uint16_t *indices = (uint16_t *)cpu_ptr;
      for (uint32_t t = 0; t < triangle_count; t++) {
         indices[t * 3] = t + 1;
         indices[(t * 3) + 1] = t + 2;
         indices[(t * 3) + 2] = 0;
      }
      state.ib.view.Format = DXGI_FORMAT_R16_UINT;
   } else {
      uint32_t *indices = (uint32_t *)cpu_ptr;
      for (uint32_t t = 0; t < triangle_count; t++) {
         indices[t * 3] = t + 1;
         indices[(t * 3) + 1] = t + 2;
         indices[(t * 3) + 2] = 0;
      }
      state.ib.view.Format = DXGI_FORMAT_R32_UINT;
   }

   state.ib.view.SizeInBytes = vertex_count * index_size;
   state.ib.view.BufferLocation = index_buf->GetGPUVirtualAddress();
   state.dirty |= DZN_CMD_DIRTY_IB;
}

void
dzn_cmd_buffer::triangle_fan_rewrite_index(uint32_t &index_count,
                                           uint32_t &first_index)
{
   uint32_t triangle_count = MAX2(index_count, 2) - 2;

   index_count = triangle_count * 3;
   if (!index_count)
      return;

   /* New index is always 32bit to make the compute shader rewriting the
    * index simpler */
   ID3D12Resource *new_index_buf =
      dzn_cmd_buffer::alloc_internal_buf(index_count * 4,
                                         D3D12_HEAP_TYPE_DEFAULT,
                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   D3D12_GPU_VIRTUAL_ADDRESS old_index_buf_gpu = state.ib.view.BufferLocation;

   auto index_type =
      dzn_meta_triangle_fan_rewrite_index::get_index_type(state.ib.view.Format);
   const dzn_meta_triangle_fan_rewrite_index *rewrite_index =
      device->triangle_fan[index_type].get();

   const dzn_pipeline *compute_pipeline =
      state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].pipeline;

   struct dzn_triangle_fan_rewrite_index_params params = {
      .first_index = first_index,
   };

   batch->cmdlist->SetComputeRootSignature(rewrite_index->root_sig.Get());
   batch->cmdlist->SetPipelineState(rewrite_index->pipeline_state.Get());
   batch->cmdlist->SetComputeRootUnorderedAccessView(0, new_index_buf->GetGPUVirtualAddress());
   batch->cmdlist->SetComputeRoot32BitConstants(1, sizeof(params) / 4,
                                                &params, 0);
   batch->cmdlist->SetComputeRootShaderResourceView(2, old_index_buf_gpu);
   batch->cmdlist->Dispatch(triangle_count, 1, 1);

   D3D12_RESOURCE_BARRIER post_barriers[] = {
      {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
          /* Transition the exec buffer to indirect arg so it can be
           * pass to ExecuteIndirect() as an argument buffer.
           */
         .Transition = {
            .pResource = new_index_buf,
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            .StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER,
         },
      },
   };

   batch->cmdlist->ResourceBarrier(ARRAY_SIZE(post_barriers), post_barriers);

   /* We don't mess up with the driver state when executing our internal
    * compute shader, but we still change the D3D12 state, so let's mark
    * things dirty if needed.
    */
   state.pipeline = NULL;
   if (state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].pipeline) {
      state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
   }

   state.ib.view.SizeInBytes = index_count * 4;
   state.ib.view.BufferLocation = new_index_buf->GetGPUVirtualAddress();
   state.ib.view.Format = DXGI_FORMAT_R32_UINT;
   state.dirty |= DZN_CMD_DIRTY_IB;
   first_index = 0;
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

void
dzn_cmd_buffer::draw(uint32_t vertex_count,
                     uint32_t instance_count,
                     uint32_t first_vertex,
                     uint32_t first_instance)
{
   const dzn_graphics_pipeline *pipeline =
      reinterpret_cast<const dzn_graphics_pipeline *>(state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].pipeline);
   dzn_batch *batch = get_batch();

   state.sysvals.gfx.first_vertex = first_vertex;
   state.sysvals.gfx.base_instance = first_instance;
   state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
      DZN_CMD_BINDPOINT_DIRTY_SYSVALS;

   if (pipeline->ia.triangle_fan) {
      D3D12_INDEX_BUFFER_VIEW ib_view = state.ib.view;

      triangle_fan_create_index(vertex_count);
      if (!vertex_count)
         return;

      state.sysvals.gfx.is_indexed_draw = true;
      prepare_draw(true);
      batch->cmdlist->DrawIndexedInstanced(vertex_count, instance_count, 0,
                                           first_vertex, first_instance);

      /* Restore the IB view if we modified it when lowering triangle fans. */
      if (ib_view.SizeInBytes > 0) {
         state.ib.view = ib_view;
         state.dirty |= DZN_CMD_DIRTY_IB;
      }
   } else {
      state.sysvals.gfx.is_indexed_draw = false;
      prepare_draw(false);
      batch->cmdlist->DrawInstanced(vertex_count, instance_count,
                                    first_vertex, first_instance);
   }
}

void
dzn_cmd_buffer::draw(uint32_t index_count,
                     uint32_t instance_count,
                     uint32_t first_index,
                     int32_t vertex_offset,
                     uint32_t first_instance)
{
   const dzn_graphics_pipeline *pipeline =
      reinterpret_cast<const dzn_graphics_pipeline *>(state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].pipeline);
   dzn_batch *batch = get_batch();

   state.sysvals.gfx.first_vertex = vertex_offset;
   state.sysvals.gfx.base_instance = first_instance;
   state.sysvals.gfx.is_indexed_draw = true;
   state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
      DZN_CMD_BINDPOINT_DIRTY_SYSVALS;

   D3D12_INDEX_BUFFER_VIEW ib_view = state.ib.view;

   if (pipeline->ia.triangle_fan) {
      triangle_fan_rewrite_index(index_count, first_index);
      if (!index_count)
         return;
   }

   prepare_draw(true);
   batch->cmdlist->DrawIndexedInstanced(index_count, instance_count, first_index,
                                        vertex_offset, first_instance);

   /* Restore the IB view if we modified it when lowering triangle fans. */
   if (pipeline->ia.triangle_fan && ib_view.SizeInBytes) {
      state.ib.view = ib_view;
      state.dirty |= DZN_CMD_DIRTY_IB;
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdDraw(VkCommandBuffer commandBuffer,
            uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->draw(vertexCount, instanceCount, firstVertex, firstInstance);
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

   cmd_buffer->draw(indexCount, instanceCount, firstIndex, vertexOffset,
                    firstInstance);
}

ID3D12Resource *
dzn_cmd_buffer::alloc_internal_buf(uint32_t size,
                                   D3D12_HEAP_TYPE heap_type,
                                   D3D12_RESOURCE_STATES init_state)
{
   ComPtr<ID3D12Resource> res;

   /* Align size on 64k (the default alignment) */
   size = ALIGN_POT(size, 64 * 1024);

   D3D12_HEAP_PROPERTIES hprops =
      device->dev->GetCustomHeapProperties(0, heap_type);
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
      .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
   };

   HRESULT hres =
      device->dev->CreateCommittedResource(&hprops, D3D12_HEAP_FLAG_NONE, &rdesc,
                                           init_state,
                                           NULL, IID_PPV_ARGS(&res));
   assert(!FAILED(hres));

   internal_bufs.push_back(res);
   return res.Get();
}

uint32_t
dzn_cmd_buffer::triangle_fan_get_max_index_buf_size(bool indexed)
{
   dzn_graphics_pipeline *pipeline =
      reinterpret_cast<dzn_graphics_pipeline *>(state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].pipeline);

   if (!pipeline->ia.triangle_fan)
      return 0;

   uint32_t max_triangles;

   if (indexed) {
      uint32_t index_size = state.ib.view.Format == DXGI_FORMAT_R32_UINT ? 4 : 2;
      uint32_t max_indices = state.ib.view.SizeInBytes / index_size;

      max_triangles = MAX2(max_indices, 2) - 2;
   } else {
      uint32_t max_vertex = 0;
      for (uint32_t i = 0; i < pipeline->vb.count; i++) {
         max_vertex =
            MAX2(max_vertex,
                 state.vb.views[i].SizeInBytes / state.vb.views[i].StrideInBytes);
      }

      max_triangles = MAX2(max_vertex, 2) - 2;
   }

   return max_triangles * 3;
}

void
dzn_cmd_buffer::draw(dzn_buffer *draw_buf,
                     size_t draw_buf_offset,
                     uint32_t draw_count,
                     uint32_t draw_buf_stride,
                     bool indexed)
{
   dzn_graphics_pipeline *pipeline =
      reinterpret_cast<dzn_graphics_pipeline *>(state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].pipeline);
   bool triangle_fan = pipeline->ia.triangle_fan;
   dzn_batch *batch = get_batch();
   uint32_t min_draw_buf_stride =
      indexed ?
      sizeof(struct dzn_indirect_indexed_draw_params) :
      sizeof(struct dzn_indirect_draw_params);

   draw_buf_stride = draw_buf_stride ? draw_buf_stride : min_draw_buf_stride;
   assert(draw_buf_stride >= min_draw_buf_stride);
   assert((draw_buf_stride & 3) == 0);

   uint32_t sysvals_stride = ALIGN_POT(sizeof(state.sysvals.gfx), 256);
   uint32_t exec_buf_stride = 32;
   uint32_t triangle_fan_index_buf_stride =
      triangle_fan_get_max_index_buf_size(indexed) * sizeof(uint32_t);
   uint32_t triangle_fan_exec_buf_stride =
      sizeof(struct dzn_indirect_triangle_fan_rewrite_index_exec_params);
   ID3D12Resource *exec_buf =
      alloc_internal_buf(draw_count * exec_buf_stride,
                         D3D12_HEAP_TYPE_DEFAULT,
                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   D3D12_GPU_VIRTUAL_ADDRESS draw_buf_gpu =
      draw_buf->res->GetGPUVirtualAddress() + draw_buf_offset;
   ID3D12Resource *triangle_fan_index_buf =
      triangle_fan_index_buf_stride ?
      alloc_internal_buf(draw_count * triangle_fan_index_buf_stride,
                         D3D12_HEAP_TYPE_DEFAULT,
                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS) :
      NULL;
   ID3D12Resource *triangle_fan_exec_buf =
      triangle_fan_index_buf_stride ?
      alloc_internal_buf(draw_count * triangle_fan_exec_buf_stride,
                         D3D12_HEAP_TYPE_DEFAULT,
                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS) :
      NULL;

   struct dzn_indirect_draw_triangle_fan_rewrite_params params = {
      .draw_buf_stride = draw_buf_stride,
      .triangle_fan_index_buf_stride = triangle_fan_index_buf_stride,
      .triangle_fan_index_buf_start =
         triangle_fan_index_buf ?
         triangle_fan_index_buf->GetGPUVirtualAddress() : 0,
   };
   uint32_t params_size =
      triangle_fan_index_buf_stride > 0 ?
      sizeof(struct dzn_indirect_draw_triangle_fan_rewrite_params) :
      sizeof(struct dzn_indirect_draw_rewrite_params);

   enum dzn_indirect_draw_type draw_type;

   if (indexed && triangle_fan_index_buf_stride > 0)
      draw_type = DZN_INDIRECT_INDEXED_DRAW_TRIANGLE_FAN;
   else if (!indexed && triangle_fan_index_buf_stride > 0)
      draw_type = DZN_INDIRECT_DRAW_TRIANGLE_FAN;
   else if (indexed)
      draw_type = DZN_INDIRECT_INDEXED_DRAW;
   else
      draw_type = DZN_INDIRECT_DRAW;

   dzn_meta_indirect_draw *indirect_draw =
      device->indirect_draws[draw_type].get();

   const dzn_pipeline *compute_pipeline =
      state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].pipeline;

   batch->cmdlist->SetComputeRootSignature(indirect_draw->root_sig.Get());
   batch->cmdlist->SetPipelineState(indirect_draw->pipeline_state.Get());
   batch->cmdlist->SetComputeRoot32BitConstants(0, params_size / 4, (const void *)&params, 0);
   batch->cmdlist->SetComputeRootShaderResourceView(1, draw_buf_gpu);
   batch->cmdlist->SetComputeRootUnorderedAccessView(2, exec_buf->GetGPUVirtualAddress());
   if (triangle_fan_exec_buf)
      batch->cmdlist->SetComputeRootUnorderedAccessView(3, triangle_fan_exec_buf->GetGPUVirtualAddress());

   batch->cmdlist->Dispatch(draw_count, 1, 1);

   D3D12_RESOURCE_BARRIER post_barriers[] = {
      {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
          /* Transition the exec buffer to indirect arg so it can be
           * pass to ExecuteIndirect() as an argument buffer.
           */
         .Transition = {
            .pResource = exec_buf,
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            .StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
         },
      },
      {
         .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
          /* Transition the exec buffer to indirect arg so it can be
           * pass to ExecuteIndirect() as an argument buffer.
           */
         .Transition = {
            .pResource = triangle_fan_exec_buf,
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            .StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
         },
      },
   };

   uint32_t post_barrier_count = triangle_fan_exec_buf ? 2 : 1;

   batch->cmdlist->ResourceBarrier(post_barrier_count, post_barriers);

   D3D12_INDEX_BUFFER_VIEW ib_view = {};

   if (triangle_fan_exec_buf) {
      auto index_type =
         indexed ?
         dzn_meta_triangle_fan_rewrite_index::get_index_type(state.ib.view.Format) :
         dzn_meta_triangle_fan_rewrite_index::NO_INDEX;
      dzn_meta_triangle_fan_rewrite_index *rewrite_index =
         device->triangle_fan[index_type].get();

      struct dzn_triangle_fan_rewrite_index_params rewrite_index_params = {};

      batch->cmdlist->SetComputeRootSignature(rewrite_index->root_sig.Get());
      batch->cmdlist->SetPipelineState(rewrite_index->pipeline_state.Get());
      batch->cmdlist->SetComputeRootUnorderedAccessView(0, triangle_fan_index_buf->GetGPUVirtualAddress());
      batch->cmdlist->SetComputeRoot32BitConstants(1, sizeof(rewrite_index_params) / 4,
                                                   (const void *)&rewrite_index_params, 0);

      if (indexed)
         batch->cmdlist->SetComputeRootShaderResourceView(2, state.ib.view.BufferLocation);

      ID3D12CommandSignature *cmd_sig = rewrite_index->cmd_sig.Get();
      batch->cmdlist->ExecuteIndirect(cmd_sig,
                                      draw_count, triangle_fan_exec_buf,
                                      0, NULL, 0);

      D3D12_RESOURCE_BARRIER index_buf_barriers[] = {
         {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
               .pResource = triangle_fan_index_buf,
               .Subresource = 0,
               .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
               .StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER,
            },
         },
      };

      batch->cmdlist->ResourceBarrier(ARRAY_SIZE(index_buf_barriers), index_buf_barriers);

      /* After our triangle-fan lowering the draw is indexed */
      indexed = true;
      ib_view = state.ib.view;
      state.ib.view.BufferLocation = triangle_fan_index_buf->GetGPUVirtualAddress();
      state.ib.view.SizeInBytes = triangle_fan_index_buf_stride;
      state.ib.view.Format = DXGI_FORMAT_R32_UINT;
      state.dirty |= DZN_CMD_DIRTY_IB;
   }

   /* We don't mess up with the driver state when executing our internal
    * compute shader, but we still change the D3D12 state, so let's mark
    * things dirty if needed.
    */
   state.pipeline = NULL;
   if (state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].pipeline) {
      state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].dirty |=
         DZN_CMD_BINDPOINT_DIRTY_PIPELINE;
   }

   state.sysvals.gfx.first_vertex = 0;
   state.sysvals.gfx.base_instance = 0;
   state.sysvals.gfx.is_indexed_draw = indexed;
   state.bindpoint[VK_PIPELINE_BIND_POINT_GRAPHICS].dirty |=
      DZN_CMD_BINDPOINT_DIRTY_SYSVALS;

   prepare_draw(indexed);

   /* Restore the old IB view if we modified it during the triangle fan lowering */
   if (ib_view.SizeInBytes) {
      state.ib.view = ib_view;
      state.dirty |= DZN_CMD_DIRTY_IB;
   }

   dzn_graphics_pipeline::indirect_cmd_sig_type cmd_sig_type =
      triangle_fan_index_buf_stride > 0 ?
      dzn_graphics_pipeline::INDIRECT_DRAW_TRIANGLE_FAN_CMD_SIG :
      indexed ?
      dzn_graphics_pipeline::INDIRECT_INDEXED_DRAW_CMD_SIG :
      dzn_graphics_pipeline::INDIRECT_DRAW_CMD_SIG;
   ID3D12CommandSignature *cmdsig =
      pipeline->get_indirect_cmd_sig(cmd_sig_type);

   batch->cmdlist->ExecuteIndirect(cmdsig,
                                   draw_count, exec_buf, 0, NULL, 0);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdDrawIndirect(VkCommandBuffer commandBuffer,
                    VkBuffer buffer,
                    VkDeviceSize offset,
                    uint32_t drawCount,
                    uint32_t stride)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_buffer, buf, buffer);

   cmd_buffer->draw(buf, offset, drawCount, stride, false);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdDrawIndexedIndirect(VkCommandBuffer commandBuffer,
                           VkBuffer buffer,
                           VkDeviceSize offset,
                           uint32_t drawCount,
                           uint32_t stride)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(dzn_buffer, buf, buffer);

   cmd_buffer->draw(buf, offset, drawCount, stride, true);
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

void
dzn_cmd_buffer::dispatch(uint32_t group_count_x,
                         uint32_t group_count_y,
                         uint32_t group_count_z)
{
   dzn_batch *batch = get_batch();

   state.sysvals.compute.group_count_x = group_count_x;
   state.sysvals.compute.group_count_y = group_count_y;
   state.sysvals.compute.group_count_z = group_count_z;
   state.bindpoint[VK_PIPELINE_BIND_POINT_COMPUTE].dirty |=
      DZN_CMD_BINDPOINT_DIRTY_SYSVALS;

   prepare_dispatch();
   batch->cmdlist->Dispatch(group_count_x, group_count_y, group_count_z);
}

VKAPI_ATTR void VKAPI_CALL
dzn_CmdDispatch(VkCommandBuffer commandBuffer,
                uint32_t groupCountX,
                uint32_t groupCountY,
                uint32_t groupCountZ)
{
   VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer, commandBuffer);

   cmd_buffer->dispatch(groupCountX, groupCountY, groupCountZ);
}
