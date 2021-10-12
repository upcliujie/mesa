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

#ifndef DZN_PRIVATE_H
#define DZN_PRIVATE_H

#include "vk_debug_report.h"
#include "vk_device.h"
#include "vk_physical_device.h"
#include "vk_queue.h"
#include "wsi_common.h"

#include "util/bitset.h"
#include "util/blob.h"
#include "util/u_dynarray.h"
#include "util/log.h"

#include "shader_enums.h"

#include "dzn_entrypoints.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_icd.h>

#include <dxgi1_4.h>

#define D3D12_IGNORE_SDK_LAYERS
#include <directx/d3d12.h>

#include "d3d12_descriptor_pool.h"

struct dzn_instance;

struct dzn_physical_device {
   struct vk_physical_device vk;

   /* Link in dzn_instance::physical_devices */
   struct list_head link;

   struct dzn_instance *instance;

   struct vk_device_extension_table supported_extensions;
   struct vk_physical_device_dispatch_table dispatch;

   IDXGIAdapter1 *adapter;

   uint8_t pipeline_cache_uuid[VK_UUID_SIZE];
   uint8_t device_uuid[VK_UUID_SIZE];
   uint8_t driver_uuid[VK_UUID_SIZE];

   struct wsi_device wsi_device;

   VkPhysicalDeviceMemoryProperties memory;
};

VkResult __vk_errorv(struct dzn_instance *instance,
                     const struct vk_object_base *object, VkResult error,
                     const char *file, int line, const char *format,
                     va_list args);

VkResult __vk_errorf(struct dzn_instance *instance,
                     const struct vk_object_base *object, VkResult error,
                     const char *file, int line, const char *format, ...)
   PRINTFLIKE(6, 7);

#ifdef DEBUG
#define vk_error(error) __vk_errorf(NULL, NULL, error, __FILE__, __LINE__, NULL)
#define vk_errorfi(instance, obj, error, format, ...)\
    __vk_errorf(instance, obj, error,\
                __FILE__, __LINE__, format, ## __VA_ARGS__)
#else

static inline VkResult __dummy_vk_error(VkResult error, UNUSED const void *ignored)
{
   return error;
}

#define vk_error(error) __dummy_vk_error(error, NULL)
#define vk_errorfi(instance, obj, error, format, ...) __dummy_vk_error(error, instance)
#define vk_errorf(device, obj, error, format, ...) __dummy_vk_error(error, device)
#endif

#define dzn_debug_ignored_stype(sType) \
   mesa_logd("%s: ignored VkStructureType %u\n", __func__, (sType))

IDXGIFactory4 *
dxgi_get_factory(bool debug);

void
d3d12_enable_debug_layer();

void
d3d12_enable_gpu_validation();

ID3D12Device *
d3d12_create_device(IUnknown *adapter, bool experimental_features);

UINT
dzn_get_subresource_index(const D3D12_RESOURCE_DESC *desc,
                          VkImageAspectFlags aspectMask,
                          unsigned mipLevel, unsigned arrayLayer);

struct dzn_queue {
   struct vk_queue vk;
   struct dzn_device *device;

   ID3D12CommandQueue *cmdqueue;
   ID3D12Fence *fence;
   uint64_t fence_point;
};

struct dzn_device {
   struct vk_device vk;

   struct dzn_instance *instance;
   struct dzn_physical_device *physical_device;

   struct dzn_queue queue;

   struct vk_device_extension_table enabled_extensions;
   struct vk_device_dispatch_table dispatch;

   ID3D12Device *dev;
   mtx_t pools_lock;
   struct d3d12_descriptor_pool *rtv_pool, *dsv_pool;
   D3D12_FEATURE_DATA_ARCHITECTURE1 arch;
};

struct dzn_device_memory {
   struct vk_object_base base;

   struct list_head link;

   ID3D12Heap *heap;
   VkDeviceSize size;
   D3D12_RESOURCE_STATES initial_state; /* initial state for this memory type */

   /* A buffer-resource spanning the entire heap, used for mapping memory */
   ID3D12Resource *map_res;

   VkDeviceSize map_size;
   void *map;
};

enum dzn_cmd_bindpoint_dirty {
   DZN_CMD_BINDPOINT_DIRTY_PIPELINE = 1 << 0,
   DZN_CMD_BINDPOINT_DIRTY_HEAPS = 1 << 1,
};

enum dzn_cmd_dirty {
   DZN_CMD_DIRTY_VIEWPORTS = 1 << 0,
   DZN_CMD_DIRTY_SCISSORS = 1 << 1,
};

#define MAX_VBS 16
#define MAX_VP 16
#define MAX_SCISSOR 16
#define MAX_SETS 4

#define NUM_BIND_POINT VK_PIPELINE_BIND_POINT_COMPUTE + 1
#define NUM_POOL_TYPES D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1

struct dzn_cmd_buffer {
   struct vk_object_base base;

   struct dzn_device *device;

   struct dzn_cmd_pool *pool;
   struct list_head pool_link;

   struct {
      struct dzn_framebuffer *framebuffer;
      const struct dzn_pipeline *pipeline;
      ID3D12DescriptorHeap *heaps[NUM_POOL_TYPES];
      struct dzn_render_pass *pass;
      struct {
         BITSET_DECLARE(dirty, MAX_VBS);
         D3D12_VERTEX_BUFFER_VIEW views[MAX_VBS];
      } vb;
      D3D12_VIEWPORT viewports[MAX_VP];
      D3D12_RECT scissors[MAX_SCISSOR];
      uint32_t dirty;
      uint32_t subpass;
      struct {
         const struct dzn_pipeline *pipeline;
         const struct dzn_descriptor_set *sets[MAX_SETS];
         ID3D12DescriptorHeap *heaps[NUM_POOL_TYPES];
         uint32_t dirty;
      } bindpoint[NUM_BIND_POINT];
   } state;

   struct util_dynarray heaps;

   VkCommandBufferUsageFlags usage_flags;
   VkCommandBufferLevel level;

   ID3D12CommandAllocator *alloc;
   ID3D12GraphicsCommandList *cmdlist;
   ID3D12Resource *rt0;
};

struct dzn_cmd_pool {
   struct vk_object_base base;
   VkAllocationCallbacks alloc;
   struct list_head cmd_buffers;

   VkCommandPoolCreateFlags flags;
};

struct dzn_descriptor_pool {
   struct vk_object_base base;
};

#define MAX_ROOT_PARAMS D3D12_SHADER_VISIBILITY_PIXEL + 1

struct dzn_descriptor_set_layout_binding {
   D3D12_SHADER_VISIBILITY visibility;
   uint32_t view_range_idx;
   uint32_t sampler_range_idx;
   uint32_t static_sampler_idx;
};

struct dzn_descriptor_set_layout {
   struct vk_object_base base;
   struct {
      uint32_t view_count;
      const D3D12_DESCRIPTOR_RANGE1 *views;
      uint32_t sampler_count;
      const D3D12_DESCRIPTOR_RANGE1 *samplers;
   } ranges[MAX_ROOT_PARAMS];
   uint32_t static_sampler_count;
   const D3D12_STATIC_SAMPLER_DESC *static_samplers;
   uint32_t view_desc_count, sampler_desc_count;
   uint32_t binding_count;
   const struct dzn_descriptor_set_layout_binding *bindings;
};

struct dzn_descriptor_set_binding {
   D3D12_CPU_DESCRIPTOR_HANDLE views;
   D3D12_CPU_DESCRIPTOR_HANDLE samplers;
};

struct dzn_descriptor_set {
   struct vk_object_base base;
   ID3D12DescriptorHeap *heaps[NUM_POOL_TYPES];
   const struct dzn_descriptor_set_layout *layout;
   const struct dzn_descriptor_set_binding *bindings;
};

struct dzn_pipeline_layout {
   struct vk_object_base base;
   uint32_t heap_offsets[MAX_SETS][NUM_POOL_TYPES];
   uint32_t desc_count[NUM_POOL_TYPES];
   struct {
      uint32_t param_count;
      D3D12_DESCRIPTOR_HEAP_TYPE type[MAX_ROOT_PARAMS];
      ID3D12RootSignature *sig;
   } root;
};

struct dzn_attachment_ref {
   uint32_t idx;
};

#define MAX_RTS 8

struct dzn_subpass {
   uint32_t color_count;
   struct dzn_attachment_ref colors[MAX_RTS];
   struct dzn_attachment_ref zs;
};

struct dzn_attachment {
   DXGI_FORMAT format;
   uint32_t samples;
   D3D12_RESOURCE_STATES before, during, after;
};

struct dzn_render_pass {
   struct vk_object_base base;
   uint32_t attachment_count;
   const struct dzn_attachment *attachments;
   uint32_t subpass_count;
   const struct dzn_subpass *subpasses;
};

struct dzn_pipeline_cache {
   struct vk_object_base base;
};

struct dzn_pipeline {
   struct vk_object_base base;
   const struct dzn_pipeline_layout *layout;
   ID3D12PipelineState *state;
};

struct dzn_graphics_pipeline {
   struct dzn_pipeline base;
   struct {
      unsigned count;
      uint32_t strides[MAX_VBS];
   } vb;

   struct {
      D3D_PRIMITIVE_TOPOLOGY topology;
   } ia;

   struct {
      unsigned count;
      D3D12_VIEWPORT desc[MAX_VP];
   } vp;

   struct {
      unsigned count;
      D3D12_RECT desc[MAX_SCISSOR];
   } scissor;

   struct {
      uint8_t stencil_ref;
   } zsa;
};

struct dzn_image {
   struct vk_object_base base;

   VkImageType type;
   VkFormat vk_format;

   D3D12_RESOURCE_DESC desc;
   ID3D12Resource *res;
   struct dzn_device_memory *mem;

   VkImageAspectFlags aspects;
   VkExtent3D extent;
   uint32_t levels;
   uint32_t array_size;
   uint32_t samples;
   uint32_t n_planes;
   VkImageUsageFlags usage;
   VkImageCreateFlags create_flags;
   VkImageTiling tiling;
};

struct dzn_image_view {
   struct vk_object_base base;

   const struct dzn_image *image;

   VkFormat vk_format;
   VkExtent3D extent;

   D3D12_SHADER_RESOURCE_VIEW_DESC desc;

   struct d3d12_descriptor_handle rt_handle;
   struct d3d12_descriptor_handle zs_handle;
};

struct dzn_buffer_view {
   struct vk_object_base base;

   const struct dzn_buffer *buffer;

   D3D12_SHADER_RESOURCE_VIEW_DESC desc;
};

struct dzn_framebuffer {
   struct vk_object_base base;

   uint32_t width, height, layers;

   uint32_t attachment_count;
   struct dzn_image_view *attachments[0];
};

struct dzn_buffer {
   struct vk_object_base base;

   struct dzn_device *device;
   VkDeviceSize size;

   D3D12_RESOURCE_DESC desc;
   ID3D12Resource *res;

   VkBufferCreateFlags create_flags;
   VkBufferUsageFlags usage;
};

struct dzn_sampler {
   struct vk_object_base base;
   D3D12_SAMPLER_DESC desc;
};

/* This is defined as a macro so that it works for both
 * VkImageSubresourceRange and VkImageSubresourceLayers
 */
#define dzn_get_layerCount(_image, _range) \
   ((_range)->layerCount == VK_REMAINING_ARRAY_LAYERS ? \
    (_image)->array_size - (_range)->baseArrayLayer : (_range)->layerCount)


#ifdef __cplusplus
extern "C" {
#endif
VkResult dzn_wsi_init(struct dzn_physical_device *physical_device);
void dzn_wsi_finish(struct dzn_physical_device *physical_device);
DXGI_FORMAT dzn_pipe_to_dxgi_format(enum pipe_format in);
D3D12_FILTER dzn_translate_sampler_filter(const VkSamplerCreateInfo *create_info);
D3D12_COMPARISON_FUNC dzn_translate_compare_op(VkCompareOp in);
void dzn_translate_viewport(D3D12_VIEWPORT *out, const VkViewport *in);
void dzn_translate_scissor(D3D12_RECT *out, const VkRect2D *in);
#ifdef __cplusplus
}
#endif

struct dzn_app_info {
   const char *app_name;
   uint32_t app_version;
   const char *engine_name;
   uint32_t engine_version;
   uint32_t api_version;
};

enum dzn_debug_flags {
   DZN_DEBUG_SYNC = 1 << 0,
   DZN_DEBUG_NIR = 1 << 1,
   DZN_DEBUG_DXIL = 1 << 2,
   DZN_DEBUG_WARP = 1 << 3,
};

struct dzn_instance {
   struct vk_instance vk;

   bool physical_devices_enumerated;
   struct list_head physical_devices;
   uint32_t debug_flags;
};

struct dzn_semaphore {
   struct vk_object_base base;
};

struct dzn_fence {
   struct vk_object_base base;

   ID3D12Fence *fence;
   HANDLE event;
};

struct dzn_shader_module {
   struct vk_object_base base;
   uint32_t code_size;
   uint32_t code[0];
};

DXGI_FORMAT
dzn_get_format(VkFormat format);

D3D12_RESOURCE_STATES
dzn_get_states(VkImageLayout layout);

#define DZN_DEFINE_HANDLE_CASTS(__dzn_type, __VkType)   \
                                                        \
   static inline struct __dzn_type *                    \
   __dzn_type ## _from_handle(__VkType _handle)         \
   {                                                    \
      return (struct __dzn_type *) _handle;             \
   }                                                    \
                                                        \
   static inline __VkType                               \
   __dzn_type ## _to_handle(struct __dzn_type *_obj)    \
   {                                                    \
      return (__VkType) _obj;                           \
   }

#define DZN_DEFINE_NONDISP_HANDLE_CASTS(__dzn_type, __VkType)              \
                                                                           \
   static inline struct __dzn_type *                                       \
   __dzn_type ## _from_handle(__VkType _handle)                            \
   {                                                                       \
      return (struct __dzn_type *)(uintptr_t) _handle;                     \
   }                                                                       \
                                                                           \
   static inline __VkType                                                  \
   __dzn_type ## _to_handle(struct __dzn_type *_obj)                       \
   {                                                                       \
      return (__VkType)(uintptr_t) _obj;                                   \
   }

#define DZN_FROM_HANDLE(__dzn_type, __name, __handle)			\
   struct __dzn_type *__name = __dzn_type ## _from_handle(__handle)

DZN_DEFINE_HANDLE_CASTS(dzn_cmd_buffer, VkCommandBuffer)
DZN_DEFINE_HANDLE_CASTS(dzn_device, VkDevice)
DZN_DEFINE_HANDLE_CASTS(dzn_instance, VkInstance)
DZN_DEFINE_HANDLE_CASTS(dzn_physical_device, VkPhysicalDevice)
DZN_DEFINE_HANDLE_CASTS(dzn_queue, VkQueue)

DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_cmd_pool, VkCommandPool)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_buffer, VkBuffer)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_buffer_view, VkBufferView)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_device_memory, VkDeviceMemory)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_descriptor_pool, VkDescriptorPool)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_descriptor_set, VkDescriptorSet)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_descriptor_set_layout, VkDescriptorSetLayout)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_event, VkEvent)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_fence, VkFence)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_framebuffer, VkFramebuffer)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_image, VkImage)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_image_view, VkImageView)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline, VkPipeline)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline_cache, VkPipelineCache)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline_layout, VkPipelineLayout)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_query_pool, VkQueryPool)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_render_pass, VkRenderPass)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_sampler, VkSampler)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_semaphore, VkSemaphore)
DZN_DEFINE_NONDISP_HANDLE_CASTS(dzn_shader_module, VkShaderModule)

#endif /* DZN_PRIVATE_H */
