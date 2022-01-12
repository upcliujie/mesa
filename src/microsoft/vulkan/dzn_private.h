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
#include "vk_shader_module.h"
#include "wsi_common.h"

#include "util/bitset.h"
#include "util/blob.h"
#include "util/u_dynarray.h"
#include "util/log.h"

#include "shader_enums.h"

#include "dzn_entrypoints.h"
#include "dzn_nir.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_icd.h>

#include <dxgi1_4.h>

#define D3D12_IGNORE_SDK_LAYERS
#include <directx/d3d12.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include "spirv_to_dxil.h"
#include "d3d12_descriptor_pool.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

#define dzn_stub() unreachable("Unsupported feature")

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
      T *obj = (T *)vk_alloc(&allocator, sizeof(T) * n, alignof(T), scope);
      if (!obj)
         throw vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

      return obj;
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
   if (!ptr)
      throw vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   try {
      return dzn_transient_object<T>(ptr, deleter);
   } catch (...) {
      vk_free2(parent_alloc, alloc, ptr);
      throw;
   }
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
   if (!ptr)
      throw vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   try {
      return dzn_transient_object<T>(ptr, deleter);
   } catch (...) {
      vk_free2(parent_alloc, alloc, ptr);
      throw;
   }
}

template <typename T, typename... CreateArgs>
dzn_object_unique_ptr<T>
dzn_private_object_create(const VkAllocationCallbacks *parent_alloc,
                          CreateArgs... args)
{
   T *obj = (T *)
      vk_alloc(parent_alloc, sizeof(T), alignof(T),
               VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (!obj)
      throw vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   try {
      std::construct_at(obj, std::forward<CreateArgs>(args)...);
   } catch (...) {
      vk_free(parent_alloc, obj);
      throw;
   }
   return dzn_object_unique_ptr<T>(obj);
}

struct dzn_meta {
   dzn_meta(struct dzn_device *device);
   ~dzn_meta() = default;

   const VkAllocationCallbacks *get_vk_allocator();
   static void
   compile_shader(struct dzn_device *pdev,
                  nir_shader *nir,
                  D3D12_SHADER_BYTECODE *slot);

   struct dzn_device *device;
   ComPtr<ID3D12RootSignature> root_sig;
   ComPtr<ID3D12PipelineState> pipeline_state;
};

struct dzn_meta_indirect_draw : public dzn_meta {
   dzn_meta_indirect_draw(struct dzn_device *device,
                          enum dzn_indirect_draw_type type);
   ~dzn_meta_indirect_draw() = default;
};

struct dzn_meta_triangle_fan_rewrite_index : public dzn_meta {
   enum index_type {
      NO_INDEX,
      INDEX_2B,
      INDEX_4B,
      NUM_INDEX_TYPE,
   };

   dzn_meta_triangle_fan_rewrite_index(struct dzn_device *device,
                                       enum index_type old_index_type);
   ~dzn_meta_triangle_fan_rewrite_index() = default;

   static index_type get_index_type(uint8_t index_size);
   static index_type get_index_type(DXGI_FORMAT format);
   static uint8_t get_index_size(enum index_type type);

   ID3D12CommandSignature *get_indirect_cmd_sig();

   ComPtr<ID3D12CommandSignature> cmd_sig;
};

struct dzn_meta_blit : public dzn_meta {
   struct key {
      union {
         struct {
           DXGI_FORMAT out_format;
           uint32_t samples : 6;
           uint32_t loc : 4;
           uint32_t out_type : 4;
           uint32_t sampler_dim : 4;
           uint32_t src_is_array : 1;
           uint32_t resolve : 1;
           uint32_t linear_filter : 1;
           uint32_t padding : 11;
         };
         const uint64_t u64;
      };
   };

   struct shader {
      shader() = default;
      shader(struct dzn_device *dev);
      shader(struct dzn_device *dev, const D3D12_SHADER_BYTECODE &in);
      shader(const shader &src) = delete;
      ~shader();

      const VkAllocationCallbacks *get_vk_allocator();

      struct dzn_device *device = NULL;
      D3D12_SHADER_BYTECODE code = {};
   };

   dzn_meta_blit(struct dzn_device *device,
                 const key &key);
   ~dzn_meta_blit() = default;
};

struct dzn_meta_blits {
   dzn_meta_blits(struct dzn_device *dev);
   ~dzn_meta_blits() = default;

   const VkAllocationCallbacks *get_vk_allocator();

   const dzn_meta_blit::shader *get_vs();
   const dzn_meta_blit::shader *get_fs(const struct dzn_nir_blit_info &info);
   const dzn_meta_blit *get_context(const dzn_meta_blit::key &key);

   struct dzn_device *device;
   std::mutex shaders_lock;
   dzn_object_unique_ptr<dzn_meta_blit::shader> vs;
   using fs_allocator = dzn_allocator<std::pair<const uint32_t, dzn_object_unique_ptr<dzn_meta_blit::shader>>>;
   std::unordered_map<uint32_t, dzn_object_unique_ptr<dzn_meta_blit::shader>,
                      std::hash<uint32_t>, std::equal_to<uint32_t>, fs_allocator> fs;
   std::mutex contexts_lock;
   using contexts_allocator = dzn_allocator<std::pair<const uint64_t, dzn_object_unique_ptr<dzn_meta_blit>>>;
   std::unordered_map<uint64_t, dzn_object_unique_ptr<dzn_meta_blit>,
                      std::hash<uint64_t>, std::equal_to<uint64_t>, contexts_allocator> contexts;
};

struct dzn_physical_device {
   struct vk_physical_device vk;

   struct dzn_instance *instance;

   struct vk_device_extension_table supported_extensions;
   struct vk_physical_device_dispatch_table dispatch;

   const ComPtr<IDXGIAdapter1> adapter;
   const DXGI_ADAPTER_DESC1 adapter_desc;

   uint8_t pipeline_cache_uuid[VK_UUID_SIZE];
   uint8_t device_uuid[VK_UUID_SIZE];
   uint8_t driver_uuid[VK_UUID_SIZE];

   struct wsi_device wsi_device;

   dzn_physical_device(dzn_instance *instance,
                       ComPtr<IDXGIAdapter1> &adapter,
                       const DXGI_ADAPTER_DESC1 &adapter_desc,
                       const VkAllocationCallbacks *alloc);
   ~dzn_physical_device();
   const VkAllocationCallbacks *get_vk_allocator();
   ID3D12Device *get_d3d12_dev();

   const D3D12_FEATURE_DATA_ARCHITECTURE1 &get_arch_caps() const;
   const VkPhysicalDeviceMemoryProperties &get_memory() const;

   D3D12_HEAP_FLAGS get_heap_flags_for_mem_type(uint32_t mem_type) const;
   uint32_t get_mem_type_mask_for_resource(const D3D12_RESOURCE_DESC &desc) const;

   D3D12_FEATURE_DATA_FORMAT_SUPPORT get_format_support(VkFormat format);
   void get_format_properties(VkFormat format,
                              VkFormatProperties *pFormatProperties);
   void get_format_properties(VkFormat format,
                              VkFormatProperties2 *pFormatProperties);
   VkResult get_image_format_properties(const VkPhysicalDeviceImageFormatInfo2 *info,
                                        VkImageFormatProperties2 *properties);
   bool supports_bc();

private:
   void get_device_extensions();
   void cache_caps(std::lock_guard<std::mutex>&);
   void init_memory(std::lock_guard<std::mutex>&);
   bool supports_compressed_format(const VkFormat *formats, uint32_t format_count);
   uint32_t get_max_array_layers();
   uint32_t get_max_mip_levels(bool is_3d);
   uint32_t get_max_extent(bool is_3d);

   std::mutex dev_lock;
   ComPtr<ID3D12Device> dev;
   D3D_FEATURE_LEVEL feature_level = (D3D_FEATURE_LEVEL)0;
   D3D12_FEATURE_DATA_ARCHITECTURE1 architecture = {};
   D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
   VkPhysicalDeviceMemoryProperties memory = {};
   D3D12_HEAP_FLAGS heap_flags_for_mem_type[VK_MAX_MEMORY_TYPES] = {};
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

   ID3D12Device *dev;

   dzn_object_unique_ptr<dzn_meta_indirect_draw> indirect_draws[DZN_NUM_INDIRECT_DRAW_TYPES];
   dzn_object_unique_ptr<dzn_meta_triangle_fan_rewrite_index> triangle_fan[dzn_meta_triangle_fan_rewrite_index::NUM_INDEX_TYPE];
   dzn_object_unique_ptr<dzn_meta_blits> blits;

   struct {
      static const uint32_t refs_all_ones_offset = 0;
      static const uint32_t refs_all_zeros_offset = sizeof(uint64_t);
      ComPtr<ID3D12Resource> refs;
   } queries;

   dzn_device(VkPhysicalDevice pdev,
              const VkDeviceCreateInfo *pCreateInfo,
              const VkAllocationCallbacks *pAllocator);
   ~dzn_device();
   void alloc_rtv_handle(struct d3d12_descriptor_handle *handle);
   void alloc_dsv_handle(struct d3d12_descriptor_handle *handle);
   void free_handle(struct d3d12_descriptor_handle *handle);
   ComPtr<ID3D12RootSignature>
   create_root_sig(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc);
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
#define MAX_DYNAMIC_UNIFORM_BUFFERS 8
#define MAX_DYNAMIC_STORAGE_BUFFERS 4
#define MAX_DYNAMIC_BUFFERS                                                  \
   (MAX_DYNAMIC_UNIFORM_BUFFERS + MAX_DYNAMIC_STORAGE_BUFFERS)
#define MAX_PUSH_CONSTANT_DWORDS 32

#define NUM_BIND_POINT VK_PIPELINE_BIND_POINT_COMPUTE + 1
#define NUM_POOL_TYPES D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1

#define dzn_foreach_pool_type(type) \
   for (D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; \
        type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; \
        type = (D3D12_DESCRIPTOR_HEAP_TYPE)(type + 1))

struct dzn_cmd_event_signal {
   struct dzn_event *event;
   bool value;
};

struct dzn_cmd_buffer;

struct dzn_attachment {
   uint32_t idx;
   VkFormat format;
   uint32_t samples;
   union {
      bool color;
      struct {
         bool depth;
         bool stencil;
      };
   } clear;
   D3D12_RESOURCE_STATES before, last, after;
};

struct dzn_attachment_ref {
   uint32_t idx;
   D3D12_RESOURCE_STATES before, during;
};

struct dzn_batch_query_op {
   struct dzn_query_pool *qpool;
   uint32_t query;
   bool wait;
   bool reset;
   bool signal;
};

struct dzn_batch {
   using wait_allocator = dzn_allocator<dzn_event *>;
   std::vector<dzn_event *, wait_allocator> wait;
   using signal_allocator = dzn_allocator<dzn_cmd_event_signal>;
   std::vector<dzn_cmd_event_signal, signal_allocator> signal;
   using queries_allocator = dzn_allocator<dzn_batch_query_op>;
   std::vector<dzn_batch_query_op, queries_allocator> queries;
   ComPtr<ID3D12GraphicsCommandList1> cmdlist;
   struct dzn_cmd_pool *pool;

   dzn_batch(struct dzn_cmd_buffer *cmd_buffer);
   ~dzn_batch();
   const VkAllocationCallbacks *get_vk_allocator();
   static dzn_batch *create(struct dzn_cmd_buffer *cmd_buffer);
   static void destroy(dzn_batch *batch, struct dzn_cmd_buffer *cmd_buffer);
};

struct dzn_descriptor_state {
   struct {
      const struct dzn_descriptor_set *set;
      uint32_t dynamic_offsets[MAX_DYNAMIC_BUFFERS];
   } sets[MAX_SETS];
   ID3D12DescriptorHeap *heaps[NUM_POOL_TYPES];
};

struct dzn_sampler;
struct dzn_image_view;

struct dzn_buffer_desc {
   dzn_buffer_desc(VkDescriptorType t,
                   const VkDescriptorBufferInfo *pBufferInfo);
   dzn_buffer_desc() = default;
   dzn_buffer_desc(const dzn_buffer_desc &) = default;
   ~dzn_buffer_desc() = default;

   dzn_buffer_desc operator+(VkDeviceSize dyn_offset)
   {
      dzn_buffer_desc new_desc(*this);

      new_desc.offset = offset + dyn_offset;
      return new_desc;
   }

   VkDescriptorType type = (VkDescriptorType)0;
   const struct dzn_buffer *buffer = NULL;
   VkDeviceSize range = 0;
   VkDeviceSize offset = 0;
};

struct dzn_descriptor_heap {
   dzn_descriptor_heap(struct dzn_device *device,
                       uint32_t type,
                       uint32_t desc_count,
                       bool shader_visible);
   dzn_descriptor_heap() = default;
   dzn_descriptor_heap(const dzn_descriptor_heap &) = default;
   ~dzn_descriptor_heap() = default;

   operator ID3D12DescriptorHeap *();

   const VkAllocationCallbacks *get_vk_allocator();

   D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(uint32_t desc_offset = 0) const;
   D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(uint32_t desc_offset = 0) const;

   void write_desc(uint32_t desc_offset,
                   dzn_sampler *sampler);
   void write_desc(uint32_t desc_offset,
                   bool writeable,
                   dzn_image_view *iview);
   void write_desc(uint32_t desc_offset,
                   bool writeable,
                   struct dzn_buffer_view *bview);
   void write_desc(uint32_t desc_offset,
                   bool writeable,
                   const dzn_buffer_desc &info);
   void copy(uint32_t dst_offset,
             const dzn_descriptor_heap &src_heap,
             uint32_t src_offset,
             uint32_t desc_count);

   static bool type_depends_on_shader_usage(VkDescriptorType type);

private:
   dzn_device *device = NULL;
   ComPtr<ID3D12DescriptorHeap> heap;
   D3D12_DESCRIPTOR_HEAP_TYPE type = (D3D12_DESCRIPTOR_HEAP_TYPE)0;
   SIZE_T cpu_base = 0;
   uint64_t gpu_base = 0;
   uint32_t desc_count = 0;
   uint32_t desc_sz = 0;
};

struct dzn_query_key {
   struct dzn_query_pool *qpool;
   uint32_t query;

   bool operator<(const dzn_query_key &b) const
   {
      return qpool < b.qpool ||
             (qpool == b.qpool && query < b.query);
   }
};

struct dzn_query_state {
   bool wait = false;
   bool reset = false;
   bool collect = false;
   bool collected = false;
   enum status {
      UNKNOWN,
      INVALID,
      STARTED,
      STOPPED,
   } status = UNKNOWN;
};

struct dzn_cmd_buffer {
   struct vk_command_buffer vk;
   VkResult error;

   dzn_device *device;

   std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter> rtv_pool;
   std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter> dsv_pool;
   struct dzn_cmd_pool *pool;
   uint32_t index;
   using bufs_allocator = dzn_allocator<ComPtr<ID3D12Resource>>;
   std::vector<ComPtr<ID3D12Resource>, bufs_allocator> internal_bufs;

   struct {
      struct dzn_framebuffer *framebuffer;
      D3D12_RECT render_area;
      const struct dzn_pipeline *pipeline;
      ID3D12DescriptorHeap *heaps[NUM_POOL_TYPES];
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
         struct dzn_pipeline *pipeline;
         struct dzn_descriptor_state desc_state;
         uint32_t dirty;
      } bindpoint[NUM_BIND_POINT];
      union {
         struct dxil_spirv_vertex_runtime_data gfx;
         struct dxil_spirv_compute_runtime_data compute;
      } sysvals;
   } state = {};

   using queries_allocator = dzn_allocator<std::pair<const dzn_query_key, dzn_query_state>>;
   using queries_iterator = std::map<dzn_query_key, dzn_query_state, std::less<dzn_query_key>, queries_allocator>::iterator;
   std::map<dzn_query_key, dzn_query_state, std::less<dzn_query_key>, queries_allocator> queries;

   using heaps_allocator = dzn_allocator<dzn_descriptor_heap>;
   std::vector<dzn_descriptor_heap, heaps_allocator> heaps;

   VkCommandBufferUsageFlags usage_flags;
   VkCommandBufferLevel level;

   ComPtr<ID3D12CommandAllocator> alloc;
   D3D12_COMMAND_LIST_TYPE type;
   dzn_object_unique_ptr<dzn_batch> batch;
   using batches_allocator = dzn_allocator<dzn_object_unique_ptr<dzn_batch>>;
   dzn_object_vector<dzn_batch> batches;

   dzn_cmd_buffer(dzn_device *device,
                  dzn_cmd_pool *pool,
                  VkCommandBufferLevel lvl,
                  const VkAllocationCallbacks *pAllocator);
   ~dzn_cmd_buffer();
   void open_batch();
   void close_batch();
   dzn_batch *get_batch(bool signal_event = false);
   void reset();
   const VkAllocationCallbacks *get_vk_allocator();

   VkResult begin(const VkCommandBufferBeginInfo *info);
   VkResult end();

   void begin_pass(const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                   const VkSubpassBeginInfoKHR *pSubpassBeginInfo);
   void end_pass(const VkSubpassEndInfoKHR *pSubpassEndInfo);
   void next_subpass();

   void bind_pipeline(VkPipelineBindPoint bind_point,
                      dzn_pipeline *pipeline);

   void bind_descriptor_sets(VkPipelineBindPoint bind_point,
                             const struct dzn_pipeline_layout *layout,
                             uint32_t first_set,
                             uint32_t descriptor_set_count,
                             const VkDescriptorSet *sets,
                             uint32_t dynamic_offset_count,
                             const uint32_t *dynamic_offsets);

   void set_viewport(uint32_t first_viewport,
                     uint32_t viewport_count,
                     const VkViewport *viewports);
   void set_scissor(uint32_t first_scissor,
                    uint32_t scissor_count,
                    const VkRect2D *scissors);

   void push_constants(VkShaderStageFlags stages,
                       uint32_t offset, uint32_t size,
                       const void *values);

   void bind_vertex_buffers(uint32_t first_binding,
                            uint32_t binding_count,
                            const VkBuffer *buffers,
                            const VkDeviceSize *offsets);
   void bind_index_buffer(VkBuffer buffer,
                          VkDeviceSize offset,
                          VkIndexType index_type);

   void reset_event(VkEvent event,
                    VkPipelineStageFlags stage_mask);
   void set_event(VkEvent event,
                  VkPipelineStageFlags stage_mask);
   void wait_events(uint32_t event_count,
                    const VkEvent *events,
                    VkPipelineStageFlags src_stage_mask,
                    VkPipelineStageFlags dst_stage_mask,
                    uint32_t memory_barrier_Count,
                    const VkMemoryBarrier *memory_barriers,
                    uint32_t buffer_memory_barrier_count,
                    const VkBufferMemoryBarrier *buffer_memory_barriers,
                    uint32_t image_memory_barrier_count,
                    const VkImageMemoryBarrier *image_memory_barriers);

   void begin_query(VkQueryPool query_pool, uint32_t query,
                    VkQueryControlFlags flags);
   void end_query(VkQueryPool query_pool, uint32_t query);
   void reset_query_pool(VkQueryPool query_pool,
                         uint32_t first_query,
                         uint32_t query_count);
   void copy_query_pool_results(VkQueryPool query_pool,
                                uint32_t first_query,
                                uint32_t query_count,
                                VkBuffer buffer,
                                VkDeviceSize offset,
                                VkDeviceSize stride,
                                VkQueryResultFlags flags);

   void set_line_width(float line_width);
   void set_depth_bias(float depth_bias_constant_factor,
                       float depth_bias_clamp,
                       float depth_bias_slope_factor);
   void set_blend_constants(const float blend_constants[4]);
   void set_depth_bounds(float min_depth_bounds, float max_depth_bounds);
   void set_stencil_compare_mask(VkStencilFaceFlags face_mask, uint32_t compare_mask);
   void set_stencil_write_mask(VkStencilFaceFlags face_mask, uint32_t write_mask);
   void set_stencil_reference(VkStencilFaceFlags face_mask, uint32_t reference);

   void clear_attachment(uint32_t idx,
                         const VkClearValue *pClearValue,
                         VkImageAspectFlags aspectMask,
                         uint32_t base_layer,
                         uint32_t layer_count,
                         uint32_t rectCount,
                         D3D12_RECT *rects);
   void clear_attachments(uint32_t attachment_count,
                          const VkClearAttachment *attachments,
                          uint32_t rect_count,
                          const VkClearRect *rects);

   void clear(const struct dzn_image *image,
              VkImageLayout layout,
              const VkClearColorValue *color,
              uint32_t range_count,
              const VkImageSubresourceRange *ranges);
   void clear(const struct dzn_image *image,
              VkImageLayout layout,
              const VkClearDepthStencilValue *zs,
              uint32_t range_count,
              const VkImageSubresourceRange *ranges);

   void copy(const VkCopyBufferInfo2KHR *info);
   void copy(const VkCopyImageToBufferInfo2KHR *info);
   void copy(const VkCopyBufferToImageInfo2KHR *info);
   void copy(const VkCopyImageInfo2KHR *info);
   void fill(dzn_buffer *buf,
             VkDeviceSize offset,
             VkDeviceSize size,
             uint32_t data);
   void update(dzn_buffer *buf,
               VkDeviceSize offset,
               VkDeviceSize size,
               const void *data);

   void pipeline_barrier(const VkDependencyInfoKHR *info);

   void blit(const VkBlitImageInfo2KHR *info);
   void resolve(const VkResolveImageInfo2KHR *info);

   void draw(uint32_t vertex_count,
             uint32_t instance_count,
             uint32_t first_vertex,
             uint32_t first_instance);
   void draw(uint32_t index_count,
             uint32_t instance_count,
             uint32_t first_index,
             int32_t vertex_offset,
             uint32_t first_instance);
   void draw(struct dzn_buffer *draw_buf,
             size_t draw_buf_offset,
             uint32_t draw_count,
             uint32_t draw_buf_stride,
             bool indexed);
   void dispatch(uint32_t group_count_x,
                 uint32_t group_count_y,
                 uint32_t group_count_z);
   void dispatch(struct dzn_buffer *dispatch_buf,
                 uint32_t dispatch_buf_offset);

private:
   void collect_queries(const queries_iterator &first_iter,
                        uint32_t query_count);
   void collect_queries(dzn_query_pool *qpool,
                        uint32_t first_query,
                        uint32_t query_count);
   void gather_queries();

   void clear_with_copy(const dzn_image *image,
                        VkImageLayout layout,
                        const VkClearColorValue *color,
                        uint32_t range_count,
                        const VkImageSubresourceRange *ranges);

   void copy(const VkCopyImageToBufferInfo2KHR *info,
             uint32_t region,
             VkImageAspectFlagBits aspect,
             uint32_t layer);
   void copy(const VkCopyBufferToImageInfo2KHR *info,
             uint32_t region,
             VkImageAspectFlagBits aspect,
             uint32_t layer);
   void copy(const VkCopyImageInfo2KHR *info,
             D3D12_RESOURCE_DESC &tmp_desc,
             D3D12_TEXTURE_COPY_LOCATION &tmp_loc,
             uint32_t region,
             VkImageAspectFlagBits aspect,
             uint32_t layer);
   void copy(ID3D12Resource *src,
             dzn_buffer *dst,
             VkDeviceSize dst_offset,
             VkDeviceSize size);

   void blit_set_pipeline(const dzn_image *src,
                          const dzn_image *dst,
                          VkImageAspectFlagBits aspect,
                          VkFilter filter, bool resolve);
   void blit_set_2d_region(const dzn_image *src,
                           const VkImageSubresourceLayers &src_subres,
                           const VkOffset3D *src_offsets,
                           const dzn_image *dst,
                           const VkImageSubresourceLayers &dst_subres,
                           const VkOffset3D *dst_offsets,
                           bool normalize_src_coords);
   void blit_issue_barriers(dzn_image *src, VkImageLayout src_layout,
                            const VkImageSubresourceLayers &src_subres,
                            dzn_image *dst, VkImageLayout dst_layout,
                            const VkImageSubresourceLayers &dst_subres,
                            VkImageAspectFlagBits aspect,
                            bool post);
   void blit_prepare_src_view(VkImage image,
                              VkImageAspectFlagBits aspect,
                              const VkImageSubresourceLayers &subres,
                              dzn_descriptor_heap &heap,
                              uint32_t heap_offset);
   void blit_prepare_dst_view(dzn_image *image,
                              VkImageAspectFlagBits aspect,
                              uint32_t level, uint32_t layer);
   void blit(const VkBlitImageInfo2KHR *info,
             dzn_descriptor_heap &heap,
             uint32_t &heap_offset,
             uint32_t r);
   void resolve(const VkResolveImageInfo2KHR *info,
                dzn_descriptor_heap &heap,
                uint32_t &heap_offset,
                uint32_t r);

   void resolve_attachment(uint32_t idx);
   void attachment_transition(const dzn_attachment_ref &att);
   void attachment_transition(const dzn_attachment &att);
   void begin_subpass();
   void end_subpass();
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
   ID3D12Resource *
   alloc_internal_buf(uint32_t size,
                      D3D12_HEAP_TYPE heap_type,
                      D3D12_RESOURCE_STATES init_state);
   void triangle_fan_create_index(uint32_t &vertex_count);
   void triangle_fan_rewrite_index(uint32_t &index_count, uint32_t &first_index);
   uint32_t triangle_fan_get_max_index_buf_size(bool indexed);
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
   allocate_cmd_buffers(dzn_device *device,
                        const VkCommandBufferAllocateInfo *pAllocateInfo,
                        VkCommandBuffer *pCommandBuffers);
   void
   free_cmd_buffers(dzn_device *device,
                    uint32_t commandBufferCount,
                    const VkCommandBuffer *pCommandBuffers);
   VkResult reset(dzn_device *device);

   using bufs_allocator = dzn_allocator<dzn_object_unique_ptr<dzn_cmd_buffer>>;
   dzn_object_vector<dzn_cmd_buffer> bufs;
};

struct dzn_descriptor_pool {
   struct vk_object_base base;
   VkAllocationCallbacks alloc;

   dzn_descriptor_pool(dzn_device *device,
                       const VkDescriptorPoolCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator);
   ~dzn_descriptor_pool();

   VkResult
   allocate_sets(dzn_device *device,
                 const VkDescriptorSetAllocateInfo *pAllocateInfo,
                 VkDescriptorSet *pDescriptorSets);
   VkResult
   free_sets(dzn_device *device,
             uint32_t count,
             const VkDescriptorSet *pDescriptorSets);
   VkResult reset(dzn_device *device);

   using sets_allocator = dzn_allocator<dzn_object_unique_ptr<dzn_descriptor_set>>;
   dzn_object_vector<dzn_descriptor_set> sets;
};

#define MAX_SHADER_VISIBILITIES (D3D12_SHADER_VISIBILITY_PIXEL + 1)

struct dzn_descriptor_set_layout_binding {
   VkDescriptorType type;
   D3D12_SHADER_VISIBILITY visibility;
   uint32_t base_shader_register;
   uint32_t range_idx[NUM_POOL_TYPES];
   union {
      uint32_t static_sampler_idx;
      uint32_t dynamic_buffer_idx;
   };
};

struct dzn_descriptor_set_layout {
   struct vk_object_base base;
   uint32_t range_count[MAX_SHADER_VISIBILITIES][NUM_POOL_TYPES];
   const D3D12_DESCRIPTOR_RANGE1 *ranges[MAX_SHADER_VISIBILITIES][NUM_POOL_TYPES];
   uint32_t range_desc_count[NUM_POOL_TYPES];
   uint32_t static_sampler_count;
   const D3D12_STATIC_SAMPLER_DESC *static_samplers;
   struct {
      uint32_t bindings[MAX_DYNAMIC_BUFFERS];
      uint32_t count;
      uint32_t desc_count;
      uint32_t range_offset;
   } dynamic_buffers;
   uint32_t binding_count;
   const struct dzn_descriptor_set_layout_binding *bindings;

   dzn_descriptor_set_layout(dzn_device *device,
                             const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator);
   ~dzn_descriptor_set_layout();

   uint32_t get_heap_offset(uint32_t b, D3D12_DESCRIPTOR_HEAP_TYPE type, bool writeable = false) const;
   uint32_t get_desc_count(uint32_t b) const;
};

struct dzn_descriptor_set {
   struct vk_object_base base;
   dzn_descriptor_heap heaps[NUM_POOL_TYPES];
   const struct dzn_descriptor_set_layout *layout;
   struct dzn_buffer_desc *dynamic_buffers;
   uint32_t index;
   dzn_descriptor_pool *pool;

   dzn_descriptor_set(dzn_device *device,
                      dzn_descriptor_pool *pool,
                      VkDescriptorSetLayout layout,
                      const VkAllocationCallbacks *pAllocator);
   ~dzn_descriptor_set();

   void write(const VkWriteDescriptorSet *pDescriptorWrite);
   void copy(const dzn_descriptor_set *src, const VkCopyDescriptorSet *pDescriptorCopy);
   const VkAllocationCallbacks *get_vk_allocator();

private:
   struct range {
      struct iterator {
         uint32_t binding;
         uint32_t elem;
         const range &range;

         iterator &operator+=(uint32_t count);
         iterator &operator++() { return operator+=(1UL); }
         bool operator!=(const iterator &iter) const;
         iterator &operator*() { return *this; }
         uint32_t get_heap_offset(D3D12_DESCRIPTOR_HEAP_TYPE type, bool writeable = false) const;
         uint32_t get_dynamic_buffer_idx() const;
         VkDescriptorType get_vk_type() const;
         uint32_t remaining_descs_in_binding() const;
      };

      range(const dzn_descriptor_set_layout &layout,
            uint32_t binding,
            uint32_t desc_offset,
            uint32_t desc_count);

      iterator begin();
      iterator end();

      const dzn_descriptor_set_layout &layout;
      struct {
         uint32_t binding, elem;
      } first, last;
   };

   void
   write_desc(const range::iterator &iter, dzn_sampler *sampler);
   void
   write_dynamic_buffer_desc(const range::iterator &iter, const dzn_buffer_desc &info);
   template<typename ... Args> void
   write_desc(const range::iterator &iter, Args... args);
};

struct dzn_pipeline_layout {
   struct vk_object_base base;
   struct {
      uint32_t heap_offsets[NUM_POOL_TYPES];
   } sets[MAX_SETS];
   dxil_spirv_vulkan_descriptor_set binding_translation[MAX_SETS];
   uint32_t set_count;
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

#define MAX_RTS 8
#define MAX_INPUT_ATTACHMENTS 4

struct dzn_subpass {
   uint32_t color_count;
   struct dzn_attachment_ref colors[MAX_RTS];
   struct dzn_attachment_ref resolve[MAX_RTS];
   struct dzn_attachment_ref zs;
   uint32_t input_count;
   struct dzn_attachment_ref inputs[MAX_INPUT_ATTACHMENTS];
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
   dzn_device *device;
   ComPtr<ID3D12PipelineState> state;

   dzn_pipeline(dzn_device *device, VkPipelineBindPoint type);
   ~dzn_pipeline();

   static VkResult compile_shader(dzn_device *device,
                                  dzn_pipeline_layout *layout,
                                  const VkPipelineShaderStageCreateInfo *stage_info,
                                  enum dxil_spirv_yz_flip_mode yz_flip_mode,
                                  uint32_t yz_flip_mask,
                                  D3D12_SHADER_BYTECODE *slot);
};

struct dzn_graphics_pipeline {
   dzn_pipeline base;
   struct {
      unsigned count;
      uint32_t strides[MAX_VBS];
   } vb = {};

   struct {
      bool triangle_fan;
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

   enum indirect_cmd_sig_type {
      INDIRECT_DRAW_CMD_SIG,
      INDIRECT_INDEXED_DRAW_CMD_SIG,
      INDIRECT_DRAW_TRIANGLE_FAN_CMD_SIG,
      NUM_INDIRECT_DRAW_CMD_SIGS,
   };

   ID3D12CommandSignature *get_indirect_cmd_sig(enum indirect_cmd_sig_type type);

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

   ComPtr<ID3D12CommandSignature> indirect_cmd_sigs[NUM_INDIRECT_DRAW_CMD_SIGS];
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

   ID3D12CommandSignature *get_indirect_cmd_sig();

private:
   ComPtr<ID3D12CommandSignature> indirect_cmd_sig;
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
   VkDeviceSize mem_offset;

   dzn_image(dzn_device *device,
             const VkImageCreateInfo *pCreateInfo,
             const VkAllocationCallbacks *alloc);
   ~dzn_image();

   uint32_t
   get_subresource_index(const VkImageSubresource &subres,
                         VkImageAspectFlagBits aspect) const;
   uint32_t
   get_subresource_index(const VkImageSubresourceLayers &subres,
                         VkImageAspectFlagBits aspect,
                         uint32_t layer) const;
   uint32_t
   get_subresource_index(const VkImageSubresourceRange &subres,
                         VkImageAspectFlagBits aspect,
                         uint32_t level, uint32_t layer) const;
   D3D12_TEXTURE_COPY_LOCATION
   get_copy_loc(const VkImageSubresourceLayers &subres,
                VkImageAspectFlagBits aspect,
                uint32_t layer) const;

   void create_rtv(dzn_device *device,
	           const VkImageSubresourceRange &range,
	           uint32_t level,
	           D3D12_CPU_DESCRIPTOR_HANDLE handle) const;
   void create_dsv(dzn_device *device,
                   const VkImageSubresourceRange &range,
                   uint32_t level,
                   D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

   static DXGI_FORMAT
   get_dxgi_format(VkFormat format,
                   VkImageUsageFlags usage,
                   VkImageAspectFlags aspects);
   static DXGI_FORMAT
   get_placed_footprint_format(VkFormat format,
                               VkImageAspectFlags aspect);
   static VkFormat
   get_plane_format(VkFormat format, VkImageAspectFlags aspectMask);

   static D3D12_RESOURCE_STATES get_state(VkImageLayout layout);
};

struct dzn_image_view {
   struct vk_image_view vk;

   D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
   D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};

   struct d3d12_descriptor_handle rt_handle = {};
   struct d3d12_descriptor_handle zs_handle = {};

   dzn_device *get_device() {
      return container_of(vk.base.device, dzn_device, vk);
   }

   const dzn_image *get_image() const {
      return container_of(vk.image, dzn_image, vk);
   }

   dzn_image *get_image() {
      return container_of(vk.image, dzn_image, vk);
   }

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

   static DXGI_FORMAT get_dxgi_format(VkFormat format);

   D3D12_TEXTURE_COPY_LOCATION
   get_copy_loc(VkFormat format,
                const VkBufferImageCopy2KHR &info,
                VkImageAspectFlagBits aspect,
                uint32_t layer);

   D3D12_TEXTURE_COPY_LOCATION
   get_line_copy_loc(VkFormat format,
                     const VkBufferImageCopy2KHR &region,
                     const D3D12_TEXTURE_COPY_LOCATION &loc,
                     uint32_t y, uint32_t z, uint32_t &start_x);

   bool supports_region_copy(const D3D12_TEXTURE_COPY_LOCATION &loc);
};

struct dzn_buffer_view {
   struct vk_object_base base;

   const dzn_buffer *buffer;

   D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
   D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};

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
   D3D12_STATIC_BORDER_COLOR static_border_color = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;

   dzn_sampler(dzn_device *device,
               const VkSamplerCreateInfo *pCreateInfo,
               const VkAllocationCallbacks *pAllocator);
   ~dzn_sampler();
};

/* This is defined as a macro so that it works for both
 * VkImageSubresourceRange and VkImageSubresourceLayers
 */
#define dzn_get_layer_count(_image, _range) \
   ((_range)->layerCount == VK_REMAINING_ARRAY_LAYERS ? \
    (_image)->vk.array_layers - (_range)->baseArrayLayer : (_range)->layerCount)

#define dzn_get_level_count(_image, _range) \
   ((_range)->levelCount == VK_REMAINING_MIP_LEVELS ? \
    (_image)->vk.mip_levels - (_range)->baseMipLevel : (_range)->levelCount)

#ifdef __cplusplus
extern "C" {
#endif
DXGI_FORMAT dzn_pipe_to_dxgi_format(enum pipe_format in);
D3D12_FILTER dzn_translate_sampler_filter(const VkSamplerCreateInfo *create_info);
D3D12_COMPARISON_FUNC dzn_translate_compare_op(VkCompareOp in);
void dzn_translate_viewport(D3D12_VIEWPORT *out, const VkViewport *in);
void dzn_translate_rect(D3D12_RECT *out, const VkRect2D *in);
#ifdef __cplusplus
}
#endif

#define dzn_foreach_aspect(aspect, mask) \
        for (VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT; \
             aspect <= VK_IMAGE_ASPECT_STENCIL_BIT; \
             aspect = (VkImageAspectFlagBits)(aspect << 1)) \
           if (mask & aspect)

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
   DZN_DEBUG_SIG = 1 << 5,
   DZN_DEBUG_GBV = 1 << 6,
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

struct dzn_query_pool {
   struct query {
      enum status {
         RESET,
         STARTED,
         STOPPED,
         RESOLVED,
      };
      enum status status = RESET;
      D3D12_QUERY_TYPE type = D3D12_QUERY_TYPE_OCCLUSION;
      ComPtr<ID3D12Fence> fence;
      std::atomic<uint64_t> fence_value;

      query() : fence_value(0) {}
      query(const query &q) {
         status = q.status;
	 fence = q.fence;
	 fence_value.store(q.fence_value);
      }
   };

   struct vk_object_base base;

   D3D12_QUERY_TYPE get_query_type(VkQueryControlFlags flags);
   D3D12_QUERY_HEAP_TYPE heap_type;
   ComPtr<ID3D12QueryHeap> heap;
   std::vector<query, dzn_allocator<query>> queries;
   ComPtr<ID3D12Resource> resolve_buffer;
   ComPtr<ID3D12Resource> collect_buffer;
   VkQueryPipelineStatisticFlags pipeline_statistics;
   uint32_t query_size;

   dzn_query_pool(dzn_device *device,
                  const VkQueryPoolCreateInfo *info,
                  const VkAllocationCallbacks *alloc);
   ~dzn_query_pool();

   void reset(uint32_t first_query, uint32_t query_count);
   VkResult get_results(uint32_t first_query,
                        uint32_t query_count,
                        size_t data_size,
                        void *data,
                        VkDeviceSize stride,
                        VkQueryResultFlags flags);

   uint32_t get_result_offset(uint32_t query);
   uint32_t get_result_size(uint32_t query_count);
   uint32_t get_availability_offset(uint32_t query);

private:
   uint64_t *collect_map;
   static D3D12_QUERY_HEAP_TYPE get_heap_type(VkQueryType in);
};

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
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_image_view, vk.base, VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline, base, VkPipeline, VK_OBJECT_TYPE_PIPELINE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_graphics_pipeline, base.base, VkPipeline, VK_OBJECT_TYPE_PIPELINE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_compute_pipeline, base.base, VkPipeline, VK_OBJECT_TYPE_PIPELINE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline_cache, base, VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_pipeline_layout, base, VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_query_pool, base, VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_render_pass, base, VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_sampler, base, VkSampler, VK_OBJECT_TYPE_SAMPLER)
VK_DEFINE_NONDISP_HANDLE_CASTS(dzn_semaphore, base, VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE)

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

      return result;
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
DZN_OBJ_FACTORY(dzn_physical_device, VkPhysicalDevice, dzn_instance *, ComPtr<IDXGIAdapter1> &, const DXGI_ADAPTER_DESC1 &);
DZN_OBJ_FACTORY(dzn_pipeline_cache, VkPipelineCache, VkDevice, const VkPipelineCacheCreateInfo *);
DZN_OBJ_FACTORY(dzn_pipeline_layout, VkPipelineLayout, VkDevice, const VkPipelineLayoutCreateInfo *);
DZN_OBJ_FACTORY(dzn_queue, VkQueue, VkDevice, const VkDeviceQueueCreateInfo *);
DZN_OBJ_FACTORY(dzn_query_pool, VkQueryPool, VkDevice, const VkQueryPoolCreateInfo *);
DZN_OBJ_FACTORY(dzn_render_pass, VkRenderPass, VkDevice, const VkRenderPassCreateInfo2KHR *);
DZN_OBJ_FACTORY(dzn_sampler, VkSampler, VkDevice, const VkSamplerCreateInfo *);
DZN_OBJ_FACTORY(dzn_semaphore, VkSemaphore, VkDevice, const VkSemaphoreCreateInfo *);

#endif /* DZN_PRIVATE_H */
