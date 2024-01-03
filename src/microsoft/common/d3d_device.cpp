
/*
 * Copyright 2022 Yonggang Luo
 * SPDX-License-Identifier: MIT
 *
 */

#include "d3d_device.h"

#include "util/os_misc.h"
#include "util/u_debug.h"

#include "d3d12_common.h"

#if !defined(_GAMING_XBOX)
#include <directx/d3d12sdklayers.h>
#include <directx/dxcore.h>
#endif /* _GAMING_XBOX */

#if defined(_WIN32) && !defined(_GAMING_XBOX)
#include <dxgi1_4.h>
#endif

#include <dxguids/dxguids.h>

static_assert(sizeof(d3d_device_luid) == sizeof(uint64_t), "");
static_assert(sizeof(d3d_device_luid) == sizeof(LUID), "");
static_assert(alignof(d3d_device_luid) == alignof(LUID), "");

static const struct debug_named_value
d3d_choosed_type_options[] = {
   { "discrete",     d3d_device_hardware_discrete,     "Discrete graphics adapter" },
   { "integrated",   d3d_device_hardware_integrated,   "Integrated graphics adapter" },
   { "software",     d3d_device_software,              "Software emulation adapter" },
   { "all",          d3d_device_all,                   "All adapters" },
   DEBUG_NAMED_VALUE_END
};

static int d3d12_mod_refcount = 0;

static bool
d3d_list_add_item(const d3d_device_item &item, struct list_head *list)
{
   if (d3d_device_list_find_by_luid(list, &item.desc.adapter_luid) != nullptr) {
      /* The adapter_luid already present, ignore it */
      return false;
   }
   d3d_device_item *item_new = (d3d_device_item *)calloc(1, sizeof(d3d_device_item));
   if (!item_new) {
      return false;
   }
   *item_new = item;
   item_new->desc.memory_size_megabytes = (item.desc.dedicated_video_memory + item.desc.dedicated_system_memory + item.desc.shared_system_memory) >> 20;
   list_addtail(&item_new->link, list);
   return true;
}

#if defined(_WIN32) && !defined(_GAMING_XBOX)

static struct IDXGIFactory4 *
get_dxgi_factory(bool dxgi_factory_debug, struct util_dl_library *dxgi_mod)
{
   static const GUID IID_IDXGIFactory4 = {
      0x1bc6ea02, 0xef36, 0x464f,
      { 0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a }
   };

   typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY2)(UINT flags, REFIID riid, void **ppFactory);
   PFN_CREATE_DXGI_FACTORY2 CreateDXGIFactory2;

   CreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)util_dl_get_proc_address(dxgi_mod, "CreateDXGIFactory2");
   if (!CreateDXGIFactory2) {
      debug_printf("D3D12: failed to load CreateDXGIFactory2 from DXGI.DLL\n");
      return nullptr;
   }

   UINT flags = 0;
   if (dxgi_factory_debug)
      flags |= DXGI_CREATE_FACTORY_DEBUG;

   IDXGIFactory4 *factory = nullptr;
   HRESULT hr = CreateDXGIFactory2(flags, IID_IDXGIFactory4, (void **)&factory);
   if (FAILED(hr)) {
      debug_printf("D3D12: CreateDXGIFactory2 failed: %08x\n", (unsigned)hr);
      return nullptr;
   }
   return factory;
}

#endif /* defined(_WIN32) && !defined(_GAMING_XBOX) */

#if defined(_WIN32)

/**
 * @brief
 *
 * @param adapter adapter will be take over
 * @param list
 */
static void
d3d_add_dxgi_adapter(IDXGIAdapter *adapter, struct list_head *list)
{
   DXGI_ADAPTER_DESC dxgi_desc;
   if (SUCCEEDED(adapter->GetDesc(&dxgi_desc))) {
      d3d_device_item item = {};
      item.desc.type = d3d_device_hardware_discrete;
#if defined(_GAMING_XBOX)
      item.desc.factory_type = d3d_factory_xbox;
#else
      item.desc.factory_type = d3d_factory_dxgi;
#endif
      item.desc.vendor_id = dxgi_desc.VendorId;
      item.desc.device_id = dxgi_desc.DeviceId;
      item.desc.subsys_id = dxgi_desc.SubSysId;
      item.desc.revision = dxgi_desc.Revision;
      item.desc.shared_system_memory = dxgi_desc.SharedSystemMemory;
      item.desc.dedicated_system_memory = dxgi_desc.DedicatedSystemMemory;
      item.desc.dedicated_video_memory = dxgi_desc.DedicatedVideoMemory;
      memcpy(&item.desc.adapter_luid, &dxgi_desc.AdapterLuid, sizeof(item.desc.adapter_luid));

      /* Retrieve the driver version */
      LARGE_INTEGER driver_version;
      adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver_version);
      item.desc.driver_version = driver_version.QuadPart;

      WideCharToMultiByte(CP_UTF8, 0, dxgi_desc.Description, ARRAYSIZE(dxgi_desc.Description),
                          item.desc.description, ARRAYSIZE(item.desc.description), nullptr, nullptr);
#if !defined(_GAMING_XBOX)
      IDXGIAdapter1 *adapter1;
      if (SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&adapter1)))) {
         DXGI_ADAPTER_DESC1 dxgi_desc1;
         if (SUCCEEDED(adapter1->GetDesc1(&dxgi_desc1))) {
            if (dxgi_desc1.Flags != DXGI_ADAPTER_FLAG_NONE)
            {
               /* DXGI_ADAPTER_FLAG_REMOTE is unused, so none DXGI_ADAPTER_FLAG_NONE is software */
               item.desc.type = d3d_device_software;
            }
         }
         adapter1->Release();
      }
#endif
      item.adapter = adapter;
      if (d3d_list_add_item(item, list))
         return;
   }
   /* Add to list failed, release the adapter */
   adapter->Release();
}

#endif

#if defined(_GAMING_XBOX)

ID3D12Device3 *
d3d_device_info_create_d3d12(d3d_device_info *info, d3d_device_create_options *options, IUnknown *adapter)
{
   D3D12XBOX_PROCESS_DEBUG_FLAGS debugFlags =
      D3D12XBOX_PROCESS_DEBUG_FLAG_ENABLE_COMMON_STATE_PROMOTION; /* For compatibility with desktop D3D12 */

   if (options->debug_experimental) {
      debug_printf("D3D12: experimental shader models are not supported on GDKX\n");
      return nullptr;
   }

   if (info->options.debug_gpu_validator) {
      debug_printf("D3D12: gpu validation is not supported on GDKX\n"); /* FIXME: Is this right? */
      return nullptr;
   }

   if (info->options.debug_debug_layer)
      debugFlags |= D3D12XBOX_PROCESS_DEBUG_FLAG_DEBUG;

   D3D12XBOX_CREATE_DEVICE_PARAMETERS params = {};
   params.Version = D3D12_SDK_VERSION;
   params.ProcessDebugFlags = debugFlags;
   params.GraphicsCommandQueueRingSizeBytes = D3D12XBOX_DEFAULT_SIZE_BYTES;
   params.GraphicsScratchMemorySizeBytes = D3D12XBOX_DEFAULT_SIZE_BYTES;
   params.ComputeScratchMemorySizeBytes = D3D12XBOX_DEFAULT_SIZE_BYTES;

   typedef HRESULT(WINAPI * PFN_D3D12XBOXCREATEDEVICE)(IGraphicsUnknown *, const D3D12XBOX_CREATE_DEVICE_PARAMETERS *, REFIID, void **);
   PFN_D3D12XBOXCREATEDEVICE D3D12XboxCreateDevice =
      (PFN_D3D12XBOXCREATEDEVICE) util_dl_get_proc_address(info->d3d12_mod, "D3D12XboxCreateDevice");
   if (!D3D12XboxCreateDevice) {
      debug_printf("D3D12: failed to load D3D12XboxCreateDevice from D3D12 DLL\n");
      return nullptr;
   }
   ID3D12Device3 *dev = nullptr;
   if (FAILED(D3D12XboxCreateDevice((IGraphicsUnknown*) adapter, &params, IID_PPV_ARGS(&dev))))
      debug_printf("D3D12: D3D12XboxCreateDevice failed\n");
   return dev;
}

#else /* !defined(_GAMING_XBOX) */

static IDXCoreAdapterFactory *
get_dxcore_factory(struct util_dl_library *dxcore_mod)
{
   typedef HRESULT(WINAPI *PFN_CREATE_DXCORE_ADAPTER_FACTORY)(REFIID riid, void **ppFactory);
   PFN_CREATE_DXCORE_ADAPTER_FACTORY DXCoreCreateAdapterFactory;

   DXCoreCreateAdapterFactory = (PFN_CREATE_DXCORE_ADAPTER_FACTORY)util_dl_get_proc_address(dxcore_mod, "DXCoreCreateAdapterFactory");
   if (!DXCoreCreateAdapterFactory) {
      debug_printf("D3D12: failed to load DXCoreCreateAdapterFactory from DXCore.DLL\n");
      return nullptr;
   }

   IDXCoreAdapterFactory *factory = nullptr;
   HRESULT hr = DXCoreCreateAdapterFactory(IID_IDXCoreAdapterFactory, (void **)&factory);
   if (FAILED(hr)) {
      debug_printf("D3D12: DXCoreCreateAdapterFactory failed: %08x\n", (unsigned)hr);
      return nullptr;
   }

   return factory;
}

/**
 * @brief
 *
 * @param adapter adapter will be take over
 * @param list
 */
static void
d3d_add_dxcore_adapter(IDXCoreAdapter *adapter, struct list_head *list)
{
   d3d_device_item item = {};
   DXCoreHardwareID hardware_id;
   bool is_hardware;
   bool is_integrated;
   if (FAILED(adapter->GetProperty(DXCoreAdapterProperty::HardwareID, &hardware_id)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::DedicatedAdapterMemory, &item.desc.dedicated_video_memory)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::SharedSystemMemory, &item.desc.shared_system_memory)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::DedicatedSystemMemory, &item.desc.dedicated_system_memory)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &item.desc.adapter_luid)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &is_hardware)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::IsIntegrated, &is_integrated)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::DriverVersion, &item.desc.driver_version)) ||
       FAILED(adapter->GetProperty(DXCoreAdapterProperty::DriverDescription, sizeof(item.desc.description), item.desc.description))
   ) {
      adapter->Release();
   } else {
      if (is_hardware) {
         if (is_integrated) {
            item.desc.type = d3d_device_hardware_integrated;
         } else {
            item.desc.type = d3d_device_hardware_discrete;
         }
      } else {
         item.desc.type = d3d_device_software;
      }
      item.desc.factory_type = d3d_factory_dxcore;
      item.desc.vendor_id = hardware_id.vendorID;
      item.desc.device_id = hardware_id.deviceID;
      item.desc.subsys_id = hardware_id.subSysID;
      item.desc.revision = hardware_id.revision;
      item.adapter = adapter;
      if (d3d_list_add_item(item, list))
         return;
   }
   /* Add to list failed, release the adapter */
   adapter->Release();
}

#ifdef _WIN32
extern "C" IMAGE_DOS_HEADER __ImageBase;
static const char *
try_find_d3d12core_next_to_self(char *path, size_t path_arr_size)
{
   uint32_t path_size = GetModuleFileNameA((HINSTANCE)&__ImageBase,
                                           path, path_arr_size);
   if (!path_arr_size || path_size == path_arr_size) {
      debug_printf("Unable to get path to self\n");
      return nullptr;
   }

   auto last_slash = strrchr(path, '\\');
   if (!last_slash) {
      debug_printf("Unable to get path to self\n");
      return nullptr;
   }

   *(last_slash + 1) = '\0';
   if (strcat_s(path, path_arr_size, "D3D12Core.dll") != 0) {
      debug_printf("Unable to get path to D3D12Core.dll next to self\n");
      return nullptr;
   }

   if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
      debug_printf("No D3D12Core.dll exists next to self\n");
      return nullptr;
   }

   /* The returned path is the directory of "D3D12Core.dll" */
   *(last_slash + 1) = '\0';

   return path;
}
#endif

static ID3D12DeviceFactory *
try_create_device_factory(d3d_device_info_optoins *options, struct util_dl_library *d3d12_mod)
{
   /* A device factory allows us to isolate things like debug layer enablement from other callers,
    * and can potentially even refer to a different D3D12 redist implementation from others.
    */
   ID3D12DeviceFactory *factory = nullptr;
   char self_path[MAX_PATH];
   HRESULT hr;

   typedef HRESULT(WINAPI *PFN_D3D12_GET_INTERFACE)(REFCLSID clsid, REFIID riid, void **ppFactory);
   PFN_D3D12_GET_INTERFACE D3D12GetInterface = (PFN_D3D12_GET_INTERFACE)util_dl_get_proc_address(d3d12_mod, "D3D12GetInterface");
   if (!D3D12GetInterface) {
      debug_printf("D3D12: Failed to retrieve D3D12GetInterface\n");
      return nullptr;
   }

#ifdef _WIN32
   /* First, try to create a device factory from a DLL-parallel D3D12Core.dll */
   ID3D12SDKConfiguration *sdk_config = nullptr;
   if (SUCCEEDED(D3D12GetInterface(CLSID_D3D12SDKConfiguration, IID_PPV_ARGS(&sdk_config)))) {
      /* It's possible there's a D3D12Core.dll next to the .exe, for development/testing purposes.
       * If so, the D3D12 Agility SDKPath path (either relative to the current .exe or absolute path)
       * is notified by environment variable; Along with the SDKPath path, the D3D12 Agility version
       * is also notified by environment variable.
       */
      ID3D12SDKConfiguration1 *sdk_config1 = nullptr;
      if (SUCCEEDED(sdk_config->QueryInterface(&sdk_config1))) {
         do {
            if (options->agility_sdk_path_cached && options->agility_sdk_version > 0) {
               hr = sdk_config1->CreateDeviceFactory(options->agility_sdk_version, options->agility_sdk_path_cached, IID_PPV_ARGS(&factory));
               if (SUCCEEDED(hr)) break;
            }

            const char *d3d12core_dir = try_find_d3d12core_next_to_self(self_path, sizeof(self_path));
            if (d3d12core_dir) {
               hr = sdk_config1->CreateDeviceFactory(D3D12_PREVIEW_SDK_VERSION, d3d12core_dir, IID_PPV_ARGS(&factory));
               if (SUCCEEDED(hr)) break;
               hr = sdk_config1->CreateDeviceFactory(D3D12_SDK_VERSION, d3d12core_dir, IID_PPV_ARGS(&factory));
               if (SUCCEEDED(hr)) break;
            }
         } while (0);

         sdk_config1->Release();
      } else {
         /**
          * Once SetSDKVersion take effect(that means D3D12Core.dll are loaded), then the result of
          * SetSDKVersion will always to be failed. So we only SetSDKVersion when d3d12.dll are just
          * newly loaded(the unload of d3d12.dll needs to be considered, so we use refcount to
          * take care of that)
          */
         if (d3d12_mod_refcount == 1 &&
             options->agility_sdk_path_cached &&
             options->agility_sdk_version > 0) {
            hr = sdk_config->SetSDKVersion(options->agility_sdk_version, options->agility_sdk_path_cached);
            if (FAILED(hr)) {
               GetModuleFileNameA(NULL, self_path, sizeof(self_path));
               debug_printf("D3D12: SetSDKVersion with pid:%d tid:%u exec_path:%s sdk_path:%s version:%d hr:0x%x\n",
                            (unsigned)GetCurrentProcessId(), (unsigned)GetCurrentThreadId(),
                            self_path, options->agility_sdk_path_cached, options->agility_sdk_version, (int)hr);
            }
         }
      }
      sdk_config->Release();
   }
#endif

   if (!factory) {
      /* Nope, seems we don't have a matching D3D12Core.dll next to ourselves or by environment variables */
      hr = D3D12GetInterface(CLSID_D3D12DeviceFactory, IID_PPV_ARGS(&factory));
      if (FAILED(hr)) {
         factory = nullptr;
      }
   }
   return factory;
}

static ID3D12Debug *
get_debug_interface(struct util_dl_library *d3d12_mod, ID3D12DeviceFactory *factory)
{
   ID3D12Debug *debug = nullptr;
   if (factory) {
      factory->GetConfigurationInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&debug));
      return debug;
   }

   typedef HRESULT(WINAPI *PFN_D3D12_GET_DEBUG_INTERFACE)(REFIID riid, void **ppFactory);
   PFN_D3D12_GET_DEBUG_INTERFACE D3D12GetDebugInterface;

   D3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)util_dl_get_proc_address(d3d12_mod, "D3D12GetDebugInterface");
   if (!D3D12GetDebugInterface) {
      debug_printf("D3D12: failed to load D3D12GetDebugInterface from D3D12.DLL\n");
      return nullptr;
   }

   if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
      debug_printf("D3D12: D3D12GetDebugInterface failed\n");
      return nullptr;
   }

   return debug;
}

static void
enable_d3d12_debug_layer(struct util_dl_library *d3d12_mod, ID3D12DeviceFactory *factory)
{
   ID3D12Debug *debug = get_debug_interface(d3d12_mod, factory);
   if (debug) {
      debug->EnableDebugLayer();
      debug->Release();
   }
}

static void
enable_gpu_validation(struct util_dl_library *d3d12_mod, ID3D12DeviceFactory *factory)
{
   ID3D12Debug *debug = get_debug_interface(d3d12_mod, factory);
   ID3D12Debug3 *debug3;
   if (debug) {
      if (SUCCEEDED(debug->QueryInterface(IID_PPV_ARGS(&debug3)))) {
         debug3->SetEnableGPUBasedValidation(true);
         debug3->Release();
      }
      debug->Release();
   }
}

ID3D12Device3 *
d3d_device_info_create_d3d12(d3d_device_info *info, d3d_device_create_options *options, IUnknown *adapter)
{
   ID3D12DeviceFactory *factory = info->d3d12_factory;
#ifdef _WIN32
   if (options->debug_experimental)
#endif
   {
      if (factory) {
         if (FAILED(factory->EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr))) {
            debug_printf("D3D12: failed to enable experimental shader models\n");
            return nullptr;
         }
      } else {
         typedef HRESULT(WINAPI *PFN_D3D12ENABLEEXPERIMENTALFEATURES)(UINT, const IID*, void*, UINT*);
         PFN_D3D12ENABLEEXPERIMENTALFEATURES D3D12EnableExperimentalFeatures =
            (PFN_D3D12ENABLEEXPERIMENTALFEATURES)util_dl_get_proc_address(info->d3d12_mod, "D3D12EnableExperimentalFeatures");

         if (!D3D12EnableExperimentalFeatures ||
             FAILED(D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr))) {
            debug_printf("D3D12: failed to enable experimental shader models\n");
            return nullptr;
         }
      }
   }

   if (options->debug_singleton) {
      /* Use the default D3D12CreateDevice */
      factory = nullptr;
      adapter = nullptr;
   }

   ID3D12Device3 *dev = nullptr;
   if (factory) {
      factory->SetFlags(D3D12_DEVICE_FACTORY_FLAG_ALLOW_RETURNING_EXISTING_DEVICE |
                        D3D12_DEVICE_FACTORY_FLAG_ALLOW_RETURNING_INCOMPATIBLE_EXISTING_DEVICE);
      if (FAILED(factory->CreateDevice(adapter, (D3D_FEATURE_LEVEL)options->d3d_feature_level, IID_PPV_ARGS(&dev))))
         debug_printf("D3D12: D3D12CreateDevice failed by factory\n");
   } else {
      typedef HRESULT(WINAPI *PFN_D3D12CREATEDEVICE)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
      PFN_D3D12CREATEDEVICE D3D12CreateDevice = (PFN_D3D12CREATEDEVICE)util_dl_get_proc_address(info->d3d12_mod, "D3D12CreateDevice");
      if (!D3D12CreateDevice) {
         debug_printf("D3D12: failed to load D3D12CreateDevice from D3D12.DLL\n");
         return nullptr;
      }
      if (FAILED(D3D12CreateDevice(adapter, (D3D_FEATURE_LEVEL)options->d3d_feature_level, IID_PPV_ARGS(&dev))))
         debug_printf("D3D12: D3D12CreateDevice failed\n");
   }

   return dev;
}

#endif /* defined(_GAMING_XBOX) */

bool
d3d_device_info_load(d3d_device_info *info, d3d_device_info_optoins *options)
{
   memset(info, 0, sizeof(*info));
   info->options = *options;
   list_inithead(&info->list);

   const char d3d12_mod_name[] = UTIL_DL_PREFIX
#ifdef _GAMING_XBOX_SCARLETT
      "d3d12_xs"
#elif defined(_GAMING_XBOX)
      "d3d12_x"
#else
      "d3d12"
#endif
      UTIL_DL_EXT;
   info->d3d12_mod = util_dl_open(d3d12_mod_name);
   if (!info->d3d12_mod) {
      debug_printf("D3D12: failed to load %s\n", d3d12_mod_name);
      return false;
   }
   d3d12_mod_refcount += 1;

   info->d3d12_factory = try_create_device_factory(options, info->d3d12_mod);
   if (options->debug_debug_layer)
      enable_d3d12_debug_layer(info->d3d12_mod, info->d3d12_factory);

   if (options->debug_gpu_validator)
      enable_gpu_validation(info->d3d12_mod, info->d3d12_factory);

#if !defined(_GAMING_XBOX)
   /* dxcore support for both Win32(not XBOX) and Linux */
   info->dxcore_mod = util_dl_open(UTIL_DL_PREFIX "dxcore" UTIL_DL_EXT);
   if (info->dxcore_mod) {
      info->dxcore_factory = get_dxcore_factory(info->dxcore_mod);
      if (options->load_list && info->dxcore_factory) {
         IDXCoreAdapterList *list = nullptr;
         if (SUCCEEDED(info->dxcore_factory->CreateAdapterList(1, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS, IID_PPV_ARGS(&list)))) {
            uint32_t adapter_count = list->GetAdapterCount();
            for (uint32_t i = 0; i < adapter_count; ++i) {
               IDXCoreAdapter *adapter = nullptr;
               if (SUCCEEDED(list->GetAdapter(i, IID_PPV_ARGS(&adapter)))) {
                  d3d_add_dxcore_adapter(adapter, &info->list);
               }
            }
            list->Release();
         }
      }
   }
#endif

#if defined(_WIN32) && !defined(_GAMING_XBOX)
   /* dxgi support for both Win32(not XBOX) only */
   info->dxgi_mod = util_dl_open(UTIL_DL_PREFIX "dxgi" UTIL_DL_EXT);
   if (info->dxgi_mod) {
      info->dxgi_factory = get_dxgi_factory(options->dxgi_factory_debug, info->dxgi_mod);
      if (options->load_list && info->dxgi_factory) {
         IDXGIAdapter *adapter;
         for (unsigned i = 0; SUCCEEDED(info->dxgi_factory->EnumAdapters(i, &adapter)); i++) {
            d3d_add_dxgi_adapter(adapter, &info->list);
         }
      }
   }
#endif

#if defined(_GAMING_XBOX)
   /* XBOX have no dxgi dll but have IDXGIAdapter adapter */
   if (options->load_list)
   {
      d3d_device_create_options device_options = {};
      device_options.d3d_feature_level = D3D_FEATURE_LEVEL_11_0;
      ID3D12Device3 *dev = d3d_device_info_create_d3d12(info, &device_options, nullptr);
      if (dev != nullptr) {
         IDXGIDevice1 *dxgiDevice = nullptr;
         if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
            IDXGIAdapter *adapter = nullptr;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
               d3d_add_dxgi_adapter(adapter, &info->list);
            }
            dxgiDevice->Release();
         } else {
            debug_printf("D3D12: failed to query dxgi interface\n");
         }
         dev->Release();
      }
   }
#endif

   return true;
}

void
d3d_device_info_unload(d3d_device_info *info)
{
   /* Free the list */
   list_for_each_entry_safe(d3d_device_item, pos, &info->list, link) {
      pos->adapter->Release();
      list_del(&pos->link);
      free(pos);
   }
#if defined(_WIN32) && !defined(_GAMING_XBOX)
   if (info->dxgi_factory) {
      info->dxgi_factory->Release();
      info->dxgi_factory = nullptr;
   }
   if (info->dxgi_mod) {
      util_dl_close(info->dxgi_mod);
      info->dxgi_mod = nullptr;
   }
#endif
#if !defined(_GAMING_XBOX)
   if (info->dxcore_factory) {
      info->dxcore_factory->Release();
      info->dxcore_factory = nullptr;
   }
   if (info->dxcore_mod) {
      util_dl_close(info->dxcore_mod);
      info->dxcore_mod = nullptr;
   }
#endif

   if (info->d3d12_factory) {
      info->d3d12_factory->Release();
      info->d3d12_factory = nullptr;
   }

   if (info->d3d12_mod) {
      util_dl_close(info->d3d12_mod);
      info->d3d12_mod = nullptr;
      d3d12_mod_refcount -= 1;
   }
}

d3d_device_item *
d3d_device_list_find_by_luid(struct list_head *list, const d3d_device_luid *luid)
{
   list_for_each_entry(d3d_device_item, pos, list, link) {
      if (memcmp(&pos->desc.adapter_luid, luid, sizeof(*luid)) == 0)
         return pos;
   }
   return nullptr;
}

/**
 * @brief Choose the proper adapter through choose options
 *
 * @param list
 * @param options
 * @return d3d_device_item*
 */
d3d_device_item *
d3d_device_list_choose(struct list_head *list, const d3d_device_choose_options *options)
{
   uint64_t adapter_type_choosed_flags;
   d3d_device_luid adapter_luid_choosed;
   d3d_device_luid *adapter_luid = options->adapter_luid;
   if (!adapter_luid) {
      const char *adapter_luid_str = options->adapter_luid_env_key ? os_get_option(options->adapter_luid_env_key) : NULL;
      uint64_t adapter_luid_value;
      if (adapter_luid_str != nullptr && sscanf(adapter_luid_str, "%llx", &adapter_luid_value) == 1) {
         memcpy(&adapter_luid_choosed, &adapter_luid_value, sizeof(adapter_luid_value));
         adapter_luid = &adapter_luid_choosed;
      }
   }
   if (adapter_luid) {
      /* Choose device_item/adapter by adapter_luid  */
      return d3d_device_list_find_by_luid(list, adapter_luid);
   }
   adapter_type_choosed_flags= debug_get_flags_option(options->adapter_type_env_key,
                                                      d3d_choosed_type_options,
                                                      d3d_device_all);
   enum d3d_device_type adapter_type_choosed = (enum d3d_device_type)adapter_type_choosed_flags;
   const char *adapter_name = options->adapter_name_env_key ? os_get_option(options->adapter_name_env_key) : NULL;
   d3d_device_item *device_item = nullptr;
   d3d_device_item *hardware_discrete_item = nullptr;
   d3d_device_item *hardware_integrated_item = nullptr;
   d3d_device_item *software_item = nullptr;
   list_for_each_entry(d3d_device_item, pos, list, link) {
      /* Choose by adapter_name */
      if (adapter_name != nullptr && strcmp(adapter_name, pos->desc.description) == 0) {
         device_item  = pos;
         break;
      }
      if ((pos->desc.type & adapter_type_choosed) == 0) {
         continue;
      }
      if (pos->desc.type == d3d_device_hardware_discrete) {
         if (hardware_discrete_item == nullptr) {
            hardware_discrete_item = pos;
         }
      } else if (pos->desc.type == d3d_device_hardware_integrated) {
         if (hardware_integrated_item == nullptr) {
            hardware_integrated_item = pos;
         }
      } else if (pos->desc.type == d3d_device_software) {
         if (software_item == nullptr) {
            software_item = pos;
         }
      }
   }

   // Choose by hardware(discrete)
   if (device_item == nullptr && hardware_discrete_item) {
      device_item = hardware_discrete_item;
   }

   // Choose by hardware(integrated)
   if (device_item == nullptr && hardware_integrated_item) {
      device_item = hardware_integrated_item;
   }

   // No discrete/integrated GPUs, so pick the first software one
   if (device_item == nullptr && software_item) {
      device_item = software_item;
   }
   return device_item;
}

void
d3d_device_get_memory_info(d3d_device_info *info, d3d_device_item *item, d3d_device_memory_info *memory_info)
{
   if (item->desc.factory_type == d3d_factory_dxcore) {
#if !defined(_GAMING_XBOX)
      IDXCoreAdapter *adapter = nullptr;
      if (SUCCEEDED(item->adapter->QueryInterface(IID_PPV_ARGS(&adapter)))) {
         DXCoreAdapterMemoryBudget local_info, nonlocal_info;
         DXCoreAdapterMemoryBudgetNodeSegmentGroup local_node_segment = { 0, DXCoreSegmentGroup::Local };
         DXCoreAdapterMemoryBudgetNodeSegmentGroup nonlocal_node_segment = { 0, DXCoreSegmentGroup::NonLocal };
         adapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &local_node_segment, &local_info);
         adapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &nonlocal_node_segment, &nonlocal_info);
         memory_info->budget = local_info.budget + nonlocal_info.budget;
         memory_info->usage = local_info.currentUsage + nonlocal_info.currentUsage;
      }
#endif
   } else if (item->desc.factory_type == d3d_factory_dxgi) {
#if defined(_WIN32) && !defined(_GAMING_XBOX)
      IDXGIAdapter3 *adapter = nullptr;
      if (SUCCEEDED(item->adapter->QueryInterface(IID_PPV_ARGS(&adapter)))) {
         DXGI_QUERY_VIDEO_MEMORY_INFO local_info, nonlocal_info;
         adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local_info);
         adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonlocal_info);
         memory_info->budget = local_info.Budget + nonlocal_info.Budget;
         memory_info->usage = local_info.CurrentUsage + nonlocal_info.CurrentUsage;
      }
#endif
   } else {
      /* d3d_factory_xbox do nothing  */
   }
}
