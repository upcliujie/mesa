/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "trace_layer_gen.h"
#include <mutex>
#include <perfetto.h>
#include <unordered_set>
#include <vector>
#include <vulkan/vk_icd.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#include "vk_util.h"

#define TRACE(...) TRACE_EVENT("mesa.vulkan.trace", __VA_ARGS__)
#define TRACE_SLOW(...) TRACE_EVENT("mesa.vulkan.trace.cmd", __VA_ARGS__)

PERFETTO_DEFINE_CATEGORIES(perfetto::Category("mesa.vulkan.trace")
                              .SetDescription("Events from non-vkCmd commands"),
                           perfetto::Category("mesa.vulkan.trace.cmd")
                              .SetDescription("Events from vkCmd commands")
                              .SetTags("slow"));

template <typename TraceType, typename VkType> class TraceHandle {
 public:
   TraceHandle(VkType handle) : wrapped_handle_(handle) {}

   static TraceType *fromHandle(VkType handle)
   {
      return reinterpret_cast<TraceType *>(handle);
   }

   static VkType unwrapHandle(VkType handle)
   {
      return fromHandle(handle)->unwrapHandle();
   }

   VkType unwrapHandle() const { return wrapped_handle_; }
   VkType toHandle() const
   {
      return reinterpret_cast<VkType>(
         const_cast<TraceHandle<TraceType, VkType> *>(this));
   }

 private:
   VK_LOADER_DATA loader_data_{ ICD_LOADER_MAGIC };
   const VkType wrapped_handle_;
};

template <typename TraceType, typename VkNonDispType>
class TraceNonDispHandle {
 public:
   TraceNonDispHandle(VkNonDispType handle) : wrapped_handle_(handle) {}

   static TraceType *fromHandle(VkNonDispType handle)
   {
      return reinterpret_cast<TraceType *>(
         reinterpret_cast<uintptr_t>(handle));
   }

   static VkNonDispType unwrapHandle(VkNonDispType handle)
   {
      auto traceHandle = fromHandle(handle);
      return traceHandle ? traceHandle->unwrapHandle() : VK_NULL_HANDLE;
   }

   VkNonDispType unwrapHandle() const { return wrapped_handle_; }
   VkNonDispType toHandle() const
   {
      return reinterpret_cast<VkNonDispType>(
         reinterpret_cast<uintptr_t>(this));
   }

 private:
   const VkNonDispType wrapped_handle_;
};

class TraceInstance;
class TracePhysicalDevice;
class TraceDevice;
class TraceQueue;
class TraceCommandPool;
class TraceCommandBuffer;

class TraceInstance : public TraceHandle<TraceInstance, VkInstance> {
 public:
   TraceInstance(VkInstance instance,
                 PFN_vkGetInstanceProcAddr gipa,
                 PFN_vkSetInstanceLoaderData sild)
       : TraceHandle(instance), dispatch_table_(instance, gipa),
         set_instance_loader_data_(sild)
   {
      set_instance_loader_data_(unwrapHandle(), toHandle());
   }

   const TraceInstanceDispatchTable &getDispatchTable() const
   {
      return dispatch_table_;
   }

   template <typename TraceType>
   void setLoaderData(TraceType &traceHandle) const
   {
      set_instance_loader_data_(toHandle(), traceHandle.toHandle());
   }

   static VkResult CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator,
                                  VkInstance *pInstance);

   static VkResult
   EnumeratePhysicalDevices(VkInstance instance,
                            uint32_t *pPhysicalDeviceCount,
                            VkPhysicalDevice *pPhysicalDevices);

   static VkResult EnumeratePhysicalDeviceGroups(
      VkInstance instance,
      uint32_t *pPhysicalDeviceGroupCount,
      VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties);

   static VkResult EnumeratePhysicalDeviceGroupsKHR(
      VkInstance instance,
      uint32_t *pPhysicalDeviceGroupCount,
      VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
   {
      return EnumeratePhysicalDeviceGroups(
         instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
   }

   static VkResult
   EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                      const char *pLayerName,
                                      uint32_t *pPropertyCount,
                                      VkExtensionProperties *pProperties);

 private:
   VkResult initPhysicalDevicesLocked();

   TraceInstanceDispatchTable dispatch_table_;

   PFN_vkSetInstanceLoaderData set_instance_loader_data_;

   std::mutex physical_device_mutex;
   std::vector<TracePhysicalDevice> physical_devices_;
   std::vector<VkPhysicalDeviceGroupProperties> physical_device_groups_;
};

class TracePhysicalDevice
    : public TraceHandle<TracePhysicalDevice, VkPhysicalDevice> {
 public:
   TracePhysicalDevice(VkPhysicalDevice physicalDevice,
                       const TraceInstance &instance)
       : TraceHandle(physicalDevice),
         dispatch_table_(instance.getDispatchTable())
   {
      instance.setLoaderData(*this);
   }

   const TraceInstanceDispatchTable &getDispatchTable() const
   {
      return dispatch_table_;
   }

 private:
   const TraceInstanceDispatchTable &dispatch_table_;
};

class TraceDevice : public TraceHandle<TraceDevice, VkDevice> {
 public:
   TraceDevice(VkDevice device,
               PFN_vkGetDeviceProcAddr gdpa,
               PFN_vkSetDeviceLoaderData sdld)
       : TraceHandle(device), dispatch_table_(device, gdpa),
         set_device_loader_data_(sdld)
   {
      set_device_loader_data_(unwrapHandle(), toHandle());
   }

   const TraceDeviceDispatchTable &getDispatchTable() const
   {
      return dispatch_table_;
   }

   template <typename TraceType>
   void setLoaderData(TraceType &traceHandle) const
   {
      set_device_loader_data_(toHandle(), traceHandle.toHandle());
   }

   static VkResult CreateDevice(VkPhysicalDevice physicalDevice,
                                const VkDeviceCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDevice *pDevice);

   static void GetDeviceQueue(VkDevice device,
                              uint32_t queueFamilyIndex,
                              uint32_t queueIndex,
                              VkQueue *pQueue);

   static void GetDeviceQueue2(VkDevice device,
                               const VkDeviceQueueInfo2 *pQueueInfo,
                               VkQueue *pQueue);

   static VkResult
   CreateCommandPool(VkDevice device,
                     const VkCommandPoolCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator,
                     VkCommandPool *pCommandPool);

   static VkResult
   AllocateCommandBuffers(VkDevice device,
                          const VkCommandBufferAllocateInfo *pAllocateInfo,
                          VkCommandBuffer *pCommandBuffers);

   static void FreeCommandBuffers(VkDevice device,
                                  VkCommandPool commandPool,
                                  uint32_t commandBufferCount,
                                  const VkCommandBuffer *pCommandBuffers);

 private:
   void initQueues(const VkDeviceCreateInfo *pCreateInfo);
   void addQueue(const VkDeviceQueueInfo2 *pQueueInfo);

   TraceDeviceDispatchTable dispatch_table_;
   PFN_vkSetDeviceLoaderData set_device_loader_data_;

   std::vector<TraceQueue> queues_;
};

class TraceQueue : public TraceHandle<TraceQueue, VkQueue> {
 public:
   TraceQueue(VkQueue queue,
              const TraceDevice &device,
              uint32_t queueFamilyIndex,
              uint32_t queueIndex,
              VkDeviceQueueCreateFlags flags)
       : TraceHandle(queue), dispatch_table_(device.getDispatchTable()),
         queue_family_index_(queueFamilyIndex), queue_index_(queueIndex),
         flags_(flags)
   {
      device.setLoaderData(*this);
   }

   const TraceDeviceDispatchTable &getDispatchTable() const
   {
      return dispatch_table_;
   }

   bool match(uint32_t queueFamilyIndex, uint32_t queueIndex) const
   {
      return queue_family_index_ == queueFamilyIndex &&
             queue_index_ == queueIndex;
   }

   bool match(const VkDeviceQueueInfo2 *pQueueInfo) const
   {
      return queue_family_index_ == pQueueInfo->queueFamilyIndex &&
             queue_index_ == pQueueInfo->queueIndex &&
             flags_ == pQueueInfo->flags;
   }

   static VkResult QueueSubmit(VkQueue queue,
                               uint32_t submitCount,
                               const VkSubmitInfo *pSubmits,
                               VkFence fence);

 private:
   const TraceDeviceDispatchTable &dispatch_table_;

   const uint32_t queue_family_index_;
   const uint32_t queue_index_;
   const VkDeviceQueueCreateFlags flags_;
};

class TraceCommandBuffer
    : public TraceHandle<TraceCommandBuffer, VkCommandBuffer> {
 public:
   TraceCommandBuffer(VkCommandBuffer commandBuffer,
                      const TraceDevice &device)
       : TraceHandle(commandBuffer),
         dispatch_table_(device.getDispatchTable())
   {
      // I am supposed to do this, but the loader does it for me!?
      // device.setLoaderData(*this);
   }

   const TraceDeviceDispatchTable &getDispatchTable() const
   {
      return dispatch_table_;
   }

   static void CmdExecuteCommands(VkCommandBuffer commandBuffer,
                                  uint32_t commandBufferCount,
                                  const VkCommandBuffer *pCommandBuffers);

 private:
   const TraceDeviceDispatchTable &dispatch_table_;
};

class TraceCommandPool
    : public TraceNonDispHandle<TraceCommandPool, VkCommandPool> {
 public:
   TraceCommandPool(VkCommandPool commandPool)
       : TraceNonDispHandle(commandPool)
   {
   }

   ~TraceCommandPool()
   {
      for (auto cmd : command_buffers_)
         delete cmd;
   }

   void addCommandBuffers(uint32_t commandBufferCount,
                          const VkCommandBuffer *pCommandBuffers)
   {
      for (uint32_t i = 0; i < commandBufferCount; i++) {
         auto cmd = TraceCommandBuffer::fromHandle(pCommandBuffers[i]);
         command_buffers_.emplace(cmd);
      }
   }

   void removeCommandBuffers(uint32_t commandBufferCount,
                             const VkCommandBuffer *pCommandBuffers)
   {

      for (uint32_t i = 0; i < commandBufferCount; i++) {
         auto cmd = TraceCommandBuffer::fromHandle(pCommandBuffers[i]);
         command_buffers_.erase(cmd);
      }
   }

 private:
   std::unordered_set<TraceCommandBuffer *> command_buffers_;
};

template <typename VkType> struct TraceType {
};

template <> struct TraceType<VkInstance> {
   using Type = TraceInstance;
   using DispatchTableType = TraceInstanceDispatchTable;
};
template <> struct TraceType<VkPhysicalDevice> {
   using Type = TracePhysicalDevice;
   using DispatchTableType = TraceInstanceDispatchTable;
};
template <> struct TraceType<VkDevice> {
   using Type = TraceDevice;
   using DispatchTableType = TraceDeviceDispatchTable;
};
template <> struct TraceType<VkQueue> {
   using Type = TraceQueue;
   using DispatchTableType = TraceDeviceDispatchTable;
};
template <> struct TraceType<VkCommandPool> {
   using Type = TraceCommandPool;
};
template <> struct TraceType<VkCommandBuffer> {
   using Type = TraceCommandBuffer;
   using DispatchTableType = TraceDeviceDispatchTable;
};

template <typename VkType>
typename TraceType<VkType>::Type *
traceFrom(VkType handle)
{
   return TraceType<VkType>::Type::fromHandle(handle);
}

template <typename VkType>
VkType
traceUnwrap(VkType handle)
{
   return TraceType<VkType>::Type::unwrapHandle(handle);
}

template <typename VkType>
const typename TraceType<VkType>::DispatchTableType &
traceDispatch(VkType handle)
{
   return TraceType<VkType>::Type::fromHandle(handle)->getDispatchTable();
}

inline VkResult
TraceInstance::CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                              const VkAllocationCallbacks *pAllocator,
                              VkInstance *pInstance)
{
   VkLayerInstanceCreateInfo *layer_info = nullptr;
   PFN_vkSetInstanceLoaderData sild = nullptr;
   vk_foreach_struct_const(pnext, pCreateInfo->pNext) {
      if (pnext->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO)
         continue;

      const auto *tmp =
         reinterpret_cast<const VkLayerInstanceCreateInfo *>(pnext);
      switch (tmp->function) {
      case VK_LAYER_LINK_INFO:
         /* yeah... */
         layer_info = const_cast<VkLayerInstanceCreateInfo *>(tmp);
         break;
      case VK_LOADER_DATA_CALLBACK:
         sild = tmp->u.pfnSetInstanceLoaderData;
         break;
      default:
         break;
      }
   }

   if (!layer_info)
      return VK_ERROR_INITIALIZATION_FAILED;

   PFN_vkGetInstanceProcAddr gipa =
      layer_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
   PFN_vkCreateInstance create_instance =
      reinterpret_cast<PFN_vkCreateInstance>(gipa(NULL, "vkCreateInstance"));
   if (!create_instance)
      return VK_ERROR_INITIALIZATION_FAILED;

   layer_info->u.pLayerInfo = layer_info->u.pLayerInfo->pNext;
   VkResult result = create_instance(pCreateInfo, pAllocator, pInstance);
   if (result != VK_SUCCESS)
      return result;

   auto instance = new TraceInstance(*pInstance, gipa, sild);
   *pInstance = instance->toHandle();

   return VK_SUCCESS;
}

inline VkResult
TraceInstance::initPhysicalDevicesLocked()
{
   if (!physical_devices_.empty())
      return VK_SUCCESS;

   /* enumerate physical devices */
   PFN_vkEnumeratePhysicalDevices enumerate_physical_devices =
      dispatch_table_.EnumeratePhysicalDevices;
   uint32_t count;
   VkResult result = enumerate_physical_devices(unwrapHandle(), &count, NULL);
   if (result != VK_SUCCESS)
      return result;
   std::vector<VkPhysicalDevice> physical_devices(count);
   result = enumerate_physical_devices(unwrapHandle(), &count,
                                       physical_devices.data());
   if (result < VK_SUCCESS)
      return result;
   physical_devices.resize(count);

   /* enumerate physical device groups */
   std::vector<VkPhysicalDeviceGroupProperties> physical_device_groups;
   PFN_vkEnumeratePhysicalDeviceGroups enumerate_physical_device_groups =
      dispatch_table_.EnumeratePhysicalDeviceGroups;
   if (!enumerate_physical_device_groups)
      enumerate_physical_device_groups =
         dispatch_table_.EnumeratePhysicalDeviceGroupsKHR;
   if (enumerate_physical_device_groups) {
      result = enumerate_physical_device_groups(unwrapHandle(), &count, NULL);
      if (result != VK_SUCCESS)
         return result;

      physical_device_groups.resize(count);
      result = enumerate_physical_device_groups(
         unwrapHandle(), &count, physical_device_groups.data());
      if (result < VK_SUCCESS)
         return result;
      physical_device_groups.resize(count);
   }

   for (auto physical_dev : physical_devices) {
      physical_devices_.emplace_back(physical_dev, *this);
   }

   for (auto &group : physical_device_groups) {
      for (uint32_t i = 0; i < group.physicalDeviceCount; i++) {
         for (const auto &physical_dev : physical_devices_) {
            if (group.physicalDevices[i] == physical_dev.unwrapHandle()) {
               group.physicalDevices[i] = physical_dev.toHandle();
               break;
            }
         }
      }
   }
   physical_device_groups_ = std::move(physical_device_groups);

   return VK_SUCCESS;
}

inline VkResult
TraceInstance::EnumeratePhysicalDevices(VkInstance instance_,
                                        uint32_t *pPhysicalDeviceCount,
                                        VkPhysicalDevice *pPhysicalDevices)
{
   auto instance = TraceInstance::fromHandle(instance_);
   std::lock_guard<std::mutex> lock(instance->physical_device_mutex);

   VkResult result = instance->initPhysicalDevicesLocked();
   if (result != VK_SUCCESS)
      return result;

   VK_OUTARRAY_MAKE(out, pPhysicalDevices, pPhysicalDeviceCount);
   for (const auto &physical_dev : instance->physical_devices_) {
      vk_outarray_append(&out, elem) {
         *elem = physical_dev.toHandle();
      }
   }

   return vk_outarray_status(&out);
}

inline VkResult
TraceInstance::EnumeratePhysicalDeviceGroups(
   VkInstance instance_,
   uint32_t *pPhysicalDeviceGroupCount,
   VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
   auto instance = TraceInstance::fromHandle(instance_);
   std::lock_guard<std::mutex> lock(instance->physical_device_mutex);

   VkResult result = instance->initPhysicalDevicesLocked();
   if (result != VK_SUCCESS)
      return result;

   VK_OUTARRAY_MAKE(out, pPhysicalDeviceGroupProperties,
                    pPhysicalDeviceGroupCount);
   for (const auto &group : instance->physical_device_groups_) {
      vk_outarray_append(&out, elem) {
         *elem = group;
      }
   }

   return vk_outarray_status(&out);
}

inline VkResult
TraceInstance::EnumerateDeviceExtensionProperties(
   VkPhysicalDevice physicalDevice,
   const char *pLayerName,
   uint32_t *pPropertyCount,
   VkExtensionProperties *pProperties)
{
   const auto &physical_dev =
      *TracePhysicalDevice::fromHandle(physicalDevice);
   const auto &dispatch_table = physical_dev.getDispatchTable();
   /* TODO filter out unknown extensions */
   return dispatch_table.EnumerateDeviceExtensionProperties(
      physical_dev.unwrapHandle(), pLayerName, pPropertyCount, pProperties);
}

inline VkResult
TraceDevice::CreateDevice(VkPhysicalDevice physicalDevice,
                          const VkDeviceCreateInfo *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          VkDevice *pDevice)
{
   const auto &physical_dev =
      *TracePhysicalDevice::fromHandle(physicalDevice);
   const auto &dispatch_table = physical_dev.getDispatchTable();

   VkLayerDeviceCreateInfo *layer_info = nullptr;
   PFN_vkSetDeviceLoaderData sdld = nullptr;
   vk_foreach_struct_const(pnext, pCreateInfo->pNext) {
      if (pnext->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)
         continue;

      const auto *tmp =
         reinterpret_cast<const VkLayerDeviceCreateInfo *>(pnext);
      switch (tmp->function) {
      case VK_LAYER_LINK_INFO:
         /* yeah... */
         layer_info = const_cast<VkLayerDeviceCreateInfo *>(tmp);
         break;
      case VK_LOADER_DATA_CALLBACK:
         sdld = tmp->u.pfnSetDeviceLoaderData;
         break;
      default:
         break;
      }
   }

   if (!layer_info)
      return VK_ERROR_INITIALIZATION_FAILED;

   PFN_vkGetDeviceProcAddr gdpa =
      layer_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;

   layer_info->u.pLayerInfo = layer_info->u.pLayerInfo->pNext;
   VkResult result = dispatch_table.CreateDevice(
      physical_dev.unwrapHandle(), pCreateInfo, pAllocator, pDevice);
   if (result != VK_SUCCESS)
      return result;

   auto device = new TraceDevice(*pDevice, gdpa, sdld);
   device->initQueues(pCreateInfo);
   *pDevice = device->toHandle();

   return VK_SUCCESS;
}

inline void
TraceDevice::addQueue(const VkDeviceQueueInfo2 *pQueueInfo)
{
   VkQueue queue;
   if (pQueueInfo->flags) {
      dispatch_table_.GetDeviceQueue2(unwrapHandle(), pQueueInfo, &queue);
   } else {
      dispatch_table_.GetDeviceQueue(unwrapHandle(),
                                     pQueueInfo->queueFamilyIndex,
                                     pQueueInfo->queueIndex, &queue);
   }

   queues_.emplace_back(queue, *this, pQueueInfo->queueFamilyIndex,
                        pQueueInfo->queueIndex, pQueueInfo->flags);
}

inline void
TraceDevice::initQueues(const VkDeviceCreateInfo *pCreateInfo)
{
   for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
      const VkDeviceQueueCreateInfo *queue_info =
         &pCreateInfo->pQueueCreateInfos[i];
      for (uint32_t j = 0; j < queue_info->queueCount; j++) {
         const VkDeviceQueueInfo2 info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
            .flags = queue_info->flags,
            .queueFamilyIndex = queue_info->queueFamilyIndex,
            .queueIndex = j,
         };
         addQueue(&info);
      }
   }
}

inline void
TraceDevice::GetDeviceQueue(VkDevice device,
                            uint32_t queueFamilyIndex,
                            uint32_t queueIndex,
                            VkQueue *pQueue)
{
   const auto &dev = *TraceDevice::fromHandle(device);
   for (const auto &queue : dev.queues_) {
      if (queue.match(queueFamilyIndex, queueIndex)) {
         *pQueue = queue.toHandle();
         break;
      }
   }
}

inline void
TraceDevice::GetDeviceQueue2(VkDevice device,
                             const VkDeviceQueueInfo2 *pQueueInfo,
                             VkQueue *pQueue)
{
   const auto &dev = *TraceDevice::fromHandle(device);
   for (const auto &queue : dev.queues_) {
      if (queue.match(pQueueInfo)) {
         *pQueue = queue.toHandle();
         break;
      }
   }
}

inline VkResult
TraceDevice::CreateCommandPool(VkDevice device,
                               const VkCommandPoolCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator,
                               VkCommandPool *pCommandPool)
{
   const auto &dev = *TraceDevice::fromHandle(device);
   const auto &dispatch_table = dev.getDispatchTable();

   VkResult result = dispatch_table.CreateCommandPool(
      dev.unwrapHandle(), pCreateInfo, pAllocator, pCommandPool);
   if (result != VK_SUCCESS)
      return result;

   auto pool = new TraceCommandPool(*pCommandPool);
   *pCommandPool = pool->toHandle();

   return VK_SUCCESS;
}

inline VkResult
TraceDevice::AllocateCommandBuffers(
   VkDevice device,
   const VkCommandBufferAllocateInfo *pAllocateInfo,
   VkCommandBuffer *pCommandBuffers)
{
   const auto &dev = *TraceDevice::fromHandle(device);
   const auto &dispatch_table = dev.getDispatchTable();

   auto &pool = *TraceCommandPool::fromHandle(pAllocateInfo->commandPool);
   VkCommandBufferAllocateInfo alloc_info = *pAllocateInfo;
   alloc_info.commandPool = pool.unwrapHandle();
   VkResult result = dispatch_table.AllocateCommandBuffers(
      dev.unwrapHandle(), &alloc_info, pCommandBuffers);
   if (result != VK_SUCCESS)
      return result;

   for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++) {
      auto cmd = new TraceCommandBuffer(pCommandBuffers[i], dev);
      pCommandBuffers[i] = cmd->toHandle();
   }

   pool.addCommandBuffers(pAllocateInfo->commandBufferCount, pCommandBuffers);

   return VK_SUCCESS;
}

inline void
TraceDevice::FreeCommandBuffers(VkDevice device,
                                VkCommandPool commandPool,
                                uint32_t commandBufferCount,
                                const VkCommandBuffer *pCommandBuffers)
{
   const auto &dev = *TraceDevice::fromHandle(device);
   const auto &dispatch_table = dev.getDispatchTable();
   auto &pool = *TraceCommandPool::fromHandle(commandPool);

   pool.removeCommandBuffers(commandBufferCount, pCommandBuffers);

   std::vector<VkCommandBuffer> cmds;
   cmds.reserve(commandBufferCount);
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      auto cmd = TraceCommandBuffer::fromHandle(pCommandBuffers[i]);
      cmds.emplace_back(cmd->unwrapHandle());
      delete cmd;
   }

   dispatch_table.FreeCommandBuffers(dev.unwrapHandle(), pool.unwrapHandle(),
                                     commandBufferCount, cmds.data());
}

inline VkResult
TraceQueue::QueueSubmit(VkQueue queue_,
                        uint32_t submitCount,
                        const VkSubmitInfo *pSubmits,
                        VkFence fence)
{
   const auto &queue = *TraceQueue::fromHandle(queue_);
   const auto &dispatch_table = queue.getDispatchTable();

   uint32_t cmd_count = 0;
   for (uint32_t i = 0; i < submitCount; i++)
      cmd_count += pSubmits[i].commandBufferCount;

   if (!cmd_count) {
      return dispatch_table.QueueSubmit(queue.unwrapHandle(), submitCount,
                                        pSubmits, fence);
   }

   std::vector<VkSubmitInfo> submits(pSubmits, pSubmits + submitCount);
   std::vector<VkCommandBuffer> cmds(cmd_count);
   cmd_count = 0;
   for (auto &submit : submits) {
      for (uint32_t i = 0; i < submit.commandBufferCount; i++) {
         cmds[cmd_count + i] =
            TraceCommandBuffer::unwrapHandle(submit.pCommandBuffers[i]);
      }
      submit.pCommandBuffers = cmds.data() + cmd_count;
      cmd_count += submit.commandBufferCount;
   }

   return dispatch_table.QueueSubmit(queue.unwrapHandle(), submitCount,
                                     submits.data(), fence);
}

inline void
TraceCommandBuffer::CmdExecuteCommands(VkCommandBuffer commandBuffer,
                                       uint32_t commandBufferCount,
                                       const VkCommandBuffer *pCommandBuffers)
{
   const auto &cmd = *TraceCommandBuffer::fromHandle(commandBuffer);
   const auto &dispatch_table = cmd.getDispatchTable();

   std::vector<VkCommandBuffer> cmds(commandBufferCount);
   for (uint32_t i = 0; i < commandBufferCount; i++)
      cmds[i] = TraceCommandBuffer::unwrapHandle(pCommandBuffers[i]);

   dispatch_table.CmdExecuteCommands(cmd.unwrapHandle(), commandBufferCount,
                                     cmds.data());
}
