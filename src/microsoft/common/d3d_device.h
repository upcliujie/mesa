
/*
 * Copyright 2022 Yonggang Luo
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef D3D_DEVICE_H
#define D3D_DEVICE_H

#include <stdint.h>
#include "util/u_dl.h"
#include "util/list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IUnknown IUnknown;
typedef struct IDXGIFactory4 IDXGIFactory4;
typedef struct IDXCoreAdapterFactory IDXCoreAdapterFactory;

typedef struct ID3D12DeviceFactory ID3D12DeviceFactory;
typedef struct ID3D12Device3 ID3D12Device3;

/**
 * @brief We have
 * static_assert(sizeof(d3d_device_luid) == sizeof(LUID), "");
 * static_assert(alignof(d3d_device_luid) == alignof(LUID), "");
 */
typedef struct
{
   uint32_t low;
   uint32_t high;
} d3d_device_luid;

typedef struct {
   uint64_t usage;
   uint64_t budget;
} d3d_device_memory_info;

enum d3d_factory_type {
   d3d_factory_dxgi = 0,
   d3d_factory_dxcore = 1,
   d3d_factory_xbox = 2,
};

enum d3d_device_type {
   /**
    * any of
    * DXGI_ADAPTER_FLAG_NONE for DXGI
    * DXCoreAdapterProperty::IsHardware==true and DXCoreAdapterProperty::IsIntegrated==false for DXCore
    * XBOX
    */
   d3d_device_hardware_discrete = 1 << 0,
   /* DXCoreAdapterProperty::IsHardware==true and DXCoreAdapterProperty::IsIntegrated==true for DXCore */
   d3d_device_hardware_integrated = 1 << 1,
   /**
    * any of
    * DXGI_ADAPTER_FLAG_SOFTWARE for DXGI
    * DXCoreAdapterProperty::IsHardware==false for DXCore
    */
   d3d_device_software = 1 << 2,
   d3d_device_all = d3d_device_hardware_discrete | d3d_device_hardware_integrated | d3d_device_software,
};

typedef struct {
   enum d3d_device_type type;
   enum d3d_factory_type factory_type;
   uint32_t vendor_id;
   uint32_t device_id;
   uint32_t subsys_id;
   uint32_t revision;
   uint64_t driver_version;
   uint64_t shared_system_memory;
   uint64_t dedicated_system_memory;
   uint64_t dedicated_video_memory;
   uint64_t memory_size_megabytes;

   d3d_device_luid adapter_luid;
   char description[256]; /* UTF-8 encoding */
} d3d_device_desc;

typedef struct {
   struct list_head link; /* link for device list */
   d3d_device_desc desc;

   /**
    * d3d_factory_dxgi IDXGIAdapter*(XBOX or Win32)
    * d3d_factory_dxcore IDXCoreAdapter*(Win32/Linux)
    */
   IUnknown *adapter;
} d3d_device_item;

typedef struct
{
   bool load_list;
   bool dxgi_factory_debug;
   bool debug_debug_layer;
   bool debug_gpu_validator;
   const char *agility_sdk_path_cached;
   int agility_sdk_version;
} d3d_device_info_optoins;

typedef struct
{
   int d3d_feature_level;
   bool debug_experimental;
   bool debug_singleton;
} d3d_device_create_options;

typedef struct
{
   d3d_device_luid *adapter_luid;
   const char *adapter_luid_env_key;
   const char *adapter_name_env_key;
   const char *adapter_type_env_key;
} d3d_device_choose_options;

typedef struct
{
   /* input */
   d3d_device_info_optoins options;
   /* output */
   struct util_dl_library *d3d12_mod;
   ID3D12DeviceFactory *d3d12_factory;
#if !defined(_GAMING_XBOX)
   /* Factory for Win32(not XBOX)/Linux */
   struct util_dl_library *dxcore_mod;
   IDXCoreAdapterFactory *dxcore_factory;
#endif
#if defined(_WIN32) && !defined(_GAMING_XBOX)
   /* Factory for Win32(not XBOX)*/
   struct util_dl_library *dxgi_mod;
   IDXGIFactory4 *dxgi_factory;
#endif
#if defined(_GAMING_XBOX)
   /* XBOX have no factory */
#endif
   struct list_head list; /* device list */
} d3d_device_info;

bool
d3d_device_info_load(d3d_device_info *info, d3d_device_info_optoins *options);

void
d3d_device_info_unload(d3d_device_info *info);

d3d_device_item *
d3d_device_list_find_by_luid(struct list_head *list, const d3d_device_luid *luid);

d3d_device_item *
d3d_device_list_choose(struct list_head *list, const d3d_device_choose_options *options);

ID3D12Device3 *
d3d_device_info_create_d3d12(d3d_device_info *info, d3d_device_create_options *options, IUnknown *adapter);

void
d3d_device_get_memory_info(d3d_device_info *info, d3d_device_item *item, d3d_device_memory_info *memory_info);

#ifdef __cplusplus
}
#endif

#endif /* D3D_DEVICE_H */
