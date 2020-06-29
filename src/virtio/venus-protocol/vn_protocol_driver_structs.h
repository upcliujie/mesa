/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_PROTOCOL_DRIVER_STRUCTS_H
#define VN_PROTOCOL_DRIVER_STRUCTS_H

#include "vn_protocol_driver_handles.h"

/*
 * These structs/unions are not included
 *
 *   VkBaseOutStructure
 *   VkBaseInStructure
 *   VkAllocationCallbacks
 */

/* struct VkOffset2D */

static inline size_t
vn_sizeof_VkOffset2D(const VkOffset2D *val)
{
    size_t size = 0;
    size += vn_sizeof_int32_t(&val->x);
    size += vn_sizeof_int32_t(&val->y);
    return size;
}

static inline void
vn_encode_VkOffset2D(struct vn_cs *cs, const VkOffset2D *val)
{
    vn_encode_int32_t(cs, &val->x);
    vn_encode_int32_t(cs, &val->y);
}

static inline void
vn_decode_VkOffset2D(struct vn_cs *cs, VkOffset2D *val)
{
    vn_decode_int32_t(cs, &val->x);
    vn_decode_int32_t(cs, &val->y);
}

static inline size_t
vn_sizeof_VkOffset2D_partial(const VkOffset2D *val)
{
    size_t size = 0;
    /* skip val->x */
    /* skip val->y */
    return size;
}

static inline void
vn_encode_VkOffset2D_partial(struct vn_cs *cs, const VkOffset2D *val)
{
    /* skip val->x */
    /* skip val->y */
}

/* struct VkOffset3D */

static inline size_t
vn_sizeof_VkOffset3D(const VkOffset3D *val)
{
    size_t size = 0;
    size += vn_sizeof_int32_t(&val->x);
    size += vn_sizeof_int32_t(&val->y);
    size += vn_sizeof_int32_t(&val->z);
    return size;
}

static inline void
vn_encode_VkOffset3D(struct vn_cs *cs, const VkOffset3D *val)
{
    vn_encode_int32_t(cs, &val->x);
    vn_encode_int32_t(cs, &val->y);
    vn_encode_int32_t(cs, &val->z);
}

/* struct VkExtent2D */

static inline size_t
vn_sizeof_VkExtent2D(const VkExtent2D *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->width);
    size += vn_sizeof_uint32_t(&val->height);
    return size;
}

static inline void
vn_encode_VkExtent2D(struct vn_cs *cs, const VkExtent2D *val)
{
    vn_encode_uint32_t(cs, &val->width);
    vn_encode_uint32_t(cs, &val->height);
}

static inline void
vn_decode_VkExtent2D(struct vn_cs *cs, VkExtent2D *val)
{
    vn_decode_uint32_t(cs, &val->width);
    vn_decode_uint32_t(cs, &val->height);
}

static inline size_t
vn_sizeof_VkExtent2D_partial(const VkExtent2D *val)
{
    size_t size = 0;
    /* skip val->width */
    /* skip val->height */
    return size;
}

static inline void
vn_encode_VkExtent2D_partial(struct vn_cs *cs, const VkExtent2D *val)
{
    /* skip val->width */
    /* skip val->height */
}

/* struct VkExtent3D */

static inline size_t
vn_sizeof_VkExtent3D(const VkExtent3D *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->width);
    size += vn_sizeof_uint32_t(&val->height);
    size += vn_sizeof_uint32_t(&val->depth);
    return size;
}

static inline void
vn_encode_VkExtent3D(struct vn_cs *cs, const VkExtent3D *val)
{
    vn_encode_uint32_t(cs, &val->width);
    vn_encode_uint32_t(cs, &val->height);
    vn_encode_uint32_t(cs, &val->depth);
}

static inline void
vn_decode_VkExtent3D(struct vn_cs *cs, VkExtent3D *val)
{
    vn_decode_uint32_t(cs, &val->width);
    vn_decode_uint32_t(cs, &val->height);
    vn_decode_uint32_t(cs, &val->depth);
}

static inline size_t
vn_sizeof_VkExtent3D_partial(const VkExtent3D *val)
{
    size_t size = 0;
    /* skip val->width */
    /* skip val->height */
    /* skip val->depth */
    return size;
}

static inline void
vn_encode_VkExtent3D_partial(struct vn_cs *cs, const VkExtent3D *val)
{
    /* skip val->width */
    /* skip val->height */
    /* skip val->depth */
}

/* struct VkViewport */

static inline size_t
vn_sizeof_VkViewport(const VkViewport *val)
{
    size_t size = 0;
    size += vn_sizeof_float(&val->x);
    size += vn_sizeof_float(&val->y);
    size += vn_sizeof_float(&val->width);
    size += vn_sizeof_float(&val->height);
    size += vn_sizeof_float(&val->minDepth);
    size += vn_sizeof_float(&val->maxDepth);
    return size;
}

static inline void
vn_encode_VkViewport(struct vn_cs *cs, const VkViewport *val)
{
    vn_encode_float(cs, &val->x);
    vn_encode_float(cs, &val->y);
    vn_encode_float(cs, &val->width);
    vn_encode_float(cs, &val->height);
    vn_encode_float(cs, &val->minDepth);
    vn_encode_float(cs, &val->maxDepth);
}

/* struct VkRect2D */

static inline size_t
vn_sizeof_VkRect2D(const VkRect2D *val)
{
    size_t size = 0;
    size += vn_sizeof_VkOffset2D(&val->offset);
    size += vn_sizeof_VkExtent2D(&val->extent);
    return size;
}

static inline void
vn_encode_VkRect2D(struct vn_cs *cs, const VkRect2D *val)
{
    vn_encode_VkOffset2D(cs, &val->offset);
    vn_encode_VkExtent2D(cs, &val->extent);
}

static inline void
vn_decode_VkRect2D(struct vn_cs *cs, VkRect2D *val)
{
    vn_decode_VkOffset2D(cs, &val->offset);
    vn_decode_VkExtent2D(cs, &val->extent);
}

static inline size_t
vn_sizeof_VkRect2D_partial(const VkRect2D *val)
{
    size_t size = 0;
    size += vn_sizeof_VkOffset2D_partial(&val->offset);
    size += vn_sizeof_VkExtent2D_partial(&val->extent);
    return size;
}

static inline void
vn_encode_VkRect2D_partial(struct vn_cs *cs, const VkRect2D *val)
{
    vn_encode_VkOffset2D_partial(cs, &val->offset);
    vn_encode_VkExtent2D_partial(cs, &val->extent);
}

/* struct VkClearRect */

static inline size_t
vn_sizeof_VkClearRect(const VkClearRect *val)
{
    size_t size = 0;
    size += vn_sizeof_VkRect2D(&val->rect);
    size += vn_sizeof_uint32_t(&val->baseArrayLayer);
    size += vn_sizeof_uint32_t(&val->layerCount);
    return size;
}

static inline void
vn_encode_VkClearRect(struct vn_cs *cs, const VkClearRect *val)
{
    vn_encode_VkRect2D(cs, &val->rect);
    vn_encode_uint32_t(cs, &val->baseArrayLayer);
    vn_encode_uint32_t(cs, &val->layerCount);
}

/* struct VkComponentMapping */

static inline size_t
vn_sizeof_VkComponentMapping(const VkComponentMapping *val)
{
    size_t size = 0;
    size += vn_sizeof_VkComponentSwizzle(&val->r);
    size += vn_sizeof_VkComponentSwizzle(&val->g);
    size += vn_sizeof_VkComponentSwizzle(&val->b);
    size += vn_sizeof_VkComponentSwizzle(&val->a);
    return size;
}

static inline void
vn_encode_VkComponentMapping(struct vn_cs *cs, const VkComponentMapping *val)
{
    vn_encode_VkComponentSwizzle(cs, &val->r);
    vn_encode_VkComponentSwizzle(cs, &val->g);
    vn_encode_VkComponentSwizzle(cs, &val->b);
    vn_encode_VkComponentSwizzle(cs, &val->a);
}

static inline void
vn_decode_VkComponentMapping(struct vn_cs *cs, VkComponentMapping *val)
{
    vn_decode_VkComponentSwizzle(cs, &val->r);
    vn_decode_VkComponentSwizzle(cs, &val->g);
    vn_decode_VkComponentSwizzle(cs, &val->b);
    vn_decode_VkComponentSwizzle(cs, &val->a);
}

static inline size_t
vn_sizeof_VkComponentMapping_partial(const VkComponentMapping *val)
{
    size_t size = 0;
    /* skip val->r */
    /* skip val->g */
    /* skip val->b */
    /* skip val->a */
    return size;
}

static inline void
vn_encode_VkComponentMapping_partial(struct vn_cs *cs, const VkComponentMapping *val)
{
    /* skip val->r */
    /* skip val->g */
    /* skip val->b */
    /* skip val->a */
}

/* struct VkPhysicalDeviceLimits */

static inline size_t
vn_sizeof_VkPhysicalDeviceLimits(const VkPhysicalDeviceLimits *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->maxImageDimension1D);
    size += vn_sizeof_uint32_t(&val->maxImageDimension2D);
    size += vn_sizeof_uint32_t(&val->maxImageDimension3D);
    size += vn_sizeof_uint32_t(&val->maxImageDimensionCube);
    size += vn_sizeof_uint32_t(&val->maxImageArrayLayers);
    size += vn_sizeof_uint32_t(&val->maxTexelBufferElements);
    size += vn_sizeof_uint32_t(&val->maxUniformBufferRange);
    size += vn_sizeof_uint32_t(&val->maxStorageBufferRange);
    size += vn_sizeof_uint32_t(&val->maxPushConstantsSize);
    size += vn_sizeof_uint32_t(&val->maxMemoryAllocationCount);
    size += vn_sizeof_uint32_t(&val->maxSamplerAllocationCount);
    size += vn_sizeof_VkDeviceSize(&val->bufferImageGranularity);
    size += vn_sizeof_VkDeviceSize(&val->sparseAddressSpaceSize);
    size += vn_sizeof_uint32_t(&val->maxBoundDescriptorSets);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorSamplers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUniformBuffers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorStorageBuffers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorSampledImages);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorStorageImages);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorInputAttachments);
    size += vn_sizeof_uint32_t(&val->maxPerStageResources);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetSamplers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUniformBuffers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUniformBuffersDynamic);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetStorageBuffers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetStorageBuffersDynamic);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetSampledImages);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetStorageImages);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetInputAttachments);
    size += vn_sizeof_uint32_t(&val->maxVertexInputAttributes);
    size += vn_sizeof_uint32_t(&val->maxVertexInputBindings);
    size += vn_sizeof_uint32_t(&val->maxVertexInputAttributeOffset);
    size += vn_sizeof_uint32_t(&val->maxVertexInputBindingStride);
    size += vn_sizeof_uint32_t(&val->maxVertexOutputComponents);
    size += vn_sizeof_uint32_t(&val->maxTessellationGenerationLevel);
    size += vn_sizeof_uint32_t(&val->maxTessellationPatchSize);
    size += vn_sizeof_uint32_t(&val->maxTessellationControlPerVertexInputComponents);
    size += vn_sizeof_uint32_t(&val->maxTessellationControlPerVertexOutputComponents);
    size += vn_sizeof_uint32_t(&val->maxTessellationControlPerPatchOutputComponents);
    size += vn_sizeof_uint32_t(&val->maxTessellationControlTotalOutputComponents);
    size += vn_sizeof_uint32_t(&val->maxTessellationEvaluationInputComponents);
    size += vn_sizeof_uint32_t(&val->maxTessellationEvaluationOutputComponents);
    size += vn_sizeof_uint32_t(&val->maxGeometryShaderInvocations);
    size += vn_sizeof_uint32_t(&val->maxGeometryInputComponents);
    size += vn_sizeof_uint32_t(&val->maxGeometryOutputComponents);
    size += vn_sizeof_uint32_t(&val->maxGeometryOutputVertices);
    size += vn_sizeof_uint32_t(&val->maxGeometryTotalOutputComponents);
    size += vn_sizeof_uint32_t(&val->maxFragmentInputComponents);
    size += vn_sizeof_uint32_t(&val->maxFragmentOutputAttachments);
    size += vn_sizeof_uint32_t(&val->maxFragmentDualSrcAttachments);
    size += vn_sizeof_uint32_t(&val->maxFragmentCombinedOutputResources);
    size += vn_sizeof_uint32_t(&val->maxComputeSharedMemorySize);
    size += vn_sizeof_array_size(3);
    size += vn_sizeof_uint32_t_array(val->maxComputeWorkGroupCount, 3);
    size += vn_sizeof_uint32_t(&val->maxComputeWorkGroupInvocations);
    size += vn_sizeof_array_size(3);
    size += vn_sizeof_uint32_t_array(val->maxComputeWorkGroupSize, 3);
    size += vn_sizeof_uint32_t(&val->subPixelPrecisionBits);
    size += vn_sizeof_uint32_t(&val->subTexelPrecisionBits);
    size += vn_sizeof_uint32_t(&val->mipmapPrecisionBits);
    size += vn_sizeof_uint32_t(&val->maxDrawIndexedIndexValue);
    size += vn_sizeof_uint32_t(&val->maxDrawIndirectCount);
    size += vn_sizeof_float(&val->maxSamplerLodBias);
    size += vn_sizeof_float(&val->maxSamplerAnisotropy);
    size += vn_sizeof_uint32_t(&val->maxViewports);
    size += vn_sizeof_array_size(2);
    size += vn_sizeof_uint32_t_array(val->maxViewportDimensions, 2);
    size += vn_sizeof_array_size(2);
    size += vn_sizeof_float_array(val->viewportBoundsRange, 2);
    size += vn_sizeof_uint32_t(&val->viewportSubPixelBits);
    size += vn_sizeof_size_t(&val->minMemoryMapAlignment);
    size += vn_sizeof_VkDeviceSize(&val->minTexelBufferOffsetAlignment);
    size += vn_sizeof_VkDeviceSize(&val->minUniformBufferOffsetAlignment);
    size += vn_sizeof_VkDeviceSize(&val->minStorageBufferOffsetAlignment);
    size += vn_sizeof_int32_t(&val->minTexelOffset);
    size += vn_sizeof_uint32_t(&val->maxTexelOffset);
    size += vn_sizeof_int32_t(&val->minTexelGatherOffset);
    size += vn_sizeof_uint32_t(&val->maxTexelGatherOffset);
    size += vn_sizeof_float(&val->minInterpolationOffset);
    size += vn_sizeof_float(&val->maxInterpolationOffset);
    size += vn_sizeof_uint32_t(&val->subPixelInterpolationOffsetBits);
    size += vn_sizeof_uint32_t(&val->maxFramebufferWidth);
    size += vn_sizeof_uint32_t(&val->maxFramebufferHeight);
    size += vn_sizeof_uint32_t(&val->maxFramebufferLayers);
    size += vn_sizeof_VkFlags(&val->framebufferColorSampleCounts);
    size += vn_sizeof_VkFlags(&val->framebufferDepthSampleCounts);
    size += vn_sizeof_VkFlags(&val->framebufferStencilSampleCounts);
    size += vn_sizeof_VkFlags(&val->framebufferNoAttachmentsSampleCounts);
    size += vn_sizeof_uint32_t(&val->maxColorAttachments);
    size += vn_sizeof_VkFlags(&val->sampledImageColorSampleCounts);
    size += vn_sizeof_VkFlags(&val->sampledImageIntegerSampleCounts);
    size += vn_sizeof_VkFlags(&val->sampledImageDepthSampleCounts);
    size += vn_sizeof_VkFlags(&val->sampledImageStencilSampleCounts);
    size += vn_sizeof_VkFlags(&val->storageImageSampleCounts);
    size += vn_sizeof_uint32_t(&val->maxSampleMaskWords);
    size += vn_sizeof_VkBool32(&val->timestampComputeAndGraphics);
    size += vn_sizeof_float(&val->timestampPeriod);
    size += vn_sizeof_uint32_t(&val->maxClipDistances);
    size += vn_sizeof_uint32_t(&val->maxCullDistances);
    size += vn_sizeof_uint32_t(&val->maxCombinedClipAndCullDistances);
    size += vn_sizeof_uint32_t(&val->discreteQueuePriorities);
    size += vn_sizeof_array_size(2);
    size += vn_sizeof_float_array(val->pointSizeRange, 2);
    size += vn_sizeof_array_size(2);
    size += vn_sizeof_float_array(val->lineWidthRange, 2);
    size += vn_sizeof_float(&val->pointSizeGranularity);
    size += vn_sizeof_float(&val->lineWidthGranularity);
    size += vn_sizeof_VkBool32(&val->strictLines);
    size += vn_sizeof_VkBool32(&val->standardSampleLocations);
    size += vn_sizeof_VkDeviceSize(&val->optimalBufferCopyOffsetAlignment);
    size += vn_sizeof_VkDeviceSize(&val->optimalBufferCopyRowPitchAlignment);
    size += vn_sizeof_VkDeviceSize(&val->nonCoherentAtomSize);
    return size;
}

static inline void
vn_decode_VkPhysicalDeviceLimits(struct vn_cs *cs, VkPhysicalDeviceLimits *val)
{
    vn_decode_uint32_t(cs, &val->maxImageDimension1D);
    vn_decode_uint32_t(cs, &val->maxImageDimension2D);
    vn_decode_uint32_t(cs, &val->maxImageDimension3D);
    vn_decode_uint32_t(cs, &val->maxImageDimensionCube);
    vn_decode_uint32_t(cs, &val->maxImageArrayLayers);
    vn_decode_uint32_t(cs, &val->maxTexelBufferElements);
    vn_decode_uint32_t(cs, &val->maxUniformBufferRange);
    vn_decode_uint32_t(cs, &val->maxStorageBufferRange);
    vn_decode_uint32_t(cs, &val->maxPushConstantsSize);
    vn_decode_uint32_t(cs, &val->maxMemoryAllocationCount);
    vn_decode_uint32_t(cs, &val->maxSamplerAllocationCount);
    vn_decode_VkDeviceSize(cs, &val->bufferImageGranularity);
    vn_decode_VkDeviceSize(cs, &val->sparseAddressSpaceSize);
    vn_decode_uint32_t(cs, &val->maxBoundDescriptorSets);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorSamplers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUniformBuffers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorStorageBuffers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorSampledImages);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorStorageImages);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorInputAttachments);
    vn_decode_uint32_t(cs, &val->maxPerStageResources);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetSamplers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUniformBuffers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUniformBuffersDynamic);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetStorageBuffers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetStorageBuffersDynamic);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetSampledImages);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetStorageImages);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetInputAttachments);
    vn_decode_uint32_t(cs, &val->maxVertexInputAttributes);
    vn_decode_uint32_t(cs, &val->maxVertexInputBindings);
    vn_decode_uint32_t(cs, &val->maxVertexInputAttributeOffset);
    vn_decode_uint32_t(cs, &val->maxVertexInputBindingStride);
    vn_decode_uint32_t(cs, &val->maxVertexOutputComponents);
    vn_decode_uint32_t(cs, &val->maxTessellationGenerationLevel);
    vn_decode_uint32_t(cs, &val->maxTessellationPatchSize);
    vn_decode_uint32_t(cs, &val->maxTessellationControlPerVertexInputComponents);
    vn_decode_uint32_t(cs, &val->maxTessellationControlPerVertexOutputComponents);
    vn_decode_uint32_t(cs, &val->maxTessellationControlPerPatchOutputComponents);
    vn_decode_uint32_t(cs, &val->maxTessellationControlTotalOutputComponents);
    vn_decode_uint32_t(cs, &val->maxTessellationEvaluationInputComponents);
    vn_decode_uint32_t(cs, &val->maxTessellationEvaluationOutputComponents);
    vn_decode_uint32_t(cs, &val->maxGeometryShaderInvocations);
    vn_decode_uint32_t(cs, &val->maxGeometryInputComponents);
    vn_decode_uint32_t(cs, &val->maxGeometryOutputComponents);
    vn_decode_uint32_t(cs, &val->maxGeometryOutputVertices);
    vn_decode_uint32_t(cs, &val->maxGeometryTotalOutputComponents);
    vn_decode_uint32_t(cs, &val->maxFragmentInputComponents);
    vn_decode_uint32_t(cs, &val->maxFragmentOutputAttachments);
    vn_decode_uint32_t(cs, &val->maxFragmentDualSrcAttachments);
    vn_decode_uint32_t(cs, &val->maxFragmentCombinedOutputResources);
    vn_decode_uint32_t(cs, &val->maxComputeSharedMemorySize);
    {
        const size_t array_size = vn_decode_array_size(cs, 3);
        vn_decode_uint32_t_array(cs, val->maxComputeWorkGroupCount, array_size);
    }
    vn_decode_uint32_t(cs, &val->maxComputeWorkGroupInvocations);
    {
        const size_t array_size = vn_decode_array_size(cs, 3);
        vn_decode_uint32_t_array(cs, val->maxComputeWorkGroupSize, array_size);
    }
    vn_decode_uint32_t(cs, &val->subPixelPrecisionBits);
    vn_decode_uint32_t(cs, &val->subTexelPrecisionBits);
    vn_decode_uint32_t(cs, &val->mipmapPrecisionBits);
    vn_decode_uint32_t(cs, &val->maxDrawIndexedIndexValue);
    vn_decode_uint32_t(cs, &val->maxDrawIndirectCount);
    vn_decode_float(cs, &val->maxSamplerLodBias);
    vn_decode_float(cs, &val->maxSamplerAnisotropy);
    vn_decode_uint32_t(cs, &val->maxViewports);
    {
        const size_t array_size = vn_decode_array_size(cs, 2);
        vn_decode_uint32_t_array(cs, val->maxViewportDimensions, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, 2);
        vn_decode_float_array(cs, val->viewportBoundsRange, array_size);
    }
    vn_decode_uint32_t(cs, &val->viewportSubPixelBits);
    vn_decode_size_t(cs, &val->minMemoryMapAlignment);
    vn_decode_VkDeviceSize(cs, &val->minTexelBufferOffsetAlignment);
    vn_decode_VkDeviceSize(cs, &val->minUniformBufferOffsetAlignment);
    vn_decode_VkDeviceSize(cs, &val->minStorageBufferOffsetAlignment);
    vn_decode_int32_t(cs, &val->minTexelOffset);
    vn_decode_uint32_t(cs, &val->maxTexelOffset);
    vn_decode_int32_t(cs, &val->minTexelGatherOffset);
    vn_decode_uint32_t(cs, &val->maxTexelGatherOffset);
    vn_decode_float(cs, &val->minInterpolationOffset);
    vn_decode_float(cs, &val->maxInterpolationOffset);
    vn_decode_uint32_t(cs, &val->subPixelInterpolationOffsetBits);
    vn_decode_uint32_t(cs, &val->maxFramebufferWidth);
    vn_decode_uint32_t(cs, &val->maxFramebufferHeight);
    vn_decode_uint32_t(cs, &val->maxFramebufferLayers);
    vn_decode_VkFlags(cs, &val->framebufferColorSampleCounts);
    vn_decode_VkFlags(cs, &val->framebufferDepthSampleCounts);
    vn_decode_VkFlags(cs, &val->framebufferStencilSampleCounts);
    vn_decode_VkFlags(cs, &val->framebufferNoAttachmentsSampleCounts);
    vn_decode_uint32_t(cs, &val->maxColorAttachments);
    vn_decode_VkFlags(cs, &val->sampledImageColorSampleCounts);
    vn_decode_VkFlags(cs, &val->sampledImageIntegerSampleCounts);
    vn_decode_VkFlags(cs, &val->sampledImageDepthSampleCounts);
    vn_decode_VkFlags(cs, &val->sampledImageStencilSampleCounts);
    vn_decode_VkFlags(cs, &val->storageImageSampleCounts);
    vn_decode_uint32_t(cs, &val->maxSampleMaskWords);
    vn_decode_VkBool32(cs, &val->timestampComputeAndGraphics);
    vn_decode_float(cs, &val->timestampPeriod);
    vn_decode_uint32_t(cs, &val->maxClipDistances);
    vn_decode_uint32_t(cs, &val->maxCullDistances);
    vn_decode_uint32_t(cs, &val->maxCombinedClipAndCullDistances);
    vn_decode_uint32_t(cs, &val->discreteQueuePriorities);
    {
        const size_t array_size = vn_decode_array_size(cs, 2);
        vn_decode_float_array(cs, val->pointSizeRange, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, 2);
        vn_decode_float_array(cs, val->lineWidthRange, array_size);
    }
    vn_decode_float(cs, &val->pointSizeGranularity);
    vn_decode_float(cs, &val->lineWidthGranularity);
    vn_decode_VkBool32(cs, &val->strictLines);
    vn_decode_VkBool32(cs, &val->standardSampleLocations);
    vn_decode_VkDeviceSize(cs, &val->optimalBufferCopyOffsetAlignment);
    vn_decode_VkDeviceSize(cs, &val->optimalBufferCopyRowPitchAlignment);
    vn_decode_VkDeviceSize(cs, &val->nonCoherentAtomSize);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceLimits_partial(const VkPhysicalDeviceLimits *val)
{
    size_t size = 0;
    /* skip val->maxImageDimension1D */
    /* skip val->maxImageDimension2D */
    /* skip val->maxImageDimension3D */
    /* skip val->maxImageDimensionCube */
    /* skip val->maxImageArrayLayers */
    /* skip val->maxTexelBufferElements */
    /* skip val->maxUniformBufferRange */
    /* skip val->maxStorageBufferRange */
    /* skip val->maxPushConstantsSize */
    /* skip val->maxMemoryAllocationCount */
    /* skip val->maxSamplerAllocationCount */
    /* skip val->bufferImageGranularity */
    /* skip val->sparseAddressSpaceSize */
    /* skip val->maxBoundDescriptorSets */
    /* skip val->maxPerStageDescriptorSamplers */
    /* skip val->maxPerStageDescriptorUniformBuffers */
    /* skip val->maxPerStageDescriptorStorageBuffers */
    /* skip val->maxPerStageDescriptorSampledImages */
    /* skip val->maxPerStageDescriptorStorageImages */
    /* skip val->maxPerStageDescriptorInputAttachments */
    /* skip val->maxPerStageResources */
    /* skip val->maxDescriptorSetSamplers */
    /* skip val->maxDescriptorSetUniformBuffers */
    /* skip val->maxDescriptorSetUniformBuffersDynamic */
    /* skip val->maxDescriptorSetStorageBuffers */
    /* skip val->maxDescriptorSetStorageBuffersDynamic */
    /* skip val->maxDescriptorSetSampledImages */
    /* skip val->maxDescriptorSetStorageImages */
    /* skip val->maxDescriptorSetInputAttachments */
    /* skip val->maxVertexInputAttributes */
    /* skip val->maxVertexInputBindings */
    /* skip val->maxVertexInputAttributeOffset */
    /* skip val->maxVertexInputBindingStride */
    /* skip val->maxVertexOutputComponents */
    /* skip val->maxTessellationGenerationLevel */
    /* skip val->maxTessellationPatchSize */
    /* skip val->maxTessellationControlPerVertexInputComponents */
    /* skip val->maxTessellationControlPerVertexOutputComponents */
    /* skip val->maxTessellationControlPerPatchOutputComponents */
    /* skip val->maxTessellationControlTotalOutputComponents */
    /* skip val->maxTessellationEvaluationInputComponents */
    /* skip val->maxTessellationEvaluationOutputComponents */
    /* skip val->maxGeometryShaderInvocations */
    /* skip val->maxGeometryInputComponents */
    /* skip val->maxGeometryOutputComponents */
    /* skip val->maxGeometryOutputVertices */
    /* skip val->maxGeometryTotalOutputComponents */
    /* skip val->maxFragmentInputComponents */
    /* skip val->maxFragmentOutputAttachments */
    /* skip val->maxFragmentDualSrcAttachments */
    /* skip val->maxFragmentCombinedOutputResources */
    /* skip val->maxComputeSharedMemorySize */
    /* skip val->maxComputeWorkGroupCount */
    /* skip val->maxComputeWorkGroupInvocations */
    /* skip val->maxComputeWorkGroupSize */
    /* skip val->subPixelPrecisionBits */
    /* skip val->subTexelPrecisionBits */
    /* skip val->mipmapPrecisionBits */
    /* skip val->maxDrawIndexedIndexValue */
    /* skip val->maxDrawIndirectCount */
    /* skip val->maxSamplerLodBias */
    /* skip val->maxSamplerAnisotropy */
    /* skip val->maxViewports */
    /* skip val->maxViewportDimensions */
    /* skip val->viewportBoundsRange */
    /* skip val->viewportSubPixelBits */
    /* skip val->minMemoryMapAlignment */
    /* skip val->minTexelBufferOffsetAlignment */
    /* skip val->minUniformBufferOffsetAlignment */
    /* skip val->minStorageBufferOffsetAlignment */
    /* skip val->minTexelOffset */
    /* skip val->maxTexelOffset */
    /* skip val->minTexelGatherOffset */
    /* skip val->maxTexelGatherOffset */
    /* skip val->minInterpolationOffset */
    /* skip val->maxInterpolationOffset */
    /* skip val->subPixelInterpolationOffsetBits */
    /* skip val->maxFramebufferWidth */
    /* skip val->maxFramebufferHeight */
    /* skip val->maxFramebufferLayers */
    /* skip val->framebufferColorSampleCounts */
    /* skip val->framebufferDepthSampleCounts */
    /* skip val->framebufferStencilSampleCounts */
    /* skip val->framebufferNoAttachmentsSampleCounts */
    /* skip val->maxColorAttachments */
    /* skip val->sampledImageColorSampleCounts */
    /* skip val->sampledImageIntegerSampleCounts */
    /* skip val->sampledImageDepthSampleCounts */
    /* skip val->sampledImageStencilSampleCounts */
    /* skip val->storageImageSampleCounts */
    /* skip val->maxSampleMaskWords */
    /* skip val->timestampComputeAndGraphics */
    /* skip val->timestampPeriod */
    /* skip val->maxClipDistances */
    /* skip val->maxCullDistances */
    /* skip val->maxCombinedClipAndCullDistances */
    /* skip val->discreteQueuePriorities */
    /* skip val->pointSizeRange */
    /* skip val->lineWidthRange */
    /* skip val->pointSizeGranularity */
    /* skip val->lineWidthGranularity */
    /* skip val->strictLines */
    /* skip val->standardSampleLocations */
    /* skip val->optimalBufferCopyOffsetAlignment */
    /* skip val->optimalBufferCopyRowPitchAlignment */
    /* skip val->nonCoherentAtomSize */
    return size;
}

static inline void
vn_encode_VkPhysicalDeviceLimits_partial(struct vn_cs *cs, const VkPhysicalDeviceLimits *val)
{
    /* skip val->maxImageDimension1D */
    /* skip val->maxImageDimension2D */
    /* skip val->maxImageDimension3D */
    /* skip val->maxImageDimensionCube */
    /* skip val->maxImageArrayLayers */
    /* skip val->maxTexelBufferElements */
    /* skip val->maxUniformBufferRange */
    /* skip val->maxStorageBufferRange */
    /* skip val->maxPushConstantsSize */
    /* skip val->maxMemoryAllocationCount */
    /* skip val->maxSamplerAllocationCount */
    /* skip val->bufferImageGranularity */
    /* skip val->sparseAddressSpaceSize */
    /* skip val->maxBoundDescriptorSets */
    /* skip val->maxPerStageDescriptorSamplers */
    /* skip val->maxPerStageDescriptorUniformBuffers */
    /* skip val->maxPerStageDescriptorStorageBuffers */
    /* skip val->maxPerStageDescriptorSampledImages */
    /* skip val->maxPerStageDescriptorStorageImages */
    /* skip val->maxPerStageDescriptorInputAttachments */
    /* skip val->maxPerStageResources */
    /* skip val->maxDescriptorSetSamplers */
    /* skip val->maxDescriptorSetUniformBuffers */
    /* skip val->maxDescriptorSetUniformBuffersDynamic */
    /* skip val->maxDescriptorSetStorageBuffers */
    /* skip val->maxDescriptorSetStorageBuffersDynamic */
    /* skip val->maxDescriptorSetSampledImages */
    /* skip val->maxDescriptorSetStorageImages */
    /* skip val->maxDescriptorSetInputAttachments */
    /* skip val->maxVertexInputAttributes */
    /* skip val->maxVertexInputBindings */
    /* skip val->maxVertexInputAttributeOffset */
    /* skip val->maxVertexInputBindingStride */
    /* skip val->maxVertexOutputComponents */
    /* skip val->maxTessellationGenerationLevel */
    /* skip val->maxTessellationPatchSize */
    /* skip val->maxTessellationControlPerVertexInputComponents */
    /* skip val->maxTessellationControlPerVertexOutputComponents */
    /* skip val->maxTessellationControlPerPatchOutputComponents */
    /* skip val->maxTessellationControlTotalOutputComponents */
    /* skip val->maxTessellationEvaluationInputComponents */
    /* skip val->maxTessellationEvaluationOutputComponents */
    /* skip val->maxGeometryShaderInvocations */
    /* skip val->maxGeometryInputComponents */
    /* skip val->maxGeometryOutputComponents */
    /* skip val->maxGeometryOutputVertices */
    /* skip val->maxGeometryTotalOutputComponents */
    /* skip val->maxFragmentInputComponents */
    /* skip val->maxFragmentOutputAttachments */
    /* skip val->maxFragmentDualSrcAttachments */
    /* skip val->maxFragmentCombinedOutputResources */
    /* skip val->maxComputeSharedMemorySize */
    /* skip val->maxComputeWorkGroupCount */
    /* skip val->maxComputeWorkGroupInvocations */
    /* skip val->maxComputeWorkGroupSize */
    /* skip val->subPixelPrecisionBits */
    /* skip val->subTexelPrecisionBits */
    /* skip val->mipmapPrecisionBits */
    /* skip val->maxDrawIndexedIndexValue */
    /* skip val->maxDrawIndirectCount */
    /* skip val->maxSamplerLodBias */
    /* skip val->maxSamplerAnisotropy */
    /* skip val->maxViewports */
    /* skip val->maxViewportDimensions */
    /* skip val->viewportBoundsRange */
    /* skip val->viewportSubPixelBits */
    /* skip val->minMemoryMapAlignment */
    /* skip val->minTexelBufferOffsetAlignment */
    /* skip val->minUniformBufferOffsetAlignment */
    /* skip val->minStorageBufferOffsetAlignment */
    /* skip val->minTexelOffset */
    /* skip val->maxTexelOffset */
    /* skip val->minTexelGatherOffset */
    /* skip val->maxTexelGatherOffset */
    /* skip val->minInterpolationOffset */
    /* skip val->maxInterpolationOffset */
    /* skip val->subPixelInterpolationOffsetBits */
    /* skip val->maxFramebufferWidth */
    /* skip val->maxFramebufferHeight */
    /* skip val->maxFramebufferLayers */
    /* skip val->framebufferColorSampleCounts */
    /* skip val->framebufferDepthSampleCounts */
    /* skip val->framebufferStencilSampleCounts */
    /* skip val->framebufferNoAttachmentsSampleCounts */
    /* skip val->maxColorAttachments */
    /* skip val->sampledImageColorSampleCounts */
    /* skip val->sampledImageIntegerSampleCounts */
    /* skip val->sampledImageDepthSampleCounts */
    /* skip val->sampledImageStencilSampleCounts */
    /* skip val->storageImageSampleCounts */
    /* skip val->maxSampleMaskWords */
    /* skip val->timestampComputeAndGraphics */
    /* skip val->timestampPeriod */
    /* skip val->maxClipDistances */
    /* skip val->maxCullDistances */
    /* skip val->maxCombinedClipAndCullDistances */
    /* skip val->discreteQueuePriorities */
    /* skip val->pointSizeRange */
    /* skip val->lineWidthRange */
    /* skip val->pointSizeGranularity */
    /* skip val->lineWidthGranularity */
    /* skip val->strictLines */
    /* skip val->standardSampleLocations */
    /* skip val->optimalBufferCopyOffsetAlignment */
    /* skip val->optimalBufferCopyRowPitchAlignment */
    /* skip val->nonCoherentAtomSize */
}

/* struct VkPhysicalDeviceSparseProperties */

static inline size_t
vn_sizeof_VkPhysicalDeviceSparseProperties(const VkPhysicalDeviceSparseProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_VkBool32(&val->residencyStandard2DBlockShape);
    size += vn_sizeof_VkBool32(&val->residencyStandard2DMultisampleBlockShape);
    size += vn_sizeof_VkBool32(&val->residencyStandard3DBlockShape);
    size += vn_sizeof_VkBool32(&val->residencyAlignedMipSize);
    size += vn_sizeof_VkBool32(&val->residencyNonResidentStrict);
    return size;
}

static inline void
vn_decode_VkPhysicalDeviceSparseProperties(struct vn_cs *cs, VkPhysicalDeviceSparseProperties *val)
{
    vn_decode_VkBool32(cs, &val->residencyStandard2DBlockShape);
    vn_decode_VkBool32(cs, &val->residencyStandard2DMultisampleBlockShape);
    vn_decode_VkBool32(cs, &val->residencyStandard3DBlockShape);
    vn_decode_VkBool32(cs, &val->residencyAlignedMipSize);
    vn_decode_VkBool32(cs, &val->residencyNonResidentStrict);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSparseProperties_partial(const VkPhysicalDeviceSparseProperties *val)
{
    size_t size = 0;
    /* skip val->residencyStandard2DBlockShape */
    /* skip val->residencyStandard2DMultisampleBlockShape */
    /* skip val->residencyStandard3DBlockShape */
    /* skip val->residencyAlignedMipSize */
    /* skip val->residencyNonResidentStrict */
    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSparseProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceSparseProperties *val)
{
    /* skip val->residencyStandard2DBlockShape */
    /* skip val->residencyStandard2DMultisampleBlockShape */
    /* skip val->residencyStandard3DBlockShape */
    /* skip val->residencyAlignedMipSize */
    /* skip val->residencyNonResidentStrict */
}

/* struct VkPhysicalDeviceProperties */

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties(const VkPhysicalDeviceProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->apiVersion);
    size += vn_sizeof_uint32_t(&val->driverVersion);
    size += vn_sizeof_uint32_t(&val->vendorID);
    size += vn_sizeof_uint32_t(&val->deviceID);
    size += vn_sizeof_VkPhysicalDeviceType(&val->deviceType);
    size += vn_sizeof_array_size(VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
    size += vn_sizeof_blob_array(val->deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
    size += vn_sizeof_array_size(VK_UUID_SIZE);
    size += vn_sizeof_uint8_t_array(val->pipelineCacheUUID, VK_UUID_SIZE);
    size += vn_sizeof_VkPhysicalDeviceLimits(&val->limits);
    size += vn_sizeof_VkPhysicalDeviceSparseProperties(&val->sparseProperties);
    return size;
}

static inline void
vn_decode_VkPhysicalDeviceProperties(struct vn_cs *cs, VkPhysicalDeviceProperties *val)
{
    vn_decode_uint32_t(cs, &val->apiVersion);
    vn_decode_uint32_t(cs, &val->driverVersion);
    vn_decode_uint32_t(cs, &val->vendorID);
    vn_decode_uint32_t(cs, &val->deviceID);
    vn_decode_VkPhysicalDeviceType(cs, &val->deviceType);
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
        vn_decode_blob_array(cs, val->deviceName, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, VK_UUID_SIZE);
        vn_decode_uint8_t_array(cs, val->pipelineCacheUUID, array_size);
    }
    vn_decode_VkPhysicalDeviceLimits(cs, &val->limits);
    vn_decode_VkPhysicalDeviceSparseProperties(cs, &val->sparseProperties);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties_partial(const VkPhysicalDeviceProperties *val)
{
    size_t size = 0;
    /* skip val->apiVersion */
    /* skip val->driverVersion */
    /* skip val->vendorID */
    /* skip val->deviceID */
    /* skip val->deviceType */
    /* skip val->deviceName */
    /* skip val->pipelineCacheUUID */
    size += vn_sizeof_VkPhysicalDeviceLimits_partial(&val->limits);
    size += vn_sizeof_VkPhysicalDeviceSparseProperties_partial(&val->sparseProperties);
    return size;
}

static inline void
vn_encode_VkPhysicalDeviceProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceProperties *val)
{
    /* skip val->apiVersion */
    /* skip val->driverVersion */
    /* skip val->vendorID */
    /* skip val->deviceID */
    /* skip val->deviceType */
    /* skip val->deviceName */
    /* skip val->pipelineCacheUUID */
    vn_encode_VkPhysicalDeviceLimits_partial(cs, &val->limits);
    vn_encode_VkPhysicalDeviceSparseProperties_partial(cs, &val->sparseProperties);
}

/* struct VkExtensionProperties */

static inline size_t
vn_sizeof_VkExtensionProperties(const VkExtensionProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_array_size(VK_MAX_EXTENSION_NAME_SIZE);
    size += vn_sizeof_blob_array(val->extensionName, VK_MAX_EXTENSION_NAME_SIZE);
    size += vn_sizeof_uint32_t(&val->specVersion);
    return size;
}

static inline void
vn_decode_VkExtensionProperties(struct vn_cs *cs, VkExtensionProperties *val)
{
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_EXTENSION_NAME_SIZE);
        vn_decode_blob_array(cs, val->extensionName, array_size);
    }
    vn_decode_uint32_t(cs, &val->specVersion);
}

static inline size_t
vn_sizeof_VkExtensionProperties_partial(const VkExtensionProperties *val)
{
    size_t size = 0;
    /* skip val->extensionName */
    /* skip val->specVersion */
    return size;
}

static inline void
vn_encode_VkExtensionProperties_partial(struct vn_cs *cs, const VkExtensionProperties *val)
{
    /* skip val->extensionName */
    /* skip val->specVersion */
}

/* struct VkLayerProperties */

static inline size_t
vn_sizeof_VkLayerProperties(const VkLayerProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_array_size(VK_MAX_EXTENSION_NAME_SIZE);
    size += vn_sizeof_blob_array(val->layerName, VK_MAX_EXTENSION_NAME_SIZE);
    size += vn_sizeof_uint32_t(&val->specVersion);
    size += vn_sizeof_uint32_t(&val->implementationVersion);
    size += vn_sizeof_array_size(VK_MAX_DESCRIPTION_SIZE);
    size += vn_sizeof_blob_array(val->description, VK_MAX_DESCRIPTION_SIZE);
    return size;
}

static inline void
vn_decode_VkLayerProperties(struct vn_cs *cs, VkLayerProperties *val)
{
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_EXTENSION_NAME_SIZE);
        vn_decode_blob_array(cs, val->layerName, array_size);
    }
    vn_decode_uint32_t(cs, &val->specVersion);
    vn_decode_uint32_t(cs, &val->implementationVersion);
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_DESCRIPTION_SIZE);
        vn_decode_blob_array(cs, val->description, array_size);
    }
}

static inline size_t
vn_sizeof_VkLayerProperties_partial(const VkLayerProperties *val)
{
    size_t size = 0;
    /* skip val->layerName */
    /* skip val->specVersion */
    /* skip val->implementationVersion */
    /* skip val->description */
    return size;
}

static inline void
vn_encode_VkLayerProperties_partial(struct vn_cs *cs, const VkLayerProperties *val)
{
    /* skip val->layerName */
    /* skip val->specVersion */
    /* skip val->implementationVersion */
    /* skip val->description */
}

/* struct VkApplicationInfo chain */

static inline size_t
vn_sizeof_VkApplicationInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkApplicationInfo_self(const VkApplicationInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    if (val->pApplicationName) {
        const size_t string_size = strlen(val->pApplicationName) + 1;
        size += vn_sizeof_array_size(string_size);
        size += vn_sizeof_blob_array(val->pApplicationName, string_size);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->applicationVersion);
    if (val->pEngineName) {
        const size_t string_size = strlen(val->pEngineName) + 1;
        size += vn_sizeof_array_size(string_size);
        size += vn_sizeof_blob_array(val->pEngineName, string_size);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->engineVersion);
    size += vn_sizeof_uint32_t(&val->apiVersion);
    return size;
}

static inline size_t
vn_sizeof_VkApplicationInfo(const VkApplicationInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkApplicationInfo_pnext(val->pNext);
    size += vn_sizeof_VkApplicationInfo_self(val);

    return size;
}

static inline void
vn_encode_VkApplicationInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkApplicationInfo_self(struct vn_cs *cs, const VkApplicationInfo *val)
{
    /* skip val->{sType,pNext} */
    if (val->pApplicationName) {
        const size_t string_size = strlen(val->pApplicationName) + 1;
        vn_encode_array_size(cs, string_size);
        vn_encode_blob_array(cs, val->pApplicationName, string_size);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->applicationVersion);
    if (val->pEngineName) {
        const size_t string_size = strlen(val->pEngineName) + 1;
        vn_encode_array_size(cs, string_size);
        vn_encode_blob_array(cs, val->pEngineName, string_size);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->engineVersion);
    vn_encode_uint32_t(cs, &val->apiVersion);
}

static inline void
vn_encode_VkApplicationInfo(struct vn_cs *cs, const VkApplicationInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_APPLICATION_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_APPLICATION_INFO });
    vn_encode_VkApplicationInfo_pnext(cs, val->pNext);
    vn_encode_VkApplicationInfo_self(cs, val);
}

/* struct VkDeviceQueueCreateInfo chain */

static inline size_t
vn_sizeof_VkDeviceQueueCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceQueueCreateInfo_self(const VkDeviceQueueCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->queueFamilyIndex);
    size += vn_sizeof_uint32_t(&val->queueCount);
    if (val->pQueuePriorities) {
        size += vn_sizeof_array_size(val->queueCount);
        size += vn_sizeof_float_array(val->pQueuePriorities, val->queueCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDeviceQueueCreateInfo(const VkDeviceQueueCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceQueueCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceQueueCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceQueueCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceQueueCreateInfo_self(struct vn_cs *cs, const VkDeviceQueueCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->queueFamilyIndex);
    vn_encode_uint32_t(cs, &val->queueCount);
    if (val->pQueuePriorities) {
        vn_encode_array_size(cs, val->queueCount);
        vn_encode_float_array(cs, val->pQueuePriorities, val->queueCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDeviceQueueCreateInfo(struct vn_cs *cs, const VkDeviceQueueCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO });
    vn_encode_VkDeviceQueueCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceQueueCreateInfo_self(cs, val);
}

/* struct VkPhysicalDeviceFeatures */

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures *val)
{
    size_t size = 0;
    size += vn_sizeof_VkBool32(&val->robustBufferAccess);
    size += vn_sizeof_VkBool32(&val->fullDrawIndexUint32);
    size += vn_sizeof_VkBool32(&val->imageCubeArray);
    size += vn_sizeof_VkBool32(&val->independentBlend);
    size += vn_sizeof_VkBool32(&val->geometryShader);
    size += vn_sizeof_VkBool32(&val->tessellationShader);
    size += vn_sizeof_VkBool32(&val->sampleRateShading);
    size += vn_sizeof_VkBool32(&val->dualSrcBlend);
    size += vn_sizeof_VkBool32(&val->logicOp);
    size += vn_sizeof_VkBool32(&val->multiDrawIndirect);
    size += vn_sizeof_VkBool32(&val->drawIndirectFirstInstance);
    size += vn_sizeof_VkBool32(&val->depthClamp);
    size += vn_sizeof_VkBool32(&val->depthBiasClamp);
    size += vn_sizeof_VkBool32(&val->fillModeNonSolid);
    size += vn_sizeof_VkBool32(&val->depthBounds);
    size += vn_sizeof_VkBool32(&val->wideLines);
    size += vn_sizeof_VkBool32(&val->largePoints);
    size += vn_sizeof_VkBool32(&val->alphaToOne);
    size += vn_sizeof_VkBool32(&val->multiViewport);
    size += vn_sizeof_VkBool32(&val->samplerAnisotropy);
    size += vn_sizeof_VkBool32(&val->textureCompressionETC2);
    size += vn_sizeof_VkBool32(&val->textureCompressionASTC_LDR);
    size += vn_sizeof_VkBool32(&val->textureCompressionBC);
    size += vn_sizeof_VkBool32(&val->occlusionQueryPrecise);
    size += vn_sizeof_VkBool32(&val->pipelineStatisticsQuery);
    size += vn_sizeof_VkBool32(&val->vertexPipelineStoresAndAtomics);
    size += vn_sizeof_VkBool32(&val->fragmentStoresAndAtomics);
    size += vn_sizeof_VkBool32(&val->shaderTessellationAndGeometryPointSize);
    size += vn_sizeof_VkBool32(&val->shaderImageGatherExtended);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageExtendedFormats);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageMultisample);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageReadWithoutFormat);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageWriteWithoutFormat);
    size += vn_sizeof_VkBool32(&val->shaderUniformBufferArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderSampledImageArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageBufferArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderClipDistance);
    size += vn_sizeof_VkBool32(&val->shaderCullDistance);
    size += vn_sizeof_VkBool32(&val->shaderFloat64);
    size += vn_sizeof_VkBool32(&val->shaderInt64);
    size += vn_sizeof_VkBool32(&val->shaderInt16);
    size += vn_sizeof_VkBool32(&val->shaderResourceResidency);
    size += vn_sizeof_VkBool32(&val->shaderResourceMinLod);
    size += vn_sizeof_VkBool32(&val->sparseBinding);
    size += vn_sizeof_VkBool32(&val->sparseResidencyBuffer);
    size += vn_sizeof_VkBool32(&val->sparseResidencyImage2D);
    size += vn_sizeof_VkBool32(&val->sparseResidencyImage3D);
    size += vn_sizeof_VkBool32(&val->sparseResidency2Samples);
    size += vn_sizeof_VkBool32(&val->sparseResidency4Samples);
    size += vn_sizeof_VkBool32(&val->sparseResidency8Samples);
    size += vn_sizeof_VkBool32(&val->sparseResidency16Samples);
    size += vn_sizeof_VkBool32(&val->sparseResidencyAliased);
    size += vn_sizeof_VkBool32(&val->variableMultisampleRate);
    size += vn_sizeof_VkBool32(&val->inheritedQueries);
    return size;
}

static inline void
vn_encode_VkPhysicalDeviceFeatures(struct vn_cs *cs, const VkPhysicalDeviceFeatures *val)
{
    vn_encode_VkBool32(cs, &val->robustBufferAccess);
    vn_encode_VkBool32(cs, &val->fullDrawIndexUint32);
    vn_encode_VkBool32(cs, &val->imageCubeArray);
    vn_encode_VkBool32(cs, &val->independentBlend);
    vn_encode_VkBool32(cs, &val->geometryShader);
    vn_encode_VkBool32(cs, &val->tessellationShader);
    vn_encode_VkBool32(cs, &val->sampleRateShading);
    vn_encode_VkBool32(cs, &val->dualSrcBlend);
    vn_encode_VkBool32(cs, &val->logicOp);
    vn_encode_VkBool32(cs, &val->multiDrawIndirect);
    vn_encode_VkBool32(cs, &val->drawIndirectFirstInstance);
    vn_encode_VkBool32(cs, &val->depthClamp);
    vn_encode_VkBool32(cs, &val->depthBiasClamp);
    vn_encode_VkBool32(cs, &val->fillModeNonSolid);
    vn_encode_VkBool32(cs, &val->depthBounds);
    vn_encode_VkBool32(cs, &val->wideLines);
    vn_encode_VkBool32(cs, &val->largePoints);
    vn_encode_VkBool32(cs, &val->alphaToOne);
    vn_encode_VkBool32(cs, &val->multiViewport);
    vn_encode_VkBool32(cs, &val->samplerAnisotropy);
    vn_encode_VkBool32(cs, &val->textureCompressionETC2);
    vn_encode_VkBool32(cs, &val->textureCompressionASTC_LDR);
    vn_encode_VkBool32(cs, &val->textureCompressionBC);
    vn_encode_VkBool32(cs, &val->occlusionQueryPrecise);
    vn_encode_VkBool32(cs, &val->pipelineStatisticsQuery);
    vn_encode_VkBool32(cs, &val->vertexPipelineStoresAndAtomics);
    vn_encode_VkBool32(cs, &val->fragmentStoresAndAtomics);
    vn_encode_VkBool32(cs, &val->shaderTessellationAndGeometryPointSize);
    vn_encode_VkBool32(cs, &val->shaderImageGatherExtended);
    vn_encode_VkBool32(cs, &val->shaderStorageImageExtendedFormats);
    vn_encode_VkBool32(cs, &val->shaderStorageImageMultisample);
    vn_encode_VkBool32(cs, &val->shaderStorageImageReadWithoutFormat);
    vn_encode_VkBool32(cs, &val->shaderStorageImageWriteWithoutFormat);
    vn_encode_VkBool32(cs, &val->shaderUniformBufferArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderSampledImageArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageBufferArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageImageArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderClipDistance);
    vn_encode_VkBool32(cs, &val->shaderCullDistance);
    vn_encode_VkBool32(cs, &val->shaderFloat64);
    vn_encode_VkBool32(cs, &val->shaderInt64);
    vn_encode_VkBool32(cs, &val->shaderInt16);
    vn_encode_VkBool32(cs, &val->shaderResourceResidency);
    vn_encode_VkBool32(cs, &val->shaderResourceMinLod);
    vn_encode_VkBool32(cs, &val->sparseBinding);
    vn_encode_VkBool32(cs, &val->sparseResidencyBuffer);
    vn_encode_VkBool32(cs, &val->sparseResidencyImage2D);
    vn_encode_VkBool32(cs, &val->sparseResidencyImage3D);
    vn_encode_VkBool32(cs, &val->sparseResidency2Samples);
    vn_encode_VkBool32(cs, &val->sparseResidency4Samples);
    vn_encode_VkBool32(cs, &val->sparseResidency8Samples);
    vn_encode_VkBool32(cs, &val->sparseResidency16Samples);
    vn_encode_VkBool32(cs, &val->sparseResidencyAliased);
    vn_encode_VkBool32(cs, &val->variableMultisampleRate);
    vn_encode_VkBool32(cs, &val->inheritedQueries);
}

static inline void
vn_decode_VkPhysicalDeviceFeatures(struct vn_cs *cs, VkPhysicalDeviceFeatures *val)
{
    vn_decode_VkBool32(cs, &val->robustBufferAccess);
    vn_decode_VkBool32(cs, &val->fullDrawIndexUint32);
    vn_decode_VkBool32(cs, &val->imageCubeArray);
    vn_decode_VkBool32(cs, &val->independentBlend);
    vn_decode_VkBool32(cs, &val->geometryShader);
    vn_decode_VkBool32(cs, &val->tessellationShader);
    vn_decode_VkBool32(cs, &val->sampleRateShading);
    vn_decode_VkBool32(cs, &val->dualSrcBlend);
    vn_decode_VkBool32(cs, &val->logicOp);
    vn_decode_VkBool32(cs, &val->multiDrawIndirect);
    vn_decode_VkBool32(cs, &val->drawIndirectFirstInstance);
    vn_decode_VkBool32(cs, &val->depthClamp);
    vn_decode_VkBool32(cs, &val->depthBiasClamp);
    vn_decode_VkBool32(cs, &val->fillModeNonSolid);
    vn_decode_VkBool32(cs, &val->depthBounds);
    vn_decode_VkBool32(cs, &val->wideLines);
    vn_decode_VkBool32(cs, &val->largePoints);
    vn_decode_VkBool32(cs, &val->alphaToOne);
    vn_decode_VkBool32(cs, &val->multiViewport);
    vn_decode_VkBool32(cs, &val->samplerAnisotropy);
    vn_decode_VkBool32(cs, &val->textureCompressionETC2);
    vn_decode_VkBool32(cs, &val->textureCompressionASTC_LDR);
    vn_decode_VkBool32(cs, &val->textureCompressionBC);
    vn_decode_VkBool32(cs, &val->occlusionQueryPrecise);
    vn_decode_VkBool32(cs, &val->pipelineStatisticsQuery);
    vn_decode_VkBool32(cs, &val->vertexPipelineStoresAndAtomics);
    vn_decode_VkBool32(cs, &val->fragmentStoresAndAtomics);
    vn_decode_VkBool32(cs, &val->shaderTessellationAndGeometryPointSize);
    vn_decode_VkBool32(cs, &val->shaderImageGatherExtended);
    vn_decode_VkBool32(cs, &val->shaderStorageImageExtendedFormats);
    vn_decode_VkBool32(cs, &val->shaderStorageImageMultisample);
    vn_decode_VkBool32(cs, &val->shaderStorageImageReadWithoutFormat);
    vn_decode_VkBool32(cs, &val->shaderStorageImageWriteWithoutFormat);
    vn_decode_VkBool32(cs, &val->shaderUniformBufferArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderSampledImageArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageBufferArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageImageArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderClipDistance);
    vn_decode_VkBool32(cs, &val->shaderCullDistance);
    vn_decode_VkBool32(cs, &val->shaderFloat64);
    vn_decode_VkBool32(cs, &val->shaderInt64);
    vn_decode_VkBool32(cs, &val->shaderInt16);
    vn_decode_VkBool32(cs, &val->shaderResourceResidency);
    vn_decode_VkBool32(cs, &val->shaderResourceMinLod);
    vn_decode_VkBool32(cs, &val->sparseBinding);
    vn_decode_VkBool32(cs, &val->sparseResidencyBuffer);
    vn_decode_VkBool32(cs, &val->sparseResidencyImage2D);
    vn_decode_VkBool32(cs, &val->sparseResidencyImage3D);
    vn_decode_VkBool32(cs, &val->sparseResidency2Samples);
    vn_decode_VkBool32(cs, &val->sparseResidency4Samples);
    vn_decode_VkBool32(cs, &val->sparseResidency8Samples);
    vn_decode_VkBool32(cs, &val->sparseResidency16Samples);
    vn_decode_VkBool32(cs, &val->sparseResidencyAliased);
    vn_decode_VkBool32(cs, &val->variableMultisampleRate);
    vn_decode_VkBool32(cs, &val->inheritedQueries);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures_partial(const VkPhysicalDeviceFeatures *val)
{
    size_t size = 0;
    /* skip val->robustBufferAccess */
    /* skip val->fullDrawIndexUint32 */
    /* skip val->imageCubeArray */
    /* skip val->independentBlend */
    /* skip val->geometryShader */
    /* skip val->tessellationShader */
    /* skip val->sampleRateShading */
    /* skip val->dualSrcBlend */
    /* skip val->logicOp */
    /* skip val->multiDrawIndirect */
    /* skip val->drawIndirectFirstInstance */
    /* skip val->depthClamp */
    /* skip val->depthBiasClamp */
    /* skip val->fillModeNonSolid */
    /* skip val->depthBounds */
    /* skip val->wideLines */
    /* skip val->largePoints */
    /* skip val->alphaToOne */
    /* skip val->multiViewport */
    /* skip val->samplerAnisotropy */
    /* skip val->textureCompressionETC2 */
    /* skip val->textureCompressionASTC_LDR */
    /* skip val->textureCompressionBC */
    /* skip val->occlusionQueryPrecise */
    /* skip val->pipelineStatisticsQuery */
    /* skip val->vertexPipelineStoresAndAtomics */
    /* skip val->fragmentStoresAndAtomics */
    /* skip val->shaderTessellationAndGeometryPointSize */
    /* skip val->shaderImageGatherExtended */
    /* skip val->shaderStorageImageExtendedFormats */
    /* skip val->shaderStorageImageMultisample */
    /* skip val->shaderStorageImageReadWithoutFormat */
    /* skip val->shaderStorageImageWriteWithoutFormat */
    /* skip val->shaderUniformBufferArrayDynamicIndexing */
    /* skip val->shaderSampledImageArrayDynamicIndexing */
    /* skip val->shaderStorageBufferArrayDynamicIndexing */
    /* skip val->shaderStorageImageArrayDynamicIndexing */
    /* skip val->shaderClipDistance */
    /* skip val->shaderCullDistance */
    /* skip val->shaderFloat64 */
    /* skip val->shaderInt64 */
    /* skip val->shaderInt16 */
    /* skip val->shaderResourceResidency */
    /* skip val->shaderResourceMinLod */
    /* skip val->sparseBinding */
    /* skip val->sparseResidencyBuffer */
    /* skip val->sparseResidencyImage2D */
    /* skip val->sparseResidencyImage3D */
    /* skip val->sparseResidency2Samples */
    /* skip val->sparseResidency4Samples */
    /* skip val->sparseResidency8Samples */
    /* skip val->sparseResidency16Samples */
    /* skip val->sparseResidencyAliased */
    /* skip val->variableMultisampleRate */
    /* skip val->inheritedQueries */
    return size;
}

static inline void
vn_encode_VkPhysicalDeviceFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceFeatures *val)
{
    /* skip val->robustBufferAccess */
    /* skip val->fullDrawIndexUint32 */
    /* skip val->imageCubeArray */
    /* skip val->independentBlend */
    /* skip val->geometryShader */
    /* skip val->tessellationShader */
    /* skip val->sampleRateShading */
    /* skip val->dualSrcBlend */
    /* skip val->logicOp */
    /* skip val->multiDrawIndirect */
    /* skip val->drawIndirectFirstInstance */
    /* skip val->depthClamp */
    /* skip val->depthBiasClamp */
    /* skip val->fillModeNonSolid */
    /* skip val->depthBounds */
    /* skip val->wideLines */
    /* skip val->largePoints */
    /* skip val->alphaToOne */
    /* skip val->multiViewport */
    /* skip val->samplerAnisotropy */
    /* skip val->textureCompressionETC2 */
    /* skip val->textureCompressionASTC_LDR */
    /* skip val->textureCompressionBC */
    /* skip val->occlusionQueryPrecise */
    /* skip val->pipelineStatisticsQuery */
    /* skip val->vertexPipelineStoresAndAtomics */
    /* skip val->fragmentStoresAndAtomics */
    /* skip val->shaderTessellationAndGeometryPointSize */
    /* skip val->shaderImageGatherExtended */
    /* skip val->shaderStorageImageExtendedFormats */
    /* skip val->shaderStorageImageMultisample */
    /* skip val->shaderStorageImageReadWithoutFormat */
    /* skip val->shaderStorageImageWriteWithoutFormat */
    /* skip val->shaderUniformBufferArrayDynamicIndexing */
    /* skip val->shaderSampledImageArrayDynamicIndexing */
    /* skip val->shaderStorageBufferArrayDynamicIndexing */
    /* skip val->shaderStorageImageArrayDynamicIndexing */
    /* skip val->shaderClipDistance */
    /* skip val->shaderCullDistance */
    /* skip val->shaderFloat64 */
    /* skip val->shaderInt64 */
    /* skip val->shaderInt16 */
    /* skip val->shaderResourceResidency */
    /* skip val->shaderResourceMinLod */
    /* skip val->sparseBinding */
    /* skip val->sparseResidencyBuffer */
    /* skip val->sparseResidencyImage2D */
    /* skip val->sparseResidencyImage3D */
    /* skip val->sparseResidency2Samples */
    /* skip val->sparseResidency4Samples */
    /* skip val->sparseResidency8Samples */
    /* skip val->sparseResidency16Samples */
    /* skip val->sparseResidencyAliased */
    /* skip val->variableMultisampleRate */
    /* skip val->inheritedQueries */
}

/* struct VkPhysicalDeviceVariablePointersFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_self(const VkPhysicalDeviceVariablePointersFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->variablePointersStorageBuffer);
    size += vn_sizeof_VkBool32(&val->variablePointers);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVariablePointersFeatures(const VkPhysicalDeviceVariablePointersFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVariablePointersFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVariablePointersFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceVariablePointersFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->variablePointersStorageBuffer);
    vn_encode_VkBool32(cs, &val->variablePointers);
}

static inline void
vn_encode_VkPhysicalDeviceVariablePointersFeatures(struct vn_cs *cs, const VkPhysicalDeviceVariablePointersFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES });
    vn_encode_VkPhysicalDeviceVariablePointersFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVariablePointersFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceVariablePointersFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceVariablePointersFeatures_self(struct vn_cs *cs, VkPhysicalDeviceVariablePointersFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->variablePointersStorageBuffer);
    vn_decode_VkBool32(cs, &val->variablePointers);
}

static inline void
vn_decode_VkPhysicalDeviceVariablePointersFeatures(struct vn_cs *cs, VkPhysicalDeviceVariablePointersFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceVariablePointersFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceVariablePointersFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_self_partial(const VkPhysicalDeviceVariablePointersFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->variablePointersStorageBuffer */
    /* skip val->variablePointers */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_partial(const VkPhysicalDeviceVariablePointersFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVariablePointersFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVariablePointersFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceVariablePointersFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->variablePointersStorageBuffer */
    /* skip val->variablePointers */
}

static inline void
vn_encode_VkPhysicalDeviceVariablePointersFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceVariablePointersFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES });
    vn_encode_VkPhysicalDeviceVariablePointersFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVariablePointersFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceMultiviewFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewFeatures_self(const VkPhysicalDeviceMultiviewFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->multiview);
    size += vn_sizeof_VkBool32(&val->multiviewGeometryShader);
    size += vn_sizeof_VkBool32(&val->multiviewTessellationShader);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewFeatures(const VkPhysicalDeviceMultiviewFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMultiviewFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMultiviewFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceMultiviewFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->multiview);
    vn_encode_VkBool32(cs, &val->multiviewGeometryShader);
    vn_encode_VkBool32(cs, &val->multiviewTessellationShader);
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewFeatures(struct vn_cs *cs, const VkPhysicalDeviceMultiviewFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES });
    vn_encode_VkPhysicalDeviceMultiviewFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceMultiviewFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceMultiviewFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceMultiviewFeatures_self(struct vn_cs *cs, VkPhysicalDeviceMultiviewFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->multiview);
    vn_decode_VkBool32(cs, &val->multiviewGeometryShader);
    vn_decode_VkBool32(cs, &val->multiviewTessellationShader);
}

static inline void
vn_decode_VkPhysicalDeviceMultiviewFeatures(struct vn_cs *cs, VkPhysicalDeviceMultiviewFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceMultiviewFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceMultiviewFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewFeatures_self_partial(const VkPhysicalDeviceMultiviewFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->multiview */
    /* skip val->multiviewGeometryShader */
    /* skip val->multiviewTessellationShader */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewFeatures_partial(const VkPhysicalDeviceMultiviewFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMultiviewFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMultiviewFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceMultiviewFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->multiview */
    /* skip val->multiviewGeometryShader */
    /* skip val->multiviewTessellationShader */
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceMultiviewFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES });
    vn_encode_VkPhysicalDeviceMultiviewFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceMultiviewFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDevice16BitStorageFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDevice16BitStorageFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDevice16BitStorageFeatures_self(const VkPhysicalDevice16BitStorageFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->storageBuffer16BitAccess);
    size += vn_sizeof_VkBool32(&val->uniformAndStorageBuffer16BitAccess);
    size += vn_sizeof_VkBool32(&val->storagePushConstant16);
    size += vn_sizeof_VkBool32(&val->storageInputOutput16);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDevice16BitStorageFeatures(const VkPhysicalDevice16BitStorageFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDevice16BitStorageFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDevice16BitStorageFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDevice16BitStorageFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDevice16BitStorageFeatures_self(struct vn_cs *cs, const VkPhysicalDevice16BitStorageFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->storageBuffer16BitAccess);
    vn_encode_VkBool32(cs, &val->uniformAndStorageBuffer16BitAccess);
    vn_encode_VkBool32(cs, &val->storagePushConstant16);
    vn_encode_VkBool32(cs, &val->storageInputOutput16);
}

static inline void
vn_encode_VkPhysicalDevice16BitStorageFeatures(struct vn_cs *cs, const VkPhysicalDevice16BitStorageFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES });
    vn_encode_VkPhysicalDevice16BitStorageFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDevice16BitStorageFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDevice16BitStorageFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDevice16BitStorageFeatures_self(struct vn_cs *cs, VkPhysicalDevice16BitStorageFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->storageBuffer16BitAccess);
    vn_decode_VkBool32(cs, &val->uniformAndStorageBuffer16BitAccess);
    vn_decode_VkBool32(cs, &val->storagePushConstant16);
    vn_decode_VkBool32(cs, &val->storageInputOutput16);
}

static inline void
vn_decode_VkPhysicalDevice16BitStorageFeatures(struct vn_cs *cs, VkPhysicalDevice16BitStorageFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDevice16BitStorageFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDevice16BitStorageFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDevice16BitStorageFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDevice16BitStorageFeatures_self_partial(const VkPhysicalDevice16BitStorageFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->storageBuffer16BitAccess */
    /* skip val->uniformAndStorageBuffer16BitAccess */
    /* skip val->storagePushConstant16 */
    /* skip val->storageInputOutput16 */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDevice16BitStorageFeatures_partial(const VkPhysicalDevice16BitStorageFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDevice16BitStorageFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDevice16BitStorageFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDevice16BitStorageFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDevice16BitStorageFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDevice16BitStorageFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->storageBuffer16BitAccess */
    /* skip val->uniformAndStorageBuffer16BitAccess */
    /* skip val->storagePushConstant16 */
    /* skip val->storageInputOutput16 */
}

static inline void
vn_encode_VkPhysicalDevice16BitStorageFeatures_partial(struct vn_cs *cs, const VkPhysicalDevice16BitStorageFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES });
    vn_encode_VkPhysicalDevice16BitStorageFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDevice16BitStorageFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->shaderSubgroupExtendedTypes);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures(const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->shaderSubgroupExtendedTypes);
}

static inline void
vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures(struct vn_cs *cs, const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES });
    vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(struct vn_cs *cs, VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->shaderSubgroupExtendedTypes);
}

static inline void
vn_decode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures(struct vn_cs *cs, VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self_partial(const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->shaderSubgroupExtendedTypes */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_partial(const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->shaderSubgroupExtendedTypes */
}

static inline void
vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES });
    vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceSamplerYcbcrConversionFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->samplerYcbcrConversion);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures(const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->samplerYcbcrConversion);
}

static inline void
vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures(struct vn_cs *cs, const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES });
    vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(struct vn_cs *cs, VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->samplerYcbcrConversion);
}

static inline void
vn_decode_VkPhysicalDeviceSamplerYcbcrConversionFeatures(struct vn_cs *cs, VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self_partial(const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->samplerYcbcrConversion */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_partial(const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->samplerYcbcrConversion */
}

static inline void
vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceSamplerYcbcrConversionFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES });
    vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceProtectedMemoryFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_self(const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->protectedMemory);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures(const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->protectedMemory);
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryFeatures(struct vn_cs *cs, const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES });
    vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceProtectedMemoryFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceProtectedMemoryFeatures_self(struct vn_cs *cs, VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->protectedMemory);
}

static inline void
vn_decode_VkPhysicalDeviceProtectedMemoryFeatures(struct vn_cs *cs, VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceProtectedMemoryFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceProtectedMemoryFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_self_partial(const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->protectedMemory */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_partial(const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->protectedMemory */
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceProtectedMemoryFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES });
    vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceShaderDrawParametersFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_self(const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->shaderDrawParameters);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures(const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->shaderDrawParameters);
}

static inline void
vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures(struct vn_cs *cs, const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES });
    vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceShaderDrawParametersFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceShaderDrawParametersFeatures_self(struct vn_cs *cs, VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->shaderDrawParameters);
}

static inline void
vn_decode_VkPhysicalDeviceShaderDrawParametersFeatures(struct vn_cs *cs, VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceShaderDrawParametersFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceShaderDrawParametersFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_self_partial(const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->shaderDrawParameters */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_partial(const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->shaderDrawParameters */
}

static inline void
vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderDrawParametersFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES });
    vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceShaderFloat16Int8Features chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_self(const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->shaderFloat16);
    size += vn_sizeof_VkBool32(&val->shaderInt8);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features(const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_self(struct vn_cs *cs, const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->shaderFloat16);
    vn_encode_VkBool32(cs, &val->shaderInt8);
}

static inline void
vn_encode_VkPhysicalDeviceShaderFloat16Int8Features(struct vn_cs *cs, const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES });
    vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceShaderFloat16Int8Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceShaderFloat16Int8Features_self(struct vn_cs *cs, VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->shaderFloat16);
    vn_decode_VkBool32(cs, &val->shaderInt8);
}

static inline void
vn_decode_VkPhysicalDeviceShaderFloat16Int8Features(struct vn_cs *cs, VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceShaderFloat16Int8Features_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceShaderFloat16Int8Features_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_self_partial(const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->shaderFloat16 */
    /* skip val->shaderInt8 */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_partial(const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_self_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->shaderFloat16 */
    /* skip val->shaderInt8 */
}

static inline void
vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderFloat16Int8Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES });
    vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_self_partial(cs, val);
}

/* struct VkPhysicalDeviceHostQueryResetFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_self(const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->hostQueryReset);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures(const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceHostQueryResetFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceHostQueryResetFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->hostQueryReset);
}

static inline void
vn_encode_VkPhysicalDeviceHostQueryResetFeatures(struct vn_cs *cs, const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES });
    vn_encode_VkPhysicalDeviceHostQueryResetFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceHostQueryResetFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceHostQueryResetFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceHostQueryResetFeatures_self(struct vn_cs *cs, VkPhysicalDeviceHostQueryResetFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->hostQueryReset);
}

static inline void
vn_decode_VkPhysicalDeviceHostQueryResetFeatures(struct vn_cs *cs, VkPhysicalDeviceHostQueryResetFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceHostQueryResetFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceHostQueryResetFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_self_partial(const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->hostQueryReset */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_partial(const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceHostQueryResetFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceHostQueryResetFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->hostQueryReset */
}

static inline void
vn_encode_VkPhysicalDeviceHostQueryResetFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceHostQueryResetFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES });
    vn_encode_VkPhysicalDeviceHostQueryResetFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceHostQueryResetFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceDescriptorIndexingFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_self(const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->shaderInputAttachmentArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderUniformTexelBufferArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageTexelBufferArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderUniformBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderSampledImageArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderInputAttachmentArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderUniformTexelBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageTexelBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->descriptorBindingUniformBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingSampledImageUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingStorageImageUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingStorageBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingUniformTexelBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingStorageTexelBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingUpdateUnusedWhilePending);
    size += vn_sizeof_VkBool32(&val->descriptorBindingPartiallyBound);
    size += vn_sizeof_VkBool32(&val->descriptorBindingVariableDescriptorCount);
    size += vn_sizeof_VkBool32(&val->runtimeDescriptorArray);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures(const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->shaderInputAttachmentArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderUniformTexelBufferArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageTexelBufferArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderUniformBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderSampledImageArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageImageArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderInputAttachmentArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderUniformTexelBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageTexelBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->descriptorBindingUniformBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingSampledImageUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingStorageImageUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingStorageBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingUniformTexelBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingStorageTexelBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingUpdateUnusedWhilePending);
    vn_encode_VkBool32(cs, &val->descriptorBindingPartiallyBound);
    vn_encode_VkBool32(cs, &val->descriptorBindingVariableDescriptorCount);
    vn_encode_VkBool32(cs, &val->runtimeDescriptorArray);
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures(struct vn_cs *cs, const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES });
    vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceDescriptorIndexingFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceDescriptorIndexingFeatures_self(struct vn_cs *cs, VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->shaderInputAttachmentArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderUniformTexelBufferArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageTexelBufferArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderUniformBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderSampledImageArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageImageArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderInputAttachmentArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderUniformTexelBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageTexelBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->descriptorBindingUniformBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingSampledImageUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingStorageImageUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingStorageBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingUniformTexelBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingStorageTexelBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingUpdateUnusedWhilePending);
    vn_decode_VkBool32(cs, &val->descriptorBindingPartiallyBound);
    vn_decode_VkBool32(cs, &val->descriptorBindingVariableDescriptorCount);
    vn_decode_VkBool32(cs, &val->runtimeDescriptorArray);
}

static inline void
vn_decode_VkPhysicalDeviceDescriptorIndexingFeatures(struct vn_cs *cs, VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceDescriptorIndexingFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceDescriptorIndexingFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_self_partial(const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->shaderInputAttachmentArrayDynamicIndexing */
    /* skip val->shaderUniformTexelBufferArrayDynamicIndexing */
    /* skip val->shaderStorageTexelBufferArrayDynamicIndexing */
    /* skip val->shaderUniformBufferArrayNonUniformIndexing */
    /* skip val->shaderSampledImageArrayNonUniformIndexing */
    /* skip val->shaderStorageBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageImageArrayNonUniformIndexing */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexing */
    /* skip val->shaderUniformTexelBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageTexelBufferArrayNonUniformIndexing */
    /* skip val->descriptorBindingUniformBufferUpdateAfterBind */
    /* skip val->descriptorBindingSampledImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageBufferUpdateAfterBind */
    /* skip val->descriptorBindingUniformTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingStorageTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingUpdateUnusedWhilePending */
    /* skip val->descriptorBindingPartiallyBound */
    /* skip val->descriptorBindingVariableDescriptorCount */
    /* skip val->runtimeDescriptorArray */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_partial(const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->shaderInputAttachmentArrayDynamicIndexing */
    /* skip val->shaderUniformTexelBufferArrayDynamicIndexing */
    /* skip val->shaderStorageTexelBufferArrayDynamicIndexing */
    /* skip val->shaderUniformBufferArrayNonUniformIndexing */
    /* skip val->shaderSampledImageArrayNonUniformIndexing */
    /* skip val->shaderStorageBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageImageArrayNonUniformIndexing */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexing */
    /* skip val->shaderUniformTexelBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageTexelBufferArrayNonUniformIndexing */
    /* skip val->descriptorBindingUniformBufferUpdateAfterBind */
    /* skip val->descriptorBindingSampledImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageBufferUpdateAfterBind */
    /* skip val->descriptorBindingUniformTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingStorageTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingUpdateUnusedWhilePending */
    /* skip val->descriptorBindingPartiallyBound */
    /* skip val->descriptorBindingVariableDescriptorCount */
    /* skip val->runtimeDescriptorArray */
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceDescriptorIndexingFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES });
    vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceTimelineSemaphoreFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_self(const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->timelineSemaphore);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures(const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->timelineSemaphore);
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures(struct vn_cs *cs, const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES });
    vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceTimelineSemaphoreFeatures_self(struct vn_cs *cs, VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->timelineSemaphore);
}

static inline void
vn_decode_VkPhysicalDeviceTimelineSemaphoreFeatures(struct vn_cs *cs, VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceTimelineSemaphoreFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_self_partial(const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->timelineSemaphore */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_partial(const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->timelineSemaphore */
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceTimelineSemaphoreFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES });
    vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDevice8BitStorageFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDevice8BitStorageFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDevice8BitStorageFeatures_self(const VkPhysicalDevice8BitStorageFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->storageBuffer8BitAccess);
    size += vn_sizeof_VkBool32(&val->uniformAndStorageBuffer8BitAccess);
    size += vn_sizeof_VkBool32(&val->storagePushConstant8);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDevice8BitStorageFeatures(const VkPhysicalDevice8BitStorageFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDevice8BitStorageFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDevice8BitStorageFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDevice8BitStorageFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDevice8BitStorageFeatures_self(struct vn_cs *cs, const VkPhysicalDevice8BitStorageFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->storageBuffer8BitAccess);
    vn_encode_VkBool32(cs, &val->uniformAndStorageBuffer8BitAccess);
    vn_encode_VkBool32(cs, &val->storagePushConstant8);
}

static inline void
vn_encode_VkPhysicalDevice8BitStorageFeatures(struct vn_cs *cs, const VkPhysicalDevice8BitStorageFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES });
    vn_encode_VkPhysicalDevice8BitStorageFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDevice8BitStorageFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDevice8BitStorageFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDevice8BitStorageFeatures_self(struct vn_cs *cs, VkPhysicalDevice8BitStorageFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->storageBuffer8BitAccess);
    vn_decode_VkBool32(cs, &val->uniformAndStorageBuffer8BitAccess);
    vn_decode_VkBool32(cs, &val->storagePushConstant8);
}

static inline void
vn_decode_VkPhysicalDevice8BitStorageFeatures(struct vn_cs *cs, VkPhysicalDevice8BitStorageFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDevice8BitStorageFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDevice8BitStorageFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDevice8BitStorageFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDevice8BitStorageFeatures_self_partial(const VkPhysicalDevice8BitStorageFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->storageBuffer8BitAccess */
    /* skip val->uniformAndStorageBuffer8BitAccess */
    /* skip val->storagePushConstant8 */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDevice8BitStorageFeatures_partial(const VkPhysicalDevice8BitStorageFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDevice8BitStorageFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDevice8BitStorageFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDevice8BitStorageFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDevice8BitStorageFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDevice8BitStorageFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->storageBuffer8BitAccess */
    /* skip val->uniformAndStorageBuffer8BitAccess */
    /* skip val->storagePushConstant8 */
}

static inline void
vn_encode_VkPhysicalDevice8BitStorageFeatures_partial(struct vn_cs *cs, const VkPhysicalDevice8BitStorageFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES });
    vn_encode_VkPhysicalDevice8BitStorageFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDevice8BitStorageFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceVulkanMemoryModelFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_self(const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->vulkanMemoryModel);
    size += vn_sizeof_VkBool32(&val->vulkanMemoryModelDeviceScope);
    size += vn_sizeof_VkBool32(&val->vulkanMemoryModelAvailabilityVisibilityChains);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures(const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->vulkanMemoryModel);
    vn_encode_VkBool32(cs, &val->vulkanMemoryModelDeviceScope);
    vn_encode_VkBool32(cs, &val->vulkanMemoryModelAvailabilityVisibilityChains);
}

static inline void
vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures(struct vn_cs *cs, const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES });
    vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceVulkanMemoryModelFeatures_self(struct vn_cs *cs, VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->vulkanMemoryModel);
    vn_decode_VkBool32(cs, &val->vulkanMemoryModelDeviceScope);
    vn_decode_VkBool32(cs, &val->vulkanMemoryModelAvailabilityVisibilityChains);
}

static inline void
vn_decode_VkPhysicalDeviceVulkanMemoryModelFeatures(struct vn_cs *cs, VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceVulkanMemoryModelFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_self_partial(const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->vulkanMemoryModel */
    /* skip val->vulkanMemoryModelDeviceScope */
    /* skip val->vulkanMemoryModelAvailabilityVisibilityChains */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_partial(const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->vulkanMemoryModel */
    /* skip val->vulkanMemoryModelDeviceScope */
    /* skip val->vulkanMemoryModelAvailabilityVisibilityChains */
}

static inline void
vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkanMemoryModelFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES });
    vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceShaderAtomicInt64Features chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_self(const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->shaderBufferInt64Atomics);
    size += vn_sizeof_VkBool32(&val->shaderSharedInt64Atomics);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features(const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_self(struct vn_cs *cs, const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->shaderBufferInt64Atomics);
    vn_encode_VkBool32(cs, &val->shaderSharedInt64Atomics);
}

static inline void
vn_encode_VkPhysicalDeviceShaderAtomicInt64Features(struct vn_cs *cs, const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES });
    vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceShaderAtomicInt64Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceShaderAtomicInt64Features_self(struct vn_cs *cs, VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->shaderBufferInt64Atomics);
    vn_decode_VkBool32(cs, &val->shaderSharedInt64Atomics);
}

static inline void
vn_decode_VkPhysicalDeviceShaderAtomicInt64Features(struct vn_cs *cs, VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceShaderAtomicInt64Features_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceShaderAtomicInt64Features_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_self_partial(const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->shaderBufferInt64Atomics */
    /* skip val->shaderSharedInt64Atomics */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_partial(const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_self_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->shaderBufferInt64Atomics */
    /* skip val->shaderSharedInt64Atomics */
}

static inline void
vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_partial(struct vn_cs *cs, const VkPhysicalDeviceShaderAtomicInt64Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES });
    vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_self_partial(cs, val);
}

/* struct VkPhysicalDeviceTransformFeedbackFeaturesEXT chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->transformFeedback);
    size += vn_sizeof_VkBool32(&val->geometryStreams);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT(const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(struct vn_cs *cs, const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->transformFeedback);
    vn_encode_VkBool32(cs, &val->geometryStreams);
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT(struct vn_cs *cs, const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT });
    vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(struct vn_cs *cs, VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->transformFeedback);
    vn_decode_VkBool32(cs, &val->geometryStreams);
}

static inline void
vn_decode_VkPhysicalDeviceTransformFeedbackFeaturesEXT(struct vn_cs *cs, VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self_partial(const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->transformFeedback */
    /* skip val->geometryStreams */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_partial(const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self_partial(struct vn_cs *cs, const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->transformFeedback */
    /* skip val->geometryStreams */
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_partial(struct vn_cs *cs, const VkPhysicalDeviceTransformFeedbackFeaturesEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT });
    vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self_partial(cs, val);
}

/* struct VkPhysicalDeviceScalarBlockLayoutFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_self(const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->scalarBlockLayout);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures(const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->scalarBlockLayout);
}

static inline void
vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures(struct vn_cs *cs, const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES });
    vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceScalarBlockLayoutFeatures_self(struct vn_cs *cs, VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->scalarBlockLayout);
}

static inline void
vn_decode_VkPhysicalDeviceScalarBlockLayoutFeatures(struct vn_cs *cs, VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceScalarBlockLayoutFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_self_partial(const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->scalarBlockLayout */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_partial(const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->scalarBlockLayout */
}

static inline void
vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceScalarBlockLayoutFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES });
    vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceUniformBufferStandardLayoutFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->uniformBufferStandardLayout);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures(const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->uniformBufferStandardLayout);
}

static inline void
vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures(struct vn_cs *cs, const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES });
    vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(struct vn_cs *cs, VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->uniformBufferStandardLayout);
}

static inline void
vn_decode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures(struct vn_cs *cs, VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self_partial(const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->uniformBufferStandardLayout */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_partial(const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->uniformBufferStandardLayout */
}

static inline void
vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES });
    vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceBufferDeviceAddressFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_self(const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->bufferDeviceAddress);
    size += vn_sizeof_VkBool32(&val->bufferDeviceAddressCaptureReplay);
    size += vn_sizeof_VkBool32(&val->bufferDeviceAddressMultiDevice);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures(const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->bufferDeviceAddress);
    vn_encode_VkBool32(cs, &val->bufferDeviceAddressCaptureReplay);
    vn_encode_VkBool32(cs, &val->bufferDeviceAddressMultiDevice);
}

static inline void
vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures(struct vn_cs *cs, const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES });
    vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceBufferDeviceAddressFeatures_self(struct vn_cs *cs, VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->bufferDeviceAddress);
    vn_decode_VkBool32(cs, &val->bufferDeviceAddressCaptureReplay);
    vn_decode_VkBool32(cs, &val->bufferDeviceAddressMultiDevice);
}

static inline void
vn_decode_VkPhysicalDeviceBufferDeviceAddressFeatures(struct vn_cs *cs, VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceBufferDeviceAddressFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_self_partial(const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->bufferDeviceAddress */
    /* skip val->bufferDeviceAddressCaptureReplay */
    /* skip val->bufferDeviceAddressMultiDevice */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_partial(const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->bufferDeviceAddress */
    /* skip val->bufferDeviceAddressCaptureReplay */
    /* skip val->bufferDeviceAddressMultiDevice */
}

static inline void
vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceBufferDeviceAddressFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES });
    vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceImagelessFramebufferFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_self(const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->imagelessFramebuffer);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures(const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->imagelessFramebuffer);
}

static inline void
vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures(struct vn_cs *cs, const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES });
    vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceImagelessFramebufferFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceImagelessFramebufferFeatures_self(struct vn_cs *cs, VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->imagelessFramebuffer);
}

static inline void
vn_decode_VkPhysicalDeviceImagelessFramebufferFeatures(struct vn_cs *cs, VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceImagelessFramebufferFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceImagelessFramebufferFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_self_partial(const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->imagelessFramebuffer */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_partial(const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->imagelessFramebuffer */
}

static inline void
vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceImagelessFramebufferFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES });
    vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->separateDepthStencilLayouts);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures(const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(struct vn_cs *cs, const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->separateDepthStencilLayouts);
}

static inline void
vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures(struct vn_cs *cs, const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES });
    vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(struct vn_cs *cs, VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->separateDepthStencilLayouts);
}

static inline void
vn_decode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures(struct vn_cs *cs, VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self_partial(const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->separateDepthStencilLayouts */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_partial(const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self_partial(struct vn_cs *cs, const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->separateDepthStencilLayouts */
}

static inline void
vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_partial(struct vn_cs *cs, const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES });
    vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self_partial(cs, val);
}

/* struct VkPhysicalDeviceVulkan11Features chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Features_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Features_self(const VkPhysicalDeviceVulkan11Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->storageBuffer16BitAccess);
    size += vn_sizeof_VkBool32(&val->uniformAndStorageBuffer16BitAccess);
    size += vn_sizeof_VkBool32(&val->storagePushConstant16);
    size += vn_sizeof_VkBool32(&val->storageInputOutput16);
    size += vn_sizeof_VkBool32(&val->multiview);
    size += vn_sizeof_VkBool32(&val->multiviewGeometryShader);
    size += vn_sizeof_VkBool32(&val->multiviewTessellationShader);
    size += vn_sizeof_VkBool32(&val->variablePointersStorageBuffer);
    size += vn_sizeof_VkBool32(&val->variablePointers);
    size += vn_sizeof_VkBool32(&val->protectedMemory);
    size += vn_sizeof_VkBool32(&val->samplerYcbcrConversion);
    size += vn_sizeof_VkBool32(&val->shaderDrawParameters);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Features(const VkPhysicalDeviceVulkan11Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Features_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Features_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Features_self(struct vn_cs *cs, const VkPhysicalDeviceVulkan11Features *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->storageBuffer16BitAccess);
    vn_encode_VkBool32(cs, &val->uniformAndStorageBuffer16BitAccess);
    vn_encode_VkBool32(cs, &val->storagePushConstant16);
    vn_encode_VkBool32(cs, &val->storageInputOutput16);
    vn_encode_VkBool32(cs, &val->multiview);
    vn_encode_VkBool32(cs, &val->multiviewGeometryShader);
    vn_encode_VkBool32(cs, &val->multiviewTessellationShader);
    vn_encode_VkBool32(cs, &val->variablePointersStorageBuffer);
    vn_encode_VkBool32(cs, &val->variablePointers);
    vn_encode_VkBool32(cs, &val->protectedMemory);
    vn_encode_VkBool32(cs, &val->samplerYcbcrConversion);
    vn_encode_VkBool32(cs, &val->shaderDrawParameters);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Features(struct vn_cs *cs, const VkPhysicalDeviceVulkan11Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES });
    vn_encode_VkPhysicalDeviceVulkan11Features_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkan11Features_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan11Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan11Features_self(struct vn_cs *cs, VkPhysicalDeviceVulkan11Features *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->storageBuffer16BitAccess);
    vn_decode_VkBool32(cs, &val->uniformAndStorageBuffer16BitAccess);
    vn_decode_VkBool32(cs, &val->storagePushConstant16);
    vn_decode_VkBool32(cs, &val->storageInputOutput16);
    vn_decode_VkBool32(cs, &val->multiview);
    vn_decode_VkBool32(cs, &val->multiviewGeometryShader);
    vn_decode_VkBool32(cs, &val->multiviewTessellationShader);
    vn_decode_VkBool32(cs, &val->variablePointersStorageBuffer);
    vn_decode_VkBool32(cs, &val->variablePointers);
    vn_decode_VkBool32(cs, &val->protectedMemory);
    vn_decode_VkBool32(cs, &val->samplerYcbcrConversion);
    vn_decode_VkBool32(cs, &val->shaderDrawParameters);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan11Features(struct vn_cs *cs, VkPhysicalDeviceVulkan11Features *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceVulkan11Features_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceVulkan11Features_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Features_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Features_self_partial(const VkPhysicalDeviceVulkan11Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->storageBuffer16BitAccess */
    /* skip val->uniformAndStorageBuffer16BitAccess */
    /* skip val->storagePushConstant16 */
    /* skip val->storageInputOutput16 */
    /* skip val->multiview */
    /* skip val->multiviewGeometryShader */
    /* skip val->multiviewTessellationShader */
    /* skip val->variablePointersStorageBuffer */
    /* skip val->variablePointers */
    /* skip val->protectedMemory */
    /* skip val->samplerYcbcrConversion */
    /* skip val->shaderDrawParameters */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Features_partial(const VkPhysicalDeviceVulkan11Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Features_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Features_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Features_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Features_self_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan11Features *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->storageBuffer16BitAccess */
    /* skip val->uniformAndStorageBuffer16BitAccess */
    /* skip val->storagePushConstant16 */
    /* skip val->storageInputOutput16 */
    /* skip val->multiview */
    /* skip val->multiviewGeometryShader */
    /* skip val->multiviewTessellationShader */
    /* skip val->variablePointersStorageBuffer */
    /* skip val->variablePointers */
    /* skip val->protectedMemory */
    /* skip val->samplerYcbcrConversion */
    /* skip val->shaderDrawParameters */
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Features_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan11Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES });
    vn_encode_VkPhysicalDeviceVulkan11Features_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkan11Features_self_partial(cs, val);
}

/* struct VkPhysicalDeviceVulkan12Features chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Features_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Features_self(const VkPhysicalDeviceVulkan12Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->samplerMirrorClampToEdge);
    size += vn_sizeof_VkBool32(&val->drawIndirectCount);
    size += vn_sizeof_VkBool32(&val->storageBuffer8BitAccess);
    size += vn_sizeof_VkBool32(&val->uniformAndStorageBuffer8BitAccess);
    size += vn_sizeof_VkBool32(&val->storagePushConstant8);
    size += vn_sizeof_VkBool32(&val->shaderBufferInt64Atomics);
    size += vn_sizeof_VkBool32(&val->shaderSharedInt64Atomics);
    size += vn_sizeof_VkBool32(&val->shaderFloat16);
    size += vn_sizeof_VkBool32(&val->shaderInt8);
    size += vn_sizeof_VkBool32(&val->descriptorIndexing);
    size += vn_sizeof_VkBool32(&val->shaderInputAttachmentArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderUniformTexelBufferArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageTexelBufferArrayDynamicIndexing);
    size += vn_sizeof_VkBool32(&val->shaderUniformBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderSampledImageArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderInputAttachmentArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderUniformTexelBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->shaderStorageTexelBufferArrayNonUniformIndexing);
    size += vn_sizeof_VkBool32(&val->descriptorBindingUniformBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingSampledImageUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingStorageImageUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingStorageBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingUniformTexelBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingStorageTexelBufferUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->descriptorBindingUpdateUnusedWhilePending);
    size += vn_sizeof_VkBool32(&val->descriptorBindingPartiallyBound);
    size += vn_sizeof_VkBool32(&val->descriptorBindingVariableDescriptorCount);
    size += vn_sizeof_VkBool32(&val->runtimeDescriptorArray);
    size += vn_sizeof_VkBool32(&val->samplerFilterMinmax);
    size += vn_sizeof_VkBool32(&val->scalarBlockLayout);
    size += vn_sizeof_VkBool32(&val->imagelessFramebuffer);
    size += vn_sizeof_VkBool32(&val->uniformBufferStandardLayout);
    size += vn_sizeof_VkBool32(&val->shaderSubgroupExtendedTypes);
    size += vn_sizeof_VkBool32(&val->separateDepthStencilLayouts);
    size += vn_sizeof_VkBool32(&val->hostQueryReset);
    size += vn_sizeof_VkBool32(&val->timelineSemaphore);
    size += vn_sizeof_VkBool32(&val->bufferDeviceAddress);
    size += vn_sizeof_VkBool32(&val->bufferDeviceAddressCaptureReplay);
    size += vn_sizeof_VkBool32(&val->bufferDeviceAddressMultiDevice);
    size += vn_sizeof_VkBool32(&val->vulkanMemoryModel);
    size += vn_sizeof_VkBool32(&val->vulkanMemoryModelDeviceScope);
    size += vn_sizeof_VkBool32(&val->vulkanMemoryModelAvailabilityVisibilityChains);
    size += vn_sizeof_VkBool32(&val->shaderOutputViewportIndex);
    size += vn_sizeof_VkBool32(&val->shaderOutputLayer);
    size += vn_sizeof_VkBool32(&val->subgroupBroadcastDynamicId);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Features(const VkPhysicalDeviceVulkan12Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Features_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Features_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Features_self(struct vn_cs *cs, const VkPhysicalDeviceVulkan12Features *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->samplerMirrorClampToEdge);
    vn_encode_VkBool32(cs, &val->drawIndirectCount);
    vn_encode_VkBool32(cs, &val->storageBuffer8BitAccess);
    vn_encode_VkBool32(cs, &val->uniformAndStorageBuffer8BitAccess);
    vn_encode_VkBool32(cs, &val->storagePushConstant8);
    vn_encode_VkBool32(cs, &val->shaderBufferInt64Atomics);
    vn_encode_VkBool32(cs, &val->shaderSharedInt64Atomics);
    vn_encode_VkBool32(cs, &val->shaderFloat16);
    vn_encode_VkBool32(cs, &val->shaderInt8);
    vn_encode_VkBool32(cs, &val->descriptorIndexing);
    vn_encode_VkBool32(cs, &val->shaderInputAttachmentArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderUniformTexelBufferArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageTexelBufferArrayDynamicIndexing);
    vn_encode_VkBool32(cs, &val->shaderUniformBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderSampledImageArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageImageArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderInputAttachmentArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderUniformTexelBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->shaderStorageTexelBufferArrayNonUniformIndexing);
    vn_encode_VkBool32(cs, &val->descriptorBindingUniformBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingSampledImageUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingStorageImageUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingStorageBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingUniformTexelBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingStorageTexelBufferUpdateAfterBind);
    vn_encode_VkBool32(cs, &val->descriptorBindingUpdateUnusedWhilePending);
    vn_encode_VkBool32(cs, &val->descriptorBindingPartiallyBound);
    vn_encode_VkBool32(cs, &val->descriptorBindingVariableDescriptorCount);
    vn_encode_VkBool32(cs, &val->runtimeDescriptorArray);
    vn_encode_VkBool32(cs, &val->samplerFilterMinmax);
    vn_encode_VkBool32(cs, &val->scalarBlockLayout);
    vn_encode_VkBool32(cs, &val->imagelessFramebuffer);
    vn_encode_VkBool32(cs, &val->uniformBufferStandardLayout);
    vn_encode_VkBool32(cs, &val->shaderSubgroupExtendedTypes);
    vn_encode_VkBool32(cs, &val->separateDepthStencilLayouts);
    vn_encode_VkBool32(cs, &val->hostQueryReset);
    vn_encode_VkBool32(cs, &val->timelineSemaphore);
    vn_encode_VkBool32(cs, &val->bufferDeviceAddress);
    vn_encode_VkBool32(cs, &val->bufferDeviceAddressCaptureReplay);
    vn_encode_VkBool32(cs, &val->bufferDeviceAddressMultiDevice);
    vn_encode_VkBool32(cs, &val->vulkanMemoryModel);
    vn_encode_VkBool32(cs, &val->vulkanMemoryModelDeviceScope);
    vn_encode_VkBool32(cs, &val->vulkanMemoryModelAvailabilityVisibilityChains);
    vn_encode_VkBool32(cs, &val->shaderOutputViewportIndex);
    vn_encode_VkBool32(cs, &val->shaderOutputLayer);
    vn_encode_VkBool32(cs, &val->subgroupBroadcastDynamicId);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Features(struct vn_cs *cs, const VkPhysicalDeviceVulkan12Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES });
    vn_encode_VkPhysicalDeviceVulkan12Features_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkan12Features_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan12Features_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan12Features_self(struct vn_cs *cs, VkPhysicalDeviceVulkan12Features *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->samplerMirrorClampToEdge);
    vn_decode_VkBool32(cs, &val->drawIndirectCount);
    vn_decode_VkBool32(cs, &val->storageBuffer8BitAccess);
    vn_decode_VkBool32(cs, &val->uniformAndStorageBuffer8BitAccess);
    vn_decode_VkBool32(cs, &val->storagePushConstant8);
    vn_decode_VkBool32(cs, &val->shaderBufferInt64Atomics);
    vn_decode_VkBool32(cs, &val->shaderSharedInt64Atomics);
    vn_decode_VkBool32(cs, &val->shaderFloat16);
    vn_decode_VkBool32(cs, &val->shaderInt8);
    vn_decode_VkBool32(cs, &val->descriptorIndexing);
    vn_decode_VkBool32(cs, &val->shaderInputAttachmentArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderUniformTexelBufferArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageTexelBufferArrayDynamicIndexing);
    vn_decode_VkBool32(cs, &val->shaderUniformBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderSampledImageArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageImageArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderInputAttachmentArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderUniformTexelBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->shaderStorageTexelBufferArrayNonUniformIndexing);
    vn_decode_VkBool32(cs, &val->descriptorBindingUniformBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingSampledImageUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingStorageImageUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingStorageBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingUniformTexelBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingStorageTexelBufferUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->descriptorBindingUpdateUnusedWhilePending);
    vn_decode_VkBool32(cs, &val->descriptorBindingPartiallyBound);
    vn_decode_VkBool32(cs, &val->descriptorBindingVariableDescriptorCount);
    vn_decode_VkBool32(cs, &val->runtimeDescriptorArray);
    vn_decode_VkBool32(cs, &val->samplerFilterMinmax);
    vn_decode_VkBool32(cs, &val->scalarBlockLayout);
    vn_decode_VkBool32(cs, &val->imagelessFramebuffer);
    vn_decode_VkBool32(cs, &val->uniformBufferStandardLayout);
    vn_decode_VkBool32(cs, &val->shaderSubgroupExtendedTypes);
    vn_decode_VkBool32(cs, &val->separateDepthStencilLayouts);
    vn_decode_VkBool32(cs, &val->hostQueryReset);
    vn_decode_VkBool32(cs, &val->timelineSemaphore);
    vn_decode_VkBool32(cs, &val->bufferDeviceAddress);
    vn_decode_VkBool32(cs, &val->bufferDeviceAddressCaptureReplay);
    vn_decode_VkBool32(cs, &val->bufferDeviceAddressMultiDevice);
    vn_decode_VkBool32(cs, &val->vulkanMemoryModel);
    vn_decode_VkBool32(cs, &val->vulkanMemoryModelDeviceScope);
    vn_decode_VkBool32(cs, &val->vulkanMemoryModelAvailabilityVisibilityChains);
    vn_decode_VkBool32(cs, &val->shaderOutputViewportIndex);
    vn_decode_VkBool32(cs, &val->shaderOutputLayer);
    vn_decode_VkBool32(cs, &val->subgroupBroadcastDynamicId);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan12Features(struct vn_cs *cs, VkPhysicalDeviceVulkan12Features *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceVulkan12Features_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceVulkan12Features_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Features_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Features_self_partial(const VkPhysicalDeviceVulkan12Features *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->samplerMirrorClampToEdge */
    /* skip val->drawIndirectCount */
    /* skip val->storageBuffer8BitAccess */
    /* skip val->uniformAndStorageBuffer8BitAccess */
    /* skip val->storagePushConstant8 */
    /* skip val->shaderBufferInt64Atomics */
    /* skip val->shaderSharedInt64Atomics */
    /* skip val->shaderFloat16 */
    /* skip val->shaderInt8 */
    /* skip val->descriptorIndexing */
    /* skip val->shaderInputAttachmentArrayDynamicIndexing */
    /* skip val->shaderUniformTexelBufferArrayDynamicIndexing */
    /* skip val->shaderStorageTexelBufferArrayDynamicIndexing */
    /* skip val->shaderUniformBufferArrayNonUniformIndexing */
    /* skip val->shaderSampledImageArrayNonUniformIndexing */
    /* skip val->shaderStorageBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageImageArrayNonUniformIndexing */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexing */
    /* skip val->shaderUniformTexelBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageTexelBufferArrayNonUniformIndexing */
    /* skip val->descriptorBindingUniformBufferUpdateAfterBind */
    /* skip val->descriptorBindingSampledImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageBufferUpdateAfterBind */
    /* skip val->descriptorBindingUniformTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingStorageTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingUpdateUnusedWhilePending */
    /* skip val->descriptorBindingPartiallyBound */
    /* skip val->descriptorBindingVariableDescriptorCount */
    /* skip val->runtimeDescriptorArray */
    /* skip val->samplerFilterMinmax */
    /* skip val->scalarBlockLayout */
    /* skip val->imagelessFramebuffer */
    /* skip val->uniformBufferStandardLayout */
    /* skip val->shaderSubgroupExtendedTypes */
    /* skip val->separateDepthStencilLayouts */
    /* skip val->hostQueryReset */
    /* skip val->timelineSemaphore */
    /* skip val->bufferDeviceAddress */
    /* skip val->bufferDeviceAddressCaptureReplay */
    /* skip val->bufferDeviceAddressMultiDevice */
    /* skip val->vulkanMemoryModel */
    /* skip val->vulkanMemoryModelDeviceScope */
    /* skip val->vulkanMemoryModelAvailabilityVisibilityChains */
    /* skip val->shaderOutputViewportIndex */
    /* skip val->shaderOutputLayer */
    /* skip val->subgroupBroadcastDynamicId */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Features_partial(const VkPhysicalDeviceVulkan12Features *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Features_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Features_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Features_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Features_self_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan12Features *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->samplerMirrorClampToEdge */
    /* skip val->drawIndirectCount */
    /* skip val->storageBuffer8BitAccess */
    /* skip val->uniformAndStorageBuffer8BitAccess */
    /* skip val->storagePushConstant8 */
    /* skip val->shaderBufferInt64Atomics */
    /* skip val->shaderSharedInt64Atomics */
    /* skip val->shaderFloat16 */
    /* skip val->shaderInt8 */
    /* skip val->descriptorIndexing */
    /* skip val->shaderInputAttachmentArrayDynamicIndexing */
    /* skip val->shaderUniformTexelBufferArrayDynamicIndexing */
    /* skip val->shaderStorageTexelBufferArrayDynamicIndexing */
    /* skip val->shaderUniformBufferArrayNonUniformIndexing */
    /* skip val->shaderSampledImageArrayNonUniformIndexing */
    /* skip val->shaderStorageBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageImageArrayNonUniformIndexing */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexing */
    /* skip val->shaderUniformTexelBufferArrayNonUniformIndexing */
    /* skip val->shaderStorageTexelBufferArrayNonUniformIndexing */
    /* skip val->descriptorBindingUniformBufferUpdateAfterBind */
    /* skip val->descriptorBindingSampledImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageImageUpdateAfterBind */
    /* skip val->descriptorBindingStorageBufferUpdateAfterBind */
    /* skip val->descriptorBindingUniformTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingStorageTexelBufferUpdateAfterBind */
    /* skip val->descriptorBindingUpdateUnusedWhilePending */
    /* skip val->descriptorBindingPartiallyBound */
    /* skip val->descriptorBindingVariableDescriptorCount */
    /* skip val->runtimeDescriptorArray */
    /* skip val->samplerFilterMinmax */
    /* skip val->scalarBlockLayout */
    /* skip val->imagelessFramebuffer */
    /* skip val->uniformBufferStandardLayout */
    /* skip val->shaderSubgroupExtendedTypes */
    /* skip val->separateDepthStencilLayouts */
    /* skip val->hostQueryReset */
    /* skip val->timelineSemaphore */
    /* skip val->bufferDeviceAddress */
    /* skip val->bufferDeviceAddressCaptureReplay */
    /* skip val->bufferDeviceAddressMultiDevice */
    /* skip val->vulkanMemoryModel */
    /* skip val->vulkanMemoryModelDeviceScope */
    /* skip val->vulkanMemoryModelAvailabilityVisibilityChains */
    /* skip val->shaderOutputViewportIndex */
    /* skip val->shaderOutputLayer */
    /* skip val->subgroupBroadcastDynamicId */
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Features_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan12Features *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES });
    vn_encode_VkPhysicalDeviceVulkan12Features_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkan12Features_self_partial(cs, val);
}

/* struct VkPhysicalDeviceFeatures2 chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_self((const VkPhysicalDeviceVariablePointersFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceMultiviewFeatures_self((const VkPhysicalDeviceMultiviewFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevice16BitStorageFeatures_self((const VkPhysicalDevice16BitStorageFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self((const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self((const VkPhysicalDeviceSamplerYcbcrConversionFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_self((const VkPhysicalDeviceProtectedMemoryFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_self((const VkPhysicalDeviceShaderDrawParametersFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_self((const VkPhysicalDeviceShaderFloat16Int8Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_self((const VkPhysicalDeviceHostQueryResetFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_self((const VkPhysicalDeviceDescriptorIndexingFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_self((const VkPhysicalDeviceTimelineSemaphoreFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevice8BitStorageFeatures_self((const VkPhysicalDevice8BitStorageFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_self((const VkPhysicalDeviceVulkanMemoryModelFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_self((const VkPhysicalDeviceShaderAtomicInt64Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self((const VkPhysicalDeviceTransformFeedbackFeaturesEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_self((const VkPhysicalDeviceScalarBlockLayoutFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self((const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_self((const VkPhysicalDeviceBufferDeviceAddressFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_self((const VkPhysicalDeviceImagelessFramebufferFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self((const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan11Features_self((const VkPhysicalDeviceVulkan11Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan12Features_self((const VkPhysicalDeviceVulkan12Features *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures2_self(const VkPhysicalDeviceFeatures2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkPhysicalDeviceFeatures(&val->features);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures2(const VkPhysicalDeviceFeatures2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceFeatures2_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceFeatures2_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVariablePointersFeatures_self(cs, (const VkPhysicalDeviceVariablePointersFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceMultiviewFeatures_self(cs, (const VkPhysicalDeviceMultiviewFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDevice16BitStorageFeatures_self(cs, (const VkPhysicalDevice16BitStorageFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(cs, (const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(cs, (const VkPhysicalDeviceSamplerYcbcrConversionFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_self(cs, (const VkPhysicalDeviceProtectedMemoryFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_self(cs, (const VkPhysicalDeviceShaderDrawParametersFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_self(cs, (const VkPhysicalDeviceShaderFloat16Int8Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceHostQueryResetFeatures_self(cs, (const VkPhysicalDeviceHostQueryResetFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_self(cs, (const VkPhysicalDeviceDescriptorIndexingFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_self(cs, (const VkPhysicalDeviceTimelineSemaphoreFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDevice8BitStorageFeatures_self(cs, (const VkPhysicalDevice8BitStorageFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_self(cs, (const VkPhysicalDeviceVulkanMemoryModelFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_self(cs, (const VkPhysicalDeviceShaderAtomicInt64Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(cs, (const VkPhysicalDeviceTransformFeedbackFeaturesEXT *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_self(cs, (const VkPhysicalDeviceScalarBlockLayoutFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(cs, (const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_self(cs, (const VkPhysicalDeviceBufferDeviceAddressFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_self(cs, (const VkPhysicalDeviceImagelessFramebufferFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(cs, (const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan11Features_self(cs, (const VkPhysicalDeviceVulkan11Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan12Features_self(cs, (const VkPhysicalDeviceVulkan12Features *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceFeatures2_self(struct vn_cs *cs, const VkPhysicalDeviceFeatures2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkPhysicalDeviceFeatures(cs, &val->features);
}

static inline void
vn_encode_VkPhysicalDeviceFeatures2(struct vn_cs *cs, const VkPhysicalDeviceFeatures2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 });
    vn_encode_VkPhysicalDeviceFeatures2_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceFeatures2_self(cs, val);
}

static inline void
vn_decode_VkPhysicalDeviceFeatures2_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceVariablePointersFeatures_self(cs, (VkPhysicalDeviceVariablePointersFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceMultiviewFeatures_self(cs, (VkPhysicalDeviceMultiviewFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDevice16BitStorageFeatures_self(cs, (VkPhysicalDevice16BitStorageFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(cs, (VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(cs, (VkPhysicalDeviceSamplerYcbcrConversionFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceProtectedMemoryFeatures_self(cs, (VkPhysicalDeviceProtectedMemoryFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceShaderDrawParametersFeatures_self(cs, (VkPhysicalDeviceShaderDrawParametersFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceShaderFloat16Int8Features_self(cs, (VkPhysicalDeviceShaderFloat16Int8Features *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceHostQueryResetFeatures_self(cs, (VkPhysicalDeviceHostQueryResetFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceDescriptorIndexingFeatures_self(cs, (VkPhysicalDeviceDescriptorIndexingFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceTimelineSemaphoreFeatures_self(cs, (VkPhysicalDeviceTimelineSemaphoreFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDevice8BitStorageFeatures_self(cs, (VkPhysicalDevice8BitStorageFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceVulkanMemoryModelFeatures_self(cs, (VkPhysicalDeviceVulkanMemoryModelFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceShaderAtomicInt64Features_self(cs, (VkPhysicalDeviceShaderAtomicInt64Features *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(cs, (VkPhysicalDeviceTransformFeedbackFeaturesEXT *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceScalarBlockLayoutFeatures_self(cs, (VkPhysicalDeviceScalarBlockLayoutFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(cs, (VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceBufferDeviceAddressFeatures_self(cs, (VkPhysicalDeviceBufferDeviceAddressFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceImagelessFramebufferFeatures_self(cs, (VkPhysicalDeviceImagelessFramebufferFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(cs, (VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceVulkan11Features_self(cs, (VkPhysicalDeviceVulkan11Features *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
        vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceVulkan12Features_self(cs, (VkPhysicalDeviceVulkan12Features *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkPhysicalDeviceFeatures2_self(struct vn_cs *cs, VkPhysicalDeviceFeatures2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkPhysicalDeviceFeatures(cs, &val->features);
}

static inline void
vn_decode_VkPhysicalDeviceFeatures2(struct vn_cs *cs, VkPhysicalDeviceFeatures2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceFeatures2_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceFeatures2_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_self_partial((const VkPhysicalDeviceVariablePointersFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceMultiviewFeatures_self_partial((const VkPhysicalDeviceMultiviewFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevice16BitStorageFeatures_self_partial((const VkPhysicalDevice16BitStorageFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self_partial((const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self_partial((const VkPhysicalDeviceSamplerYcbcrConversionFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_self_partial((const VkPhysicalDeviceProtectedMemoryFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_self_partial((const VkPhysicalDeviceShaderDrawParametersFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_self_partial((const VkPhysicalDeviceShaderFloat16Int8Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_self_partial((const VkPhysicalDeviceHostQueryResetFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_self_partial((const VkPhysicalDeviceDescriptorIndexingFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_self_partial((const VkPhysicalDeviceTimelineSemaphoreFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevice8BitStorageFeatures_self_partial((const VkPhysicalDevice8BitStorageFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_self_partial((const VkPhysicalDeviceVulkanMemoryModelFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_self_partial((const VkPhysicalDeviceShaderAtomicInt64Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self_partial((const VkPhysicalDeviceTransformFeedbackFeaturesEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_self_partial((const VkPhysicalDeviceScalarBlockLayoutFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self_partial((const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_self_partial((const VkPhysicalDeviceBufferDeviceAddressFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_self_partial((const VkPhysicalDeviceImagelessFramebufferFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self_partial((const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan11Features_self_partial((const VkPhysicalDeviceVulkan11Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan12Features_self_partial((const VkPhysicalDeviceVulkan12Features *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures2_self_partial(const VkPhysicalDeviceFeatures2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkPhysicalDeviceFeatures_partial(&val->features);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFeatures2_partial(const VkPhysicalDeviceFeatures2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceFeatures2_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceFeatures2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVariablePointersFeatures_self_partial(cs, (const VkPhysicalDeviceVariablePointersFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceMultiviewFeatures_self_partial(cs, (const VkPhysicalDeviceMultiviewFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDevice16BitStorageFeatures_self_partial(cs, (const VkPhysicalDevice16BitStorageFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self_partial(cs, (const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self_partial(cs, (const VkPhysicalDeviceSamplerYcbcrConversionFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_self_partial(cs, (const VkPhysicalDeviceProtectedMemoryFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_self_partial(cs, (const VkPhysicalDeviceShaderDrawParametersFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_self_partial(cs, (const VkPhysicalDeviceShaderFloat16Int8Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceHostQueryResetFeatures_self_partial(cs, (const VkPhysicalDeviceHostQueryResetFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_self_partial(cs, (const VkPhysicalDeviceDescriptorIndexingFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_self_partial(cs, (const VkPhysicalDeviceTimelineSemaphoreFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDevice8BitStorageFeatures_self_partial(cs, (const VkPhysicalDevice8BitStorageFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_self_partial(cs, (const VkPhysicalDeviceVulkanMemoryModelFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_self_partial(cs, (const VkPhysicalDeviceShaderAtomicInt64Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self_partial(cs, (const VkPhysicalDeviceTransformFeedbackFeaturesEXT *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_self_partial(cs, (const VkPhysicalDeviceScalarBlockLayoutFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self_partial(cs, (const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_self_partial(cs, (const VkPhysicalDeviceBufferDeviceAddressFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_self_partial(cs, (const VkPhysicalDeviceImagelessFramebufferFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self_partial(cs, (const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan11Features_self_partial(cs, (const VkPhysicalDeviceVulkan11Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan12Features_self_partial(cs, (const VkPhysicalDeviceVulkan12Features *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceFeatures2_self_partial(struct vn_cs *cs, const VkPhysicalDeviceFeatures2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkPhysicalDeviceFeatures_partial(cs, &val->features);
}

static inline void
vn_encode_VkPhysicalDeviceFeatures2_partial(struct vn_cs *cs, const VkPhysicalDeviceFeatures2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 });
    vn_encode_VkPhysicalDeviceFeatures2_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceFeatures2_self_partial(cs, val);
}

/* struct VkDeviceGroupDeviceCreateInfo chain */

static inline size_t
vn_sizeof_VkDeviceGroupDeviceCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceGroupDeviceCreateInfo_self(const VkDeviceGroupDeviceCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->physicalDeviceCount);
    if (val->pPhysicalDevices) {
        size += vn_sizeof_array_size(val->physicalDeviceCount);
        for (uint32_t i = 0; i < val->physicalDeviceCount; i++)
            size += vn_sizeof_VkPhysicalDevice(&val->pPhysicalDevices[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDeviceGroupDeviceCreateInfo(const VkDeviceGroupDeviceCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceGroupDeviceCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceGroupDeviceCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceGroupDeviceCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceGroupDeviceCreateInfo_self(struct vn_cs *cs, const VkDeviceGroupDeviceCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->physicalDeviceCount);
    if (val->pPhysicalDevices) {
        vn_encode_array_size(cs, val->physicalDeviceCount);
        for (uint32_t i = 0; i < val->physicalDeviceCount; i++)
            vn_encode_VkPhysicalDevice(cs, &val->pPhysicalDevices[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDeviceGroupDeviceCreateInfo(struct vn_cs *cs, const VkDeviceGroupDeviceCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO });
    vn_encode_VkDeviceGroupDeviceCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceGroupDeviceCreateInfo_self(cs, val);
}

/* struct VkDeviceCreateInfo chain */

static inline size_t
vn_sizeof_VkDeviceCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceFeatures2_self((const VkPhysicalDeviceFeatures2 *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVariablePointersFeatures_self((const VkPhysicalDeviceVariablePointersFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceMultiviewFeatures_self((const VkPhysicalDeviceMultiviewFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkDeviceGroupDeviceCreateInfo_self((const VkDeviceGroupDeviceCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevice16BitStorageFeatures_self((const VkPhysicalDevice16BitStorageFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self((const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self((const VkPhysicalDeviceSamplerYcbcrConversionFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceProtectedMemoryFeatures_self((const VkPhysicalDeviceProtectedMemoryFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderDrawParametersFeatures_self((const VkPhysicalDeviceShaderDrawParametersFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderFloat16Int8Features_self((const VkPhysicalDeviceShaderFloat16Int8Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceHostQueryResetFeatures_self((const VkPhysicalDeviceHostQueryResetFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingFeatures_self((const VkPhysicalDeviceDescriptorIndexingFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreFeatures_self((const VkPhysicalDeviceTimelineSemaphoreFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevice8BitStorageFeatures_self((const VkPhysicalDevice8BitStorageFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkanMemoryModelFeatures_self((const VkPhysicalDeviceVulkanMemoryModelFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceShaderAtomicInt64Features_self((const VkPhysicalDeviceShaderAtomicInt64Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self((const VkPhysicalDeviceTransformFeedbackFeaturesEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceScalarBlockLayoutFeatures_self((const VkPhysicalDeviceScalarBlockLayoutFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self((const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceBufferDeviceAddressFeatures_self((const VkPhysicalDeviceBufferDeviceAddressFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceImagelessFramebufferFeatures_self((const VkPhysicalDeviceImagelessFramebufferFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self((const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan11Features_self((const VkPhysicalDeviceVulkan11Features *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDeviceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan12Features_self((const VkPhysicalDeviceVulkan12Features *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceCreateInfo_self(const VkDeviceCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->queueCreateInfoCount);
    if (val->pQueueCreateInfos) {
        size += vn_sizeof_array_size(val->queueCreateInfoCount);
        for (uint32_t i = 0; i < val->queueCreateInfoCount; i++)
            size += vn_sizeof_VkDeviceQueueCreateInfo(&val->pQueueCreateInfos[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->enabledLayerCount);
    if (val->ppEnabledLayerNames) {
        size += vn_sizeof_array_size(val->enabledLayerCount);
        for (uint32_t i = 0; i < val->enabledLayerCount; i++) {
            const size_t string_size = strlen(val->ppEnabledLayerNames[i]) + 1;
            size += vn_sizeof_array_size(string_size);
            size += vn_sizeof_blob_array(val->ppEnabledLayerNames[i], string_size);
        }
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->enabledExtensionCount);
    if (val->ppEnabledExtensionNames) {
        size += vn_sizeof_array_size(val->enabledExtensionCount);
        for (uint32_t i = 0; i < val->enabledExtensionCount; i++) {
            const size_t string_size = strlen(val->ppEnabledExtensionNames[i]) + 1;
            size += vn_sizeof_array_size(string_size);
            size += vn_sizeof_blob_array(val->ppEnabledExtensionNames[i], string_size);
        }
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_simple_pointer(val->pEnabledFeatures);
    if (val->pEnabledFeatures)
        size += vn_sizeof_VkPhysicalDeviceFeatures(val->pEnabledFeatures);
    return size;
}

static inline size_t
vn_sizeof_VkDeviceCreateInfo(const VkDeviceCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceFeatures2_self(cs, (const VkPhysicalDeviceFeatures2 *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVariablePointersFeatures_self(cs, (const VkPhysicalDeviceVariablePointersFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceMultiviewFeatures_self(cs, (const VkPhysicalDeviceMultiviewFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkDeviceGroupDeviceCreateInfo_self(cs, (const VkDeviceGroupDeviceCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDevice16BitStorageFeatures_self(cs, (const VkPhysicalDevice16BitStorageFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures_self(cs, (const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSamplerYcbcrConversionFeatures_self(cs, (const VkPhysicalDeviceSamplerYcbcrConversionFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceProtectedMemoryFeatures_self(cs, (const VkPhysicalDeviceProtectedMemoryFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderDrawParametersFeatures_self(cs, (const VkPhysicalDeviceShaderDrawParametersFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderFloat16Int8Features_self(cs, (const VkPhysicalDeviceShaderFloat16Int8Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceHostQueryResetFeatures_self(cs, (const VkPhysicalDeviceHostQueryResetFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceDescriptorIndexingFeatures_self(cs, (const VkPhysicalDeviceDescriptorIndexingFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTimelineSemaphoreFeatures_self(cs, (const VkPhysicalDeviceTimelineSemaphoreFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDevice8BitStorageFeatures_self(cs, (const VkPhysicalDevice8BitStorageFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkanMemoryModelFeatures_self(cs, (const VkPhysicalDeviceVulkanMemoryModelFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceShaderAtomicInt64Features_self(cs, (const VkPhysicalDeviceShaderAtomicInt64Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTransformFeedbackFeaturesEXT_self(cs, (const VkPhysicalDeviceTransformFeedbackFeaturesEXT *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceScalarBlockLayoutFeatures_self(cs, (const VkPhysicalDeviceScalarBlockLayoutFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceUniformBufferStandardLayoutFeatures_self(cs, (const VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceBufferDeviceAddressFeatures_self(cs, (const VkPhysicalDeviceBufferDeviceAddressFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceImagelessFramebufferFeatures_self(cs, (const VkPhysicalDeviceImagelessFramebufferFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures_self(cs, (const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan11Features_self(cs, (const VkPhysicalDeviceVulkan11Features *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDeviceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan12Features_self(cs, (const VkPhysicalDeviceVulkan12Features *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceCreateInfo_self(struct vn_cs *cs, const VkDeviceCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->queueCreateInfoCount);
    if (val->pQueueCreateInfos) {
        vn_encode_array_size(cs, val->queueCreateInfoCount);
        for (uint32_t i = 0; i < val->queueCreateInfoCount; i++)
            vn_encode_VkDeviceQueueCreateInfo(cs, &val->pQueueCreateInfos[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->enabledLayerCount);
    if (val->ppEnabledLayerNames) {
        vn_encode_array_size(cs, val->enabledLayerCount);
        for (uint32_t i = 0; i < val->enabledLayerCount; i++) {
            const size_t string_size = strlen(val->ppEnabledLayerNames[i]) + 1;
            vn_encode_array_size(cs, string_size);
            vn_encode_blob_array(cs, val->ppEnabledLayerNames[i], string_size);
        }
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->enabledExtensionCount);
    if (val->ppEnabledExtensionNames) {
        vn_encode_array_size(cs, val->enabledExtensionCount);
        for (uint32_t i = 0; i < val->enabledExtensionCount; i++) {
            const size_t string_size = strlen(val->ppEnabledExtensionNames[i]) + 1;
            vn_encode_array_size(cs, string_size);
            vn_encode_blob_array(cs, val->ppEnabledExtensionNames[i], string_size);
        }
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (vn_encode_simple_pointer(cs, val->pEnabledFeatures))
        vn_encode_VkPhysicalDeviceFeatures(cs, val->pEnabledFeatures);
}

static inline void
vn_encode_VkDeviceCreateInfo(struct vn_cs *cs, const VkDeviceCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO });
    vn_encode_VkDeviceCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceCreateInfo_self(cs, val);
}

/* struct VkInstanceCreateInfo chain */

static inline size_t
vn_sizeof_VkInstanceCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkInstanceCreateInfo_self(const VkInstanceCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_simple_pointer(val->pApplicationInfo);
    if (val->pApplicationInfo)
        size += vn_sizeof_VkApplicationInfo(val->pApplicationInfo);
    size += vn_sizeof_uint32_t(&val->enabledLayerCount);
    if (val->ppEnabledLayerNames) {
        size += vn_sizeof_array_size(val->enabledLayerCount);
        for (uint32_t i = 0; i < val->enabledLayerCount; i++) {
            const size_t string_size = strlen(val->ppEnabledLayerNames[i]) + 1;
            size += vn_sizeof_array_size(string_size);
            size += vn_sizeof_blob_array(val->ppEnabledLayerNames[i], string_size);
        }
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->enabledExtensionCount);
    if (val->ppEnabledExtensionNames) {
        size += vn_sizeof_array_size(val->enabledExtensionCount);
        for (uint32_t i = 0; i < val->enabledExtensionCount; i++) {
            const size_t string_size = strlen(val->ppEnabledExtensionNames[i]) + 1;
            size += vn_sizeof_array_size(string_size);
            size += vn_sizeof_blob_array(val->ppEnabledExtensionNames[i], string_size);
        }
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkInstanceCreateInfo(const VkInstanceCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkInstanceCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkInstanceCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkInstanceCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkInstanceCreateInfo_self(struct vn_cs *cs, const VkInstanceCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    if (vn_encode_simple_pointer(cs, val->pApplicationInfo))
        vn_encode_VkApplicationInfo(cs, val->pApplicationInfo);
    vn_encode_uint32_t(cs, &val->enabledLayerCount);
    if (val->ppEnabledLayerNames) {
        vn_encode_array_size(cs, val->enabledLayerCount);
        for (uint32_t i = 0; i < val->enabledLayerCount; i++) {
            const size_t string_size = strlen(val->ppEnabledLayerNames[i]) + 1;
            vn_encode_array_size(cs, string_size);
            vn_encode_blob_array(cs, val->ppEnabledLayerNames[i], string_size);
        }
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->enabledExtensionCount);
    if (val->ppEnabledExtensionNames) {
        vn_encode_array_size(cs, val->enabledExtensionCount);
        for (uint32_t i = 0; i < val->enabledExtensionCount; i++) {
            const size_t string_size = strlen(val->ppEnabledExtensionNames[i]) + 1;
            vn_encode_array_size(cs, string_size);
            vn_encode_blob_array(cs, val->ppEnabledExtensionNames[i], string_size);
        }
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkInstanceCreateInfo(struct vn_cs *cs, const VkInstanceCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO });
    vn_encode_VkInstanceCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkInstanceCreateInfo_self(cs, val);
}

/* struct VkQueueFamilyProperties */

static inline size_t
vn_sizeof_VkQueueFamilyProperties(const VkQueueFamilyProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->queueFlags);
    size += vn_sizeof_uint32_t(&val->queueCount);
    size += vn_sizeof_uint32_t(&val->timestampValidBits);
    size += vn_sizeof_VkExtent3D(&val->minImageTransferGranularity);
    return size;
}

static inline void
vn_decode_VkQueueFamilyProperties(struct vn_cs *cs, VkQueueFamilyProperties *val)
{
    vn_decode_VkFlags(cs, &val->queueFlags);
    vn_decode_uint32_t(cs, &val->queueCount);
    vn_decode_uint32_t(cs, &val->timestampValidBits);
    vn_decode_VkExtent3D(cs, &val->minImageTransferGranularity);
}

static inline size_t
vn_sizeof_VkQueueFamilyProperties_partial(const VkQueueFamilyProperties *val)
{
    size_t size = 0;
    /* skip val->queueFlags */
    /* skip val->queueCount */
    /* skip val->timestampValidBits */
    size += vn_sizeof_VkExtent3D_partial(&val->minImageTransferGranularity);
    return size;
}

static inline void
vn_encode_VkQueueFamilyProperties_partial(struct vn_cs *cs, const VkQueueFamilyProperties *val)
{
    /* skip val->queueFlags */
    /* skip val->queueCount */
    /* skip val->timestampValidBits */
    vn_encode_VkExtent3D_partial(cs, &val->minImageTransferGranularity);
}

/* struct VkMemoryType */

static inline size_t
vn_sizeof_VkMemoryType(const VkMemoryType *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->propertyFlags);
    size += vn_sizeof_uint32_t(&val->heapIndex);
    return size;
}

static inline void
vn_decode_VkMemoryType(struct vn_cs *cs, VkMemoryType *val)
{
    vn_decode_VkFlags(cs, &val->propertyFlags);
    vn_decode_uint32_t(cs, &val->heapIndex);
}

static inline size_t
vn_sizeof_VkMemoryType_partial(const VkMemoryType *val)
{
    size_t size = 0;
    /* skip val->propertyFlags */
    /* skip val->heapIndex */
    return size;
}

static inline void
vn_encode_VkMemoryType_partial(struct vn_cs *cs, const VkMemoryType *val)
{
    /* skip val->propertyFlags */
    /* skip val->heapIndex */
}

/* struct VkMemoryHeap */

static inline size_t
vn_sizeof_VkMemoryHeap(const VkMemoryHeap *val)
{
    size_t size = 0;
    size += vn_sizeof_VkDeviceSize(&val->size);
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline void
vn_decode_VkMemoryHeap(struct vn_cs *cs, VkMemoryHeap *val)
{
    vn_decode_VkDeviceSize(cs, &val->size);
    vn_decode_VkFlags(cs, &val->flags);
}

static inline size_t
vn_sizeof_VkMemoryHeap_partial(const VkMemoryHeap *val)
{
    size_t size = 0;
    /* skip val->size */
    /* skip val->flags */
    return size;
}

static inline void
vn_encode_VkMemoryHeap_partial(struct vn_cs *cs, const VkMemoryHeap *val)
{
    /* skip val->size */
    /* skip val->flags */
}

/* struct VkPhysicalDeviceMemoryProperties */

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties(const VkPhysicalDeviceMemoryProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->memoryTypeCount);
    size += vn_sizeof_array_size(VK_MAX_MEMORY_TYPES);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
        size += vn_sizeof_VkMemoryType(&val->memoryTypes[i]);
    size += vn_sizeof_uint32_t(&val->memoryHeapCount);
    size += vn_sizeof_array_size(VK_MAX_MEMORY_HEAPS);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; i++)
        size += vn_sizeof_VkMemoryHeap(&val->memoryHeaps[i]);
    return size;
}

static inline void
vn_decode_VkPhysicalDeviceMemoryProperties(struct vn_cs *cs, VkPhysicalDeviceMemoryProperties *val)
{
    vn_decode_uint32_t(cs, &val->memoryTypeCount);
    {
        vn_decode_array_size(cs, VK_MAX_MEMORY_TYPES);
        for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
            vn_decode_VkMemoryType(cs, &val->memoryTypes[i]);
    }
    vn_decode_uint32_t(cs, &val->memoryHeapCount);
    {
        vn_decode_array_size(cs, VK_MAX_MEMORY_HEAPS);
        for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; i++)
            vn_decode_VkMemoryHeap(cs, &val->memoryHeaps[i]);
    }
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties_partial(const VkPhysicalDeviceMemoryProperties *val)
{
    size_t size = 0;
    /* skip val->memoryTypeCount */
    size += vn_sizeof_array_size(VK_MAX_MEMORY_TYPES);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
        size += vn_sizeof_VkMemoryType_partial(&val->memoryTypes[i]);
    /* skip val->memoryHeapCount */
    size += vn_sizeof_array_size(VK_MAX_MEMORY_HEAPS);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; i++)
        size += vn_sizeof_VkMemoryHeap_partial(&val->memoryHeaps[i]);
    return size;
}

static inline void
vn_encode_VkPhysicalDeviceMemoryProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceMemoryProperties *val)
{
    /* skip val->memoryTypeCount */
    vn_encode_array_size(cs, VK_MAX_MEMORY_TYPES);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
        vn_encode_VkMemoryType_partial(cs, &val->memoryTypes[i]);
    /* skip val->memoryHeapCount */
    vn_encode_array_size(cs, VK_MAX_MEMORY_HEAPS);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; i++)
        vn_encode_VkMemoryHeap_partial(cs, &val->memoryHeaps[i]);
}

/* struct VkExportMemoryAllocateInfo chain */

static inline size_t
vn_sizeof_VkExportMemoryAllocateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExportMemoryAllocateInfo_self(const VkExportMemoryAllocateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->handleTypes);
    return size;
}

static inline size_t
vn_sizeof_VkExportMemoryAllocateInfo(const VkExportMemoryAllocateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExportMemoryAllocateInfo_pnext(val->pNext);
    size += vn_sizeof_VkExportMemoryAllocateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkExportMemoryAllocateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExportMemoryAllocateInfo_self(struct vn_cs *cs, const VkExportMemoryAllocateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->handleTypes);
}

static inline void
vn_encode_VkExportMemoryAllocateInfo(struct vn_cs *cs, const VkExportMemoryAllocateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO });
    vn_encode_VkExportMemoryAllocateInfo_pnext(cs, val->pNext);
    vn_encode_VkExportMemoryAllocateInfo_self(cs, val);
}

/* struct VkMemoryAllocateFlagsInfo chain */

static inline size_t
vn_sizeof_VkMemoryAllocateFlagsInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryAllocateFlagsInfo_self(const VkMemoryAllocateFlagsInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->deviceMask);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryAllocateFlagsInfo(const VkMemoryAllocateFlagsInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryAllocateFlagsInfo_pnext(val->pNext);
    size += vn_sizeof_VkMemoryAllocateFlagsInfo_self(val);

    return size;
}

static inline void
vn_encode_VkMemoryAllocateFlagsInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMemoryAllocateFlagsInfo_self(struct vn_cs *cs, const VkMemoryAllocateFlagsInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->deviceMask);
}

static inline void
vn_encode_VkMemoryAllocateFlagsInfo(struct vn_cs *cs, const VkMemoryAllocateFlagsInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO });
    vn_encode_VkMemoryAllocateFlagsInfo_pnext(cs, val->pNext);
    vn_encode_VkMemoryAllocateFlagsInfo_self(cs, val);
}

/* struct VkMemoryDedicatedAllocateInfo chain */

static inline size_t
vn_sizeof_VkMemoryDedicatedAllocateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryDedicatedAllocateInfo_self(const VkMemoryDedicatedAllocateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImage(&val->image);
    size += vn_sizeof_VkBuffer(&val->buffer);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryDedicatedAllocateInfo(const VkMemoryDedicatedAllocateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryDedicatedAllocateInfo_pnext(val->pNext);
    size += vn_sizeof_VkMemoryDedicatedAllocateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkMemoryDedicatedAllocateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMemoryDedicatedAllocateInfo_self(struct vn_cs *cs, const VkMemoryDedicatedAllocateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImage(cs, &val->image);
    vn_encode_VkBuffer(cs, &val->buffer);
}

static inline void
vn_encode_VkMemoryDedicatedAllocateInfo(struct vn_cs *cs, const VkMemoryDedicatedAllocateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO });
    vn_encode_VkMemoryDedicatedAllocateInfo_pnext(cs, val->pNext);
    vn_encode_VkMemoryDedicatedAllocateInfo_self(cs, val);
}

/* struct VkMemoryOpaqueCaptureAddressAllocateInfo chain */

static inline size_t
vn_sizeof_VkMemoryOpaqueCaptureAddressAllocateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryOpaqueCaptureAddressAllocateInfo_self(const VkMemoryOpaqueCaptureAddressAllocateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint64_t(&val->opaqueCaptureAddress);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryOpaqueCaptureAddressAllocateInfo(const VkMemoryOpaqueCaptureAddressAllocateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryOpaqueCaptureAddressAllocateInfo_pnext(val->pNext);
    size += vn_sizeof_VkMemoryOpaqueCaptureAddressAllocateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkMemoryOpaqueCaptureAddressAllocateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMemoryOpaqueCaptureAddressAllocateInfo_self(struct vn_cs *cs, const VkMemoryOpaqueCaptureAddressAllocateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint64_t(cs, &val->opaqueCaptureAddress);
}

static inline void
vn_encode_VkMemoryOpaqueCaptureAddressAllocateInfo(struct vn_cs *cs, const VkMemoryOpaqueCaptureAddressAllocateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO });
    vn_encode_VkMemoryOpaqueCaptureAddressAllocateInfo_pnext(cs, val->pNext);
    vn_encode_VkMemoryOpaqueCaptureAddressAllocateInfo_self(cs, val);
}

/* struct VkMemoryAllocateInfo chain */

static inline size_t
vn_sizeof_VkMemoryAllocateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkMemoryAllocateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkExportMemoryAllocateInfo_self((const VkExportMemoryAllocateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkMemoryAllocateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkMemoryAllocateFlagsInfo_self((const VkMemoryAllocateFlagsInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkMemoryAllocateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkMemoryDedicatedAllocateInfo_self((const VkMemoryDedicatedAllocateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkMemoryAllocateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkMemoryOpaqueCaptureAddressAllocateInfo_self((const VkMemoryOpaqueCaptureAddressAllocateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryAllocateInfo_self(const VkMemoryAllocateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDeviceSize(&val->allocationSize);
    size += vn_sizeof_uint32_t(&val->memoryTypeIndex);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryAllocateInfo(const VkMemoryAllocateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryAllocateInfo_pnext(val->pNext);
    size += vn_sizeof_VkMemoryAllocateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkMemoryAllocateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkMemoryAllocateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkExportMemoryAllocateInfo_self(cs, (const VkExportMemoryAllocateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkMemoryAllocateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkMemoryAllocateFlagsInfo_self(cs, (const VkMemoryAllocateFlagsInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkMemoryAllocateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkMemoryDedicatedAllocateInfo_self(cs, (const VkMemoryDedicatedAllocateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkMemoryAllocateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkMemoryOpaqueCaptureAddressAllocateInfo_self(cs, (const VkMemoryOpaqueCaptureAddressAllocateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMemoryAllocateInfo_self(struct vn_cs *cs, const VkMemoryAllocateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkDeviceSize(cs, &val->allocationSize);
    vn_encode_uint32_t(cs, &val->memoryTypeIndex);
}

static inline void
vn_encode_VkMemoryAllocateInfo(struct vn_cs *cs, const VkMemoryAllocateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO });
    vn_encode_VkMemoryAllocateInfo_pnext(cs, val->pNext);
    vn_encode_VkMemoryAllocateInfo_self(cs, val);
}

/* struct VkMemoryRequirements */

static inline size_t
vn_sizeof_VkMemoryRequirements(const VkMemoryRequirements *val)
{
    size_t size = 0;
    size += vn_sizeof_VkDeviceSize(&val->size);
    size += vn_sizeof_VkDeviceSize(&val->alignment);
    size += vn_sizeof_uint32_t(&val->memoryTypeBits);
    return size;
}

static inline void
vn_decode_VkMemoryRequirements(struct vn_cs *cs, VkMemoryRequirements *val)
{
    vn_decode_VkDeviceSize(cs, &val->size);
    vn_decode_VkDeviceSize(cs, &val->alignment);
    vn_decode_uint32_t(cs, &val->memoryTypeBits);
}

static inline size_t
vn_sizeof_VkMemoryRequirements_partial(const VkMemoryRequirements *val)
{
    size_t size = 0;
    /* skip val->size */
    /* skip val->alignment */
    /* skip val->memoryTypeBits */
    return size;
}

static inline void
vn_encode_VkMemoryRequirements_partial(struct vn_cs *cs, const VkMemoryRequirements *val)
{
    /* skip val->size */
    /* skip val->alignment */
    /* skip val->memoryTypeBits */
}

/* struct VkSparseImageFormatProperties */

static inline size_t
vn_sizeof_VkSparseImageFormatProperties(const VkSparseImageFormatProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->aspectMask);
    size += vn_sizeof_VkExtent3D(&val->imageGranularity);
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline void
vn_decode_VkSparseImageFormatProperties(struct vn_cs *cs, VkSparseImageFormatProperties *val)
{
    vn_decode_VkFlags(cs, &val->aspectMask);
    vn_decode_VkExtent3D(cs, &val->imageGranularity);
    vn_decode_VkFlags(cs, &val->flags);
}

static inline size_t
vn_sizeof_VkSparseImageFormatProperties_partial(const VkSparseImageFormatProperties *val)
{
    size_t size = 0;
    /* skip val->aspectMask */
    size += vn_sizeof_VkExtent3D_partial(&val->imageGranularity);
    /* skip val->flags */
    return size;
}

static inline void
vn_encode_VkSparseImageFormatProperties_partial(struct vn_cs *cs, const VkSparseImageFormatProperties *val)
{
    /* skip val->aspectMask */
    vn_encode_VkExtent3D_partial(cs, &val->imageGranularity);
    /* skip val->flags */
}

/* struct VkSparseImageMemoryRequirements */

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements(const VkSparseImageMemoryRequirements *val)
{
    size_t size = 0;
    size += vn_sizeof_VkSparseImageFormatProperties(&val->formatProperties);
    size += vn_sizeof_uint32_t(&val->imageMipTailFirstLod);
    size += vn_sizeof_VkDeviceSize(&val->imageMipTailSize);
    size += vn_sizeof_VkDeviceSize(&val->imageMipTailOffset);
    size += vn_sizeof_VkDeviceSize(&val->imageMipTailStride);
    return size;
}

static inline void
vn_decode_VkSparseImageMemoryRequirements(struct vn_cs *cs, VkSparseImageMemoryRequirements *val)
{
    vn_decode_VkSparseImageFormatProperties(cs, &val->formatProperties);
    vn_decode_uint32_t(cs, &val->imageMipTailFirstLod);
    vn_decode_VkDeviceSize(cs, &val->imageMipTailSize);
    vn_decode_VkDeviceSize(cs, &val->imageMipTailOffset);
    vn_decode_VkDeviceSize(cs, &val->imageMipTailStride);
}

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements_partial(const VkSparseImageMemoryRequirements *val)
{
    size_t size = 0;
    size += vn_sizeof_VkSparseImageFormatProperties_partial(&val->formatProperties);
    /* skip val->imageMipTailFirstLod */
    /* skip val->imageMipTailSize */
    /* skip val->imageMipTailOffset */
    /* skip val->imageMipTailStride */
    return size;
}

static inline void
vn_encode_VkSparseImageMemoryRequirements_partial(struct vn_cs *cs, const VkSparseImageMemoryRequirements *val)
{
    vn_encode_VkSparseImageFormatProperties_partial(cs, &val->formatProperties);
    /* skip val->imageMipTailFirstLod */
    /* skip val->imageMipTailSize */
    /* skip val->imageMipTailOffset */
    /* skip val->imageMipTailStride */
}

/* struct VkMappedMemoryRange chain */

static inline size_t
vn_sizeof_VkMappedMemoryRange_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMappedMemoryRange_self(const VkMappedMemoryRange *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDeviceMemory(&val->memory);
    size += vn_sizeof_VkDeviceSize(&val->offset);
    size += vn_sizeof_VkDeviceSize(&val->size);
    return size;
}

static inline size_t
vn_sizeof_VkMappedMemoryRange(const VkMappedMemoryRange *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMappedMemoryRange_pnext(val->pNext);
    size += vn_sizeof_VkMappedMemoryRange_self(val);

    return size;
}

static inline void
vn_encode_VkMappedMemoryRange_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMappedMemoryRange_self(struct vn_cs *cs, const VkMappedMemoryRange *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkDeviceMemory(cs, &val->memory);
    vn_encode_VkDeviceSize(cs, &val->offset);
    vn_encode_VkDeviceSize(cs, &val->size);
}

static inline void
vn_encode_VkMappedMemoryRange(struct vn_cs *cs, const VkMappedMemoryRange *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE });
    vn_encode_VkMappedMemoryRange_pnext(cs, val->pNext);
    vn_encode_VkMappedMemoryRange_self(cs, val);
}

static inline void
vn_decode_VkMappedMemoryRange_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkMappedMemoryRange_self(struct vn_cs *cs, VkMappedMemoryRange *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkDeviceMemory(cs, &val->memory);
    vn_decode_VkDeviceSize(cs, &val->offset);
    vn_decode_VkDeviceSize(cs, &val->size);
}

static inline void
vn_decode_VkMappedMemoryRange(struct vn_cs *cs, VkMappedMemoryRange *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);

    assert(val->sType == stype);
    vn_decode_VkMappedMemoryRange_pnext(cs, val->pNext);
    vn_decode_VkMappedMemoryRange_self(cs, val);
}

/* struct VkFormatProperties */

static inline size_t
vn_sizeof_VkFormatProperties(const VkFormatProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->linearTilingFeatures);
    size += vn_sizeof_VkFlags(&val->optimalTilingFeatures);
    size += vn_sizeof_VkFlags(&val->bufferFeatures);
    return size;
}

static inline void
vn_decode_VkFormatProperties(struct vn_cs *cs, VkFormatProperties *val)
{
    vn_decode_VkFlags(cs, &val->linearTilingFeatures);
    vn_decode_VkFlags(cs, &val->optimalTilingFeatures);
    vn_decode_VkFlags(cs, &val->bufferFeatures);
}

static inline size_t
vn_sizeof_VkFormatProperties_partial(const VkFormatProperties *val)
{
    size_t size = 0;
    /* skip val->linearTilingFeatures */
    /* skip val->optimalTilingFeatures */
    /* skip val->bufferFeatures */
    return size;
}

static inline void
vn_encode_VkFormatProperties_partial(struct vn_cs *cs, const VkFormatProperties *val)
{
    /* skip val->linearTilingFeatures */
    /* skip val->optimalTilingFeatures */
    /* skip val->bufferFeatures */
}

/* struct VkImageFormatProperties */

static inline size_t
vn_sizeof_VkImageFormatProperties(const VkImageFormatProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_VkExtent3D(&val->maxExtent);
    size += vn_sizeof_uint32_t(&val->maxMipLevels);
    size += vn_sizeof_uint32_t(&val->maxArrayLayers);
    size += vn_sizeof_VkFlags(&val->sampleCounts);
    size += vn_sizeof_VkDeviceSize(&val->maxResourceSize);
    return size;
}

static inline void
vn_decode_VkImageFormatProperties(struct vn_cs *cs, VkImageFormatProperties *val)
{
    vn_decode_VkExtent3D(cs, &val->maxExtent);
    vn_decode_uint32_t(cs, &val->maxMipLevels);
    vn_decode_uint32_t(cs, &val->maxArrayLayers);
    vn_decode_VkFlags(cs, &val->sampleCounts);
    vn_decode_VkDeviceSize(cs, &val->maxResourceSize);
}

static inline size_t
vn_sizeof_VkImageFormatProperties_partial(const VkImageFormatProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_VkExtent3D_partial(&val->maxExtent);
    /* skip val->maxMipLevels */
    /* skip val->maxArrayLayers */
    /* skip val->sampleCounts */
    /* skip val->maxResourceSize */
    return size;
}

static inline void
vn_encode_VkImageFormatProperties_partial(struct vn_cs *cs, const VkImageFormatProperties *val)
{
    vn_encode_VkExtent3D_partial(cs, &val->maxExtent);
    /* skip val->maxMipLevels */
    /* skip val->maxArrayLayers */
    /* skip val->sampleCounts */
    /* skip val->maxResourceSize */
}

/* struct VkDescriptorBufferInfo */

static inline size_t
vn_sizeof_VkDescriptorBufferInfo(const VkDescriptorBufferInfo *val)
{
    size_t size = 0;
    size += vn_sizeof_VkBuffer(&val->buffer);
    size += vn_sizeof_VkDeviceSize(&val->offset);
    size += vn_sizeof_VkDeviceSize(&val->range);
    return size;
}

static inline void
vn_encode_VkDescriptorBufferInfo(struct vn_cs *cs, const VkDescriptorBufferInfo *val)
{
    vn_encode_VkBuffer(cs, &val->buffer);
    vn_encode_VkDeviceSize(cs, &val->offset);
    vn_encode_VkDeviceSize(cs, &val->range);
}

/* struct VkDescriptorImageInfo */

static inline size_t
vn_sizeof_VkDescriptorImageInfo(const VkDescriptorImageInfo *val)
{
    size_t size = 0;
    size += vn_sizeof_VkSampler(&val->sampler);
    size += vn_sizeof_VkImageView(&val->imageView);
    size += vn_sizeof_VkImageLayout(&val->imageLayout);
    return size;
}

static inline void
vn_encode_VkDescriptorImageInfo(struct vn_cs *cs, const VkDescriptorImageInfo *val)
{
    vn_encode_VkSampler(cs, &val->sampler);
    vn_encode_VkImageView(cs, &val->imageView);
    vn_encode_VkImageLayout(cs, &val->imageLayout);
}

/* struct VkWriteDescriptorSet chain */

static inline size_t
vn_sizeof_VkWriteDescriptorSet_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkWriteDescriptorSet_self(const VkWriteDescriptorSet *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDescriptorSet(&val->dstSet);
    size += vn_sizeof_uint32_t(&val->dstBinding);
    size += vn_sizeof_uint32_t(&val->dstArrayElement);
    size += vn_sizeof_uint32_t(&val->descriptorCount);
    size += vn_sizeof_VkDescriptorType(&val->descriptorType);
    if (val->pImageInfo) {
        size += vn_sizeof_array_size(val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            size += vn_sizeof_VkDescriptorImageInfo(&val->pImageInfo[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    if (val->pBufferInfo) {
        size += vn_sizeof_array_size(val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            size += vn_sizeof_VkDescriptorBufferInfo(&val->pBufferInfo[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    if (val->pTexelBufferView) {
        size += vn_sizeof_array_size(val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            size += vn_sizeof_VkBufferView(&val->pTexelBufferView[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkWriteDescriptorSet(const VkWriteDescriptorSet *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkWriteDescriptorSet_pnext(val->pNext);
    size += vn_sizeof_VkWriteDescriptorSet_self(val);

    return size;
}

static inline void
vn_encode_VkWriteDescriptorSet_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkWriteDescriptorSet_self(struct vn_cs *cs, const VkWriteDescriptorSet *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkDescriptorSet(cs, &val->dstSet);
    vn_encode_uint32_t(cs, &val->dstBinding);
    vn_encode_uint32_t(cs, &val->dstArrayElement);
    vn_encode_uint32_t(cs, &val->descriptorCount);
    vn_encode_VkDescriptorType(cs, &val->descriptorType);
    if (val->pImageInfo) {
        vn_encode_array_size(cs, val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            vn_encode_VkDescriptorImageInfo(cs, &val->pImageInfo[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (val->pBufferInfo) {
        vn_encode_array_size(cs, val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            vn_encode_VkDescriptorBufferInfo(cs, &val->pBufferInfo[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (val->pTexelBufferView) {
        vn_encode_array_size(cs, val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            vn_encode_VkBufferView(cs, &val->pTexelBufferView[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkWriteDescriptorSet(struct vn_cs *cs, const VkWriteDescriptorSet *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET });
    vn_encode_VkWriteDescriptorSet_pnext(cs, val->pNext);
    vn_encode_VkWriteDescriptorSet_self(cs, val);
}

/* struct VkCopyDescriptorSet chain */

static inline size_t
vn_sizeof_VkCopyDescriptorSet_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkCopyDescriptorSet_self(const VkCopyDescriptorSet *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDescriptorSet(&val->srcSet);
    size += vn_sizeof_uint32_t(&val->srcBinding);
    size += vn_sizeof_uint32_t(&val->srcArrayElement);
    size += vn_sizeof_VkDescriptorSet(&val->dstSet);
    size += vn_sizeof_uint32_t(&val->dstBinding);
    size += vn_sizeof_uint32_t(&val->dstArrayElement);
    size += vn_sizeof_uint32_t(&val->descriptorCount);
    return size;
}

static inline size_t
vn_sizeof_VkCopyDescriptorSet(const VkCopyDescriptorSet *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkCopyDescriptorSet_pnext(val->pNext);
    size += vn_sizeof_VkCopyDescriptorSet_self(val);

    return size;
}

static inline void
vn_encode_VkCopyDescriptorSet_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkCopyDescriptorSet_self(struct vn_cs *cs, const VkCopyDescriptorSet *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkDescriptorSet(cs, &val->srcSet);
    vn_encode_uint32_t(cs, &val->srcBinding);
    vn_encode_uint32_t(cs, &val->srcArrayElement);
    vn_encode_VkDescriptorSet(cs, &val->dstSet);
    vn_encode_uint32_t(cs, &val->dstBinding);
    vn_encode_uint32_t(cs, &val->dstArrayElement);
    vn_encode_uint32_t(cs, &val->descriptorCount);
}

static inline void
vn_encode_VkCopyDescriptorSet(struct vn_cs *cs, const VkCopyDescriptorSet *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET });
    vn_encode_VkCopyDescriptorSet_pnext(cs, val->pNext);
    vn_encode_VkCopyDescriptorSet_self(cs, val);
}

/* struct VkExternalMemoryBufferCreateInfo chain */

static inline size_t
vn_sizeof_VkExternalMemoryBufferCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalMemoryBufferCreateInfo_self(const VkExternalMemoryBufferCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->handleTypes);
    return size;
}

static inline size_t
vn_sizeof_VkExternalMemoryBufferCreateInfo(const VkExternalMemoryBufferCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalMemoryBufferCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkExternalMemoryBufferCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkExternalMemoryBufferCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExternalMemoryBufferCreateInfo_self(struct vn_cs *cs, const VkExternalMemoryBufferCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->handleTypes);
}

static inline void
vn_encode_VkExternalMemoryBufferCreateInfo(struct vn_cs *cs, const VkExternalMemoryBufferCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO });
    vn_encode_VkExternalMemoryBufferCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkExternalMemoryBufferCreateInfo_self(cs, val);
}

/* struct VkBufferOpaqueCaptureAddressCreateInfo chain */

static inline size_t
vn_sizeof_VkBufferOpaqueCaptureAddressCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBufferOpaqueCaptureAddressCreateInfo_self(const VkBufferOpaqueCaptureAddressCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint64_t(&val->opaqueCaptureAddress);
    return size;
}

static inline size_t
vn_sizeof_VkBufferOpaqueCaptureAddressCreateInfo(const VkBufferOpaqueCaptureAddressCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBufferOpaqueCaptureAddressCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkBufferOpaqueCaptureAddressCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBufferOpaqueCaptureAddressCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBufferOpaqueCaptureAddressCreateInfo_self(struct vn_cs *cs, const VkBufferOpaqueCaptureAddressCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint64_t(cs, &val->opaqueCaptureAddress);
}

static inline void
vn_encode_VkBufferOpaqueCaptureAddressCreateInfo(struct vn_cs *cs, const VkBufferOpaqueCaptureAddressCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO });
    vn_encode_VkBufferOpaqueCaptureAddressCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkBufferOpaqueCaptureAddressCreateInfo_self(cs, val);
}

/* struct VkBufferCreateInfo chain */

static inline size_t
vn_sizeof_VkBufferCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkBufferCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkExternalMemoryBufferCreateInfo_self((const VkExternalMemoryBufferCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkBufferCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkBufferOpaqueCaptureAddressCreateInfo_self((const VkBufferOpaqueCaptureAddressCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBufferCreateInfo_self(const VkBufferCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkDeviceSize(&val->size);
    size += vn_sizeof_VkFlags(&val->usage);
    size += vn_sizeof_VkSharingMode(&val->sharingMode);
    size += vn_sizeof_uint32_t(&val->queueFamilyIndexCount);
    if (val->pQueueFamilyIndices) {
        size += vn_sizeof_array_size(val->queueFamilyIndexCount);
        size += vn_sizeof_uint32_t_array(val->pQueueFamilyIndices, val->queueFamilyIndexCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkBufferCreateInfo(const VkBufferCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBufferCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkBufferCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBufferCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkBufferCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkExternalMemoryBufferCreateInfo_self(cs, (const VkExternalMemoryBufferCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkBufferCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkBufferOpaqueCaptureAddressCreateInfo_self(cs, (const VkBufferOpaqueCaptureAddressCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBufferCreateInfo_self(struct vn_cs *cs, const VkBufferCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkDeviceSize(cs, &val->size);
    vn_encode_VkFlags(cs, &val->usage);
    vn_encode_VkSharingMode(cs, &val->sharingMode);
    vn_encode_uint32_t(cs, &val->queueFamilyIndexCount);
    if (val->pQueueFamilyIndices) {
        vn_encode_array_size(cs, val->queueFamilyIndexCount);
        vn_encode_uint32_t_array(cs, val->pQueueFamilyIndices, val->queueFamilyIndexCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkBufferCreateInfo(struct vn_cs *cs, const VkBufferCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO });
    vn_encode_VkBufferCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkBufferCreateInfo_self(cs, val);
}

/* struct VkBufferViewCreateInfo chain */

static inline size_t
vn_sizeof_VkBufferViewCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBufferViewCreateInfo_self(const VkBufferViewCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkBuffer(&val->buffer);
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkDeviceSize(&val->offset);
    size += vn_sizeof_VkDeviceSize(&val->range);
    return size;
}

static inline size_t
vn_sizeof_VkBufferViewCreateInfo(const VkBufferViewCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBufferViewCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkBufferViewCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBufferViewCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBufferViewCreateInfo_self(struct vn_cs *cs, const VkBufferViewCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkBuffer(cs, &val->buffer);
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkDeviceSize(cs, &val->offset);
    vn_encode_VkDeviceSize(cs, &val->range);
}

static inline void
vn_encode_VkBufferViewCreateInfo(struct vn_cs *cs, const VkBufferViewCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO });
    vn_encode_VkBufferViewCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkBufferViewCreateInfo_self(cs, val);
}

/* struct VkImageSubresource */

static inline size_t
vn_sizeof_VkImageSubresource(const VkImageSubresource *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->aspectMask);
    size += vn_sizeof_uint32_t(&val->mipLevel);
    size += vn_sizeof_uint32_t(&val->arrayLayer);
    return size;
}

static inline void
vn_encode_VkImageSubresource(struct vn_cs *cs, const VkImageSubresource *val)
{
    vn_encode_VkFlags(cs, &val->aspectMask);
    vn_encode_uint32_t(cs, &val->mipLevel);
    vn_encode_uint32_t(cs, &val->arrayLayer);
}

/* struct VkImageSubresourceLayers */

static inline size_t
vn_sizeof_VkImageSubresourceLayers(const VkImageSubresourceLayers *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->aspectMask);
    size += vn_sizeof_uint32_t(&val->mipLevel);
    size += vn_sizeof_uint32_t(&val->baseArrayLayer);
    size += vn_sizeof_uint32_t(&val->layerCount);
    return size;
}

static inline void
vn_encode_VkImageSubresourceLayers(struct vn_cs *cs, const VkImageSubresourceLayers *val)
{
    vn_encode_VkFlags(cs, &val->aspectMask);
    vn_encode_uint32_t(cs, &val->mipLevel);
    vn_encode_uint32_t(cs, &val->baseArrayLayer);
    vn_encode_uint32_t(cs, &val->layerCount);
}

/* struct VkImageSubresourceRange */

static inline size_t
vn_sizeof_VkImageSubresourceRange(const VkImageSubresourceRange *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->aspectMask);
    size += vn_sizeof_uint32_t(&val->baseMipLevel);
    size += vn_sizeof_uint32_t(&val->levelCount);
    size += vn_sizeof_uint32_t(&val->baseArrayLayer);
    size += vn_sizeof_uint32_t(&val->layerCount);
    return size;
}

static inline void
vn_encode_VkImageSubresourceRange(struct vn_cs *cs, const VkImageSubresourceRange *val)
{
    vn_encode_VkFlags(cs, &val->aspectMask);
    vn_encode_uint32_t(cs, &val->baseMipLevel);
    vn_encode_uint32_t(cs, &val->levelCount);
    vn_encode_uint32_t(cs, &val->baseArrayLayer);
    vn_encode_uint32_t(cs, &val->layerCount);
}

/* struct VkMemoryBarrier chain */

static inline size_t
vn_sizeof_VkMemoryBarrier_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryBarrier_self(const VkMemoryBarrier *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->srcAccessMask);
    size += vn_sizeof_VkFlags(&val->dstAccessMask);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryBarrier(const VkMemoryBarrier *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryBarrier_pnext(val->pNext);
    size += vn_sizeof_VkMemoryBarrier_self(val);

    return size;
}

static inline void
vn_encode_VkMemoryBarrier_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMemoryBarrier_self(struct vn_cs *cs, const VkMemoryBarrier *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->srcAccessMask);
    vn_encode_VkFlags(cs, &val->dstAccessMask);
}

static inline void
vn_encode_VkMemoryBarrier(struct vn_cs *cs, const VkMemoryBarrier *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MEMORY_BARRIER });
    vn_encode_VkMemoryBarrier_pnext(cs, val->pNext);
    vn_encode_VkMemoryBarrier_self(cs, val);
}

/* struct VkBufferMemoryBarrier chain */

static inline size_t
vn_sizeof_VkBufferMemoryBarrier_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBufferMemoryBarrier_self(const VkBufferMemoryBarrier *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->srcAccessMask);
    size += vn_sizeof_VkFlags(&val->dstAccessMask);
    size += vn_sizeof_uint32_t(&val->srcQueueFamilyIndex);
    size += vn_sizeof_uint32_t(&val->dstQueueFamilyIndex);
    size += vn_sizeof_VkBuffer(&val->buffer);
    size += vn_sizeof_VkDeviceSize(&val->offset);
    size += vn_sizeof_VkDeviceSize(&val->size);
    return size;
}

static inline size_t
vn_sizeof_VkBufferMemoryBarrier(const VkBufferMemoryBarrier *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBufferMemoryBarrier_pnext(val->pNext);
    size += vn_sizeof_VkBufferMemoryBarrier_self(val);

    return size;
}

static inline void
vn_encode_VkBufferMemoryBarrier_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBufferMemoryBarrier_self(struct vn_cs *cs, const VkBufferMemoryBarrier *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->srcAccessMask);
    vn_encode_VkFlags(cs, &val->dstAccessMask);
    vn_encode_uint32_t(cs, &val->srcQueueFamilyIndex);
    vn_encode_uint32_t(cs, &val->dstQueueFamilyIndex);
    vn_encode_VkBuffer(cs, &val->buffer);
    vn_encode_VkDeviceSize(cs, &val->offset);
    vn_encode_VkDeviceSize(cs, &val->size);
}

static inline void
vn_encode_VkBufferMemoryBarrier(struct vn_cs *cs, const VkBufferMemoryBarrier *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER });
    vn_encode_VkBufferMemoryBarrier_pnext(cs, val->pNext);
    vn_encode_VkBufferMemoryBarrier_self(cs, val);
}

/* struct VkImageMemoryBarrier chain */

static inline size_t
vn_sizeof_VkImageMemoryBarrier_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageMemoryBarrier_self(const VkImageMemoryBarrier *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->srcAccessMask);
    size += vn_sizeof_VkFlags(&val->dstAccessMask);
    size += vn_sizeof_VkImageLayout(&val->oldLayout);
    size += vn_sizeof_VkImageLayout(&val->newLayout);
    size += vn_sizeof_uint32_t(&val->srcQueueFamilyIndex);
    size += vn_sizeof_uint32_t(&val->dstQueueFamilyIndex);
    size += vn_sizeof_VkImage(&val->image);
    size += vn_sizeof_VkImageSubresourceRange(&val->subresourceRange);
    return size;
}

static inline size_t
vn_sizeof_VkImageMemoryBarrier(const VkImageMemoryBarrier *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageMemoryBarrier_pnext(val->pNext);
    size += vn_sizeof_VkImageMemoryBarrier_self(val);

    return size;
}

static inline void
vn_encode_VkImageMemoryBarrier_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageMemoryBarrier_self(struct vn_cs *cs, const VkImageMemoryBarrier *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->srcAccessMask);
    vn_encode_VkFlags(cs, &val->dstAccessMask);
    vn_encode_VkImageLayout(cs, &val->oldLayout);
    vn_encode_VkImageLayout(cs, &val->newLayout);
    vn_encode_uint32_t(cs, &val->srcQueueFamilyIndex);
    vn_encode_uint32_t(cs, &val->dstQueueFamilyIndex);
    vn_encode_VkImage(cs, &val->image);
    vn_encode_VkImageSubresourceRange(cs, &val->subresourceRange);
}

static inline void
vn_encode_VkImageMemoryBarrier(struct vn_cs *cs, const VkImageMemoryBarrier *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER });
    vn_encode_VkImageMemoryBarrier_pnext(cs, val->pNext);
    vn_encode_VkImageMemoryBarrier_self(cs, val);
}

/* struct VkExternalMemoryImageCreateInfo chain */

static inline size_t
vn_sizeof_VkExternalMemoryImageCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalMemoryImageCreateInfo_self(const VkExternalMemoryImageCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->handleTypes);
    return size;
}

static inline size_t
vn_sizeof_VkExternalMemoryImageCreateInfo(const VkExternalMemoryImageCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalMemoryImageCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkExternalMemoryImageCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkExternalMemoryImageCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExternalMemoryImageCreateInfo_self(struct vn_cs *cs, const VkExternalMemoryImageCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->handleTypes);
}

static inline void
vn_encode_VkExternalMemoryImageCreateInfo(struct vn_cs *cs, const VkExternalMemoryImageCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO });
    vn_encode_VkExternalMemoryImageCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkExternalMemoryImageCreateInfo_self(cs, val);
}

/* struct VkImageFormatListCreateInfo chain */

static inline size_t
vn_sizeof_VkImageFormatListCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageFormatListCreateInfo_self(const VkImageFormatListCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->viewFormatCount);
    if (val->pViewFormats) {
        size += vn_sizeof_array_size(val->viewFormatCount);
        size += vn_sizeof_VkFormat_array(val->pViewFormats, val->viewFormatCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkImageFormatListCreateInfo(const VkImageFormatListCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageFormatListCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkImageFormatListCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkImageFormatListCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageFormatListCreateInfo_self(struct vn_cs *cs, const VkImageFormatListCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->viewFormatCount);
    if (val->pViewFormats) {
        vn_encode_array_size(cs, val->viewFormatCount);
        vn_encode_VkFormat_array(cs, val->pViewFormats, val->viewFormatCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkImageFormatListCreateInfo(struct vn_cs *cs, const VkImageFormatListCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO });
    vn_encode_VkImageFormatListCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkImageFormatListCreateInfo_self(cs, val);
}

/* struct VkImageDrmFormatModifierListCreateInfoEXT chain */

static inline size_t
vn_sizeof_VkImageDrmFormatModifierListCreateInfoEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierListCreateInfoEXT_self(const VkImageDrmFormatModifierListCreateInfoEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->drmFormatModifierCount);
    if (val->pDrmFormatModifiers) {
        size += vn_sizeof_array_size(val->drmFormatModifierCount);
        size += vn_sizeof_uint64_t_array(val->pDrmFormatModifiers, val->drmFormatModifierCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierListCreateInfoEXT(const VkImageDrmFormatModifierListCreateInfoEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageDrmFormatModifierListCreateInfoEXT_pnext(val->pNext);
    size += vn_sizeof_VkImageDrmFormatModifierListCreateInfoEXT_self(val);

    return size;
}

static inline void
vn_encode_VkImageDrmFormatModifierListCreateInfoEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageDrmFormatModifierListCreateInfoEXT_self(struct vn_cs *cs, const VkImageDrmFormatModifierListCreateInfoEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->drmFormatModifierCount);
    if (val->pDrmFormatModifiers) {
        vn_encode_array_size(cs, val->drmFormatModifierCount);
        vn_encode_uint64_t_array(cs, val->pDrmFormatModifiers, val->drmFormatModifierCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkImageDrmFormatModifierListCreateInfoEXT(struct vn_cs *cs, const VkImageDrmFormatModifierListCreateInfoEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT });
    vn_encode_VkImageDrmFormatModifierListCreateInfoEXT_pnext(cs, val->pNext);
    vn_encode_VkImageDrmFormatModifierListCreateInfoEXT_self(cs, val);
}

/* struct VkSubresourceLayout */

static inline size_t
vn_sizeof_VkSubresourceLayout(const VkSubresourceLayout *val)
{
    size_t size = 0;
    size += vn_sizeof_VkDeviceSize(&val->offset);
    size += vn_sizeof_VkDeviceSize(&val->size);
    size += vn_sizeof_VkDeviceSize(&val->rowPitch);
    size += vn_sizeof_VkDeviceSize(&val->arrayPitch);
    size += vn_sizeof_VkDeviceSize(&val->depthPitch);
    return size;
}

static inline void
vn_encode_VkSubresourceLayout(struct vn_cs *cs, const VkSubresourceLayout *val)
{
    vn_encode_VkDeviceSize(cs, &val->offset);
    vn_encode_VkDeviceSize(cs, &val->size);
    vn_encode_VkDeviceSize(cs, &val->rowPitch);
    vn_encode_VkDeviceSize(cs, &val->arrayPitch);
    vn_encode_VkDeviceSize(cs, &val->depthPitch);
}

static inline void
vn_decode_VkSubresourceLayout(struct vn_cs *cs, VkSubresourceLayout *val)
{
    vn_decode_VkDeviceSize(cs, &val->offset);
    vn_decode_VkDeviceSize(cs, &val->size);
    vn_decode_VkDeviceSize(cs, &val->rowPitch);
    vn_decode_VkDeviceSize(cs, &val->arrayPitch);
    vn_decode_VkDeviceSize(cs, &val->depthPitch);
}

static inline size_t
vn_sizeof_VkSubresourceLayout_partial(const VkSubresourceLayout *val)
{
    size_t size = 0;
    /* skip val->offset */
    /* skip val->size */
    /* skip val->rowPitch */
    /* skip val->arrayPitch */
    /* skip val->depthPitch */
    return size;
}

static inline void
vn_encode_VkSubresourceLayout_partial(struct vn_cs *cs, const VkSubresourceLayout *val)
{
    /* skip val->offset */
    /* skip val->size */
    /* skip val->rowPitch */
    /* skip val->arrayPitch */
    /* skip val->depthPitch */
}

/* struct VkImageDrmFormatModifierExplicitCreateInfoEXT chain */

static inline size_t
vn_sizeof_VkImageDrmFormatModifierExplicitCreateInfoEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierExplicitCreateInfoEXT_self(const VkImageDrmFormatModifierExplicitCreateInfoEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint64_t(&val->drmFormatModifier);
    size += vn_sizeof_uint32_t(&val->drmFormatModifierPlaneCount);
    if (val->pPlaneLayouts) {
        size += vn_sizeof_array_size(val->drmFormatModifierPlaneCount);
        for (uint32_t i = 0; i < val->drmFormatModifierPlaneCount; i++)
            size += vn_sizeof_VkSubresourceLayout(&val->pPlaneLayouts[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierExplicitCreateInfoEXT(const VkImageDrmFormatModifierExplicitCreateInfoEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageDrmFormatModifierExplicitCreateInfoEXT_pnext(val->pNext);
    size += vn_sizeof_VkImageDrmFormatModifierExplicitCreateInfoEXT_self(val);

    return size;
}

static inline void
vn_encode_VkImageDrmFormatModifierExplicitCreateInfoEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageDrmFormatModifierExplicitCreateInfoEXT_self(struct vn_cs *cs, const VkImageDrmFormatModifierExplicitCreateInfoEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint64_t(cs, &val->drmFormatModifier);
    vn_encode_uint32_t(cs, &val->drmFormatModifierPlaneCount);
    if (val->pPlaneLayouts) {
        vn_encode_array_size(cs, val->drmFormatModifierPlaneCount);
        for (uint32_t i = 0; i < val->drmFormatModifierPlaneCount; i++)
            vn_encode_VkSubresourceLayout(cs, &val->pPlaneLayouts[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkImageDrmFormatModifierExplicitCreateInfoEXT(struct vn_cs *cs, const VkImageDrmFormatModifierExplicitCreateInfoEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT });
    vn_encode_VkImageDrmFormatModifierExplicitCreateInfoEXT_pnext(cs, val->pNext);
    vn_encode_VkImageDrmFormatModifierExplicitCreateInfoEXT_self(cs, val);
}

/* struct VkImageStencilUsageCreateInfo chain */

static inline size_t
vn_sizeof_VkImageStencilUsageCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageStencilUsageCreateInfo_self(const VkImageStencilUsageCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->stencilUsage);
    return size;
}

static inline size_t
vn_sizeof_VkImageStencilUsageCreateInfo(const VkImageStencilUsageCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageStencilUsageCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkImageStencilUsageCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkImageStencilUsageCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageStencilUsageCreateInfo_self(struct vn_cs *cs, const VkImageStencilUsageCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->stencilUsage);
}

static inline void
vn_encode_VkImageStencilUsageCreateInfo(struct vn_cs *cs, const VkImageStencilUsageCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO });
    vn_encode_VkImageStencilUsageCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkImageStencilUsageCreateInfo_self(cs, val);
}

/* struct VkImageCreateInfo chain */

static inline size_t
vn_sizeof_VkImageCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkExternalMemoryImageCreateInfo_self((const VkExternalMemoryImageCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkImageFormatListCreateInfo_self((const VkImageFormatListCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkImageDrmFormatModifierListCreateInfoEXT_self((const VkImageDrmFormatModifierListCreateInfoEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkImageDrmFormatModifierExplicitCreateInfoEXT_self((const VkImageDrmFormatModifierExplicitCreateInfoEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkImageStencilUsageCreateInfo_self((const VkImageStencilUsageCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageCreateInfo_self(const VkImageCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkImageType(&val->imageType);
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkExtent3D(&val->extent);
    size += vn_sizeof_uint32_t(&val->mipLevels);
    size += vn_sizeof_uint32_t(&val->arrayLayers);
    size += vn_sizeof_VkSampleCountFlagBits(&val->samples);
    size += vn_sizeof_VkImageTiling(&val->tiling);
    size += vn_sizeof_VkFlags(&val->usage);
    size += vn_sizeof_VkSharingMode(&val->sharingMode);
    size += vn_sizeof_uint32_t(&val->queueFamilyIndexCount);
    if (val->pQueueFamilyIndices) {
        size += vn_sizeof_array_size(val->queueFamilyIndexCount);
        size += vn_sizeof_uint32_t_array(val->pQueueFamilyIndices, val->queueFamilyIndexCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_VkImageLayout(&val->initialLayout);
    return size;
}

static inline size_t
vn_sizeof_VkImageCreateInfo(const VkImageCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkImageCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkImageCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkExternalMemoryImageCreateInfo_self(cs, (const VkExternalMemoryImageCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkImageFormatListCreateInfo_self(cs, (const VkImageFormatListCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkImageDrmFormatModifierListCreateInfoEXT_self(cs, (const VkImageDrmFormatModifierListCreateInfoEXT *)pnext);
            return;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkImageDrmFormatModifierExplicitCreateInfoEXT_self(cs, (const VkImageDrmFormatModifierExplicitCreateInfoEXT *)pnext);
            return;
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkImageStencilUsageCreateInfo_self(cs, (const VkImageStencilUsageCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageCreateInfo_self(struct vn_cs *cs, const VkImageCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkImageType(cs, &val->imageType);
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkExtent3D(cs, &val->extent);
    vn_encode_uint32_t(cs, &val->mipLevels);
    vn_encode_uint32_t(cs, &val->arrayLayers);
    vn_encode_VkSampleCountFlagBits(cs, &val->samples);
    vn_encode_VkImageTiling(cs, &val->tiling);
    vn_encode_VkFlags(cs, &val->usage);
    vn_encode_VkSharingMode(cs, &val->sharingMode);
    vn_encode_uint32_t(cs, &val->queueFamilyIndexCount);
    if (val->pQueueFamilyIndices) {
        vn_encode_array_size(cs, val->queueFamilyIndexCount);
        vn_encode_uint32_t_array(cs, val->pQueueFamilyIndices, val->queueFamilyIndexCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_VkImageLayout(cs, &val->initialLayout);
}

static inline void
vn_encode_VkImageCreateInfo(struct vn_cs *cs, const VkImageCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO });
    vn_encode_VkImageCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkImageCreateInfo_self(cs, val);
}

/* struct VkImageViewUsageCreateInfo chain */

static inline size_t
vn_sizeof_VkImageViewUsageCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageViewUsageCreateInfo_self(const VkImageViewUsageCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->usage);
    return size;
}

static inline size_t
vn_sizeof_VkImageViewUsageCreateInfo(const VkImageViewUsageCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageViewUsageCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkImageViewUsageCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkImageViewUsageCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageViewUsageCreateInfo_self(struct vn_cs *cs, const VkImageViewUsageCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->usage);
}

static inline void
vn_encode_VkImageViewUsageCreateInfo(struct vn_cs *cs, const VkImageViewUsageCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO });
    vn_encode_VkImageViewUsageCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkImageViewUsageCreateInfo_self(cs, val);
}

/* struct VkSamplerYcbcrConversionInfo chain */

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionInfo_self(const VkSamplerYcbcrConversionInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSamplerYcbcrConversion(&val->conversion);
    return size;
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionInfo(const VkSamplerYcbcrConversionInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSamplerYcbcrConversionInfo_pnext(val->pNext);
    size += vn_sizeof_VkSamplerYcbcrConversionInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSamplerYcbcrConversionInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSamplerYcbcrConversionInfo_self(struct vn_cs *cs, const VkSamplerYcbcrConversionInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkSamplerYcbcrConversion(cs, &val->conversion);
}

static inline void
vn_encode_VkSamplerYcbcrConversionInfo(struct vn_cs *cs, const VkSamplerYcbcrConversionInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO });
    vn_encode_VkSamplerYcbcrConversionInfo_pnext(cs, val->pNext);
    vn_encode_VkSamplerYcbcrConversionInfo_self(cs, val);
}

/* struct VkImageViewCreateInfo chain */

static inline size_t
vn_sizeof_VkImageViewCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageViewCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkImageViewUsageCreateInfo_self((const VkImageViewUsageCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageViewCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkSamplerYcbcrConversionInfo_self((const VkSamplerYcbcrConversionInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageViewCreateInfo_self(const VkImageViewCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkImage(&val->image);
    size += vn_sizeof_VkImageViewType(&val->viewType);
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkComponentMapping(&val->components);
    size += vn_sizeof_VkImageSubresourceRange(&val->subresourceRange);
    return size;
}

static inline size_t
vn_sizeof_VkImageViewCreateInfo(const VkImageViewCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageViewCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkImageViewCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkImageViewCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageViewCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkImageViewUsageCreateInfo_self(cs, (const VkImageViewUsageCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageViewCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkSamplerYcbcrConversionInfo_self(cs, (const VkSamplerYcbcrConversionInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageViewCreateInfo_self(struct vn_cs *cs, const VkImageViewCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkImage(cs, &val->image);
    vn_encode_VkImageViewType(cs, &val->viewType);
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkComponentMapping(cs, &val->components);
    vn_encode_VkImageSubresourceRange(cs, &val->subresourceRange);
}

static inline void
vn_encode_VkImageViewCreateInfo(struct vn_cs *cs, const VkImageViewCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO });
    vn_encode_VkImageViewCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkImageViewCreateInfo_self(cs, val);
}

/* struct VkBufferCopy */

static inline size_t
vn_sizeof_VkBufferCopy(const VkBufferCopy *val)
{
    size_t size = 0;
    size += vn_sizeof_VkDeviceSize(&val->srcOffset);
    size += vn_sizeof_VkDeviceSize(&val->dstOffset);
    size += vn_sizeof_VkDeviceSize(&val->size);
    return size;
}

static inline void
vn_encode_VkBufferCopy(struct vn_cs *cs, const VkBufferCopy *val)
{
    vn_encode_VkDeviceSize(cs, &val->srcOffset);
    vn_encode_VkDeviceSize(cs, &val->dstOffset);
    vn_encode_VkDeviceSize(cs, &val->size);
}

/* struct VkSparseMemoryBind */

static inline size_t
vn_sizeof_VkSparseMemoryBind(const VkSparseMemoryBind *val)
{
    size_t size = 0;
    size += vn_sizeof_VkDeviceSize(&val->resourceOffset);
    size += vn_sizeof_VkDeviceSize(&val->size);
    size += vn_sizeof_VkDeviceMemory(&val->memory);
    size += vn_sizeof_VkDeviceSize(&val->memoryOffset);
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline void
vn_encode_VkSparseMemoryBind(struct vn_cs *cs, const VkSparseMemoryBind *val)
{
    vn_encode_VkDeviceSize(cs, &val->resourceOffset);
    vn_encode_VkDeviceSize(cs, &val->size);
    vn_encode_VkDeviceMemory(cs, &val->memory);
    vn_encode_VkDeviceSize(cs, &val->memoryOffset);
    vn_encode_VkFlags(cs, &val->flags);
}

/* struct VkSparseImageMemoryBind */

static inline size_t
vn_sizeof_VkSparseImageMemoryBind(const VkSparseImageMemoryBind *val)
{
    size_t size = 0;
    size += vn_sizeof_VkImageSubresource(&val->subresource);
    size += vn_sizeof_VkOffset3D(&val->offset);
    size += vn_sizeof_VkExtent3D(&val->extent);
    size += vn_sizeof_VkDeviceMemory(&val->memory);
    size += vn_sizeof_VkDeviceSize(&val->memoryOffset);
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline void
vn_encode_VkSparseImageMemoryBind(struct vn_cs *cs, const VkSparseImageMemoryBind *val)
{
    vn_encode_VkImageSubresource(cs, &val->subresource);
    vn_encode_VkOffset3D(cs, &val->offset);
    vn_encode_VkExtent3D(cs, &val->extent);
    vn_encode_VkDeviceMemory(cs, &val->memory);
    vn_encode_VkDeviceSize(cs, &val->memoryOffset);
    vn_encode_VkFlags(cs, &val->flags);
}

/* struct VkSparseBufferMemoryBindInfo */

static inline size_t
vn_sizeof_VkSparseBufferMemoryBindInfo(const VkSparseBufferMemoryBindInfo *val)
{
    size_t size = 0;
    size += vn_sizeof_VkBuffer(&val->buffer);
    size += vn_sizeof_uint32_t(&val->bindCount);
    if (val->pBinds) {
        size += vn_sizeof_array_size(val->bindCount);
        for (uint32_t i = 0; i < val->bindCount; i++)
            size += vn_sizeof_VkSparseMemoryBind(&val->pBinds[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline void
vn_encode_VkSparseBufferMemoryBindInfo(struct vn_cs *cs, const VkSparseBufferMemoryBindInfo *val)
{
    vn_encode_VkBuffer(cs, &val->buffer);
    vn_encode_uint32_t(cs, &val->bindCount);
    if (val->pBinds) {
        vn_encode_array_size(cs, val->bindCount);
        for (uint32_t i = 0; i < val->bindCount; i++)
            vn_encode_VkSparseMemoryBind(cs, &val->pBinds[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

/* struct VkSparseImageOpaqueMemoryBindInfo */

static inline size_t
vn_sizeof_VkSparseImageOpaqueMemoryBindInfo(const VkSparseImageOpaqueMemoryBindInfo *val)
{
    size_t size = 0;
    size += vn_sizeof_VkImage(&val->image);
    size += vn_sizeof_uint32_t(&val->bindCount);
    if (val->pBinds) {
        size += vn_sizeof_array_size(val->bindCount);
        for (uint32_t i = 0; i < val->bindCount; i++)
            size += vn_sizeof_VkSparseMemoryBind(&val->pBinds[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline void
vn_encode_VkSparseImageOpaqueMemoryBindInfo(struct vn_cs *cs, const VkSparseImageOpaqueMemoryBindInfo *val)
{
    vn_encode_VkImage(cs, &val->image);
    vn_encode_uint32_t(cs, &val->bindCount);
    if (val->pBinds) {
        vn_encode_array_size(cs, val->bindCount);
        for (uint32_t i = 0; i < val->bindCount; i++)
            vn_encode_VkSparseMemoryBind(cs, &val->pBinds[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

/* struct VkSparseImageMemoryBindInfo */

static inline size_t
vn_sizeof_VkSparseImageMemoryBindInfo(const VkSparseImageMemoryBindInfo *val)
{
    size_t size = 0;
    size += vn_sizeof_VkImage(&val->image);
    size += vn_sizeof_uint32_t(&val->bindCount);
    if (val->pBinds) {
        size += vn_sizeof_array_size(val->bindCount);
        for (uint32_t i = 0; i < val->bindCount; i++)
            size += vn_sizeof_VkSparseImageMemoryBind(&val->pBinds[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline void
vn_encode_VkSparseImageMemoryBindInfo(struct vn_cs *cs, const VkSparseImageMemoryBindInfo *val)
{
    vn_encode_VkImage(cs, &val->image);
    vn_encode_uint32_t(cs, &val->bindCount);
    if (val->pBinds) {
        vn_encode_array_size(cs, val->bindCount);
        for (uint32_t i = 0; i < val->bindCount; i++)
            vn_encode_VkSparseImageMemoryBind(cs, &val->pBinds[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

/* struct VkDeviceGroupBindSparseInfo chain */

static inline size_t
vn_sizeof_VkDeviceGroupBindSparseInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceGroupBindSparseInfo_self(const VkDeviceGroupBindSparseInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->resourceDeviceIndex);
    size += vn_sizeof_uint32_t(&val->memoryDeviceIndex);
    return size;
}

static inline size_t
vn_sizeof_VkDeviceGroupBindSparseInfo(const VkDeviceGroupBindSparseInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceGroupBindSparseInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceGroupBindSparseInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceGroupBindSparseInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceGroupBindSparseInfo_self(struct vn_cs *cs, const VkDeviceGroupBindSparseInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->resourceDeviceIndex);
    vn_encode_uint32_t(cs, &val->memoryDeviceIndex);
}

static inline void
vn_encode_VkDeviceGroupBindSparseInfo(struct vn_cs *cs, const VkDeviceGroupBindSparseInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO });
    vn_encode_VkDeviceGroupBindSparseInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceGroupBindSparseInfo_self(cs, val);
}

/* struct VkTimelineSemaphoreSubmitInfo chain */

static inline size_t
vn_sizeof_VkTimelineSemaphoreSubmitInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkTimelineSemaphoreSubmitInfo_self(const VkTimelineSemaphoreSubmitInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->waitSemaphoreValueCount);
    if (val->pWaitSemaphoreValues) {
        size += vn_sizeof_array_size(val->waitSemaphoreValueCount);
        size += vn_sizeof_uint64_t_array(val->pWaitSemaphoreValues, val->waitSemaphoreValueCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->signalSemaphoreValueCount);
    if (val->pSignalSemaphoreValues) {
        size += vn_sizeof_array_size(val->signalSemaphoreValueCount);
        size += vn_sizeof_uint64_t_array(val->pSignalSemaphoreValues, val->signalSemaphoreValueCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkTimelineSemaphoreSubmitInfo(const VkTimelineSemaphoreSubmitInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkTimelineSemaphoreSubmitInfo_pnext(val->pNext);
    size += vn_sizeof_VkTimelineSemaphoreSubmitInfo_self(val);

    return size;
}

static inline void
vn_encode_VkTimelineSemaphoreSubmitInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkTimelineSemaphoreSubmitInfo_self(struct vn_cs *cs, const VkTimelineSemaphoreSubmitInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->waitSemaphoreValueCount);
    if (val->pWaitSemaphoreValues) {
        vn_encode_array_size(cs, val->waitSemaphoreValueCount);
        vn_encode_uint64_t_array(cs, val->pWaitSemaphoreValues, val->waitSemaphoreValueCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->signalSemaphoreValueCount);
    if (val->pSignalSemaphoreValues) {
        vn_encode_array_size(cs, val->signalSemaphoreValueCount);
        vn_encode_uint64_t_array(cs, val->pSignalSemaphoreValues, val->signalSemaphoreValueCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkTimelineSemaphoreSubmitInfo(struct vn_cs *cs, const VkTimelineSemaphoreSubmitInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO });
    vn_encode_VkTimelineSemaphoreSubmitInfo_pnext(cs, val->pNext);
    vn_encode_VkTimelineSemaphoreSubmitInfo_self(cs, val);
}

/* struct VkBindSparseInfo chain */

static inline size_t
vn_sizeof_VkBindSparseInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkBindSparseInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkDeviceGroupBindSparseInfo_self((const VkDeviceGroupBindSparseInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkBindSparseInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkTimelineSemaphoreSubmitInfo_self((const VkTimelineSemaphoreSubmitInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBindSparseInfo_self(const VkBindSparseInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->waitSemaphoreCount);
    if (val->pWaitSemaphores) {
        size += vn_sizeof_array_size(val->waitSemaphoreCount);
        for (uint32_t i = 0; i < val->waitSemaphoreCount; i++)
            size += vn_sizeof_VkSemaphore(&val->pWaitSemaphores[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->bufferBindCount);
    if (val->pBufferBinds) {
        size += vn_sizeof_array_size(val->bufferBindCount);
        for (uint32_t i = 0; i < val->bufferBindCount; i++)
            size += vn_sizeof_VkSparseBufferMemoryBindInfo(&val->pBufferBinds[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->imageOpaqueBindCount);
    if (val->pImageOpaqueBinds) {
        size += vn_sizeof_array_size(val->imageOpaqueBindCount);
        for (uint32_t i = 0; i < val->imageOpaqueBindCount; i++)
            size += vn_sizeof_VkSparseImageOpaqueMemoryBindInfo(&val->pImageOpaqueBinds[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->imageBindCount);
    if (val->pImageBinds) {
        size += vn_sizeof_array_size(val->imageBindCount);
        for (uint32_t i = 0; i < val->imageBindCount; i++)
            size += vn_sizeof_VkSparseImageMemoryBindInfo(&val->pImageBinds[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->signalSemaphoreCount);
    if (val->pSignalSemaphores) {
        size += vn_sizeof_array_size(val->signalSemaphoreCount);
        for (uint32_t i = 0; i < val->signalSemaphoreCount; i++)
            size += vn_sizeof_VkSemaphore(&val->pSignalSemaphores[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkBindSparseInfo(const VkBindSparseInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBindSparseInfo_pnext(val->pNext);
    size += vn_sizeof_VkBindSparseInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBindSparseInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkBindSparseInfo_pnext(cs, pnext->pNext);
            vn_encode_VkDeviceGroupBindSparseInfo_self(cs, (const VkDeviceGroupBindSparseInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkBindSparseInfo_pnext(cs, pnext->pNext);
            vn_encode_VkTimelineSemaphoreSubmitInfo_self(cs, (const VkTimelineSemaphoreSubmitInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBindSparseInfo_self(struct vn_cs *cs, const VkBindSparseInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->waitSemaphoreCount);
    if (val->pWaitSemaphores) {
        vn_encode_array_size(cs, val->waitSemaphoreCount);
        for (uint32_t i = 0; i < val->waitSemaphoreCount; i++)
            vn_encode_VkSemaphore(cs, &val->pWaitSemaphores[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->bufferBindCount);
    if (val->pBufferBinds) {
        vn_encode_array_size(cs, val->bufferBindCount);
        for (uint32_t i = 0; i < val->bufferBindCount; i++)
            vn_encode_VkSparseBufferMemoryBindInfo(cs, &val->pBufferBinds[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->imageOpaqueBindCount);
    if (val->pImageOpaqueBinds) {
        vn_encode_array_size(cs, val->imageOpaqueBindCount);
        for (uint32_t i = 0; i < val->imageOpaqueBindCount; i++)
            vn_encode_VkSparseImageOpaqueMemoryBindInfo(cs, &val->pImageOpaqueBinds[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->imageBindCount);
    if (val->pImageBinds) {
        vn_encode_array_size(cs, val->imageBindCount);
        for (uint32_t i = 0; i < val->imageBindCount; i++)
            vn_encode_VkSparseImageMemoryBindInfo(cs, &val->pImageBinds[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->signalSemaphoreCount);
    if (val->pSignalSemaphores) {
        vn_encode_array_size(cs, val->signalSemaphoreCount);
        for (uint32_t i = 0; i < val->signalSemaphoreCount; i++)
            vn_encode_VkSemaphore(cs, &val->pSignalSemaphores[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkBindSparseInfo(struct vn_cs *cs, const VkBindSparseInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BIND_SPARSE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BIND_SPARSE_INFO });
    vn_encode_VkBindSparseInfo_pnext(cs, val->pNext);
    vn_encode_VkBindSparseInfo_self(cs, val);
}

/* struct VkImageCopy */

static inline size_t
vn_sizeof_VkImageCopy(const VkImageCopy *val)
{
    size_t size = 0;
    size += vn_sizeof_VkImageSubresourceLayers(&val->srcSubresource);
    size += vn_sizeof_VkOffset3D(&val->srcOffset);
    size += vn_sizeof_VkImageSubresourceLayers(&val->dstSubresource);
    size += vn_sizeof_VkOffset3D(&val->dstOffset);
    size += vn_sizeof_VkExtent3D(&val->extent);
    return size;
}

static inline void
vn_encode_VkImageCopy(struct vn_cs *cs, const VkImageCopy *val)
{
    vn_encode_VkImageSubresourceLayers(cs, &val->srcSubresource);
    vn_encode_VkOffset3D(cs, &val->srcOffset);
    vn_encode_VkImageSubresourceLayers(cs, &val->dstSubresource);
    vn_encode_VkOffset3D(cs, &val->dstOffset);
    vn_encode_VkExtent3D(cs, &val->extent);
}

/* struct VkImageBlit */

static inline size_t
vn_sizeof_VkImageBlit(const VkImageBlit *val)
{
    size_t size = 0;
    size += vn_sizeof_VkImageSubresourceLayers(&val->srcSubresource);
    size += vn_sizeof_array_size(2);
    for (uint32_t i = 0; i < 2; i++)
        size += vn_sizeof_VkOffset3D(&val->srcOffsets[i]);
    size += vn_sizeof_VkImageSubresourceLayers(&val->dstSubresource);
    size += vn_sizeof_array_size(2);
    for (uint32_t i = 0; i < 2; i++)
        size += vn_sizeof_VkOffset3D(&val->dstOffsets[i]);
    return size;
}

static inline void
vn_encode_VkImageBlit(struct vn_cs *cs, const VkImageBlit *val)
{
    vn_encode_VkImageSubresourceLayers(cs, &val->srcSubresource);
    vn_encode_array_size(cs, 2);
    for (uint32_t i = 0; i < 2; i++)
        vn_encode_VkOffset3D(cs, &val->srcOffsets[i]);
    vn_encode_VkImageSubresourceLayers(cs, &val->dstSubresource);
    vn_encode_array_size(cs, 2);
    for (uint32_t i = 0; i < 2; i++)
        vn_encode_VkOffset3D(cs, &val->dstOffsets[i]);
}

/* struct VkBufferImageCopy */

static inline size_t
vn_sizeof_VkBufferImageCopy(const VkBufferImageCopy *val)
{
    size_t size = 0;
    size += vn_sizeof_VkDeviceSize(&val->bufferOffset);
    size += vn_sizeof_uint32_t(&val->bufferRowLength);
    size += vn_sizeof_uint32_t(&val->bufferImageHeight);
    size += vn_sizeof_VkImageSubresourceLayers(&val->imageSubresource);
    size += vn_sizeof_VkOffset3D(&val->imageOffset);
    size += vn_sizeof_VkExtent3D(&val->imageExtent);
    return size;
}

static inline void
vn_encode_VkBufferImageCopy(struct vn_cs *cs, const VkBufferImageCopy *val)
{
    vn_encode_VkDeviceSize(cs, &val->bufferOffset);
    vn_encode_uint32_t(cs, &val->bufferRowLength);
    vn_encode_uint32_t(cs, &val->bufferImageHeight);
    vn_encode_VkImageSubresourceLayers(cs, &val->imageSubresource);
    vn_encode_VkOffset3D(cs, &val->imageOffset);
    vn_encode_VkExtent3D(cs, &val->imageExtent);
}

/* struct VkImageResolve */

static inline size_t
vn_sizeof_VkImageResolve(const VkImageResolve *val)
{
    size_t size = 0;
    size += vn_sizeof_VkImageSubresourceLayers(&val->srcSubresource);
    size += vn_sizeof_VkOffset3D(&val->srcOffset);
    size += vn_sizeof_VkImageSubresourceLayers(&val->dstSubresource);
    size += vn_sizeof_VkOffset3D(&val->dstOffset);
    size += vn_sizeof_VkExtent3D(&val->extent);
    return size;
}

static inline void
vn_encode_VkImageResolve(struct vn_cs *cs, const VkImageResolve *val)
{
    vn_encode_VkImageSubresourceLayers(cs, &val->srcSubresource);
    vn_encode_VkOffset3D(cs, &val->srcOffset);
    vn_encode_VkImageSubresourceLayers(cs, &val->dstSubresource);
    vn_encode_VkOffset3D(cs, &val->dstOffset);
    vn_encode_VkExtent3D(cs, &val->extent);
}

/* struct VkShaderModuleCreateInfo chain */

static inline size_t
vn_sizeof_VkShaderModuleCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkShaderModuleCreateInfo_self(const VkShaderModuleCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_size_t(&val->codeSize);
    if (val->pCode) {
        size += vn_sizeof_array_size(val->codeSize / 4);
        size += vn_sizeof_uint32_t_array(val->pCode, val->codeSize / 4);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkShaderModuleCreateInfo(const VkShaderModuleCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkShaderModuleCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkShaderModuleCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkShaderModuleCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkShaderModuleCreateInfo_self(struct vn_cs *cs, const VkShaderModuleCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_size_t(cs, &val->codeSize);
    if (val->pCode) {
        vn_encode_array_size(cs, val->codeSize / 4);
        vn_encode_uint32_t_array(cs, val->pCode, val->codeSize / 4);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkShaderModuleCreateInfo(struct vn_cs *cs, const VkShaderModuleCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO });
    vn_encode_VkShaderModuleCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkShaderModuleCreateInfo_self(cs, val);
}

/* struct VkDescriptorSetLayoutBinding */

static inline size_t
vn_sizeof_VkDescriptorSetLayoutBinding(const VkDescriptorSetLayoutBinding *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->binding);
    size += vn_sizeof_VkDescriptorType(&val->descriptorType);
    size += vn_sizeof_uint32_t(&val->descriptorCount);
    size += vn_sizeof_VkFlags(&val->stageFlags);
    if (val->pImmutableSamplers) {
        size += vn_sizeof_array_size(val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            size += vn_sizeof_VkSampler(&val->pImmutableSamplers[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline void
vn_encode_VkDescriptorSetLayoutBinding(struct vn_cs *cs, const VkDescriptorSetLayoutBinding *val)
{
    vn_encode_uint32_t(cs, &val->binding);
    vn_encode_VkDescriptorType(cs, &val->descriptorType);
    vn_encode_uint32_t(cs, &val->descriptorCount);
    vn_encode_VkFlags(cs, &val->stageFlags);
    if (val->pImmutableSamplers) {
        vn_encode_array_size(cs, val->descriptorCount);
        for (uint32_t i = 0; i < val->descriptorCount; i++)
            vn_encode_VkSampler(cs, &val->pImmutableSamplers[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

/* struct VkDescriptorSetLayoutBindingFlagsCreateInfo chain */

static inline size_t
vn_sizeof_VkDescriptorSetLayoutBindingFlagsCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutBindingFlagsCreateInfo_self(const VkDescriptorSetLayoutBindingFlagsCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->bindingCount);
    if (val->pBindingFlags) {
        size += vn_sizeof_array_size(val->bindingCount);
        for (uint32_t i = 0; i < val->bindingCount; i++)
            size += vn_sizeof_VkFlags(&val->pBindingFlags[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutBindingFlagsCreateInfo(const VkDescriptorSetLayoutBindingFlagsCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetLayoutBindingFlagsCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorSetLayoutBindingFlagsCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDescriptorSetLayoutBindingFlagsCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorSetLayoutBindingFlagsCreateInfo_self(struct vn_cs *cs, const VkDescriptorSetLayoutBindingFlagsCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->bindingCount);
    if (val->pBindingFlags) {
        vn_encode_array_size(cs, val->bindingCount);
        for (uint32_t i = 0; i < val->bindingCount; i++)
            vn_encode_VkFlags(cs, &val->pBindingFlags[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDescriptorSetLayoutBindingFlagsCreateInfo(struct vn_cs *cs, const VkDescriptorSetLayoutBindingFlagsCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO });
    vn_encode_VkDescriptorSetLayoutBindingFlagsCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkDescriptorSetLayoutBindingFlagsCreateInfo_self(cs, val);
}

/* struct VkDescriptorSetLayoutCreateInfo chain */

static inline size_t
vn_sizeof_VkDescriptorSetLayoutCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDescriptorSetLayoutCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkDescriptorSetLayoutBindingFlagsCreateInfo_self((const VkDescriptorSetLayoutBindingFlagsCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutCreateInfo_self(const VkDescriptorSetLayoutCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->bindingCount);
    if (val->pBindings) {
        size += vn_sizeof_array_size(val->bindingCount);
        for (uint32_t i = 0; i < val->bindingCount; i++)
            size += vn_sizeof_VkDescriptorSetLayoutBinding(&val->pBindings[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetLayoutCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorSetLayoutCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDescriptorSetLayoutCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDescriptorSetLayoutCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkDescriptorSetLayoutBindingFlagsCreateInfo_self(cs, (const VkDescriptorSetLayoutBindingFlagsCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorSetLayoutCreateInfo_self(struct vn_cs *cs, const VkDescriptorSetLayoutCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->bindingCount);
    if (val->pBindings) {
        vn_encode_array_size(cs, val->bindingCount);
        for (uint32_t i = 0; i < val->bindingCount; i++)
            vn_encode_VkDescriptorSetLayoutBinding(cs, &val->pBindings[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDescriptorSetLayoutCreateInfo(struct vn_cs *cs, const VkDescriptorSetLayoutCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO });
    vn_encode_VkDescriptorSetLayoutCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkDescriptorSetLayoutCreateInfo_self(cs, val);
}

/* struct VkDescriptorPoolSize */

static inline size_t
vn_sizeof_VkDescriptorPoolSize(const VkDescriptorPoolSize *val)
{
    size_t size = 0;
    size += vn_sizeof_VkDescriptorType(&val->type);
    size += vn_sizeof_uint32_t(&val->descriptorCount);
    return size;
}

static inline void
vn_encode_VkDescriptorPoolSize(struct vn_cs *cs, const VkDescriptorPoolSize *val)
{
    vn_encode_VkDescriptorType(cs, &val->type);
    vn_encode_uint32_t(cs, &val->descriptorCount);
}

/* struct VkDescriptorPoolCreateInfo chain */

static inline size_t
vn_sizeof_VkDescriptorPoolCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorPoolCreateInfo_self(const VkDescriptorPoolCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->maxSets);
    size += vn_sizeof_uint32_t(&val->poolSizeCount);
    if (val->pPoolSizes) {
        size += vn_sizeof_array_size(val->poolSizeCount);
        for (uint32_t i = 0; i < val->poolSizeCount; i++)
            size += vn_sizeof_VkDescriptorPoolSize(&val->pPoolSizes[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorPoolCreateInfo(const VkDescriptorPoolCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorPoolCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorPoolCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDescriptorPoolCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorPoolCreateInfo_self(struct vn_cs *cs, const VkDescriptorPoolCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->maxSets);
    vn_encode_uint32_t(cs, &val->poolSizeCount);
    if (val->pPoolSizes) {
        vn_encode_array_size(cs, val->poolSizeCount);
        for (uint32_t i = 0; i < val->poolSizeCount; i++)
            vn_encode_VkDescriptorPoolSize(cs, &val->pPoolSizes[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDescriptorPoolCreateInfo(struct vn_cs *cs, const VkDescriptorPoolCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO });
    vn_encode_VkDescriptorPoolCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkDescriptorPoolCreateInfo_self(cs, val);
}

/* struct VkDescriptorSetVariableDescriptorCountAllocateInfo chain */

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountAllocateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountAllocateInfo_self(const VkDescriptorSetVariableDescriptorCountAllocateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->descriptorSetCount);
    if (val->pDescriptorCounts) {
        size += vn_sizeof_array_size(val->descriptorSetCount);
        size += vn_sizeof_uint32_t_array(val->pDescriptorCounts, val->descriptorSetCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountAllocateInfo(const VkDescriptorSetVariableDescriptorCountAllocateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetVariableDescriptorCountAllocateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorSetVariableDescriptorCountAllocateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDescriptorSetVariableDescriptorCountAllocateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorSetVariableDescriptorCountAllocateInfo_self(struct vn_cs *cs, const VkDescriptorSetVariableDescriptorCountAllocateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->descriptorSetCount);
    if (val->pDescriptorCounts) {
        vn_encode_array_size(cs, val->descriptorSetCount);
        vn_encode_uint32_t_array(cs, val->pDescriptorCounts, val->descriptorSetCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDescriptorSetVariableDescriptorCountAllocateInfo(struct vn_cs *cs, const VkDescriptorSetVariableDescriptorCountAllocateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO });
    vn_encode_VkDescriptorSetVariableDescriptorCountAllocateInfo_pnext(cs, val->pNext);
    vn_encode_VkDescriptorSetVariableDescriptorCountAllocateInfo_self(cs, val);
}

/* struct VkDescriptorSetAllocateInfo chain */

static inline size_t
vn_sizeof_VkDescriptorSetAllocateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDescriptorSetAllocateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkDescriptorSetVariableDescriptorCountAllocateInfo_self((const VkDescriptorSetVariableDescriptorCountAllocateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetAllocateInfo_self(const VkDescriptorSetAllocateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDescriptorPool(&val->descriptorPool);
    size += vn_sizeof_uint32_t(&val->descriptorSetCount);
    if (val->pSetLayouts) {
        size += vn_sizeof_array_size(val->descriptorSetCount);
        for (uint32_t i = 0; i < val->descriptorSetCount; i++)
            size += vn_sizeof_VkDescriptorSetLayout(&val->pSetLayouts[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetAllocateInfo(const VkDescriptorSetAllocateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetAllocateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorSetAllocateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDescriptorSetAllocateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDescriptorSetAllocateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkDescriptorSetVariableDescriptorCountAllocateInfo_self(cs, (const VkDescriptorSetVariableDescriptorCountAllocateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorSetAllocateInfo_self(struct vn_cs *cs, const VkDescriptorSetAllocateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkDescriptorPool(cs, &val->descriptorPool);
    vn_encode_uint32_t(cs, &val->descriptorSetCount);
    if (val->pSetLayouts) {
        vn_encode_array_size(cs, val->descriptorSetCount);
        for (uint32_t i = 0; i < val->descriptorSetCount; i++)
            vn_encode_VkDescriptorSetLayout(cs, &val->pSetLayouts[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDescriptorSetAllocateInfo(struct vn_cs *cs, const VkDescriptorSetAllocateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO });
    vn_encode_VkDescriptorSetAllocateInfo_pnext(cs, val->pNext);
    vn_encode_VkDescriptorSetAllocateInfo_self(cs, val);
}

/* struct VkSpecializationMapEntry */

static inline size_t
vn_sizeof_VkSpecializationMapEntry(const VkSpecializationMapEntry *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->constantID);
    size += vn_sizeof_uint32_t(&val->offset);
    size += vn_sizeof_size_t(&val->size);
    return size;
}

static inline void
vn_encode_VkSpecializationMapEntry(struct vn_cs *cs, const VkSpecializationMapEntry *val)
{
    vn_encode_uint32_t(cs, &val->constantID);
    vn_encode_uint32_t(cs, &val->offset);
    vn_encode_size_t(cs, &val->size);
}

/* struct VkSpecializationInfo */

static inline size_t
vn_sizeof_VkSpecializationInfo(const VkSpecializationInfo *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->mapEntryCount);
    if (val->pMapEntries) {
        size += vn_sizeof_array_size(val->mapEntryCount);
        for (uint32_t i = 0; i < val->mapEntryCount; i++)
            size += vn_sizeof_VkSpecializationMapEntry(&val->pMapEntries[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_size_t(&val->dataSize);
    if (val->pData) {
        size += vn_sizeof_array_size(val->dataSize);
        size += vn_sizeof_blob_array(val->pData, val->dataSize);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline void
vn_encode_VkSpecializationInfo(struct vn_cs *cs, const VkSpecializationInfo *val)
{
    vn_encode_uint32_t(cs, &val->mapEntryCount);
    if (val->pMapEntries) {
        vn_encode_array_size(cs, val->mapEntryCount);
        for (uint32_t i = 0; i < val->mapEntryCount; i++)
            vn_encode_VkSpecializationMapEntry(cs, &val->pMapEntries[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_size_t(cs, &val->dataSize);
    if (val->pData) {
        vn_encode_array_size(cs, val->dataSize);
        vn_encode_blob_array(cs, val->pData, val->dataSize);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

/* struct VkPipelineShaderStageCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineShaderStageCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineShaderStageCreateInfo_self(const VkPipelineShaderStageCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkShaderStageFlagBits(&val->stage);
    size += vn_sizeof_VkShaderModule(&val->module);
    if (val->pName) {
        const size_t string_size = strlen(val->pName) + 1;
        size += vn_sizeof_array_size(string_size);
        size += vn_sizeof_blob_array(val->pName, string_size);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_simple_pointer(val->pSpecializationInfo);
    if (val->pSpecializationInfo)
        size += vn_sizeof_VkSpecializationInfo(val->pSpecializationInfo);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineShaderStageCreateInfo(const VkPipelineShaderStageCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineShaderStageCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineShaderStageCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineShaderStageCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineShaderStageCreateInfo_self(struct vn_cs *cs, const VkPipelineShaderStageCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkShaderStageFlagBits(cs, &val->stage);
    vn_encode_VkShaderModule(cs, &val->module);
    if (val->pName) {
        const size_t string_size = strlen(val->pName) + 1;
        vn_encode_array_size(cs, string_size);
        vn_encode_blob_array(cs, val->pName, string_size);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (vn_encode_simple_pointer(cs, val->pSpecializationInfo))
        vn_encode_VkSpecializationInfo(cs, val->pSpecializationInfo);
}

static inline void
vn_encode_VkPipelineShaderStageCreateInfo(struct vn_cs *cs, const VkPipelineShaderStageCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO });
    vn_encode_VkPipelineShaderStageCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineShaderStageCreateInfo_self(cs, val);
}

/* struct VkComputePipelineCreateInfo chain */

static inline size_t
vn_sizeof_VkComputePipelineCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkComputePipelineCreateInfo_self(const VkComputePipelineCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkPipelineShaderStageCreateInfo(&val->stage);
    size += vn_sizeof_VkPipelineLayout(&val->layout);
    size += vn_sizeof_VkPipeline(&val->basePipelineHandle);
    size += vn_sizeof_int32_t(&val->basePipelineIndex);
    return size;
}

static inline size_t
vn_sizeof_VkComputePipelineCreateInfo(const VkComputePipelineCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkComputePipelineCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkComputePipelineCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkComputePipelineCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkComputePipelineCreateInfo_self(struct vn_cs *cs, const VkComputePipelineCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkPipelineShaderStageCreateInfo(cs, &val->stage);
    vn_encode_VkPipelineLayout(cs, &val->layout);
    vn_encode_VkPipeline(cs, &val->basePipelineHandle);
    vn_encode_int32_t(cs, &val->basePipelineIndex);
}

static inline void
vn_encode_VkComputePipelineCreateInfo(struct vn_cs *cs, const VkComputePipelineCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO });
    vn_encode_VkComputePipelineCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkComputePipelineCreateInfo_self(cs, val);
}

/* struct VkVertexInputBindingDescription */

static inline size_t
vn_sizeof_VkVertexInputBindingDescription(const VkVertexInputBindingDescription *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->binding);
    size += vn_sizeof_uint32_t(&val->stride);
    size += vn_sizeof_VkVertexInputRate(&val->inputRate);
    return size;
}

static inline void
vn_encode_VkVertexInputBindingDescription(struct vn_cs *cs, const VkVertexInputBindingDescription *val)
{
    vn_encode_uint32_t(cs, &val->binding);
    vn_encode_uint32_t(cs, &val->stride);
    vn_encode_VkVertexInputRate(cs, &val->inputRate);
}

/* struct VkVertexInputAttributeDescription */

static inline size_t
vn_sizeof_VkVertexInputAttributeDescription(const VkVertexInputAttributeDescription *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->location);
    size += vn_sizeof_uint32_t(&val->binding);
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_uint32_t(&val->offset);
    return size;
}

static inline void
vn_encode_VkVertexInputAttributeDescription(struct vn_cs *cs, const VkVertexInputAttributeDescription *val)
{
    vn_encode_uint32_t(cs, &val->location);
    vn_encode_uint32_t(cs, &val->binding);
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_uint32_t(cs, &val->offset);
}

/* struct VkPipelineVertexInputStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineVertexInputStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineVertexInputStateCreateInfo_self(const VkPipelineVertexInputStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->vertexBindingDescriptionCount);
    if (val->pVertexBindingDescriptions) {
        size += vn_sizeof_array_size(val->vertexBindingDescriptionCount);
        for (uint32_t i = 0; i < val->vertexBindingDescriptionCount; i++)
            size += vn_sizeof_VkVertexInputBindingDescription(&val->pVertexBindingDescriptions[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->vertexAttributeDescriptionCount);
    if (val->pVertexAttributeDescriptions) {
        size += vn_sizeof_array_size(val->vertexAttributeDescriptionCount);
        for (uint32_t i = 0; i < val->vertexAttributeDescriptionCount; i++)
            size += vn_sizeof_VkVertexInputAttributeDescription(&val->pVertexAttributeDescriptions[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkPipelineVertexInputStateCreateInfo(const VkPipelineVertexInputStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineVertexInputStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineVertexInputStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineVertexInputStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineVertexInputStateCreateInfo_self(struct vn_cs *cs, const VkPipelineVertexInputStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->vertexBindingDescriptionCount);
    if (val->pVertexBindingDescriptions) {
        vn_encode_array_size(cs, val->vertexBindingDescriptionCount);
        for (uint32_t i = 0; i < val->vertexBindingDescriptionCount; i++)
            vn_encode_VkVertexInputBindingDescription(cs, &val->pVertexBindingDescriptions[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->vertexAttributeDescriptionCount);
    if (val->pVertexAttributeDescriptions) {
        vn_encode_array_size(cs, val->vertexAttributeDescriptionCount);
        for (uint32_t i = 0; i < val->vertexAttributeDescriptionCount; i++)
            vn_encode_VkVertexInputAttributeDescription(cs, &val->pVertexAttributeDescriptions[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkPipelineVertexInputStateCreateInfo(struct vn_cs *cs, const VkPipelineVertexInputStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO });
    vn_encode_VkPipelineVertexInputStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineVertexInputStateCreateInfo_self(cs, val);
}

/* struct VkPipelineInputAssemblyStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineInputAssemblyStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineInputAssemblyStateCreateInfo_self(const VkPipelineInputAssemblyStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkPrimitiveTopology(&val->topology);
    size += vn_sizeof_VkBool32(&val->primitiveRestartEnable);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineInputAssemblyStateCreateInfo(const VkPipelineInputAssemblyStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineInputAssemblyStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineInputAssemblyStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineInputAssemblyStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineInputAssemblyStateCreateInfo_self(struct vn_cs *cs, const VkPipelineInputAssemblyStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkPrimitiveTopology(cs, &val->topology);
    vn_encode_VkBool32(cs, &val->primitiveRestartEnable);
}

static inline void
vn_encode_VkPipelineInputAssemblyStateCreateInfo(struct vn_cs *cs, const VkPipelineInputAssemblyStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO });
    vn_encode_VkPipelineInputAssemblyStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineInputAssemblyStateCreateInfo_self(cs, val);
}

/* struct VkPipelineTessellationDomainOriginStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineTessellationDomainOriginStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineTessellationDomainOriginStateCreateInfo_self(const VkPipelineTessellationDomainOriginStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkTessellationDomainOrigin(&val->domainOrigin);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineTessellationDomainOriginStateCreateInfo(const VkPipelineTessellationDomainOriginStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineTessellationDomainOriginStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineTessellationDomainOriginStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineTessellationDomainOriginStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineTessellationDomainOriginStateCreateInfo_self(struct vn_cs *cs, const VkPipelineTessellationDomainOriginStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkTessellationDomainOrigin(cs, &val->domainOrigin);
}

static inline void
vn_encode_VkPipelineTessellationDomainOriginStateCreateInfo(struct vn_cs *cs, const VkPipelineTessellationDomainOriginStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO });
    vn_encode_VkPipelineTessellationDomainOriginStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineTessellationDomainOriginStateCreateInfo_self(cs, val);
}

/* struct VkPipelineTessellationStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineTessellationStateCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPipelineTessellationStateCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPipelineTessellationDomainOriginStateCreateInfo_self((const VkPipelineTessellationDomainOriginStateCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineTessellationStateCreateInfo_self(const VkPipelineTessellationStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->patchControlPoints);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineTessellationStateCreateInfo(const VkPipelineTessellationStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineTessellationStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineTessellationStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineTessellationStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPipelineTessellationStateCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPipelineTessellationDomainOriginStateCreateInfo_self(cs, (const VkPipelineTessellationDomainOriginStateCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineTessellationStateCreateInfo_self(struct vn_cs *cs, const VkPipelineTessellationStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->patchControlPoints);
}

static inline void
vn_encode_VkPipelineTessellationStateCreateInfo(struct vn_cs *cs, const VkPipelineTessellationStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO });
    vn_encode_VkPipelineTessellationStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineTessellationStateCreateInfo_self(cs, val);
}

/* struct VkPipelineViewportStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineViewportStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineViewportStateCreateInfo_self(const VkPipelineViewportStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->viewportCount);
    if (val->pViewports) {
        size += vn_sizeof_array_size(val->viewportCount);
        for (uint32_t i = 0; i < val->viewportCount; i++)
            size += vn_sizeof_VkViewport(&val->pViewports[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->scissorCount);
    if (val->pScissors) {
        size += vn_sizeof_array_size(val->scissorCount);
        for (uint32_t i = 0; i < val->scissorCount; i++)
            size += vn_sizeof_VkRect2D(&val->pScissors[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkPipelineViewportStateCreateInfo(const VkPipelineViewportStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineViewportStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineViewportStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineViewportStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineViewportStateCreateInfo_self(struct vn_cs *cs, const VkPipelineViewportStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->viewportCount);
    if (val->pViewports) {
        vn_encode_array_size(cs, val->viewportCount);
        for (uint32_t i = 0; i < val->viewportCount; i++)
            vn_encode_VkViewport(cs, &val->pViewports[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->scissorCount);
    if (val->pScissors) {
        vn_encode_array_size(cs, val->scissorCount);
        for (uint32_t i = 0; i < val->scissorCount; i++)
            vn_encode_VkRect2D(cs, &val->pScissors[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkPipelineViewportStateCreateInfo(struct vn_cs *cs, const VkPipelineViewportStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO });
    vn_encode_VkPipelineViewportStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineViewportStateCreateInfo_self(cs, val);
}

/* struct VkPipelineRasterizationStateStreamCreateInfoEXT chain */

static inline size_t
vn_sizeof_VkPipelineRasterizationStateStreamCreateInfoEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineRasterizationStateStreamCreateInfoEXT_self(const VkPipelineRasterizationStateStreamCreateInfoEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->rasterizationStream);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineRasterizationStateStreamCreateInfoEXT(const VkPipelineRasterizationStateStreamCreateInfoEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineRasterizationStateStreamCreateInfoEXT_pnext(val->pNext);
    size += vn_sizeof_VkPipelineRasterizationStateStreamCreateInfoEXT_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineRasterizationStateStreamCreateInfoEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineRasterizationStateStreamCreateInfoEXT_self(struct vn_cs *cs, const VkPipelineRasterizationStateStreamCreateInfoEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->rasterizationStream);
}

static inline void
vn_encode_VkPipelineRasterizationStateStreamCreateInfoEXT(struct vn_cs *cs, const VkPipelineRasterizationStateStreamCreateInfoEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT });
    vn_encode_VkPipelineRasterizationStateStreamCreateInfoEXT_pnext(cs, val->pNext);
    vn_encode_VkPipelineRasterizationStateStreamCreateInfoEXT_self(cs, val);
}

/* struct VkPipelineRasterizationStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineRasterizationStateCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPipelineRasterizationStateCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkPipelineRasterizationStateStreamCreateInfoEXT_self((const VkPipelineRasterizationStateStreamCreateInfoEXT *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineRasterizationStateCreateInfo_self(const VkPipelineRasterizationStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkBool32(&val->depthClampEnable);
    size += vn_sizeof_VkBool32(&val->rasterizerDiscardEnable);
    size += vn_sizeof_VkPolygonMode(&val->polygonMode);
    size += vn_sizeof_VkFlags(&val->cullMode);
    size += vn_sizeof_VkFrontFace(&val->frontFace);
    size += vn_sizeof_VkBool32(&val->depthBiasEnable);
    size += vn_sizeof_float(&val->depthBiasConstantFactor);
    size += vn_sizeof_float(&val->depthBiasClamp);
    size += vn_sizeof_float(&val->depthBiasSlopeFactor);
    size += vn_sizeof_float(&val->lineWidth);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineRasterizationStateCreateInfo(const VkPipelineRasterizationStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineRasterizationStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineRasterizationStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineRasterizationStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPipelineRasterizationStateCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkPipelineRasterizationStateStreamCreateInfoEXT_self(cs, (const VkPipelineRasterizationStateStreamCreateInfoEXT *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineRasterizationStateCreateInfo_self(struct vn_cs *cs, const VkPipelineRasterizationStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkBool32(cs, &val->depthClampEnable);
    vn_encode_VkBool32(cs, &val->rasterizerDiscardEnable);
    vn_encode_VkPolygonMode(cs, &val->polygonMode);
    vn_encode_VkFlags(cs, &val->cullMode);
    vn_encode_VkFrontFace(cs, &val->frontFace);
    vn_encode_VkBool32(cs, &val->depthBiasEnable);
    vn_encode_float(cs, &val->depthBiasConstantFactor);
    vn_encode_float(cs, &val->depthBiasClamp);
    vn_encode_float(cs, &val->depthBiasSlopeFactor);
    vn_encode_float(cs, &val->lineWidth);
}

static inline void
vn_encode_VkPipelineRasterizationStateCreateInfo(struct vn_cs *cs, const VkPipelineRasterizationStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO });
    vn_encode_VkPipelineRasterizationStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineRasterizationStateCreateInfo_self(cs, val);
}

/* struct VkPipelineMultisampleStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineMultisampleStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineMultisampleStateCreateInfo_self(const VkPipelineMultisampleStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkSampleCountFlagBits(&val->rasterizationSamples);
    size += vn_sizeof_VkBool32(&val->sampleShadingEnable);
    size += vn_sizeof_float(&val->minSampleShading);
    if (val->pSampleMask) {
        size += vn_sizeof_array_size((val->rasterizationSamples + 31) / 32);
        size += vn_sizeof_VkSampleMask_array(val->pSampleMask, (val->rasterizationSamples + 31) / 32);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_VkBool32(&val->alphaToCoverageEnable);
    size += vn_sizeof_VkBool32(&val->alphaToOneEnable);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineMultisampleStateCreateInfo(const VkPipelineMultisampleStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineMultisampleStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineMultisampleStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineMultisampleStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineMultisampleStateCreateInfo_self(struct vn_cs *cs, const VkPipelineMultisampleStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkSampleCountFlagBits(cs, &val->rasterizationSamples);
    vn_encode_VkBool32(cs, &val->sampleShadingEnable);
    vn_encode_float(cs, &val->minSampleShading);
    if (val->pSampleMask) {
        vn_encode_array_size(cs, (val->rasterizationSamples + 31) / 32);
        vn_encode_VkSampleMask_array(cs, val->pSampleMask, (val->rasterizationSamples + 31) / 32);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_VkBool32(cs, &val->alphaToCoverageEnable);
    vn_encode_VkBool32(cs, &val->alphaToOneEnable);
}

static inline void
vn_encode_VkPipelineMultisampleStateCreateInfo(struct vn_cs *cs, const VkPipelineMultisampleStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO });
    vn_encode_VkPipelineMultisampleStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineMultisampleStateCreateInfo_self(cs, val);
}

/* struct VkPipelineColorBlendAttachmentState */

static inline size_t
vn_sizeof_VkPipelineColorBlendAttachmentState(const VkPipelineColorBlendAttachmentState *val)
{
    size_t size = 0;
    size += vn_sizeof_VkBool32(&val->blendEnable);
    size += vn_sizeof_VkBlendFactor(&val->srcColorBlendFactor);
    size += vn_sizeof_VkBlendFactor(&val->dstColorBlendFactor);
    size += vn_sizeof_VkBlendOp(&val->colorBlendOp);
    size += vn_sizeof_VkBlendFactor(&val->srcAlphaBlendFactor);
    size += vn_sizeof_VkBlendFactor(&val->dstAlphaBlendFactor);
    size += vn_sizeof_VkBlendOp(&val->alphaBlendOp);
    size += vn_sizeof_VkFlags(&val->colorWriteMask);
    return size;
}

static inline void
vn_encode_VkPipelineColorBlendAttachmentState(struct vn_cs *cs, const VkPipelineColorBlendAttachmentState *val)
{
    vn_encode_VkBool32(cs, &val->blendEnable);
    vn_encode_VkBlendFactor(cs, &val->srcColorBlendFactor);
    vn_encode_VkBlendFactor(cs, &val->dstColorBlendFactor);
    vn_encode_VkBlendOp(cs, &val->colorBlendOp);
    vn_encode_VkBlendFactor(cs, &val->srcAlphaBlendFactor);
    vn_encode_VkBlendFactor(cs, &val->dstAlphaBlendFactor);
    vn_encode_VkBlendOp(cs, &val->alphaBlendOp);
    vn_encode_VkFlags(cs, &val->colorWriteMask);
}

/* struct VkPipelineColorBlendStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineColorBlendStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineColorBlendStateCreateInfo_self(const VkPipelineColorBlendStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkBool32(&val->logicOpEnable);
    size += vn_sizeof_VkLogicOp(&val->logicOp);
    size += vn_sizeof_uint32_t(&val->attachmentCount);
    if (val->pAttachments) {
        size += vn_sizeof_array_size(val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            size += vn_sizeof_VkPipelineColorBlendAttachmentState(&val->pAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_array_size(4);
    size += vn_sizeof_float_array(val->blendConstants, 4);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineColorBlendStateCreateInfo(const VkPipelineColorBlendStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineColorBlendStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineColorBlendStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineColorBlendStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineColorBlendStateCreateInfo_self(struct vn_cs *cs, const VkPipelineColorBlendStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkBool32(cs, &val->logicOpEnable);
    vn_encode_VkLogicOp(cs, &val->logicOp);
    vn_encode_uint32_t(cs, &val->attachmentCount);
    if (val->pAttachments) {
        vn_encode_array_size(cs, val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            vn_encode_VkPipelineColorBlendAttachmentState(cs, &val->pAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_array_size(cs, 4);
    vn_encode_float_array(cs, val->blendConstants, 4);
}

static inline void
vn_encode_VkPipelineColorBlendStateCreateInfo(struct vn_cs *cs, const VkPipelineColorBlendStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO });
    vn_encode_VkPipelineColorBlendStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineColorBlendStateCreateInfo_self(cs, val);
}

/* struct VkPipelineDynamicStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineDynamicStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineDynamicStateCreateInfo_self(const VkPipelineDynamicStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->dynamicStateCount);
    if (val->pDynamicStates) {
        size += vn_sizeof_array_size(val->dynamicStateCount);
        size += vn_sizeof_VkDynamicState_array(val->pDynamicStates, val->dynamicStateCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkPipelineDynamicStateCreateInfo(const VkPipelineDynamicStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineDynamicStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineDynamicStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineDynamicStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineDynamicStateCreateInfo_self(struct vn_cs *cs, const VkPipelineDynamicStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->dynamicStateCount);
    if (val->pDynamicStates) {
        vn_encode_array_size(cs, val->dynamicStateCount);
        vn_encode_VkDynamicState_array(cs, val->pDynamicStates, val->dynamicStateCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkPipelineDynamicStateCreateInfo(struct vn_cs *cs, const VkPipelineDynamicStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO });
    vn_encode_VkPipelineDynamicStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineDynamicStateCreateInfo_self(cs, val);
}

/* struct VkStencilOpState */

static inline size_t
vn_sizeof_VkStencilOpState(const VkStencilOpState *val)
{
    size_t size = 0;
    size += vn_sizeof_VkStencilOp(&val->failOp);
    size += vn_sizeof_VkStencilOp(&val->passOp);
    size += vn_sizeof_VkStencilOp(&val->depthFailOp);
    size += vn_sizeof_VkCompareOp(&val->compareOp);
    size += vn_sizeof_uint32_t(&val->compareMask);
    size += vn_sizeof_uint32_t(&val->writeMask);
    size += vn_sizeof_uint32_t(&val->reference);
    return size;
}

static inline void
vn_encode_VkStencilOpState(struct vn_cs *cs, const VkStencilOpState *val)
{
    vn_encode_VkStencilOp(cs, &val->failOp);
    vn_encode_VkStencilOp(cs, &val->passOp);
    vn_encode_VkStencilOp(cs, &val->depthFailOp);
    vn_encode_VkCompareOp(cs, &val->compareOp);
    vn_encode_uint32_t(cs, &val->compareMask);
    vn_encode_uint32_t(cs, &val->writeMask);
    vn_encode_uint32_t(cs, &val->reference);
}

/* struct VkPipelineDepthStencilStateCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineDepthStencilStateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineDepthStencilStateCreateInfo_self(const VkPipelineDepthStencilStateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkBool32(&val->depthTestEnable);
    size += vn_sizeof_VkBool32(&val->depthWriteEnable);
    size += vn_sizeof_VkCompareOp(&val->depthCompareOp);
    size += vn_sizeof_VkBool32(&val->depthBoundsTestEnable);
    size += vn_sizeof_VkBool32(&val->stencilTestEnable);
    size += vn_sizeof_VkStencilOpState(&val->front);
    size += vn_sizeof_VkStencilOpState(&val->back);
    size += vn_sizeof_float(&val->minDepthBounds);
    size += vn_sizeof_float(&val->maxDepthBounds);
    return size;
}

static inline size_t
vn_sizeof_VkPipelineDepthStencilStateCreateInfo(const VkPipelineDepthStencilStateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineDepthStencilStateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineDepthStencilStateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineDepthStencilStateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineDepthStencilStateCreateInfo_self(struct vn_cs *cs, const VkPipelineDepthStencilStateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkBool32(cs, &val->depthTestEnable);
    vn_encode_VkBool32(cs, &val->depthWriteEnable);
    vn_encode_VkCompareOp(cs, &val->depthCompareOp);
    vn_encode_VkBool32(cs, &val->depthBoundsTestEnable);
    vn_encode_VkBool32(cs, &val->stencilTestEnable);
    vn_encode_VkStencilOpState(cs, &val->front);
    vn_encode_VkStencilOpState(cs, &val->back);
    vn_encode_float(cs, &val->minDepthBounds);
    vn_encode_float(cs, &val->maxDepthBounds);
}

static inline void
vn_encode_VkPipelineDepthStencilStateCreateInfo(struct vn_cs *cs, const VkPipelineDepthStencilStateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO });
    vn_encode_VkPipelineDepthStencilStateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineDepthStencilStateCreateInfo_self(cs, val);
}

/* struct VkGraphicsPipelineCreateInfo chain */

static inline size_t
vn_sizeof_VkGraphicsPipelineCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkGraphicsPipelineCreateInfo_self(const VkGraphicsPipelineCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->stageCount);
    if (val->pStages) {
        size += vn_sizeof_array_size(val->stageCount);
        for (uint32_t i = 0; i < val->stageCount; i++)
            size += vn_sizeof_VkPipelineShaderStageCreateInfo(&val->pStages[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_simple_pointer(val->pVertexInputState);
    if (val->pVertexInputState)
        size += vn_sizeof_VkPipelineVertexInputStateCreateInfo(val->pVertexInputState);
    size += vn_sizeof_simple_pointer(val->pInputAssemblyState);
    if (val->pInputAssemblyState)
        size += vn_sizeof_VkPipelineInputAssemblyStateCreateInfo(val->pInputAssemblyState);
    size += vn_sizeof_simple_pointer(val->pTessellationState);
    if (val->pTessellationState)
        size += vn_sizeof_VkPipelineTessellationStateCreateInfo(val->pTessellationState);
    size += vn_sizeof_simple_pointer(val->pViewportState);
    if (val->pViewportState)
        size += vn_sizeof_VkPipelineViewportStateCreateInfo(val->pViewportState);
    size += vn_sizeof_simple_pointer(val->pRasterizationState);
    if (val->pRasterizationState)
        size += vn_sizeof_VkPipelineRasterizationStateCreateInfo(val->pRasterizationState);
    size += vn_sizeof_simple_pointer(val->pMultisampleState);
    if (val->pMultisampleState)
        size += vn_sizeof_VkPipelineMultisampleStateCreateInfo(val->pMultisampleState);
    size += vn_sizeof_simple_pointer(val->pDepthStencilState);
    if (val->pDepthStencilState)
        size += vn_sizeof_VkPipelineDepthStencilStateCreateInfo(val->pDepthStencilState);
    size += vn_sizeof_simple_pointer(val->pColorBlendState);
    if (val->pColorBlendState)
        size += vn_sizeof_VkPipelineColorBlendStateCreateInfo(val->pColorBlendState);
    size += vn_sizeof_simple_pointer(val->pDynamicState);
    if (val->pDynamicState)
        size += vn_sizeof_VkPipelineDynamicStateCreateInfo(val->pDynamicState);
    size += vn_sizeof_VkPipelineLayout(&val->layout);
    size += vn_sizeof_VkRenderPass(&val->renderPass);
    size += vn_sizeof_uint32_t(&val->subpass);
    size += vn_sizeof_VkPipeline(&val->basePipelineHandle);
    size += vn_sizeof_int32_t(&val->basePipelineIndex);
    return size;
}

static inline size_t
vn_sizeof_VkGraphicsPipelineCreateInfo(const VkGraphicsPipelineCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkGraphicsPipelineCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkGraphicsPipelineCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkGraphicsPipelineCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkGraphicsPipelineCreateInfo_self(struct vn_cs *cs, const VkGraphicsPipelineCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->stageCount);
    if (val->pStages) {
        vn_encode_array_size(cs, val->stageCount);
        for (uint32_t i = 0; i < val->stageCount; i++)
            vn_encode_VkPipelineShaderStageCreateInfo(cs, &val->pStages[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (vn_encode_simple_pointer(cs, val->pVertexInputState))
        vn_encode_VkPipelineVertexInputStateCreateInfo(cs, val->pVertexInputState);
    if (vn_encode_simple_pointer(cs, val->pInputAssemblyState))
        vn_encode_VkPipelineInputAssemblyStateCreateInfo(cs, val->pInputAssemblyState);
    if (vn_encode_simple_pointer(cs, val->pTessellationState))
        vn_encode_VkPipelineTessellationStateCreateInfo(cs, val->pTessellationState);
    if (vn_encode_simple_pointer(cs, val->pViewportState))
        vn_encode_VkPipelineViewportStateCreateInfo(cs, val->pViewportState);
    if (vn_encode_simple_pointer(cs, val->pRasterizationState))
        vn_encode_VkPipelineRasterizationStateCreateInfo(cs, val->pRasterizationState);
    if (vn_encode_simple_pointer(cs, val->pMultisampleState))
        vn_encode_VkPipelineMultisampleStateCreateInfo(cs, val->pMultisampleState);
    if (vn_encode_simple_pointer(cs, val->pDepthStencilState))
        vn_encode_VkPipelineDepthStencilStateCreateInfo(cs, val->pDepthStencilState);
    if (vn_encode_simple_pointer(cs, val->pColorBlendState))
        vn_encode_VkPipelineColorBlendStateCreateInfo(cs, val->pColorBlendState);
    if (vn_encode_simple_pointer(cs, val->pDynamicState))
        vn_encode_VkPipelineDynamicStateCreateInfo(cs, val->pDynamicState);
    vn_encode_VkPipelineLayout(cs, &val->layout);
    vn_encode_VkRenderPass(cs, &val->renderPass);
    vn_encode_uint32_t(cs, &val->subpass);
    vn_encode_VkPipeline(cs, &val->basePipelineHandle);
    vn_encode_int32_t(cs, &val->basePipelineIndex);
}

static inline void
vn_encode_VkGraphicsPipelineCreateInfo(struct vn_cs *cs, const VkGraphicsPipelineCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO });
    vn_encode_VkGraphicsPipelineCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkGraphicsPipelineCreateInfo_self(cs, val);
}

/* struct VkPipelineCacheCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineCacheCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineCacheCreateInfo_self(const VkPipelineCacheCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_size_t(&val->initialDataSize);
    if (val->pInitialData) {
        size += vn_sizeof_array_size(val->initialDataSize);
        size += vn_sizeof_blob_array(val->pInitialData, val->initialDataSize);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkPipelineCacheCreateInfo(const VkPipelineCacheCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineCacheCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineCacheCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineCacheCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineCacheCreateInfo_self(struct vn_cs *cs, const VkPipelineCacheCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_size_t(cs, &val->initialDataSize);
    if (val->pInitialData) {
        vn_encode_array_size(cs, val->initialDataSize);
        vn_encode_blob_array(cs, val->pInitialData, val->initialDataSize);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkPipelineCacheCreateInfo(struct vn_cs *cs, const VkPipelineCacheCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO });
    vn_encode_VkPipelineCacheCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineCacheCreateInfo_self(cs, val);
}

/* struct VkPushConstantRange */

static inline size_t
vn_sizeof_VkPushConstantRange(const VkPushConstantRange *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->stageFlags);
    size += vn_sizeof_uint32_t(&val->offset);
    size += vn_sizeof_uint32_t(&val->size);
    return size;
}

static inline void
vn_encode_VkPushConstantRange(struct vn_cs *cs, const VkPushConstantRange *val)
{
    vn_encode_VkFlags(cs, &val->stageFlags);
    vn_encode_uint32_t(cs, &val->offset);
    vn_encode_uint32_t(cs, &val->size);
}

/* struct VkPipelineLayoutCreateInfo chain */

static inline size_t
vn_sizeof_VkPipelineLayoutCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPipelineLayoutCreateInfo_self(const VkPipelineLayoutCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->setLayoutCount);
    if (val->pSetLayouts) {
        size += vn_sizeof_array_size(val->setLayoutCount);
        for (uint32_t i = 0; i < val->setLayoutCount; i++)
            size += vn_sizeof_VkDescriptorSetLayout(&val->pSetLayouts[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->pushConstantRangeCount);
    if (val->pPushConstantRanges) {
        size += vn_sizeof_array_size(val->pushConstantRangeCount);
        for (uint32_t i = 0; i < val->pushConstantRangeCount; i++)
            size += vn_sizeof_VkPushConstantRange(&val->pPushConstantRanges[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkPipelineLayoutCreateInfo(const VkPipelineLayoutCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPipelineLayoutCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkPipelineLayoutCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPipelineLayoutCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPipelineLayoutCreateInfo_self(struct vn_cs *cs, const VkPipelineLayoutCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->setLayoutCount);
    if (val->pSetLayouts) {
        vn_encode_array_size(cs, val->setLayoutCount);
        for (uint32_t i = 0; i < val->setLayoutCount; i++)
            vn_encode_VkDescriptorSetLayout(cs, &val->pSetLayouts[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->pushConstantRangeCount);
    if (val->pPushConstantRanges) {
        vn_encode_array_size(cs, val->pushConstantRangeCount);
        for (uint32_t i = 0; i < val->pushConstantRangeCount; i++)
            vn_encode_VkPushConstantRange(cs, &val->pPushConstantRanges[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkPipelineLayoutCreateInfo(struct vn_cs *cs, const VkPipelineLayoutCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO });
    vn_encode_VkPipelineLayoutCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkPipelineLayoutCreateInfo_self(cs, val);
}

/* struct VkSamplerReductionModeCreateInfo chain */

static inline size_t
vn_sizeof_VkSamplerReductionModeCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSamplerReductionModeCreateInfo_self(const VkSamplerReductionModeCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSamplerReductionMode(&val->reductionMode);
    return size;
}

static inline size_t
vn_sizeof_VkSamplerReductionModeCreateInfo(const VkSamplerReductionModeCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSamplerReductionModeCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkSamplerReductionModeCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSamplerReductionModeCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSamplerReductionModeCreateInfo_self(struct vn_cs *cs, const VkSamplerReductionModeCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkSamplerReductionMode(cs, &val->reductionMode);
}

static inline void
vn_encode_VkSamplerReductionModeCreateInfo(struct vn_cs *cs, const VkSamplerReductionModeCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO });
    vn_encode_VkSamplerReductionModeCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkSamplerReductionModeCreateInfo_self(cs, val);
}

/* struct VkSamplerCreateInfo chain */

static inline size_t
vn_sizeof_VkSamplerCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSamplerCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkSamplerYcbcrConversionInfo_self((const VkSamplerYcbcrConversionInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSamplerCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkSamplerReductionModeCreateInfo_self((const VkSamplerReductionModeCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSamplerCreateInfo_self(const VkSamplerCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkFilter(&val->magFilter);
    size += vn_sizeof_VkFilter(&val->minFilter);
    size += vn_sizeof_VkSamplerMipmapMode(&val->mipmapMode);
    size += vn_sizeof_VkSamplerAddressMode(&val->addressModeU);
    size += vn_sizeof_VkSamplerAddressMode(&val->addressModeV);
    size += vn_sizeof_VkSamplerAddressMode(&val->addressModeW);
    size += vn_sizeof_float(&val->mipLodBias);
    size += vn_sizeof_VkBool32(&val->anisotropyEnable);
    size += vn_sizeof_float(&val->maxAnisotropy);
    size += vn_sizeof_VkBool32(&val->compareEnable);
    size += vn_sizeof_VkCompareOp(&val->compareOp);
    size += vn_sizeof_float(&val->minLod);
    size += vn_sizeof_float(&val->maxLod);
    size += vn_sizeof_VkBorderColor(&val->borderColor);
    size += vn_sizeof_VkBool32(&val->unnormalizedCoordinates);
    return size;
}

static inline size_t
vn_sizeof_VkSamplerCreateInfo(const VkSamplerCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSamplerCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkSamplerCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSamplerCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSamplerCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkSamplerYcbcrConversionInfo_self(cs, (const VkSamplerYcbcrConversionInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSamplerCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkSamplerReductionModeCreateInfo_self(cs, (const VkSamplerReductionModeCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSamplerCreateInfo_self(struct vn_cs *cs, const VkSamplerCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkFilter(cs, &val->magFilter);
    vn_encode_VkFilter(cs, &val->minFilter);
    vn_encode_VkSamplerMipmapMode(cs, &val->mipmapMode);
    vn_encode_VkSamplerAddressMode(cs, &val->addressModeU);
    vn_encode_VkSamplerAddressMode(cs, &val->addressModeV);
    vn_encode_VkSamplerAddressMode(cs, &val->addressModeW);
    vn_encode_float(cs, &val->mipLodBias);
    vn_encode_VkBool32(cs, &val->anisotropyEnable);
    vn_encode_float(cs, &val->maxAnisotropy);
    vn_encode_VkBool32(cs, &val->compareEnable);
    vn_encode_VkCompareOp(cs, &val->compareOp);
    vn_encode_float(cs, &val->minLod);
    vn_encode_float(cs, &val->maxLod);
    vn_encode_VkBorderColor(cs, &val->borderColor);
    vn_encode_VkBool32(cs, &val->unnormalizedCoordinates);
}

static inline void
vn_encode_VkSamplerCreateInfo(struct vn_cs *cs, const VkSamplerCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO });
    vn_encode_VkSamplerCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkSamplerCreateInfo_self(cs, val);
}

/* struct VkCommandPoolCreateInfo chain */

static inline size_t
vn_sizeof_VkCommandPoolCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkCommandPoolCreateInfo_self(const VkCommandPoolCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->queueFamilyIndex);
    return size;
}

static inline size_t
vn_sizeof_VkCommandPoolCreateInfo(const VkCommandPoolCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkCommandPoolCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkCommandPoolCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkCommandPoolCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkCommandPoolCreateInfo_self(struct vn_cs *cs, const VkCommandPoolCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->queueFamilyIndex);
}

static inline void
vn_encode_VkCommandPoolCreateInfo(struct vn_cs *cs, const VkCommandPoolCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO });
    vn_encode_VkCommandPoolCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkCommandPoolCreateInfo_self(cs, val);
}

/* struct VkCommandBufferAllocateInfo chain */

static inline size_t
vn_sizeof_VkCommandBufferAllocateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkCommandBufferAllocateInfo_self(const VkCommandBufferAllocateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkCommandPool(&val->commandPool);
    size += vn_sizeof_VkCommandBufferLevel(&val->level);
    size += vn_sizeof_uint32_t(&val->commandBufferCount);
    return size;
}

static inline size_t
vn_sizeof_VkCommandBufferAllocateInfo(const VkCommandBufferAllocateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkCommandBufferAllocateInfo_pnext(val->pNext);
    size += vn_sizeof_VkCommandBufferAllocateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkCommandBufferAllocateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkCommandBufferAllocateInfo_self(struct vn_cs *cs, const VkCommandBufferAllocateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkCommandPool(cs, &val->commandPool);
    vn_encode_VkCommandBufferLevel(cs, &val->level);
    vn_encode_uint32_t(cs, &val->commandBufferCount);
}

static inline void
vn_encode_VkCommandBufferAllocateInfo(struct vn_cs *cs, const VkCommandBufferAllocateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO });
    vn_encode_VkCommandBufferAllocateInfo_pnext(cs, val->pNext);
    vn_encode_VkCommandBufferAllocateInfo_self(cs, val);
}

/* struct VkCommandBufferInheritanceInfo chain */

static inline size_t
vn_sizeof_VkCommandBufferInheritanceInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkCommandBufferInheritanceInfo_self(const VkCommandBufferInheritanceInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkRenderPass(&val->renderPass);
    size += vn_sizeof_uint32_t(&val->subpass);
    size += vn_sizeof_VkFramebuffer(&val->framebuffer);
    size += vn_sizeof_VkBool32(&val->occlusionQueryEnable);
    size += vn_sizeof_VkFlags(&val->queryFlags);
    size += vn_sizeof_VkFlags(&val->pipelineStatistics);
    return size;
}

static inline size_t
vn_sizeof_VkCommandBufferInheritanceInfo(const VkCommandBufferInheritanceInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkCommandBufferInheritanceInfo_pnext(val->pNext);
    size += vn_sizeof_VkCommandBufferInheritanceInfo_self(val);

    return size;
}

static inline void
vn_encode_VkCommandBufferInheritanceInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkCommandBufferInheritanceInfo_self(struct vn_cs *cs, const VkCommandBufferInheritanceInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkRenderPass(cs, &val->renderPass);
    vn_encode_uint32_t(cs, &val->subpass);
    vn_encode_VkFramebuffer(cs, &val->framebuffer);
    vn_encode_VkBool32(cs, &val->occlusionQueryEnable);
    vn_encode_VkFlags(cs, &val->queryFlags);
    vn_encode_VkFlags(cs, &val->pipelineStatistics);
}

static inline void
vn_encode_VkCommandBufferInheritanceInfo(struct vn_cs *cs, const VkCommandBufferInheritanceInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO });
    vn_encode_VkCommandBufferInheritanceInfo_pnext(cs, val->pNext);
    vn_encode_VkCommandBufferInheritanceInfo_self(cs, val);
}

static inline void
vn_decode_VkCommandBufferInheritanceInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkCommandBufferInheritanceInfo_self(struct vn_cs *cs, VkCommandBufferInheritanceInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkRenderPass(cs, &val->renderPass);
    vn_decode_uint32_t(cs, &val->subpass);
    vn_decode_VkFramebuffer(cs, &val->framebuffer);
    vn_decode_VkBool32(cs, &val->occlusionQueryEnable);
    vn_decode_VkFlags(cs, &val->queryFlags);
    vn_decode_VkFlags(cs, &val->pipelineStatistics);
}

static inline void
vn_decode_VkCommandBufferInheritanceInfo(struct vn_cs *cs, VkCommandBufferInheritanceInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);

    assert(val->sType == stype);
    vn_decode_VkCommandBufferInheritanceInfo_pnext(cs, val->pNext);
    vn_decode_VkCommandBufferInheritanceInfo_self(cs, val);
}

/* struct VkDeviceGroupCommandBufferBeginInfo chain */

static inline size_t
vn_sizeof_VkDeviceGroupCommandBufferBeginInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceGroupCommandBufferBeginInfo_self(const VkDeviceGroupCommandBufferBeginInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->deviceMask);
    return size;
}

static inline size_t
vn_sizeof_VkDeviceGroupCommandBufferBeginInfo(const VkDeviceGroupCommandBufferBeginInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceGroupCommandBufferBeginInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceGroupCommandBufferBeginInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceGroupCommandBufferBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceGroupCommandBufferBeginInfo_self(struct vn_cs *cs, const VkDeviceGroupCommandBufferBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->deviceMask);
}

static inline void
vn_encode_VkDeviceGroupCommandBufferBeginInfo(struct vn_cs *cs, const VkDeviceGroupCommandBufferBeginInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO });
    vn_encode_VkDeviceGroupCommandBufferBeginInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceGroupCommandBufferBeginInfo_self(cs, val);
}

static inline void
vn_decode_VkDeviceGroupCommandBufferBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkDeviceGroupCommandBufferBeginInfo_self(struct vn_cs *cs, VkDeviceGroupCommandBufferBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->deviceMask);
}

static inline void
vn_decode_VkDeviceGroupCommandBufferBeginInfo(struct vn_cs *cs, VkDeviceGroupCommandBufferBeginInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO);

    assert(val->sType == stype);
    vn_decode_VkDeviceGroupCommandBufferBeginInfo_pnext(cs, val->pNext);
    vn_decode_VkDeviceGroupCommandBufferBeginInfo_self(cs, val);
}

/* struct VkCommandBufferBeginInfo chain */

static inline size_t
vn_sizeof_VkCommandBufferBeginInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkCommandBufferBeginInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkDeviceGroupCommandBufferBeginInfo_self((const VkDeviceGroupCommandBufferBeginInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkCommandBufferBeginInfo_self(const VkCommandBufferBeginInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_simple_pointer(val->pInheritanceInfo);
    if (val->pInheritanceInfo)
        size += vn_sizeof_VkCommandBufferInheritanceInfo(val->pInheritanceInfo);
    return size;
}

static inline size_t
vn_sizeof_VkCommandBufferBeginInfo(const VkCommandBufferBeginInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkCommandBufferBeginInfo_pnext(val->pNext);
    size += vn_sizeof_VkCommandBufferBeginInfo_self(val);

    return size;
}

static inline void
vn_encode_VkCommandBufferBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkCommandBufferBeginInfo_pnext(cs, pnext->pNext);
            vn_encode_VkDeviceGroupCommandBufferBeginInfo_self(cs, (const VkDeviceGroupCommandBufferBeginInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkCommandBufferBeginInfo_self(struct vn_cs *cs, const VkCommandBufferBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    if (vn_encode_simple_pointer(cs, val->pInheritanceInfo))
        vn_encode_VkCommandBufferInheritanceInfo(cs, val->pInheritanceInfo);
}

static inline void
vn_encode_VkCommandBufferBeginInfo(struct vn_cs *cs, const VkCommandBufferBeginInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });
    vn_encode_VkCommandBufferBeginInfo_pnext(cs, val->pNext);
    vn_encode_VkCommandBufferBeginInfo_self(cs, val);
}

static inline void
vn_decode_VkCommandBufferBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO:
        vn_decode_VkCommandBufferBeginInfo_pnext(cs, pnext->pNext);
        vn_decode_VkDeviceGroupCommandBufferBeginInfo_self(cs, (VkDeviceGroupCommandBufferBeginInfo *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkCommandBufferBeginInfo_self(struct vn_cs *cs, VkCommandBufferBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkFlags(cs, &val->flags);
    if (vn_decode_simple_pointer(cs)) {
        vn_decode_VkCommandBufferInheritanceInfo(cs, (VkCommandBufferInheritanceInfo *)val->pInheritanceInfo);
    } else {
        val->pInheritanceInfo = NULL;
    }
}

static inline void
vn_decode_VkCommandBufferBeginInfo(struct vn_cs *cs, VkCommandBufferBeginInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);

    assert(val->sType == stype);
    vn_decode_VkCommandBufferBeginInfo_pnext(cs, val->pNext);
    vn_decode_VkCommandBufferBeginInfo_self(cs, val);
}

/* union VkClearColorValue */

static inline size_t
vn_sizeof_VkClearColorValue_tag(const VkClearColorValue *val, uint32_t tag)
{
    size_t size = vn_sizeof_uint32_t(&tag);
    switch (tag) {
    case 0:
        size += vn_sizeof_array_size(4);
    size += vn_sizeof_float_array(val->float32, 4);
        break;
    case 1:
        size += vn_sizeof_array_size(4);
    size += vn_sizeof_int32_t_array(val->int32, 4);
        break;
    case 2:
        size += vn_sizeof_array_size(4);
    size += vn_sizeof_uint32_t_array(val->uint32, 4);
        break;
    default:
        assert(false);
        break;
    }
    return size;
}

static inline size_t
vn_sizeof_VkClearColorValue(const VkClearColorValue *val)
{
    return vn_sizeof_VkClearColorValue_tag(val, 2);
}

static inline void
vn_encode_VkClearColorValue_tag(struct vn_cs *cs, const VkClearColorValue *val, uint32_t tag)
{
    vn_encode_uint32_t(cs, &tag);
    switch (tag) {
    case 0:
        vn_encode_array_size(cs, 4);
    vn_encode_float_array(cs, val->float32, 4);
        break;
    case 1:
        vn_encode_array_size(cs, 4);
    vn_encode_int32_t_array(cs, val->int32, 4);
        break;
    case 2:
        vn_encode_array_size(cs, 4);
    vn_encode_uint32_t_array(cs, val->uint32, 4);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_encode_VkClearColorValue(struct vn_cs *cs, const VkClearColorValue *val)
{
    vn_encode_VkClearColorValue_tag(cs, val, 2); /* union with default tag */
}

/* struct VkClearDepthStencilValue */

static inline size_t
vn_sizeof_VkClearDepthStencilValue(const VkClearDepthStencilValue *val)
{
    size_t size = 0;
    size += vn_sizeof_float(&val->depth);
    size += vn_sizeof_uint32_t(&val->stencil);
    return size;
}

static inline void
vn_encode_VkClearDepthStencilValue(struct vn_cs *cs, const VkClearDepthStencilValue *val)
{
    vn_encode_float(cs, &val->depth);
    vn_encode_uint32_t(cs, &val->stencil);
}

/* union VkClearValue */

static inline size_t
vn_sizeof_VkClearValue_tag(const VkClearValue *val, uint32_t tag)
{
    size_t size = vn_sizeof_uint32_t(&tag);
    switch (tag) {
    case 0:
        size += vn_sizeof_VkClearColorValue(&val->color);
        break;
    case 1:
        size += vn_sizeof_VkClearDepthStencilValue(&val->depthStencil);
        break;
    default:
        assert(false);
        break;
    }
    return size;
}

static inline size_t
vn_sizeof_VkClearValue(const VkClearValue *val)
{
    return vn_sizeof_VkClearValue_tag(val, 0);
}

static inline void
vn_encode_VkClearValue_tag(struct vn_cs *cs, const VkClearValue *val, uint32_t tag)
{
    vn_encode_uint32_t(cs, &tag);
    switch (tag) {
    case 0:
        vn_encode_VkClearColorValue(cs, &val->color);
        break;
    case 1:
        vn_encode_VkClearDepthStencilValue(cs, &val->depthStencil);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_encode_VkClearValue(struct vn_cs *cs, const VkClearValue *val)
{
    vn_encode_VkClearValue_tag(cs, val, 0); /* union with default tag */
}

/* struct VkDeviceGroupRenderPassBeginInfo chain */

static inline size_t
vn_sizeof_VkDeviceGroupRenderPassBeginInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceGroupRenderPassBeginInfo_self(const VkDeviceGroupRenderPassBeginInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->deviceMask);
    size += vn_sizeof_uint32_t(&val->deviceRenderAreaCount);
    if (val->pDeviceRenderAreas) {
        size += vn_sizeof_array_size(val->deviceRenderAreaCount);
        for (uint32_t i = 0; i < val->deviceRenderAreaCount; i++)
            size += vn_sizeof_VkRect2D(&val->pDeviceRenderAreas[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDeviceGroupRenderPassBeginInfo(const VkDeviceGroupRenderPassBeginInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceGroupRenderPassBeginInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceGroupRenderPassBeginInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceGroupRenderPassBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceGroupRenderPassBeginInfo_self(struct vn_cs *cs, const VkDeviceGroupRenderPassBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->deviceMask);
    vn_encode_uint32_t(cs, &val->deviceRenderAreaCount);
    if (val->pDeviceRenderAreas) {
        vn_encode_array_size(cs, val->deviceRenderAreaCount);
        for (uint32_t i = 0; i < val->deviceRenderAreaCount; i++)
            vn_encode_VkRect2D(cs, &val->pDeviceRenderAreas[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDeviceGroupRenderPassBeginInfo(struct vn_cs *cs, const VkDeviceGroupRenderPassBeginInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO });
    vn_encode_VkDeviceGroupRenderPassBeginInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceGroupRenderPassBeginInfo_self(cs, val);
}

/* struct VkRenderPassAttachmentBeginInfo chain */

static inline size_t
vn_sizeof_VkRenderPassAttachmentBeginInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkRenderPassAttachmentBeginInfo_self(const VkRenderPassAttachmentBeginInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->attachmentCount);
    if (val->pAttachments) {
        size += vn_sizeof_array_size(val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            size += vn_sizeof_VkImageView(&val->pAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkRenderPassAttachmentBeginInfo(const VkRenderPassAttachmentBeginInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkRenderPassAttachmentBeginInfo_pnext(val->pNext);
    size += vn_sizeof_VkRenderPassAttachmentBeginInfo_self(val);

    return size;
}

static inline void
vn_encode_VkRenderPassAttachmentBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkRenderPassAttachmentBeginInfo_self(struct vn_cs *cs, const VkRenderPassAttachmentBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->attachmentCount);
    if (val->pAttachments) {
        vn_encode_array_size(cs, val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            vn_encode_VkImageView(cs, &val->pAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkRenderPassAttachmentBeginInfo(struct vn_cs *cs, const VkRenderPassAttachmentBeginInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO });
    vn_encode_VkRenderPassAttachmentBeginInfo_pnext(cs, val->pNext);
    vn_encode_VkRenderPassAttachmentBeginInfo_self(cs, val);
}

/* struct VkRenderPassBeginInfo chain */

static inline size_t
vn_sizeof_VkRenderPassBeginInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkRenderPassBeginInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkDeviceGroupRenderPassBeginInfo_self((const VkDeviceGroupRenderPassBeginInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkRenderPassBeginInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkRenderPassAttachmentBeginInfo_self((const VkRenderPassAttachmentBeginInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkRenderPassBeginInfo_self(const VkRenderPassBeginInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkRenderPass(&val->renderPass);
    size += vn_sizeof_VkFramebuffer(&val->framebuffer);
    size += vn_sizeof_VkRect2D(&val->renderArea);
    size += vn_sizeof_uint32_t(&val->clearValueCount);
    if (val->pClearValues) {
        size += vn_sizeof_array_size(val->clearValueCount);
        for (uint32_t i = 0; i < val->clearValueCount; i++)
            size += vn_sizeof_VkClearValue(&val->pClearValues[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkRenderPassBeginInfo(const VkRenderPassBeginInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkRenderPassBeginInfo_pnext(val->pNext);
    size += vn_sizeof_VkRenderPassBeginInfo_self(val);

    return size;
}

static inline void
vn_encode_VkRenderPassBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkRenderPassBeginInfo_pnext(cs, pnext->pNext);
            vn_encode_VkDeviceGroupRenderPassBeginInfo_self(cs, (const VkDeviceGroupRenderPassBeginInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkRenderPassBeginInfo_pnext(cs, pnext->pNext);
            vn_encode_VkRenderPassAttachmentBeginInfo_self(cs, (const VkRenderPassAttachmentBeginInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkRenderPassBeginInfo_self(struct vn_cs *cs, const VkRenderPassBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkRenderPass(cs, &val->renderPass);
    vn_encode_VkFramebuffer(cs, &val->framebuffer);
    vn_encode_VkRect2D(cs, &val->renderArea);
    vn_encode_uint32_t(cs, &val->clearValueCount);
    if (val->pClearValues) {
        vn_encode_array_size(cs, val->clearValueCount);
        for (uint32_t i = 0; i < val->clearValueCount; i++)
            vn_encode_VkClearValue(cs, &val->pClearValues[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkRenderPassBeginInfo(struct vn_cs *cs, const VkRenderPassBeginInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO });
    vn_encode_VkRenderPassBeginInfo_pnext(cs, val->pNext);
    vn_encode_VkRenderPassBeginInfo_self(cs, val);
}

/* struct VkClearAttachment */

static inline size_t
vn_sizeof_VkClearAttachment(const VkClearAttachment *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->aspectMask);
    size += vn_sizeof_uint32_t(&val->colorAttachment);
    size += vn_sizeof_VkClearValue(&val->clearValue);
    return size;
}

static inline void
vn_encode_VkClearAttachment(struct vn_cs *cs, const VkClearAttachment *val)
{
    vn_encode_VkFlags(cs, &val->aspectMask);
    vn_encode_uint32_t(cs, &val->colorAttachment);
    vn_encode_VkClearValue(cs, &val->clearValue);
}

/* struct VkAttachmentDescription */

static inline size_t
vn_sizeof_VkAttachmentDescription(const VkAttachmentDescription *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkSampleCountFlagBits(&val->samples);
    size += vn_sizeof_VkAttachmentLoadOp(&val->loadOp);
    size += vn_sizeof_VkAttachmentStoreOp(&val->storeOp);
    size += vn_sizeof_VkAttachmentLoadOp(&val->stencilLoadOp);
    size += vn_sizeof_VkAttachmentStoreOp(&val->stencilStoreOp);
    size += vn_sizeof_VkImageLayout(&val->initialLayout);
    size += vn_sizeof_VkImageLayout(&val->finalLayout);
    return size;
}

static inline void
vn_encode_VkAttachmentDescription(struct vn_cs *cs, const VkAttachmentDescription *val)
{
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkSampleCountFlagBits(cs, &val->samples);
    vn_encode_VkAttachmentLoadOp(cs, &val->loadOp);
    vn_encode_VkAttachmentStoreOp(cs, &val->storeOp);
    vn_encode_VkAttachmentLoadOp(cs, &val->stencilLoadOp);
    vn_encode_VkAttachmentStoreOp(cs, &val->stencilStoreOp);
    vn_encode_VkImageLayout(cs, &val->initialLayout);
    vn_encode_VkImageLayout(cs, &val->finalLayout);
}

/* struct VkAttachmentReference */

static inline size_t
vn_sizeof_VkAttachmentReference(const VkAttachmentReference *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->attachment);
    size += vn_sizeof_VkImageLayout(&val->layout);
    return size;
}

static inline void
vn_encode_VkAttachmentReference(struct vn_cs *cs, const VkAttachmentReference *val)
{
    vn_encode_uint32_t(cs, &val->attachment);
    vn_encode_VkImageLayout(cs, &val->layout);
}

/* struct VkSubpassDescription */

static inline size_t
vn_sizeof_VkSubpassDescription(const VkSubpassDescription *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkPipelineBindPoint(&val->pipelineBindPoint);
    size += vn_sizeof_uint32_t(&val->inputAttachmentCount);
    if (val->pInputAttachments) {
        size += vn_sizeof_array_size(val->inputAttachmentCount);
        for (uint32_t i = 0; i < val->inputAttachmentCount; i++)
            size += vn_sizeof_VkAttachmentReference(&val->pInputAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->colorAttachmentCount);
    if (val->pColorAttachments) {
        size += vn_sizeof_array_size(val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            size += vn_sizeof_VkAttachmentReference(&val->pColorAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    if (val->pResolveAttachments) {
        size += vn_sizeof_array_size(val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            size += vn_sizeof_VkAttachmentReference(&val->pResolveAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_simple_pointer(val->pDepthStencilAttachment);
    if (val->pDepthStencilAttachment)
        size += vn_sizeof_VkAttachmentReference(val->pDepthStencilAttachment);
    size += vn_sizeof_uint32_t(&val->preserveAttachmentCount);
    if (val->pPreserveAttachments) {
        size += vn_sizeof_array_size(val->preserveAttachmentCount);
        size += vn_sizeof_uint32_t_array(val->pPreserveAttachments, val->preserveAttachmentCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline void
vn_encode_VkSubpassDescription(struct vn_cs *cs, const VkSubpassDescription *val)
{
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkPipelineBindPoint(cs, &val->pipelineBindPoint);
    vn_encode_uint32_t(cs, &val->inputAttachmentCount);
    if (val->pInputAttachments) {
        vn_encode_array_size(cs, val->inputAttachmentCount);
        for (uint32_t i = 0; i < val->inputAttachmentCount; i++)
            vn_encode_VkAttachmentReference(cs, &val->pInputAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->colorAttachmentCount);
    if (val->pColorAttachments) {
        vn_encode_array_size(cs, val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            vn_encode_VkAttachmentReference(cs, &val->pColorAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (val->pResolveAttachments) {
        vn_encode_array_size(cs, val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            vn_encode_VkAttachmentReference(cs, &val->pResolveAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (vn_encode_simple_pointer(cs, val->pDepthStencilAttachment))
        vn_encode_VkAttachmentReference(cs, val->pDepthStencilAttachment);
    vn_encode_uint32_t(cs, &val->preserveAttachmentCount);
    if (val->pPreserveAttachments) {
        vn_encode_array_size(cs, val->preserveAttachmentCount);
        vn_encode_uint32_t_array(cs, val->pPreserveAttachments, val->preserveAttachmentCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

/* struct VkSubpassDependency */

static inline size_t
vn_sizeof_VkSubpassDependency(const VkSubpassDependency *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->srcSubpass);
    size += vn_sizeof_uint32_t(&val->dstSubpass);
    size += vn_sizeof_VkFlags(&val->srcStageMask);
    size += vn_sizeof_VkFlags(&val->dstStageMask);
    size += vn_sizeof_VkFlags(&val->srcAccessMask);
    size += vn_sizeof_VkFlags(&val->dstAccessMask);
    size += vn_sizeof_VkFlags(&val->dependencyFlags);
    return size;
}

static inline void
vn_encode_VkSubpassDependency(struct vn_cs *cs, const VkSubpassDependency *val)
{
    vn_encode_uint32_t(cs, &val->srcSubpass);
    vn_encode_uint32_t(cs, &val->dstSubpass);
    vn_encode_VkFlags(cs, &val->srcStageMask);
    vn_encode_VkFlags(cs, &val->dstStageMask);
    vn_encode_VkFlags(cs, &val->srcAccessMask);
    vn_encode_VkFlags(cs, &val->dstAccessMask);
    vn_encode_VkFlags(cs, &val->dependencyFlags);
}

/* struct VkRenderPassMultiviewCreateInfo chain */

static inline size_t
vn_sizeof_VkRenderPassMultiviewCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkRenderPassMultiviewCreateInfo_self(const VkRenderPassMultiviewCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->subpassCount);
    if (val->pViewMasks) {
        size += vn_sizeof_array_size(val->subpassCount);
        size += vn_sizeof_uint32_t_array(val->pViewMasks, val->subpassCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->dependencyCount);
    if (val->pViewOffsets) {
        size += vn_sizeof_array_size(val->dependencyCount);
        size += vn_sizeof_int32_t_array(val->pViewOffsets, val->dependencyCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->correlationMaskCount);
    if (val->pCorrelationMasks) {
        size += vn_sizeof_array_size(val->correlationMaskCount);
        size += vn_sizeof_uint32_t_array(val->pCorrelationMasks, val->correlationMaskCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkRenderPassMultiviewCreateInfo(const VkRenderPassMultiviewCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkRenderPassMultiviewCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkRenderPassMultiviewCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkRenderPassMultiviewCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkRenderPassMultiviewCreateInfo_self(struct vn_cs *cs, const VkRenderPassMultiviewCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->subpassCount);
    if (val->pViewMasks) {
        vn_encode_array_size(cs, val->subpassCount);
        vn_encode_uint32_t_array(cs, val->pViewMasks, val->subpassCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->dependencyCount);
    if (val->pViewOffsets) {
        vn_encode_array_size(cs, val->dependencyCount);
        vn_encode_int32_t_array(cs, val->pViewOffsets, val->dependencyCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->correlationMaskCount);
    if (val->pCorrelationMasks) {
        vn_encode_array_size(cs, val->correlationMaskCount);
        vn_encode_uint32_t_array(cs, val->pCorrelationMasks, val->correlationMaskCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkRenderPassMultiviewCreateInfo(struct vn_cs *cs, const VkRenderPassMultiviewCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO });
    vn_encode_VkRenderPassMultiviewCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkRenderPassMultiviewCreateInfo_self(cs, val);
}

/* struct VkInputAttachmentAspectReference */

static inline size_t
vn_sizeof_VkInputAttachmentAspectReference(const VkInputAttachmentAspectReference *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->subpass);
    size += vn_sizeof_uint32_t(&val->inputAttachmentIndex);
    size += vn_sizeof_VkFlags(&val->aspectMask);
    return size;
}

static inline void
vn_encode_VkInputAttachmentAspectReference(struct vn_cs *cs, const VkInputAttachmentAspectReference *val)
{
    vn_encode_uint32_t(cs, &val->subpass);
    vn_encode_uint32_t(cs, &val->inputAttachmentIndex);
    vn_encode_VkFlags(cs, &val->aspectMask);
}

/* struct VkRenderPassInputAttachmentAspectCreateInfo chain */

static inline size_t
vn_sizeof_VkRenderPassInputAttachmentAspectCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkRenderPassInputAttachmentAspectCreateInfo_self(const VkRenderPassInputAttachmentAspectCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->aspectReferenceCount);
    if (val->pAspectReferences) {
        size += vn_sizeof_array_size(val->aspectReferenceCount);
        for (uint32_t i = 0; i < val->aspectReferenceCount; i++)
            size += vn_sizeof_VkInputAttachmentAspectReference(&val->pAspectReferences[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkRenderPassInputAttachmentAspectCreateInfo(const VkRenderPassInputAttachmentAspectCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkRenderPassInputAttachmentAspectCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkRenderPassInputAttachmentAspectCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkRenderPassInputAttachmentAspectCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkRenderPassInputAttachmentAspectCreateInfo_self(struct vn_cs *cs, const VkRenderPassInputAttachmentAspectCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->aspectReferenceCount);
    if (val->pAspectReferences) {
        vn_encode_array_size(cs, val->aspectReferenceCount);
        for (uint32_t i = 0; i < val->aspectReferenceCount; i++)
            vn_encode_VkInputAttachmentAspectReference(cs, &val->pAspectReferences[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkRenderPassInputAttachmentAspectCreateInfo(struct vn_cs *cs, const VkRenderPassInputAttachmentAspectCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO });
    vn_encode_VkRenderPassInputAttachmentAspectCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkRenderPassInputAttachmentAspectCreateInfo_self(cs, val);
}

/* struct VkRenderPassCreateInfo chain */

static inline size_t
vn_sizeof_VkRenderPassCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkRenderPassCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkRenderPassMultiviewCreateInfo_self((const VkRenderPassMultiviewCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkRenderPassCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkRenderPassInputAttachmentAspectCreateInfo_self((const VkRenderPassInputAttachmentAspectCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkRenderPassCreateInfo_self(const VkRenderPassCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->attachmentCount);
    if (val->pAttachments) {
        size += vn_sizeof_array_size(val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            size += vn_sizeof_VkAttachmentDescription(&val->pAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->subpassCount);
    if (val->pSubpasses) {
        size += vn_sizeof_array_size(val->subpassCount);
        for (uint32_t i = 0; i < val->subpassCount; i++)
            size += vn_sizeof_VkSubpassDescription(&val->pSubpasses[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->dependencyCount);
    if (val->pDependencies) {
        size += vn_sizeof_array_size(val->dependencyCount);
        for (uint32_t i = 0; i < val->dependencyCount; i++)
            size += vn_sizeof_VkSubpassDependency(&val->pDependencies[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkRenderPassCreateInfo(const VkRenderPassCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkRenderPassCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkRenderPassCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkRenderPassCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkRenderPassCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkRenderPassMultiviewCreateInfo_self(cs, (const VkRenderPassMultiviewCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkRenderPassCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkRenderPassInputAttachmentAspectCreateInfo_self(cs, (const VkRenderPassInputAttachmentAspectCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkRenderPassCreateInfo_self(struct vn_cs *cs, const VkRenderPassCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->attachmentCount);
    if (val->pAttachments) {
        vn_encode_array_size(cs, val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            vn_encode_VkAttachmentDescription(cs, &val->pAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->subpassCount);
    if (val->pSubpasses) {
        vn_encode_array_size(cs, val->subpassCount);
        for (uint32_t i = 0; i < val->subpassCount; i++)
            vn_encode_VkSubpassDescription(cs, &val->pSubpasses[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->dependencyCount);
    if (val->pDependencies) {
        vn_encode_array_size(cs, val->dependencyCount);
        for (uint32_t i = 0; i < val->dependencyCount; i++)
            vn_encode_VkSubpassDependency(cs, &val->pDependencies[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkRenderPassCreateInfo(struct vn_cs *cs, const VkRenderPassCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO });
    vn_encode_VkRenderPassCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkRenderPassCreateInfo_self(cs, val);
}

/* struct VkEventCreateInfo chain */

static inline size_t
vn_sizeof_VkEventCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkEventCreateInfo_self(const VkEventCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline size_t
vn_sizeof_VkEventCreateInfo(const VkEventCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkEventCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkEventCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkEventCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkEventCreateInfo_self(struct vn_cs *cs, const VkEventCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
}

static inline void
vn_encode_VkEventCreateInfo(struct vn_cs *cs, const VkEventCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EVENT_CREATE_INFO });
    vn_encode_VkEventCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkEventCreateInfo_self(cs, val);
}

/* struct VkExportFenceCreateInfo chain */

static inline size_t
vn_sizeof_VkExportFenceCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExportFenceCreateInfo_self(const VkExportFenceCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->handleTypes);
    return size;
}

static inline size_t
vn_sizeof_VkExportFenceCreateInfo(const VkExportFenceCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExportFenceCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkExportFenceCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkExportFenceCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExportFenceCreateInfo_self(struct vn_cs *cs, const VkExportFenceCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->handleTypes);
}

static inline void
vn_encode_VkExportFenceCreateInfo(struct vn_cs *cs, const VkExportFenceCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO });
    vn_encode_VkExportFenceCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkExportFenceCreateInfo_self(cs, val);
}

/* struct VkFenceCreateInfo chain */

static inline size_t
vn_sizeof_VkFenceCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkFenceCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkExportFenceCreateInfo_self((const VkExportFenceCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkFenceCreateInfo_self(const VkFenceCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline size_t
vn_sizeof_VkFenceCreateInfo(const VkFenceCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkFenceCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkFenceCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkFenceCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkFenceCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkExportFenceCreateInfo_self(cs, (const VkExportFenceCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkFenceCreateInfo_self(struct vn_cs *cs, const VkFenceCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
}

static inline void
vn_encode_VkFenceCreateInfo(struct vn_cs *cs, const VkFenceCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO });
    vn_encode_VkFenceCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkFenceCreateInfo_self(cs, val);
}

/* struct VkExportSemaphoreCreateInfo chain */

static inline size_t
vn_sizeof_VkExportSemaphoreCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExportSemaphoreCreateInfo_self(const VkExportSemaphoreCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->handleTypes);
    return size;
}

static inline size_t
vn_sizeof_VkExportSemaphoreCreateInfo(const VkExportSemaphoreCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExportSemaphoreCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkExportSemaphoreCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkExportSemaphoreCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExportSemaphoreCreateInfo_self(struct vn_cs *cs, const VkExportSemaphoreCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->handleTypes);
}

static inline void
vn_encode_VkExportSemaphoreCreateInfo(struct vn_cs *cs, const VkExportSemaphoreCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO });
    vn_encode_VkExportSemaphoreCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkExportSemaphoreCreateInfo_self(cs, val);
}

/* struct VkSemaphoreTypeCreateInfo chain */

static inline size_t
vn_sizeof_VkSemaphoreTypeCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSemaphoreTypeCreateInfo_self(const VkSemaphoreTypeCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSemaphoreType(&val->semaphoreType);
    size += vn_sizeof_uint64_t(&val->initialValue);
    return size;
}

static inline size_t
vn_sizeof_VkSemaphoreTypeCreateInfo(const VkSemaphoreTypeCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSemaphoreTypeCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkSemaphoreTypeCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSemaphoreTypeCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSemaphoreTypeCreateInfo_self(struct vn_cs *cs, const VkSemaphoreTypeCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkSemaphoreType(cs, &val->semaphoreType);
    vn_encode_uint64_t(cs, &val->initialValue);
}

static inline void
vn_encode_VkSemaphoreTypeCreateInfo(struct vn_cs *cs, const VkSemaphoreTypeCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO });
    vn_encode_VkSemaphoreTypeCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkSemaphoreTypeCreateInfo_self(cs, val);
}

/* struct VkSemaphoreCreateInfo chain */

static inline size_t
vn_sizeof_VkSemaphoreCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSemaphoreCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkExportSemaphoreCreateInfo_self((const VkExportSemaphoreCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSemaphoreCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkSemaphoreTypeCreateInfo_self((const VkSemaphoreTypeCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSemaphoreCreateInfo_self(const VkSemaphoreCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline size_t
vn_sizeof_VkSemaphoreCreateInfo(const VkSemaphoreCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSemaphoreCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkSemaphoreCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSemaphoreCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSemaphoreCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkExportSemaphoreCreateInfo_self(cs, (const VkExportSemaphoreCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSemaphoreCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkSemaphoreTypeCreateInfo_self(cs, (const VkSemaphoreTypeCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSemaphoreCreateInfo_self(struct vn_cs *cs, const VkSemaphoreCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
}

static inline void
vn_encode_VkSemaphoreCreateInfo(struct vn_cs *cs, const VkSemaphoreCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO });
    vn_encode_VkSemaphoreCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkSemaphoreCreateInfo_self(cs, val);
}

/* struct VkQueryPoolCreateInfo chain */

static inline size_t
vn_sizeof_VkQueryPoolCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkQueryPoolCreateInfo_self(const VkQueryPoolCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkQueryType(&val->queryType);
    size += vn_sizeof_uint32_t(&val->queryCount);
    size += vn_sizeof_VkFlags(&val->pipelineStatistics);
    return size;
}

static inline size_t
vn_sizeof_VkQueryPoolCreateInfo(const VkQueryPoolCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkQueryPoolCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkQueryPoolCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkQueryPoolCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkQueryPoolCreateInfo_self(struct vn_cs *cs, const VkQueryPoolCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkQueryType(cs, &val->queryType);
    vn_encode_uint32_t(cs, &val->queryCount);
    vn_encode_VkFlags(cs, &val->pipelineStatistics);
}

static inline void
vn_encode_VkQueryPoolCreateInfo(struct vn_cs *cs, const VkQueryPoolCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO });
    vn_encode_VkQueryPoolCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkQueryPoolCreateInfo_self(cs, val);
}

/* struct VkFramebufferAttachmentImageInfo chain */

static inline size_t
vn_sizeof_VkFramebufferAttachmentImageInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkFramebufferAttachmentImageInfo_self(const VkFramebufferAttachmentImageInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkFlags(&val->usage);
    size += vn_sizeof_uint32_t(&val->width);
    size += vn_sizeof_uint32_t(&val->height);
    size += vn_sizeof_uint32_t(&val->layerCount);
    size += vn_sizeof_uint32_t(&val->viewFormatCount);
    if (val->pViewFormats) {
        size += vn_sizeof_array_size(val->viewFormatCount);
        size += vn_sizeof_VkFormat_array(val->pViewFormats, val->viewFormatCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkFramebufferAttachmentImageInfo(const VkFramebufferAttachmentImageInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkFramebufferAttachmentImageInfo_pnext(val->pNext);
    size += vn_sizeof_VkFramebufferAttachmentImageInfo_self(val);

    return size;
}

static inline void
vn_encode_VkFramebufferAttachmentImageInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkFramebufferAttachmentImageInfo_self(struct vn_cs *cs, const VkFramebufferAttachmentImageInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkFlags(cs, &val->usage);
    vn_encode_uint32_t(cs, &val->width);
    vn_encode_uint32_t(cs, &val->height);
    vn_encode_uint32_t(cs, &val->layerCount);
    vn_encode_uint32_t(cs, &val->viewFormatCount);
    if (val->pViewFormats) {
        vn_encode_array_size(cs, val->viewFormatCount);
        vn_encode_VkFormat_array(cs, val->pViewFormats, val->viewFormatCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkFramebufferAttachmentImageInfo(struct vn_cs *cs, const VkFramebufferAttachmentImageInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO });
    vn_encode_VkFramebufferAttachmentImageInfo_pnext(cs, val->pNext);
    vn_encode_VkFramebufferAttachmentImageInfo_self(cs, val);
}

/* struct VkFramebufferAttachmentsCreateInfo chain */

static inline size_t
vn_sizeof_VkFramebufferAttachmentsCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkFramebufferAttachmentsCreateInfo_self(const VkFramebufferAttachmentsCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->attachmentImageInfoCount);
    if (val->pAttachmentImageInfos) {
        size += vn_sizeof_array_size(val->attachmentImageInfoCount);
        for (uint32_t i = 0; i < val->attachmentImageInfoCount; i++)
            size += vn_sizeof_VkFramebufferAttachmentImageInfo(&val->pAttachmentImageInfos[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkFramebufferAttachmentsCreateInfo(const VkFramebufferAttachmentsCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkFramebufferAttachmentsCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkFramebufferAttachmentsCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkFramebufferAttachmentsCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkFramebufferAttachmentsCreateInfo_self(struct vn_cs *cs, const VkFramebufferAttachmentsCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->attachmentImageInfoCount);
    if (val->pAttachmentImageInfos) {
        vn_encode_array_size(cs, val->attachmentImageInfoCount);
        for (uint32_t i = 0; i < val->attachmentImageInfoCount; i++)
            vn_encode_VkFramebufferAttachmentImageInfo(cs, &val->pAttachmentImageInfos[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkFramebufferAttachmentsCreateInfo(struct vn_cs *cs, const VkFramebufferAttachmentsCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO });
    vn_encode_VkFramebufferAttachmentsCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkFramebufferAttachmentsCreateInfo_self(cs, val);
}

/* struct VkFramebufferCreateInfo chain */

static inline size_t
vn_sizeof_VkFramebufferCreateInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkFramebufferCreateInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkFramebufferAttachmentsCreateInfo_self((const VkFramebufferAttachmentsCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkFramebufferCreateInfo_self(const VkFramebufferCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkRenderPass(&val->renderPass);
    size += vn_sizeof_uint32_t(&val->attachmentCount);
    if (val->pAttachments) {
        size += vn_sizeof_array_size(val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            size += vn_sizeof_VkImageView(&val->pAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->width);
    size += vn_sizeof_uint32_t(&val->height);
    size += vn_sizeof_uint32_t(&val->layers);
    return size;
}

static inline size_t
vn_sizeof_VkFramebufferCreateInfo(const VkFramebufferCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkFramebufferCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkFramebufferCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkFramebufferCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkFramebufferCreateInfo_pnext(cs, pnext->pNext);
            vn_encode_VkFramebufferAttachmentsCreateInfo_self(cs, (const VkFramebufferAttachmentsCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkFramebufferCreateInfo_self(struct vn_cs *cs, const VkFramebufferCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkRenderPass(cs, &val->renderPass);
    vn_encode_uint32_t(cs, &val->attachmentCount);
    if (val->pAttachments) {
        vn_encode_array_size(cs, val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            vn_encode_VkImageView(cs, &val->pAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->width);
    vn_encode_uint32_t(cs, &val->height);
    vn_encode_uint32_t(cs, &val->layers);
}

static inline void
vn_encode_VkFramebufferCreateInfo(struct vn_cs *cs, const VkFramebufferCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO });
    vn_encode_VkFramebufferCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkFramebufferCreateInfo_self(cs, val);
}

/* struct VkDrawIndirectCommand */

static inline size_t
vn_sizeof_VkDrawIndirectCommand(const VkDrawIndirectCommand *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->vertexCount);
    size += vn_sizeof_uint32_t(&val->instanceCount);
    size += vn_sizeof_uint32_t(&val->firstVertex);
    size += vn_sizeof_uint32_t(&val->firstInstance);
    return size;
}

/* struct VkDrawIndexedIndirectCommand */

static inline size_t
vn_sizeof_VkDrawIndexedIndirectCommand(const VkDrawIndexedIndirectCommand *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->indexCount);
    size += vn_sizeof_uint32_t(&val->instanceCount);
    size += vn_sizeof_uint32_t(&val->firstIndex);
    size += vn_sizeof_int32_t(&val->vertexOffset);
    size += vn_sizeof_uint32_t(&val->firstInstance);
    return size;
}

/* struct VkDispatchIndirectCommand */

static inline size_t
vn_sizeof_VkDispatchIndirectCommand(const VkDispatchIndirectCommand *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->x);
    size += vn_sizeof_uint32_t(&val->y);
    size += vn_sizeof_uint32_t(&val->z);
    return size;
}

/* struct VkDeviceGroupSubmitInfo chain */

static inline size_t
vn_sizeof_VkDeviceGroupSubmitInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceGroupSubmitInfo_self(const VkDeviceGroupSubmitInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->waitSemaphoreCount);
    if (val->pWaitSemaphoreDeviceIndices) {
        size += vn_sizeof_array_size(val->waitSemaphoreCount);
        size += vn_sizeof_uint32_t_array(val->pWaitSemaphoreDeviceIndices, val->waitSemaphoreCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->commandBufferCount);
    if (val->pCommandBufferDeviceMasks) {
        size += vn_sizeof_array_size(val->commandBufferCount);
        size += vn_sizeof_uint32_t_array(val->pCommandBufferDeviceMasks, val->commandBufferCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->signalSemaphoreCount);
    if (val->pSignalSemaphoreDeviceIndices) {
        size += vn_sizeof_array_size(val->signalSemaphoreCount);
        size += vn_sizeof_uint32_t_array(val->pSignalSemaphoreDeviceIndices, val->signalSemaphoreCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDeviceGroupSubmitInfo(const VkDeviceGroupSubmitInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceGroupSubmitInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceGroupSubmitInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceGroupSubmitInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceGroupSubmitInfo_self(struct vn_cs *cs, const VkDeviceGroupSubmitInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->waitSemaphoreCount);
    if (val->pWaitSemaphoreDeviceIndices) {
        vn_encode_array_size(cs, val->waitSemaphoreCount);
        vn_encode_uint32_t_array(cs, val->pWaitSemaphoreDeviceIndices, val->waitSemaphoreCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->commandBufferCount);
    if (val->pCommandBufferDeviceMasks) {
        vn_encode_array_size(cs, val->commandBufferCount);
        vn_encode_uint32_t_array(cs, val->pCommandBufferDeviceMasks, val->commandBufferCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->signalSemaphoreCount);
    if (val->pSignalSemaphoreDeviceIndices) {
        vn_encode_array_size(cs, val->signalSemaphoreCount);
        vn_encode_uint32_t_array(cs, val->pSignalSemaphoreDeviceIndices, val->signalSemaphoreCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDeviceGroupSubmitInfo(struct vn_cs *cs, const VkDeviceGroupSubmitInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO });
    vn_encode_VkDeviceGroupSubmitInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceGroupSubmitInfo_self(cs, val);
}

/* struct VkProtectedSubmitInfo chain */

static inline size_t
vn_sizeof_VkProtectedSubmitInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkProtectedSubmitInfo_self(const VkProtectedSubmitInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->protectedSubmit);
    return size;
}

static inline size_t
vn_sizeof_VkProtectedSubmitInfo(const VkProtectedSubmitInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkProtectedSubmitInfo_pnext(val->pNext);
    size += vn_sizeof_VkProtectedSubmitInfo_self(val);

    return size;
}

static inline void
vn_encode_VkProtectedSubmitInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkProtectedSubmitInfo_self(struct vn_cs *cs, const VkProtectedSubmitInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBool32(cs, &val->protectedSubmit);
}

static inline void
vn_encode_VkProtectedSubmitInfo(struct vn_cs *cs, const VkProtectedSubmitInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO });
    vn_encode_VkProtectedSubmitInfo_pnext(cs, val->pNext);
    vn_encode_VkProtectedSubmitInfo_self(cs, val);
}

/* struct VkSubmitInfo chain */

static inline size_t
vn_sizeof_VkSubmitInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSubmitInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkDeviceGroupSubmitInfo_self((const VkDeviceGroupSubmitInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSubmitInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkProtectedSubmitInfo_self((const VkProtectedSubmitInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSubmitInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkTimelineSemaphoreSubmitInfo_self((const VkTimelineSemaphoreSubmitInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSubmitInfo_self(const VkSubmitInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->waitSemaphoreCount);
    if (val->pWaitSemaphores) {
        size += vn_sizeof_array_size(val->waitSemaphoreCount);
        for (uint32_t i = 0; i < val->waitSemaphoreCount; i++)
            size += vn_sizeof_VkSemaphore(&val->pWaitSemaphores[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    if (val->pWaitDstStageMask) {
        size += vn_sizeof_array_size(val->waitSemaphoreCount);
        for (uint32_t i = 0; i < val->waitSemaphoreCount; i++)
            size += vn_sizeof_VkFlags(&val->pWaitDstStageMask[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->commandBufferCount);
    if (val->pCommandBuffers) {
        size += vn_sizeof_array_size(val->commandBufferCount);
        for (uint32_t i = 0; i < val->commandBufferCount; i++)
            size += vn_sizeof_VkCommandBuffer(&val->pCommandBuffers[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->signalSemaphoreCount);
    if (val->pSignalSemaphores) {
        size += vn_sizeof_array_size(val->signalSemaphoreCount);
        for (uint32_t i = 0; i < val->signalSemaphoreCount; i++)
            size += vn_sizeof_VkSemaphore(&val->pSignalSemaphores[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkSubmitInfo(const VkSubmitInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSubmitInfo_pnext(val->pNext);
    size += vn_sizeof_VkSubmitInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSubmitInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSubmitInfo_pnext(cs, pnext->pNext);
            vn_encode_VkDeviceGroupSubmitInfo_self(cs, (const VkDeviceGroupSubmitInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSubmitInfo_pnext(cs, pnext->pNext);
            vn_encode_VkProtectedSubmitInfo_self(cs, (const VkProtectedSubmitInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSubmitInfo_pnext(cs, pnext->pNext);
            vn_encode_VkTimelineSemaphoreSubmitInfo_self(cs, (const VkTimelineSemaphoreSubmitInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSubmitInfo_self(struct vn_cs *cs, const VkSubmitInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->waitSemaphoreCount);
    if (val->pWaitSemaphores) {
        vn_encode_array_size(cs, val->waitSemaphoreCount);
        for (uint32_t i = 0; i < val->waitSemaphoreCount; i++)
            vn_encode_VkSemaphore(cs, &val->pWaitSemaphores[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (val->pWaitDstStageMask) {
        vn_encode_array_size(cs, val->waitSemaphoreCount);
        for (uint32_t i = 0; i < val->waitSemaphoreCount; i++)
            vn_encode_VkFlags(cs, &val->pWaitDstStageMask[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->commandBufferCount);
    if (val->pCommandBuffers) {
        vn_encode_array_size(cs, val->commandBufferCount);
        for (uint32_t i = 0; i < val->commandBufferCount; i++)
            vn_encode_VkCommandBuffer(cs, &val->pCommandBuffers[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->signalSemaphoreCount);
    if (val->pSignalSemaphores) {
        vn_encode_array_size(cs, val->signalSemaphoreCount);
        for (uint32_t i = 0; i < val->signalSemaphoreCount; i++)
            vn_encode_VkSemaphore(cs, &val->pSignalSemaphores[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkSubmitInfo(struct vn_cs *cs, const VkSubmitInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SUBMIT_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SUBMIT_INFO });
    vn_encode_VkSubmitInfo_pnext(cs, val->pNext);
    vn_encode_VkSubmitInfo_self(cs, val);
}

/* struct VkConformanceVersion */

static inline size_t
vn_sizeof_VkConformanceVersion(const VkConformanceVersion *val)
{
    size_t size = 0;
    size += vn_sizeof_uint8_t(&val->major);
    size += vn_sizeof_uint8_t(&val->minor);
    size += vn_sizeof_uint8_t(&val->subminor);
    size += vn_sizeof_uint8_t(&val->patch);
    return size;
}

static inline void
vn_decode_VkConformanceVersion(struct vn_cs *cs, VkConformanceVersion *val)
{
    vn_decode_uint8_t(cs, &val->major);
    vn_decode_uint8_t(cs, &val->minor);
    vn_decode_uint8_t(cs, &val->subminor);
    vn_decode_uint8_t(cs, &val->patch);
}

static inline size_t
vn_sizeof_VkConformanceVersion_partial(const VkConformanceVersion *val)
{
    size_t size = 0;
    /* skip val->major */
    /* skip val->minor */
    /* skip val->subminor */
    /* skip val->patch */
    return size;
}

static inline void
vn_encode_VkConformanceVersion_partial(struct vn_cs *cs, const VkConformanceVersion *val)
{
    /* skip val->major */
    /* skip val->minor */
    /* skip val->subminor */
    /* skip val->patch */
}

/* struct VkPhysicalDeviceDriverProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceDriverProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDriverProperties_self(const VkPhysicalDeviceDriverProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDriverId(&val->driverID);
    size += vn_sizeof_array_size(VK_MAX_DRIVER_NAME_SIZE);
    size += vn_sizeof_blob_array(val->driverName, VK_MAX_DRIVER_NAME_SIZE);
    size += vn_sizeof_array_size(VK_MAX_DRIVER_INFO_SIZE);
    size += vn_sizeof_blob_array(val->driverInfo, VK_MAX_DRIVER_INFO_SIZE);
    size += vn_sizeof_VkConformanceVersion(&val->conformanceVersion);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDriverProperties(const VkPhysicalDeviceDriverProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDriverProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDriverProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceDriverProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceDriverProperties_self(struct vn_cs *cs, VkPhysicalDeviceDriverProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkDriverId(cs, &val->driverID);
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_DRIVER_NAME_SIZE);
        vn_decode_blob_array(cs, val->driverName, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_DRIVER_INFO_SIZE);
        vn_decode_blob_array(cs, val->driverInfo, array_size);
    }
    vn_decode_VkConformanceVersion(cs, &val->conformanceVersion);
}

static inline void
vn_decode_VkPhysicalDeviceDriverProperties(struct vn_cs *cs, VkPhysicalDeviceDriverProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceDriverProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceDriverProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDriverProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDriverProperties_self_partial(const VkPhysicalDeviceDriverProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->driverID */
    /* skip val->driverName */
    /* skip val->driverInfo */
    size += vn_sizeof_VkConformanceVersion_partial(&val->conformanceVersion);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDriverProperties_partial(const VkPhysicalDeviceDriverProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDriverProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDriverProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceDriverProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceDriverProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceDriverProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->driverID */
    /* skip val->driverName */
    /* skip val->driverInfo */
    vn_encode_VkConformanceVersion_partial(cs, &val->conformanceVersion);
}

static inline void
vn_encode_VkPhysicalDeviceDriverProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceDriverProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES });
    vn_encode_VkPhysicalDeviceDriverProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceDriverProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceIDProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceIDProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceIDProperties_self(const VkPhysicalDeviceIDProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_array_size(VK_UUID_SIZE);
    size += vn_sizeof_uint8_t_array(val->deviceUUID, VK_UUID_SIZE);
    size += vn_sizeof_array_size(VK_UUID_SIZE);
    size += vn_sizeof_uint8_t_array(val->driverUUID, VK_UUID_SIZE);
    size += vn_sizeof_array_size(VK_LUID_SIZE);
    size += vn_sizeof_uint8_t_array(val->deviceLUID, VK_LUID_SIZE);
    size += vn_sizeof_uint32_t(&val->deviceNodeMask);
    size += vn_sizeof_VkBool32(&val->deviceLUIDValid);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceIDProperties(const VkPhysicalDeviceIDProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceIDProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceIDProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceIDProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceIDProperties_self(struct vn_cs *cs, VkPhysicalDeviceIDProperties *val)
{
    /* skip val->{sType,pNext} */
    {
        const size_t array_size = vn_decode_array_size(cs, VK_UUID_SIZE);
        vn_decode_uint8_t_array(cs, val->deviceUUID, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, VK_UUID_SIZE);
        vn_decode_uint8_t_array(cs, val->driverUUID, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, VK_LUID_SIZE);
        vn_decode_uint8_t_array(cs, val->deviceLUID, array_size);
    }
    vn_decode_uint32_t(cs, &val->deviceNodeMask);
    vn_decode_VkBool32(cs, &val->deviceLUIDValid);
}

static inline void
vn_decode_VkPhysicalDeviceIDProperties(struct vn_cs *cs, VkPhysicalDeviceIDProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceIDProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceIDProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceIDProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceIDProperties_self_partial(const VkPhysicalDeviceIDProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->deviceUUID */
    /* skip val->driverUUID */
    /* skip val->deviceLUID */
    /* skip val->deviceNodeMask */
    /* skip val->deviceLUIDValid */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceIDProperties_partial(const VkPhysicalDeviceIDProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceIDProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceIDProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceIDProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceIDProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceIDProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->deviceUUID */
    /* skip val->driverUUID */
    /* skip val->deviceLUID */
    /* skip val->deviceNodeMask */
    /* skip val->deviceLUIDValid */
}

static inline void
vn_encode_VkPhysicalDeviceIDProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceIDProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES });
    vn_encode_VkPhysicalDeviceIDProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceIDProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceMultiviewProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewProperties_self(const VkPhysicalDeviceMultiviewProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->maxMultiviewViewCount);
    size += vn_sizeof_uint32_t(&val->maxMultiviewInstanceIndex);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewProperties(const VkPhysicalDeviceMultiviewProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMultiviewProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMultiviewProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceMultiviewProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceMultiviewProperties_self(struct vn_cs *cs, VkPhysicalDeviceMultiviewProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->maxMultiviewViewCount);
    vn_decode_uint32_t(cs, &val->maxMultiviewInstanceIndex);
}

static inline void
vn_decode_VkPhysicalDeviceMultiviewProperties(struct vn_cs *cs, VkPhysicalDeviceMultiviewProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceMultiviewProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceMultiviewProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewProperties_self_partial(const VkPhysicalDeviceMultiviewProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->maxMultiviewViewCount */
    /* skip val->maxMultiviewInstanceIndex */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMultiviewProperties_partial(const VkPhysicalDeviceMultiviewProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMultiviewProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMultiviewProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceMultiviewProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->maxMultiviewViewCount */
    /* skip val->maxMultiviewInstanceIndex */
}

static inline void
vn_encode_VkPhysicalDeviceMultiviewProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceMultiviewProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES });
    vn_encode_VkPhysicalDeviceMultiviewProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceMultiviewProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceSubgroupProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceSubgroupProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSubgroupProperties_self(const VkPhysicalDeviceSubgroupProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->subgroupSize);
    size += vn_sizeof_VkFlags(&val->supportedStages);
    size += vn_sizeof_VkFlags(&val->supportedOperations);
    size += vn_sizeof_VkBool32(&val->quadOperationsInAllStages);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSubgroupProperties(const VkPhysicalDeviceSubgroupProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSubgroupProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSubgroupProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceSubgroupProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceSubgroupProperties_self(struct vn_cs *cs, VkPhysicalDeviceSubgroupProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->subgroupSize);
    vn_decode_VkFlags(cs, &val->supportedStages);
    vn_decode_VkFlags(cs, &val->supportedOperations);
    vn_decode_VkBool32(cs, &val->quadOperationsInAllStages);
}

static inline void
vn_decode_VkPhysicalDeviceSubgroupProperties(struct vn_cs *cs, VkPhysicalDeviceSubgroupProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceSubgroupProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceSubgroupProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSubgroupProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSubgroupProperties_self_partial(const VkPhysicalDeviceSubgroupProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->subgroupSize */
    /* skip val->supportedStages */
    /* skip val->supportedOperations */
    /* skip val->quadOperationsInAllStages */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSubgroupProperties_partial(const VkPhysicalDeviceSubgroupProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSubgroupProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSubgroupProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSubgroupProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceSubgroupProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceSubgroupProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->subgroupSize */
    /* skip val->supportedStages */
    /* skip val->supportedOperations */
    /* skip val->quadOperationsInAllStages */
}

static inline void
vn_encode_VkPhysicalDeviceSubgroupProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceSubgroupProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES });
    vn_encode_VkPhysicalDeviceSubgroupProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceSubgroupProperties_self_partial(cs, val);
}

/* struct VkPhysicalDevicePointClippingProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDevicePointClippingProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDevicePointClippingProperties_self(const VkPhysicalDevicePointClippingProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkPointClippingBehavior(&val->pointClippingBehavior);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDevicePointClippingProperties(const VkPhysicalDevicePointClippingProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDevicePointClippingProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDevicePointClippingProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDevicePointClippingProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDevicePointClippingProperties_self(struct vn_cs *cs, VkPhysicalDevicePointClippingProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkPointClippingBehavior(cs, &val->pointClippingBehavior);
}

static inline void
vn_decode_VkPhysicalDevicePointClippingProperties(struct vn_cs *cs, VkPhysicalDevicePointClippingProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDevicePointClippingProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDevicePointClippingProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDevicePointClippingProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDevicePointClippingProperties_self_partial(const VkPhysicalDevicePointClippingProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->pointClippingBehavior */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDevicePointClippingProperties_partial(const VkPhysicalDevicePointClippingProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDevicePointClippingProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDevicePointClippingProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDevicePointClippingProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDevicePointClippingProperties_self_partial(struct vn_cs *cs, const VkPhysicalDevicePointClippingProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->pointClippingBehavior */
}

static inline void
vn_encode_VkPhysicalDevicePointClippingProperties_partial(struct vn_cs *cs, const VkPhysicalDevicePointClippingProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES });
    vn_encode_VkPhysicalDevicePointClippingProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDevicePointClippingProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceProtectedMemoryProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_self(const VkPhysicalDeviceProtectedMemoryProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->protectedNoFault);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties(const VkPhysicalDeviceProtectedMemoryProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceProtectedMemoryProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceProtectedMemoryProperties_self(struct vn_cs *cs, VkPhysicalDeviceProtectedMemoryProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->protectedNoFault);
}

static inline void
vn_decode_VkPhysicalDeviceProtectedMemoryProperties(struct vn_cs *cs, VkPhysicalDeviceProtectedMemoryProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceProtectedMemoryProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceProtectedMemoryProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_self_partial(const VkPhysicalDeviceProtectedMemoryProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->protectedNoFault */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_partial(const VkPhysicalDeviceProtectedMemoryProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceProtectedMemoryProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->protectedNoFault */
}

static inline void
vn_encode_VkPhysicalDeviceProtectedMemoryProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceProtectedMemoryProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES });
    vn_encode_VkPhysicalDeviceProtectedMemoryProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceProtectedMemoryProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceSamplerFilterMinmaxProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_self(const VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->filterMinmaxSingleComponentFormats);
    size += vn_sizeof_VkBool32(&val->filterMinmaxImageComponentMapping);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties(const VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceSamplerFilterMinmaxProperties_self(struct vn_cs *cs, VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->filterMinmaxSingleComponentFormats);
    vn_decode_VkBool32(cs, &val->filterMinmaxImageComponentMapping);
}

static inline void
vn_decode_VkPhysicalDeviceSamplerFilterMinmaxProperties(struct vn_cs *cs, VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceSamplerFilterMinmaxProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_self_partial(const VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->filterMinmaxSingleComponentFormats */
    /* skip val->filterMinmaxImageComponentMapping */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_partial(const VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceSamplerFilterMinmaxProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->filterMinmaxSingleComponentFormats */
    /* skip val->filterMinmaxImageComponentMapping */
}

static inline void
vn_encode_VkPhysicalDeviceSamplerFilterMinmaxProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceSamplerFilterMinmaxProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES });
    vn_encode_VkPhysicalDeviceSamplerFilterMinmaxProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceSamplerFilterMinmaxProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceMaintenance3Properties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceMaintenance3Properties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMaintenance3Properties_self(const VkPhysicalDeviceMaintenance3Properties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->maxPerSetDescriptors);
    size += vn_sizeof_VkDeviceSize(&val->maxMemoryAllocationSize);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMaintenance3Properties(const VkPhysicalDeviceMaintenance3Properties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMaintenance3Properties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMaintenance3Properties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceMaintenance3Properties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceMaintenance3Properties_self(struct vn_cs *cs, VkPhysicalDeviceMaintenance3Properties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->maxPerSetDescriptors);
    vn_decode_VkDeviceSize(cs, &val->maxMemoryAllocationSize);
}

static inline void
vn_decode_VkPhysicalDeviceMaintenance3Properties(struct vn_cs *cs, VkPhysicalDeviceMaintenance3Properties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceMaintenance3Properties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceMaintenance3Properties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMaintenance3Properties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMaintenance3Properties_self_partial(const VkPhysicalDeviceMaintenance3Properties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->maxPerSetDescriptors */
    /* skip val->maxMemoryAllocationSize */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMaintenance3Properties_partial(const VkPhysicalDeviceMaintenance3Properties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMaintenance3Properties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMaintenance3Properties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceMaintenance3Properties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceMaintenance3Properties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceMaintenance3Properties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->maxPerSetDescriptors */
    /* skip val->maxMemoryAllocationSize */
}

static inline void
vn_encode_VkPhysicalDeviceMaintenance3Properties_partial(struct vn_cs *cs, const VkPhysicalDeviceMaintenance3Properties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES });
    vn_encode_VkPhysicalDeviceMaintenance3Properties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceMaintenance3Properties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceFloatControlsProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceFloatControlsProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFloatControlsProperties_self(const VkPhysicalDeviceFloatControlsProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkShaderFloatControlsIndependence(&val->denormBehaviorIndependence);
    size += vn_sizeof_VkShaderFloatControlsIndependence(&val->roundingModeIndependence);
    size += vn_sizeof_VkBool32(&val->shaderSignedZeroInfNanPreserveFloat16);
    size += vn_sizeof_VkBool32(&val->shaderSignedZeroInfNanPreserveFloat32);
    size += vn_sizeof_VkBool32(&val->shaderSignedZeroInfNanPreserveFloat64);
    size += vn_sizeof_VkBool32(&val->shaderDenormPreserveFloat16);
    size += vn_sizeof_VkBool32(&val->shaderDenormPreserveFloat32);
    size += vn_sizeof_VkBool32(&val->shaderDenormPreserveFloat64);
    size += vn_sizeof_VkBool32(&val->shaderDenormFlushToZeroFloat16);
    size += vn_sizeof_VkBool32(&val->shaderDenormFlushToZeroFloat32);
    size += vn_sizeof_VkBool32(&val->shaderDenormFlushToZeroFloat64);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTEFloat16);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTEFloat32);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTEFloat64);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTZFloat16);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTZFloat32);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTZFloat64);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFloatControlsProperties(const VkPhysicalDeviceFloatControlsProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceFloatControlsProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceFloatControlsProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceFloatControlsProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceFloatControlsProperties_self(struct vn_cs *cs, VkPhysicalDeviceFloatControlsProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkShaderFloatControlsIndependence(cs, &val->denormBehaviorIndependence);
    vn_decode_VkShaderFloatControlsIndependence(cs, &val->roundingModeIndependence);
    vn_decode_VkBool32(cs, &val->shaderSignedZeroInfNanPreserveFloat16);
    vn_decode_VkBool32(cs, &val->shaderSignedZeroInfNanPreserveFloat32);
    vn_decode_VkBool32(cs, &val->shaderSignedZeroInfNanPreserveFloat64);
    vn_decode_VkBool32(cs, &val->shaderDenormPreserveFloat16);
    vn_decode_VkBool32(cs, &val->shaderDenormPreserveFloat32);
    vn_decode_VkBool32(cs, &val->shaderDenormPreserveFloat64);
    vn_decode_VkBool32(cs, &val->shaderDenormFlushToZeroFloat16);
    vn_decode_VkBool32(cs, &val->shaderDenormFlushToZeroFloat32);
    vn_decode_VkBool32(cs, &val->shaderDenormFlushToZeroFloat64);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTEFloat16);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTEFloat32);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTEFloat64);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTZFloat16);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTZFloat32);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTZFloat64);
}

static inline void
vn_decode_VkPhysicalDeviceFloatControlsProperties(struct vn_cs *cs, VkPhysicalDeviceFloatControlsProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceFloatControlsProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceFloatControlsProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFloatControlsProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFloatControlsProperties_self_partial(const VkPhysicalDeviceFloatControlsProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->denormBehaviorIndependence */
    /* skip val->roundingModeIndependence */
    /* skip val->shaderSignedZeroInfNanPreserveFloat16 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat32 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat64 */
    /* skip val->shaderDenormPreserveFloat16 */
    /* skip val->shaderDenormPreserveFloat32 */
    /* skip val->shaderDenormPreserveFloat64 */
    /* skip val->shaderDenormFlushToZeroFloat16 */
    /* skip val->shaderDenormFlushToZeroFloat32 */
    /* skip val->shaderDenormFlushToZeroFloat64 */
    /* skip val->shaderRoundingModeRTEFloat16 */
    /* skip val->shaderRoundingModeRTEFloat32 */
    /* skip val->shaderRoundingModeRTEFloat64 */
    /* skip val->shaderRoundingModeRTZFloat16 */
    /* skip val->shaderRoundingModeRTZFloat32 */
    /* skip val->shaderRoundingModeRTZFloat64 */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceFloatControlsProperties_partial(const VkPhysicalDeviceFloatControlsProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceFloatControlsProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceFloatControlsProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceFloatControlsProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceFloatControlsProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceFloatControlsProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->denormBehaviorIndependence */
    /* skip val->roundingModeIndependence */
    /* skip val->shaderSignedZeroInfNanPreserveFloat16 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat32 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat64 */
    /* skip val->shaderDenormPreserveFloat16 */
    /* skip val->shaderDenormPreserveFloat32 */
    /* skip val->shaderDenormPreserveFloat64 */
    /* skip val->shaderDenormFlushToZeroFloat16 */
    /* skip val->shaderDenormFlushToZeroFloat32 */
    /* skip val->shaderDenormFlushToZeroFloat64 */
    /* skip val->shaderRoundingModeRTEFloat16 */
    /* skip val->shaderRoundingModeRTEFloat32 */
    /* skip val->shaderRoundingModeRTEFloat64 */
    /* skip val->shaderRoundingModeRTZFloat16 */
    /* skip val->shaderRoundingModeRTZFloat32 */
    /* skip val->shaderRoundingModeRTZFloat64 */
}

static inline void
vn_encode_VkPhysicalDeviceFloatControlsProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceFloatControlsProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES });
    vn_encode_VkPhysicalDeviceFloatControlsProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceFloatControlsProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceDescriptorIndexingProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_self(const VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->maxUpdateAfterBindDescriptorsInAllPools);
    size += vn_sizeof_VkBool32(&val->shaderUniformBufferArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderSampledImageArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderStorageBufferArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderInputAttachmentArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->robustBufferAccessUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->quadDivergentImplicitLod);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindSamplers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindUniformBuffers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindStorageBuffers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindSampledImages);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindStorageImages);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindInputAttachments);
    size += vn_sizeof_uint32_t(&val->maxPerStageUpdateAfterBindResources);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindSamplers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindUniformBuffers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindStorageBuffers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindSampledImages);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindStorageImages);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindInputAttachments);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties(const VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceDescriptorIndexingProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceDescriptorIndexingProperties_self(struct vn_cs *cs, VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->maxUpdateAfterBindDescriptorsInAllPools);
    vn_decode_VkBool32(cs, &val->shaderUniformBufferArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderSampledImageArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderStorageBufferArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderStorageImageArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderInputAttachmentArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->robustBufferAccessUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->quadDivergentImplicitLod);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindSamplers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindUniformBuffers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindStorageBuffers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindSampledImages);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindStorageImages);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindInputAttachments);
    vn_decode_uint32_t(cs, &val->maxPerStageUpdateAfterBindResources);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindSamplers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindUniformBuffers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindStorageBuffers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindSampledImages);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindStorageImages);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindInputAttachments);
}

static inline void
vn_decode_VkPhysicalDeviceDescriptorIndexingProperties(struct vn_cs *cs, VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceDescriptorIndexingProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceDescriptorIndexingProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_self_partial(const VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->maxUpdateAfterBindDescriptorsInAllPools */
    /* skip val->shaderUniformBufferArrayNonUniformIndexingNative */
    /* skip val->shaderSampledImageArrayNonUniformIndexingNative */
    /* skip val->shaderStorageBufferArrayNonUniformIndexingNative */
    /* skip val->shaderStorageImageArrayNonUniformIndexingNative */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexingNative */
    /* skip val->robustBufferAccessUpdateAfterBind */
    /* skip val->quadDivergentImplicitLod */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSamplers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindUniformBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSampledImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindInputAttachments */
    /* skip val->maxPerStageUpdateAfterBindResources */
    /* skip val->maxDescriptorSetUpdateAfterBindSamplers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindSampledImages */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageImages */
    /* skip val->maxDescriptorSetUpdateAfterBindInputAttachments */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_partial(const VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->maxUpdateAfterBindDescriptorsInAllPools */
    /* skip val->shaderUniformBufferArrayNonUniformIndexingNative */
    /* skip val->shaderSampledImageArrayNonUniformIndexingNative */
    /* skip val->shaderStorageBufferArrayNonUniformIndexingNative */
    /* skip val->shaderStorageImageArrayNonUniformIndexingNative */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexingNative */
    /* skip val->robustBufferAccessUpdateAfterBind */
    /* skip val->quadDivergentImplicitLod */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSamplers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindUniformBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSampledImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindInputAttachments */
    /* skip val->maxPerStageUpdateAfterBindResources */
    /* skip val->maxDescriptorSetUpdateAfterBindSamplers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindSampledImages */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageImages */
    /* skip val->maxDescriptorSetUpdateAfterBindInputAttachments */
}

static inline void
vn_encode_VkPhysicalDeviceDescriptorIndexingProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceDescriptorIndexingProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES });
    vn_encode_VkPhysicalDeviceDescriptorIndexingProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceDescriptorIndexingProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceTimelineSemaphoreProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_self(const VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint64_t(&val->maxTimelineSemaphoreValueDifference);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties(const VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceTimelineSemaphoreProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceTimelineSemaphoreProperties_self(struct vn_cs *cs, VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint64_t(cs, &val->maxTimelineSemaphoreValueDifference);
}

static inline void
vn_decode_VkPhysicalDeviceTimelineSemaphoreProperties(struct vn_cs *cs, VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceTimelineSemaphoreProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceTimelineSemaphoreProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_self_partial(const VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->maxTimelineSemaphoreValueDifference */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_partial(const VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->maxTimelineSemaphoreValueDifference */
}

static inline void
vn_encode_VkPhysicalDeviceTimelineSemaphoreProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceTimelineSemaphoreProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES });
    vn_encode_VkPhysicalDeviceTimelineSemaphoreProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceTimelineSemaphoreProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceDepthStencilResolveProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_self(const VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->supportedDepthResolveModes);
    size += vn_sizeof_VkFlags(&val->supportedStencilResolveModes);
    size += vn_sizeof_VkBool32(&val->independentResolveNone);
    size += vn_sizeof_VkBool32(&val->independentResolve);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties(const VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceDepthStencilResolveProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceDepthStencilResolveProperties_self(struct vn_cs *cs, VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkFlags(cs, &val->supportedDepthResolveModes);
    vn_decode_VkFlags(cs, &val->supportedStencilResolveModes);
    vn_decode_VkBool32(cs, &val->independentResolveNone);
    vn_decode_VkBool32(cs, &val->independentResolve);
}

static inline void
vn_decode_VkPhysicalDeviceDepthStencilResolveProperties(struct vn_cs *cs, VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceDepthStencilResolveProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceDepthStencilResolveProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_self_partial(const VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->supportedDepthResolveModes */
    /* skip val->supportedStencilResolveModes */
    /* skip val->independentResolveNone */
    /* skip val->independentResolve */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_partial(const VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceDepthStencilResolveProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceDepthStencilResolveProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->supportedDepthResolveModes */
    /* skip val->supportedStencilResolveModes */
    /* skip val->independentResolveNone */
    /* skip val->independentResolve */
}

static inline void
vn_encode_VkPhysicalDeviceDepthStencilResolveProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceDepthStencilResolveProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES });
    vn_encode_VkPhysicalDeviceDepthStencilResolveProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceDepthStencilResolveProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceTransformFeedbackPropertiesEXT chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self(const VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->maxTransformFeedbackStreams);
    size += vn_sizeof_uint32_t(&val->maxTransformFeedbackBuffers);
    size += vn_sizeof_VkDeviceSize(&val->maxTransformFeedbackBufferSize);
    size += vn_sizeof_uint32_t(&val->maxTransformFeedbackStreamDataSize);
    size += vn_sizeof_uint32_t(&val->maxTransformFeedbackBufferDataSize);
    size += vn_sizeof_uint32_t(&val->maxTransformFeedbackBufferDataStride);
    size += vn_sizeof_VkBool32(&val->transformFeedbackQueries);
    size += vn_sizeof_VkBool32(&val->transformFeedbackStreamsLinesTriangles);
    size += vn_sizeof_VkBool32(&val->transformFeedbackRasterizationStreamSelect);
    size += vn_sizeof_VkBool32(&val->transformFeedbackDraw);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT(const VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self(struct vn_cs *cs, VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->maxTransformFeedbackStreams);
    vn_decode_uint32_t(cs, &val->maxTransformFeedbackBuffers);
    vn_decode_VkDeviceSize(cs, &val->maxTransformFeedbackBufferSize);
    vn_decode_uint32_t(cs, &val->maxTransformFeedbackStreamDataSize);
    vn_decode_uint32_t(cs, &val->maxTransformFeedbackBufferDataSize);
    vn_decode_uint32_t(cs, &val->maxTransformFeedbackBufferDataStride);
    vn_decode_VkBool32(cs, &val->transformFeedbackQueries);
    vn_decode_VkBool32(cs, &val->transformFeedbackStreamsLinesTriangles);
    vn_decode_VkBool32(cs, &val->transformFeedbackRasterizationStreamSelect);
    vn_decode_VkBool32(cs, &val->transformFeedbackDraw);
}

static inline void
vn_decode_VkPhysicalDeviceTransformFeedbackPropertiesEXT(struct vn_cs *cs, VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self_partial(const VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->maxTransformFeedbackStreams */
    /* skip val->maxTransformFeedbackBuffers */
    /* skip val->maxTransformFeedbackBufferSize */
    /* skip val->maxTransformFeedbackStreamDataSize */
    /* skip val->maxTransformFeedbackBufferDataSize */
    /* skip val->maxTransformFeedbackBufferDataStride */
    /* skip val->transformFeedbackQueries */
    /* skip val->transformFeedbackStreamsLinesTriangles */
    /* skip val->transformFeedbackRasterizationStreamSelect */
    /* skip val->transformFeedbackDraw */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_partial(const VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self_partial(struct vn_cs *cs, const VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->maxTransformFeedbackStreams */
    /* skip val->maxTransformFeedbackBuffers */
    /* skip val->maxTransformFeedbackBufferSize */
    /* skip val->maxTransformFeedbackStreamDataSize */
    /* skip val->maxTransformFeedbackBufferDataSize */
    /* skip val->maxTransformFeedbackBufferDataStride */
    /* skip val->transformFeedbackQueries */
    /* skip val->transformFeedbackStreamsLinesTriangles */
    /* skip val->transformFeedbackRasterizationStreamSelect */
    /* skip val->transformFeedbackDraw */
}

static inline void
vn_encode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_partial(struct vn_cs *cs, const VkPhysicalDeviceTransformFeedbackPropertiesEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT });
    vn_encode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self_partial(cs, val);
}

/* struct VkPhysicalDeviceVulkan11Properties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Properties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Properties_self(const VkPhysicalDeviceVulkan11Properties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_array_size(VK_UUID_SIZE);
    size += vn_sizeof_uint8_t_array(val->deviceUUID, VK_UUID_SIZE);
    size += vn_sizeof_array_size(VK_UUID_SIZE);
    size += vn_sizeof_uint8_t_array(val->driverUUID, VK_UUID_SIZE);
    size += vn_sizeof_array_size(VK_LUID_SIZE);
    size += vn_sizeof_uint8_t_array(val->deviceLUID, VK_LUID_SIZE);
    size += vn_sizeof_uint32_t(&val->deviceNodeMask);
    size += vn_sizeof_VkBool32(&val->deviceLUIDValid);
    size += vn_sizeof_uint32_t(&val->subgroupSize);
    size += vn_sizeof_VkFlags(&val->subgroupSupportedStages);
    size += vn_sizeof_VkFlags(&val->subgroupSupportedOperations);
    size += vn_sizeof_VkBool32(&val->subgroupQuadOperationsInAllStages);
    size += vn_sizeof_VkPointClippingBehavior(&val->pointClippingBehavior);
    size += vn_sizeof_uint32_t(&val->maxMultiviewViewCount);
    size += vn_sizeof_uint32_t(&val->maxMultiviewInstanceIndex);
    size += vn_sizeof_VkBool32(&val->protectedNoFault);
    size += vn_sizeof_uint32_t(&val->maxPerSetDescriptors);
    size += vn_sizeof_VkDeviceSize(&val->maxMemoryAllocationSize);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Properties(const VkPhysicalDeviceVulkan11Properties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Properties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Properties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceVulkan11Properties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan11Properties_self(struct vn_cs *cs, VkPhysicalDeviceVulkan11Properties *val)
{
    /* skip val->{sType,pNext} */
    {
        const size_t array_size = vn_decode_array_size(cs, VK_UUID_SIZE);
        vn_decode_uint8_t_array(cs, val->deviceUUID, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, VK_UUID_SIZE);
        vn_decode_uint8_t_array(cs, val->driverUUID, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, VK_LUID_SIZE);
        vn_decode_uint8_t_array(cs, val->deviceLUID, array_size);
    }
    vn_decode_uint32_t(cs, &val->deviceNodeMask);
    vn_decode_VkBool32(cs, &val->deviceLUIDValid);
    vn_decode_uint32_t(cs, &val->subgroupSize);
    vn_decode_VkFlags(cs, &val->subgroupSupportedStages);
    vn_decode_VkFlags(cs, &val->subgroupSupportedOperations);
    vn_decode_VkBool32(cs, &val->subgroupQuadOperationsInAllStages);
    vn_decode_VkPointClippingBehavior(cs, &val->pointClippingBehavior);
    vn_decode_uint32_t(cs, &val->maxMultiviewViewCount);
    vn_decode_uint32_t(cs, &val->maxMultiviewInstanceIndex);
    vn_decode_VkBool32(cs, &val->protectedNoFault);
    vn_decode_uint32_t(cs, &val->maxPerSetDescriptors);
    vn_decode_VkDeviceSize(cs, &val->maxMemoryAllocationSize);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan11Properties(struct vn_cs *cs, VkPhysicalDeviceVulkan11Properties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceVulkan11Properties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceVulkan11Properties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Properties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Properties_self_partial(const VkPhysicalDeviceVulkan11Properties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->deviceUUID */
    /* skip val->driverUUID */
    /* skip val->deviceLUID */
    /* skip val->deviceNodeMask */
    /* skip val->deviceLUIDValid */
    /* skip val->subgroupSize */
    /* skip val->subgroupSupportedStages */
    /* skip val->subgroupSupportedOperations */
    /* skip val->subgroupQuadOperationsInAllStages */
    /* skip val->pointClippingBehavior */
    /* skip val->maxMultiviewViewCount */
    /* skip val->maxMultiviewInstanceIndex */
    /* skip val->protectedNoFault */
    /* skip val->maxPerSetDescriptors */
    /* skip val->maxMemoryAllocationSize */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan11Properties_partial(const VkPhysicalDeviceVulkan11Properties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Properties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan11Properties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Properties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Properties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan11Properties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->deviceUUID */
    /* skip val->driverUUID */
    /* skip val->deviceLUID */
    /* skip val->deviceNodeMask */
    /* skip val->deviceLUIDValid */
    /* skip val->subgroupSize */
    /* skip val->subgroupSupportedStages */
    /* skip val->subgroupSupportedOperations */
    /* skip val->subgroupQuadOperationsInAllStages */
    /* skip val->pointClippingBehavior */
    /* skip val->maxMultiviewViewCount */
    /* skip val->maxMultiviewInstanceIndex */
    /* skip val->protectedNoFault */
    /* skip val->maxPerSetDescriptors */
    /* skip val->maxMemoryAllocationSize */
}

static inline void
vn_encode_VkPhysicalDeviceVulkan11Properties_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan11Properties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES });
    vn_encode_VkPhysicalDeviceVulkan11Properties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkan11Properties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceVulkan12Properties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Properties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Properties_self(const VkPhysicalDeviceVulkan12Properties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDriverId(&val->driverID);
    size += vn_sizeof_array_size(VK_MAX_DRIVER_NAME_SIZE);
    size += vn_sizeof_blob_array(val->driverName, VK_MAX_DRIVER_NAME_SIZE);
    size += vn_sizeof_array_size(VK_MAX_DRIVER_INFO_SIZE);
    size += vn_sizeof_blob_array(val->driverInfo, VK_MAX_DRIVER_INFO_SIZE);
    size += vn_sizeof_VkConformanceVersion(&val->conformanceVersion);
    size += vn_sizeof_VkShaderFloatControlsIndependence(&val->denormBehaviorIndependence);
    size += vn_sizeof_VkShaderFloatControlsIndependence(&val->roundingModeIndependence);
    size += vn_sizeof_VkBool32(&val->shaderSignedZeroInfNanPreserveFloat16);
    size += vn_sizeof_VkBool32(&val->shaderSignedZeroInfNanPreserveFloat32);
    size += vn_sizeof_VkBool32(&val->shaderSignedZeroInfNanPreserveFloat64);
    size += vn_sizeof_VkBool32(&val->shaderDenormPreserveFloat16);
    size += vn_sizeof_VkBool32(&val->shaderDenormPreserveFloat32);
    size += vn_sizeof_VkBool32(&val->shaderDenormPreserveFloat64);
    size += vn_sizeof_VkBool32(&val->shaderDenormFlushToZeroFloat16);
    size += vn_sizeof_VkBool32(&val->shaderDenormFlushToZeroFloat32);
    size += vn_sizeof_VkBool32(&val->shaderDenormFlushToZeroFloat64);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTEFloat16);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTEFloat32);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTEFloat64);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTZFloat16);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTZFloat32);
    size += vn_sizeof_VkBool32(&val->shaderRoundingModeRTZFloat64);
    size += vn_sizeof_uint32_t(&val->maxUpdateAfterBindDescriptorsInAllPools);
    size += vn_sizeof_VkBool32(&val->shaderUniformBufferArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderSampledImageArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderStorageBufferArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderStorageImageArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->shaderInputAttachmentArrayNonUniformIndexingNative);
    size += vn_sizeof_VkBool32(&val->robustBufferAccessUpdateAfterBind);
    size += vn_sizeof_VkBool32(&val->quadDivergentImplicitLod);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindSamplers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindUniformBuffers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindStorageBuffers);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindSampledImages);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindStorageImages);
    size += vn_sizeof_uint32_t(&val->maxPerStageDescriptorUpdateAfterBindInputAttachments);
    size += vn_sizeof_uint32_t(&val->maxPerStageUpdateAfterBindResources);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindSamplers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindUniformBuffers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindStorageBuffers);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindSampledImages);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindStorageImages);
    size += vn_sizeof_uint32_t(&val->maxDescriptorSetUpdateAfterBindInputAttachments);
    size += vn_sizeof_VkFlags(&val->supportedDepthResolveModes);
    size += vn_sizeof_VkFlags(&val->supportedStencilResolveModes);
    size += vn_sizeof_VkBool32(&val->independentResolveNone);
    size += vn_sizeof_VkBool32(&val->independentResolve);
    size += vn_sizeof_VkBool32(&val->filterMinmaxSingleComponentFormats);
    size += vn_sizeof_VkBool32(&val->filterMinmaxImageComponentMapping);
    size += vn_sizeof_uint64_t(&val->maxTimelineSemaphoreValueDifference);
    size += vn_sizeof_VkFlags(&val->framebufferIntegerColorSampleCounts);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Properties(const VkPhysicalDeviceVulkan12Properties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Properties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Properties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceVulkan12Properties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan12Properties_self(struct vn_cs *cs, VkPhysicalDeviceVulkan12Properties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkDriverId(cs, &val->driverID);
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_DRIVER_NAME_SIZE);
        vn_decode_blob_array(cs, val->driverName, array_size);
    }
    {
        const size_t array_size = vn_decode_array_size(cs, VK_MAX_DRIVER_INFO_SIZE);
        vn_decode_blob_array(cs, val->driverInfo, array_size);
    }
    vn_decode_VkConformanceVersion(cs, &val->conformanceVersion);
    vn_decode_VkShaderFloatControlsIndependence(cs, &val->denormBehaviorIndependence);
    vn_decode_VkShaderFloatControlsIndependence(cs, &val->roundingModeIndependence);
    vn_decode_VkBool32(cs, &val->shaderSignedZeroInfNanPreserveFloat16);
    vn_decode_VkBool32(cs, &val->shaderSignedZeroInfNanPreserveFloat32);
    vn_decode_VkBool32(cs, &val->shaderSignedZeroInfNanPreserveFloat64);
    vn_decode_VkBool32(cs, &val->shaderDenormPreserveFloat16);
    vn_decode_VkBool32(cs, &val->shaderDenormPreserveFloat32);
    vn_decode_VkBool32(cs, &val->shaderDenormPreserveFloat64);
    vn_decode_VkBool32(cs, &val->shaderDenormFlushToZeroFloat16);
    vn_decode_VkBool32(cs, &val->shaderDenormFlushToZeroFloat32);
    vn_decode_VkBool32(cs, &val->shaderDenormFlushToZeroFloat64);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTEFloat16);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTEFloat32);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTEFloat64);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTZFloat16);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTZFloat32);
    vn_decode_VkBool32(cs, &val->shaderRoundingModeRTZFloat64);
    vn_decode_uint32_t(cs, &val->maxUpdateAfterBindDescriptorsInAllPools);
    vn_decode_VkBool32(cs, &val->shaderUniformBufferArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderSampledImageArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderStorageBufferArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderStorageImageArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->shaderInputAttachmentArrayNonUniformIndexingNative);
    vn_decode_VkBool32(cs, &val->robustBufferAccessUpdateAfterBind);
    vn_decode_VkBool32(cs, &val->quadDivergentImplicitLod);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindSamplers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindUniformBuffers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindStorageBuffers);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindSampledImages);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindStorageImages);
    vn_decode_uint32_t(cs, &val->maxPerStageDescriptorUpdateAfterBindInputAttachments);
    vn_decode_uint32_t(cs, &val->maxPerStageUpdateAfterBindResources);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindSamplers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindUniformBuffers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindStorageBuffers);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindSampledImages);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindStorageImages);
    vn_decode_uint32_t(cs, &val->maxDescriptorSetUpdateAfterBindInputAttachments);
    vn_decode_VkFlags(cs, &val->supportedDepthResolveModes);
    vn_decode_VkFlags(cs, &val->supportedStencilResolveModes);
    vn_decode_VkBool32(cs, &val->independentResolveNone);
    vn_decode_VkBool32(cs, &val->independentResolve);
    vn_decode_VkBool32(cs, &val->filterMinmaxSingleComponentFormats);
    vn_decode_VkBool32(cs, &val->filterMinmaxImageComponentMapping);
    vn_decode_uint64_t(cs, &val->maxTimelineSemaphoreValueDifference);
    vn_decode_VkFlags(cs, &val->framebufferIntegerColorSampleCounts);
}

static inline void
vn_decode_VkPhysicalDeviceVulkan12Properties(struct vn_cs *cs, VkPhysicalDeviceVulkan12Properties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceVulkan12Properties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceVulkan12Properties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Properties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Properties_self_partial(const VkPhysicalDeviceVulkan12Properties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->driverID */
    /* skip val->driverName */
    /* skip val->driverInfo */
    size += vn_sizeof_VkConformanceVersion_partial(&val->conformanceVersion);
    /* skip val->denormBehaviorIndependence */
    /* skip val->roundingModeIndependence */
    /* skip val->shaderSignedZeroInfNanPreserveFloat16 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat32 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat64 */
    /* skip val->shaderDenormPreserveFloat16 */
    /* skip val->shaderDenormPreserveFloat32 */
    /* skip val->shaderDenormPreserveFloat64 */
    /* skip val->shaderDenormFlushToZeroFloat16 */
    /* skip val->shaderDenormFlushToZeroFloat32 */
    /* skip val->shaderDenormFlushToZeroFloat64 */
    /* skip val->shaderRoundingModeRTEFloat16 */
    /* skip val->shaderRoundingModeRTEFloat32 */
    /* skip val->shaderRoundingModeRTEFloat64 */
    /* skip val->shaderRoundingModeRTZFloat16 */
    /* skip val->shaderRoundingModeRTZFloat32 */
    /* skip val->shaderRoundingModeRTZFloat64 */
    /* skip val->maxUpdateAfterBindDescriptorsInAllPools */
    /* skip val->shaderUniformBufferArrayNonUniformIndexingNative */
    /* skip val->shaderSampledImageArrayNonUniformIndexingNative */
    /* skip val->shaderStorageBufferArrayNonUniformIndexingNative */
    /* skip val->shaderStorageImageArrayNonUniformIndexingNative */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexingNative */
    /* skip val->robustBufferAccessUpdateAfterBind */
    /* skip val->quadDivergentImplicitLod */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSamplers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindUniformBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSampledImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindInputAttachments */
    /* skip val->maxPerStageUpdateAfterBindResources */
    /* skip val->maxDescriptorSetUpdateAfterBindSamplers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindSampledImages */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageImages */
    /* skip val->maxDescriptorSetUpdateAfterBindInputAttachments */
    /* skip val->supportedDepthResolveModes */
    /* skip val->supportedStencilResolveModes */
    /* skip val->independentResolveNone */
    /* skip val->independentResolve */
    /* skip val->filterMinmaxSingleComponentFormats */
    /* skip val->filterMinmaxImageComponentMapping */
    /* skip val->maxTimelineSemaphoreValueDifference */
    /* skip val->framebufferIntegerColorSampleCounts */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceVulkan12Properties_partial(const VkPhysicalDeviceVulkan12Properties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Properties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceVulkan12Properties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Properties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Properties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan12Properties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->driverID */
    /* skip val->driverName */
    /* skip val->driverInfo */
    vn_encode_VkConformanceVersion_partial(cs, &val->conformanceVersion);
    /* skip val->denormBehaviorIndependence */
    /* skip val->roundingModeIndependence */
    /* skip val->shaderSignedZeroInfNanPreserveFloat16 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat32 */
    /* skip val->shaderSignedZeroInfNanPreserveFloat64 */
    /* skip val->shaderDenormPreserveFloat16 */
    /* skip val->shaderDenormPreserveFloat32 */
    /* skip val->shaderDenormPreserveFloat64 */
    /* skip val->shaderDenormFlushToZeroFloat16 */
    /* skip val->shaderDenormFlushToZeroFloat32 */
    /* skip val->shaderDenormFlushToZeroFloat64 */
    /* skip val->shaderRoundingModeRTEFloat16 */
    /* skip val->shaderRoundingModeRTEFloat32 */
    /* skip val->shaderRoundingModeRTEFloat64 */
    /* skip val->shaderRoundingModeRTZFloat16 */
    /* skip val->shaderRoundingModeRTZFloat32 */
    /* skip val->shaderRoundingModeRTZFloat64 */
    /* skip val->maxUpdateAfterBindDescriptorsInAllPools */
    /* skip val->shaderUniformBufferArrayNonUniformIndexingNative */
    /* skip val->shaderSampledImageArrayNonUniformIndexingNative */
    /* skip val->shaderStorageBufferArrayNonUniformIndexingNative */
    /* skip val->shaderStorageImageArrayNonUniformIndexingNative */
    /* skip val->shaderInputAttachmentArrayNonUniformIndexingNative */
    /* skip val->robustBufferAccessUpdateAfterBind */
    /* skip val->quadDivergentImplicitLod */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSamplers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindUniformBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageBuffers */
    /* skip val->maxPerStageDescriptorUpdateAfterBindSampledImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindStorageImages */
    /* skip val->maxPerStageDescriptorUpdateAfterBindInputAttachments */
    /* skip val->maxPerStageUpdateAfterBindResources */
    /* skip val->maxDescriptorSetUpdateAfterBindSamplers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffers */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic */
    /* skip val->maxDescriptorSetUpdateAfterBindSampledImages */
    /* skip val->maxDescriptorSetUpdateAfterBindStorageImages */
    /* skip val->maxDescriptorSetUpdateAfterBindInputAttachments */
    /* skip val->supportedDepthResolveModes */
    /* skip val->supportedStencilResolveModes */
    /* skip val->independentResolveNone */
    /* skip val->independentResolve */
    /* skip val->filterMinmaxSingleComponentFormats */
    /* skip val->filterMinmaxImageComponentMapping */
    /* skip val->maxTimelineSemaphoreValueDifference */
    /* skip val->framebufferIntegerColorSampleCounts */
}

static inline void
vn_encode_VkPhysicalDeviceVulkan12Properties_partial(struct vn_cs *cs, const VkPhysicalDeviceVulkan12Properties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES });
    vn_encode_VkPhysicalDeviceVulkan12Properties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceVulkan12Properties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceProperties2 chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDriverProperties_self((const VkPhysicalDeviceDriverProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceIDProperties_self((const VkPhysicalDeviceIDProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceMultiviewProperties_self((const VkPhysicalDeviceMultiviewProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSubgroupProperties_self((const VkPhysicalDeviceSubgroupProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevicePointClippingProperties_self((const VkPhysicalDevicePointClippingProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_self((const VkPhysicalDeviceProtectedMemoryProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_self((const VkPhysicalDeviceSamplerFilterMinmaxProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceMaintenance3Properties_self((const VkPhysicalDeviceMaintenance3Properties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceFloatControlsProperties_self((const VkPhysicalDeviceFloatControlsProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_self((const VkPhysicalDeviceDescriptorIndexingProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_self((const VkPhysicalDeviceTimelineSemaphoreProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_self((const VkPhysicalDeviceDepthStencilResolveProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self((const VkPhysicalDeviceTransformFeedbackPropertiesEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan11Properties_self((const VkPhysicalDeviceVulkan11Properties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan12Properties_self((const VkPhysicalDeviceVulkan12Properties *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties2_self(const VkPhysicalDeviceProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkPhysicalDeviceProperties(&val->properties);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties2(const VkPhysicalDeviceProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceProperties2_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceProperties2_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceProperties2_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceDriverProperties_self(cs, (VkPhysicalDeviceDriverProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceIDProperties_self(cs, (VkPhysicalDeviceIDProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceMultiviewProperties_self(cs, (VkPhysicalDeviceMultiviewProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceSubgroupProperties_self(cs, (VkPhysicalDeviceSubgroupProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDevicePointClippingProperties_self(cs, (VkPhysicalDevicePointClippingProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceProtectedMemoryProperties_self(cs, (VkPhysicalDeviceProtectedMemoryProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceSamplerFilterMinmaxProperties_self(cs, (VkPhysicalDeviceSamplerFilterMinmaxProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceMaintenance3Properties_self(cs, (VkPhysicalDeviceMaintenance3Properties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceFloatControlsProperties_self(cs, (VkPhysicalDeviceFloatControlsProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceDescriptorIndexingProperties_self(cs, (VkPhysicalDeviceDescriptorIndexingProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceTimelineSemaphoreProperties_self(cs, (VkPhysicalDeviceTimelineSemaphoreProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceDepthStencilResolveProperties_self(cs, (VkPhysicalDeviceDepthStencilResolveProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self(cs, (VkPhysicalDeviceTransformFeedbackPropertiesEXT *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceVulkan11Properties_self(cs, (VkPhysicalDeviceVulkan11Properties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
        vn_decode_VkPhysicalDeviceProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkPhysicalDeviceVulkan12Properties_self(cs, (VkPhysicalDeviceVulkan12Properties *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkPhysicalDeviceProperties2_self(struct vn_cs *cs, VkPhysicalDeviceProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkPhysicalDeviceProperties(cs, &val->properties);
}

static inline void
vn_decode_VkPhysicalDeviceProperties2(struct vn_cs *cs, VkPhysicalDeviceProperties2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceProperties2_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceProperties2_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDriverProperties_self_partial((const VkPhysicalDeviceDriverProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceIDProperties_self_partial((const VkPhysicalDeviceIDProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceMultiviewProperties_self_partial((const VkPhysicalDeviceMultiviewProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSubgroupProperties_self_partial((const VkPhysicalDeviceSubgroupProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDevicePointClippingProperties_self_partial((const VkPhysicalDevicePointClippingProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceProtectedMemoryProperties_self_partial((const VkPhysicalDeviceProtectedMemoryProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceSamplerFilterMinmaxProperties_self_partial((const VkPhysicalDeviceSamplerFilterMinmaxProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceMaintenance3Properties_self_partial((const VkPhysicalDeviceMaintenance3Properties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceFloatControlsProperties_self_partial((const VkPhysicalDeviceFloatControlsProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDescriptorIndexingProperties_self_partial((const VkPhysicalDeviceDescriptorIndexingProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTimelineSemaphoreProperties_self_partial((const VkPhysicalDeviceTimelineSemaphoreProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceDepthStencilResolveProperties_self_partial((const VkPhysicalDeviceDepthStencilResolveProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self_partial((const VkPhysicalDeviceTransformFeedbackPropertiesEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan11Properties_self_partial((const VkPhysicalDeviceVulkan11Properties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceVulkan12Properties_self_partial((const VkPhysicalDeviceVulkan12Properties *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties2_self_partial(const VkPhysicalDeviceProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkPhysicalDeviceProperties_partial(&val->properties);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceProperties2_partial(const VkPhysicalDeviceProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceProperties2_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceProperties2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceProperties2_pnext_partial(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceDriverProperties_self_partial(cs, (const VkPhysicalDeviceDriverProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceIDProperties_self_partial(cs, (const VkPhysicalDeviceIDProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceMultiviewProperties_self_partial(cs, (const VkPhysicalDeviceMultiviewProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSubgroupProperties_self_partial(cs, (const VkPhysicalDeviceSubgroupProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDevicePointClippingProperties_self_partial(cs, (const VkPhysicalDevicePointClippingProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceProtectedMemoryProperties_self_partial(cs, (const VkPhysicalDeviceProtectedMemoryProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceSamplerFilterMinmaxProperties_self_partial(cs, (const VkPhysicalDeviceSamplerFilterMinmaxProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceMaintenance3Properties_self_partial(cs, (const VkPhysicalDeviceMaintenance3Properties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceFloatControlsProperties_self_partial(cs, (const VkPhysicalDeviceFloatControlsProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceDescriptorIndexingProperties_self_partial(cs, (const VkPhysicalDeviceDescriptorIndexingProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTimelineSemaphoreProperties_self_partial(cs, (const VkPhysicalDeviceTimelineSemaphoreProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceDepthStencilResolveProperties_self_partial(cs, (const VkPhysicalDeviceDepthStencilResolveProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceTransformFeedbackPropertiesEXT_self_partial(cs, (const VkPhysicalDeviceTransformFeedbackPropertiesEXT *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan11Properties_self_partial(cs, (const VkPhysicalDeviceVulkan11Properties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceVulkan12Properties_self_partial(cs, (const VkPhysicalDeviceVulkan12Properties *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceProperties2_self_partial(struct vn_cs *cs, const VkPhysicalDeviceProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkPhysicalDeviceProperties_partial(cs, &val->properties);
}

static inline void
vn_encode_VkPhysicalDeviceProperties2_partial(struct vn_cs *cs, const VkPhysicalDeviceProperties2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 });
    vn_encode_VkPhysicalDeviceProperties2_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceProperties2_self_partial(cs, val);
}

/* struct VkDrmFormatModifierPropertiesEXT */

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesEXT(const VkDrmFormatModifierPropertiesEXT *val)
{
    size_t size = 0;
    size += vn_sizeof_uint64_t(&val->drmFormatModifier);
    size += vn_sizeof_uint32_t(&val->drmFormatModifierPlaneCount);
    size += vn_sizeof_VkFlags(&val->drmFormatModifierTilingFeatures);
    return size;
}

static inline void
vn_decode_VkDrmFormatModifierPropertiesEXT(struct vn_cs *cs, VkDrmFormatModifierPropertiesEXT *val)
{
    vn_decode_uint64_t(cs, &val->drmFormatModifier);
    vn_decode_uint32_t(cs, &val->drmFormatModifierPlaneCount);
    vn_decode_VkFlags(cs, &val->drmFormatModifierTilingFeatures);
}

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesEXT_partial(const VkDrmFormatModifierPropertiesEXT *val)
{
    size_t size = 0;
    /* skip val->drmFormatModifier */
    /* skip val->drmFormatModifierPlaneCount */
    /* skip val->drmFormatModifierTilingFeatures */
    return size;
}

static inline void
vn_encode_VkDrmFormatModifierPropertiesEXT_partial(struct vn_cs *cs, const VkDrmFormatModifierPropertiesEXT *val)
{
    /* skip val->drmFormatModifier */
    /* skip val->drmFormatModifierPlaneCount */
    /* skip val->drmFormatModifierTilingFeatures */
}

/* struct VkDrmFormatModifierPropertiesListEXT chain */

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesListEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesListEXT_self(const VkDrmFormatModifierPropertiesListEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->drmFormatModifierCount);
    if (val->pDrmFormatModifierProperties) {
        size += vn_sizeof_array_size(val->drmFormatModifierCount);
        for (uint32_t i = 0; i < val->drmFormatModifierCount; i++)
            size += vn_sizeof_VkDrmFormatModifierPropertiesEXT(&val->pDrmFormatModifierProperties[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesListEXT(const VkDrmFormatModifierPropertiesListEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDrmFormatModifierPropertiesListEXT_pnext(val->pNext);
    size += vn_sizeof_VkDrmFormatModifierPropertiesListEXT_self(val);

    return size;
}

static inline void
vn_decode_VkDrmFormatModifierPropertiesListEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkDrmFormatModifierPropertiesListEXT_self(struct vn_cs *cs, VkDrmFormatModifierPropertiesListEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->drmFormatModifierCount);
    if (vn_peek_array_size(cs)) {
        vn_decode_array_size(cs, val->drmFormatModifierCount);
        for (uint32_t i = 0; i < val->drmFormatModifierCount; i++)
            vn_decode_VkDrmFormatModifierPropertiesEXT(cs, &val->pDrmFormatModifierProperties[i]);
    } else {
        vn_decode_array_size(cs, 0);
        val->pDrmFormatModifierProperties = NULL;
    }
}

static inline void
vn_decode_VkDrmFormatModifierPropertiesListEXT(struct vn_cs *cs, VkDrmFormatModifierPropertiesListEXT *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT);

    assert(val->sType == stype);
    vn_decode_VkDrmFormatModifierPropertiesListEXT_pnext(cs, val->pNext);
    vn_decode_VkDrmFormatModifierPropertiesListEXT_self(cs, val);
}

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesListEXT_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesListEXT_self_partial(const VkDrmFormatModifierPropertiesListEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->drmFormatModifierCount */
    if (val->pDrmFormatModifierProperties) {
        size += vn_sizeof_array_size(val->drmFormatModifierCount);
        for (uint32_t i = 0; i < val->drmFormatModifierCount; i++)
            size += vn_sizeof_VkDrmFormatModifierPropertiesEXT_partial(&val->pDrmFormatModifierProperties[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkDrmFormatModifierPropertiesListEXT_partial(const VkDrmFormatModifierPropertiesListEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDrmFormatModifierPropertiesListEXT_pnext_partial(val->pNext);
    size += vn_sizeof_VkDrmFormatModifierPropertiesListEXT_self_partial(val);

    return size;
}

static inline void
vn_encode_VkDrmFormatModifierPropertiesListEXT_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDrmFormatModifierPropertiesListEXT_self_partial(struct vn_cs *cs, const VkDrmFormatModifierPropertiesListEXT *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->drmFormatModifierCount */
    if (val->pDrmFormatModifierProperties) {
        vn_encode_array_size(cs, val->drmFormatModifierCount);
        for (uint32_t i = 0; i < val->drmFormatModifierCount; i++)
            vn_encode_VkDrmFormatModifierPropertiesEXT_partial(cs, &val->pDrmFormatModifierProperties[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkDrmFormatModifierPropertiesListEXT_partial(struct vn_cs *cs, const VkDrmFormatModifierPropertiesListEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT });
    vn_encode_VkDrmFormatModifierPropertiesListEXT_pnext_partial(cs, val->pNext);
    vn_encode_VkDrmFormatModifierPropertiesListEXT_self_partial(cs, val);
}

/* struct VkFormatProperties2 chain */

static inline size_t
vn_sizeof_VkFormatProperties2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkFormatProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkDrmFormatModifierPropertiesListEXT_self((const VkDrmFormatModifierPropertiesListEXT *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkFormatProperties2_self(const VkFormatProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFormatProperties(&val->formatProperties);
    return size;
}

static inline size_t
vn_sizeof_VkFormatProperties2(const VkFormatProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkFormatProperties2_pnext(val->pNext);
    size += vn_sizeof_VkFormatProperties2_self(val);

    return size;
}

static inline void
vn_decode_VkFormatProperties2_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:
        vn_decode_VkFormatProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkDrmFormatModifierPropertiesListEXT_self(cs, (VkDrmFormatModifierPropertiesListEXT *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkFormatProperties2_self(struct vn_cs *cs, VkFormatProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkFormatProperties(cs, &val->formatProperties);
}

static inline void
vn_decode_VkFormatProperties2(struct vn_cs *cs, VkFormatProperties2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);

    assert(val->sType == stype);
    vn_decode_VkFormatProperties2_pnext(cs, val->pNext);
    vn_decode_VkFormatProperties2_self(cs, val);
}

static inline size_t
vn_sizeof_VkFormatProperties2_pnext_partial(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkFormatProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkDrmFormatModifierPropertiesListEXT_self_partial((const VkDrmFormatModifierPropertiesListEXT *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkFormatProperties2_self_partial(const VkFormatProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFormatProperties_partial(&val->formatProperties);
    return size;
}

static inline size_t
vn_sizeof_VkFormatProperties2_partial(const VkFormatProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkFormatProperties2_pnext_partial(val->pNext);
    size += vn_sizeof_VkFormatProperties2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkFormatProperties2_pnext_partial(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkFormatProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkDrmFormatModifierPropertiesListEXT_self_partial(cs, (const VkDrmFormatModifierPropertiesListEXT *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkFormatProperties2_self_partial(struct vn_cs *cs, const VkFormatProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFormatProperties_partial(cs, &val->formatProperties);
}

static inline void
vn_encode_VkFormatProperties2_partial(struct vn_cs *cs, const VkFormatProperties2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 });
    vn_encode_VkFormatProperties2_pnext_partial(cs, val->pNext);
    vn_encode_VkFormatProperties2_self_partial(cs, val);
}

/* struct VkExternalMemoryProperties */

static inline size_t
vn_sizeof_VkExternalMemoryProperties(const VkExternalMemoryProperties *val)
{
    size_t size = 0;
    size += vn_sizeof_VkFlags(&val->externalMemoryFeatures);
    size += vn_sizeof_VkFlags(&val->exportFromImportedHandleTypes);
    size += vn_sizeof_VkFlags(&val->compatibleHandleTypes);
    return size;
}

static inline void
vn_decode_VkExternalMemoryProperties(struct vn_cs *cs, VkExternalMemoryProperties *val)
{
    vn_decode_VkFlags(cs, &val->externalMemoryFeatures);
    vn_decode_VkFlags(cs, &val->exportFromImportedHandleTypes);
    vn_decode_VkFlags(cs, &val->compatibleHandleTypes);
}

static inline size_t
vn_sizeof_VkExternalMemoryProperties_partial(const VkExternalMemoryProperties *val)
{
    size_t size = 0;
    /* skip val->externalMemoryFeatures */
    /* skip val->exportFromImportedHandleTypes */
    /* skip val->compatibleHandleTypes */
    return size;
}

static inline void
vn_encode_VkExternalMemoryProperties_partial(struct vn_cs *cs, const VkExternalMemoryProperties *val)
{
    /* skip val->externalMemoryFeatures */
    /* skip val->exportFromImportedHandleTypes */
    /* skip val->compatibleHandleTypes */
}

/* struct VkExternalImageFormatProperties chain */

static inline size_t
vn_sizeof_VkExternalImageFormatProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalImageFormatProperties_self(const VkExternalImageFormatProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkExternalMemoryProperties(&val->externalMemoryProperties);
    return size;
}

static inline size_t
vn_sizeof_VkExternalImageFormatProperties(const VkExternalImageFormatProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalImageFormatProperties_pnext(val->pNext);
    size += vn_sizeof_VkExternalImageFormatProperties_self(val);

    return size;
}

static inline void
vn_decode_VkExternalImageFormatProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkExternalImageFormatProperties_self(struct vn_cs *cs, VkExternalImageFormatProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkExternalMemoryProperties(cs, &val->externalMemoryProperties);
}

static inline void
vn_decode_VkExternalImageFormatProperties(struct vn_cs *cs, VkExternalImageFormatProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkExternalImageFormatProperties_pnext(cs, val->pNext);
    vn_decode_VkExternalImageFormatProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkExternalImageFormatProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalImageFormatProperties_self_partial(const VkExternalImageFormatProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkExternalMemoryProperties_partial(&val->externalMemoryProperties);
    return size;
}

static inline size_t
vn_sizeof_VkExternalImageFormatProperties_partial(const VkExternalImageFormatProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalImageFormatProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkExternalImageFormatProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkExternalImageFormatProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExternalImageFormatProperties_self_partial(struct vn_cs *cs, const VkExternalImageFormatProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkExternalMemoryProperties_partial(cs, &val->externalMemoryProperties);
}

static inline void
vn_encode_VkExternalImageFormatProperties_partial(struct vn_cs *cs, const VkExternalImageFormatProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES });
    vn_encode_VkExternalImageFormatProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkExternalImageFormatProperties_self_partial(cs, val);
}

/* struct VkSamplerYcbcrConversionImageFormatProperties chain */

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_self(const VkSamplerYcbcrConversionImageFormatProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->combinedImageSamplerDescriptorCount);
    return size;
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties(const VkSamplerYcbcrConversionImageFormatProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_pnext(val->pNext);
    size += vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_self(val);

    return size;
}

static inline void
vn_decode_VkSamplerYcbcrConversionImageFormatProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkSamplerYcbcrConversionImageFormatProperties_self(struct vn_cs *cs, VkSamplerYcbcrConversionImageFormatProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->combinedImageSamplerDescriptorCount);
}

static inline void
vn_decode_VkSamplerYcbcrConversionImageFormatProperties(struct vn_cs *cs, VkSamplerYcbcrConversionImageFormatProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkSamplerYcbcrConversionImageFormatProperties_pnext(cs, val->pNext);
    vn_decode_VkSamplerYcbcrConversionImageFormatProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_self_partial(const VkSamplerYcbcrConversionImageFormatProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->combinedImageSamplerDescriptorCount */
    return size;
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_partial(const VkSamplerYcbcrConversionImageFormatProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkSamplerYcbcrConversionImageFormatProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSamplerYcbcrConversionImageFormatProperties_self_partial(struct vn_cs *cs, const VkSamplerYcbcrConversionImageFormatProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->combinedImageSamplerDescriptorCount */
}

static inline void
vn_encode_VkSamplerYcbcrConversionImageFormatProperties_partial(struct vn_cs *cs, const VkSamplerYcbcrConversionImageFormatProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES });
    vn_encode_VkSamplerYcbcrConversionImageFormatProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkSamplerYcbcrConversionImageFormatProperties_self_partial(cs, val);
}

/* struct VkImageFormatProperties2 chain */

static inline size_t
vn_sizeof_VkImageFormatProperties2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageFormatProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkExternalImageFormatProperties_self((const VkExternalImageFormatProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageFormatProperties2_pnext(pnext->pNext);
            size += vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_self((const VkSamplerYcbcrConversionImageFormatProperties *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageFormatProperties2_self(const VkImageFormatProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImageFormatProperties(&val->imageFormatProperties);
    return size;
}

static inline size_t
vn_sizeof_VkImageFormatProperties2(const VkImageFormatProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageFormatProperties2_pnext(val->pNext);
    size += vn_sizeof_VkImageFormatProperties2_self(val);

    return size;
}

static inline void
vn_decode_VkImageFormatProperties2_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
        vn_decode_VkImageFormatProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkExternalImageFormatProperties_self(cs, (VkExternalImageFormatProperties *)pnext);
        break;
    case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
        vn_decode_VkImageFormatProperties2_pnext(cs, pnext->pNext);
        vn_decode_VkSamplerYcbcrConversionImageFormatProperties_self(cs, (VkSamplerYcbcrConversionImageFormatProperties *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkImageFormatProperties2_self(struct vn_cs *cs, VkImageFormatProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkImageFormatProperties(cs, &val->imageFormatProperties);
}

static inline void
vn_decode_VkImageFormatProperties2(struct vn_cs *cs, VkImageFormatProperties2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2);

    assert(val->sType == stype);
    vn_decode_VkImageFormatProperties2_pnext(cs, val->pNext);
    vn_decode_VkImageFormatProperties2_self(cs, val);
}

static inline size_t
vn_sizeof_VkImageFormatProperties2_pnext_partial(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageFormatProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkExternalImageFormatProperties_self_partial((const VkExternalImageFormatProperties *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageFormatProperties2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkSamplerYcbcrConversionImageFormatProperties_self_partial((const VkSamplerYcbcrConversionImageFormatProperties *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageFormatProperties2_self_partial(const VkImageFormatProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImageFormatProperties_partial(&val->imageFormatProperties);
    return size;
}

static inline size_t
vn_sizeof_VkImageFormatProperties2_partial(const VkImageFormatProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageFormatProperties2_pnext_partial(val->pNext);
    size += vn_sizeof_VkImageFormatProperties2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkImageFormatProperties2_pnext_partial(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageFormatProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkExternalImageFormatProperties_self_partial(cs, (const VkExternalImageFormatProperties *)pnext);
            return;
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageFormatProperties2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkSamplerYcbcrConversionImageFormatProperties_self_partial(cs, (const VkSamplerYcbcrConversionImageFormatProperties *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageFormatProperties2_self_partial(struct vn_cs *cs, const VkImageFormatProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImageFormatProperties_partial(cs, &val->imageFormatProperties);
}

static inline void
vn_encode_VkImageFormatProperties2_partial(struct vn_cs *cs, const VkImageFormatProperties2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 });
    vn_encode_VkImageFormatProperties2_pnext_partial(cs, val->pNext);
    vn_encode_VkImageFormatProperties2_self_partial(cs, val);
}

/* struct VkPhysicalDeviceExternalImageFormatInfo chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalImageFormatInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalImageFormatInfo_self(const VkPhysicalDeviceExternalImageFormatInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkExternalMemoryHandleTypeFlagBits(&val->handleType);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalImageFormatInfo(const VkPhysicalDeviceExternalImageFormatInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceExternalImageFormatInfo_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceExternalImageFormatInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceExternalImageFormatInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceExternalImageFormatInfo_self(struct vn_cs *cs, const VkPhysicalDeviceExternalImageFormatInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkExternalMemoryHandleTypeFlagBits(cs, &val->handleType);
}

static inline void
vn_encode_VkPhysicalDeviceExternalImageFormatInfo(struct vn_cs *cs, const VkPhysicalDeviceExternalImageFormatInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO });
    vn_encode_VkPhysicalDeviceExternalImageFormatInfo_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceExternalImageFormatInfo_self(cs, val);
}

/* struct VkPhysicalDeviceImageDrmFormatModifierInfoEXT chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_self(const VkPhysicalDeviceImageDrmFormatModifierInfoEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint64_t(&val->drmFormatModifier);
    size += vn_sizeof_VkSharingMode(&val->sharingMode);
    size += vn_sizeof_uint32_t(&val->queueFamilyIndexCount);
    if (val->pQueueFamilyIndices) {
        size += vn_sizeof_array_size(val->queueFamilyIndexCount);
        size += vn_sizeof_uint32_t_array(val->pQueueFamilyIndices, val->queueFamilyIndexCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImageDrmFormatModifierInfoEXT(const VkPhysicalDeviceImageDrmFormatModifierInfoEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_self(struct vn_cs *cs, const VkPhysicalDeviceImageDrmFormatModifierInfoEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint64_t(cs, &val->drmFormatModifier);
    vn_encode_VkSharingMode(cs, &val->sharingMode);
    vn_encode_uint32_t(cs, &val->queueFamilyIndexCount);
    if (val->pQueueFamilyIndices) {
        vn_encode_array_size(cs, val->queueFamilyIndexCount);
        vn_encode_uint32_t_array(cs, val->pQueueFamilyIndices, val->queueFamilyIndexCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkPhysicalDeviceImageDrmFormatModifierInfoEXT(struct vn_cs *cs, const VkPhysicalDeviceImageDrmFormatModifierInfoEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT });
    vn_encode_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_self(cs, val);
}

/* struct VkPhysicalDeviceImageFormatInfo2 chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceImageFormatInfo2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceImageFormatInfo2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceExternalImageFormatInfo_self((const VkPhysicalDeviceExternalImageFormatInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceImageFormatInfo2_pnext(pnext->pNext);
            size += vn_sizeof_VkImageFormatListCreateInfo_self((const VkImageFormatListCreateInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceImageFormatInfo2_pnext(pnext->pNext);
            size += vn_sizeof_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_self((const VkPhysicalDeviceImageDrmFormatModifierInfoEXT *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceImageFormatInfo2_pnext(pnext->pNext);
            size += vn_sizeof_VkImageStencilUsageCreateInfo_self((const VkImageStencilUsageCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImageFormatInfo2_self(const VkPhysicalDeviceImageFormatInfo2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkImageType(&val->type);
    size += vn_sizeof_VkImageTiling(&val->tiling);
    size += vn_sizeof_VkFlags(&val->usage);
    size += vn_sizeof_VkFlags(&val->flags);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceImageFormatInfo2(const VkPhysicalDeviceImageFormatInfo2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceImageFormatInfo2_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceImageFormatInfo2_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceImageFormatInfo2_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceImageFormatInfo2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceExternalImageFormatInfo_self(cs, (const VkPhysicalDeviceExternalImageFormatInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceImageFormatInfo2_pnext(cs, pnext->pNext);
            vn_encode_VkImageFormatListCreateInfo_self(cs, (const VkImageFormatListCreateInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceImageFormatInfo2_pnext(cs, pnext->pNext);
            vn_encode_VkPhysicalDeviceImageDrmFormatModifierInfoEXT_self(cs, (const VkPhysicalDeviceImageDrmFormatModifierInfoEXT *)pnext);
            return;
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceImageFormatInfo2_pnext(cs, pnext->pNext);
            vn_encode_VkImageStencilUsageCreateInfo_self(cs, (const VkImageStencilUsageCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceImageFormatInfo2_self(struct vn_cs *cs, const VkPhysicalDeviceImageFormatInfo2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkImageType(cs, &val->type);
    vn_encode_VkImageTiling(cs, &val->tiling);
    vn_encode_VkFlags(cs, &val->usage);
    vn_encode_VkFlags(cs, &val->flags);
}

static inline void
vn_encode_VkPhysicalDeviceImageFormatInfo2(struct vn_cs *cs, const VkPhysicalDeviceImageFormatInfo2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 });
    vn_encode_VkPhysicalDeviceImageFormatInfo2_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceImageFormatInfo2_self(cs, val);
}

/* struct VkQueueFamilyProperties2 chain */

static inline size_t
vn_sizeof_VkQueueFamilyProperties2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkQueueFamilyProperties2_self(const VkQueueFamilyProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkQueueFamilyProperties(&val->queueFamilyProperties);
    return size;
}

static inline size_t
vn_sizeof_VkQueueFamilyProperties2(const VkQueueFamilyProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkQueueFamilyProperties2_pnext(val->pNext);
    size += vn_sizeof_VkQueueFamilyProperties2_self(val);

    return size;
}

static inline void
vn_decode_VkQueueFamilyProperties2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkQueueFamilyProperties2_self(struct vn_cs *cs, VkQueueFamilyProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkQueueFamilyProperties(cs, &val->queueFamilyProperties);
}

static inline void
vn_decode_VkQueueFamilyProperties2(struct vn_cs *cs, VkQueueFamilyProperties2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);

    assert(val->sType == stype);
    vn_decode_VkQueueFamilyProperties2_pnext(cs, val->pNext);
    vn_decode_VkQueueFamilyProperties2_self(cs, val);
}

static inline size_t
vn_sizeof_VkQueueFamilyProperties2_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkQueueFamilyProperties2_self_partial(const VkQueueFamilyProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkQueueFamilyProperties_partial(&val->queueFamilyProperties);
    return size;
}

static inline size_t
vn_sizeof_VkQueueFamilyProperties2_partial(const VkQueueFamilyProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkQueueFamilyProperties2_pnext_partial(val->pNext);
    size += vn_sizeof_VkQueueFamilyProperties2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkQueueFamilyProperties2_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkQueueFamilyProperties2_self_partial(struct vn_cs *cs, const VkQueueFamilyProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkQueueFamilyProperties_partial(cs, &val->queueFamilyProperties);
}

static inline void
vn_encode_VkQueueFamilyProperties2_partial(struct vn_cs *cs, const VkQueueFamilyProperties2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
    vn_encode_VkQueueFamilyProperties2_pnext_partial(cs, val->pNext);
    vn_encode_VkQueueFamilyProperties2_self_partial(cs, val);
}

/* struct VkPhysicalDeviceMemoryProperties2 chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties2_self(const VkPhysicalDeviceMemoryProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkPhysicalDeviceMemoryProperties(&val->memoryProperties);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties2(const VkPhysicalDeviceMemoryProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMemoryProperties2_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMemoryProperties2_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceMemoryProperties2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceMemoryProperties2_self(struct vn_cs *cs, VkPhysicalDeviceMemoryProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkPhysicalDeviceMemoryProperties(cs, &val->memoryProperties);
}

static inline void
vn_decode_VkPhysicalDeviceMemoryProperties2(struct vn_cs *cs, VkPhysicalDeviceMemoryProperties2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceMemoryProperties2_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceMemoryProperties2_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties2_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties2_self_partial(const VkPhysicalDeviceMemoryProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkPhysicalDeviceMemoryProperties_partial(&val->memoryProperties);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceMemoryProperties2_partial(const VkPhysicalDeviceMemoryProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceMemoryProperties2_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceMemoryProperties2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceMemoryProperties2_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceMemoryProperties2_self_partial(struct vn_cs *cs, const VkPhysicalDeviceMemoryProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkPhysicalDeviceMemoryProperties_partial(cs, &val->memoryProperties);
}

static inline void
vn_encode_VkPhysicalDeviceMemoryProperties2_partial(struct vn_cs *cs, const VkPhysicalDeviceMemoryProperties2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 });
    vn_encode_VkPhysicalDeviceMemoryProperties2_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceMemoryProperties2_self_partial(cs, val);
}

/* struct VkSparseImageFormatProperties2 chain */

static inline size_t
vn_sizeof_VkSparseImageFormatProperties2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSparseImageFormatProperties2_self(const VkSparseImageFormatProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSparseImageFormatProperties(&val->properties);
    return size;
}

static inline size_t
vn_sizeof_VkSparseImageFormatProperties2(const VkSparseImageFormatProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSparseImageFormatProperties2_pnext(val->pNext);
    size += vn_sizeof_VkSparseImageFormatProperties2_self(val);

    return size;
}

static inline void
vn_decode_VkSparseImageFormatProperties2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkSparseImageFormatProperties2_self(struct vn_cs *cs, VkSparseImageFormatProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkSparseImageFormatProperties(cs, &val->properties);
}

static inline void
vn_decode_VkSparseImageFormatProperties2(struct vn_cs *cs, VkSparseImageFormatProperties2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2);

    assert(val->sType == stype);
    vn_decode_VkSparseImageFormatProperties2_pnext(cs, val->pNext);
    vn_decode_VkSparseImageFormatProperties2_self(cs, val);
}

static inline size_t
vn_sizeof_VkSparseImageFormatProperties2_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSparseImageFormatProperties2_self_partial(const VkSparseImageFormatProperties2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSparseImageFormatProperties_partial(&val->properties);
    return size;
}

static inline size_t
vn_sizeof_VkSparseImageFormatProperties2_partial(const VkSparseImageFormatProperties2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSparseImageFormatProperties2_pnext_partial(val->pNext);
    size += vn_sizeof_VkSparseImageFormatProperties2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkSparseImageFormatProperties2_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSparseImageFormatProperties2_self_partial(struct vn_cs *cs, const VkSparseImageFormatProperties2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkSparseImageFormatProperties_partial(cs, &val->properties);
}

static inline void
vn_encode_VkSparseImageFormatProperties2_partial(struct vn_cs *cs, const VkSparseImageFormatProperties2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2 });
    vn_encode_VkSparseImageFormatProperties2_pnext_partial(cs, val->pNext);
    vn_encode_VkSparseImageFormatProperties2_self_partial(cs, val);
}

/* struct VkPhysicalDeviceSparseImageFormatInfo2 chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceSparseImageFormatInfo2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSparseImageFormatInfo2_self(const VkPhysicalDeviceSparseImageFormatInfo2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkImageType(&val->type);
    size += vn_sizeof_VkSampleCountFlagBits(&val->samples);
    size += vn_sizeof_VkFlags(&val->usage);
    size += vn_sizeof_VkImageTiling(&val->tiling);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceSparseImageFormatInfo2(const VkPhysicalDeviceSparseImageFormatInfo2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceSparseImageFormatInfo2_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceSparseImageFormatInfo2_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceSparseImageFormatInfo2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceSparseImageFormatInfo2_self(struct vn_cs *cs, const VkPhysicalDeviceSparseImageFormatInfo2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkImageType(cs, &val->type);
    vn_encode_VkSampleCountFlagBits(cs, &val->samples);
    vn_encode_VkFlags(cs, &val->usage);
    vn_encode_VkImageTiling(cs, &val->tiling);
}

static inline void
vn_encode_VkPhysicalDeviceSparseImageFormatInfo2(struct vn_cs *cs, const VkPhysicalDeviceSparseImageFormatInfo2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2 });
    vn_encode_VkPhysicalDeviceSparseImageFormatInfo2_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceSparseImageFormatInfo2_self(cs, val);
}

/* struct VkPhysicalDeviceExternalBufferInfo chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalBufferInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalBufferInfo_self(const VkPhysicalDeviceExternalBufferInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkFlags(&val->usage);
    size += vn_sizeof_VkExternalMemoryHandleTypeFlagBits(&val->handleType);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalBufferInfo(const VkPhysicalDeviceExternalBufferInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceExternalBufferInfo_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceExternalBufferInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceExternalBufferInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceExternalBufferInfo_self(struct vn_cs *cs, const VkPhysicalDeviceExternalBufferInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkFlags(cs, &val->usage);
    vn_encode_VkExternalMemoryHandleTypeFlagBits(cs, &val->handleType);
}

static inline void
vn_encode_VkPhysicalDeviceExternalBufferInfo(struct vn_cs *cs, const VkPhysicalDeviceExternalBufferInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO });
    vn_encode_VkPhysicalDeviceExternalBufferInfo_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceExternalBufferInfo_self(cs, val);
}

/* struct VkExternalBufferProperties chain */

static inline size_t
vn_sizeof_VkExternalBufferProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalBufferProperties_self(const VkExternalBufferProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkExternalMemoryProperties(&val->externalMemoryProperties);
    return size;
}

static inline size_t
vn_sizeof_VkExternalBufferProperties(const VkExternalBufferProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalBufferProperties_pnext(val->pNext);
    size += vn_sizeof_VkExternalBufferProperties_self(val);

    return size;
}

static inline void
vn_decode_VkExternalBufferProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkExternalBufferProperties_self(struct vn_cs *cs, VkExternalBufferProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkExternalMemoryProperties(cs, &val->externalMemoryProperties);
}

static inline void
vn_decode_VkExternalBufferProperties(struct vn_cs *cs, VkExternalBufferProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkExternalBufferProperties_pnext(cs, val->pNext);
    vn_decode_VkExternalBufferProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkExternalBufferProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalBufferProperties_self_partial(const VkExternalBufferProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkExternalMemoryProperties_partial(&val->externalMemoryProperties);
    return size;
}

static inline size_t
vn_sizeof_VkExternalBufferProperties_partial(const VkExternalBufferProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalBufferProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkExternalBufferProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkExternalBufferProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExternalBufferProperties_self_partial(struct vn_cs *cs, const VkExternalBufferProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkExternalMemoryProperties_partial(cs, &val->externalMemoryProperties);
}

static inline void
vn_encode_VkExternalBufferProperties_partial(struct vn_cs *cs, const VkExternalBufferProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES });
    vn_encode_VkExternalBufferProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkExternalBufferProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceExternalSemaphoreInfo chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalSemaphoreInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkPhysicalDeviceExternalSemaphoreInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkSemaphoreTypeCreateInfo_self((const VkSemaphoreTypeCreateInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalSemaphoreInfo_self(const VkPhysicalDeviceExternalSemaphoreInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkExternalSemaphoreHandleTypeFlagBits(&val->handleType);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalSemaphoreInfo(const VkPhysicalDeviceExternalSemaphoreInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceExternalSemaphoreInfo_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceExternalSemaphoreInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceExternalSemaphoreInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkPhysicalDeviceExternalSemaphoreInfo_pnext(cs, pnext->pNext);
            vn_encode_VkSemaphoreTypeCreateInfo_self(cs, (const VkSemaphoreTypeCreateInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceExternalSemaphoreInfo_self(struct vn_cs *cs, const VkPhysicalDeviceExternalSemaphoreInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkExternalSemaphoreHandleTypeFlagBits(cs, &val->handleType);
}

static inline void
vn_encode_VkPhysicalDeviceExternalSemaphoreInfo(struct vn_cs *cs, const VkPhysicalDeviceExternalSemaphoreInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO });
    vn_encode_VkPhysicalDeviceExternalSemaphoreInfo_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceExternalSemaphoreInfo_self(cs, val);
}

/* struct VkExternalSemaphoreProperties chain */

static inline size_t
vn_sizeof_VkExternalSemaphoreProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalSemaphoreProperties_self(const VkExternalSemaphoreProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->exportFromImportedHandleTypes);
    size += vn_sizeof_VkFlags(&val->compatibleHandleTypes);
    size += vn_sizeof_VkFlags(&val->externalSemaphoreFeatures);
    return size;
}

static inline size_t
vn_sizeof_VkExternalSemaphoreProperties(const VkExternalSemaphoreProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalSemaphoreProperties_pnext(val->pNext);
    size += vn_sizeof_VkExternalSemaphoreProperties_self(val);

    return size;
}

static inline void
vn_decode_VkExternalSemaphoreProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkExternalSemaphoreProperties_self(struct vn_cs *cs, VkExternalSemaphoreProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkFlags(cs, &val->exportFromImportedHandleTypes);
    vn_decode_VkFlags(cs, &val->compatibleHandleTypes);
    vn_decode_VkFlags(cs, &val->externalSemaphoreFeatures);
}

static inline void
vn_decode_VkExternalSemaphoreProperties(struct vn_cs *cs, VkExternalSemaphoreProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkExternalSemaphoreProperties_pnext(cs, val->pNext);
    vn_decode_VkExternalSemaphoreProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkExternalSemaphoreProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalSemaphoreProperties_self_partial(const VkExternalSemaphoreProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->exportFromImportedHandleTypes */
    /* skip val->compatibleHandleTypes */
    /* skip val->externalSemaphoreFeatures */
    return size;
}

static inline size_t
vn_sizeof_VkExternalSemaphoreProperties_partial(const VkExternalSemaphoreProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalSemaphoreProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkExternalSemaphoreProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkExternalSemaphoreProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExternalSemaphoreProperties_self_partial(struct vn_cs *cs, const VkExternalSemaphoreProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->exportFromImportedHandleTypes */
    /* skip val->compatibleHandleTypes */
    /* skip val->externalSemaphoreFeatures */
}

static inline void
vn_encode_VkExternalSemaphoreProperties_partial(struct vn_cs *cs, const VkExternalSemaphoreProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES });
    vn_encode_VkExternalSemaphoreProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkExternalSemaphoreProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceExternalFenceInfo chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalFenceInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalFenceInfo_self(const VkPhysicalDeviceExternalFenceInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkExternalFenceHandleTypeFlagBits(&val->handleType);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceExternalFenceInfo(const VkPhysicalDeviceExternalFenceInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceExternalFenceInfo_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceExternalFenceInfo_self(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceExternalFenceInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceExternalFenceInfo_self(struct vn_cs *cs, const VkPhysicalDeviceExternalFenceInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkExternalFenceHandleTypeFlagBits(cs, &val->handleType);
}

static inline void
vn_encode_VkPhysicalDeviceExternalFenceInfo(struct vn_cs *cs, const VkPhysicalDeviceExternalFenceInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO });
    vn_encode_VkPhysicalDeviceExternalFenceInfo_pnext(cs, val->pNext);
    vn_encode_VkPhysicalDeviceExternalFenceInfo_self(cs, val);
}

/* struct VkExternalFenceProperties chain */

static inline size_t
vn_sizeof_VkExternalFenceProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalFenceProperties_self(const VkExternalFenceProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->exportFromImportedHandleTypes);
    size += vn_sizeof_VkFlags(&val->compatibleHandleTypes);
    size += vn_sizeof_VkFlags(&val->externalFenceFeatures);
    return size;
}

static inline size_t
vn_sizeof_VkExternalFenceProperties(const VkExternalFenceProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalFenceProperties_pnext(val->pNext);
    size += vn_sizeof_VkExternalFenceProperties_self(val);

    return size;
}

static inline void
vn_decode_VkExternalFenceProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkExternalFenceProperties_self(struct vn_cs *cs, VkExternalFenceProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkFlags(cs, &val->exportFromImportedHandleTypes);
    vn_decode_VkFlags(cs, &val->compatibleHandleTypes);
    vn_decode_VkFlags(cs, &val->externalFenceFeatures);
}

static inline void
vn_decode_VkExternalFenceProperties(struct vn_cs *cs, VkExternalFenceProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkExternalFenceProperties_pnext(cs, val->pNext);
    vn_decode_VkExternalFenceProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkExternalFenceProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkExternalFenceProperties_self_partial(const VkExternalFenceProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->exportFromImportedHandleTypes */
    /* skip val->compatibleHandleTypes */
    /* skip val->externalFenceFeatures */
    return size;
}

static inline size_t
vn_sizeof_VkExternalFenceProperties_partial(const VkExternalFenceProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkExternalFenceProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkExternalFenceProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkExternalFenceProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkExternalFenceProperties_self_partial(struct vn_cs *cs, const VkExternalFenceProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->exportFromImportedHandleTypes */
    /* skip val->compatibleHandleTypes */
    /* skip val->externalFenceFeatures */
}

static inline void
vn_encode_VkExternalFenceProperties_partial(struct vn_cs *cs, const VkExternalFenceProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES });
    vn_encode_VkExternalFenceProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkExternalFenceProperties_self_partial(cs, val);
}

/* struct VkPhysicalDeviceGroupProperties chain */

static inline size_t
vn_sizeof_VkPhysicalDeviceGroupProperties_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceGroupProperties_self(const VkPhysicalDeviceGroupProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->physicalDeviceCount);
    size += vn_sizeof_array_size(VK_MAX_DEVICE_GROUP_SIZE);
    for (uint32_t i = 0; i < VK_MAX_DEVICE_GROUP_SIZE; i++)
        size += vn_sizeof_VkPhysicalDevice(&val->physicalDevices[i]);
    size += vn_sizeof_VkBool32(&val->subsetAllocation);
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceGroupProperties(const VkPhysicalDeviceGroupProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceGroupProperties_pnext(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceGroupProperties_self(val);

    return size;
}

static inline void
vn_decode_VkPhysicalDeviceGroupProperties_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkPhysicalDeviceGroupProperties_self(struct vn_cs *cs, VkPhysicalDeviceGroupProperties *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->physicalDeviceCount);
    {
        vn_decode_array_size(cs, VK_MAX_DEVICE_GROUP_SIZE);
        for (uint32_t i = 0; i < VK_MAX_DEVICE_GROUP_SIZE; i++)
            vn_decode_VkPhysicalDevice(cs, &val->physicalDevices[i]);
    }
    vn_decode_VkBool32(cs, &val->subsetAllocation);
}

static inline void
vn_decode_VkPhysicalDeviceGroupProperties(struct vn_cs *cs, VkPhysicalDeviceGroupProperties *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES);

    assert(val->sType == stype);
    vn_decode_VkPhysicalDeviceGroupProperties_pnext(cs, val->pNext);
    vn_decode_VkPhysicalDeviceGroupProperties_self(cs, val);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceGroupProperties_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkPhysicalDeviceGroupProperties_self_partial(const VkPhysicalDeviceGroupProperties *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->physicalDeviceCount */
    size += vn_sizeof_array_size(VK_MAX_DEVICE_GROUP_SIZE);
    for (uint32_t i = 0; i < VK_MAX_DEVICE_GROUP_SIZE; i++)
        size += vn_sizeof_VkPhysicalDevice(&val->physicalDevices[i]);
    /* skip val->subsetAllocation */
    return size;
}

static inline size_t
vn_sizeof_VkPhysicalDeviceGroupProperties_partial(const VkPhysicalDeviceGroupProperties *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkPhysicalDeviceGroupProperties_pnext_partial(val->pNext);
    size += vn_sizeof_VkPhysicalDeviceGroupProperties_self_partial(val);

    return size;
}

static inline void
vn_encode_VkPhysicalDeviceGroupProperties_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkPhysicalDeviceGroupProperties_self_partial(struct vn_cs *cs, const VkPhysicalDeviceGroupProperties *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->physicalDeviceCount */
    vn_encode_array_size(cs, VK_MAX_DEVICE_GROUP_SIZE);
    for (uint32_t i = 0; i < VK_MAX_DEVICE_GROUP_SIZE; i++)
        vn_encode_VkPhysicalDevice(cs, &val->physicalDevices[i]);
    /* skip val->subsetAllocation */
}

static inline void
vn_encode_VkPhysicalDeviceGroupProperties_partial(struct vn_cs *cs, const VkPhysicalDeviceGroupProperties *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES });
    vn_encode_VkPhysicalDeviceGroupProperties_pnext_partial(cs, val->pNext);
    vn_encode_VkPhysicalDeviceGroupProperties_self_partial(cs, val);
}

/* struct VkBindBufferMemoryDeviceGroupInfo chain */

static inline size_t
vn_sizeof_VkBindBufferMemoryDeviceGroupInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBindBufferMemoryDeviceGroupInfo_self(const VkBindBufferMemoryDeviceGroupInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->deviceIndexCount);
    if (val->pDeviceIndices) {
        size += vn_sizeof_array_size(val->deviceIndexCount);
        size += vn_sizeof_uint32_t_array(val->pDeviceIndices, val->deviceIndexCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkBindBufferMemoryDeviceGroupInfo(const VkBindBufferMemoryDeviceGroupInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBindBufferMemoryDeviceGroupInfo_pnext(val->pNext);
    size += vn_sizeof_VkBindBufferMemoryDeviceGroupInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBindBufferMemoryDeviceGroupInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBindBufferMemoryDeviceGroupInfo_self(struct vn_cs *cs, const VkBindBufferMemoryDeviceGroupInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->deviceIndexCount);
    if (val->pDeviceIndices) {
        vn_encode_array_size(cs, val->deviceIndexCount);
        vn_encode_uint32_t_array(cs, val->pDeviceIndices, val->deviceIndexCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkBindBufferMemoryDeviceGroupInfo(struct vn_cs *cs, const VkBindBufferMemoryDeviceGroupInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO });
    vn_encode_VkBindBufferMemoryDeviceGroupInfo_pnext(cs, val->pNext);
    vn_encode_VkBindBufferMemoryDeviceGroupInfo_self(cs, val);
}

static inline void
vn_decode_VkBindBufferMemoryDeviceGroupInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkBindBufferMemoryDeviceGroupInfo_self(struct vn_cs *cs, VkBindBufferMemoryDeviceGroupInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->deviceIndexCount);
    if (vn_peek_array_size(cs)) {
        const size_t array_size = vn_decode_array_size(cs, val->deviceIndexCount);
        vn_decode_uint32_t_array(cs, (uint32_t *)val->pDeviceIndices, array_size);
    } else {
        vn_decode_array_size(cs, 0);
        val->pDeviceIndices = NULL;
    }
}

static inline void
vn_decode_VkBindBufferMemoryDeviceGroupInfo(struct vn_cs *cs, VkBindBufferMemoryDeviceGroupInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO);

    assert(val->sType == stype);
    vn_decode_VkBindBufferMemoryDeviceGroupInfo_pnext(cs, val->pNext);
    vn_decode_VkBindBufferMemoryDeviceGroupInfo_self(cs, val);
}

/* struct VkBindBufferMemoryInfo chain */

static inline size_t
vn_sizeof_VkBindBufferMemoryInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkBindBufferMemoryInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkBindBufferMemoryDeviceGroupInfo_self((const VkBindBufferMemoryDeviceGroupInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBindBufferMemoryInfo_self(const VkBindBufferMemoryInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBuffer(&val->buffer);
    size += vn_sizeof_VkDeviceMemory(&val->memory);
    size += vn_sizeof_VkDeviceSize(&val->memoryOffset);
    return size;
}

static inline size_t
vn_sizeof_VkBindBufferMemoryInfo(const VkBindBufferMemoryInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBindBufferMemoryInfo_pnext(val->pNext);
    size += vn_sizeof_VkBindBufferMemoryInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBindBufferMemoryInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkBindBufferMemoryInfo_pnext(cs, pnext->pNext);
            vn_encode_VkBindBufferMemoryDeviceGroupInfo_self(cs, (const VkBindBufferMemoryDeviceGroupInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBindBufferMemoryInfo_self(struct vn_cs *cs, const VkBindBufferMemoryInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBuffer(cs, &val->buffer);
    vn_encode_VkDeviceMemory(cs, &val->memory);
    vn_encode_VkDeviceSize(cs, &val->memoryOffset);
}

static inline void
vn_encode_VkBindBufferMemoryInfo(struct vn_cs *cs, const VkBindBufferMemoryInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO });
    vn_encode_VkBindBufferMemoryInfo_pnext(cs, val->pNext);
    vn_encode_VkBindBufferMemoryInfo_self(cs, val);
}

static inline void
vn_decode_VkBindBufferMemoryInfo_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO:
        vn_decode_VkBindBufferMemoryInfo_pnext(cs, pnext->pNext);
        vn_decode_VkBindBufferMemoryDeviceGroupInfo_self(cs, (VkBindBufferMemoryDeviceGroupInfo *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkBindBufferMemoryInfo_self(struct vn_cs *cs, VkBindBufferMemoryInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBuffer(cs, &val->buffer);
    vn_decode_VkDeviceMemory(cs, &val->memory);
    vn_decode_VkDeviceSize(cs, &val->memoryOffset);
}

static inline void
vn_decode_VkBindBufferMemoryInfo(struct vn_cs *cs, VkBindBufferMemoryInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO);

    assert(val->sType == stype);
    vn_decode_VkBindBufferMemoryInfo_pnext(cs, val->pNext);
    vn_decode_VkBindBufferMemoryInfo_self(cs, val);
}

/* struct VkBindImageMemoryDeviceGroupInfo chain */

static inline size_t
vn_sizeof_VkBindImageMemoryDeviceGroupInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBindImageMemoryDeviceGroupInfo_self(const VkBindImageMemoryDeviceGroupInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->deviceIndexCount);
    if (val->pDeviceIndices) {
        size += vn_sizeof_array_size(val->deviceIndexCount);
        size += vn_sizeof_uint32_t_array(val->pDeviceIndices, val->deviceIndexCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->splitInstanceBindRegionCount);
    if (val->pSplitInstanceBindRegions) {
        size += vn_sizeof_array_size(val->splitInstanceBindRegionCount);
        for (uint32_t i = 0; i < val->splitInstanceBindRegionCount; i++)
            size += vn_sizeof_VkRect2D(&val->pSplitInstanceBindRegions[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkBindImageMemoryDeviceGroupInfo(const VkBindImageMemoryDeviceGroupInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBindImageMemoryDeviceGroupInfo_pnext(val->pNext);
    size += vn_sizeof_VkBindImageMemoryDeviceGroupInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBindImageMemoryDeviceGroupInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBindImageMemoryDeviceGroupInfo_self(struct vn_cs *cs, const VkBindImageMemoryDeviceGroupInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->deviceIndexCount);
    if (val->pDeviceIndices) {
        vn_encode_array_size(cs, val->deviceIndexCount);
        vn_encode_uint32_t_array(cs, val->pDeviceIndices, val->deviceIndexCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->splitInstanceBindRegionCount);
    if (val->pSplitInstanceBindRegions) {
        vn_encode_array_size(cs, val->splitInstanceBindRegionCount);
        for (uint32_t i = 0; i < val->splitInstanceBindRegionCount; i++)
            vn_encode_VkRect2D(cs, &val->pSplitInstanceBindRegions[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkBindImageMemoryDeviceGroupInfo(struct vn_cs *cs, const VkBindImageMemoryDeviceGroupInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO });
    vn_encode_VkBindImageMemoryDeviceGroupInfo_pnext(cs, val->pNext);
    vn_encode_VkBindImageMemoryDeviceGroupInfo_self(cs, val);
}

static inline void
vn_decode_VkBindImageMemoryDeviceGroupInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkBindImageMemoryDeviceGroupInfo_self(struct vn_cs *cs, VkBindImageMemoryDeviceGroupInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->deviceIndexCount);
    if (vn_peek_array_size(cs)) {
        const size_t array_size = vn_decode_array_size(cs, val->deviceIndexCount);
        vn_decode_uint32_t_array(cs, (uint32_t *)val->pDeviceIndices, array_size);
    } else {
        vn_decode_array_size(cs, 0);
        val->pDeviceIndices = NULL;
    }
    vn_decode_uint32_t(cs, &val->splitInstanceBindRegionCount);
    if (vn_peek_array_size(cs)) {
        vn_decode_array_size(cs, val->splitInstanceBindRegionCount);
        for (uint32_t i = 0; i < val->splitInstanceBindRegionCount; i++)
            vn_decode_VkRect2D(cs, &((VkRect2D *)val->pSplitInstanceBindRegions)[i]);
    } else {
        vn_decode_array_size(cs, 0);
        val->pSplitInstanceBindRegions = NULL;
    }
}

static inline void
vn_decode_VkBindImageMemoryDeviceGroupInfo(struct vn_cs *cs, VkBindImageMemoryDeviceGroupInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO);

    assert(val->sType == stype);
    vn_decode_VkBindImageMemoryDeviceGroupInfo_pnext(cs, val->pNext);
    vn_decode_VkBindImageMemoryDeviceGroupInfo_self(cs, val);
}

/* struct VkBindImagePlaneMemoryInfo chain */

static inline size_t
vn_sizeof_VkBindImagePlaneMemoryInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBindImagePlaneMemoryInfo_self(const VkBindImagePlaneMemoryInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImageAspectFlagBits(&val->planeAspect);
    return size;
}

static inline size_t
vn_sizeof_VkBindImagePlaneMemoryInfo(const VkBindImagePlaneMemoryInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBindImagePlaneMemoryInfo_pnext(val->pNext);
    size += vn_sizeof_VkBindImagePlaneMemoryInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBindImagePlaneMemoryInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBindImagePlaneMemoryInfo_self(struct vn_cs *cs, const VkBindImagePlaneMemoryInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImageAspectFlagBits(cs, &val->planeAspect);
}

static inline void
vn_encode_VkBindImagePlaneMemoryInfo(struct vn_cs *cs, const VkBindImagePlaneMemoryInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO });
    vn_encode_VkBindImagePlaneMemoryInfo_pnext(cs, val->pNext);
    vn_encode_VkBindImagePlaneMemoryInfo_self(cs, val);
}

static inline void
vn_decode_VkBindImagePlaneMemoryInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkBindImagePlaneMemoryInfo_self(struct vn_cs *cs, VkBindImagePlaneMemoryInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkImageAspectFlagBits(cs, &val->planeAspect);
}

static inline void
vn_decode_VkBindImagePlaneMemoryInfo(struct vn_cs *cs, VkBindImagePlaneMemoryInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO);

    assert(val->sType == stype);
    vn_decode_VkBindImagePlaneMemoryInfo_pnext(cs, val->pNext);
    vn_decode_VkBindImagePlaneMemoryInfo_self(cs, val);
}

/* struct VkBindImageMemoryInfo chain */

static inline size_t
vn_sizeof_VkBindImageMemoryInfo_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkBindImageMemoryInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkBindImageMemoryDeviceGroupInfo_self((const VkBindImageMemoryDeviceGroupInfo *)pnext);
            return size;
        case VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkBindImageMemoryInfo_pnext(pnext->pNext);
            size += vn_sizeof_VkBindImagePlaneMemoryInfo_self((const VkBindImagePlaneMemoryInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBindImageMemoryInfo_self(const VkBindImageMemoryInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImage(&val->image);
    size += vn_sizeof_VkDeviceMemory(&val->memory);
    size += vn_sizeof_VkDeviceSize(&val->memoryOffset);
    return size;
}

static inline size_t
vn_sizeof_VkBindImageMemoryInfo(const VkBindImageMemoryInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBindImageMemoryInfo_pnext(val->pNext);
    size += vn_sizeof_VkBindImageMemoryInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBindImageMemoryInfo_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkBindImageMemoryInfo_pnext(cs, pnext->pNext);
            vn_encode_VkBindImageMemoryDeviceGroupInfo_self(cs, (const VkBindImageMemoryDeviceGroupInfo *)pnext);
            return;
        case VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkBindImageMemoryInfo_pnext(cs, pnext->pNext);
            vn_encode_VkBindImagePlaneMemoryInfo_self(cs, (const VkBindImagePlaneMemoryInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBindImageMemoryInfo_self(struct vn_cs *cs, const VkBindImageMemoryInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImage(cs, &val->image);
    vn_encode_VkDeviceMemory(cs, &val->memory);
    vn_encode_VkDeviceSize(cs, &val->memoryOffset);
}

static inline void
vn_encode_VkBindImageMemoryInfo(struct vn_cs *cs, const VkBindImageMemoryInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO });
    vn_encode_VkBindImageMemoryInfo_pnext(cs, val->pNext);
    vn_encode_VkBindImageMemoryInfo_self(cs, val);
}

static inline void
vn_decode_VkBindImageMemoryInfo_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO:
        vn_decode_VkBindImageMemoryInfo_pnext(cs, pnext->pNext);
        vn_decode_VkBindImageMemoryDeviceGroupInfo_self(cs, (VkBindImageMemoryDeviceGroupInfo *)pnext);
        break;
    case VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO:
        vn_decode_VkBindImageMemoryInfo_pnext(cs, pnext->pNext);
        vn_decode_VkBindImagePlaneMemoryInfo_self(cs, (VkBindImagePlaneMemoryInfo *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkBindImageMemoryInfo_self(struct vn_cs *cs, VkBindImageMemoryInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkImage(cs, &val->image);
    vn_decode_VkDeviceMemory(cs, &val->memory);
    vn_decode_VkDeviceSize(cs, &val->memoryOffset);
}

static inline void
vn_decode_VkBindImageMemoryInfo(struct vn_cs *cs, VkBindImageMemoryInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO);

    assert(val->sType == stype);
    vn_decode_VkBindImageMemoryInfo_pnext(cs, val->pNext);
    vn_decode_VkBindImageMemoryInfo_self(cs, val);
}

/* struct VkDescriptorUpdateTemplateEntry */

static inline size_t
vn_sizeof_VkDescriptorUpdateTemplateEntry(const VkDescriptorUpdateTemplateEntry *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->dstBinding);
    size += vn_sizeof_uint32_t(&val->dstArrayElement);
    size += vn_sizeof_uint32_t(&val->descriptorCount);
    size += vn_sizeof_VkDescriptorType(&val->descriptorType);
    size += vn_sizeof_size_t(&val->offset);
    size += vn_sizeof_size_t(&val->stride);
    return size;
}

static inline void
vn_encode_VkDescriptorUpdateTemplateEntry(struct vn_cs *cs, const VkDescriptorUpdateTemplateEntry *val)
{
    vn_encode_uint32_t(cs, &val->dstBinding);
    vn_encode_uint32_t(cs, &val->dstArrayElement);
    vn_encode_uint32_t(cs, &val->descriptorCount);
    vn_encode_VkDescriptorType(cs, &val->descriptorType);
    vn_encode_size_t(cs, &val->offset);
    vn_encode_size_t(cs, &val->stride);
}

/* struct VkDescriptorUpdateTemplateCreateInfo chain */

static inline size_t
vn_sizeof_VkDescriptorUpdateTemplateCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorUpdateTemplateCreateInfo_self(const VkDescriptorUpdateTemplateCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->descriptorUpdateEntryCount);
    if (val->pDescriptorUpdateEntries) {
        size += vn_sizeof_array_size(val->descriptorUpdateEntryCount);
        for (uint32_t i = 0; i < val->descriptorUpdateEntryCount; i++)
            size += vn_sizeof_VkDescriptorUpdateTemplateEntry(&val->pDescriptorUpdateEntries[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_VkDescriptorUpdateTemplateType(&val->templateType);
    size += vn_sizeof_VkDescriptorSetLayout(&val->descriptorSetLayout);
    size += vn_sizeof_VkPipelineBindPoint(&val->pipelineBindPoint);
    size += vn_sizeof_VkPipelineLayout(&val->pipelineLayout);
    size += vn_sizeof_uint32_t(&val->set);
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorUpdateTemplateCreateInfo(const VkDescriptorUpdateTemplateCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorUpdateTemplateCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorUpdateTemplateCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDescriptorUpdateTemplateCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorUpdateTemplateCreateInfo_self(struct vn_cs *cs, const VkDescriptorUpdateTemplateCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->descriptorUpdateEntryCount);
    if (val->pDescriptorUpdateEntries) {
        vn_encode_array_size(cs, val->descriptorUpdateEntryCount);
        for (uint32_t i = 0; i < val->descriptorUpdateEntryCount; i++)
            vn_encode_VkDescriptorUpdateTemplateEntry(cs, &val->pDescriptorUpdateEntries[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_VkDescriptorUpdateTemplateType(cs, &val->templateType);
    vn_encode_VkDescriptorSetLayout(cs, &val->descriptorSetLayout);
    vn_encode_VkPipelineBindPoint(cs, &val->pipelineBindPoint);
    vn_encode_VkPipelineLayout(cs, &val->pipelineLayout);
    vn_encode_uint32_t(cs, &val->set);
}

static inline void
vn_encode_VkDescriptorUpdateTemplateCreateInfo(struct vn_cs *cs, const VkDescriptorUpdateTemplateCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO });
    vn_encode_VkDescriptorUpdateTemplateCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkDescriptorUpdateTemplateCreateInfo_self(cs, val);
}

/* struct VkBufferMemoryRequirementsInfo2 chain */

static inline size_t
vn_sizeof_VkBufferMemoryRequirementsInfo2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBufferMemoryRequirementsInfo2_self(const VkBufferMemoryRequirementsInfo2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBuffer(&val->buffer);
    return size;
}

static inline size_t
vn_sizeof_VkBufferMemoryRequirementsInfo2(const VkBufferMemoryRequirementsInfo2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBufferMemoryRequirementsInfo2_pnext(val->pNext);
    size += vn_sizeof_VkBufferMemoryRequirementsInfo2_self(val);

    return size;
}

static inline void
vn_encode_VkBufferMemoryRequirementsInfo2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBufferMemoryRequirementsInfo2_self(struct vn_cs *cs, const VkBufferMemoryRequirementsInfo2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBuffer(cs, &val->buffer);
}

static inline void
vn_encode_VkBufferMemoryRequirementsInfo2(struct vn_cs *cs, const VkBufferMemoryRequirementsInfo2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 });
    vn_encode_VkBufferMemoryRequirementsInfo2_pnext(cs, val->pNext);
    vn_encode_VkBufferMemoryRequirementsInfo2_self(cs, val);
}

/* struct VkImagePlaneMemoryRequirementsInfo chain */

static inline size_t
vn_sizeof_VkImagePlaneMemoryRequirementsInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImagePlaneMemoryRequirementsInfo_self(const VkImagePlaneMemoryRequirementsInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImageAspectFlagBits(&val->planeAspect);
    return size;
}

static inline size_t
vn_sizeof_VkImagePlaneMemoryRequirementsInfo(const VkImagePlaneMemoryRequirementsInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImagePlaneMemoryRequirementsInfo_pnext(val->pNext);
    size += vn_sizeof_VkImagePlaneMemoryRequirementsInfo_self(val);

    return size;
}

static inline void
vn_encode_VkImagePlaneMemoryRequirementsInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImagePlaneMemoryRequirementsInfo_self(struct vn_cs *cs, const VkImagePlaneMemoryRequirementsInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImageAspectFlagBits(cs, &val->planeAspect);
}

static inline void
vn_encode_VkImagePlaneMemoryRequirementsInfo(struct vn_cs *cs, const VkImagePlaneMemoryRequirementsInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO });
    vn_encode_VkImagePlaneMemoryRequirementsInfo_pnext(cs, val->pNext);
    vn_encode_VkImagePlaneMemoryRequirementsInfo_self(cs, val);
}

/* struct VkImageMemoryRequirementsInfo2 chain */

static inline size_t
vn_sizeof_VkImageMemoryRequirementsInfo2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkImageMemoryRequirementsInfo2_pnext(pnext->pNext);
            size += vn_sizeof_VkImagePlaneMemoryRequirementsInfo_self((const VkImagePlaneMemoryRequirementsInfo *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageMemoryRequirementsInfo2_self(const VkImageMemoryRequirementsInfo2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImage(&val->image);
    return size;
}

static inline size_t
vn_sizeof_VkImageMemoryRequirementsInfo2(const VkImageMemoryRequirementsInfo2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageMemoryRequirementsInfo2_pnext(val->pNext);
    size += vn_sizeof_VkImageMemoryRequirementsInfo2_self(val);

    return size;
}

static inline void
vn_encode_VkImageMemoryRequirementsInfo2_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkImageMemoryRequirementsInfo2_pnext(cs, pnext->pNext);
            vn_encode_VkImagePlaneMemoryRequirementsInfo_self(cs, (const VkImagePlaneMemoryRequirementsInfo *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageMemoryRequirementsInfo2_self(struct vn_cs *cs, const VkImageMemoryRequirementsInfo2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImage(cs, &val->image);
}

static inline void
vn_encode_VkImageMemoryRequirementsInfo2(struct vn_cs *cs, const VkImageMemoryRequirementsInfo2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 });
    vn_encode_VkImageMemoryRequirementsInfo2_pnext(cs, val->pNext);
    vn_encode_VkImageMemoryRequirementsInfo2_self(cs, val);
}

/* struct VkImageSparseMemoryRequirementsInfo2 chain */

static inline size_t
vn_sizeof_VkImageSparseMemoryRequirementsInfo2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageSparseMemoryRequirementsInfo2_self(const VkImageSparseMemoryRequirementsInfo2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImage(&val->image);
    return size;
}

static inline size_t
vn_sizeof_VkImageSparseMemoryRequirementsInfo2(const VkImageSparseMemoryRequirementsInfo2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageSparseMemoryRequirementsInfo2_pnext(val->pNext);
    size += vn_sizeof_VkImageSparseMemoryRequirementsInfo2_self(val);

    return size;
}

static inline void
vn_encode_VkImageSparseMemoryRequirementsInfo2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageSparseMemoryRequirementsInfo2_self(struct vn_cs *cs, const VkImageSparseMemoryRequirementsInfo2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImage(cs, &val->image);
}

static inline void
vn_encode_VkImageSparseMemoryRequirementsInfo2(struct vn_cs *cs, const VkImageSparseMemoryRequirementsInfo2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2 });
    vn_encode_VkImageSparseMemoryRequirementsInfo2_pnext(cs, val->pNext);
    vn_encode_VkImageSparseMemoryRequirementsInfo2_self(cs, val);
}

/* struct VkMemoryDedicatedRequirements chain */

static inline size_t
vn_sizeof_VkMemoryDedicatedRequirements_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryDedicatedRequirements_self(const VkMemoryDedicatedRequirements *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->prefersDedicatedAllocation);
    size += vn_sizeof_VkBool32(&val->requiresDedicatedAllocation);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryDedicatedRequirements(const VkMemoryDedicatedRequirements *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryDedicatedRequirements_pnext(val->pNext);
    size += vn_sizeof_VkMemoryDedicatedRequirements_self(val);

    return size;
}

static inline void
vn_decode_VkMemoryDedicatedRequirements_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkMemoryDedicatedRequirements_self(struct vn_cs *cs, VkMemoryDedicatedRequirements *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->prefersDedicatedAllocation);
    vn_decode_VkBool32(cs, &val->requiresDedicatedAllocation);
}

static inline void
vn_decode_VkMemoryDedicatedRequirements(struct vn_cs *cs, VkMemoryDedicatedRequirements *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS);

    assert(val->sType == stype);
    vn_decode_VkMemoryDedicatedRequirements_pnext(cs, val->pNext);
    vn_decode_VkMemoryDedicatedRequirements_self(cs, val);
}

static inline size_t
vn_sizeof_VkMemoryDedicatedRequirements_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryDedicatedRequirements_self_partial(const VkMemoryDedicatedRequirements *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->prefersDedicatedAllocation */
    /* skip val->requiresDedicatedAllocation */
    return size;
}

static inline size_t
vn_sizeof_VkMemoryDedicatedRequirements_partial(const VkMemoryDedicatedRequirements *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryDedicatedRequirements_pnext_partial(val->pNext);
    size += vn_sizeof_VkMemoryDedicatedRequirements_self_partial(val);

    return size;
}

static inline void
vn_encode_VkMemoryDedicatedRequirements_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMemoryDedicatedRequirements_self_partial(struct vn_cs *cs, const VkMemoryDedicatedRequirements *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->prefersDedicatedAllocation */
    /* skip val->requiresDedicatedAllocation */
}

static inline void
vn_encode_VkMemoryDedicatedRequirements_partial(struct vn_cs *cs, const VkMemoryDedicatedRequirements *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS });
    vn_encode_VkMemoryDedicatedRequirements_pnext_partial(cs, val->pNext);
    vn_encode_VkMemoryDedicatedRequirements_self_partial(cs, val);
}

/* struct VkMemoryRequirements2 chain */

static inline size_t
vn_sizeof_VkMemoryRequirements2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkMemoryRequirements2_pnext(pnext->pNext);
            size += vn_sizeof_VkMemoryDedicatedRequirements_self((const VkMemoryDedicatedRequirements *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryRequirements2_self(const VkMemoryRequirements2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkMemoryRequirements(&val->memoryRequirements);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryRequirements2(const VkMemoryRequirements2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryRequirements2_pnext(val->pNext);
    size += vn_sizeof_VkMemoryRequirements2_self(val);

    return size;
}

static inline void
vn_decode_VkMemoryRequirements2_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS:
        vn_decode_VkMemoryRequirements2_pnext(cs, pnext->pNext);
        vn_decode_VkMemoryDedicatedRequirements_self(cs, (VkMemoryDedicatedRequirements *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkMemoryRequirements2_self(struct vn_cs *cs, VkMemoryRequirements2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkMemoryRequirements(cs, &val->memoryRequirements);
}

static inline void
vn_decode_VkMemoryRequirements2(struct vn_cs *cs, VkMemoryRequirements2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);

    assert(val->sType == stype);
    vn_decode_VkMemoryRequirements2_pnext(cs, val->pNext);
    vn_decode_VkMemoryRequirements2_self(cs, val);
}

static inline size_t
vn_sizeof_VkMemoryRequirements2_pnext_partial(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkMemoryRequirements2_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkMemoryDedicatedRequirements_self_partial((const VkMemoryDedicatedRequirements *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkMemoryRequirements2_self_partial(const VkMemoryRequirements2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkMemoryRequirements_partial(&val->memoryRequirements);
    return size;
}

static inline size_t
vn_sizeof_VkMemoryRequirements2_partial(const VkMemoryRequirements2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkMemoryRequirements2_pnext_partial(val->pNext);
    size += vn_sizeof_VkMemoryRequirements2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkMemoryRequirements2_pnext_partial(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkMemoryRequirements2_pnext_partial(cs, pnext->pNext);
            vn_encode_VkMemoryDedicatedRequirements_self_partial(cs, (const VkMemoryDedicatedRequirements *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkMemoryRequirements2_self_partial(struct vn_cs *cs, const VkMemoryRequirements2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkMemoryRequirements_partial(cs, &val->memoryRequirements);
}

static inline void
vn_encode_VkMemoryRequirements2_partial(struct vn_cs *cs, const VkMemoryRequirements2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 });
    vn_encode_VkMemoryRequirements2_pnext_partial(cs, val->pNext);
    vn_encode_VkMemoryRequirements2_self_partial(cs, val);
}

/* struct VkSparseImageMemoryRequirements2 chain */

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements2_self(const VkSparseImageMemoryRequirements2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSparseImageMemoryRequirements(&val->memoryRequirements);
    return size;
}

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements2(const VkSparseImageMemoryRequirements2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSparseImageMemoryRequirements2_pnext(val->pNext);
    size += vn_sizeof_VkSparseImageMemoryRequirements2_self(val);

    return size;
}

static inline void
vn_decode_VkSparseImageMemoryRequirements2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkSparseImageMemoryRequirements2_self(struct vn_cs *cs, VkSparseImageMemoryRequirements2 *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkSparseImageMemoryRequirements(cs, &val->memoryRequirements);
}

static inline void
vn_decode_VkSparseImageMemoryRequirements2(struct vn_cs *cs, VkSparseImageMemoryRequirements2 *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2);

    assert(val->sType == stype);
    vn_decode_VkSparseImageMemoryRequirements2_pnext(cs, val->pNext);
    vn_decode_VkSparseImageMemoryRequirements2_self(cs, val);
}

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements2_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements2_self_partial(const VkSparseImageMemoryRequirements2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSparseImageMemoryRequirements_partial(&val->memoryRequirements);
    return size;
}

static inline size_t
vn_sizeof_VkSparseImageMemoryRequirements2_partial(const VkSparseImageMemoryRequirements2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSparseImageMemoryRequirements2_pnext_partial(val->pNext);
    size += vn_sizeof_VkSparseImageMemoryRequirements2_self_partial(val);

    return size;
}

static inline void
vn_encode_VkSparseImageMemoryRequirements2_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSparseImageMemoryRequirements2_self_partial(struct vn_cs *cs, const VkSparseImageMemoryRequirements2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkSparseImageMemoryRequirements_partial(cs, &val->memoryRequirements);
}

static inline void
vn_encode_VkSparseImageMemoryRequirements2_partial(struct vn_cs *cs, const VkSparseImageMemoryRequirements2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2 });
    vn_encode_VkSparseImageMemoryRequirements2_pnext_partial(cs, val->pNext);
    vn_encode_VkSparseImageMemoryRequirements2_self_partial(cs, val);
}

/* struct VkSamplerYcbcrConversionCreateInfo chain */

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionCreateInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionCreateInfo_self(const VkSamplerYcbcrConversionCreateInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkSamplerYcbcrModelConversion(&val->ycbcrModel);
    size += vn_sizeof_VkSamplerYcbcrRange(&val->ycbcrRange);
    size += vn_sizeof_VkComponentMapping(&val->components);
    size += vn_sizeof_VkChromaLocation(&val->xChromaOffset);
    size += vn_sizeof_VkChromaLocation(&val->yChromaOffset);
    size += vn_sizeof_VkFilter(&val->chromaFilter);
    size += vn_sizeof_VkBool32(&val->forceExplicitReconstruction);
    return size;
}

static inline size_t
vn_sizeof_VkSamplerYcbcrConversionCreateInfo(const VkSamplerYcbcrConversionCreateInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSamplerYcbcrConversionCreateInfo_pnext(val->pNext);
    size += vn_sizeof_VkSamplerYcbcrConversionCreateInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSamplerYcbcrConversionCreateInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSamplerYcbcrConversionCreateInfo_self(struct vn_cs *cs, const VkSamplerYcbcrConversionCreateInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkSamplerYcbcrModelConversion(cs, &val->ycbcrModel);
    vn_encode_VkSamplerYcbcrRange(cs, &val->ycbcrRange);
    vn_encode_VkComponentMapping(cs, &val->components);
    vn_encode_VkChromaLocation(cs, &val->xChromaOffset);
    vn_encode_VkChromaLocation(cs, &val->yChromaOffset);
    vn_encode_VkFilter(cs, &val->chromaFilter);
    vn_encode_VkBool32(cs, &val->forceExplicitReconstruction);
}

static inline void
vn_encode_VkSamplerYcbcrConversionCreateInfo(struct vn_cs *cs, const VkSamplerYcbcrConversionCreateInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO });
    vn_encode_VkSamplerYcbcrConversionCreateInfo_pnext(cs, val->pNext);
    vn_encode_VkSamplerYcbcrConversionCreateInfo_self(cs, val);
}

/* struct VkDeviceQueueInfo2 chain */

static inline size_t
vn_sizeof_VkDeviceQueueInfo2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceQueueInfo2_self(const VkDeviceQueueInfo2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->queueFamilyIndex);
    size += vn_sizeof_uint32_t(&val->queueIndex);
    return size;
}

static inline size_t
vn_sizeof_VkDeviceQueueInfo2(const VkDeviceQueueInfo2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceQueueInfo2_pnext(val->pNext);
    size += vn_sizeof_VkDeviceQueueInfo2_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceQueueInfo2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceQueueInfo2_self(struct vn_cs *cs, const VkDeviceQueueInfo2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->queueFamilyIndex);
    vn_encode_uint32_t(cs, &val->queueIndex);
}

static inline void
vn_encode_VkDeviceQueueInfo2(struct vn_cs *cs, const VkDeviceQueueInfo2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 });
    vn_encode_VkDeviceQueueInfo2_pnext(cs, val->pNext);
    vn_encode_VkDeviceQueueInfo2_self(cs, val);
}

/* struct VkDescriptorSetVariableDescriptorCountLayoutSupport chain */

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_self(const VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->maxVariableDescriptorCount);
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport(const VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_self(val);

    return size;
}

static inline void
vn_decode_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkDescriptorSetVariableDescriptorCountLayoutSupport_self(struct vn_cs *cs, VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint32_t(cs, &val->maxVariableDescriptorCount);
}

static inline void
vn_decode_VkDescriptorSetVariableDescriptorCountLayoutSupport(struct vn_cs *cs, VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT);

    assert(val->sType == stype);
    vn_decode_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext(cs, val->pNext);
    vn_decode_VkDescriptorSetVariableDescriptorCountLayoutSupport_self(cs, val);
}

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_self_partial(const VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->maxVariableDescriptorCount */
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_partial(const VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext_partial(val->pNext);
    size += vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_self_partial(val);

    return size;
}

static inline void
vn_encode_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorSetVariableDescriptorCountLayoutSupport_self_partial(struct vn_cs *cs, const VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->maxVariableDescriptorCount */
}

static inline void
vn_encode_VkDescriptorSetVariableDescriptorCountLayoutSupport_partial(struct vn_cs *cs, const VkDescriptorSetVariableDescriptorCountLayoutSupport *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT });
    vn_encode_VkDescriptorSetVariableDescriptorCountLayoutSupport_pnext_partial(cs, val->pNext);
    vn_encode_VkDescriptorSetVariableDescriptorCountLayoutSupport_self_partial(cs, val);
}

/* struct VkDescriptorSetLayoutSupport chain */

static inline size_t
vn_sizeof_VkDescriptorSetLayoutSupport_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDescriptorSetLayoutSupport_pnext(pnext->pNext);
            size += vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_self((const VkDescriptorSetVariableDescriptorCountLayoutSupport *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutSupport_self(const VkDescriptorSetLayoutSupport *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBool32(&val->supported);
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutSupport(const VkDescriptorSetLayoutSupport *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetLayoutSupport_pnext(val->pNext);
    size += vn_sizeof_VkDescriptorSetLayoutSupport_self(val);

    return size;
}

static inline void
vn_decode_VkDescriptorSetLayoutSupport_pnext(struct vn_cs *cs, const void *val)
{
    VkBaseOutStructure *pnext = (VkBaseOutStructure *)val;
    VkStructureType stype;

    if (!vn_decode_simple_pointer(cs))
        return;

    vn_decode_VkStructureType(cs, &stype);
    while (true) {
        assert(pnext);
        if (pnext->sType == stype)
            break;
    }

    switch ((int32_t)pnext->sType) {
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT:
        vn_decode_VkDescriptorSetLayoutSupport_pnext(cs, pnext->pNext);
        vn_decode_VkDescriptorSetVariableDescriptorCountLayoutSupport_self(cs, (VkDescriptorSetVariableDescriptorCountLayoutSupport *)pnext);
        break;
    default:
        assert(false);
        break;
    }
}

static inline void
vn_decode_VkDescriptorSetLayoutSupport_self(struct vn_cs *cs, VkDescriptorSetLayoutSupport *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBool32(cs, &val->supported);
}

static inline void
vn_decode_VkDescriptorSetLayoutSupport(struct vn_cs *cs, VkDescriptorSetLayoutSupport *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT);

    assert(val->sType == stype);
    vn_decode_VkDescriptorSetLayoutSupport_pnext(cs, val->pNext);
    vn_decode_VkDescriptorSetLayoutSupport_self(cs, val);
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutSupport_pnext_partial(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkDescriptorSetLayoutSupport_pnext_partial(pnext->pNext);
            size += vn_sizeof_VkDescriptorSetVariableDescriptorCountLayoutSupport_self_partial((const VkDescriptorSetVariableDescriptorCountLayoutSupport *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutSupport_self_partial(const VkDescriptorSetLayoutSupport *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->supported */
    return size;
}

static inline size_t
vn_sizeof_VkDescriptorSetLayoutSupport_partial(const VkDescriptorSetLayoutSupport *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDescriptorSetLayoutSupport_pnext_partial(val->pNext);
    size += vn_sizeof_VkDescriptorSetLayoutSupport_self_partial(val);

    return size;
}

static inline void
vn_encode_VkDescriptorSetLayoutSupport_pnext_partial(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkDescriptorSetLayoutSupport_pnext_partial(cs, pnext->pNext);
            vn_encode_VkDescriptorSetVariableDescriptorCountLayoutSupport_self_partial(cs, (const VkDescriptorSetVariableDescriptorCountLayoutSupport *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDescriptorSetLayoutSupport_self_partial(struct vn_cs *cs, const VkDescriptorSetLayoutSupport *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->supported */
}

static inline void
vn_encode_VkDescriptorSetLayoutSupport_partial(struct vn_cs *cs, const VkDescriptorSetLayoutSupport *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT });
    vn_encode_VkDescriptorSetLayoutSupport_pnext_partial(cs, val->pNext);
    vn_encode_VkDescriptorSetLayoutSupport_self_partial(cs, val);
}

/* struct VkAttachmentDescriptionStencilLayout chain */

static inline size_t
vn_sizeof_VkAttachmentDescriptionStencilLayout_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkAttachmentDescriptionStencilLayout_self(const VkAttachmentDescriptionStencilLayout *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImageLayout(&val->stencilInitialLayout);
    size += vn_sizeof_VkImageLayout(&val->stencilFinalLayout);
    return size;
}

static inline size_t
vn_sizeof_VkAttachmentDescriptionStencilLayout(const VkAttachmentDescriptionStencilLayout *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkAttachmentDescriptionStencilLayout_pnext(val->pNext);
    size += vn_sizeof_VkAttachmentDescriptionStencilLayout_self(val);

    return size;
}

static inline void
vn_encode_VkAttachmentDescriptionStencilLayout_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkAttachmentDescriptionStencilLayout_self(struct vn_cs *cs, const VkAttachmentDescriptionStencilLayout *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImageLayout(cs, &val->stencilInitialLayout);
    vn_encode_VkImageLayout(cs, &val->stencilFinalLayout);
}

static inline void
vn_encode_VkAttachmentDescriptionStencilLayout(struct vn_cs *cs, const VkAttachmentDescriptionStencilLayout *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT });
    vn_encode_VkAttachmentDescriptionStencilLayout_pnext(cs, val->pNext);
    vn_encode_VkAttachmentDescriptionStencilLayout_self(cs, val);
}

/* struct VkAttachmentDescription2 chain */

static inline size_t
vn_sizeof_VkAttachmentDescription2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkAttachmentDescription2_pnext(pnext->pNext);
            size += vn_sizeof_VkAttachmentDescriptionStencilLayout_self((const VkAttachmentDescriptionStencilLayout *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkAttachmentDescription2_self(const VkAttachmentDescription2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkFormat(&val->format);
    size += vn_sizeof_VkSampleCountFlagBits(&val->samples);
    size += vn_sizeof_VkAttachmentLoadOp(&val->loadOp);
    size += vn_sizeof_VkAttachmentStoreOp(&val->storeOp);
    size += vn_sizeof_VkAttachmentLoadOp(&val->stencilLoadOp);
    size += vn_sizeof_VkAttachmentStoreOp(&val->stencilStoreOp);
    size += vn_sizeof_VkImageLayout(&val->initialLayout);
    size += vn_sizeof_VkImageLayout(&val->finalLayout);
    return size;
}

static inline size_t
vn_sizeof_VkAttachmentDescription2(const VkAttachmentDescription2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkAttachmentDescription2_pnext(val->pNext);
    size += vn_sizeof_VkAttachmentDescription2_self(val);

    return size;
}

static inline void
vn_encode_VkAttachmentDescription2_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkAttachmentDescription2_pnext(cs, pnext->pNext);
            vn_encode_VkAttachmentDescriptionStencilLayout_self(cs, (const VkAttachmentDescriptionStencilLayout *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkAttachmentDescription2_self(struct vn_cs *cs, const VkAttachmentDescription2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkFormat(cs, &val->format);
    vn_encode_VkSampleCountFlagBits(cs, &val->samples);
    vn_encode_VkAttachmentLoadOp(cs, &val->loadOp);
    vn_encode_VkAttachmentStoreOp(cs, &val->storeOp);
    vn_encode_VkAttachmentLoadOp(cs, &val->stencilLoadOp);
    vn_encode_VkAttachmentStoreOp(cs, &val->stencilStoreOp);
    vn_encode_VkImageLayout(cs, &val->initialLayout);
    vn_encode_VkImageLayout(cs, &val->finalLayout);
}

static inline void
vn_encode_VkAttachmentDescription2(struct vn_cs *cs, const VkAttachmentDescription2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2 });
    vn_encode_VkAttachmentDescription2_pnext(cs, val->pNext);
    vn_encode_VkAttachmentDescription2_self(cs, val);
}

/* struct VkAttachmentReferenceStencilLayout chain */

static inline size_t
vn_sizeof_VkAttachmentReferenceStencilLayout_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkAttachmentReferenceStencilLayout_self(const VkAttachmentReferenceStencilLayout *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkImageLayout(&val->stencilLayout);
    return size;
}

static inline size_t
vn_sizeof_VkAttachmentReferenceStencilLayout(const VkAttachmentReferenceStencilLayout *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkAttachmentReferenceStencilLayout_pnext(val->pNext);
    size += vn_sizeof_VkAttachmentReferenceStencilLayout_self(val);

    return size;
}

static inline void
vn_encode_VkAttachmentReferenceStencilLayout_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkAttachmentReferenceStencilLayout_self(struct vn_cs *cs, const VkAttachmentReferenceStencilLayout *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkImageLayout(cs, &val->stencilLayout);
}

static inline void
vn_encode_VkAttachmentReferenceStencilLayout(struct vn_cs *cs, const VkAttachmentReferenceStencilLayout *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT });
    vn_encode_VkAttachmentReferenceStencilLayout_pnext(cs, val->pNext);
    vn_encode_VkAttachmentReferenceStencilLayout_self(cs, val);
}

/* struct VkAttachmentReference2 chain */

static inline size_t
vn_sizeof_VkAttachmentReference2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkAttachmentReference2_pnext(pnext->pNext);
            size += vn_sizeof_VkAttachmentReferenceStencilLayout_self((const VkAttachmentReferenceStencilLayout *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkAttachmentReference2_self(const VkAttachmentReference2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->attachment);
    size += vn_sizeof_VkImageLayout(&val->layout);
    size += vn_sizeof_VkFlags(&val->aspectMask);
    return size;
}

static inline size_t
vn_sizeof_VkAttachmentReference2(const VkAttachmentReference2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkAttachmentReference2_pnext(val->pNext);
    size += vn_sizeof_VkAttachmentReference2_self(val);

    return size;
}

static inline void
vn_encode_VkAttachmentReference2_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkAttachmentReference2_pnext(cs, pnext->pNext);
            vn_encode_VkAttachmentReferenceStencilLayout_self(cs, (const VkAttachmentReferenceStencilLayout *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkAttachmentReference2_self(struct vn_cs *cs, const VkAttachmentReference2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->attachment);
    vn_encode_VkImageLayout(cs, &val->layout);
    vn_encode_VkFlags(cs, &val->aspectMask);
}

static inline void
vn_encode_VkAttachmentReference2(struct vn_cs *cs, const VkAttachmentReference2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2 });
    vn_encode_VkAttachmentReference2_pnext(cs, val->pNext);
    vn_encode_VkAttachmentReference2_self(cs, val);
}

/* struct VkSubpassDescriptionDepthStencilResolve chain */

static inline size_t
vn_sizeof_VkSubpassDescriptionDepthStencilResolve_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSubpassDescriptionDepthStencilResolve_self(const VkSubpassDescriptionDepthStencilResolve *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkResolveModeFlagBits(&val->depthResolveMode);
    size += vn_sizeof_VkResolveModeFlagBits(&val->stencilResolveMode);
    size += vn_sizeof_simple_pointer(val->pDepthStencilResolveAttachment);
    if (val->pDepthStencilResolveAttachment)
        size += vn_sizeof_VkAttachmentReference2(val->pDepthStencilResolveAttachment);
    return size;
}

static inline size_t
vn_sizeof_VkSubpassDescriptionDepthStencilResolve(const VkSubpassDescriptionDepthStencilResolve *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSubpassDescriptionDepthStencilResolve_pnext(val->pNext);
    size += vn_sizeof_VkSubpassDescriptionDepthStencilResolve_self(val);

    return size;
}

static inline void
vn_encode_VkSubpassDescriptionDepthStencilResolve_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSubpassDescriptionDepthStencilResolve_self(struct vn_cs *cs, const VkSubpassDescriptionDepthStencilResolve *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkResolveModeFlagBits(cs, &val->depthResolveMode);
    vn_encode_VkResolveModeFlagBits(cs, &val->stencilResolveMode);
    if (vn_encode_simple_pointer(cs, val->pDepthStencilResolveAttachment))
        vn_encode_VkAttachmentReference2(cs, val->pDepthStencilResolveAttachment);
}

static inline void
vn_encode_VkSubpassDescriptionDepthStencilResolve(struct vn_cs *cs, const VkSubpassDescriptionDepthStencilResolve *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE });
    vn_encode_VkSubpassDescriptionDepthStencilResolve_pnext(cs, val->pNext);
    vn_encode_VkSubpassDescriptionDepthStencilResolve_self(cs, val);
}

/* struct VkSubpassDescription2 chain */

static inline size_t
vn_sizeof_VkSubpassDescription2_pnext(const void *val)
{
    const VkBaseInStructure *pnext = val;
    size_t size = 0;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE:
            size += vn_sizeof_simple_pointer(pnext);
            size += vn_sizeof_VkStructureType(&pnext->sType);
            size += vn_sizeof_VkSubpassDescription2_pnext(pnext->pNext);
            size += vn_sizeof_VkSubpassDescriptionDepthStencilResolve_self((const VkSubpassDescriptionDepthStencilResolve *)pnext);
            return size;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSubpassDescription2_self(const VkSubpassDescription2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_VkPipelineBindPoint(&val->pipelineBindPoint);
    size += vn_sizeof_uint32_t(&val->viewMask);
    size += vn_sizeof_uint32_t(&val->inputAttachmentCount);
    if (val->pInputAttachments) {
        size += vn_sizeof_array_size(val->inputAttachmentCount);
        for (uint32_t i = 0; i < val->inputAttachmentCount; i++)
            size += vn_sizeof_VkAttachmentReference2(&val->pInputAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->colorAttachmentCount);
    if (val->pColorAttachments) {
        size += vn_sizeof_array_size(val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            size += vn_sizeof_VkAttachmentReference2(&val->pColorAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    if (val->pResolveAttachments) {
        size += vn_sizeof_array_size(val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            size += vn_sizeof_VkAttachmentReference2(&val->pResolveAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_simple_pointer(val->pDepthStencilAttachment);
    if (val->pDepthStencilAttachment)
        size += vn_sizeof_VkAttachmentReference2(val->pDepthStencilAttachment);
    size += vn_sizeof_uint32_t(&val->preserveAttachmentCount);
    if (val->pPreserveAttachments) {
        size += vn_sizeof_array_size(val->preserveAttachmentCount);
        size += vn_sizeof_uint32_t_array(val->pPreserveAttachments, val->preserveAttachmentCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkSubpassDescription2(const VkSubpassDescription2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSubpassDescription2_pnext(val->pNext);
    size += vn_sizeof_VkSubpassDescription2_self(val);

    return size;
}

static inline void
vn_encode_VkSubpassDescription2_pnext(struct vn_cs *cs, const void *val)
{
    const VkBaseInStructure *pnext = val;

    while (pnext) {
        switch ((int32_t)pnext->sType) {
        case VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE:
            vn_encode_simple_pointer(cs, pnext);
            vn_encode_VkStructureType(cs, &pnext->sType);
            vn_encode_VkSubpassDescription2_pnext(cs, pnext->pNext);
            vn_encode_VkSubpassDescriptionDepthStencilResolve_self(cs, (const VkSubpassDescriptionDepthStencilResolve *)pnext);
            return;
        default:
            /* ignore unknown/unsupported struct */
            break;
        }
        pnext = pnext->pNext;
    }

    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSubpassDescription2_self(struct vn_cs *cs, const VkSubpassDescription2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_VkPipelineBindPoint(cs, &val->pipelineBindPoint);
    vn_encode_uint32_t(cs, &val->viewMask);
    vn_encode_uint32_t(cs, &val->inputAttachmentCount);
    if (val->pInputAttachments) {
        vn_encode_array_size(cs, val->inputAttachmentCount);
        for (uint32_t i = 0; i < val->inputAttachmentCount; i++)
            vn_encode_VkAttachmentReference2(cs, &val->pInputAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->colorAttachmentCount);
    if (val->pColorAttachments) {
        vn_encode_array_size(cs, val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            vn_encode_VkAttachmentReference2(cs, &val->pColorAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (val->pResolveAttachments) {
        vn_encode_array_size(cs, val->colorAttachmentCount);
        for (uint32_t i = 0; i < val->colorAttachmentCount; i++)
            vn_encode_VkAttachmentReference2(cs, &val->pResolveAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (vn_encode_simple_pointer(cs, val->pDepthStencilAttachment))
        vn_encode_VkAttachmentReference2(cs, val->pDepthStencilAttachment);
    vn_encode_uint32_t(cs, &val->preserveAttachmentCount);
    if (val->pPreserveAttachments) {
        vn_encode_array_size(cs, val->preserveAttachmentCount);
        vn_encode_uint32_t_array(cs, val->pPreserveAttachments, val->preserveAttachmentCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkSubpassDescription2(struct vn_cs *cs, const VkSubpassDescription2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2 });
    vn_encode_VkSubpassDescription2_pnext(cs, val->pNext);
    vn_encode_VkSubpassDescription2_self(cs, val);
}

/* struct VkSubpassDependency2 chain */

static inline size_t
vn_sizeof_VkSubpassDependency2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSubpassDependency2_self(const VkSubpassDependency2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint32_t(&val->srcSubpass);
    size += vn_sizeof_uint32_t(&val->dstSubpass);
    size += vn_sizeof_VkFlags(&val->srcStageMask);
    size += vn_sizeof_VkFlags(&val->dstStageMask);
    size += vn_sizeof_VkFlags(&val->srcAccessMask);
    size += vn_sizeof_VkFlags(&val->dstAccessMask);
    size += vn_sizeof_VkFlags(&val->dependencyFlags);
    size += vn_sizeof_int32_t(&val->viewOffset);
    return size;
}

static inline size_t
vn_sizeof_VkSubpassDependency2(const VkSubpassDependency2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSubpassDependency2_pnext(val->pNext);
    size += vn_sizeof_VkSubpassDependency2_self(val);

    return size;
}

static inline void
vn_encode_VkSubpassDependency2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSubpassDependency2_self(struct vn_cs *cs, const VkSubpassDependency2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_uint32_t(cs, &val->srcSubpass);
    vn_encode_uint32_t(cs, &val->dstSubpass);
    vn_encode_VkFlags(cs, &val->srcStageMask);
    vn_encode_VkFlags(cs, &val->dstStageMask);
    vn_encode_VkFlags(cs, &val->srcAccessMask);
    vn_encode_VkFlags(cs, &val->dstAccessMask);
    vn_encode_VkFlags(cs, &val->dependencyFlags);
    vn_encode_int32_t(cs, &val->viewOffset);
}

static inline void
vn_encode_VkSubpassDependency2(struct vn_cs *cs, const VkSubpassDependency2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2 });
    vn_encode_VkSubpassDependency2_pnext(cs, val->pNext);
    vn_encode_VkSubpassDependency2_self(cs, val);
}

/* struct VkRenderPassCreateInfo2 chain */

static inline size_t
vn_sizeof_VkRenderPassCreateInfo2_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkRenderPassCreateInfo2_self(const VkRenderPassCreateInfo2 *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->attachmentCount);
    if (val->pAttachments) {
        size += vn_sizeof_array_size(val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            size += vn_sizeof_VkAttachmentDescription2(&val->pAttachments[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->subpassCount);
    if (val->pSubpasses) {
        size += vn_sizeof_array_size(val->subpassCount);
        for (uint32_t i = 0; i < val->subpassCount; i++)
            size += vn_sizeof_VkSubpassDescription2(&val->pSubpasses[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->dependencyCount);
    if (val->pDependencies) {
        size += vn_sizeof_array_size(val->dependencyCount);
        for (uint32_t i = 0; i < val->dependencyCount; i++)
            size += vn_sizeof_VkSubpassDependency2(&val->pDependencies[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    size += vn_sizeof_uint32_t(&val->correlatedViewMaskCount);
    if (val->pCorrelatedViewMasks) {
        size += vn_sizeof_array_size(val->correlatedViewMaskCount);
        size += vn_sizeof_uint32_t_array(val->pCorrelatedViewMasks, val->correlatedViewMaskCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkRenderPassCreateInfo2(const VkRenderPassCreateInfo2 *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkRenderPassCreateInfo2_pnext(val->pNext);
    size += vn_sizeof_VkRenderPassCreateInfo2_self(val);

    return size;
}

static inline void
vn_encode_VkRenderPassCreateInfo2_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkRenderPassCreateInfo2_self(struct vn_cs *cs, const VkRenderPassCreateInfo2 *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->attachmentCount);
    if (val->pAttachments) {
        vn_encode_array_size(cs, val->attachmentCount);
        for (uint32_t i = 0; i < val->attachmentCount; i++)
            vn_encode_VkAttachmentDescription2(cs, &val->pAttachments[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->subpassCount);
    if (val->pSubpasses) {
        vn_encode_array_size(cs, val->subpassCount);
        for (uint32_t i = 0; i < val->subpassCount; i++)
            vn_encode_VkSubpassDescription2(cs, &val->pSubpasses[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->dependencyCount);
    if (val->pDependencies) {
        vn_encode_array_size(cs, val->dependencyCount);
        for (uint32_t i = 0; i < val->dependencyCount; i++)
            vn_encode_VkSubpassDependency2(cs, &val->pDependencies[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    vn_encode_uint32_t(cs, &val->correlatedViewMaskCount);
    if (val->pCorrelatedViewMasks) {
        vn_encode_array_size(cs, val->correlatedViewMaskCount);
        vn_encode_uint32_t_array(cs, val->pCorrelatedViewMasks, val->correlatedViewMaskCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkRenderPassCreateInfo2(struct vn_cs *cs, const VkRenderPassCreateInfo2 *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2 });
    vn_encode_VkRenderPassCreateInfo2_pnext(cs, val->pNext);
    vn_encode_VkRenderPassCreateInfo2_self(cs, val);
}

/* struct VkSubpassBeginInfo chain */

static inline size_t
vn_sizeof_VkSubpassBeginInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSubpassBeginInfo_self(const VkSubpassBeginInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSubpassContents(&val->contents);
    return size;
}

static inline size_t
vn_sizeof_VkSubpassBeginInfo(const VkSubpassBeginInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSubpassBeginInfo_pnext(val->pNext);
    size += vn_sizeof_VkSubpassBeginInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSubpassBeginInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSubpassBeginInfo_self(struct vn_cs *cs, const VkSubpassBeginInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkSubpassContents(cs, &val->contents);
}

static inline void
vn_encode_VkSubpassBeginInfo(struct vn_cs *cs, const VkSubpassBeginInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO });
    vn_encode_VkSubpassBeginInfo_pnext(cs, val->pNext);
    vn_encode_VkSubpassBeginInfo_self(cs, val);
}

/* struct VkSubpassEndInfo chain */

static inline size_t
vn_sizeof_VkSubpassEndInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSubpassEndInfo_self(const VkSubpassEndInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    return size;
}

static inline size_t
vn_sizeof_VkSubpassEndInfo(const VkSubpassEndInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSubpassEndInfo_pnext(val->pNext);
    size += vn_sizeof_VkSubpassEndInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSubpassEndInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSubpassEndInfo_self(struct vn_cs *cs, const VkSubpassEndInfo *val)
{
    /* skip val->{sType,pNext} */
}

static inline void
vn_encode_VkSubpassEndInfo(struct vn_cs *cs, const VkSubpassEndInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SUBPASS_END_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SUBPASS_END_INFO });
    vn_encode_VkSubpassEndInfo_pnext(cs, val->pNext);
    vn_encode_VkSubpassEndInfo_self(cs, val);
}

/* struct VkSemaphoreWaitInfo chain */

static inline size_t
vn_sizeof_VkSemaphoreWaitInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSemaphoreWaitInfo_self(const VkSemaphoreWaitInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkFlags(&val->flags);
    size += vn_sizeof_uint32_t(&val->semaphoreCount);
    if (val->pSemaphores) {
        size += vn_sizeof_array_size(val->semaphoreCount);
        for (uint32_t i = 0; i < val->semaphoreCount; i++)
            size += vn_sizeof_VkSemaphore(&val->pSemaphores[i]);
    } else {
        size += vn_sizeof_array_size(0);
    }
    if (val->pValues) {
        size += vn_sizeof_array_size(val->semaphoreCount);
        size += vn_sizeof_uint64_t_array(val->pValues, val->semaphoreCount);
    } else {
        size += vn_sizeof_array_size(0);
    }
    return size;
}

static inline size_t
vn_sizeof_VkSemaphoreWaitInfo(const VkSemaphoreWaitInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSemaphoreWaitInfo_pnext(val->pNext);
    size += vn_sizeof_VkSemaphoreWaitInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSemaphoreWaitInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSemaphoreWaitInfo_self(struct vn_cs *cs, const VkSemaphoreWaitInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkFlags(cs, &val->flags);
    vn_encode_uint32_t(cs, &val->semaphoreCount);
    if (val->pSemaphores) {
        vn_encode_array_size(cs, val->semaphoreCount);
        for (uint32_t i = 0; i < val->semaphoreCount; i++)
            vn_encode_VkSemaphore(cs, &val->pSemaphores[i]);
    } else {
        vn_encode_array_size(cs, 0);
    }
    if (val->pValues) {
        vn_encode_array_size(cs, val->semaphoreCount);
        vn_encode_uint64_t_array(cs, val->pValues, val->semaphoreCount);
    } else {
        vn_encode_array_size(cs, 0);
    }
}

static inline void
vn_encode_VkSemaphoreWaitInfo(struct vn_cs *cs, const VkSemaphoreWaitInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO });
    vn_encode_VkSemaphoreWaitInfo_pnext(cs, val->pNext);
    vn_encode_VkSemaphoreWaitInfo_self(cs, val);
}

/* struct VkSemaphoreSignalInfo chain */

static inline size_t
vn_sizeof_VkSemaphoreSignalInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkSemaphoreSignalInfo_self(const VkSemaphoreSignalInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkSemaphore(&val->semaphore);
    size += vn_sizeof_uint64_t(&val->value);
    return size;
}

static inline size_t
vn_sizeof_VkSemaphoreSignalInfo(const VkSemaphoreSignalInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkSemaphoreSignalInfo_pnext(val->pNext);
    size += vn_sizeof_VkSemaphoreSignalInfo_self(val);

    return size;
}

static inline void
vn_encode_VkSemaphoreSignalInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkSemaphoreSignalInfo_self(struct vn_cs *cs, const VkSemaphoreSignalInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkSemaphore(cs, &val->semaphore);
    vn_encode_uint64_t(cs, &val->value);
}

static inline void
vn_encode_VkSemaphoreSignalInfo(struct vn_cs *cs, const VkSemaphoreSignalInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO });
    vn_encode_VkSemaphoreSignalInfo_pnext(cs, val->pNext);
    vn_encode_VkSemaphoreSignalInfo_self(cs, val);
}

static inline void
vn_decode_VkSemaphoreSignalInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkSemaphoreSignalInfo_self(struct vn_cs *cs, VkSemaphoreSignalInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkSemaphore(cs, &val->semaphore);
    vn_decode_uint64_t(cs, &val->value);
}

static inline void
vn_decode_VkSemaphoreSignalInfo(struct vn_cs *cs, VkSemaphoreSignalInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO);

    assert(val->sType == stype);
    vn_decode_VkSemaphoreSignalInfo_pnext(cs, val->pNext);
    vn_decode_VkSemaphoreSignalInfo_self(cs, val);
}

/* struct VkImageDrmFormatModifierPropertiesEXT chain */

static inline size_t
vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_self(const VkImageDrmFormatModifierPropertiesEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_uint64_t(&val->drmFormatModifier);
    return size;
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierPropertiesEXT(const VkImageDrmFormatModifierPropertiesEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_pnext(val->pNext);
    size += vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_self(val);

    return size;
}

static inline void
vn_decode_VkImageDrmFormatModifierPropertiesEXT_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkImageDrmFormatModifierPropertiesEXT_self(struct vn_cs *cs, VkImageDrmFormatModifierPropertiesEXT *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_uint64_t(cs, &val->drmFormatModifier);
}

static inline void
vn_decode_VkImageDrmFormatModifierPropertiesEXT(struct vn_cs *cs, VkImageDrmFormatModifierPropertiesEXT *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT);

    assert(val->sType == stype);
    vn_decode_VkImageDrmFormatModifierPropertiesEXT_pnext(cs, val->pNext);
    vn_decode_VkImageDrmFormatModifierPropertiesEXT_self(cs, val);
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_pnext_partial(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_self_partial(const VkImageDrmFormatModifierPropertiesEXT *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    /* skip val->drmFormatModifier */
    return size;
}

static inline size_t
vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_partial(const VkImageDrmFormatModifierPropertiesEXT *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_pnext_partial(val->pNext);
    size += vn_sizeof_VkImageDrmFormatModifierPropertiesEXT_self_partial(val);

    return size;
}

static inline void
vn_encode_VkImageDrmFormatModifierPropertiesEXT_pnext_partial(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkImageDrmFormatModifierPropertiesEXT_self_partial(struct vn_cs *cs, const VkImageDrmFormatModifierPropertiesEXT *val)
{
    /* skip val->{sType,pNext} */
    /* skip val->drmFormatModifier */
}

static inline void
vn_encode_VkImageDrmFormatModifierPropertiesEXT_partial(struct vn_cs *cs, const VkImageDrmFormatModifierPropertiesEXT *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT });
    vn_encode_VkImageDrmFormatModifierPropertiesEXT_pnext_partial(cs, val->pNext);
    vn_encode_VkImageDrmFormatModifierPropertiesEXT_self_partial(cs, val);
}

/* struct VkBufferDeviceAddressInfo chain */

static inline size_t
vn_sizeof_VkBufferDeviceAddressInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkBufferDeviceAddressInfo_self(const VkBufferDeviceAddressInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkBuffer(&val->buffer);
    return size;
}

static inline size_t
vn_sizeof_VkBufferDeviceAddressInfo(const VkBufferDeviceAddressInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkBufferDeviceAddressInfo_pnext(val->pNext);
    size += vn_sizeof_VkBufferDeviceAddressInfo_self(val);

    return size;
}

static inline void
vn_encode_VkBufferDeviceAddressInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkBufferDeviceAddressInfo_self(struct vn_cs *cs, const VkBufferDeviceAddressInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkBuffer(cs, &val->buffer);
}

static inline void
vn_encode_VkBufferDeviceAddressInfo(struct vn_cs *cs, const VkBufferDeviceAddressInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO });
    vn_encode_VkBufferDeviceAddressInfo_pnext(cs, val->pNext);
    vn_encode_VkBufferDeviceAddressInfo_self(cs, val);
}

static inline void
vn_decode_VkBufferDeviceAddressInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkBufferDeviceAddressInfo_self(struct vn_cs *cs, VkBufferDeviceAddressInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkBuffer(cs, &val->buffer);
}

static inline void
vn_decode_VkBufferDeviceAddressInfo(struct vn_cs *cs, VkBufferDeviceAddressInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);

    assert(val->sType == stype);
    vn_decode_VkBufferDeviceAddressInfo_pnext(cs, val->pNext);
    vn_decode_VkBufferDeviceAddressInfo_self(cs, val);
}

/* struct VkDeviceMemoryOpaqueCaptureAddressInfo chain */

static inline size_t
vn_sizeof_VkDeviceMemoryOpaqueCaptureAddressInfo_pnext(const void *val)
{
    /* no known/supported struct */
    return vn_sizeof_simple_pointer(NULL);
}

static inline size_t
vn_sizeof_VkDeviceMemoryOpaqueCaptureAddressInfo_self(const VkDeviceMemoryOpaqueCaptureAddressInfo *val)
{
    size_t size = 0;
    /* skip val->{sType,pNext} */
    size += vn_sizeof_VkDeviceMemory(&val->memory);
    return size;
}

static inline size_t
vn_sizeof_VkDeviceMemoryOpaqueCaptureAddressInfo(const VkDeviceMemoryOpaqueCaptureAddressInfo *val)
{
    size_t size = 0;

    size += vn_sizeof_VkStructureType(&val->sType);
    size += vn_sizeof_VkDeviceMemoryOpaqueCaptureAddressInfo_pnext(val->pNext);
    size += vn_sizeof_VkDeviceMemoryOpaqueCaptureAddressInfo_self(val);

    return size;
}

static inline void
vn_encode_VkDeviceMemoryOpaqueCaptureAddressInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    vn_encode_simple_pointer(cs, NULL);
}

static inline void
vn_encode_VkDeviceMemoryOpaqueCaptureAddressInfo_self(struct vn_cs *cs, const VkDeviceMemoryOpaqueCaptureAddressInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_encode_VkDeviceMemory(cs, &val->memory);
}

static inline void
vn_encode_VkDeviceMemoryOpaqueCaptureAddressInfo(struct vn_cs *cs, const VkDeviceMemoryOpaqueCaptureAddressInfo *val)
{
    assert(val->sType == VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO);
    vn_encode_VkStructureType(cs, &(VkStructureType){ VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO });
    vn_encode_VkDeviceMemoryOpaqueCaptureAddressInfo_pnext(cs, val->pNext);
    vn_encode_VkDeviceMemoryOpaqueCaptureAddressInfo_self(cs, val);
}

static inline void
vn_decode_VkDeviceMemoryOpaqueCaptureAddressInfo_pnext(struct vn_cs *cs, const void *val)
{
    /* no known/supported struct */
    if (vn_decode_simple_pointer(cs))
        assert(false);
}

static inline void
vn_decode_VkDeviceMemoryOpaqueCaptureAddressInfo_self(struct vn_cs *cs, VkDeviceMemoryOpaqueCaptureAddressInfo *val)
{
    /* skip val->{sType,pNext} */
    vn_decode_VkDeviceMemory(cs, &val->memory);
}

static inline void
vn_decode_VkDeviceMemoryOpaqueCaptureAddressInfo(struct vn_cs *cs, VkDeviceMemoryOpaqueCaptureAddressInfo *val)
{
    VkStructureType stype;
    vn_decode_VkStructureType(cs, &stype);
    assert(stype == VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO);

    assert(val->sType == stype);
    vn_decode_VkDeviceMemoryOpaqueCaptureAddressInfo_pnext(cs, val->pNext);
    vn_decode_VkDeviceMemoryOpaqueCaptureAddressInfo_self(cs, val);
}

/* struct VkCommandStreamDescriptionMESA */

static inline size_t
vn_sizeof_VkCommandStreamDescriptionMESA(const VkCommandStreamDescriptionMESA *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->resourceId);
    size += vn_sizeof_size_t(&val->offset);
    size += vn_sizeof_size_t(&val->size);
    return size;
}

static inline void
vn_encode_VkCommandStreamDescriptionMESA(struct vn_cs *cs, const VkCommandStreamDescriptionMESA *val)
{
    vn_encode_uint32_t(cs, &val->resourceId);
    vn_encode_size_t(cs, &val->offset);
    vn_encode_size_t(cs, &val->size);
}

/* struct VkCommandStreamDependencyMESA */

static inline size_t
vn_sizeof_VkCommandStreamDependencyMESA(const VkCommandStreamDependencyMESA *val)
{
    size_t size = 0;
    size += vn_sizeof_uint32_t(&val->srcCommandStream);
    size += vn_sizeof_uint32_t(&val->dstCommandStream);
    return size;
}

static inline void
vn_encode_VkCommandStreamDependencyMESA(struct vn_cs *cs, const VkCommandStreamDependencyMESA *val)
{
    vn_encode_uint32_t(cs, &val->srcCommandStream);
    vn_encode_uint32_t(cs, &val->dstCommandStream);
}

/*
 * Helpers for manual serialization
 */

#endif /* VN_PROTOCOL_DRIVER_STRUCTS_H */
