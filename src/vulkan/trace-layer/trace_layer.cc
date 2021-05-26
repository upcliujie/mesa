/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "trace_layer.h"

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

static void
traceInit()
{
   perfetto::TracingInitArgs args;
   args.backends = perfetto::kSystemBackend;
   perfetto::Tracing::Initialize(args);
   perfetto::TrackEvent::Register();
}

VK_LAYER_EXPORT VkResult
vkEnumerateInstanceExtensionPropertiesChain(
   const struct VkEnumerateInstanceExtensionPropertiesChain *chain,
   const char *pLayerName,
   uint32_t *pPropertyCount,
   VkExtensionProperties *pProperties)
{
   TRACE("traceEnumerateInstanceExtensionProperties");
   return chain->pfnNextLayer(chain->pNextLink, pLayerName, pPropertyCount,
                              pProperties);
}

VK_LAYER_EXPORT VkResult
vkEnumerateInstanceLayerPropertiesChain(
   const struct VkEnumerateInstanceLayerPropertiesChain *chain,
   uint32_t *pPropertyCount,
   VkLayerProperties *pProperties)
{
   TRACE("traceEnumerateInstanceLayerProperties");
   return chain->pfnNextLayer(chain->pNextLink, pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult
vkEnumerateInstanceVersionChain(
   const struct VkEnumerateInstanceVersionChain *chain, uint32_t *pApiVersion)
{
   TRACE("traceEnumerateInstanceVersion");
   return chain->pfnNextLayer(chain->pNextLink, pApiVersion);
}

VK_LAYER_EXPORT VkResult
vkNegotiateLoaderLayerInterfaceVersion(
   VkNegotiateLayerInterface *pVersionStruct)
{
   if (pVersionStruct->loaderLayerInterfaceVersion < 2)
      return VK_ERROR_INITIALIZATION_FAILED;
   pVersionStruct->loaderLayerInterfaceVersion = 2;

   pVersionStruct->pfnGetInstanceProcAddr =
      reinterpret_cast<PFN_vkGetInstanceProcAddr>(
         traceGetLayerProcAddr("vkGetInstanceProcAddr", false));
   pVersionStruct->pfnGetDeviceProcAddr =
      reinterpret_cast<PFN_vkGetDeviceProcAddr>(
         traceGetLayerProcAddr("vkGetDeviceProcAddr", true));

   traceInit();

   return VK_SUCCESS;
}
