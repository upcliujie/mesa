/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_PROTOCOL_DRIVER_CALLS_H
#define VN_PROTOCOL_DRIVER_CALLS_H

#include "vn_protocol_driver_commands.h"
#include "vn_device.h"

static inline VkResult vn_call_vkCreateInstance(struct vn_instance *vn_instance, const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    const size_t cmd_size = vn_sizeof_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    const size_t reply_size = vn_sizeof_vkCreateInstance_reply(pCreateInfo, pAllocator, pInstance);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateInstance(instance_cs, cmd_flags, pCreateInfo, pAllocator, pInstance);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateInstance_reply(&parser, pCreateInfo, pAllocator, pInstance);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateInstance(struct vn_instance *vn_instance, const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    const size_t cmd_size = vn_sizeof_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateInstance(cs, cmd_flags, pCreateInfo, pAllocator, pInstance);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyInstance(struct vn_instance *vn_instance, VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyInstance(instance, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyInstance_reply(instance, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyInstance(instance_cs, cmd_flags, instance, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyInstance_reply(&parser, instance, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyInstance(struct vn_instance *vn_instance, VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyInstance(instance, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyInstance(cs, cmd_flags, instance, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEnumeratePhysicalDevices(struct vn_instance *vn_instance, VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices)
{
    const size_t cmd_size = vn_sizeof_vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    const size_t reply_size = vn_sizeof_vkEnumeratePhysicalDevices_reply(instance, pPhysicalDeviceCount, pPhysicalDevices);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEnumeratePhysicalDevices(instance_cs, cmd_flags, instance, pPhysicalDeviceCount, pPhysicalDevices);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEnumeratePhysicalDevices_reply(&parser, instance, pPhysicalDeviceCount, pPhysicalDevices);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEnumeratePhysicalDevices(struct vn_instance *vn_instance, VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices)
{
    const size_t cmd_size = vn_sizeof_vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEnumeratePhysicalDevices(cs, cmd_flags, instance, pPhysicalDeviceCount, pPhysicalDevices);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceProperties(physicalDevice, pProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceProperties_reply(physicalDevice, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceProperties(instance_cs, cmd_flags, physicalDevice, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceProperties_reply(&parser, physicalDevice, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceProperties(physicalDevice, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceProperties(cs, cmd_flags, physicalDevice, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceQueueFamilyProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceQueueFamilyProperties_reply(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceQueueFamilyProperties(instance_cs, cmd_flags, physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceQueueFamilyProperties_reply(&parser, physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceQueueFamilyProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceQueueFamilyProperties(cs, cmd_flags, physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceMemoryProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceMemoryProperties_reply(physicalDevice, pMemoryProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceMemoryProperties(instance_cs, cmd_flags, physicalDevice, pMemoryProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceMemoryProperties_reply(&parser, physicalDevice, pMemoryProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceMemoryProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceMemoryProperties(cs, cmd_flags, physicalDevice, pMemoryProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceFeatures(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFeatures(physicalDevice, pFeatures);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceFeatures_reply(physicalDevice, pFeatures);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceFeatures(instance_cs, cmd_flags, physicalDevice, pFeatures);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceFeatures_reply(&parser, physicalDevice, pFeatures);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceFeatures(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFeatures(physicalDevice, pFeatures);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceFeatures(cs, cmd_flags, physicalDevice, pFeatures);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceFormatProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceFormatProperties_reply(physicalDevice, format, pFormatProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceFormatProperties(instance_cs, cmd_flags, physicalDevice, format, pFormatProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceFormatProperties_reply(&parser, physicalDevice, format, pFormatProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceFormatProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceFormatProperties(cs, cmd_flags, physicalDevice, format, pFormatProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetPhysicalDeviceImageFormatProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceImageFormatProperties_reply(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceImageFormatProperties(instance_cs, cmd_flags, physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetPhysicalDeviceImageFormatProperties_reply(&parser, physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetPhysicalDeviceImageFormatProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceImageFormatProperties(cs, cmd_flags, physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateDevice(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    const size_t cmd_size = vn_sizeof_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    const size_t reply_size = vn_sizeof_vkCreateDevice_reply(physicalDevice, pCreateInfo, pAllocator, pDevice);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateDevice(instance_cs, cmd_flags, physicalDevice, pCreateInfo, pAllocator, pDevice);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateDevice_reply(&parser, physicalDevice, pCreateInfo, pAllocator, pDevice);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateDevice(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    const size_t cmd_size = vn_sizeof_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateDevice(cs, cmd_flags, physicalDevice, pCreateInfo, pAllocator, pDevice);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyDevice(struct vn_instance *vn_instance, VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDevice(device, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyDevice_reply(device, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyDevice(instance_cs, cmd_flags, device, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyDevice_reply(&parser, device, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyDevice(struct vn_instance *vn_instance, VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDevice(device, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyDevice(cs, cmd_flags, device, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEnumerateInstanceVersion(struct vn_instance *vn_instance, uint32_t* pApiVersion)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateInstanceVersion(pApiVersion);
    const size_t reply_size = vn_sizeof_vkEnumerateInstanceVersion_reply(pApiVersion);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEnumerateInstanceVersion(instance_cs, cmd_flags, pApiVersion);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEnumerateInstanceVersion_reply(&parser, pApiVersion);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEnumerateInstanceVersion(struct vn_instance *vn_instance, uint32_t* pApiVersion)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateInstanceVersion(pApiVersion);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEnumerateInstanceVersion(cs, cmd_flags, pApiVersion);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEnumerateInstanceLayerProperties(struct vn_instance *vn_instance, uint32_t* pPropertyCount, VkLayerProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
    const size_t reply_size = vn_sizeof_vkEnumerateInstanceLayerProperties_reply(pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEnumerateInstanceLayerProperties(instance_cs, cmd_flags, pPropertyCount, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEnumerateInstanceLayerProperties_reply(&parser, pPropertyCount, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEnumerateInstanceLayerProperties(struct vn_instance *vn_instance, uint32_t* pPropertyCount, VkLayerProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEnumerateInstanceLayerProperties(cs, cmd_flags, pPropertyCount, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEnumerateInstanceExtensionProperties(struct vn_instance *vn_instance, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
    const size_t reply_size = vn_sizeof_vkEnumerateInstanceExtensionProperties_reply(pLayerName, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEnumerateInstanceExtensionProperties(instance_cs, cmd_flags, pLayerName, pPropertyCount, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEnumerateInstanceExtensionProperties_reply(&parser, pLayerName, pPropertyCount, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEnumerateInstanceExtensionProperties(struct vn_instance *vn_instance, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEnumerateInstanceExtensionProperties(cs, cmd_flags, pLayerName, pPropertyCount, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEnumerateDeviceLayerProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
    const size_t reply_size = vn_sizeof_vkEnumerateDeviceLayerProperties_reply(physicalDevice, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEnumerateDeviceLayerProperties(instance_cs, cmd_flags, physicalDevice, pPropertyCount, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEnumerateDeviceLayerProperties_reply(&parser, physicalDevice, pPropertyCount, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEnumerateDeviceLayerProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEnumerateDeviceLayerProperties(cs, cmd_flags, physicalDevice, pPropertyCount, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEnumerateDeviceExtensionProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
    const size_t reply_size = vn_sizeof_vkEnumerateDeviceExtensionProperties_reply(physicalDevice, pLayerName, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEnumerateDeviceExtensionProperties(instance_cs, cmd_flags, physicalDevice, pLayerName, pPropertyCount, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEnumerateDeviceExtensionProperties_reply(&parser, physicalDevice, pLayerName, pPropertyCount, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEnumerateDeviceExtensionProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEnumerateDeviceExtensionProperties(cs, cmd_flags, physicalDevice, pLayerName, pPropertyCount, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetDeviceQueue(struct vn_instance *vn_instance, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    const size_t reply_size = vn_sizeof_vkGetDeviceQueue_reply(device, queueFamilyIndex, queueIndex, pQueue);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetDeviceQueue(instance_cs, cmd_flags, device, queueFamilyIndex, queueIndex, pQueue);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetDeviceQueue_reply(&parser, device, queueFamilyIndex, queueIndex, pQueue);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetDeviceQueue(struct vn_instance *vn_instance, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetDeviceQueue(cs, cmd_flags, device, queueFamilyIndex, queueIndex, pQueue);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkQueueSubmit(struct vn_instance *vn_instance, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
    const size_t cmd_size = vn_sizeof_vkQueueSubmit(queue, submitCount, pSubmits, fence);
    const size_t reply_size = vn_sizeof_vkQueueSubmit_reply(queue, submitCount, pSubmits, fence);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkQueueSubmit(instance_cs, cmd_flags, queue, submitCount, pSubmits, fence);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkQueueSubmit_reply(&parser, queue, submitCount, pSubmits, fence);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkQueueSubmit(struct vn_instance *vn_instance, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
    const size_t cmd_size = vn_sizeof_vkQueueSubmit(queue, submitCount, pSubmits, fence);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkQueueSubmit(cs, cmd_flags, queue, submitCount, pSubmits, fence);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkQueueWaitIdle(struct vn_instance *vn_instance, VkQueue queue)
{
    const size_t cmd_size = vn_sizeof_vkQueueWaitIdle(queue);
    const size_t reply_size = vn_sizeof_vkQueueWaitIdle_reply(queue);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkQueueWaitIdle(instance_cs, cmd_flags, queue);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkQueueWaitIdle_reply(&parser, queue);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkQueueWaitIdle(struct vn_instance *vn_instance, VkQueue queue)
{
    const size_t cmd_size = vn_sizeof_vkQueueWaitIdle(queue);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkQueueWaitIdle(cs, cmd_flags, queue);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkDeviceWaitIdle(struct vn_instance *vn_instance, VkDevice device)
{
    const size_t cmd_size = vn_sizeof_vkDeviceWaitIdle(device);
    const size_t reply_size = vn_sizeof_vkDeviceWaitIdle_reply(device);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDeviceWaitIdle(instance_cs, cmd_flags, device);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkDeviceWaitIdle_reply(&parser, device);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkDeviceWaitIdle(struct vn_instance *vn_instance, VkDevice device)
{
    const size_t cmd_size = vn_sizeof_vkDeviceWaitIdle(device);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDeviceWaitIdle(cs, cmd_flags, device);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkAllocateMemory(struct vn_instance *vn_instance, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
    const size_t cmd_size = vn_sizeof_vkAllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
    const size_t reply_size = vn_sizeof_vkAllocateMemory_reply(device, pAllocateInfo, pAllocator, pMemory);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkAllocateMemory(instance_cs, cmd_flags, device, pAllocateInfo, pAllocator, pMemory);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkAllocateMemory_reply(&parser, device, pAllocateInfo, pAllocator, pMemory);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkAllocateMemory(struct vn_instance *vn_instance, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
    const size_t cmd_size = vn_sizeof_vkAllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkAllocateMemory(cs, cmd_flags, device, pAllocateInfo, pAllocator, pMemory);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkFreeMemory(struct vn_instance *vn_instance, VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkFreeMemory(device, memory, pAllocator);
    const size_t reply_size = vn_sizeof_vkFreeMemory_reply(device, memory, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkFreeMemory(instance_cs, cmd_flags, device, memory, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkFreeMemory_reply(&parser, device, memory, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkFreeMemory(struct vn_instance *vn_instance, VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkFreeMemory(device, memory, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkFreeMemory(cs, cmd_flags, device, memory, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkUnmapMemory(struct vn_instance *vn_instance, VkDevice device, VkDeviceMemory memory)
{
    const size_t cmd_size = vn_sizeof_vkUnmapMemory(device, memory);
    const size_t reply_size = vn_sizeof_vkUnmapMemory_reply(device, memory);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkUnmapMemory(instance_cs, cmd_flags, device, memory);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkUnmapMemory_reply(&parser, device, memory);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkUnmapMemory(struct vn_instance *vn_instance, VkDevice device, VkDeviceMemory memory)
{
    const size_t cmd_size = vn_sizeof_vkUnmapMemory(device, memory);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkUnmapMemory(cs, cmd_flags, device, memory);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkFlushMappedMemoryRanges(struct vn_instance *vn_instance, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    const size_t cmd_size = vn_sizeof_vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
    const size_t reply_size = vn_sizeof_vkFlushMappedMemoryRanges_reply(device, memoryRangeCount, pMemoryRanges);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkFlushMappedMemoryRanges(instance_cs, cmd_flags, device, memoryRangeCount, pMemoryRanges);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkFlushMappedMemoryRanges_reply(&parser, device, memoryRangeCount, pMemoryRanges);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkFlushMappedMemoryRanges(struct vn_instance *vn_instance, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    const size_t cmd_size = vn_sizeof_vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkFlushMappedMemoryRanges(cs, cmd_flags, device, memoryRangeCount, pMemoryRanges);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkInvalidateMappedMemoryRanges(struct vn_instance *vn_instance, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    const size_t cmd_size = vn_sizeof_vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
    const size_t reply_size = vn_sizeof_vkInvalidateMappedMemoryRanges_reply(device, memoryRangeCount, pMemoryRanges);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkInvalidateMappedMemoryRanges(instance_cs, cmd_flags, device, memoryRangeCount, pMemoryRanges);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkInvalidateMappedMemoryRanges_reply(&parser, device, memoryRangeCount, pMemoryRanges);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkInvalidateMappedMemoryRanges(struct vn_instance *vn_instance, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    const size_t cmd_size = vn_sizeof_vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkInvalidateMappedMemoryRanges(cs, cmd_flags, device, memoryRangeCount, pMemoryRanges);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetDeviceMemoryCommitment(struct vn_instance *vn_instance, VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
    const size_t reply_size = vn_sizeof_vkGetDeviceMemoryCommitment_reply(device, memory, pCommittedMemoryInBytes);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetDeviceMemoryCommitment(instance_cs, cmd_flags, device, memory, pCommittedMemoryInBytes);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetDeviceMemoryCommitment_reply(&parser, device, memory, pCommittedMemoryInBytes);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetDeviceMemoryCommitment(struct vn_instance *vn_instance, VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetDeviceMemoryCommitment(cs, cmd_flags, device, memory, pCommittedMemoryInBytes);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetBufferMemoryRequirements(struct vn_instance *vn_instance, VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
    const size_t reply_size = vn_sizeof_vkGetBufferMemoryRequirements_reply(device, buffer, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetBufferMemoryRequirements(instance_cs, cmd_flags, device, buffer, pMemoryRequirements);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetBufferMemoryRequirements_reply(&parser, device, buffer, pMemoryRequirements);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetBufferMemoryRequirements(struct vn_instance *vn_instance, VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetBufferMemoryRequirements(cs, cmd_flags, device, buffer, pMemoryRequirements);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkBindBufferMemory(struct vn_instance *vn_instance, VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    const size_t cmd_size = vn_sizeof_vkBindBufferMemory(device, buffer, memory, memoryOffset);
    const size_t reply_size = vn_sizeof_vkBindBufferMemory_reply(device, buffer, memory, memoryOffset);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkBindBufferMemory(instance_cs, cmd_flags, device, buffer, memory, memoryOffset);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkBindBufferMemory_reply(&parser, device, buffer, memory, memoryOffset);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkBindBufferMemory(struct vn_instance *vn_instance, VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    const size_t cmd_size = vn_sizeof_vkBindBufferMemory(device, buffer, memory, memoryOffset);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkBindBufferMemory(cs, cmd_flags, device, buffer, memory, memoryOffset);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetImageMemoryRequirements(struct vn_instance *vn_instance, VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageMemoryRequirements(device, image, pMemoryRequirements);
    const size_t reply_size = vn_sizeof_vkGetImageMemoryRequirements_reply(device, image, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetImageMemoryRequirements(instance_cs, cmd_flags, device, image, pMemoryRequirements);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetImageMemoryRequirements_reply(&parser, device, image, pMemoryRequirements);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetImageMemoryRequirements(struct vn_instance *vn_instance, VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageMemoryRequirements(device, image, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetImageMemoryRequirements(cs, cmd_flags, device, image, pMemoryRequirements);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkBindImageMemory(struct vn_instance *vn_instance, VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    const size_t cmd_size = vn_sizeof_vkBindImageMemory(device, image, memory, memoryOffset);
    const size_t reply_size = vn_sizeof_vkBindImageMemory_reply(device, image, memory, memoryOffset);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkBindImageMemory(instance_cs, cmd_flags, device, image, memory, memoryOffset);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkBindImageMemory_reply(&parser, device, image, memory, memoryOffset);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkBindImageMemory(struct vn_instance *vn_instance, VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    const size_t cmd_size = vn_sizeof_vkBindImageMemory(device, image, memory, memoryOffset);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkBindImageMemory(cs, cmd_flags, device, image, memory, memoryOffset);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetImageSparseMemoryRequirements(struct vn_instance *vn_instance, VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    const size_t reply_size = vn_sizeof_vkGetImageSparseMemoryRequirements_reply(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetImageSparseMemoryRequirements(instance_cs, cmd_flags, device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetImageSparseMemoryRequirements_reply(&parser, device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetImageSparseMemoryRequirements(struct vn_instance *vn_instance, VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetImageSparseMemoryRequirements(cs, cmd_flags, device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceSparseImageFormatProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceSparseImageFormatProperties_reply(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceSparseImageFormatProperties(instance_cs, cmd_flags, physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceSparseImageFormatProperties_reply(&parser, physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceSparseImageFormatProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceSparseImageFormatProperties(cs, cmd_flags, physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkQueueBindSparse(struct vn_instance *vn_instance, VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence)
{
    const size_t cmd_size = vn_sizeof_vkQueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
    const size_t reply_size = vn_sizeof_vkQueueBindSparse_reply(queue, bindInfoCount, pBindInfo, fence);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkQueueBindSparse(instance_cs, cmd_flags, queue, bindInfoCount, pBindInfo, fence);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkQueueBindSparse_reply(&parser, queue, bindInfoCount, pBindInfo, fence);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkQueueBindSparse(struct vn_instance *vn_instance, VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence)
{
    const size_t cmd_size = vn_sizeof_vkQueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkQueueBindSparse(cs, cmd_flags, queue, bindInfoCount, pBindInfo, fence);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateFence(struct vn_instance *vn_instance, VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence)
{
    const size_t cmd_size = vn_sizeof_vkCreateFence(device, pCreateInfo, pAllocator, pFence);
    const size_t reply_size = vn_sizeof_vkCreateFence_reply(device, pCreateInfo, pAllocator, pFence);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateFence(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pFence);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateFence_reply(&parser, device, pCreateInfo, pAllocator, pFence);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateFence(struct vn_instance *vn_instance, VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence)
{
    const size_t cmd_size = vn_sizeof_vkCreateFence(device, pCreateInfo, pAllocator, pFence);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateFence(cs, cmd_flags, device, pCreateInfo, pAllocator, pFence);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyFence(struct vn_instance *vn_instance, VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyFence(device, fence, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyFence_reply(device, fence, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyFence(instance_cs, cmd_flags, device, fence, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyFence_reply(&parser, device, fence, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyFence(struct vn_instance *vn_instance, VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyFence(device, fence, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyFence(cs, cmd_flags, device, fence, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkResetFences(struct vn_instance *vn_instance, VkDevice device, uint32_t fenceCount, const VkFence* pFences)
{
    const size_t cmd_size = vn_sizeof_vkResetFences(device, fenceCount, pFences);
    const size_t reply_size = vn_sizeof_vkResetFences_reply(device, fenceCount, pFences);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkResetFences(instance_cs, cmd_flags, device, fenceCount, pFences);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkResetFences_reply(&parser, device, fenceCount, pFences);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkResetFences(struct vn_instance *vn_instance, VkDevice device, uint32_t fenceCount, const VkFence* pFences)
{
    const size_t cmd_size = vn_sizeof_vkResetFences(device, fenceCount, pFences);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkResetFences(cs, cmd_flags, device, fenceCount, pFences);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetFenceStatus(struct vn_instance *vn_instance, VkDevice device, VkFence fence)
{
    const size_t cmd_size = vn_sizeof_vkGetFenceStatus(device, fence);
    const size_t reply_size = vn_sizeof_vkGetFenceStatus_reply(device, fence);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetFenceStatus(instance_cs, cmd_flags, device, fence);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetFenceStatus_reply(&parser, device, fence);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetFenceStatus(struct vn_instance *vn_instance, VkDevice device, VkFence fence)
{
    const size_t cmd_size = vn_sizeof_vkGetFenceStatus(device, fence);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetFenceStatus(cs, cmd_flags, device, fence);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkWaitForFences(struct vn_instance *vn_instance, VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout)
{
    const size_t cmd_size = vn_sizeof_vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
    const size_t reply_size = vn_sizeof_vkWaitForFences_reply(device, fenceCount, pFences, waitAll, timeout);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkWaitForFences(instance_cs, cmd_flags, device, fenceCount, pFences, waitAll, timeout);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkWaitForFences_reply(&parser, device, fenceCount, pFences, waitAll, timeout);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkWaitForFences(struct vn_instance *vn_instance, VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout)
{
    const size_t cmd_size = vn_sizeof_vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkWaitForFences(cs, cmd_flags, device, fenceCount, pFences, waitAll, timeout);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateSemaphore(struct vn_instance *vn_instance, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore)
{
    const size_t cmd_size = vn_sizeof_vkCreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
    const size_t reply_size = vn_sizeof_vkCreateSemaphore_reply(device, pCreateInfo, pAllocator, pSemaphore);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateSemaphore(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pSemaphore);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateSemaphore_reply(&parser, device, pCreateInfo, pAllocator, pSemaphore);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateSemaphore(struct vn_instance *vn_instance, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore)
{
    const size_t cmd_size = vn_sizeof_vkCreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateSemaphore(cs, cmd_flags, device, pCreateInfo, pAllocator, pSemaphore);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroySemaphore(struct vn_instance *vn_instance, VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroySemaphore(device, semaphore, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroySemaphore_reply(device, semaphore, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroySemaphore(instance_cs, cmd_flags, device, semaphore, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroySemaphore_reply(&parser, device, semaphore, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroySemaphore(struct vn_instance *vn_instance, VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroySemaphore(device, semaphore, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroySemaphore(cs, cmd_flags, device, semaphore, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateEvent(struct vn_instance *vn_instance, VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent)
{
    const size_t cmd_size = vn_sizeof_vkCreateEvent(device, pCreateInfo, pAllocator, pEvent);
    const size_t reply_size = vn_sizeof_vkCreateEvent_reply(device, pCreateInfo, pAllocator, pEvent);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateEvent(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pEvent);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateEvent_reply(&parser, device, pCreateInfo, pAllocator, pEvent);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateEvent(struct vn_instance *vn_instance, VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent)
{
    const size_t cmd_size = vn_sizeof_vkCreateEvent(device, pCreateInfo, pAllocator, pEvent);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateEvent(cs, cmd_flags, device, pCreateInfo, pAllocator, pEvent);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyEvent(struct vn_instance *vn_instance, VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyEvent(device, event, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyEvent_reply(device, event, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyEvent(instance_cs, cmd_flags, device, event, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyEvent_reply(&parser, device, event, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyEvent(struct vn_instance *vn_instance, VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyEvent(device, event, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyEvent(cs, cmd_flags, device, event, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetEventStatus(struct vn_instance *vn_instance, VkDevice device, VkEvent event)
{
    const size_t cmd_size = vn_sizeof_vkGetEventStatus(device, event);
    const size_t reply_size = vn_sizeof_vkGetEventStatus_reply(device, event);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetEventStatus(instance_cs, cmd_flags, device, event);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetEventStatus_reply(&parser, device, event);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetEventStatus(struct vn_instance *vn_instance, VkDevice device, VkEvent event)
{
    const size_t cmd_size = vn_sizeof_vkGetEventStatus(device, event);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetEventStatus(cs, cmd_flags, device, event);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkSetEvent(struct vn_instance *vn_instance, VkDevice device, VkEvent event)
{
    const size_t cmd_size = vn_sizeof_vkSetEvent(device, event);
    const size_t reply_size = vn_sizeof_vkSetEvent_reply(device, event);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkSetEvent(instance_cs, cmd_flags, device, event);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkSetEvent_reply(&parser, device, event);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkSetEvent(struct vn_instance *vn_instance, VkDevice device, VkEvent event)
{
    const size_t cmd_size = vn_sizeof_vkSetEvent(device, event);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkSetEvent(cs, cmd_flags, device, event);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkResetEvent(struct vn_instance *vn_instance, VkDevice device, VkEvent event)
{
    const size_t cmd_size = vn_sizeof_vkResetEvent(device, event);
    const size_t reply_size = vn_sizeof_vkResetEvent_reply(device, event);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkResetEvent(instance_cs, cmd_flags, device, event);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkResetEvent_reply(&parser, device, event);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkResetEvent(struct vn_instance *vn_instance, VkDevice device, VkEvent event)
{
    const size_t cmd_size = vn_sizeof_vkResetEvent(device, event);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkResetEvent(cs, cmd_flags, device, event);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateQueryPool(struct vn_instance *vn_instance, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool)
{
    const size_t cmd_size = vn_sizeof_vkCreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
    const size_t reply_size = vn_sizeof_vkCreateQueryPool_reply(device, pCreateInfo, pAllocator, pQueryPool);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateQueryPool(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pQueryPool);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateQueryPool_reply(&parser, device, pCreateInfo, pAllocator, pQueryPool);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateQueryPool(struct vn_instance *vn_instance, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool)
{
    const size_t cmd_size = vn_sizeof_vkCreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateQueryPool(cs, cmd_flags, device, pCreateInfo, pAllocator, pQueryPool);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyQueryPool(struct vn_instance *vn_instance, VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyQueryPool(device, queryPool, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyQueryPool_reply(device, queryPool, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyQueryPool(instance_cs, cmd_flags, device, queryPool, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyQueryPool_reply(&parser, device, queryPool, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyQueryPool(struct vn_instance *vn_instance, VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyQueryPool(device, queryPool, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyQueryPool(cs, cmd_flags, device, queryPool, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetQueryPoolResults(struct vn_instance *vn_instance, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
    const size_t reply_size = vn_sizeof_vkGetQueryPoolResults_reply(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetQueryPoolResults(instance_cs, cmd_flags, device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetQueryPoolResults_reply(&parser, device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetQueryPoolResults(struct vn_instance *vn_instance, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetQueryPoolResults(cs, cmd_flags, device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkResetQueryPool(struct vn_instance *vn_instance, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    const size_t cmd_size = vn_sizeof_vkResetQueryPool(device, queryPool, firstQuery, queryCount);
    const size_t reply_size = vn_sizeof_vkResetQueryPool_reply(device, queryPool, firstQuery, queryCount);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkResetQueryPool(instance_cs, cmd_flags, device, queryPool, firstQuery, queryCount);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkResetQueryPool_reply(&parser, device, queryPool, firstQuery, queryCount);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkResetQueryPool(struct vn_instance *vn_instance, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    const size_t cmd_size = vn_sizeof_vkResetQueryPool(device, queryPool, firstQuery, queryCount);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkResetQueryPool(cs, cmd_flags, device, queryPool, firstQuery, queryCount);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateBuffer(struct vn_instance *vn_instance, VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
    const size_t cmd_size = vn_sizeof_vkCreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
    const size_t reply_size = vn_sizeof_vkCreateBuffer_reply(device, pCreateInfo, pAllocator, pBuffer);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateBuffer(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pBuffer);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateBuffer_reply(&parser, device, pCreateInfo, pAllocator, pBuffer);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateBuffer(struct vn_instance *vn_instance, VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
    const size_t cmd_size = vn_sizeof_vkCreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateBuffer(cs, cmd_flags, device, pCreateInfo, pAllocator, pBuffer);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyBuffer(struct vn_instance *vn_instance, VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyBuffer(device, buffer, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyBuffer_reply(device, buffer, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyBuffer(instance_cs, cmd_flags, device, buffer, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyBuffer_reply(&parser, device, buffer, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyBuffer(struct vn_instance *vn_instance, VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyBuffer(device, buffer, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyBuffer(cs, cmd_flags, device, buffer, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateBufferView(struct vn_instance *vn_instance, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView)
{
    const size_t cmd_size = vn_sizeof_vkCreateBufferView(device, pCreateInfo, pAllocator, pView);
    const size_t reply_size = vn_sizeof_vkCreateBufferView_reply(device, pCreateInfo, pAllocator, pView);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateBufferView(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pView);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateBufferView_reply(&parser, device, pCreateInfo, pAllocator, pView);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateBufferView(struct vn_instance *vn_instance, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView)
{
    const size_t cmd_size = vn_sizeof_vkCreateBufferView(device, pCreateInfo, pAllocator, pView);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateBufferView(cs, cmd_flags, device, pCreateInfo, pAllocator, pView);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyBufferView(struct vn_instance *vn_instance, VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyBufferView(device, bufferView, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyBufferView_reply(device, bufferView, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyBufferView(instance_cs, cmd_flags, device, bufferView, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyBufferView_reply(&parser, device, bufferView, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyBufferView(struct vn_instance *vn_instance, VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyBufferView(device, bufferView, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyBufferView(cs, cmd_flags, device, bufferView, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateImage(struct vn_instance *vn_instance, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
    const size_t cmd_size = vn_sizeof_vkCreateImage(device, pCreateInfo, pAllocator, pImage);
    const size_t reply_size = vn_sizeof_vkCreateImage_reply(device, pCreateInfo, pAllocator, pImage);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateImage(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pImage);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateImage_reply(&parser, device, pCreateInfo, pAllocator, pImage);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateImage(struct vn_instance *vn_instance, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
    const size_t cmd_size = vn_sizeof_vkCreateImage(device, pCreateInfo, pAllocator, pImage);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateImage(cs, cmd_flags, device, pCreateInfo, pAllocator, pImage);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyImage(struct vn_instance *vn_instance, VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyImage(device, image, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyImage_reply(device, image, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyImage(instance_cs, cmd_flags, device, image, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyImage_reply(&parser, device, image, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyImage(struct vn_instance *vn_instance, VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyImage(device, image, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyImage(cs, cmd_flags, device, image, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetImageSubresourceLayout(struct vn_instance *vn_instance, VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout)
{
    const size_t cmd_size = vn_sizeof_vkGetImageSubresourceLayout(device, image, pSubresource, pLayout);
    const size_t reply_size = vn_sizeof_vkGetImageSubresourceLayout_reply(device, image, pSubresource, pLayout);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetImageSubresourceLayout(instance_cs, cmd_flags, device, image, pSubresource, pLayout);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetImageSubresourceLayout_reply(&parser, device, image, pSubresource, pLayout);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetImageSubresourceLayout(struct vn_instance *vn_instance, VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout)
{
    const size_t cmd_size = vn_sizeof_vkGetImageSubresourceLayout(device, image, pSubresource, pLayout);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetImageSubresourceLayout(cs, cmd_flags, device, image, pSubresource, pLayout);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateImageView(struct vn_instance *vn_instance, VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView)
{
    const size_t cmd_size = vn_sizeof_vkCreateImageView(device, pCreateInfo, pAllocator, pView);
    const size_t reply_size = vn_sizeof_vkCreateImageView_reply(device, pCreateInfo, pAllocator, pView);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateImageView(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pView);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateImageView_reply(&parser, device, pCreateInfo, pAllocator, pView);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateImageView(struct vn_instance *vn_instance, VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView)
{
    const size_t cmd_size = vn_sizeof_vkCreateImageView(device, pCreateInfo, pAllocator, pView);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateImageView(cs, cmd_flags, device, pCreateInfo, pAllocator, pView);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyImageView(struct vn_instance *vn_instance, VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyImageView(device, imageView, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyImageView_reply(device, imageView, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyImageView(instance_cs, cmd_flags, device, imageView, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyImageView_reply(&parser, device, imageView, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyImageView(struct vn_instance *vn_instance, VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyImageView(device, imageView, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyImageView(cs, cmd_flags, device, imageView, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateShaderModule(struct vn_instance *vn_instance, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule)
{
    const size_t cmd_size = vn_sizeof_vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
    const size_t reply_size = vn_sizeof_vkCreateShaderModule_reply(device, pCreateInfo, pAllocator, pShaderModule);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateShaderModule(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pShaderModule);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateShaderModule_reply(&parser, device, pCreateInfo, pAllocator, pShaderModule);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateShaderModule(struct vn_instance *vn_instance, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule)
{
    const size_t cmd_size = vn_sizeof_vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateShaderModule(cs, cmd_flags, device, pCreateInfo, pAllocator, pShaderModule);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyShaderModule(struct vn_instance *vn_instance, VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyShaderModule(device, shaderModule, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyShaderModule_reply(device, shaderModule, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyShaderModule(instance_cs, cmd_flags, device, shaderModule, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyShaderModule_reply(&parser, device, shaderModule, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyShaderModule(struct vn_instance *vn_instance, VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyShaderModule(device, shaderModule, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyShaderModule(cs, cmd_flags, device, shaderModule, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreatePipelineCache(struct vn_instance *vn_instance, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache)
{
    const size_t cmd_size = vn_sizeof_vkCreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
    const size_t reply_size = vn_sizeof_vkCreatePipelineCache_reply(device, pCreateInfo, pAllocator, pPipelineCache);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreatePipelineCache(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pPipelineCache);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreatePipelineCache_reply(&parser, device, pCreateInfo, pAllocator, pPipelineCache);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreatePipelineCache(struct vn_instance *vn_instance, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache)
{
    const size_t cmd_size = vn_sizeof_vkCreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreatePipelineCache(cs, cmd_flags, device, pCreateInfo, pAllocator, pPipelineCache);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyPipelineCache(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyPipelineCache(device, pipelineCache, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyPipelineCache_reply(device, pipelineCache, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyPipelineCache(instance_cs, cmd_flags, device, pipelineCache, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyPipelineCache_reply(&parser, device, pipelineCache, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyPipelineCache(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyPipelineCache(device, pipelineCache, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyPipelineCache(cs, cmd_flags, device, pipelineCache, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetPipelineCacheData(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData)
{
    const size_t cmd_size = vn_sizeof_vkGetPipelineCacheData(device, pipelineCache, pDataSize, pData);
    const size_t reply_size = vn_sizeof_vkGetPipelineCacheData_reply(device, pipelineCache, pDataSize, pData);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPipelineCacheData(instance_cs, cmd_flags, device, pipelineCache, pDataSize, pData);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetPipelineCacheData_reply(&parser, device, pipelineCache, pDataSize, pData);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetPipelineCacheData(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData)
{
    const size_t cmd_size = vn_sizeof_vkGetPipelineCacheData(device, pipelineCache, pDataSize, pData);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPipelineCacheData(cs, cmd_flags, device, pipelineCache, pDataSize, pData);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkMergePipelineCaches(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches)
{
    const size_t cmd_size = vn_sizeof_vkMergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
    const size_t reply_size = vn_sizeof_vkMergePipelineCaches_reply(device, dstCache, srcCacheCount, pSrcCaches);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkMergePipelineCaches(instance_cs, cmd_flags, device, dstCache, srcCacheCount, pSrcCaches);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkMergePipelineCaches_reply(&parser, device, dstCache, srcCacheCount, pSrcCaches);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkMergePipelineCaches(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches)
{
    const size_t cmd_size = vn_sizeof_vkMergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkMergePipelineCaches(cs, cmd_flags, device, dstCache, srcCacheCount, pSrcCaches);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateGraphicsPipelines(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
    const size_t cmd_size = vn_sizeof_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    const size_t reply_size = vn_sizeof_vkCreateGraphicsPipelines_reply(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateGraphicsPipelines(instance_cs, cmd_flags, device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateGraphicsPipelines_reply(&parser, device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateGraphicsPipelines(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
    const size_t cmd_size = vn_sizeof_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateGraphicsPipelines(cs, cmd_flags, device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateComputePipelines(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
    const size_t cmd_size = vn_sizeof_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    const size_t reply_size = vn_sizeof_vkCreateComputePipelines_reply(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateComputePipelines(instance_cs, cmd_flags, device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateComputePipelines_reply(&parser, device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateComputePipelines(struct vn_instance *vn_instance, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
    const size_t cmd_size = vn_sizeof_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateComputePipelines(cs, cmd_flags, device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyPipeline(struct vn_instance *vn_instance, VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyPipeline(device, pipeline, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyPipeline_reply(device, pipeline, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyPipeline(instance_cs, cmd_flags, device, pipeline, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyPipeline_reply(&parser, device, pipeline, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyPipeline(struct vn_instance *vn_instance, VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyPipeline(device, pipeline, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyPipeline(cs, cmd_flags, device, pipeline, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreatePipelineLayout(struct vn_instance *vn_instance, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout)
{
    const size_t cmd_size = vn_sizeof_vkCreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
    const size_t reply_size = vn_sizeof_vkCreatePipelineLayout_reply(device, pCreateInfo, pAllocator, pPipelineLayout);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreatePipelineLayout(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pPipelineLayout);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreatePipelineLayout_reply(&parser, device, pCreateInfo, pAllocator, pPipelineLayout);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreatePipelineLayout(struct vn_instance *vn_instance, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout)
{
    const size_t cmd_size = vn_sizeof_vkCreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreatePipelineLayout(cs, cmd_flags, device, pCreateInfo, pAllocator, pPipelineLayout);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyPipelineLayout(struct vn_instance *vn_instance, VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyPipelineLayout(device, pipelineLayout, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyPipelineLayout_reply(device, pipelineLayout, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyPipelineLayout(instance_cs, cmd_flags, device, pipelineLayout, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyPipelineLayout_reply(&parser, device, pipelineLayout, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyPipelineLayout(struct vn_instance *vn_instance, VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyPipelineLayout(device, pipelineLayout, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyPipelineLayout(cs, cmd_flags, device, pipelineLayout, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateSampler(struct vn_instance *vn_instance, VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler)
{
    const size_t cmd_size = vn_sizeof_vkCreateSampler(device, pCreateInfo, pAllocator, pSampler);
    const size_t reply_size = vn_sizeof_vkCreateSampler_reply(device, pCreateInfo, pAllocator, pSampler);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateSampler(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pSampler);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateSampler_reply(&parser, device, pCreateInfo, pAllocator, pSampler);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateSampler(struct vn_instance *vn_instance, VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler)
{
    const size_t cmd_size = vn_sizeof_vkCreateSampler(device, pCreateInfo, pAllocator, pSampler);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateSampler(cs, cmd_flags, device, pCreateInfo, pAllocator, pSampler);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroySampler(struct vn_instance *vn_instance, VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroySampler(device, sampler, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroySampler_reply(device, sampler, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroySampler(instance_cs, cmd_flags, device, sampler, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroySampler_reply(&parser, device, sampler, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroySampler(struct vn_instance *vn_instance, VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroySampler(device, sampler, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroySampler(cs, cmd_flags, device, sampler, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateDescriptorSetLayout(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout)
{
    const size_t cmd_size = vn_sizeof_vkCreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
    const size_t reply_size = vn_sizeof_vkCreateDescriptorSetLayout_reply(device, pCreateInfo, pAllocator, pSetLayout);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateDescriptorSetLayout(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pSetLayout);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateDescriptorSetLayout_reply(&parser, device, pCreateInfo, pAllocator, pSetLayout);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateDescriptorSetLayout(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout)
{
    const size_t cmd_size = vn_sizeof_vkCreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateDescriptorSetLayout(cs, cmd_flags, device, pCreateInfo, pAllocator, pSetLayout);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyDescriptorSetLayout(struct vn_instance *vn_instance, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyDescriptorSetLayout_reply(device, descriptorSetLayout, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyDescriptorSetLayout(instance_cs, cmd_flags, device, descriptorSetLayout, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyDescriptorSetLayout_reply(&parser, device, descriptorSetLayout, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyDescriptorSetLayout(struct vn_instance *vn_instance, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyDescriptorSetLayout(cs, cmd_flags, device, descriptorSetLayout, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateDescriptorPool(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool)
{
    const size_t cmd_size = vn_sizeof_vkCreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
    const size_t reply_size = vn_sizeof_vkCreateDescriptorPool_reply(device, pCreateInfo, pAllocator, pDescriptorPool);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateDescriptorPool(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pDescriptorPool);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateDescriptorPool_reply(&parser, device, pCreateInfo, pAllocator, pDescriptorPool);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateDescriptorPool(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool)
{
    const size_t cmd_size = vn_sizeof_vkCreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateDescriptorPool(cs, cmd_flags, device, pCreateInfo, pAllocator, pDescriptorPool);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyDescriptorPool(struct vn_instance *vn_instance, VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDescriptorPool(device, descriptorPool, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyDescriptorPool_reply(device, descriptorPool, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyDescriptorPool(instance_cs, cmd_flags, device, descriptorPool, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyDescriptorPool_reply(&parser, device, descriptorPool, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyDescriptorPool(struct vn_instance *vn_instance, VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDescriptorPool(device, descriptorPool, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyDescriptorPool(cs, cmd_flags, device, descriptorPool, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkResetDescriptorPool(struct vn_instance *vn_instance, VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkResetDescriptorPool(device, descriptorPool, flags);
    const size_t reply_size = vn_sizeof_vkResetDescriptorPool_reply(device, descriptorPool, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkResetDescriptorPool(instance_cs, cmd_flags, device, descriptorPool, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkResetDescriptorPool_reply(&parser, device, descriptorPool, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkResetDescriptorPool(struct vn_instance *vn_instance, VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkResetDescriptorPool(device, descriptorPool, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkResetDescriptorPool(cs, cmd_flags, device, descriptorPool, flags);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkAllocateDescriptorSets(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets)
{
    const size_t cmd_size = vn_sizeof_vkAllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
    const size_t reply_size = vn_sizeof_vkAllocateDescriptorSets_reply(device, pAllocateInfo, pDescriptorSets);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkAllocateDescriptorSets(instance_cs, cmd_flags, device, pAllocateInfo, pDescriptorSets);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkAllocateDescriptorSets_reply(&parser, device, pAllocateInfo, pDescriptorSets);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkAllocateDescriptorSets(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets)
{
    const size_t cmd_size = vn_sizeof_vkAllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkAllocateDescriptorSets(cs, cmd_flags, device, pAllocateInfo, pDescriptorSets);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkFreeDescriptorSets(struct vn_instance *vn_instance, VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets)
{
    const size_t cmd_size = vn_sizeof_vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
    const size_t reply_size = vn_sizeof_vkFreeDescriptorSets_reply(device, descriptorPool, descriptorSetCount, pDescriptorSets);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkFreeDescriptorSets(instance_cs, cmd_flags, device, descriptorPool, descriptorSetCount, pDescriptorSets);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkFreeDescriptorSets_reply(&parser, device, descriptorPool, descriptorSetCount, pDescriptorSets);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkFreeDescriptorSets(struct vn_instance *vn_instance, VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets)
{
    const size_t cmd_size = vn_sizeof_vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkFreeDescriptorSets(cs, cmd_flags, device, descriptorPool, descriptorSetCount, pDescriptorSets);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkUpdateDescriptorSets(struct vn_instance *vn_instance, VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies)
{
    const size_t cmd_size = vn_sizeof_vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    const size_t reply_size = vn_sizeof_vkUpdateDescriptorSets_reply(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkUpdateDescriptorSets(instance_cs, cmd_flags, device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkUpdateDescriptorSets_reply(&parser, device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkUpdateDescriptorSets(struct vn_instance *vn_instance, VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies)
{
    const size_t cmd_size = vn_sizeof_vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkUpdateDescriptorSets(cs, cmd_flags, device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateFramebuffer(struct vn_instance *vn_instance, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer)
{
    const size_t cmd_size = vn_sizeof_vkCreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
    const size_t reply_size = vn_sizeof_vkCreateFramebuffer_reply(device, pCreateInfo, pAllocator, pFramebuffer);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateFramebuffer(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pFramebuffer);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateFramebuffer_reply(&parser, device, pCreateInfo, pAllocator, pFramebuffer);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateFramebuffer(struct vn_instance *vn_instance, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer)
{
    const size_t cmd_size = vn_sizeof_vkCreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateFramebuffer(cs, cmd_flags, device, pCreateInfo, pAllocator, pFramebuffer);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyFramebuffer(struct vn_instance *vn_instance, VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyFramebuffer(device, framebuffer, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyFramebuffer_reply(device, framebuffer, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyFramebuffer(instance_cs, cmd_flags, device, framebuffer, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyFramebuffer_reply(&parser, device, framebuffer, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyFramebuffer(struct vn_instance *vn_instance, VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyFramebuffer(device, framebuffer, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyFramebuffer(cs, cmd_flags, device, framebuffer, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateRenderPass(struct vn_instance *vn_instance, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
{
    const size_t cmd_size = vn_sizeof_vkCreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
    const size_t reply_size = vn_sizeof_vkCreateRenderPass_reply(device, pCreateInfo, pAllocator, pRenderPass);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateRenderPass(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pRenderPass);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateRenderPass_reply(&parser, device, pCreateInfo, pAllocator, pRenderPass);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateRenderPass(struct vn_instance *vn_instance, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
{
    const size_t cmd_size = vn_sizeof_vkCreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateRenderPass(cs, cmd_flags, device, pCreateInfo, pAllocator, pRenderPass);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyRenderPass(struct vn_instance *vn_instance, VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyRenderPass(device, renderPass, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyRenderPass_reply(device, renderPass, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyRenderPass(instance_cs, cmd_flags, device, renderPass, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyRenderPass_reply(&parser, device, renderPass, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyRenderPass(struct vn_instance *vn_instance, VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyRenderPass(device, renderPass, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyRenderPass(cs, cmd_flags, device, renderPass, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetRenderAreaGranularity(struct vn_instance *vn_instance, VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity)
{
    const size_t cmd_size = vn_sizeof_vkGetRenderAreaGranularity(device, renderPass, pGranularity);
    const size_t reply_size = vn_sizeof_vkGetRenderAreaGranularity_reply(device, renderPass, pGranularity);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetRenderAreaGranularity(instance_cs, cmd_flags, device, renderPass, pGranularity);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetRenderAreaGranularity_reply(&parser, device, renderPass, pGranularity);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetRenderAreaGranularity(struct vn_instance *vn_instance, VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity)
{
    const size_t cmd_size = vn_sizeof_vkGetRenderAreaGranularity(device, renderPass, pGranularity);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetRenderAreaGranularity(cs, cmd_flags, device, renderPass, pGranularity);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateCommandPool(struct vn_instance *vn_instance, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool)
{
    const size_t cmd_size = vn_sizeof_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
    const size_t reply_size = vn_sizeof_vkCreateCommandPool_reply(device, pCreateInfo, pAllocator, pCommandPool);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateCommandPool(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pCommandPool);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateCommandPool_reply(&parser, device, pCreateInfo, pAllocator, pCommandPool);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateCommandPool(struct vn_instance *vn_instance, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool)
{
    const size_t cmd_size = vn_sizeof_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateCommandPool(cs, cmd_flags, device, pCreateInfo, pAllocator, pCommandPool);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyCommandPool(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyCommandPool(device, commandPool, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyCommandPool_reply(device, commandPool, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyCommandPool(instance_cs, cmd_flags, device, commandPool, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyCommandPool_reply(&parser, device, commandPool, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyCommandPool(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyCommandPool(device, commandPool, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyCommandPool(cs, cmd_flags, device, commandPool, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkResetCommandPool(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkResetCommandPool(device, commandPool, flags);
    const size_t reply_size = vn_sizeof_vkResetCommandPool_reply(device, commandPool, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkResetCommandPool(instance_cs, cmd_flags, device, commandPool, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkResetCommandPool_reply(&parser, device, commandPool, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkResetCommandPool(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkResetCommandPool(device, commandPool, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkResetCommandPool(cs, cmd_flags, device, commandPool, flags);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkAllocateCommandBuffers(struct vn_instance *vn_instance, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers)
{
    const size_t cmd_size = vn_sizeof_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
    const size_t reply_size = vn_sizeof_vkAllocateCommandBuffers_reply(device, pAllocateInfo, pCommandBuffers);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkAllocateCommandBuffers(instance_cs, cmd_flags, device, pAllocateInfo, pCommandBuffers);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkAllocateCommandBuffers_reply(&parser, device, pAllocateInfo, pCommandBuffers);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkAllocateCommandBuffers(struct vn_instance *vn_instance, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers)
{
    const size_t cmd_size = vn_sizeof_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkAllocateCommandBuffers(cs, cmd_flags, device, pAllocateInfo, pCommandBuffers);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkFreeCommandBuffers(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
    const size_t cmd_size = vn_sizeof_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
    const size_t reply_size = vn_sizeof_vkFreeCommandBuffers_reply(device, commandPool, commandBufferCount, pCommandBuffers);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkFreeCommandBuffers(instance_cs, cmd_flags, device, commandPool, commandBufferCount, pCommandBuffers);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkFreeCommandBuffers_reply(&parser, device, commandPool, commandBufferCount, pCommandBuffers);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkFreeCommandBuffers(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
    const size_t cmd_size = vn_sizeof_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkFreeCommandBuffers(cs, cmd_flags, device, commandPool, commandBufferCount, pCommandBuffers);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkBeginCommandBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo)
{
    const size_t cmd_size = vn_sizeof_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
    const size_t reply_size = vn_sizeof_vkBeginCommandBuffer_reply(commandBuffer, pBeginInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkBeginCommandBuffer(instance_cs, cmd_flags, commandBuffer, pBeginInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkBeginCommandBuffer_reply(&parser, commandBuffer, pBeginInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkBeginCommandBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo)
{
    const size_t cmd_size = vn_sizeof_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkBeginCommandBuffer(cs, cmd_flags, commandBuffer, pBeginInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEndCommandBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer)
{
    const size_t cmd_size = vn_sizeof_vkEndCommandBuffer(commandBuffer);
    const size_t reply_size = vn_sizeof_vkEndCommandBuffer_reply(commandBuffer);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEndCommandBuffer(instance_cs, cmd_flags, commandBuffer);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEndCommandBuffer_reply(&parser, commandBuffer);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEndCommandBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer)
{
    const size_t cmd_size = vn_sizeof_vkEndCommandBuffer(commandBuffer);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEndCommandBuffer(cs, cmd_flags, commandBuffer);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkResetCommandBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkResetCommandBuffer(commandBuffer, flags);
    const size_t reply_size = vn_sizeof_vkResetCommandBuffer_reply(commandBuffer, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkResetCommandBuffer(instance_cs, cmd_flags, commandBuffer, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkResetCommandBuffer_reply(&parser, commandBuffer, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkResetCommandBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkResetCommandBuffer(commandBuffer, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkResetCommandBuffer(cs, cmd_flags, commandBuffer, flags);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBindPipeline(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    const size_t reply_size = vn_sizeof_vkCmdBindPipeline_reply(commandBuffer, pipelineBindPoint, pipeline);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBindPipeline(instance_cs, cmd_flags, commandBuffer, pipelineBindPoint, pipeline);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBindPipeline_reply(&parser, commandBuffer, pipelineBindPoint, pipeline);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBindPipeline(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBindPipeline(cs, cmd_flags, commandBuffer, pipelineBindPoint, pipeline);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetViewport(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
    const size_t reply_size = vn_sizeof_vkCmdSetViewport_reply(commandBuffer, firstViewport, viewportCount, pViewports);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetViewport(instance_cs, cmd_flags, commandBuffer, firstViewport, viewportCount, pViewports);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetViewport_reply(&parser, commandBuffer, firstViewport, viewportCount, pViewports);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetViewport(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetViewport(cs, cmd_flags, commandBuffer, firstViewport, viewportCount, pViewports);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetScissor(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
    const size_t reply_size = vn_sizeof_vkCmdSetScissor_reply(commandBuffer, firstScissor, scissorCount, pScissors);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetScissor(instance_cs, cmd_flags, commandBuffer, firstScissor, scissorCount, pScissors);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetScissor_reply(&parser, commandBuffer, firstScissor, scissorCount, pScissors);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetScissor(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetScissor(cs, cmd_flags, commandBuffer, firstScissor, scissorCount, pScissors);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetLineWidth(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, float lineWidth)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetLineWidth(commandBuffer, lineWidth);
    const size_t reply_size = vn_sizeof_vkCmdSetLineWidth_reply(commandBuffer, lineWidth);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetLineWidth(instance_cs, cmd_flags, commandBuffer, lineWidth);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetLineWidth_reply(&parser, commandBuffer, lineWidth);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetLineWidth(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, float lineWidth)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetLineWidth(commandBuffer, lineWidth);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetLineWidth(cs, cmd_flags, commandBuffer, lineWidth);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetDepthBias(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    const size_t reply_size = vn_sizeof_vkCmdSetDepthBias_reply(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetDepthBias(instance_cs, cmd_flags, commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetDepthBias_reply(&parser, commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetDepthBias(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetDepthBias(cs, cmd_flags, commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetBlendConstants(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const float blendConstants[4])
{
    const size_t cmd_size = vn_sizeof_vkCmdSetBlendConstants(commandBuffer, blendConstants);
    const size_t reply_size = vn_sizeof_vkCmdSetBlendConstants_reply(commandBuffer, blendConstants);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetBlendConstants(instance_cs, cmd_flags, commandBuffer, blendConstants);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetBlendConstants_reply(&parser, commandBuffer, blendConstants);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetBlendConstants(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const float blendConstants[4])
{
    const size_t cmd_size = vn_sizeof_vkCmdSetBlendConstants(commandBuffer, blendConstants);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetBlendConstants(cs, cmd_flags, commandBuffer, blendConstants);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetDepthBounds(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
    const size_t reply_size = vn_sizeof_vkCmdSetDepthBounds_reply(commandBuffer, minDepthBounds, maxDepthBounds);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetDepthBounds(instance_cs, cmd_flags, commandBuffer, minDepthBounds, maxDepthBounds);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetDepthBounds_reply(&parser, commandBuffer, minDepthBounds, maxDepthBounds);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetDepthBounds(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetDepthBounds(cs, cmd_flags, commandBuffer, minDepthBounds, maxDepthBounds);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetStencilCompareMask(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
    const size_t reply_size = vn_sizeof_vkCmdSetStencilCompareMask_reply(commandBuffer, faceMask, compareMask);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetStencilCompareMask(instance_cs, cmd_flags, commandBuffer, faceMask, compareMask);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetStencilCompareMask_reply(&parser, commandBuffer, faceMask, compareMask);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetStencilCompareMask(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetStencilCompareMask(cs, cmd_flags, commandBuffer, faceMask, compareMask);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetStencilWriteMask(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
    const size_t reply_size = vn_sizeof_vkCmdSetStencilWriteMask_reply(commandBuffer, faceMask, writeMask);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetStencilWriteMask(instance_cs, cmd_flags, commandBuffer, faceMask, writeMask);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetStencilWriteMask_reply(&parser, commandBuffer, faceMask, writeMask);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetStencilWriteMask(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetStencilWriteMask(cs, cmd_flags, commandBuffer, faceMask, writeMask);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetStencilReference(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetStencilReference(commandBuffer, faceMask, reference);
    const size_t reply_size = vn_sizeof_vkCmdSetStencilReference_reply(commandBuffer, faceMask, reference);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetStencilReference(instance_cs, cmd_flags, commandBuffer, faceMask, reference);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetStencilReference_reply(&parser, commandBuffer, faceMask, reference);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetStencilReference(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetStencilReference(commandBuffer, faceMask, reference);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetStencilReference(cs, cmd_flags, commandBuffer, faceMask, reference);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBindDescriptorSets(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    const size_t reply_size = vn_sizeof_vkCmdBindDescriptorSets_reply(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBindDescriptorSets(instance_cs, cmd_flags, commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBindDescriptorSets_reply(&parser, commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBindDescriptorSets(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBindDescriptorSets(cs, cmd_flags, commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBindIndexBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
    const size_t reply_size = vn_sizeof_vkCmdBindIndexBuffer_reply(commandBuffer, buffer, offset, indexType);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBindIndexBuffer(instance_cs, cmd_flags, commandBuffer, buffer, offset, indexType);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBindIndexBuffer_reply(&parser, commandBuffer, buffer, offset, indexType);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBindIndexBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBindIndexBuffer(cs, cmd_flags, commandBuffer, buffer, offset, indexType);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBindVertexBuffers(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    const size_t reply_size = vn_sizeof_vkCmdBindVertexBuffers_reply(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBindVertexBuffers(instance_cs, cmd_flags, commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBindVertexBuffers_reply(&parser, commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBindVertexBuffers(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBindVertexBuffers(cs, cmd_flags, commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDraw(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    const size_t cmd_size = vn_sizeof_vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    const size_t reply_size = vn_sizeof_vkCmdDraw_reply(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDraw(instance_cs, cmd_flags, commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDraw_reply(&parser, commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDraw(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    const size_t cmd_size = vn_sizeof_vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDraw(cs, cmd_flags, commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDrawIndexed(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    const size_t reply_size = vn_sizeof_vkCmdDrawIndexed_reply(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDrawIndexed(instance_cs, cmd_flags, commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDrawIndexed_reply(&parser, commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDrawIndexed(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDrawIndexed(cs, cmd_flags, commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDrawIndirect(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
    const size_t reply_size = vn_sizeof_vkCmdDrawIndirect_reply(commandBuffer, buffer, offset, drawCount, stride);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDrawIndirect(instance_cs, cmd_flags, commandBuffer, buffer, offset, drawCount, stride);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDrawIndirect_reply(&parser, commandBuffer, buffer, offset, drawCount, stride);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDrawIndirect(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDrawIndirect(cs, cmd_flags, commandBuffer, buffer, offset, drawCount, stride);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDrawIndexedIndirect(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
    const size_t reply_size = vn_sizeof_vkCmdDrawIndexedIndirect_reply(commandBuffer, buffer, offset, drawCount, stride);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDrawIndexedIndirect(instance_cs, cmd_flags, commandBuffer, buffer, offset, drawCount, stride);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDrawIndexedIndirect_reply(&parser, commandBuffer, buffer, offset, drawCount, stride);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDrawIndexedIndirect(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDrawIndexedIndirect(cs, cmd_flags, commandBuffer, buffer, offset, drawCount, stride);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDispatch(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    const size_t cmd_size = vn_sizeof_vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
    const size_t reply_size = vn_sizeof_vkCmdDispatch_reply(commandBuffer, groupCountX, groupCountY, groupCountZ);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDispatch(instance_cs, cmd_flags, commandBuffer, groupCountX, groupCountY, groupCountZ);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDispatch_reply(&parser, commandBuffer, groupCountX, groupCountY, groupCountZ);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDispatch(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    const size_t cmd_size = vn_sizeof_vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDispatch(cs, cmd_flags, commandBuffer, groupCountX, groupCountY, groupCountZ);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDispatchIndirect(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
{
    const size_t cmd_size = vn_sizeof_vkCmdDispatchIndirect(commandBuffer, buffer, offset);
    const size_t reply_size = vn_sizeof_vkCmdDispatchIndirect_reply(commandBuffer, buffer, offset);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDispatchIndirect(instance_cs, cmd_flags, commandBuffer, buffer, offset);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDispatchIndirect_reply(&parser, commandBuffer, buffer, offset);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDispatchIndirect(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
{
    const size_t cmd_size = vn_sizeof_vkCmdDispatchIndirect(commandBuffer, buffer, offset);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDispatchIndirect(cs, cmd_flags, commandBuffer, buffer, offset);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdCopyBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    const size_t reply_size = vn_sizeof_vkCmdCopyBuffer_reply(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdCopyBuffer(instance_cs, cmd_flags, commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdCopyBuffer_reply(&parser, commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdCopyBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdCopyBuffer(cs, cmd_flags, commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdCopyImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    const size_t reply_size = vn_sizeof_vkCmdCopyImage_reply(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdCopyImage(instance_cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdCopyImage_reply(&parser, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdCopyImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdCopyImage(cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBlitImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter)
{
    const size_t cmd_size = vn_sizeof_vkCmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    const size_t reply_size = vn_sizeof_vkCmdBlitImage_reply(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBlitImage(instance_cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBlitImage_reply(&parser, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBlitImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter)
{
    const size_t cmd_size = vn_sizeof_vkCmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBlitImage(cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdCopyBufferToImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    const size_t reply_size = vn_sizeof_vkCmdCopyBufferToImage_reply(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdCopyBufferToImage(instance_cs, cmd_flags, commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdCopyBufferToImage_reply(&parser, commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdCopyBufferToImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdCopyBufferToImage(cs, cmd_flags, commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdCopyImageToBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
    const size_t reply_size = vn_sizeof_vkCmdCopyImageToBuffer_reply(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdCopyImageToBuffer(instance_cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdCopyImageToBuffer_reply(&parser, commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdCopyImageToBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdCopyImageToBuffer(cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdUpdateBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData)
{
    const size_t cmd_size = vn_sizeof_vkCmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
    const size_t reply_size = vn_sizeof_vkCmdUpdateBuffer_reply(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdUpdateBuffer(instance_cs, cmd_flags, commandBuffer, dstBuffer, dstOffset, dataSize, pData);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdUpdateBuffer_reply(&parser, commandBuffer, dstBuffer, dstOffset, dataSize, pData);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdUpdateBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData)
{
    const size_t cmd_size = vn_sizeof_vkCmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdUpdateBuffer(cs, cmd_flags, commandBuffer, dstBuffer, dstOffset, dataSize, pData);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdFillBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data)
{
    const size_t cmd_size = vn_sizeof_vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
    const size_t reply_size = vn_sizeof_vkCmdFillBuffer_reply(commandBuffer, dstBuffer, dstOffset, size, data);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdFillBuffer(instance_cs, cmd_flags, commandBuffer, dstBuffer, dstOffset, size, data);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdFillBuffer_reply(&parser, commandBuffer, dstBuffer, dstOffset, size, data);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdFillBuffer(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data)
{
    const size_t cmd_size = vn_sizeof_vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdFillBuffer(cs, cmd_flags, commandBuffer, dstBuffer, dstOffset, size, data);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdClearColorImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    const size_t cmd_size = vn_sizeof_vkCmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    const size_t reply_size = vn_sizeof_vkCmdClearColorImage_reply(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdClearColorImage(instance_cs, cmd_flags, commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdClearColorImage_reply(&parser, commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdClearColorImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    const size_t cmd_size = vn_sizeof_vkCmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdClearColorImage(cs, cmd_flags, commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdClearDepthStencilImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    const size_t cmd_size = vn_sizeof_vkCmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    const size_t reply_size = vn_sizeof_vkCmdClearDepthStencilImage_reply(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdClearDepthStencilImage(instance_cs, cmd_flags, commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdClearDepthStencilImage_reply(&parser, commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdClearDepthStencilImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    const size_t cmd_size = vn_sizeof_vkCmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdClearDepthStencilImage(cs, cmd_flags, commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdClearAttachments(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects)
{
    const size_t cmd_size = vn_sizeof_vkCmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
    const size_t reply_size = vn_sizeof_vkCmdClearAttachments_reply(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdClearAttachments(instance_cs, cmd_flags, commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdClearAttachments_reply(&parser, commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdClearAttachments(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects)
{
    const size_t cmd_size = vn_sizeof_vkCmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdClearAttachments(cs, cmd_flags, commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdResolveImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    const size_t reply_size = vn_sizeof_vkCmdResolveImage_reply(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdResolveImage(instance_cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdResolveImage_reply(&parser, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdResolveImage(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions)
{
    const size_t cmd_size = vn_sizeof_vkCmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdResolveImage(cs, cmd_flags, commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetEvent(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetEvent(commandBuffer, event, stageMask);
    const size_t reply_size = vn_sizeof_vkCmdSetEvent_reply(commandBuffer, event, stageMask);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetEvent(instance_cs, cmd_flags, commandBuffer, event, stageMask);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetEvent_reply(&parser, commandBuffer, event, stageMask);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetEvent(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetEvent(commandBuffer, event, stageMask);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetEvent(cs, cmd_flags, commandBuffer, event, stageMask);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdResetEvent(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdResetEvent(commandBuffer, event, stageMask);
    const size_t reply_size = vn_sizeof_vkCmdResetEvent_reply(commandBuffer, event, stageMask);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdResetEvent(instance_cs, cmd_flags, commandBuffer, event, stageMask);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdResetEvent_reply(&parser, commandBuffer, event, stageMask);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdResetEvent(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdResetEvent(commandBuffer, event, stageMask);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdResetEvent(cs, cmd_flags, commandBuffer, event, stageMask);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdWaitEvents(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    const size_t cmd_size = vn_sizeof_vkCmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    const size_t reply_size = vn_sizeof_vkCmdWaitEvents_reply(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdWaitEvents(instance_cs, cmd_flags, commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdWaitEvents_reply(&parser, commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdWaitEvents(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    const size_t cmd_size = vn_sizeof_vkCmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdWaitEvents(cs, cmd_flags, commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdPipelineBarrier(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    const size_t cmd_size = vn_sizeof_vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    const size_t reply_size = vn_sizeof_vkCmdPipelineBarrier_reply(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdPipelineBarrier(instance_cs, cmd_flags, commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdPipelineBarrier_reply(&parser, commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdPipelineBarrier(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    const size_t cmd_size = vn_sizeof_vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdPipelineBarrier(cs, cmd_flags, commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBeginQuery(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
    const size_t reply_size = vn_sizeof_vkCmdBeginQuery_reply(commandBuffer, queryPool, query, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBeginQuery(instance_cs, cmd_flags, commandBuffer, queryPool, query, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBeginQuery_reply(&parser, commandBuffer, queryPool, query, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBeginQuery(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBeginQuery(cs, cmd_flags, commandBuffer, queryPool, query, flags);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdEndQuery(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndQuery(commandBuffer, queryPool, query);
    const size_t reply_size = vn_sizeof_vkCmdEndQuery_reply(commandBuffer, queryPool, query);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdEndQuery(instance_cs, cmd_flags, commandBuffer, queryPool, query);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdEndQuery_reply(&parser, commandBuffer, queryPool, query);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdEndQuery(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndQuery(commandBuffer, queryPool, query);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdEndQuery(cs, cmd_flags, commandBuffer, queryPool, query);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdResetQueryPool(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    const size_t cmd_size = vn_sizeof_vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
    const size_t reply_size = vn_sizeof_vkCmdResetQueryPool_reply(commandBuffer, queryPool, firstQuery, queryCount);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdResetQueryPool(instance_cs, cmd_flags, commandBuffer, queryPool, firstQuery, queryCount);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdResetQueryPool_reply(&parser, commandBuffer, queryPool, firstQuery, queryCount);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdResetQueryPool(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    const size_t cmd_size = vn_sizeof_vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdResetQueryPool(cs, cmd_flags, commandBuffer, queryPool, firstQuery, queryCount);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdWriteTimestamp(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
{
    const size_t cmd_size = vn_sizeof_vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
    const size_t reply_size = vn_sizeof_vkCmdWriteTimestamp_reply(commandBuffer, pipelineStage, queryPool, query);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdWriteTimestamp(instance_cs, cmd_flags, commandBuffer, pipelineStage, queryPool, query);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdWriteTimestamp_reply(&parser, commandBuffer, pipelineStage, queryPool, query);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdWriteTimestamp(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
{
    const size_t cmd_size = vn_sizeof_vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdWriteTimestamp(cs, cmd_flags, commandBuffer, pipelineStage, queryPool, query);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdCopyQueryPoolResults(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
    const size_t reply_size = vn_sizeof_vkCmdCopyQueryPoolResults_reply(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdCopyQueryPoolResults(instance_cs, cmd_flags, commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdCopyQueryPoolResults_reply(&parser, commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdCopyQueryPoolResults(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkCmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdCopyQueryPoolResults(cs, cmd_flags, commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdPushConstants(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
{
    const size_t cmd_size = vn_sizeof_vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
    const size_t reply_size = vn_sizeof_vkCmdPushConstants_reply(commandBuffer, layout, stageFlags, offset, size, pValues);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdPushConstants(instance_cs, cmd_flags, commandBuffer, layout, stageFlags, offset, size, pValues);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdPushConstants_reply(&parser, commandBuffer, layout, stageFlags, offset, size, pValues);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdPushConstants(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
{
    const size_t cmd_size = vn_sizeof_vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdPushConstants(cs, cmd_flags, commandBuffer, layout, stageFlags, offset, size, pValues);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBeginRenderPass(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
    const size_t reply_size = vn_sizeof_vkCmdBeginRenderPass_reply(commandBuffer, pRenderPassBegin, contents);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBeginRenderPass(instance_cs, cmd_flags, commandBuffer, pRenderPassBegin, contents);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBeginRenderPass_reply(&parser, commandBuffer, pRenderPassBegin, contents);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBeginRenderPass(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBeginRenderPass(cs, cmd_flags, commandBuffer, pRenderPassBegin, contents);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdNextSubpass(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
    const size_t cmd_size = vn_sizeof_vkCmdNextSubpass(commandBuffer, contents);
    const size_t reply_size = vn_sizeof_vkCmdNextSubpass_reply(commandBuffer, contents);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdNextSubpass(instance_cs, cmd_flags, commandBuffer, contents);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdNextSubpass_reply(&parser, commandBuffer, contents);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdNextSubpass(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
    const size_t cmd_size = vn_sizeof_vkCmdNextSubpass(commandBuffer, contents);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdNextSubpass(cs, cmd_flags, commandBuffer, contents);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdEndRenderPass(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndRenderPass(commandBuffer);
    const size_t reply_size = vn_sizeof_vkCmdEndRenderPass_reply(commandBuffer);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdEndRenderPass(instance_cs, cmd_flags, commandBuffer);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdEndRenderPass_reply(&parser, commandBuffer);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdEndRenderPass(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndRenderPass(commandBuffer);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdEndRenderPass(cs, cmd_flags, commandBuffer);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdExecuteCommands(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
    const size_t cmd_size = vn_sizeof_vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
    const size_t reply_size = vn_sizeof_vkCmdExecuteCommands_reply(commandBuffer, commandBufferCount, pCommandBuffers);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdExecuteCommands(instance_cs, cmd_flags, commandBuffer, commandBufferCount, pCommandBuffers);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdExecuteCommands_reply(&parser, commandBuffer, commandBufferCount, pCommandBuffers);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdExecuteCommands(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
    const size_t cmd_size = vn_sizeof_vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdExecuteCommands(cs, cmd_flags, commandBuffer, commandBufferCount, pCommandBuffers);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceFeatures2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceFeatures2_reply(physicalDevice, pFeatures);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceFeatures2(instance_cs, cmd_flags, physicalDevice, pFeatures);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceFeatures2_reply(&parser, physicalDevice, pFeatures);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceFeatures2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceFeatures2(cs, cmd_flags, physicalDevice, pFeatures);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceProperties2(physicalDevice, pProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceProperties2_reply(physicalDevice, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceProperties2(instance_cs, cmd_flags, physicalDevice, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceProperties2_reply(&parser, physicalDevice, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceProperties2(physicalDevice, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceProperties2(cs, cmd_flags, physicalDevice, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceFormatProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, pFormatProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceFormatProperties2_reply(physicalDevice, format, pFormatProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceFormatProperties2(instance_cs, cmd_flags, physicalDevice, format, pFormatProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceFormatProperties2_reply(&parser, physicalDevice, format, pFormatProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceFormatProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, pFormatProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceFormatProperties2(cs, cmd_flags, physicalDevice, format, pFormatProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetPhysicalDeviceImageFormatProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, pImageFormatInfo, pImageFormatProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceImageFormatProperties2_reply(physicalDevice, pImageFormatInfo, pImageFormatProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceImageFormatProperties2(instance_cs, cmd_flags, physicalDevice, pImageFormatInfo, pImageFormatProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetPhysicalDeviceImageFormatProperties2_reply(&parser, physicalDevice, pImageFormatInfo, pImageFormatProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetPhysicalDeviceImageFormatProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, pImageFormatInfo, pImageFormatProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceImageFormatProperties2(cs, cmd_flags, physicalDevice, pImageFormatInfo, pImageFormatProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceQueueFamilyProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceQueueFamilyProperties2_reply(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceQueueFamilyProperties2(instance_cs, cmd_flags, physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceQueueFamilyProperties2_reply(&parser, physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceQueueFamilyProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceQueueFamilyProperties2(cs, cmd_flags, physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceMemoryProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceMemoryProperties2_reply(physicalDevice, pMemoryProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceMemoryProperties2(instance_cs, cmd_flags, physicalDevice, pMemoryProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceMemoryProperties2_reply(&parser, physicalDevice, pMemoryProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceMemoryProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceMemoryProperties2(cs, cmd_flags, physicalDevice, pMemoryProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceSparseImageFormatProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceSparseImageFormatProperties2_reply(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceSparseImageFormatProperties2(instance_cs, cmd_flags, physicalDevice, pFormatInfo, pPropertyCount, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceSparseImageFormatProperties2_reply(&parser, physicalDevice, pFormatInfo, pPropertyCount, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceSparseImageFormatProperties2(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceSparseImageFormatProperties2(cs, cmd_flags, physicalDevice, pFormatInfo, pPropertyCount, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkTrimCommandPool(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkTrimCommandPool(device, commandPool, flags);
    const size_t reply_size = vn_sizeof_vkTrimCommandPool_reply(device, commandPool, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkTrimCommandPool(instance_cs, cmd_flags, device, commandPool, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkTrimCommandPool_reply(&parser, device, commandPool, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkTrimCommandPool(struct vn_instance *vn_instance, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags)
{
    const size_t cmd_size = vn_sizeof_vkTrimCommandPool(device, commandPool, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkTrimCommandPool(cs, cmd_flags, device, commandPool, flags);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceExternalBufferProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceExternalBufferProperties(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceExternalBufferProperties_reply(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceExternalBufferProperties(instance_cs, cmd_flags, physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceExternalBufferProperties_reply(&parser, physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceExternalBufferProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceExternalBufferProperties(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceExternalBufferProperties(cs, cmd_flags, physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceExternalSemaphoreProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceExternalSemaphoreProperties_reply(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceExternalSemaphoreProperties(instance_cs, cmd_flags, physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceExternalSemaphoreProperties_reply(&parser, physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceExternalSemaphoreProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceExternalSemaphoreProperties(cs, cmd_flags, physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetPhysicalDeviceExternalFenceProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceExternalFenceProperties(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
    const size_t reply_size = vn_sizeof_vkGetPhysicalDeviceExternalFenceProperties_reply(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetPhysicalDeviceExternalFenceProperties(instance_cs, cmd_flags, physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetPhysicalDeviceExternalFenceProperties_reply(&parser, physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetPhysicalDeviceExternalFenceProperties(struct vn_instance *vn_instance, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetPhysicalDeviceExternalFenceProperties(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetPhysicalDeviceExternalFenceProperties(cs, cmd_flags, physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkEnumeratePhysicalDeviceGroups(struct vn_instance *vn_instance, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    const size_t reply_size = vn_sizeof_vkEnumeratePhysicalDeviceGroups_reply(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkEnumeratePhysicalDeviceGroups(instance_cs, cmd_flags, instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkEnumeratePhysicalDeviceGroups_reply(&parser, instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkEnumeratePhysicalDeviceGroups(struct vn_instance *vn_instance, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties)
{
    const size_t cmd_size = vn_sizeof_vkEnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkEnumeratePhysicalDeviceGroups(cs, cmd_flags, instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetDeviceGroupPeerMemoryFeatures(struct vn_instance *vn_instance, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
    const size_t reply_size = vn_sizeof_vkGetDeviceGroupPeerMemoryFeatures_reply(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetDeviceGroupPeerMemoryFeatures(instance_cs, cmd_flags, device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetDeviceGroupPeerMemoryFeatures_reply(&parser, device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetDeviceGroupPeerMemoryFeatures(struct vn_instance *vn_instance, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetDeviceGroupPeerMemoryFeatures(cs, cmd_flags, device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkBindBufferMemory2(struct vn_instance *vn_instance, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos)
{
    const size_t cmd_size = vn_sizeof_vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
    const size_t reply_size = vn_sizeof_vkBindBufferMemory2_reply(device, bindInfoCount, pBindInfos);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkBindBufferMemory2(instance_cs, cmd_flags, device, bindInfoCount, pBindInfos);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkBindBufferMemory2_reply(&parser, device, bindInfoCount, pBindInfos);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkBindBufferMemory2(struct vn_instance *vn_instance, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos)
{
    const size_t cmd_size = vn_sizeof_vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkBindBufferMemory2(cs, cmd_flags, device, bindInfoCount, pBindInfos);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkBindImageMemory2(struct vn_instance *vn_instance, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos)
{
    const size_t cmd_size = vn_sizeof_vkBindImageMemory2(device, bindInfoCount, pBindInfos);
    const size_t reply_size = vn_sizeof_vkBindImageMemory2_reply(device, bindInfoCount, pBindInfos);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkBindImageMemory2(instance_cs, cmd_flags, device, bindInfoCount, pBindInfos);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkBindImageMemory2_reply(&parser, device, bindInfoCount, pBindInfos);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkBindImageMemory2(struct vn_instance *vn_instance, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos)
{
    const size_t cmd_size = vn_sizeof_vkBindImageMemory2(device, bindInfoCount, pBindInfos);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkBindImageMemory2(cs, cmd_flags, device, bindInfoCount, pBindInfos);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdSetDeviceMask(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetDeviceMask(commandBuffer, deviceMask);
    const size_t reply_size = vn_sizeof_vkCmdSetDeviceMask_reply(commandBuffer, deviceMask);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdSetDeviceMask(instance_cs, cmd_flags, commandBuffer, deviceMask);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdSetDeviceMask_reply(&parser, commandBuffer, deviceMask);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdSetDeviceMask(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
    const size_t cmd_size = vn_sizeof_vkCmdSetDeviceMask(commandBuffer, deviceMask);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdSetDeviceMask(cs, cmd_flags, commandBuffer, deviceMask);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDispatchBase(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    const size_t cmd_size = vn_sizeof_vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
    const size_t reply_size = vn_sizeof_vkCmdDispatchBase_reply(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDispatchBase(instance_cs, cmd_flags, commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDispatchBase_reply(&parser, commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDispatchBase(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    const size_t cmd_size = vn_sizeof_vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDispatchBase(cs, cmd_flags, commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateDescriptorUpdateTemplate(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate)
{
    const size_t cmd_size = vn_sizeof_vkCreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
    const size_t reply_size = vn_sizeof_vkCreateDescriptorUpdateTemplate_reply(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateDescriptorUpdateTemplate(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateDescriptorUpdateTemplate_reply(&parser, device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateDescriptorUpdateTemplate(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate)
{
    const size_t cmd_size = vn_sizeof_vkCreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateDescriptorUpdateTemplate(cs, cmd_flags, device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroyDescriptorUpdateTemplate(struct vn_instance *vn_instance, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroyDescriptorUpdateTemplate_reply(device, descriptorUpdateTemplate, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroyDescriptorUpdateTemplate(instance_cs, cmd_flags, device, descriptorUpdateTemplate, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroyDescriptorUpdateTemplate_reply(&parser, device, descriptorUpdateTemplate, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroyDescriptorUpdateTemplate(struct vn_instance *vn_instance, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroyDescriptorUpdateTemplate(cs, cmd_flags, device, descriptorUpdateTemplate, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetBufferMemoryRequirements2(struct vn_instance *vn_instance, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
    const size_t reply_size = vn_sizeof_vkGetBufferMemoryRequirements2_reply(device, pInfo, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetBufferMemoryRequirements2(instance_cs, cmd_flags, device, pInfo, pMemoryRequirements);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetBufferMemoryRequirements2_reply(&parser, device, pInfo, pMemoryRequirements);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetBufferMemoryRequirements2(struct vn_instance *vn_instance, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetBufferMemoryRequirements2(cs, cmd_flags, device, pInfo, pMemoryRequirements);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetImageMemoryRequirements2(struct vn_instance *vn_instance, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
    const size_t reply_size = vn_sizeof_vkGetImageMemoryRequirements2_reply(device, pInfo, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetImageMemoryRequirements2(instance_cs, cmd_flags, device, pInfo, pMemoryRequirements);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetImageMemoryRequirements2_reply(&parser, device, pInfo, pMemoryRequirements);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetImageMemoryRequirements2(struct vn_instance *vn_instance, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetImageMemoryRequirements2(cs, cmd_flags, device, pInfo, pMemoryRequirements);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetImageSparseMemoryRequirements2(struct vn_instance *vn_instance, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageSparseMemoryRequirements2(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    const size_t reply_size = vn_sizeof_vkGetImageSparseMemoryRequirements2_reply(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetImageSparseMemoryRequirements2(instance_cs, cmd_flags, device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetImageSparseMemoryRequirements2_reply(&parser, device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetImageSparseMemoryRequirements2(struct vn_instance *vn_instance, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements)
{
    const size_t cmd_size = vn_sizeof_vkGetImageSparseMemoryRequirements2(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetImageSparseMemoryRequirements2(cs, cmd_flags, device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateSamplerYcbcrConversion(struct vn_instance *vn_instance, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion)
{
    const size_t cmd_size = vn_sizeof_vkCreateSamplerYcbcrConversion(device, pCreateInfo, pAllocator, pYcbcrConversion);
    const size_t reply_size = vn_sizeof_vkCreateSamplerYcbcrConversion_reply(device, pCreateInfo, pAllocator, pYcbcrConversion);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateSamplerYcbcrConversion(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pYcbcrConversion);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateSamplerYcbcrConversion_reply(&parser, device, pCreateInfo, pAllocator, pYcbcrConversion);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateSamplerYcbcrConversion(struct vn_instance *vn_instance, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion)
{
    const size_t cmd_size = vn_sizeof_vkCreateSamplerYcbcrConversion(device, pCreateInfo, pAllocator, pYcbcrConversion);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateSamplerYcbcrConversion(cs, cmd_flags, device, pCreateInfo, pAllocator, pYcbcrConversion);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkDestroySamplerYcbcrConversion(struct vn_instance *vn_instance, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroySamplerYcbcrConversion(device, ycbcrConversion, pAllocator);
    const size_t reply_size = vn_sizeof_vkDestroySamplerYcbcrConversion_reply(device, ycbcrConversion, pAllocator);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkDestroySamplerYcbcrConversion(instance_cs, cmd_flags, device, ycbcrConversion, pAllocator);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkDestroySamplerYcbcrConversion_reply(&parser, device, ycbcrConversion, pAllocator);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkDestroySamplerYcbcrConversion(struct vn_instance *vn_instance, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator)
{
    const size_t cmd_size = vn_sizeof_vkDestroySamplerYcbcrConversion(device, ycbcrConversion, pAllocator);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkDestroySamplerYcbcrConversion(cs, cmd_flags, device, ycbcrConversion, pAllocator);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetDeviceQueue2(struct vn_instance *vn_instance, VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceQueue2(device, pQueueInfo, pQueue);
    const size_t reply_size = vn_sizeof_vkGetDeviceQueue2_reply(device, pQueueInfo, pQueue);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetDeviceQueue2(instance_cs, cmd_flags, device, pQueueInfo, pQueue);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetDeviceQueue2_reply(&parser, device, pQueueInfo, pQueue);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetDeviceQueue2(struct vn_instance *vn_instance, VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceQueue2(device, pQueueInfo, pQueue);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetDeviceQueue2(cs, cmd_flags, device, pQueueInfo, pQueue);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkGetDescriptorSetLayoutSupport(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport)
{
    const size_t cmd_size = vn_sizeof_vkGetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
    const size_t reply_size = vn_sizeof_vkGetDescriptorSetLayoutSupport_reply(device, pCreateInfo, pSupport);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetDescriptorSetLayoutSupport(instance_cs, cmd_flags, device, pCreateInfo, pSupport);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkGetDescriptorSetLayoutSupport_reply(&parser, device, pCreateInfo, pSupport);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkGetDescriptorSetLayoutSupport(struct vn_instance *vn_instance, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport)
{
    const size_t cmd_size = vn_sizeof_vkGetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetDescriptorSetLayoutSupport(cs, cmd_flags, device, pCreateInfo, pSupport);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkCreateRenderPass2(struct vn_instance *vn_instance, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
{
    const size_t cmd_size = vn_sizeof_vkCreateRenderPass2(device, pCreateInfo, pAllocator, pRenderPass);
    const size_t reply_size = vn_sizeof_vkCreateRenderPass2_reply(device, pCreateInfo, pAllocator, pRenderPass);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCreateRenderPass2(instance_cs, cmd_flags, device, pCreateInfo, pAllocator, pRenderPass);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkCreateRenderPass2_reply(&parser, device, pCreateInfo, pAllocator, pRenderPass);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkCreateRenderPass2(struct vn_instance *vn_instance, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
{
    const size_t cmd_size = vn_sizeof_vkCreateRenderPass2(device, pCreateInfo, pAllocator, pRenderPass);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCreateRenderPass2(cs, cmd_flags, device, pCreateInfo, pAllocator, pRenderPass);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBeginRenderPass2(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
    const size_t reply_size = vn_sizeof_vkCmdBeginRenderPass2_reply(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBeginRenderPass2(instance_cs, cmd_flags, commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBeginRenderPass2_reply(&parser, commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBeginRenderPass2(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBeginRenderPass2(cs, cmd_flags, commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdNextSubpass2(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo)
{
    const size_t cmd_size = vn_sizeof_vkCmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
    const size_t reply_size = vn_sizeof_vkCmdNextSubpass2_reply(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdNextSubpass2(instance_cs, cmd_flags, commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdNextSubpass2_reply(&parser, commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdNextSubpass2(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo)
{
    const size_t cmd_size = vn_sizeof_vkCmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdNextSubpass2(cs, cmd_flags, commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdEndRenderPass2(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
    const size_t reply_size = vn_sizeof_vkCmdEndRenderPass2_reply(commandBuffer, pSubpassEndInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdEndRenderPass2(instance_cs, cmd_flags, commandBuffer, pSubpassEndInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdEndRenderPass2_reply(&parser, commandBuffer, pSubpassEndInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdEndRenderPass2(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdEndRenderPass2(cs, cmd_flags, commandBuffer, pSubpassEndInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetSemaphoreCounterValue(struct vn_instance *vn_instance, VkDevice device, VkSemaphore semaphore, uint64_t* pValue)
{
    const size_t cmd_size = vn_sizeof_vkGetSemaphoreCounterValue(device, semaphore, pValue);
    const size_t reply_size = vn_sizeof_vkGetSemaphoreCounterValue_reply(device, semaphore, pValue);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetSemaphoreCounterValue(instance_cs, cmd_flags, device, semaphore, pValue);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetSemaphoreCounterValue_reply(&parser, device, semaphore, pValue);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetSemaphoreCounterValue(struct vn_instance *vn_instance, VkDevice device, VkSemaphore semaphore, uint64_t* pValue)
{
    const size_t cmd_size = vn_sizeof_vkGetSemaphoreCounterValue(device, semaphore, pValue);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetSemaphoreCounterValue(cs, cmd_flags, device, semaphore, pValue);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkWaitSemaphores(struct vn_instance *vn_instance, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout)
{
    const size_t cmd_size = vn_sizeof_vkWaitSemaphores(device, pWaitInfo, timeout);
    const size_t reply_size = vn_sizeof_vkWaitSemaphores_reply(device, pWaitInfo, timeout);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkWaitSemaphores(instance_cs, cmd_flags, device, pWaitInfo, timeout);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkWaitSemaphores_reply(&parser, device, pWaitInfo, timeout);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkWaitSemaphores(struct vn_instance *vn_instance, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout)
{
    const size_t cmd_size = vn_sizeof_vkWaitSemaphores(device, pWaitInfo, timeout);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkWaitSemaphores(cs, cmd_flags, device, pWaitInfo, timeout);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkSignalSemaphore(struct vn_instance *vn_instance, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo)
{
    const size_t cmd_size = vn_sizeof_vkSignalSemaphore(device, pSignalInfo);
    const size_t reply_size = vn_sizeof_vkSignalSemaphore_reply(device, pSignalInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkSignalSemaphore(instance_cs, cmd_flags, device, pSignalInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkSignalSemaphore_reply(&parser, device, pSignalInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkSignalSemaphore(struct vn_instance *vn_instance, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo)
{
    const size_t cmd_size = vn_sizeof_vkSignalSemaphore(device, pSignalInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkSignalSemaphore(cs, cmd_flags, device, pSignalInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDrawIndirectCount(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    const size_t reply_size = vn_sizeof_vkCmdDrawIndirectCount_reply(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDrawIndirectCount(instance_cs, cmd_flags, commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDrawIndirectCount_reply(&parser, commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDrawIndirectCount(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDrawIndirectCount(cs, cmd_flags, commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDrawIndexedIndirectCount(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    const size_t reply_size = vn_sizeof_vkCmdDrawIndexedIndirectCount_reply(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDrawIndexedIndirectCount(instance_cs, cmd_flags, commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDrawIndexedIndirectCount_reply(&parser, commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDrawIndexedIndirectCount(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDrawIndexedIndirectCount(cs, cmd_flags, commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBindTransformFeedbackBuffersEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    const size_t reply_size = vn_sizeof_vkCmdBindTransformFeedbackBuffersEXT_reply(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBindTransformFeedbackBuffersEXT(instance_cs, cmd_flags, commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBindTransformFeedbackBuffersEXT_reply(&parser, commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBindTransformFeedbackBuffersEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes)
{
    const size_t cmd_size = vn_sizeof_vkCmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBindTransformFeedbackBuffersEXT(cs, cmd_flags, commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBeginTransformFeedbackEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    const size_t reply_size = vn_sizeof_vkCmdBeginTransformFeedbackEXT_reply(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBeginTransformFeedbackEXT(instance_cs, cmd_flags, commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBeginTransformFeedbackEXT_reply(&parser, commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBeginTransformFeedbackEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBeginTransformFeedbackEXT(cs, cmd_flags, commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdEndTransformFeedbackEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    const size_t reply_size = vn_sizeof_vkCmdEndTransformFeedbackEXT_reply(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdEndTransformFeedbackEXT(instance_cs, cmd_flags, commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdEndTransformFeedbackEXT_reply(&parser, commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdEndTransformFeedbackEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdEndTransformFeedbackEXT(cs, cmd_flags, commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdBeginQueryIndexedEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginQueryIndexedEXT(commandBuffer, queryPool, query, flags, index);
    const size_t reply_size = vn_sizeof_vkCmdBeginQueryIndexedEXT_reply(commandBuffer, queryPool, query, flags, index);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdBeginQueryIndexedEXT(instance_cs, cmd_flags, commandBuffer, queryPool, query, flags, index);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdBeginQueryIndexedEXT_reply(&parser, commandBuffer, queryPool, query, flags, index);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdBeginQueryIndexedEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index)
{
    const size_t cmd_size = vn_sizeof_vkCmdBeginQueryIndexedEXT(commandBuffer, queryPool, query, flags, index);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdBeginQueryIndexedEXT(cs, cmd_flags, commandBuffer, queryPool, query, flags, index);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdEndQueryIndexedEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndQueryIndexedEXT(commandBuffer, queryPool, query, index);
    const size_t reply_size = vn_sizeof_vkCmdEndQueryIndexedEXT_reply(commandBuffer, queryPool, query, index);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdEndQueryIndexedEXT(instance_cs, cmd_flags, commandBuffer, queryPool, query, index);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdEndQueryIndexedEXT_reply(&parser, commandBuffer, queryPool, query, index);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdEndQueryIndexedEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index)
{
    const size_t cmd_size = vn_sizeof_vkCmdEndQueryIndexedEXT(commandBuffer, queryPool, query, index);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdEndQueryIndexedEXT(cs, cmd_flags, commandBuffer, queryPool, query, index);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkCmdDrawIndirectByteCountEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndirectByteCountEXT(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
    const size_t reply_size = vn_sizeof_vkCmdDrawIndirectByteCountEXT_reply(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkCmdDrawIndirectByteCountEXT(instance_cs, cmd_flags, commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkCmdDrawIndirectByteCountEXT_reply(&parser, commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkCmdDrawIndirectByteCountEXT(struct vn_instance *vn_instance, VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride)
{
    const size_t cmd_size = vn_sizeof_vkCmdDrawIndirectByteCountEXT(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkCmdDrawIndirectByteCountEXT(cs, cmd_flags, commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkResult vn_call_vkGetImageDrmFormatModifierPropertiesEXT(struct vn_instance *vn_instance, VkDevice device, VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetImageDrmFormatModifierPropertiesEXT(device, image, pProperties);
    const size_t reply_size = vn_sizeof_vkGetImageDrmFormatModifierPropertiesEXT_reply(device, image, pProperties);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetImageDrmFormatModifierPropertiesEXT(instance_cs, cmd_flags, device, image, pProperties);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkResult ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetImageDrmFormatModifierPropertiesEXT_reply(&parser, device, image, pProperties);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetImageDrmFormatModifierPropertiesEXT(struct vn_instance *vn_instance, VkDevice device, VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties)
{
    const size_t cmd_size = vn_sizeof_vkGetImageDrmFormatModifierPropertiesEXT(device, image, pProperties);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetImageDrmFormatModifierPropertiesEXT(cs, cmd_flags, device, image, pProperties);
    vn_instance_unlock_cs(vn_instance);
}

static inline uint64_t vn_call_vkGetBufferOpaqueCaptureAddress(struct vn_instance *vn_instance, VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferOpaqueCaptureAddress(device, pInfo);
    const size_t reply_size = vn_sizeof_vkGetBufferOpaqueCaptureAddress_reply(device, pInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetBufferOpaqueCaptureAddress(instance_cs, cmd_flags, device, pInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    uint64_t ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetBufferOpaqueCaptureAddress_reply(&parser, device, pInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetBufferOpaqueCaptureAddress(struct vn_instance *vn_instance, VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferOpaqueCaptureAddress(device, pInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetBufferOpaqueCaptureAddress(cs, cmd_flags, device, pInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline VkDeviceAddress vn_call_vkGetBufferDeviceAddress(struct vn_instance *vn_instance, VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferDeviceAddress(device, pInfo);
    const size_t reply_size = vn_sizeof_vkGetBufferDeviceAddress_reply(device, pInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetBufferDeviceAddress(instance_cs, cmd_flags, device, pInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    VkDeviceAddress ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetBufferDeviceAddress_reply(&parser, device, pInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetBufferDeviceAddress(struct vn_instance *vn_instance, VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
{
    const size_t cmd_size = vn_sizeof_vkGetBufferDeviceAddress(device, pInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetBufferDeviceAddress(cs, cmd_flags, device, pInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline uint64_t vn_call_vkGetDeviceMemoryOpaqueCaptureAddress(struct vn_instance *vn_instance, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceMemoryOpaqueCaptureAddress(device, pInfo);
    const size_t reply_size = vn_sizeof_vkGetDeviceMemoryOpaqueCaptureAddress_reply(device, pInfo);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkGetDeviceMemoryOpaqueCaptureAddress(instance_cs, cmd_flags, device, pInfo);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    uint64_t ret = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        ret = vn_decode_vkGetDeviceMemoryOpaqueCaptureAddress_reply(&parser, device, pInfo);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }

    return ret;
}

static inline void vn_async_vkGetDeviceMemoryOpaqueCaptureAddress(struct vn_instance *vn_instance, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo)
{
    const size_t cmd_size = vn_sizeof_vkGetDeviceMemoryOpaqueCaptureAddress(device, pInfo);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkGetDeviceMemoryOpaqueCaptureAddress(cs, cmd_flags, device, pInfo);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkSetReplyCommandStreamMESA(struct vn_instance *vn_instance, const VkCommandStreamDescriptionMESA* pStream)
{
    const size_t cmd_size = vn_sizeof_vkSetReplyCommandStreamMESA(pStream);
    const size_t reply_size = vn_sizeof_vkSetReplyCommandStreamMESA_reply(pStream);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkSetReplyCommandStreamMESA(instance_cs, cmd_flags, pStream);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkSetReplyCommandStreamMESA_reply(&parser, pStream);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkSetReplyCommandStreamMESA(struct vn_instance *vn_instance, const VkCommandStreamDescriptionMESA* pStream)
{
    const size_t cmd_size = vn_sizeof_vkSetReplyCommandStreamMESA(pStream);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkSetReplyCommandStreamMESA(cs, cmd_flags, pStream);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkSeekReplyCommandStreamMESA(struct vn_instance *vn_instance, size_t position)
{
    const size_t cmd_size = vn_sizeof_vkSeekReplyCommandStreamMESA(position);
    const size_t reply_size = vn_sizeof_vkSeekReplyCommandStreamMESA_reply(position);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkSeekReplyCommandStreamMESA(instance_cs, cmd_flags, position);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkSeekReplyCommandStreamMESA_reply(&parser, position);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkSeekReplyCommandStreamMESA(struct vn_instance *vn_instance, size_t position)
{
    const size_t cmd_size = vn_sizeof_vkSeekReplyCommandStreamMESA(position);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkSeekReplyCommandStreamMESA(cs, cmd_flags, position);
    vn_instance_unlock_cs(vn_instance);
}

static inline void vn_call_vkExecuteCommandStreamsMESA(struct vn_instance *vn_instance, uint32_t streamCount, const VkCommandStreamDescriptionMESA* pStreams, const size_t* pReplyPositions, uint32_t dependencyCount, const VkCommandStreamDependencyMESA* pDependencies, VkCommandStreamExecutionFlagsMESA flags)
{
    const size_t cmd_size = vn_sizeof_vkExecuteCommandStreamsMESA(streamCount, pStreams, pReplyPositions, dependencyCount, pDependencies, flags);
    const size_t reply_size = vn_sizeof_vkExecuteCommandStreamsMESA_reply(streamCount, pStreams, pReplyPositions, dependencyCount, pDependencies, flags);
    const VkCommandFlagsEXT cmd_flags = VK_COMMAND_GENERATE_REPLY_BIT_EXT;
    bool submitted = false;
    struct vn_renderer_bo *reply_bo;
    void *reply_ptr;
    uint64_t reply_sync_val;

    /* encode and submit */
    struct vn_cs *instance_cs = vn_instance_lock_cs(vn_instance);
    reply_bo = vn_instance_get_cs_reply_bo_locked(vn_instance, reply_size, &reply_ptr);
    if (likely(reply_bo && vn_cs_reserve_out(instance_cs, cmd_size))) {
        vn_encode_vkExecuteCommandStreamsMESA(instance_cs, cmd_flags, streamCount, pStreams, pReplyPositions, dependencyCount, pDependencies, flags);
        submitted = vn_instance_submit_cs_locked(vn_instance, reply_bo, &reply_sync_val);
    }
    vn_instance_unlock_cs(vn_instance);

    /* decode reply */
    if (likely(submitted)) {
        struct vn_cs parser; /* TODO separate in/out support */
        vn_cs_init(&parser, NULL, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, 0);
        vn_cs_set_in_data(&parser, reply_ptr, reply_size);

        vn_instance_wait_cs_reply(vn_instance, reply_sync_val);
        vn_decode_vkExecuteCommandStreamsMESA_reply(&parser, streamCount, pStreams, pReplyPositions, dependencyCount, pDependencies, flags);
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    } else if (reply_bo) {
        vn_instance_free_cs_reply_bo(vn_instance, reply_bo);
    }
}

static inline void vn_async_vkExecuteCommandStreamsMESA(struct vn_instance *vn_instance, uint32_t streamCount, const VkCommandStreamDescriptionMESA* pStreams, const size_t* pReplyPositions, uint32_t dependencyCount, const VkCommandStreamDependencyMESA* pDependencies, VkCommandStreamExecutionFlagsMESA flags)
{
    const size_t cmd_size = vn_sizeof_vkExecuteCommandStreamsMESA(streamCount, pStreams, pReplyPositions, dependencyCount, pDependencies, flags);
    const VkCommandFlagsEXT cmd_flags = 0;

    struct vn_cs *cs = vn_instance_lock_cs(vn_instance);
    if (vn_cs_reserve_out(cs, cmd_size))
        vn_encode_vkExecuteCommandStreamsMESA(cs, cmd_flags, streamCount, pStreams, pReplyPositions, dependencyCount, pDependencies, flags);
    vn_instance_unlock_cs(vn_instance);
}

#endif /* VN_PROTOCOL_DRIVER_CALLS_H */
