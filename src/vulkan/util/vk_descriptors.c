#include <stdlib.h>
#include <string.h>
#include "vk_descriptors.h"
#include "util/macros.h"

static int
binding_compare(const void* av, const void *bv)
{
   const VkDescriptorSetLayoutBinding *a = (const VkDescriptorSetLayoutBinding*)av;
   const VkDescriptorSetLayoutBinding *b = (const VkDescriptorSetLayoutBinding*)bv;
 
   return (a->binding < b->binding) ? -1 : (a->binding > b->binding) ? 1 : 0;
}
 
VkDescriptorSetLayoutBinding *
vk_create_sorted_bindings(const VkDescriptorSetLayoutBinding *bindings, unsigned count)
{
   VkDescriptorSetLayoutBinding *sorted_bindings = malloc(MAX2(count * sizeof(VkDescriptorSetLayoutBinding), 1));
   if (!sorted_bindings)
      return NULL;
 
   if (count) {
      memcpy(sorted_bindings, bindings, count * sizeof(VkDescriptorSetLayoutBinding));
      qsort(sorted_bindings, count, sizeof(VkDescriptorSetLayoutBinding), binding_compare);
   } else
      /* just an empty struct */
      memset(sorted_bindings, 0, sizeof(VkDescriptorSetLayoutBinding));
 
   return sorted_bindings;
}
