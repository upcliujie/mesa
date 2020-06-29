/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_PROTOCOL_DRIVER_HANDLES_H
#define VN_PROTOCOL_DRIVER_HANDLES_H

#include "vn_protocol_driver_types.h"

/* VK_DEFINE_HANDLE(VkInstance) */

static inline size_t
vn_sizeof_VkInstance(const VkInstance *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkInstance(struct vn_cs *cs, const VkInstance *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkInstance(struct vn_cs *cs, VkInstance *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_HANDLE(VkPhysicalDevice) */

static inline size_t
vn_sizeof_VkPhysicalDevice(const VkPhysicalDevice *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkPhysicalDevice(struct vn_cs *cs, const VkPhysicalDevice *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkPhysicalDevice(struct vn_cs *cs, VkPhysicalDevice *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_HANDLE(VkDevice) */

static inline size_t
vn_sizeof_VkDevice(const VkDevice *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkDevice(struct vn_cs *cs, const VkDevice *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, true);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkDevice(struct vn_cs *cs, VkDevice *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, true);
}

/* VK_DEFINE_HANDLE(VkQueue) */

static inline size_t
vn_sizeof_VkQueue(const VkQueue *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkQueue(struct vn_cs *cs, const VkQueue *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkQueue(struct vn_cs *cs, VkQueue *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_HANDLE(VkCommandBuffer) */

static inline size_t
vn_sizeof_VkCommandBuffer(const VkCommandBuffer *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkCommandBuffer(struct vn_cs *cs, const VkCommandBuffer *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkCommandBuffer(struct vn_cs *cs, VkCommandBuffer *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDeviceMemory) */

static inline size_t
vn_sizeof_VkDeviceMemory(const VkDeviceMemory *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkDeviceMemory(struct vn_cs *cs, const VkDeviceMemory *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkDeviceMemory(struct vn_cs *cs, VkDeviceMemory *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkCommandPool) */

static inline size_t
vn_sizeof_VkCommandPool(const VkCommandPool *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkCommandPool(struct vn_cs *cs, const VkCommandPool *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkCommandPool(struct vn_cs *cs, VkCommandPool *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkBuffer) */

static inline size_t
vn_sizeof_VkBuffer(const VkBuffer *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkBuffer(struct vn_cs *cs, const VkBuffer *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkBuffer(struct vn_cs *cs, VkBuffer *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkBufferView) */

static inline size_t
vn_sizeof_VkBufferView(const VkBufferView *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkBufferView(struct vn_cs *cs, const VkBufferView *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkBufferView(struct vn_cs *cs, VkBufferView *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImage) */

static inline size_t
vn_sizeof_VkImage(const VkImage *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkImage(struct vn_cs *cs, const VkImage *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkImage(struct vn_cs *cs, VkImage *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImageView) */

static inline size_t
vn_sizeof_VkImageView(const VkImageView *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkImageView(struct vn_cs *cs, const VkImageView *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkImageView(struct vn_cs *cs, VkImageView *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkShaderModule) */

static inline size_t
vn_sizeof_VkShaderModule(const VkShaderModule *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkShaderModule(struct vn_cs *cs, const VkShaderModule *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkShaderModule(struct vn_cs *cs, VkShaderModule *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkPipeline) */

static inline size_t
vn_sizeof_VkPipeline(const VkPipeline *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkPipeline(struct vn_cs *cs, const VkPipeline *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkPipeline(struct vn_cs *cs, VkPipeline *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkPipelineLayout) */

static inline size_t
vn_sizeof_VkPipelineLayout(const VkPipelineLayout *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkPipelineLayout(struct vn_cs *cs, const VkPipelineLayout *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkPipelineLayout(struct vn_cs *cs, VkPipelineLayout *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSampler) */

static inline size_t
vn_sizeof_VkSampler(const VkSampler *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkSampler(struct vn_cs *cs, const VkSampler *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkSampler(struct vn_cs *cs, VkSampler *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorSet) */

static inline size_t
vn_sizeof_VkDescriptorSet(const VkDescriptorSet *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkDescriptorSet(struct vn_cs *cs, const VkDescriptorSet *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkDescriptorSet(struct vn_cs *cs, VkDescriptorSet *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorSetLayout) */

static inline size_t
vn_sizeof_VkDescriptorSetLayout(const VkDescriptorSetLayout *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkDescriptorSetLayout(struct vn_cs *cs, const VkDescriptorSetLayout *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkDescriptorSetLayout(struct vn_cs *cs, VkDescriptorSetLayout *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorPool) */

static inline size_t
vn_sizeof_VkDescriptorPool(const VkDescriptorPool *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkDescriptorPool(struct vn_cs *cs, const VkDescriptorPool *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkDescriptorPool(struct vn_cs *cs, VkDescriptorPool *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkFence) */

static inline size_t
vn_sizeof_VkFence(const VkFence *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkFence(struct vn_cs *cs, const VkFence *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkFence(struct vn_cs *cs, VkFence *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSemaphore) */

static inline size_t
vn_sizeof_VkSemaphore(const VkSemaphore *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkSemaphore(struct vn_cs *cs, const VkSemaphore *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkSemaphore(struct vn_cs *cs, VkSemaphore *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkEvent) */

static inline size_t
vn_sizeof_VkEvent(const VkEvent *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkEvent(struct vn_cs *cs, const VkEvent *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkEvent(struct vn_cs *cs, VkEvent *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkQueryPool) */

static inline size_t
vn_sizeof_VkQueryPool(const VkQueryPool *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkQueryPool(struct vn_cs *cs, const VkQueryPool *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkQueryPool(struct vn_cs *cs, VkQueryPool *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkFramebuffer) */

static inline size_t
vn_sizeof_VkFramebuffer(const VkFramebuffer *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkFramebuffer(struct vn_cs *cs, const VkFramebuffer *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkFramebuffer(struct vn_cs *cs, VkFramebuffer *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkRenderPass) */

static inline size_t
vn_sizeof_VkRenderPass(const VkRenderPass *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkRenderPass(struct vn_cs *cs, const VkRenderPass *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkRenderPass(struct vn_cs *cs, VkRenderPass *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkPipelineCache) */

static inline size_t
vn_sizeof_VkPipelineCache(const VkPipelineCache *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkPipelineCache(struct vn_cs *cs, const VkPipelineCache *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkPipelineCache(struct vn_cs *cs, VkPipelineCache *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorUpdateTemplate) */

static inline size_t
vn_sizeof_VkDescriptorUpdateTemplate(const VkDescriptorUpdateTemplate *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkDescriptorUpdateTemplate(struct vn_cs *cs, const VkDescriptorUpdateTemplate *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkDescriptorUpdateTemplate(struct vn_cs *cs, VkDescriptorUpdateTemplate *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

/* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSamplerYcbcrConversion) */

static inline size_t
vn_sizeof_VkSamplerYcbcrConversion(const VkSamplerYcbcrConversion *val)
{
    return sizeof(uint64_t);
}

static inline void
vn_encode_VkSamplerYcbcrConversion(struct vn_cs *cs, const VkSamplerYcbcrConversion *val)
{
    const uint64_t id = vn_cs_handle_load_id((const void *)val, false);
    vn_encode_uint64_t(cs, &id);
}

static inline void
vn_decode_VkSamplerYcbcrConversion(struct vn_cs *cs, VkSamplerYcbcrConversion *val)
{
    uint64_t id;
    vn_decode_uint64_t(cs, &id);
    vn_cs_handle_store_id((void *)val, id, false);
}

#endif /* VN_PROTOCOL_DRIVER_HANDLES_H */
