#ifndef VK_DESCRIPTORS_H
#define VK_DESCRIPTORS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>

VkDescriptorSetLayoutBinding *
vk_create_sorted_bindings(const VkDescriptorSetLayoutBinding *bindings, unsigned count);

#ifdef __cplusplus
}
#endif



#endif
