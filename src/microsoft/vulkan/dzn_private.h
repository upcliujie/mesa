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

#include "vk_command_buffer.h"
#include "vk_debug_report.h"
#include "vk_device.h"
#include "vk_image.h"
#include "vk_log.h"
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
#include <dxcapi.h>
#include <wrl/client.h>

#include "spirv_to_dxil.h"
#include "d3d12_descriptor_pool.h"

#include <memory>
#include <mutex>
#include <vector>

using Microsoft::WRL::ComPtr;

struct dzn_instance;

template <typename T>
class dzn_allocator {
public:
   using value_type = T;

   template <typename U>
   dzn_allocator(const dzn_allocator<U> &src) noexcept
   {
      allocator = src.allocator;
      scope = src.scope;
   }

   dzn_allocator(const VkAllocationCallbacks *alloc = NULL,
		 VkSystemAllocationScope scope = VK_SYSTEM_ALLOCATION_SCOPE_OBJECT) noexcept
   {
      this->allocator = alloc ? *alloc : *vk_default_allocator();
      this->scope = scope;
   }

   T *allocate(size_t n)
   {
      return (T *)vk_alloc(&allocator, sizeof(T) * n, alignof(T), scope);
   }

   void deallocate(T *p, size_t n)
   {
      vk_free(&allocator, p);
   }

   VkAllocationCallbacks allocator;
   VkSystemAllocationScope scope;
};

template <typename T, typename U>
constexpr bool operator== (const dzn_allocator<T> &a, const dzn_allocator<U> &b) noexcept
{
  return !memcmp(a.allocator, b.allocator, sizeof(a.allocator)) && a.scope == b.scope;
}

template <typename T, typename U>
constexpr bool operator!= (const dzn_allocator<T> &a, const dzn_allocator<U> &b) noexcept
{
  return a.scope != b.scope || memcmp(a.allocator, b.allocator, sizeof(a.allocator));
}

template <typename T>
class dzn_object_deleter {
public:
   constexpr dzn_object_deleter() noexcept = default;
   ~dzn_object_deleter() = default;

   void operator()(T *obj)
   {
      const VkAllocationCallbacks *alloc = obj->get_vk_allocator();
      std::destroy_at(obj);
      vk_free(alloc, obj);
   }
};

template <typename T>
using dzn_object_unique_ptr = std::unique_ptr<T, dzn_object_deleter<T>>;

template <typename T>
using dzn_object_vector = std::vector<dzn_object_unique_ptr<T>, dzn_allocator<dzn_object_unique_ptr<T>>>;

class d3d12_descriptor_pool_deleter {
public:
   constexpr d3d12_descriptor_pool_deleter() noexcept = default;
   ~d3d12_descriptor_pool_deleter() = default;

   void operator()(d3d12_descriptor_pool *pool)
   {
      d3d12_descriptor_pool_free(pool);
   }
};

struct dzn_transient_object_deleter {
   const VkAllocationCallbacks *alloc;
   template <typename T>
   void operator()(T *ptr)
   {
      vk_free(alloc, ptr);
   }
};

template <typename T>
using dzn_transient_object = std::unique_ptr<T, dzn_transient_object_deleter>;

template <typename T>
dzn_transient_object<T>
dzn_transient_alloc(size_t count,
                    const VkAllocationCallbacks *parent_alloc,
                    const VkAllocationCallbacks *alloc = NULL)
{
   dzn_transient_object_deleter deleter = { alloc ? alloc : parent_alloc };

   if (!count)
      return dzn_transient_object<T>(NULL, deleter);

   T *ptr = (T *)
      vk_alloc2(parent_alloc, alloc, count * sizeof(T), alignof(T),
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   return dzn_transient_object<T>(ptr, deleter);
}

template <typename T>
dzn_transient_object<T>
dzn_transient_zalloc(size_t count,
                     const VkAllocationCallbacks *parent_alloc,
                     const VkAllocationCallbacks *alloc = NULL)
{
   dzn_transient_object_deleter deleter = { alloc ? alloc : parent_alloc };

   if (!count)
      return dzn_transient_object<T>(NULL, deleter);

   T *ptr = (T *)
      vk_zalloc2(parent_alloc, alloc, count * sizeof(T), alignof(T),
                 VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   return dzn_transient_object<T>(ptr, deleter);
}

template <typename T, typename... CreateArgs>
dzn_object_unique_ptr<T>
dzn_private_object_create(const VkAllocationCallbacks *parent_alloc,
                          CreateArgs... args)
{
   T *obj = (T *)
      vk_alloc(parent_alloc, sizeof(T), alignof(T),
               VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   std::construct_at(obj, std::forward<CreateArgs>(args)...);
   return dzn_object_unique_ptr<T>(obj);
}

struct dzn_physical_device {
   struct vk_physical_device vk;

   struct dzn_instance *instance;

   struct vk_device_extension_table supported_extensions;
   struct vk_physical_device_dispatch_table dispatch;

   ComPtr<IDXGIAdapter1> adapter;

   uint8_t pipeline_cache_uuid[VK_UUID_SIZE];
   uint8_t device_uuid[VK_UUID_SIZE];
   uint8_t driver_uuid[VK_UUID_SIZE];

   struct wsi_device wsi_device;

   VkPhysicalDeviceMemoryProperties memory;

   dzn_physical_device(dzn_instance *instance,
                       ComPtr<IDXGIAdapter1> &adapter,
                       const VkAllocationCallbacks *alloc);
   ~dzn_physical_device();
   const VkAllocationCallbacks *get_vk_allocator();
private:
   void get_device_extensions();
};

#define dzn_debug_ignored_stype(sType) \
   mesa_logd("%s: ignored VkStructureType %u\n", __func__, (sType))

ComPtr<IDXGIFactory4>
dxgi_get_factory(bool debug);

ComPtr<IDxcValidator>
dxil_get_validator(void);

ComPtr<IDxcLibrary>
dxc_get_library(void);

ComPtr<IDxcCompiler>
dxc_get_compiler(void);

PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE
d3d12_get_serialize_root_sig(void);

void
d3d12_enable_debug_layer();

void
d3d12_enable_gpu_validation();

ComPtr<ID3D12Device>
d3d12_create_device(IUnknown *adapter, bool experimental_features);

UINT
dzn_get_subresource_index(const D3D12_RESOURCE_DESC *desc,
                          VkImageAspectFlags aspectMask,
                          unsigned mipLevel, unsigned arrayLayer);

struct dzn_queue {
   struct vk_queue vk;
   struct dzn_device *device;

   ComPtr<ID3D12CommandQueue> cmdqueue;
   ComPtr<ID3D12Fence> fence;
   uint64_t fence_point = 0;

   dzn_queue(dzn_device *device,
             const VkDeviceQueueCreateInfo *pCreateInfo,
             const VkAllocationCallbacks *alloc);
   ~dzn_queue();
   const VkAllocationCallbacks *get_vk_allocator();
};

struct dzn_device {
   struct vk_device vk;

   struct dzn_instance *instance = NULL;
   struct dzn_physical_device *physical_device = NULL;

   dzn_object_unique_ptr<dzn_queue> queue;

   struct vk_device_extension_table enabled_extensions;
   struct vk_device_dispatch_table dispatch;

   ComPtr<ID3D12Device> dev;
   D3D12_FEATURE_DATA_ARCHITECTURE1 arch;

   dzn_device(VkPhysicalDevice pdev,
              const VkDeviceCreateInfo *pCreateInfo,
              const VkAllocationCallbacks *pAllocator);
   ~dzn_device();
   void alloc_rtv_handle(struct d3d12_descriptor_handle *handle);
   void alloc_dsv_handle(struct d3d12_descriptor_handle *handle);
   void free_handle(struct d3d12_descriptor_handle *handle);
private:
   std::mutex pools_lock;
   std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter> rtv_pool;
   std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter> dsv_pool;
};

struct dzn_device_memory {
   struct vk_object_base base;

   struct list_head link;

   ComPtr<ID3D12Heap> heap;
   VkDeviceSize size;
   D3D12_RESOURCE_STATES initial_state; /* initial state for this memory type */

   /* A buffer-resource spanning the entire heap, used for mapping memory */
   ComPtr<ID3D12Resource> map_res;

   VkDeviceSize map_size = 0;
   void *map = NULL;

   dzn_device_memory(dzn_device *device,
                     const VkMemoryAllocateInfo *pAllocateInfo,
                     const VkAllocationCallbacks *pAllocator);
   ~dzn_device_memory();
};

enum dzn_cmd_bindpoint_dirty {
   DZN_CMD_BINDPOINT_DIRTY_PIPELINE = 1 << 0,
   DZN_CMD_BINDPOINT_DIRTY_HEAPS = 1 << 1,
   DZN_CMD_BINDPOINT_DIRTY_SYSVALS = 1 << 2,
};

enum dzn_cmd_dirty {
   DZN_CMD_DIRTY_VIEWPORTS = 1 << 0,
   DZN_CMD_DIRTY_SCISSORS = 1 << 1,
   DZN_CMD_DIRTY_IB = 1 << 2,
};

#define MAX_VBS 16
#define MAX_VP 16
#define MAX_SCISSOR 16
#define MAX_SETS 4
#define MAX_PUSH_CONSTANT_DWORDS 32

#define NUM_BIND_POINT VK_PIPELINE_BIND_POINT_COMPUTE + 1
#define NUM_POOL_TYPES D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1

struct dzn_cmd_event_signal {
   struct dzn_event *event;
   bool value;
};

struct dzn_cmd_buffer;

struct dzn_batch {
   using wait_allocator = dzn_allocator<dzn_event *>;
   std::vector<dzn_event *, wait_allocator> wait;
   using signal_allocator = dzn_allocator<dzn_cmd_event_signal>;
   std::vector<dzn_cmd_event_signal, signal_allocator> signal;
   ComPtr<ID3D12GraphicsCommandList> cmdlist;
   struct dzn_cmd_pool *pool;

   dzn_batch(struct dzn_cmd_buffer *cmd_buffer);
   ~dzn_batch();
   const VkAllocationCallbacks *get_vk_allocator();
   static dzn_batch *create(struct dzn_cmd_buffer *cmd_buffer);
   static void destroy(dzn_batch *batch, struct dzn_cmd_buffer *cmd_buffer);
};

struct dzn_cmd_buffer {
   struct vk_command_buffer vk;

   dzn_device *device;

   std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter> rtv_pool;
   struct dzn_cmd_pool *pool;

   struct {
      struct dzn_framebuffer *framebuffer;
      const struct dzn_pipeline *pipeline;
      ComPtr<ID3D12DescriptorHeap> heaps[NUM_POOL_TYPES];
      struct dzn_render_pass *pass;
      struct {
         BITSET_DECLARE(dirty, MAX_VBS);
         D3D12_VERTEX_BUFFER_VIEW views[MAX_VBS];
      } vb;
      struct {
         D3D12_INDEX_BUFFER_VIEW view;
      } ib;
      D3D12_VIEWPORT viewports[MAX_VP];
      D3D12_RECT scissors[MAX_SCISSOR];
      struct {
         uint32_t offset;
         uint32_t end;
         uint32_t values[MAX_PUSH_CONSTANT_DWORDS];
         uint32_t stages;
      } push_constant;
      uint32_t dirty;
      uint32_t subpass;
      struct {
         const struct dzn_pipeline *pipeline;
         const struct dzn_descriptor_set *sets[MAX_SETS];
         ComPtr<ID3D12DescriptorHeap> heaps[NUM_POOL_TYPES];
         uint32_t dirty;
      } bindpoint[NUM_BIND_POINT];
      union {
         struct dxil_spirv_vertex_runtime_data gfx;
         struct dxil_spirv_compute_runtime_data compute;
      } sysvals;
   } state = {};

   using heaps_allocator = dzn_allocator<ComPtr<ID3D12DescriptorHeap>>;
   std::vector<ComPtr<ID3D12DescriptorHeap>, heaps_allocator> heaps;

   VkCommandBufferUsageFlags usage_flags;
   VkCommandBufferLevel level;

   ComPtr<ID3D12CommandAllocator> alloc;
   D3D12_COMMAND_LIST_TYPE type;
   dzn_object_unique_ptr<dzn_batch> batch;
   using batches_allocator = dzn_allocator<dzn_object_unique_ptr<dzn_batch>>;
   dzn_object_vector<dzn_batch> batches;
   ComPtr<ID3D12Resource> rt0;

   dzn_cmd_buffer(dzn_device *device,
                  dzn_cmd_pool *pool,
                  VkCommandBufferLevel lvl,
                  const VkAllocationCallbacks *pAllocator);
   ~dzn_cmd_buffer();
   void open_batch();
   void close_batch();
   dzn_batch *get_batch(bool signal_event = false);
   void reset();

   void draw(uint32_t vertex_count,
             uint32_t instance_count,
             uint32_t first_vertex,
             uint32_t first_instance);
   void draw(uint32_t index_count,
             uint32_t instance_count,
             uint32_t first_index,
             int32_t vertex_offset,
             uint32_t first_instance);
   void dispatch(uint32_t group_count_x,
                 uint32_t group_count_y,
                 uint32_t group_count_z);

private:
   void update_pipeline(uint32_t bindpoint);
   void update_heaps(uint32_t bindpoint);
   void update_viewports();
   void update_scissors();
   void update_vbviews();
   void update_ibview();
   void update_push_constants(uint32_t bindpoint);
   void update_sysvals(uint32_t bindpoint);
   void prepare_draw(bool indexed);
   void prepare_dispatch();
};

struct dzn_cmd_pool {
   struct vk_object_base base;
   VkAllocationCallbacks alloc;

   VkCommandPoolCreateFlags flags;

   dzn_cmd_pool(dzn_device *device,
                const VkCommandPoolCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator);
   ~dzn_cmd_pool();

   VkResult
   allocate_cmd_buffers(VkDevice device,
                        const VkCommandBufferAllocateInfo *pAllocateInfo,
                        VkCommandBuffer *pCommandBuffers);
   void
   free_cmd_buffers(VkDevice device,
                    uint32_t commandBufferCount,
                    const VkCommandBuffer *pCommandBuffers);
};

struct dzn_descriptor_pool {
   struct vk_object_base base;
   VkAllocationCallbacks alloc;

   VkResult
   allocate_sets(VkDevice device,
                 const VkDescriptorSetAllocateInfo *pAllocateInfo,
                 VkDescriptorSet *pDescriptorSets);
   VkResult
   free_sets(VkDevice device,
             uint32_t count,
             const VkDescriptorSet *pDescriptorSets);

   dzn_descriptor_pool(dzn_device *device,
                       const VkDescriptorPoolCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator);
   ~dzn_descriptor_pool();
};

#define MAX_SHADER_VISIBILITIES (D3D12_SHADER_VISIBILITY_PIXEL + 1)

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
   } ranges[MAX_SHADER_VISIBILITIES];
   uint32_t static_sampler_count;
   const D3D12_STATIC_SAMPLER_DESC *static_samplers;
   uint32_t view_desc_count, sampler_desc_count;
   uint32_t binding_count;
   const struct dzn_descriptor_set_layout_binding *bindings;

   dzn_descriptor_set_layout(dzn_device *device,
                             const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator);
   ~dzn_descriptor_set_layout();
};

struct dzn_descriptor_set_binding {
   D3D12_CPU_DESCRIPTOR_HANDLE views;
   D3D12_CPU_DESCRIPTOR_HANDLE samplers;
};

struct dzn_descriptor_set {
   struct vk_object_base base;
   ComPtr<ID3D12DescriptorHeap> heaps[NUM_POOL_TYPES];
   const struct dzn_descriptor_set_layout *layout;
   const struct dzn_descriptor_set_binding *bindings;

   dzn_descriptor_set(dzn_device *device,
                      dzn_descriptor_pool *pool,
                      VkDescriptorSetLayout layout,
                      const VkAllocationCallbacks *pAllocator);
   ~dzn_descriptor_set();
};

struct dzn_pipeline_layout {
   struct vk_object_base base;
   uint32_t heap_offsets[MAX_SETS][NUM_POOL_TYPES];
   uint32_t desc_count[NUM_POOL_TYPES];
   struct {
      uint32_t param_count;
      uint32_t sets_param_count;
      uint32_t sysval_cbv_param_idx;
      uint32_t push_constant_cbv_param_idx;
      D3D12_DESCRIPTOR_HEAP_TYPE type[MAX_SHADER_VISIBILITIES];
      ComPtr<ID3D12RootSignature> sig;
   } root;

   dzn_pipeline_layout(dzn_device *device,
                       const VkPipelineLayoutCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator);
   ~dzn_pipeline_layout();
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
   union {
      bool color;
      struct {
         bool depth;
         bool stencil;
      };
   } clear;
   D3D12_RESOURCE_STATES before, during, after;
};

struct dzn_render_pass {
   struct vk_object_base base;
   uint32_t attachment_count;
   struct dzn_attachment *attachments;
   uint32_t subpass_count;
   struct dzn_subpass *subpasses;

   dzn_render_pass(dzn_device *device,
                   const VkRenderPassCreateInfo2KHR *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator);
   ~dzn_render_pass();
};

struct dzn_pipeline_cache {
   struct vk_object_base base;
   dzn_pipeline_cache(dzn_device *device,
                      const VkPipelineCacheCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator);
   ~dzn_pipeline_cache();
};

enum dzn_register_space {
   DZN_REGISTER_SPACE_SYSVALS = MAX_SETS,
   DZN_REGISTER_SPACE_PUSH_CONSTANT,
};

class dzn_shader_blob : public IDxcBlob {
public:
   dzn_shader_blob(void *buf, size_t sz) : data(buf), size(sz) {}

   LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override { return data; }

   SIZE_T STDMETHODCALLTYPE GetBufferSize() override { return size; }

   HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }

   ULONG STDMETHODCALLTYPE AddRef() override { return 1; }

   ULONG STDMETHODCALLTYPE Release() override { return 0; }

   void *data;
   size_t size;
};

struct dzn_pipeline {
   struct vk_object_base base;
   VkPipelineBindPoint type;
   const dzn_pipeline_layout *layout = NULL;
   ComPtr<ID3D12PipelineState> state;

   dzn_pipeline(dzn_device *device, VkPipelineBindPoint type);
   ~dzn_pipeline();

   static VkResult compile_shader(dzn_device *device,
                                  const VkPipelineShaderStageCreateInfo *stage_info,
                                  bool apply_yflip,
                                  D3D12_SHADER_BYTECODE *slot);
};

struct dzn_graphics_pipeline {
   dzn_pipeline base;
   struct {
      unsigned count;
      uint32_t strides[MAX_VBS];
   } vb = {};

   struct {
      D3D_PRIMITIVE_TOPOLOGY topology;
   } ia = {};

   struct {
      unsigned count;
      bool dynamic;
      D3D12_VIEWPORT desc[MAX_VP];
   } vp = {};

   struct {
      unsigned count;
      bool dynamic;
      D3D12_RECT desc[MAX_SCISSOR];
   } scissor = {};

   struct {
      uint8_t stencil_ref;
   } zsa = {};

   dzn_graphics_pipeline(dzn_device *device,
                         VkPipelineCache cache,
                         const VkGraphicsPipelineCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator);
   ~dzn_graphics_pipeline();

private:
   VkResult translate_vi(D3D12_GRAPHICS_PIPELINE_STATE_DESC &out,
                         const VkGraphicsPipelineCreateInfo *in,
                         dzn_transient_object<D3D12_INPUT_ELEMENT_DESC> &inputs);
   void translate_ia(D3D12_GRAPHICS_PIPELINE_STATE_DESC &out,
                     const VkGraphicsPipelineCreateInfo *in);
   void translate_rast(D3D12_GRAPHICS_PIPELINE_STATE_DESC &out,
                       const VkGraphicsPipelineCreateInfo *in);
   void translate_ms(D3D12_GRAPHICS_PIPELINE_STATE_DESC &out,
                     const VkGraphicsPipelineCreateInfo *in);
   void translate_zsa(D3D12_GRAPHICS_PIPELINE_STATE_DESC &out,
                      const VkGraphicsPipelineCreateInfo *in);
   void translate_blend(D3D12_GRAPHICS_PIPELINE_STATE_DESC &out,
                        const VkGraphicsPipelineCreateInfo *in);
};

struct dzn_compute_pipeline {
   dzn_pipeline base;
   struct {
      uint32_t x, y, z;
   } local_size;

   dzn_compute_pipeline(dzn_device *device,
                        VkPipelineCache cache,
                        const VkComputePipelineCreateInfo *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator);
   ~dzn_compute_pipeline();
};

#define MAX_MIP_LEVELS 14

struct dzn_image {
   struct vk_image vk;

   struct {
      uint32_t row_stride = 0;
      uint32_t size = 0;
   } linear;
   D3D12_RESOURCE_DESC desc = {};
   ComPtr<ID3D12Resource> res;
   dzn_device_memory *mem = NULL;

   dzn_image(dzn_device *device,
             const VkImageCreateInfo *pCreateInfo,
             const VkAllocationCallbacks *alloc);
   ~dzn_image();
};

struct dzn_image_view {
   struct vk_object_base base;

   dzn_device *device;
   const dzn_image *image;

   VkFormat vk_format;
   VkExtent3D extent;

   D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

   struct d3d12_descriptor_handle rt_handle = {};
   struct d3d12_descriptor_handle zs_handle = {};

   dzn_image_view(dzn_device *device,
                  const VkImageViewCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator);
   ~dzn_image_view();
};

struct dzn_buffer {
   struct vk_object_base base;

   VkDeviceSize size;

   D3D12_RESOURCE_DESC desc;
   ComPtr<ID3D12Resource> res;

   VkBufferCreateFlags create_flags;
   VkBufferUsageFlags usage;

   dzn_buffer(dzn_device *device,
              const VkBufferCreateInfo *pCreateInfo,
              const VkAllocationCallbacks *pAllocator);
   ~dzn_buffer();
};

struct dzn_buffer_view {
   struct vk_object_base base;

   const dzn_buffer *buffer;

   D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
   dzn_buffer_view(dzn_device *device,
                   const VkBufferViewCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator);
   ~dzn_buffer_view();
};

struct dzn_framebuffer {
   struct vk_object_base base;

   uint32_t width, height, layers;

   uint32_t attachment_count;
   struct dzn_image_view *attachments[0];

   dzn_framebuffer(dzn_device *device,
                   const VkFramebufferCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator);
   ~dzn_framebuffer();
};

struct dzn_sampler {
   struct vk_object_base base;
   D3D12_SAMPLER_DESC desc;

   dzn_sampler(dzn_device *device,
               const VkSamplerCreateInfo *pCreateInfo,
               const VkAllocationCallbacks *pAllocator);
   ~dzn_sampler();
};

/* This is defined as a macro so that it works for both
 * VkImageSubresourceRange and VkImageSubresourceLayers
 */
#define dzn_get_layerCount(_image, _range) \
   ((_range)->layerCount == VK_REMAINING_ARRAY_LAYERS ? \
    (_image)->vk.array_layers - (_range)->baseArrayLayer : (_range)->layerCount)


#ifdef __cplusplus
extern "C" {
#endif
DXGI_FORMAT dzn_pipe_to_dxgi_format(enum pipe_format in);
D3D12_FILTER dzn_translate_sampler_filter(const VkSamplerCreateInfo *create_info);
D3D12_COMPARISON_FUNC dzn_translate_compare_op(VkCompareOp in);
void dzn_translate_viewport(D3D12_VIEWPORT *out, const VkViewport *in);
void dzn_translate_scissor(D3D12_RECT *out, const VkRect2D *in);
#ifdef __cplusplus
}
#endif

VkResult dzn_wsi_init(struct dzn_physical_device *physical_device);
void dzn_wsi_finish(struct dzn_physical_device *physical_device);

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
   DZN_DEBUG_INTERNAL = 1 << 4,
};

struct dzn_instance {
   struct vk_instance vk;

   struct {
      ComPtr<IDxcValidator> validator;
      ComPtr<IDxcLibrary> library;
      ComPtr<IDxcCompiler> compiler;
   } dxc;
   struct {
      PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE serialize_root_sig;
   } d3d12;
   bool physical_devices_enumerated;
   uint32_t debug_flags;

   dzn_instance(const VkInstanceCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator);
   ~dzn_instance();
   VkResult enumerate_physical_devices(uint32_t *pPhysicalDeviceCount,
                                       VkPhysicalDevice *pPhysicalDevices);
private:
   using physical_devices_allocator = dzn_allocator<dzn_object_unique_ptr<dzn_physical_device>>;
   dzn_object_vector<dzn_physical_device> physical_devices;
};

struct dzn_semaphore {
   struct vk_object_base base;

   dzn_semaphore(dzn_device *device,
                 const VkSemaphoreCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *pAllocator);
   ~dzn_semaphore();
};

struct dzn_fence {
   struct vk_object_base base;

   ComPtr<ID3D12Fence> fence;
   HANDLE event;

   dzn_fence(dzn_device *device,
             const VkFenceCreateInfo *pCreateInfo,
             const VkAllocationCallbacks *pAllocator);
   ~dzn_fence();
};

struct dzn_event {
   struct vk_object_base base;

   ComPtr<ID3D12Fence> fence;

   dzn_event(dzn_device *device,
             const VkEventCreateInfo *pCreateInfo,
             const VkAllocationCallbacks *pAllocator);
   ~dzn_event();
};

struct dzn_shader_module {
   struct vk_object_base base;
   uint32_t code_size;
   uint32_t code[0];

   dzn_shader_module(dzn_device *device,
                     const VkShaderModuleCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator);
   ~dzn_shader_module();
};

struct dzn_query_pool {
   struct vk_object_base base;
};

DXGI_FORMAT
dzn_get_format(VkFormat format);

D3D12_RESOURCE_STATES
dzn_get_states(VkImageLayout layout);

VK_DEFINE_HANDLE_CASTS(dzn_cmd_buffer, vk.base, VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER)
VK_DEFINE_HANDLE_CASTS(dzn_device, vk.base, VkDevice, VK_OBJECT_TYPE_DEVICE)
VK_DEFINE_HANDLE_CASTS(dzn_instance, vk.base, VkInstance, VK_OBJECT_TYPE_INSTANCE)
VK_DEFINE_HANDLE_CASTS(dzn_physical_device, vk.base, VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE)
VK_DEFINE_HANDLE_CASTS(dzn_queue, vk.base, VkQueue, VK_OBJECT_TYPE_QUEUE)

VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_cmd_pool, base, VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_buffer, base, VkBuffer, VK_OBJECT_TYPE_BUFFER)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_buffer_view, base, VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_device_memory, base, VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_descriptor_pool, base, VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_descriptor_set, base, VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_descriptor_set_layout, base, VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_event, base, VkEvent, VK_OBJECT_TYPE_EVENT)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_fence, base, VkFence, VK_OBJECT_TYPE_FENCE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_framebuffer, base, VkFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_image, vk.base, VkImage, VK_OBJECT_TYPE_IMAGE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_image_view, base, VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline, base, VkPipeline, VK_OBJECT_TYPE_PIPELINE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_graphics_pipeline, base.base, VkPipeline, VK_OBJECT_TYPE_PIPELINE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_compute_pipeline, base.base, VkPipeline, VK_OBJECT_TYPE_PIPELINE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline_cache, base, VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline_layout, base, VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_query_pool, base, VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_render_pass, base, VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_sampler, base, VkSampler, VK_OBJECT_TYPE_SAMPLER)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_semaphore, base, VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_shader_module, base, VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE)

template <typename DT, typename VH, typename Conv, typename... CreateArgs>
class dzn_object_factory {
public:
   static VkResult
   create(CreateArgs... args,
          const VkAllocationCallbacks *alloc,
          DT **obj_ptr)
   {
      DT *obj = allocate(std::forward<CreateArgs>(args)..., alloc);
      if (obj == NULL)
         return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

      try {
         std::construct_at(obj, std::forward<CreateArgs>(args)..., alloc);
         *obj_ptr = obj;
      } catch (VkResult error) {
         deallocate(obj, alloc);
         return error;
      }

      return VK_SUCCESS;
   }

   static VkResult
   create(CreateArgs... args,
          const VkAllocationCallbacks *alloc,
          VH *handle)
   {
      DT *obj = NULL;
      VkResult result =
         create(std::forward<CreateArgs>(args)..., alloc, &obj);

      if (result == VK_SUCCESS)
         *handle = Conv::to_handle(obj);

      return VK_SUCCESS;
   }

   static void
   destroy(DT *obj,
           const VkAllocationCallbacks *alloc)
   {
      if (obj) {
         std::destroy_at(obj);
         deallocate(obj, alloc);
      }
   }

   static void
   destroy(VH handle,
           const VkAllocationCallbacks *alloc)
   {
      destroy(Conv::from_handle(handle), alloc);
   }

private:
   static DT *
   allocate(CreateArgs... args,
            const VkAllocationCallbacks *allocator)
   {
      return (DT *)vk_zalloc(vk_default_allocator(), allocator,
                             sizeof(DT), 8,
                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   }

   static void
   deallocate(DT *obj,
              const VkAllocationCallbacks *alloc)
   {
      vk_free2(vk_default_allocator(), alloc, obj);
   }
};

template <typename DT, typename VH, typename Conv, typename... CreateArgs>
class dzn_object_factory<DT, VH, Conv, VkDevice, CreateArgs...> {
public:
   static VkResult
   create(dzn_device *device,
          CreateArgs... args,
          const VkAllocationCallbacks *alloc,
          DT **obj_ptr)
   {
      DT *obj = allocate(device, std::forward<CreateArgs>(args)..., alloc);
      if (obj == NULL)
         return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

      try {
         std::construct_at(obj, device, std::forward<CreateArgs>(args)..., alloc);
         *obj_ptr = obj;
      } catch (VkResult error) {
         deallocate(device, obj, alloc);
         return error;
      }

      return VK_SUCCESS;
   }

   static VkResult
   create(VkDevice dev,
          CreateArgs... args,
          const VkAllocationCallbacks *alloc,
          VH *handle)
   {
      VK_FROM_HANDLE(dzn_device, device, dev);
      DT *obj = NULL;
      VkResult result =
         create(device, std::forward<CreateArgs>(args)..., alloc, &obj);

      if (result == VK_SUCCESS)
         *handle = Conv::to_handle(obj);

      return result;
   }

   static void
   destroy(dzn_device *dev,
           DT *obj,
           const VkAllocationCallbacks *alloc)
   {
      if (obj) {
         std::destroy_at(obj);
         deallocate(dev, obj, alloc);
      }
   }

   static void
   destroy(VkDevice dev,
           VH handle,
           const VkAllocationCallbacks *alloc)
   {
      VK_FROM_HANDLE(dzn_device, device, dev);
      DT *obj = Conv::from_handle(handle);

      destroy(device, obj, alloc);
   }

private:
   static DT *
   allocate(dzn_device *device, CreateArgs... args,
            const VkAllocationCallbacks *allocator)
   {
      return (DT *)vk_alloc2(&device->vk.alloc, allocator,
                             sizeof(DT), alignof(DT),
                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   }

   static void
   deallocate(dzn_device *device, DT *obj,
              const VkAllocationCallbacks *alloc)
   {
      vk_free2(&device->vk.alloc, alloc, obj);
   }
};

#define DZN_OBJ_FACTORY(__drv_type, __VkType, ...) \
class __drv_type ## _conv \
{ \
public: \
   static __drv_type *from_handle(__VkType handle) \
   { \
      return __drv_type ## _from_handle(handle); \
   } \
   static __VkType to_handle(__drv_type *obj) \
   { \
      return __drv_type ## _to_handle(obj); \
   } \
}; \
\
typedef dzn_object_factory<__drv_type, __VkType, __drv_type ## _conv, __VA_ARGS__> \
        __drv_type ## _factory

DZN_OBJ_FACTORY(dzn_buffer, VkBuffer, VkDevice, const VkBufferCreateInfo *);
DZN_OBJ_FACTORY(dzn_buffer_view, VkBufferView, VkDevice, const VkBufferViewCreateInfo *);
DZN_OBJ_FACTORY(dzn_cmd_buffer, VkCommandBuffer, VkDevice, dzn_cmd_pool *, VkCommandBufferLevel);
DZN_OBJ_FACTORY(dzn_cmd_pool, VkCommandPool, VkDevice, const VkCommandPoolCreateInfo *);
DZN_OBJ_FACTORY(dzn_compute_pipeline, VkPipeline, VkDevice, VkPipelineCache, const VkComputePipelineCreateInfo *);
DZN_OBJ_FACTORY(dzn_descriptor_pool, VkDescriptorPool, VkDevice, const VkDescriptorPoolCreateInfo *);
DZN_OBJ_FACTORY(dzn_descriptor_set, VkDescriptorSet, VkDevice, dzn_descriptor_pool *, VkDescriptorSetLayout);
DZN_OBJ_FACTORY(dzn_descriptor_set_layout, VkDescriptorSetLayout, VkDevice, const VkDescriptorSetLayoutCreateInfo *);
DZN_OBJ_FACTORY(dzn_device, VkDevice, VkPhysicalDevice, const VkDeviceCreateInfo *);
DZN_OBJ_FACTORY(dzn_device_memory, VkDeviceMemory, VkDevice, const VkMemoryAllocateInfo *);
DZN_OBJ_FACTORY(dzn_event, VkEvent, VkDevice, const VkEventCreateInfo *);
DZN_OBJ_FACTORY(dzn_fence, VkFence, VkDevice, const VkFenceCreateInfo *);
DZN_OBJ_FACTORY(dzn_framebuffer, VkFramebuffer, VkDevice, const VkFramebufferCreateInfo *);
DZN_OBJ_FACTORY(dzn_graphics_pipeline, VkPipeline, VkDevice, VkPipelineCache, const VkGraphicsPipelineCreateInfo *);
DZN_OBJ_FACTORY(dzn_image, VkImage, VkDevice, const VkImageCreateInfo *);
DZN_OBJ_FACTORY(dzn_image_view, VkImageView, VkDevice, const VkImageViewCreateInfo *);
DZN_OBJ_FACTORY(dzn_instance, VkInstance, const VkInstanceCreateInfo *);
DZN_OBJ_FACTORY(dzn_physical_device, VkPhysicalDevice, dzn_instance *, ComPtr<IDXGIAdapter1> &);
DZN_OBJ_FACTORY(dzn_pipeline_cache, VkPipelineCache, VkDevice, const VkPipelineCacheCreateInfo *);
DZN_OBJ_FACTORY(dzn_pipeline_layout, VkPipelineLayout, VkDevice, const VkPipelineLayoutCreateInfo *);
DZN_OBJ_FACTORY(dzn_queue, VkQueue, VkDevice, const VkDeviceQueueCreateInfo *);
DZN_OBJ_FACTORY(dzn_render_pass, VkRenderPass, VkDevice, const VkRenderPassCreateInfo2KHR *);
DZN_OBJ_FACTORY(dzn_sampler, VkSampler, VkDevice, const VkSamplerCreateInfo *);
DZN_OBJ_FACTORY(dzn_semaphore, VkSemaphore, VkDevice, const VkSemaphoreCreateInfo *);
DZN_OBJ_FACTORY(dzn_shader_module, VkShaderModule, VkDevice, const VkShaderModuleCreateInfo *);

#endif /* DZN_PRIVATE_H */
