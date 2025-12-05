#include <stdint.h>
#include <stdatomic.h>
#include <sys/cdefs.h>
#define VOLK_IMPLEMENTATION
#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>


//#ifdef CARBONARA_TARGET_PLATFORM_WIN32
//#include <vulkan/vulkan_win32.h>
//#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB_OR_XLIB
//#include <vulkan/vulkan_wayland.h>
//#include <xcb/xcb.h>
//#include <vulkan/vulkan_xcb.h>
//#include <X11/Xlib.h>
//#include <vulkan/vulkan_xlib.h>
//#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
//#include <vulkan/vulkan_android.h>
//#endif

#include "appdev_support-processor_map.h"
#include "carbonara-config.h"
#include "carbonara-mm.h"
#include "carbonara-singleton.h"
#include "carbonara-graphics.h"
#include "carbonara-texture.h"
#include "util-c_lang-basic.h"
//#include "vulkan_functions.h"
#include "memorable_arena.h"
#include "err_msg.h"

#ifdef DEBUG_MODE
#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
#if !defined( ARE_WE_SHIPPING_IT )
  [[maybe_unused]]
  static const char * enabled_layer_names[] = {
    "VK_LAYER_KHRONOS_validation",
    //"VK_LAYER_LUNARG_crash_diagnostic",  // This makes vkCreateDevice complain about requesting a
                                           //  feature that is not present in the driver.
  };
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-array"
static const char * const names_instance_extensions_required[] = {
#if !defined( ARE_WE_SHIPPING_IT )
  //VK_EXT_LAYER_SETTINGS_EXTENSION_NAME,
#endif
};
#pragma GCC diagnostic pop

static const char * const names_device_extensions_required[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,  // To draw image on screen

  //VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,  // https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/extensions/VK_KHR_synchronization2.adoc
                                            // Btw, this is not enough, You need to explicitly chain
                                            //  VkPhysicalDeviceVulkan13Features when creating a
                                            //  logical device

  //VK_KHR_MAINTENANCE1_EXTENSION_NAME,  // For various stuff https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_maintenance1.html
  VK_KHR_MAINTENANCE_8_EXTENSION_NAME,  // To make src/dst sync scopes to work when doing queue
                                        //  family ownership transfers.
};

static constexpr unsigned int test_width = 1600;
static constexpr unsigned int test_height = 900;

static constexpr unsigned int vulkan_version_major = 1;
static constexpr unsigned int vulkan_version_minor = 3;
// NOTE 1.2 for vkCmdDrawIndexedIndirectCount
// TODO Probablly bump it up to 1.3 for VK_KHR_synchronization2 ( https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/extensions/VK_KHR_synchronization2.adoc especially for the improvement in image layout transition in vkCmdSetEvent2KHR and vkCmdWaitEvents2 )

struct cara_vk_single cara_vk;


//-------------------------------------------------------------------------------------------------
//void init_graphics(void)
//{
//}


//-------------------------------------------------------------------------------------------------
uint8_t init_vulkan(
    [[maybe_unused]] uint32_t vendor_id_pref,
    [[maybe_unused]] uint32_t device_id_pref,
    [[maybe_unused]] memorable_arena_t * restrict arena,
    struct cara_worker_state * restrict worker_state
    )
{

//  const char * name_shared_library;
//#if defined(__linux)
//  name_shared_library = "libvulkan.so.1";
//#elif defined(_WIN32)
//  name_shared_library = "vulkan-1.dll";
//#endif
  VkResult result = volkInitialize();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Vulkan] ERROR: Failed to load volk/vulkan\n" );
    return 1;
  }

  //const shared_lib vulkan_library = load_shared_lib(name_shared_library);
  //if ( !vulkan_library ) {
  //  fprintf( stderr, "[Vulkan] ERROR: Failed to load '%s'\n", name_shared_library );
  //  return 1;
  //}

  //// Load the functions exported from vulkan loader library
  //if ( vf_load_func_exported_from_vulkan_loader_library(vulkan_library) ) {
  //  unload_shared_lib(vulkan_library);
  //  return 1;
  //}


  ////
  //// Load global-level functions
  ////
  //if ( vf_load_global_level_functions() ) {
  //  unload_shared_lib(vulkan_library);
  //  return 1;
  //}


  // Enumerate the instance extensions supported by vulkan implementation
  uint32_t nr_supported_extensions;

  result = vkEnumerateInstanceExtensionProperties( nullptr, &nr_supported_extensions, nullptr );
#ifdef DEBUG_MODE
  fprintf( stdout, "[Vulkan] nr_supported_extensions=%u;(instance extensions) result=%s; "
      "VK_SUCCESS=%d\n", nr_supported_extensions, string_VkResult(result), VK_SUCCESS );
#endif
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Vulkan] Could not get the number of instance extensions. (%d)\n",
        result );
    //unload_shared_lib( vulkan_library );
    return 1;
  }

  VkExtensionProperties * arr_extension_props;
  arr_extension_props = cara_malloc(
      &worker_state->malloc_helper, alignof(VkExtensionProperties),
      sizeof(VkExtensionProperties) * nr_supported_extensions );

  result = vkEnumerateInstanceExtensionProperties(
      nullptr, &nr_supported_extensions, arr_extension_props );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Vulkan] Could not enumerate instance extensions. (%s)\n",
        string_VkResult(result) );
    ma_free( cara_single.mem_arena,
        worker_state->worker_info->idx_self, arr_extension_props );
    //unload_shared_lib( vulkan_library );
    return 1;
  }



  //
  // See if there is any required instance extension that is not supported by the vulkan
  // implementation
  //

  // The very basic instance extensions required
//static const char * const names_of_required_instance_extensions[] = {
//  VK_KHR_SURFACE_EXTENSION_NAME,
//#ifdef CARBONARA_TARGET_PLATFORM_WIN32
//  VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
////#elif defined CARBONARA_TARGET_PLATFORM_XCB
////#elif defined CARBONARA_TARGET_PLATFORM_XLIB
//#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB_OR_XLIB
//  VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
//  VK_KHR_XCB_SURFACE_EXTENSION_NAME,
//  VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
//#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
//  VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
//#endif
//};
  unsigned int nr_required_extensions;
  const char * const * names_basic_instance_extensions =
    SDL_Vulkan_GetInstanceExtensions(&nr_required_extensions);
  for ( uint_fast8_t idx_required_extension = 0;
      idx_required_extension < nr_required_extensions; idx_required_extension++ ) {
    uint_fast8_t is_supported = 0;
    for ( uint_fast8_t idx_extension_properties = 0;
        idx_extension_properties < nr_supported_extensions; idx_extension_properties++ ) {
      if ( strcmp(names_basic_instance_extensions[idx_required_extension],
            arr_extension_props[idx_extension_properties].extensionName) == 0 ) {
        is_supported = 1;
        break;
      }
    }

    if ( !is_supported ) {
      fprintf( stderr, "[Vulkan] The required extension %s is not supported.\n",
          names_basic_instance_extensions[idx_required_extension] );
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_extension_props );
      //unload_shared_lib( vulkan_library );
      return 1;
    }
  }

  // The debugging purpose instance extensions required
  for ( uint_fast8_t idx_required_extension = 0;
      idx_required_extension < _arr_elem_cnt(names_instance_extensions_required);
      idx_required_extension++ ) {
    uint_fast8_t is_supported = 0;
    for ( uint_fast8_t idx_extension_properties = 0;
        idx_extension_properties < nr_supported_extensions; idx_extension_properties++ ) {
      if ( strcmp(names_instance_extensions_required[idx_required_extension],
            arr_extension_props[idx_extension_properties].extensionName) == 0 ) {
        is_supported = 1;
        break;
      }
    }

    if ( !is_supported ) {
      fprintf( stderr, "[Vulkan] The required extension %s is not supported.\n",
          names_instance_extensions_required[idx_required_extension] );
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_extension_props );
      //unload_shared_lib( vulkan_library );
      return 1;
    }
  }

  ma_free( cara_single.mem_arena,
      worker_state->worker_info->idx_self, arr_extension_props );



  //
  // Prepare VkAllocationCallbacks after a clean-up
  //
  auto the_texture = cara_vk.stone_texture;
  auto the_texture2 = cara_vk.sand_texture;
  auto the_size = cara_vk.asset_size;
  memset( &cara_vk, 0, sizeof(struct cara_vk_single) );
  cara_vk.stone_texture = the_texture;
  cara_vk.sand_texture = the_texture2;
  cara_vk.asset_size = the_size;

  //cara_vk.vk_alloc_callbacks.pfnAllocation = cara_malloc_vk;
  //cara_vk.vk_alloc_callbacks.pfnFree = cara_free_vk;
  //cara_vk.vk_alloc_callbacks.pfnInternalAllocation = nullptr;
  //cara_vk.vk_alloc_callbacks.pfnInternalFree = nullptr;
  //cara_vk.vk_alloc_callbacks.pfnReallocation = cara_realloc_vk;
  //cara_vk.vk_alloc_callbacks.pUserData = (void*) &worker_state->malloc_helper;


  //
  // Create a vulkan instance
  //

  // VkApplicationInfo
  const VkApplicationInfo vk_application_info = {
    VK_STRUCTURE_TYPE_APPLICATION_INFO,   // VkStructureType  sType
    nullptr,                              // const void *     pNext
    Carbonara_APP_NAME,                   // const char *     pApplicationName
    VK_MAKE_VERSION( Carbonara_APP_VERSION_MAJOR, Carbonara_APP_VERSION_MINOR, 0 ),  // ui32 appVers
    "carbonara",                          // const char *     pEngineName
    VK_MAKE_VERSION( Carbonara_VERSION_MAJOR, Carbonara_VERSION_MINOR, 0 ),  // ui32 engineVersion
    VK_MAKE_API_VERSION( 0, vulkan_version_major, vulkan_version_minor, 0 )  // uint32_t apiVersion
                                          // The highest version of Vulkan that the application is
                                          //  designed to use
  };

  // VkInstanceCreateInfo
  const VkInstanceCreateInfo vk_instance_create_info = {
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,   // VkStructureType            sType
    nullptr,                                  // const void *               pNext
    0,                                        // VkInstanceCreateFlags      flags
    &vk_application_info,                     // const VkApplicationInfo *  pApplicationInfo
#if defined(ARE_WE_SHIPPING_IT)
    0,                                        // uint32_t                   enabledLayerCount
    nullptr,                                  // const char * const *       ppEnabledLayerNames
#else
    0,                                        // uint32_t                   enabledLayerCount
    nullptr,                                  // const char * const *       ppEnabledLayerNames
    //_arr_elem_cnt(enabled_layer_names),       // uint32_t                   enabledLayerCount
    //enabled_layer_names,                      // const char * const *       ppEnabledLayerNames
#endif
    nr_required_extensions,                   // uint32_t                   enabledExtensionCount
    names_basic_instance_extensions     // const char * const *       ppEnabledExtensionNames
  };

  // vkCreateInstance
  VkInstance vk_instance;
  result = vkCreateInstance(
      &vk_instance_create_info,
      nullptr,
      &vk_instance
      );
  if ( (result != VK_SUCCESS) ||
      (vk_instance == VK_NULL_HANDLE) ) {
    fprintf( stderr, "[Vulkan] Could not create Vulkan instance. (%s)\n", string_VkResult(result) );
    //unload_shared_lib(vulkan_library);
    return 1;
  }



  ////
  //// Load instance-level functions
  ////
  //if ( vf_load_instance_level_functions(vk_instance,
  //      names_basic_instance_extensions, nr_required_extensions) ) {
  //  vkDestroyInstance(vk_instance, &cara_vk.vk_alloc_callbacks);
  //  //unload_shared_lib(vulkan_library);
  //  return 1;
  //}
  volkLoadInstanceOnly(vk_instance);

  // Enumerate physical devices
  uint32_t nr_phys_devices = 0;
  result = vkEnumeratePhysicalDevices( vk_instance, &nr_phys_devices, nullptr );
  if ( (result != VK_SUCCESS) ||
      (nr_phys_devices == 0) ) {
    fprintf( stderr, "[Vulkan] Could not get the number of available physical devices. (%s)\n",
       string_VkResult(result) );
    //vkDestroyInstance(vk_instance, &cara_vk.vk_alloc_callbacks);
    vkDestroyInstance(vk_instance, nullptr);
    //unload_shared_lib(vulkan_library);
    return 1;
  }

  VkPhysicalDevice * const arr_phys_devices = cara_malloc(
      &worker_state->malloc_helper, _alignof_arr_elem(arr_phys_devices),
      _sizeof_arr_elem(arr_phys_devices) * nr_phys_devices );

  result = vkEnumeratePhysicalDevices( vk_instance, &nr_phys_devices, arr_phys_devices );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Vulkan] Could not enumerate physical devices. (%s)\n",
        string_VkResult(result) );
    //vkDestroyInstance( vk_instance, &cara_vk.vk_alloc_callbacks );
    vkDestroyInstance(vk_instance, nullptr);
    //unload_shared_lib( vulkan_library );
    return 1;
  }



  //
  // Find out if there is any device that does not support all the required device extensions
  //
  for ( uint_fast8_t idx_phys_dev = 0;
      idx_phys_dev < nr_phys_devices; idx_phys_dev++ ) {
    if ( arr_phys_devices[idx_phys_dev] == 0 ) {
      continue;
    }

    // Enumerate available device extensions
    result = vkEnumerateDeviceExtensionProperties(
        arr_phys_devices[idx_phys_dev], nullptr, &nr_supported_extensions, nullptr );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Could not get the number of device extensions. (result=%s)\n",
          string_VkResult(result) );
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_phys_devices);
      //vkDestroyInstance(vk_instance, &cara_vk.vk_alloc_callbacks);
      vkDestroyInstance(vk_instance, nullptr);
      //unload_shared_lib(vulkan_library);
      return 1;
    }
#ifdef DEBUG_MODE
    fprintf( stdout, "[Vulkan] nr_supported_extensions=%u;(device extensions) "
        "idx_phys_dev=%d;\n", nr_supported_extensions, idx_phys_dev );
#endif

    arr_extension_props = cara_malloc(
        &worker_state->malloc_helper, alignof(VkExtensionProperties),
        sizeof(VkExtensionProperties) * nr_supported_extensions );

    result = vkEnumerateDeviceExtensionProperties( arr_phys_devices[idx_phys_dev],
        nullptr, &nr_supported_extensions, arr_extension_props );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Could not enumerate device extensions. (%s)\n",
          string_VkResult(result) );
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_extension_props );
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_phys_devices);
      //vkDestroyInstance( vk_instance, &cara_vk.vk_alloc_callbacks );
      vkDestroyInstance(vk_instance, nullptr);
      //unload_shared_lib( vulkan_library );
      return 1;
    }
 

    // Check if there is a required device extension not supported by the device
    nr_required_extensions = _arr_elem_cnt( names_device_extensions_required );
    for ( uint_fast8_t idx_required_extension = 0;
        idx_required_extension < nr_required_extensions; idx_required_extension++ ) {
      uint_fast8_t is_supported = 0;
      for ( uint_fast8_t idx_extension_properties = 0;
          idx_extension_properties < nr_supported_extensions; idx_extension_properties++ ) {
        if ( strcmp(names_device_extensions_required[idx_required_extension],
              arr_extension_props[idx_extension_properties].extensionName) == 0 ) {
          is_supported = 1;
        }
      }
      if ( !is_supported ) {
#ifdef DEBUG_MODE
        fprintf( stdout, "[Vulkan] The required extension %s is not supported by device-%u.\n",
            names_device_extensions_required[idx_required_extension], idx_phys_dev );
#endif
        arr_phys_devices[idx_phys_dev] = 0;
        break;
      }
    }

    ma_free( cara_single.mem_arena,
        worker_state->worker_info->idx_self, arr_extension_props );
  }



  //
  // Check the properties and supported features of each device
  //
  VkPhysicalDeviceProperties2 * arr_device_props;

  arr_device_props = cara_malloc(
      &worker_state->malloc_helper, _alignof_arr_elem(arr_device_props),
      _sizeof_arr_elem(arr_device_props) * nr_phys_devices );

  for ( uint_fast8_t idx_phys_dev = 0;
      idx_phys_dev < nr_phys_devices; idx_phys_dev++ ) {
    if ( arr_phys_devices[idx_phys_dev] == 0 ) {
      continue;
    }
    memset( &arr_device_props[idx_phys_dev], 0, _sizeof_arr_elem(arr_device_props) );
    arr_device_props[idx_phys_dev].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    VkPhysicalDeviceVulkan12Features vk12_features = {0};
    vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12_features.pNext = nullptr;
    VkPhysicalDeviceVulkan13Features vk13_features = {0};
    vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13_features.pNext = &vk12_features;

    // pNext chain includes a VkPhysicalDeviceVulkan12Features structure, then it must not include a
    //  VkPhysicalDeviceTimelineSemaphoreFeatures structure. The features in
    //  VkPhysicalDeviceTimelineSemaphoreFeatures were promoted in Vulkan 1.2 and is also found in
    //  VkPhysicalDeviceVulkan12Features.  To prevent one feature setting something to VK_TRUE and
    //  the other to VK_FALSE, only one struct containing the feature is allowed.
    //VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore_features;
    //timeline_semaphore_features.sType =
    //  VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    //timeline_semaphore_features.pNext = &vk13_features;

    VkPhysicalDeviceFeatures2 device_features;
    memset(&device_features, 0, sizeof(device_features));
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.pNext = &vk13_features;
    // The VkPhysicalDeviceFeatures structure lists additional features that may be supported by
    // the given hardware, but are not required by the core Vulkan specification. Features include
    // items such as geometry and tessellation shaders, depth clamp and bias, multiple viewports,
    // or wide lines. You may wonder why geometry and tessellation shaders are on the list.
    // Graphics hardware has supported these features for many years now. However, don't forget
    // that the Vulkan API is portable and can be supported on many different hardware platforms,
    // not only high-end PCs, but also mobile phones or even dedicated, portable devices, which
    // should be as power efficient as possible. That's why these performance-hungry features are
    // not in the core specification. This allows for some driver flexibility and, more
    // importantly, power efficiency and lower memory consumption.

    // There is one additional thing you should know about the physical device features. Like
    // extensions, they are not enabled by default and can't be used just like that. They must be
    // implicitly enabled during the logical device creation. We can't request all features during
    // this operation, because if there is any feature that is not supported, the logical device
    // creation process will fail. If we are interested in a specific feature, we need to check if
    // it is available and specify it during the creation of a logical device. If the feature is
    // not supported, we can't use such a feature on this device and we need to look for another
    // device that supports it.

    // If we want to enable all features supported by a given physical device, we just need to
    // query for the available features and provide the acquired data during logical device
    // creation.
    vkGetPhysicalDeviceFeatures2( arr_phys_devices[idx_phys_dev], &device_features );
    if ( !device_features.features.samplerAnisotropy ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "samplerAnisotropy"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
    if ( !vk12_features.timelineSemaphore ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "timelineSemaphore"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
    if ( !vk12_features.drawIndirectCount ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "drawIndirectCount"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
    if ( !vk12_features.imagelessFramebuffer ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "imagelessFramebuffer"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
    if ( !vk12_features.separateDepthStencilLayouts ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "separateDepthStencilLayouts"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
//    if ( !vk13_features.synchronization2 ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "synchronization2"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }
//Just using 1.3 now

//    if ( !device_features.features.sampleRateShading ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "sampleRateShading"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }
//    if ( !device_features.features.multiDrawIndirect ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "multiDrawIndirect"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }
//    if ( !device_features.features.drawIndirectFirstInstance ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "drawIndirectFirstInstance"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }
//    if ( !device_features.features.independentBlend ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "independentBlend"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }
//    if ( !device_features.features.imageCubeArray ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "imageCubeArray"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }
//    if ( !device_features.features.depthClamp ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "depthClamp"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }
//    if ( !device_features.features.shaderClipDistance ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "shaderClipDistance"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }

    vkGetPhysicalDeviceProperties2( arr_phys_devices[idx_phys_dev],
        &arr_device_props[idx_phys_dev] );

    // TODO Player might want to choose a specific device name.

    if ( VK_VERSION_MINOR(arr_device_props[idx_phys_dev].properties.apiVersion) < 2 &&
        VK_VERSION_MAJOR(arr_device_props[idx_phys_dev].properties.apiVersion) <= 1 ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s offers Vulkan %u.%u.%u which is not high enough.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          VK_VERSION_MAJOR(arr_device_props[idx_phys_dev].properties.apiVersion),
          VK_VERSION_MINOR(arr_device_props[idx_phys_dev].properties.apiVersion),
          VK_VERSION_PATCH(arr_device_props[idx_phys_dev].properties.apiVersion)
          );
      fprintf( stdout, "[Vulkan] The application needs Vulkan version %u.%u or a higher version."
          "\n", vulkan_version_major, vulkan_version_minor );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
    } else if ( arr_device_props[idx_phys_dev].properties.limits.maxDrawIndirectCount <
        (1<<16) - 1 ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s's maxDrawIndirectCount is small. (%u)\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          arr_device_props[idx_phys_dev].properties.limits.maxDrawIndirectCount );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
    } else if ( arr_device_props[idx_phys_dev].properties.limits.maxDrawIndexedIndexValue <
        (1<<24) - 1 ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s's maxDrawIndexedIndexValue is smaller than 2^24-1. (%u)\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          arr_device_props[idx_phys_dev].properties.limits.maxDrawIndexedIndexValue );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
    }
  }


  //
  // Make a window and a presentation surface
  //
  SDL_Window * window =
    SDL_CreateWindow( Carbonara_APP_NAME, test_width, test_height,
      SDL_WINDOW_RESIZABLE |
      SDL_WINDOW_HIDDEN |
      //SDL_WINDOW_VULKAN |
      0 );
  if ( !window ) {
    fprintf( stderr, "[Vulkan] Failed to create a window.\n" );
    goto error_after_window_early;
  }

  VkSurfaceKHR presentation_surface;
  if ( !SDL_Vulkan_CreateSurface( window,
        vk_instance,
        nullptr,
        &presentation_surface) ) {
    fprintf( stderr, "[Vulkan] Failed to create a vulkan surface.\n" );
    SDL_DestroyWindow( window );
error_after_window_early:
    //vkDestroyInstance(vk_instance, &cara_vk.vk_alloc_callbacks);
    vkDestroyInstance(vk_instance, nullptr);
    ma_free( cara_single.mem_arena, worker_state->worker_info->idx_self, arr_device_props );
    ma_free( cara_single.mem_arena,
        worker_state->worker_info->idx_self, arr_phys_devices );
    //unload_shared_lib( vulkan_library );
    return 1;
  }

  /* Rely on SDL instead of creating a window and a surface yourself.
  // tODO Take care of the lifetime of window
  struct ms_window * window;
  // Lock the sub_arena
  {
    uint8_t old_value = 0;
    while ( !atomic_compare_exchange_weak_explicit(&cara_single.mem_arena->head->lock,
          &old_value, -7, memory_order_acq_rel, memory_order_acquire) ) {
      old_value = 0;
    }
  }
  window = (void*) ma_malloc( cara_single.mem_arena->head,
      _get_nr_units_to_cover(sizeof(struct ms_window), CACHELINE_LEN) );

  // Unlock the sub_arena
  atomic_store_explicit(&cara_single.mem_arena->head->lock, 0, memory_order_release);

  //window->flags = window_state_fullscreen;
  if ( create_window(window) ) {
    goto error_after_window_early;
  }

  VkSurfaceKHR presentation_surface;
  result = create_presentation_surface(vk_instance, window, &presentation_surface);
  if ( result != VK_SUCCESS ) {
error_after_window_early:
    vkDestroyInstance(vk_instance, &cara_vk.vulkan_alloc_callbacks);
    destroy_window(window);
    ma_free(cara_single.mem_arena, worker_idx, window);
    ma_free(cara_single.mem_arena, worker_idx, arr_device_props);
    ma_free(cara_single.mem_arena, worker_idx, arr_phys_devices);
    unload_shared_lib(vulkan_library);
    return 1;
  }
  */



  constexpr uint_fast8_t dedicated_compute = 1 << 0;  // VK_QUEUE_COMPUTE_BIT without graphics
  constexpr uint_fast8_t dedicated_transfer = 1 << 1;  // VK_QUEUE_TRANSFER_BIT without graphics
  constexpr uint_fast8_t very_dedicated_transfer = 1 << 2;  ///< VK_QUEUE_TRANSFER_BIT without
                                                            ///<  graphics and compute
  constexpr VkQueueFlags required_q_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;

  uint_fast8_t dedicated_qfam_availability[nr_phys_devices];
  for ( unsigned int i = 0; i < nr_phys_devices; i++ ) {
    dedicated_qfam_availability[i] = 0;
  }

  //
  // Check the properties of queue families
  //
  uint32_t nr_q_fams[nr_phys_devices];
  uint_fast8_t nr_discrete_gpus = 0;
  for ( uint_fast8_t idx_phys_dev = 0;
      idx_phys_dev < nr_phys_devices; idx_phys_dev++ ) {
    if ( arr_phys_devices[idx_phys_dev] == 0 ) {
      continue;
    }

    VkQueueFamilyProperties2 * arr_qfam_props = nullptr;

    // Obtain the number of queue families available on the device
    vkGetPhysicalDeviceQueueFamilyProperties2( arr_phys_devices[idx_phys_dev],
        &nr_q_fams[idx_phys_dev], nullptr );
#ifdef DEBUG_MODE
    fprintf( stdout, "[Vulkan] nr_q_fams=%u; (%s)\n", nr_q_fams[idx_phys_dev],
        arr_device_props[idx_phys_dev].properties.deviceName );
#endif
    if ( nr_q_fams[idx_phys_dev] == 0 ) {
      fprintf( stdout, "[Vulkan] Could not get the number of queue families of '%s'.\n",
          arr_device_props[idx_phys_dev].properties.deviceName );
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

    arr_qfam_props = cara_malloc(
        &worker_state->malloc_helper, _alignof_arr_elem(arr_qfam_props),
        _sizeof_arr_elem(arr_qfam_props) * nr_q_fams[idx_phys_dev] );
    for ( unsigned int qfam_idx = 0; qfam_idx < nr_q_fams[idx_phys_dev]; qfam_idx++ ) {
      memset( &arr_qfam_props[qfam_idx], 0, _sizeof_arr_elem(arr_qfam_props) );
      arr_qfam_props[qfam_idx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    }

    // Graphics: For creating graphics pipelines and drawing
    // Compute: For creating compute pipelines and dispatching compute shaders
    // Transfer: Used for very fast memory-copying operations
    // Sparse: Allows for additional memory management features
    vkGetPhysicalDeviceQueueFamilyProperties2( arr_phys_devices[idx_phys_dev],
        &nr_q_fams[idx_phys_dev], arr_qfam_props );
    if ( nr_q_fams[idx_phys_dev] == 0 ) {
      fprintf( stdout,
          "[Vulkan] Could not acquire the properties of the queue families of '%s'.\n",
          arr_device_props[idx_phys_dev].properties.deviceName );
      arr_phys_devices[idx_phys_dev] = 0;
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_qfam_props );
      continue;
    }


    // arr_queue_families[idx_phys_dev].timestampValidBits
    //
    // timestampValidBits is the unsigned integer count of meaningful bits in the timestamps
    // written via vkCmdWriteTimestamp2 or vkCmdWriteTimestamp. The valid range for the count is 36
    // to 64 bits, or a value of 0, indicating no support for timestamps. Bits outside the valid
    // range are guaranteed to be zeros.


    // See if this physical device has got all the capabilities required
    VkQueueFlags q_flags_not_found_yet = required_q_flags;
    for ( uint_fast8_t idx_q_fam_props = 0;
        idx_q_fam_props < nr_q_fams[idx_phys_dev]; idx_q_fam_props++ ) {
      VkQueueFlags q_flags = arr_qfam_props[idx_q_fam_props].queueFamilyProperties.queueFlags;

      // We need a queue family that supports presentation to the specific surface created via SDL
      // for VK_QUEUE_GRAPHICS_BIT
      VkBool32 supports_presentation;
      vkGetPhysicalDeviceSurfaceSupportKHR( arr_phys_devices[idx_phys_dev],
          idx_q_fam_props, presentation_surface, &supports_presentation );
      if ( VK_QUEUE_GRAPHICS_BIT &
          arr_qfam_props[idx_q_fam_props].queueFamilyProperties.queueFlags &&
          supports_presentation != VK_TRUE ) {
        q_flags = q_flags & ~VK_QUEUE_GRAPHICS_BIT;
      }

      // Mark that we have or have not found a queue family capable to help run the program.
      if ( required_q_flags & q_flags ) {
        q_flags_not_found_yet = q_flags_not_found_yet & ~q_flags;
      }

      if ( q_flags & VK_QUEUE_COMPUTE_BIT && !(q_flags & VK_QUEUE_GRAPHICS_BIT) ) {
        const typeof(dedicated_qfam_availability[0]) new_bit = dedicated_compute;
        dedicated_qfam_availability[idx_phys_dev] |= new_bit;
      }

      if ( q_flags & VK_QUEUE_TRANSFER_BIT && !(q_flags & VK_QUEUE_GRAPHICS_BIT) ) {
        const typeof(dedicated_qfam_availability[0]) new_bit = q_flags & VK_QUEUE_COMPUTE_BIT ?
          dedicated_transfer : very_dedicated_transfer;
        dedicated_qfam_availability[idx_phys_dev] |= new_bit;
      }
    }
    // It hasn't got every required flag?
    if ( q_flags_not_found_yet ) {
      // Mark as unusable if we have not found all the required queue family capabilities.
      arr_phys_devices[idx_phys_dev] = 0;
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_qfam_props );
      continue;
    }

    // arr_queue_families[idx_phys_dev].minImageTransferGranularity
    //
    // (Ax, Ay, Az) where Ax, Ay, and Az are all integer powers of two. In this case the following
    // restrictions apply to all image transfer operations: x, y, and z of a VkOffset3D parameter
    // must be integer multiples of Ax, Ay, and Az, respectively.
    //
    // `width` of a VkExtent3D parameter must be an integer multiple of Ax, or else x + `width`
    // must equal the `width` of the image subresource corresponding to the parameter.
    // `height` of a VkExtent3D parameter must be an integer multiple of Ay, or else y + `height`
    // must equal the `height` of the image subresource corresponding to the parameter.
    // `depth` of a VkExtent3D parameter must be an integer multiple of Az, or else z + `depth`
    // must equal the `depth` of the image subresource corresponding to the parameter.
    //
    // If the format of the image corresponding to the parameters is one of the block-compressed
    // formats then for the purposes of the above calculations the granularity must be scaled up by
    // the compressed texel block dimensions.
    //
    // Queues supporting graphics and/or compute operations must report (1,1,1) in
    // minImageTransferGranularity, meaning that there are no additional restrictions on the
    // granularity of image transfer operations for these queues. Other queues supporting image
    // transfer operations are only required to support whole mip level transfers, thus
    // minImageTransferGranularity for queues belonging to such queue families may be (0,0,0).

    if ( arr_device_props[idx_phys_dev].properties.deviceType ==
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
      nr_discrete_gpus++;
    }

    ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, arr_qfam_props);
  }



  //
  // Pick a physical device to use
  //
  uint_fast8_t idx_phys_deviced_picked = UINT_FAST8_MAX;
  VkDeviceQueueCreateInfo * arr_q_create_info = nullptr;
  VkQueueFamilyProperties2 * arr_qfam_props = nullptr;
  //uint32_t max_q_count = 0;  // We probably don't need to care about queueCount
  uint_fast8_t nr_q_fams_picked = 0;
  const uint_fast8_t prefer_discrete_gpus = nr_discrete_gpus;
  uint_fast8_t idx_transfer_q = ~0, idx_graphics_q = ~0, idx_compute_q = ~0;
  for ( uint_fast8_t idx_phys_dev = 0;
      idx_phys_dev < nr_phys_devices; idx_phys_dev++ ) {

    // Marked as unusable?
    if ( arr_phys_devices[idx_phys_dev] == 0 ) {
      continue;
    }

    // We need a discrete gpu?
    if ( prefer_discrete_gpus &&
        arr_device_props[idx_phys_dev].properties.deviceType !=
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
      continue;
    }

    _carbonara_malloc_array( &worker_state->malloc_helper,
        &arr_qfam_props, nr_q_fams[idx_phys_dev] );
    for ( unsigned int qfam_idx = 0; qfam_idx < nr_q_fams[idx_phys_dev]; qfam_idx++ ) {
      memset( &arr_qfam_props[qfam_idx], 0, _sizeof_arr_elem(arr_qfam_props) );
      arr_qfam_props[qfam_idx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2( arr_phys_devices[idx_phys_dev],
        &nr_q_fams[idx_phys_dev], arr_qfam_props );
    if ( nr_q_fams[idx_phys_dev] == 0 ) {
      fprintf( stdout, "[Vulkan] Could not acquire the properties of the queue families of '%s'.\n",
          arr_device_props[idx_phys_dev].properties.deviceName );
      // arr_phys_devices[idx_phys_dev] = 0;
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_qfam_props );
      continue;
    }

    // Populate arr_q_create_info
    nr_q_fams_picked = 0;
    //VkQueueFlags flags_matched = 0;
    VkQueueFlags q_flags_not_found_yet = required_q_flags;
    for ( uint_fast8_t idx_q_fam_props = 0;
        idx_q_fam_props < nr_q_fams[idx_phys_dev]; idx_q_fam_props++ ) {
      VkQueueFlags q_flags = arr_qfam_props[idx_q_fam_props].queueFamilyProperties.queueFlags;
      VkBool32 supports_presentation;
      vkGetPhysicalDeviceSurfaceSupportKHR(arr_phys_devices[idx_phys_dev],
          idx_q_fam_props, presentation_surface, &supports_presentation);
      // We need a queue family that supports presentation to the specific surface created via SDL
      // for VK_QUEUE_GRAPHICS_BIT
      if ( VK_QUEUE_GRAPHICS_BIT &
          arr_qfam_props[idx_q_fam_props].queueFamilyProperties.queueFlags &&
          supports_presentation != VK_TRUE ) {
        q_flags = q_flags & ~VK_QUEUE_GRAPHICS_BIT;
      }

      if ( q_flags & VK_QUEUE_COMPUTE_BIT && q_flags & VK_QUEUE_GRAPHICS_BIT ) {
        if ( dedicated_qfam_availability[idx_phys_dev] & dedicated_compute ) {
          q_flags &= ~VK_QUEUE_COMPUTE_BIT;
        }
      }

      if ( q_flags & VK_QUEUE_TRANSFER_BIT ) {
        if ( dedicated_qfam_availability[idx_phys_dev] & very_dedicated_transfer ) {
          if ( q_flags & VK_QUEUE_GRAPHICS_BIT || q_flags & VK_QUEUE_COMPUTE_BIT ) {
            q_flags &= ~VK_QUEUE_TRANSFER_BIT;
          }
        } else if ( dedicated_qfam_availability[idx_phys_dev] & dedicated_transfer ) {
          if ( q_flags & VK_QUEUE_GRAPHICS_BIT ) {
            q_flags &= ~VK_QUEUE_TRANSFER_BIT;
          }
        }
      }

      if ( q_flags_not_found_yet & q_flags ) {
        nr_q_fams_picked++;
        //if ( max_q_count < arr_queue_families[idx_q_fam_props].queueCount ) {
        //  max_q_count = arr_queue_families[idx_q_fam_props].queueCount;
        //}
        q_flags_not_found_yet = q_flags_not_found_yet & ~q_flags;
      }
      if ( !q_flags_not_found_yet ) {
        break;
      }
    }


    //
    // Start preparing an array of VkDeviceQueueCreateInfo
    //

    // FIXME No need to do this kind of unsafe manual access on memorable_arena_t.

    // Lock the sub_arena before getting memory allocation
    uint8_t old_value = 0;
    while ( !atomic_compare_exchange_weak_explicit(&cara_single.mem_arena->head->lock,
          &old_value, -7, memory_order_acq_rel, memory_order_acquire) ) {
      old_value = 0;
    }

    constexpr size_t nr_queues_from_each_qfam = 1;
    static_assert( alignof(VkDeviceQueueCreateInfo) >= alignof(float), "should be aligned." );
    arr_q_create_info = (void*) ma_malloc(
        cara_single.mem_arena->head,
        _get_nr_units_to_cover(
          sizeof(VkDeviceQueueCreateInfo) * nr_q_fams_picked + sizeof(float) *
          nr_queues_from_each_qfam,
          CACHELINE_LEN ),
        nullptr, nullptr
       );

    // Unlock the sub_arena after allocation
    atomic_store_explicit(&cara_single.mem_arena->head->lock, 0, memory_order_release);

    // `pQueuePriorities` is a pointer to an array of `queueCount` normalized floating-point values
    // , specifying priorities of work that will be submitted to each created queue.
    // See Queue Priority for more information. (follow the link below to learn about the priority)
    // https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#devsandqueues-priority
    float * restrict const array_of_1s = (void*)( arr_q_create_info + nr_q_fams_picked );
    for ( uint_fast8_t idx = 0; idx < nr_queues_from_each_qfam; idx++ ) array_of_1s[idx] = 1.0f;

    q_flags_not_found_yet = required_q_flags;
    for ( uint_fast8_t idx_q_fam_props = 0, idx_q_create_info = 0;
        idx_q_fam_props < nr_q_fams[idx_phys_dev]; idx_q_fam_props++ ) {
      VkQueueFlags q_flags = arr_qfam_props[idx_q_fam_props].queueFamilyProperties.queueFlags;

      // We need a queue family that supports presentation to the specific surface created via SDL
      // for VK_QUEUE_GRAPHICS_BIT
      VkBool32 supports_presentation;
      vkGetPhysicalDeviceSurfaceSupportKHR(arr_phys_devices[idx_phys_dev],
          idx_q_fam_props, presentation_surface, &supports_presentation);
      if ( VK_QUEUE_GRAPHICS_BIT &
          arr_qfam_props[idx_q_fam_props].queueFamilyProperties.queueFlags &&
          supports_presentation != VK_TRUE ) {
        q_flags = q_flags & ~VK_QUEUE_GRAPHICS_BIT;
      }

      if ( q_flags & VK_QUEUE_COMPUTE_BIT && q_flags & VK_QUEUE_GRAPHICS_BIT ) {
        if ( dedicated_qfam_availability[idx_phys_dev] & dedicated_compute ) {
          q_flags &= ~VK_QUEUE_COMPUTE_BIT;
        }
      }

      if ( q_flags & VK_QUEUE_TRANSFER_BIT ) {
        if ( dedicated_qfam_availability[idx_phys_dev] & very_dedicated_transfer ) {
          if ( q_flags & VK_QUEUE_GRAPHICS_BIT || q_flags & VK_QUEUE_COMPUTE_BIT ) {
            q_flags &= ~VK_QUEUE_TRANSFER_BIT;
          }
        } else if ( dedicated_qfam_availability[idx_phys_dev] & dedicated_transfer ) {
          if ( q_flags & VK_QUEUE_GRAPHICS_BIT ) {
            q_flags &= ~VK_QUEUE_TRANSFER_BIT;
          }
        }
      }

      if ( q_flags_not_found_yet & q_flags ) {
        if ( q_flags & VK_QUEUE_GRAPHICS_BIT ) {
          idx_graphics_q = idx_q_create_info;
        }
        if ( q_flags & VK_QUEUE_COMPUTE_BIT ) {
          idx_compute_q = idx_q_create_info;
        }
        if ( q_flags & VK_QUEUE_TRANSFER_BIT ) {
          idx_transfer_q = idx_q_create_info;
        }
        arr_q_create_info[idx_q_create_info].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        arr_q_create_info[idx_q_create_info].pNext = 0;
        arr_q_create_info[idx_q_create_info].flags = 0;
        arr_q_create_info[idx_q_create_info].queueFamilyIndex = idx_q_fam_props;
        arr_q_create_info[idx_q_create_info].queueCount = nr_queues_from_each_qfam;
          //arr_queue_families[idx_q_fam_props].queueCount;
        arr_q_create_info[idx_q_create_info].pQueuePriorities = array_of_1s;
        idx_q_create_info++;

        q_flags_not_found_yet = q_flags_not_found_yet & ~q_flags;
      }
      if ( !q_flags_not_found_yet ) {
        break;
      }

    }
    assume( q_flags_not_found_yet == 0 );

    // XXX Other candidates are ignored?
    idx_phys_deviced_picked = idx_phys_dev;
    break;
  }

  if ( idx_phys_deviced_picked == UINT_FAST8_MAX ) {
    fprintf( stderr, "[Vulkan] Could not find an appropriate graphics processing device.\n" );
    goto error_after_presentation_surface;
  }



  //
  // Make a logical device
  //

  // The logical device allows us to perform almost all the work typically done in rendering
  // applications, such as creating images and buffers, setting the pipeline state, or loading
  // shaders. The most important ability it gives us is recording commands (such as issuing draw
  // calls or dispatching computational work) and submitting them to queues, where they are
  // executed and processed by the given hardware. After such execution, we acquire the results of
  // the submitted operations. These can be a set of values calculated by compute shaders, or
  // other data (not necessarily an image) generated by draw calls. All this is performed on a
  // logical device.
  VkPhysicalDeviceFeatures2 device_features = {0};
  device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

  VkPhysicalDeviceVulkan12Features vk12_features = {0};
  vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vk12_features.pNext = nullptr;
  vk12_features.timelineSemaphore = VK_TRUE;
  vk12_features.drawIndirectCount = VK_TRUE;
  vk12_features.imagelessFramebuffer = VK_TRUE;
  vk12_features.separateDepthStencilLayouts = VK_TRUE;
  VkPhysicalDeviceVulkan13Features vk13_features = {0};
  vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vk13_features.pNext = &vk12_features;
  vk13_features.synchronization2 = VK_TRUE;
  vk13_features.robustImageAccess = VK_FALSE;   // I make it clear that I do not use it.
                                                // I redundantly zero it once again.

  // pNext chain includes a VkPhysicalDeviceVulkan12Features structure, then it must not include a
  //  VkPhysicalDeviceTimelineSemaphoreFeatures structure. The features in
  //  VkPhysicalDeviceTimelineSemaphoreFeatures were promoted in Vulkan 1.2 and is also found in
  //  VkPhysicalDeviceVulkan12Features.  To prevent one feature setting something to VK_TRUE and the
  //  other to VK_FALSE, only one struct containing the feature is allowed.
  //VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore_features = {0};
  //timeline_semaphore_features.sType =
  //  VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
  //timeline_semaphore_features.pNext = &vk13_features;
  //timeline_semaphore_features.timelineSemaphore = VK_TRUE;

  device_features.pNext = &vk13_features;
  //device_features.features.multiDrawIndirect = VK_TRUE;
  device_features.features.robustBufferAccess = VK_FALSE;  // I make it clear that I do not use it.
                                                           // I redundantly zero it once again.
  device_features.features.samplerAnisotropy = VK_TRUE;

  const VkDeviceCreateInfo device_create_info = {
    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,   // VkStructureType                  sType
    &device_features,                       // const void *                     pNext
    0,                                      // VkDeviceCreateFlags              flags
    nr_q_fams_picked,                   // uint32_t                         queueCreateInfoCount
    arr_q_create_info,                  // const VkDeviceQueueCreateInfo *  pQueueCreateInfos
    0,                                  // uint32_t                   enabledLayerCount
    nullptr,                            // const char * const *       ppEnabledLayerNames
    _arr_elem_cnt(names_device_extensions_required),  // uint32_t           enabledExtensionCount
    names_device_extensions_required,   // const char * const *             ppEnabledExtensionNames
    nullptr                             // const VkPhysicalDeviceFeatures * pEnabledFeatures
  };
  VkDevice new_log_dev;

  result = vkCreateDevice( arr_phys_devices[idx_phys_deviced_picked],
      &device_create_info, nullptr, &new_log_dev );
      //&device_create_info, &cara_vk.vk_alloc_callbacks, &new_log_dev );
  if ( result != VK_SUCCESS || new_log_dev == VK_NULL_HANDLE ) {
    fprintf( stderr, "[Vulkan] Could not create a logical device. (%s)\n",
        string_VkResult(result) );
    goto error_after_arr_q_create_info;
  }

  // For applications that use just one VkDevice object, load device-related Vulkan entrypoints
  //  directly from the driver with this function:
  volkLoadDevice( new_log_dev );


  uint32_t nr_swapchain_imgs = 0;  // Assign 0 to the variable before a possible jmp
  ////
  //// Load device-level functions
  ////
  //if ( vf_load_device_level_functions(new_log_dev,
  //      names_device_extensions_required,
  //      _arr_elem_cnt(names_device_extensions_required)) ) {
  //  fprintf( stderr, "[Vulkan] Could not load device-level functions.\n" );
  //  goto error_after_logical_device;
  //}


  //
  // Acquire the handles of device queues
  //
  for ( uint_fast8_t idx_q_create_info = 0;
      idx_q_create_info < nr_q_fams_picked; idx_q_create_info++ ) {
    const VkDeviceQueueCreateInfo * const q_create_info = arr_q_create_info + idx_q_create_info;

    VkQueue new_queue;
    vkGetDeviceQueue( new_log_dev, q_create_info->queueFamilyIndex, 0, &new_queue );
    if ( idx_q_create_info == idx_graphics_q ) {
      cara_vk.graphics_q = new_queue;
      cara_vk.graphics_q_fam_idx = q_create_info->queueFamilyIndex;
    }
    if ( idx_q_create_info == idx_transfer_q ) {
      cara_vk.transfer_q = new_queue;
      cara_vk.trans_q_fam_idx = q_create_info->queueFamilyIndex;
    }
    if ( idx_q_create_info == idx_compute_q ) {
      cara_vk.compute_q = new_queue;
      cara_vk.compute_q_fam_idx = q_create_info->queueFamilyIndex;
    }
  }


  //
  // Prepare swapchain images
  //

  // Choose a present mode
  unsigned int nr_supported_present_modes;
  VkPresentModeKHR desired_present_modes[] = {
    VK_PRESENT_MODE_MAILBOX_KHR,
    VK_PRESENT_MODE_FIFO_KHR,
    VK_PRESENT_MODE_FIFO_RELAXED_KHR,
    VK_PRESENT_MODE_IMMEDIATE_KHR,
  };
  result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      arr_phys_devices[idx_phys_deviced_picked],
      presentation_surface,
      &nr_supported_present_modes,
      nullptr );
  if ( result != VK_SUCCESS || 0 == nr_supported_present_modes ) {
    fprintf( stderr, "[Vulkan] vkGetPhysicalDeviceSurfacePresentModesKHR failed. " );
    if ( nr_supported_present_modes == 0 && result != VK_SUCCESS ) {
      fprintf( stderr, "(nr_supported_present_modes=%u)\n", nr_supported_present_modes );
    } else {
      fprintf( stderr, "(result=%s)\n", string_VkResult(result) );
    }
    goto error_after_logical_device;
  }

  VkPresentModeKHR * arr_available_present_modes;
  _carbonara_malloc_array( &worker_state->malloc_helper,
      &arr_available_present_modes, nr_supported_present_modes );

  result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      arr_phys_devices[idx_phys_deviced_picked],
      presentation_surface,
      &nr_supported_present_modes,
      arr_available_present_modes );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Vulkan] vkGetPhysicalDeviceSurfacePresentModesKHR failed. "
        "(result=%s)\n", string_VkResult(result) );
    goto error_after_logical_device;
  }

  VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
  for ( unsigned int priority_idx = 0;
      priority_idx < _arr_elem_cnt(desired_present_modes); priority_idx++ ) {
    for ( unsigned int idx_supported_mode = 0;
        idx_supported_mode < nr_supported_present_modes; idx_supported_mode++ ) {
      if ( arr_available_present_modes[idx_supported_mode] ==
          desired_present_modes[priority_idx] ) {
        chosen_present_mode = desired_present_modes[priority_idx];
        break;
      }
    }
    // End the nested loop
    if ( chosen_present_mode != VK_PRESENT_MODE_MAX_ENUM_KHR ) {
      break;
    }
  }

  ma_free( cara_single.mem_arena,
      worker_state->worker_info->idx_self, arr_available_present_modes );

  if ( chosen_present_mode == VK_PRESENT_MODE_MAX_ENUM_KHR ) {
    fprintf( stderr,
        "[Vulkan] The vulkan instance on this system didn't support any of these:\n" );
    fprintf( stdout,
        "- VK_PRESENT_MODE %u\n"
        "- VK_PRESENT_MODE %u\n"
        "- VK_PRESENT_MODE %u\n"
        "- VK_PRESENT_MODE %u\n",
        desired_present_modes[0],
        desired_present_modes[1],
        desired_present_modes[2],
        desired_present_modes[3]
        );
    goto error_after_logical_device;
  }


  // NOTE Do we need another image?
  // Determine the number and size of swapchain images
  nr_swapchain_imgs = 2;
  VkSurfaceCapabilitiesKHR surface_capabilities;
  result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      arr_phys_devices[idx_phys_deviced_picked],
      presentation_surface,
      &surface_capabilities );

  if ( surface_capabilities.maxImageCount > 0 &&
      nr_swapchain_imgs > surface_capabilities.maxImageCount ) {
    nr_swapchain_imgs = surface_capabilities.maxImageCount;
  } else if ( nr_swapchain_imgs < surface_capabilities.minImageCount ) {
    nr_swapchain_imgs = surface_capabilities.minImageCount;
  }

  VkExtent2D image_size = {
    .width = test_width,
    .height = test_height,
  };
  if ( surface_capabilities.currentExtent.width == 0xffffffff ) {
    if ( image_size.width < surface_capabilities.minImageExtent.width ) {
      image_size.width = surface_capabilities.minImageExtent.width;
    } else if ( image_size.width > surface_capabilities.maxImageExtent.width ) {
      image_size.width = surface_capabilities.maxImageExtent.width;
    }
  }
  if ( surface_capabilities.currentExtent.height == 0xffffffff ) {
    if ( image_size.height < surface_capabilities.minImageExtent.height ) {
      image_size.height = surface_capabilities.minImageExtent.height;
    } else if ( image_size.height > surface_capabilities.maxImageExtent.height ) {
      image_size.height = surface_capabilities.maxImageExtent.height;
    }
  }
  if ( surface_capabilities.currentExtent.width != 0xffffffff ) {
    image_size.width = surface_capabilities.currentExtent.width;
    image_size.height = surface_capabilities.currentExtent.height;
  }

#ifdef DEBUG_MODE
  fprintf( stdout, "[Vulkan] image_size = %u x %u\n", image_size.width, image_size.height );
  fprintf( stdout, "[Vulkan] supportedUsageFlags = %#08x\n",
      surface_capabilities.supportedUsageFlags );
#endif


  // Determine the image format
  uint32_t nr_supported_surface_formats;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      arr_phys_devices[idx_phys_deviced_picked], presentation_surface,
      &nr_supported_surface_formats, nullptr );
  if( result != VK_SUCCESS || nr_supported_surface_formats == 0 ) {
    fprintf( stderr, "[Vulkan] Failed to get the number of supported surface formats. " );
    if ( nr_supported_surface_formats == 0 && result != VK_SUCCESS ) {
      fprintf( stderr, "(nr_supported_surface_formats=%u)\n", nr_supported_surface_formats );
    } else {
      fprintf( stderr, "(result=%s)\n", string_VkResult(result) );
    }
    goto error_after_logical_device;
  }

  VkSurfaceFormatKHR * arr_supported_surface_formats;
  _carbonara_malloc_array( &worker_state->malloc_helper,
      &arr_supported_surface_formats, nr_supported_surface_formats );

  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      arr_phys_devices[idx_phys_deviced_picked], presentation_surface,
      &nr_supported_surface_formats, arr_supported_surface_formats );
  if( result != VK_SUCCESS || nr_supported_surface_formats == 0 ) {
    fprintf( stderr, "[Vulkan] Could not enumerate supported surface formats.(%s)\n", string_VkResult(result) );
    ma_free( cara_single.mem_arena,
        worker_state->worker_info->idx_self, arr_supported_surface_formats );
    goto error_after_logical_device;
  }

#ifdef DEBUG_MODE
  for ( uint_fast8_t idx_supported_format = 0;
      idx_supported_format < nr_supported_surface_formats; idx_supported_format++ ) {
    VkSurfaceFormatKHR * surface_format = &arr_supported_surface_formats[idx_supported_format];
    fprintf( stdout, "[Vulkan] [%u] colorSpace = %#08x; format = %#08x;\n",
        idx_supported_format, surface_format->colorSpace, surface_format->format );
  }
#endif


  constexpr VkFormat desired_img_formats_for_present[] = {
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_R8G8B8_SRGB,
    VK_FORMAT_B8G8R8_SRGB,
  };
  constexpr VkColorSpaceKHR desired_color_spaces[] = {
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
  };
  VkFormat present_img_format = desired_img_formats_for_present[0];
  VkColorSpaceKHR color_space = desired_color_spaces[0];
  if ( nr_supported_surface_formats != 1 ||
      arr_supported_surface_formats[0].format != VK_FORMAT_UNDEFINED ) {
    uint_fast8_t idx_supported_format = 0;
    for ( uint_fast8_t idx_pair = 0;
        idx_pair < _arr_elem_cnt(desired_img_formats_for_present); idx_pair++ ) {
      for ( idx_supported_format = 0;
          idx_supported_format < nr_supported_surface_formats; idx_supported_format++ ) {
        VkSurfaceFormatKHR * surface_format =
          arr_supported_surface_formats + idx_supported_format;
        if ( surface_format->format == desired_img_formats_for_present[idx_pair] &&
            surface_format->colorSpace == desired_color_spaces[idx_pair] ) {
          present_img_format = surface_format->format;
          color_space = surface_format->colorSpace;
          // End the nested loop
          idx_supported_format = UINT_FAST8_MAX;
          break;
        }
        // End the nested loop
        if ( idx_supported_format == UINT_FAST8_MAX ) {
          break;
        }
      }
      // End the nested loop
      if ( idx_supported_format == UINT_FAST8_MAX ) {
        break;
      }
    }

    if ( idx_supported_format != UINT_FAST8_MAX ) {
      fprintf( stderr,
          "[Vulkan] Could not find a compatible surface format and colorspace supported "
          "by the newly created presentation surface on the selected GPU and screen.\n" );
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_supported_surface_formats );
      goto error_after_logical_device;
    }
  }
  ma_free( cara_single.mem_arena,
      worker_state->worker_info->idx_self, arr_supported_surface_formats );

#ifdef DEBUG_MODE
  fprintf( stdout, "[Vulkan] Going with {colorSpace = %#08x + format = %#08x}\n",
      color_space, present_img_format );
#endif


  VkSwapchainKHR swapchain, old_swapchain;
  old_swapchain = VK_NULL_HANDLE;
  //if ( old_swapchain != VK_NULL_HANDLE ) {
  //  vkDestroySwapchainKHR( new_log_dev, old_swapchain, &cara_vk.vulkan_alloc_callbacks );
  //  old_swapchain = VK_NULL_HANDLE;
  //}

  VkSwapchainCreateInfoKHR swapchain_create_info;
  swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_create_info.pNext = nullptr;
  swapchain_create_info.flags = 0;
  swapchain_create_info.surface = presentation_surface;
  swapchain_create_info.minImageCount = nr_swapchain_imgs;
  swapchain_create_info.imageFormat = present_img_format;
  swapchain_create_info.imageColorSpace = color_space;
  swapchain_create_info.imageExtent = image_size;
  swapchain_create_info.imageArrayLayers = 1;
  swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchain_create_info.queueFamilyIndexCount = 0;
  swapchain_create_info.pQueueFamilyIndices = nullptr;
  swapchain_create_info.preTransform = surface_capabilities.currentTransform;
  swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = chosen_present_mode;
  swapchain_create_info.clipped = VK_TRUE;  // Applications should set this value to VK_TRUE if
                                            //  they do not expect to read back the content of
                                            //  presentable images before presenting them or after
                                            //  reacquiring them, and if their fragment shaders do
                                            //  not have any side effects that require them to run
                                            //  for all pixels in the presentable image.
  swapchain_create_info.oldSwapchain = old_swapchain;

  result = vkCreateSwapchainKHR(
      new_log_dev,
      &swapchain_create_info,
      nullptr,
      //&cara_vk.vk_alloc_callbacks,
      &swapchain );
  if ( result != VK_SUCCESS || swapchain == VK_NULL_HANDLE ) {
    fprintf( stderr, "[Vulkan] Could not create a swapchain.\n" );
    goto error_after_logical_device;
  }
  cara_vk.color_att_fmt = present_img_format;


  result = vkGetSwapchainImagesKHR( new_log_dev, swapchain, &nr_swapchain_imgs, nullptr );
  if ( result != VK_SUCCESS || nr_swapchain_imgs == 0 ) {
    fprintf( stderr, "[Vulkan] Could not get the number of swapchain images.\n" );
    goto error_after_logical_device;
  }
#ifdef DEBUG_MODE
  fprintf( stdout,
      "[Vulkan] The number of vulkan images in the swapchain is %u.\n",
      nr_swapchain_imgs );
#endif

  VkImage * arr_swapchain_images;
  _carbonara_malloc_array( &worker_state->malloc_helper, &arr_swapchain_images, nr_swapchain_imgs );

  result = vkGetSwapchainImagesKHR( new_log_dev, swapchain, &nr_swapchain_imgs, arr_swapchain_images );
  if ( result != VK_SUCCESS && result != VK_INCOMPLETE ) {
    fprintf( stderr, "[Vulkan] Could not enumerate swapchain images.\n" );
    goto error_after_swapchain_images;
  }



  //
  // Prepare command buffers
  //

  // Prepare graphics command buffers
  cara_vk.arr_frame_infos[0].arr_cmd_pools =
    cara_malloc( &worker_state->malloc_helper,
      alignof(VkCommandPool),
      cara_max_nr_fif * (
        sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
        sizeof(VkCommandBuffer) *
        (cara_max_nr_subpasses * cara_single.proc_map->nr_processors) )
      );
  memset( cara_vk.arr_frame_infos[0].arr_cmd_pools,
      0,
      cara_max_nr_fif * (
        sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
        sizeof(VkCommandBuffer) *
        (cara_max_nr_subpasses * cara_single.proc_map->nr_processors) )
      );
  for ( unsigned int cmd_center_idx = 1;
      cmd_center_idx < cara_max_nr_fif; cmd_center_idx++ ) {
    cara_vk.arr_frame_infos[cmd_center_idx].arr_cmd_pools =
      cara_vk.arr_frame_infos[cmd_center_idx-1].arr_cmd_pools +
      1 + cara_single.proc_map->nr_processors;
  }
  static_assert( alignof(VkCommandPool) >= alignof(VkCommandBuffer),
      "should be aligned in order." );

  // Command pools are used mainly as a source of memory for the command buffers, but this is not
  // the only reason for which they are created. They inform the driver about the intended usage of
  // command buffers allocated from them, and whether we must reset or free them in bulk, or if we
  // can do it separately per each command buffer.
  constexpr VkCommandPoolCreateFlags parameters =
    //VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |   // Without this flag, we can do this only
                                                        // in groups - on all command buffers
                                                        // allocated from a given pool. Recording a
                                                        // command buffer implicitly resets it so,
                                                        // without this flag, we can record a
                                                        // command buffer only once. If we want to
                                                        // record it again, we need to reset the
                                                        // whole pool from which it was allocated.
    0;
  VkCommandPoolCreateInfo cmd_pool_create_info;
  cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmd_pool_create_info.pNext = nullptr;
  cmd_pool_create_info.flags = parameters;
  cmd_pool_create_info.queueFamilyIndex = arr_q_create_info[idx_graphics_q].queueFamilyIndex;

  // Prepare a template
  VkCommandBufferAllocateInfo cmd_buf_alloc_info;
  cmd_buf_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_buf_alloc_info.pNext = nullptr;
  //cmd_buf_alloc_info.commandPool = CREATE_A_POOL_FIRST;
  cmd_buf_alloc_info.commandBufferCount = cara_max_nr_subpasses;
  cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;  // - Primary command buffers can be
                                                        // directly submitted to queues. They can
                                                        // also execute (call) secondary command
                                                        // buffers.
                                                        // - Secondary command buffers can only be
                                                        // executed from primary command buffers,
                                                        // and we are not allowed to submit them.
  for ( uint_fast8_t fb_idx = 0; fb_idx < cara_max_nr_fif; fb_idx++ ) {

    for ( uint_fast8_t worker_idx = 0;
        worker_idx < cara_single.proc_map->nr_processors; worker_idx++ ) {

      result = vkCreateCommandPool(
          new_log_dev,
          &cmd_pool_create_info,
          nullptr,
          //&cara_vk.vk_alloc_callbacks,
          &cara_vk.arr_frame_infos[fb_idx].arr_cmd_pools[worker_idx]
          );
      if ( result != VK_SUCCESS ) {
        fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool for graphics commands. "
            "(%s)\n", string_VkResult(result) );
        goto error_after_swapchain_images;
      }

      cmd_buf_alloc_info.commandPool =
        cara_vk.arr_frame_infos[fb_idx].arr_cmd_pools[worker_idx];
      result = vkAllocateCommandBuffers(
          new_log_dev,
          &cmd_buf_alloc_info,
          &cara_get_arr_cmd_buf_for_graphics(
            cara_single.proc_map->nr_processors, fb_idx
            )[fb_idx * cara_single.proc_map->nr_processors * cara_max_nr_subpasses +
            worker_idx * cara_max_nr_subpasses]
          );
      
      if ( result != VK_SUCCESS ) {
        fprintf( stderr, "[Vulkan] Failed to allocate command buffers. (%s)\n",
            string_VkResult(result) );
        goto error_after_swapchain_images;
      }
#ifdef DEBUG_MODE
      fprintf( stderr, "[Vulkan-Debug] G fb_idx=%u worker_idx=%u new_cmd_buf=%#018lx\n",
          fb_idx,
          worker_idx,
          ptr2ui(cara_get_arr_cmd_buf_for_graphics(
            cara_single.proc_map->nr_processors, fb_idx
            )[fb_idx * cara_single.proc_map->nr_processors * cara_max_nr_subpasses +
            worker_idx * cara_max_nr_subpasses]) );
#endif

    }

    // Prepare one primary command buffer for graphics commands
    result = vkCreateCommandPool(
        new_log_dev,
        &cmd_pool_create_info,
        nullptr,
        //&cara_vk.vk_alloc_callbacks,
        &cara_vk.arr_frame_infos[fb_idx].arr_cmd_pools[cara_single.proc_map->nr_processors]
        );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool. (%s)\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }

    cmd_buf_alloc_info.commandBufferCount = 1;
    cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buf_alloc_info.commandPool =
      cara_vk.arr_frame_infos[fb_idx].arr_cmd_pools[cara_single.proc_map->nr_processors];

    result = vkAllocateCommandBuffers(
        new_log_dev,
        &cmd_buf_alloc_info,
        &cara_vk.arr_frame_infos[fb_idx].primary_cmd_buf
        );
    cmd_buf_alloc_info.commandBufferCount = cara_max_nr_subpasses;
    cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Failed to allocate a command buffer. (%s)\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }
#ifdef DEBUG_MODE
    fprintf( stderr, "[Vulkan-Debug] GP fb_idx=%u new_cmd_buf=%#018lx\n",
        fb_idx,
        ptr2ui(cara_vk.arr_frame_infos[fb_idx].primary_cmd_buf) ); 
#endif

  }


  // Prepare transfer command buffers
  cmd_pool_create_info.queueFamilyIndex = arr_q_create_info[idx_transfer_q].queueFamilyIndex;
  cmd_buf_alloc_info.commandBufferCount = 1;

  static_assert( alignof(VkCommandPool) >= alignof(VkCommandBuffer),
      "should be aligned in order." );
  cara_vk.trans_bufs.arr_cmd_pools =
    cara_malloc( &worker_state->malloc_helper,
      alignof(VkCommandPool),
      sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
      sizeof(VkCommandBuffer) * cara_single.proc_map->nr_processors );
  memset( cara_vk.trans_bufs.arr_cmd_pools,
      0,
      sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
      sizeof(VkCommandBuffer) * cara_single.proc_map->nr_processors );

  for ( uint_fast8_t worker_idx = 0;
      worker_idx < cara_single.proc_map->nr_processors; worker_idx++ ) {

    result = vkCreateCommandPool(
        new_log_dev,
        &cmd_pool_create_info,
        nullptr,
        //&cara_vk.vk_alloc_callbacks,
        &cara_vk.trans_bufs.arr_cmd_pools[worker_idx]
        );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool for transfer commands. "
          "(%s)\n", string_VkResult(result) );
      goto error_after_swapchain_images;
    }
#ifdef DEBUG_MODE
    fprintf( stderr, "[Vulkan-Debug] T worker_idx=%u new_cmd_buf=%#018lx\n",
        worker_idx,
        ptr2ui(cara_vk.trans_bufs.arr_cmd_pools[worker_idx]) );
#endif

    cmd_buf_alloc_info.commandPool =
      cara_vk.trans_bufs.arr_cmd_pools[worker_idx];
    result = vkAllocateCommandBuffers(
        new_log_dev,
        &cmd_buf_alloc_info,
        &cara_get_arr_cmd_bufs_for_transfer( cara_single.proc_map->nr_processors )[worker_idx]
        );
    
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Failed to allocate command buffers. (%s)\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }

  }

  // Prepare one primary command buffer for transfer commands
  result = vkCreateCommandPool(
      new_log_dev,
      &cmd_pool_create_info,
      nullptr,
      //&cara_vk.vk_alloc_callbacks,
      &cara_vk.trans_bufs.arr_cmd_pools[cara_single.proc_map->nr_processors]
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool. (%s)\n",
        string_VkResult(result) );
    goto error_after_swapchain_images;
  }

  cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_buf_alloc_info.commandPool =
    cara_vk.trans_bufs.arr_cmd_pools[cara_single.proc_map->nr_processors];

  result = vkAllocateCommandBuffers(
      new_log_dev,
      &cmd_buf_alloc_info,
      &cara_vk.trans_bufs.primary_cmd_buf
      );
  
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Vulkan] Failed to allocate a command buffer. (%s)\n",
        string_VkResult(result) );
    goto error_after_swapchain_images;
  }
#ifdef DEBUG_MODE
  fprintf( stderr, "[Vulkan-Debug] TP new_cmd_buf=%#018lx\n",
      ptr2ui(cara_vk.trans_bufs.primary_cmd_buf) );
#endif


  //
  // Prepare a few sign bearers
  //
  constexpr VkSemaphoreTypeCreateInfo semaphore_type_create_info = {
    VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
    nullptr,
    VK_SEMAPHORE_TYPE_TIMELINE,
    0
  };
  VkSemaphoreCreateInfo semaphore_create_info;
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = &semaphore_type_create_info;
  semaphore_create_info.flags = 0;

  VkSemaphore semaphore;
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &semaphore_create_info,
        nullptr,
        //&cara_vk.vk_alloc_callbacks,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (main_semaphore_before_A)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_semaphore_before_A[fif_idx] = semaphore;
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &semaphore_create_info,
        nullptr,
        //&cara_vk.vk_alloc_callbacks,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (main_semaphore_after_A)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_semaphore_after_A[fif_idx] = semaphore;
  }

  // Start making binary semaphores
  semaphore_create_info.pNext = nullptr;
  cara_vk.arr_wsi_present_semaphores = cara_malloc( &worker_state->malloc_helper,
        alignof(VkSemaphore), sizeof(VkSemaphore) * nr_swapchain_imgs );
  if ( !cara_vk.arr_wsi_present_semaphores ) {
    fprintf( stderr, "[Vulkan] %s\n", err_msg_text[err_msg_out_of_memory] );
    goto error_after_swapchain_images;
  }
  memset( cara_vk.trans_bufs.arr_cmd_pools, 0, sizeof(VkSemaphore) * nr_swapchain_imgs );
  for ( unsigned int swapchain_img_idx = 0;
      swapchain_img_idx < nr_swapchain_imgs; swapchain_img_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &semaphore_create_info,
        nullptr,
        //&cara_vk.vk_alloc_callbacks,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (wsi_semaphore_present)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_acquire_semaphores[swapchain_img_idx] = semaphore;
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &semaphore_create_info,
        nullptr,
        //&cara_vk.vk_alloc_callbacks,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (wsi_semaphore_acquire)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_acquire_semaphores[fif_idx] = semaphore;
  }

  VkPhysicalDeviceMemoryProperties2 memory_properties;
  memory_properties.pNext = nullptr;
  memory_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  vkGetPhysicalDeviceMemoryProperties2(
      arr_phys_devices[idx_phys_deviced_picked], &memory_properties );
  if ( ma_init_vk_mem_mgr(&cara_vk.vmm, &memory_properties,
        &arr_device_props[idx_phys_deviced_picked].properties.limits,
        &worker_state->malloc_helper, cara_malloc) != err_msg_no_error ) {
    cara_vk.vmm.arr_sizes = nullptr;
    goto error_after_swapchain_images;
  }

  cara_vk.max_anisotropy = min(
      arr_device_props[idx_phys_deviced_picked].properties.limits.maxSamplerAnisotropy, 8.0f );



  //
  // Determine which texture format to use between ASTC and S3TC
  //

  // Check ASTC formats first
  constexpr uint_fast8_t astc_formats_used[] = {
    VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
    VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
    VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
    VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
    VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
  };
  bool all_formats_are_supported = true;
#ifdef DEBUG_MODE
  fprintf( stdout,
      "[Vulkan] Checking supported texture decoders for ASTC format on the device...\n" );
#endif
  for ( uint_fast8_t idx_fmt = 0; idx_fmt < _arr_elem_cnt(astc_formats_used); idx_fmt++ ) {
    VkFormatProperties2 format_props;
    format_props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    format_props.pNext = nullptr;
    vkGetPhysicalDeviceFormatProperties2(
        arr_phys_devices[idx_phys_deviced_picked],
        astc_formats_used[idx_fmt],
        &format_props
        );
    if ( !(format_props.formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ) {
      all_formats_are_supported = false;
    }
#ifdef TEXTURE_BILINEAR_FILTERING
    if ( !(format_props.formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        ) {
      all_formats_are_supported = false;
    }
#endif
  }
  all_formats_are_supported = false;

  if ( all_formats_are_supported ) {
    cara_vk.texture_format_chosen = texture_format_astc;
  } else {
#ifdef DEBUG_MODE
    fprintf( stdout,
        "[Vulkan] Checking supported texture decoders for S3TC(BCn) format on the device...\n" );
#endif
    constexpr uint_fast8_t s3tc_formats_used[] = {
      //VK_FORMAT_BC1_RGB_UNORM_BLOCK,
      //VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
      //VK_FORMAT_BC2_SRGB_BLOCK,
      //VK_FORMAT_BC2_UNORM_BLOCK,
      //VK_FORMAT_BC3_SRGB_BLOCK,
      //VK_FORMAT_BC3_UNORM_BLOCK,
      //VK_FORMAT_BC4_UNORM_BLOCK,
      //VK_FORMAT_BC5_UNORM_BLOCK,
      //VK_FORMAT_BC6H_UFLOAT_BLOCK,
      //VK_FORMAT_BC7_SRGB_BLOCK,
      VK_FORMAT_BC7_UNORM_BLOCK,
    };
    all_formats_are_supported = 1;
    for ( uint_fast8_t idx_fmt = 0; idx_fmt < _arr_elem_cnt(s3tc_formats_used); idx_fmt++ ) {
      VkFormatProperties2 format_props;
      format_props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
      format_props.pNext = nullptr;
      vkGetPhysicalDeviceFormatProperties2(
          arr_phys_devices[idx_phys_deviced_picked],
          s3tc_formats_used[idx_fmt],
          &format_props
          );
      if ( !(format_props.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ) {
        all_formats_are_supported = 0;
      }
#ifdef TEXTURE_BILINEAR_FILTERING
      if ( !(format_props.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ) {
        all_formats_are_supported = 0;
      }
#endif
    }

    if ( all_formats_are_supported ) {
      cara_vk.texture_format_chosen = texture_format_s3tc;
    } else {
      fprintf( stderr,
          "[Vulkan] %s doesn't support optimal operations on compressed texture data.\n",
          arr_device_props[idx_phys_deviced_picked].properties.deviceName );
error_after_swapchain_images:
      for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
        if( cara_vk.arr_acquire_semaphores[fif_idx] != VK_NULL_HANDLE ) {
          vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_acquire_semaphores[fif_idx], nullptr );
          cara_vk.arr_acquire_semaphores[fif_idx] = VK_NULL_HANDLE;
        }
      }
      if( cara_vk.arr_wsi_present_semaphores != nullptr ) {
        for ( unsigned int swapchain_img_idx = 0;
            swapchain_img_idx < nr_swapchain_imgs; swapchain_img_idx++ ) {
          if( cara_vk.arr_wsi_present_semaphores[swapchain_img_idx] != VK_NULL_HANDLE ) {
            vkDestroySemaphore( cara_vk.log_dev,
                cara_vk.arr_wsi_present_semaphores[swapchain_img_idx],
                nullptr );
            cara_vk.arr_wsi_present_semaphores[swapchain_img_idx] = VK_NULL_HANDLE;
          }
        }
        ma_free( cara_single.mem_arena, worker_state->worker_info->idx_self,
            cara_vk.arr_wsi_present_semaphores );
        cara_vk.arr_wsi_present_semaphores = nullptr;
      }
      for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
        if( cara_vk.arr_semaphore_before_A[fif_idx] != VK_NULL_HANDLE ) {
          vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_acquire_semaphores[fif_idx], nullptr );
          cara_vk.arr_semaphore_before_A[fif_idx] = VK_NULL_HANDLE;
        }
        if( cara_vk.arr_semaphore_after_A[fif_idx] != VK_NULL_HANDLE ) {
          vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_acquire_semaphores[fif_idx], nullptr );
          cara_vk.arr_semaphore_after_A[fif_idx] = VK_NULL_HANDLE;
        }
      }
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self,
          arr_swapchain_images );
error_after_logical_device:
      if ( cara_vk.trans_bufs.arr_cmd_pools ) {
        for ( uint_fast8_t idx_command_pool = 0;
            idx_command_pool <= cara_single.proc_map->nr_processors; idx_command_pool++ )
        {
          if ( cara_vk.trans_bufs.arr_cmd_pools[idx_command_pool] )
          {
            vkDestroyCommandPool( cara_vk.log_dev,
                cara_vk.trans_bufs.arr_cmd_pools[idx_command_pool],
                nullptr );
                //&cara_vk.vk_alloc_callbacks );

            cara_vk.trans_bufs.arr_cmd_pools[idx_command_pool] = 0;
          }
        }
      }

      for ( uint_fast8_t idx_framebuffer = 0;
          idx_framebuffer < cara_max_nr_fif; idx_framebuffer++ ) {
        for ( uint_fast8_t worker_idx = 0;
            worker_idx <= cara_single.proc_map->nr_processors; worker_idx++ ) {
          if ( cara_vk.arr_frame_infos[idx_framebuffer].arr_cmd_pools[worker_idx] ) {
            vkDestroyCommandPool(
                new_log_dev,
                cara_vk.arr_frame_infos[idx_framebuffer].arr_cmd_pools[worker_idx],
                nullptr
                //&cara_vk.vk_alloc_callbacks
                );

            cara_vk.arr_frame_infos[idx_framebuffer].arr_cmd_pools[worker_idx] = 0;
          }
        }
      }

      //vkDestroyDevice( new_log_dev, &cara_vk.vk_alloc_callbacks );
      vkDestroyDevice( new_log_dev, nullptr );
error_after_arr_q_create_info:
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self,
          arr_q_create_info );
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self,
          arr_qfam_props );
error_after_presentation_surface:
      //vkDestroySurfaceKHR( vk_instance, presentation_surface,
      //    &cara_vk.vulkan_alloc_callbacks );
      SDL_Vulkan_DestroySurface(
          vk_instance,
          presentation_surface,
          nullptr
          //&cara_vk.vk_alloc_callbacks
          );
      SDL_DestroyWindow(window);
      //ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, window);
      //vkDestroyInstance(vk_instance, &cara_vk.vk_alloc_callbacks);
      vkDestroyInstance( vk_instance, nullptr );
      ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, arr_device_props);
      ma_free( cara_single.mem_arena,
          worker_state->worker_info->idx_self, arr_phys_devices );
      //unload_shared_lib( vulkan_library );
      return 1;
    }
  }

  // Choose a depth buffer format
  VkFormat arr_zbuf_cands[] = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM,
  };
  VkFormatProperties2 format_props;
  format_props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
  format_props.pNext = nullptr;
  VkFormat chosen_fmt = VK_FORMAT_R8_BOOL_ARM;
  for ( uint_fast8_t idx_fmt = 0; idx_fmt < _arr_elem_cnt(arr_zbuf_cands); idx_fmt++ ) {
    vkGetPhysicalDeviceFormatProperties2(
        arr_phys_devices[idx_phys_deviced_picked],
        arr_zbuf_cands[idx_fmt],
        &format_props
        );
    if ( format_props.formatProperties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) {
      chosen_fmt = idx_fmt;
      break;
    }
  }
  if ( chosen_fmt == VK_FORMAT_R8_BOOL_ARM ) {
    for ( uint_fast8_t idx_fmt = 0; idx_fmt < _arr_elem_cnt(arr_zbuf_cands); idx_fmt++ ) {
      vkGetPhysicalDeviceFormatProperties2(
          arr_phys_devices[idx_phys_deviced_picked],
          arr_zbuf_cands[idx_fmt],
          &format_props
          );
      if ( format_props.formatProperties.linearTilingFeatures &
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) {
        chosen_fmt = idx_fmt;
        break;
      }
    }
  }
  cara_vk.zbuf_fmt = chosen_fmt;


  cara_vk.log_dev = new_log_dev;
  cara_vk.vk_instance = vk_instance;
  //cara_vk.vk_library = vulkan_library;
  cara_vk.window = window;
  cara_vk.nr_swapchain_imgs = nr_swapchain_imgs;
  cara_vk.swapchain = swapchain;
  cara_vk.arr_swapchain_imgs = arr_swapchain_images;
  cara_vk.dims_swapchain_imgs = image_size;

  ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, arr_q_create_info);
  ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, arr_qfam_props);
  ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, arr_device_props);
  ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, arr_phys_devices);
  atomic_store_explicit( &cara_vk.tmp_var_ready, -7, memory_order_release);

  return 0;
}




//-------------------------------------------------------------------------------------------------
void destroy_vulkan_objects( struct ms_worker_info * worker_info )
{
  // TODO Host access to all VkQueue objects created from `device` must be externally synchronized
  const VkResult result = vkDeviceWaitIdle(cara_vk.log_dev);
  switch( result ) {
    case VK_ERROR_DEVICE_LOST:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_VALIDATION_FAILED_EXT:
    case VK_ERROR_UNKNOWN:
    default:
      fprintf( stderr, "[Vulkan] vkDeviceWaitIdle failed! Reason: '%s'\n",
          string_VkResult(result) );
      return;
    case VK_SUCCESS:
      break;
  }

  if ( cara_vk.framebuffer ) {
    //vkDestroyFramebuffer( cara_vk.log_dev, cara_vk.framebuffer, &cara_vk.vk_alloc_callbacks );
    vkDestroyFramebuffer( cara_vk.log_dev, cara_vk.framebuffer, nullptr );
    cara_vk.framebuffer = VK_NULL_HANDLE;
  }

  if ( cara_vk.render_pass ) {
    //vkDestroyRenderPass( cara_vk.log_dev, cara_vk.render_pass, &cara_vk.vk_alloc_callbacks );
    vkDestroyRenderPass( cara_vk.log_dev, cara_vk.render_pass, nullptr );
    cara_vk.render_pass = VK_NULL_HANDLE;
  }

  if ( cara_vk.pipeline_layout ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pipeline_layout,
        //&cara_vk.vk_alloc_callbacks );
        nullptr );
    cara_vk.pipeline_layout = VK_NULL_HANDLE;
  }

  if ( cara_vk.ds_first ) {
    vkResetDescriptorPool( cara_vk.log_dev, cara_vk.dp_first, 0 );
    cara_vk.ds_first = VK_NULL_HANDLE;
  }

  if ( cara_vk.dp_first ) {
    //vkDestroyDescriptorPool( cara_vk.log_dev, cara_vk.dp_first, &cara_vk.vk_alloc_callbacks );
    vkDestroyDescriptorPool( cara_vk.log_dev, cara_vk.dp_first, nullptr );
    cara_vk.dp_first = VK_NULL_HANDLE;
  }

  if ( cara_vk.dsl_first ) {
    //vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_first, &cara_vk.vk_alloc_callbacks );
    vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_first, nullptr );
    cara_vk.dsl_first = VK_NULL_HANDLE;
  }

  if ( cara_vk.sampler_8_2048_tex ) {
    //vkDestroySampler( cara_vk.log_dev, cara_vk.sampler_8_2048_tex, &cara_vk.vk_alloc_callbacks );
    vkDestroySampler( cara_vk.log_dev, cara_vk.sampler_8_2048_tex, nullptr );
    cara_vk.sampler_8_2048_tex = VK_NULL_HANDLE;
  }

  if ( cara_vk.iv_8_2048_tex ) {
    //vkDestroyImageView( cara_vk.log_dev, cara_vk.iv_8_2048_tex, &cara_vk.vk_alloc_callbacks );
    vkDestroyImageView( cara_vk.log_dev, cara_vk.iv_8_2048_tex, nullptr );
    cara_vk.iv_8_2048_tex = VK_NULL_HANDLE;
  }

  if ( cara_vk.sampler_8_2048_tex ) {
    //vkDestroyImage( cara_vk.log_dev, cara_vk.vki_8_2048_tex, &cara_vk.vk_alloc_callbacks );
    vkDestroyImage( cara_vk.log_dev, cara_vk.vki_8_2048_tex, nullptr );
    cara_vk.vki_8_2048_tex = VK_NULL_HANDLE;
  }

  if ( cara_vk.vmm.arr_idx_next_unoccupied_elem ) {
    // This function will put nullptr in the struct
    ma_destroy_vk_mem_mgr( &cara_vk.vmm, cara_vk.log_dev,
        ui2ptr(worker_info->idx_self), cara_free );
  }

  // FIXME Host access to `semaphore` must be externally synchronized (vkDestroySemaphore)
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    if( cara_vk.arr_acquire_semaphores[fif_idx] != VK_NULL_HANDLE ) {
      vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_acquire_semaphores[fif_idx],
          //&cara_vk.vk_alloc_callbacks );
          nullptr );
      cara_vk.arr_acquire_semaphores[fif_idx] = VK_NULL_HANDLE;
    }
  }
  if( cara_vk.arr_wsi_present_semaphores != nullptr ) {
    for ( unsigned int swapchain_img_idx = 0;
        swapchain_img_idx < cara_vk.nr_swapchain_imgs; swapchain_img_idx++ ) {
      if( cara_vk.arr_wsi_present_semaphores[swapchain_img_idx] != VK_NULL_HANDLE ) {
        vkDestroySemaphore( cara_vk.log_dev,
            cara_vk.arr_wsi_present_semaphores[swapchain_img_idx],
            //&cara_vk.vk_alloc_callbacks );
            nullptr );
        cara_vk.arr_wsi_present_semaphores[swapchain_img_idx] = VK_NULL_HANDLE;
      }
    }
    ma_free( cara_single.mem_arena, worker_info->idx_self, cara_vk.arr_wsi_present_semaphores );
    cara_vk.arr_wsi_present_semaphores = nullptr;
  }
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    if( cara_vk.arr_semaphore_before_A[fif_idx] != VK_NULL_HANDLE ) {
      vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_acquire_semaphores[fif_idx], nullptr );
      cara_vk.arr_semaphore_before_A[fif_idx] = VK_NULL_HANDLE;
    }
    if( cara_vk.arr_semaphore_after_A[fif_idx] != VK_NULL_HANDLE ) {
      vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_acquire_semaphores[fif_idx], nullptr );
      cara_vk.arr_semaphore_after_A[fif_idx] = VK_NULL_HANDLE;
    }
  }

  if ( cara_vk.swapchain ) {
    // FIXME Host access to swapchain must be externally synchronized
    vkDestroySwapchainKHR( cara_vk.log_dev,
        cara_vk.swapchain,
        //&cara_vk.vk_alloc_callbacks );
        nullptr );
    cara_vk.swapchain = VK_NULL_HANDLE;

    ma_free( cara_single.mem_arena, worker_info->idx_self, cara_vk.arr_swapchain_imgs );
    cara_vk.arr_swapchain_imgs = nullptr;
  }

  if ( cara_vk.trans_bufs.arr_cmd_pools ) {
    for ( uint_fast8_t idx_command_pool = 0;
        idx_command_pool <= cara_single.proc_map->nr_processors; idx_command_pool++ ) {
      if ( cara_vk.trans_bufs.arr_cmd_pools[idx_command_pool] ) {
        //fprintf( stdout, "trans_bufs.arr_cmd_pools[%u] = %#018lx\n", idx_command_pool,
        //    ptr2ui(cara_vk.trans_bufs.arr_cmd_pools[idx_command_pool]) );
        // FIXME Host access to `commandPool` must be externally synchronized
        vkDestroyCommandPool( cara_vk.log_dev,
            cara_vk.trans_bufs.arr_cmd_pools[idx_command_pool],
            //&cara_vk.vk_alloc_callbacks );
            nullptr );
       }
    }
    ma_free( cara_single.mem_arena, worker_info->idx_self, cara_vk.trans_bufs.arr_cmd_pools );
    cara_vk.trans_bufs.arr_cmd_pools = nullptr;
  }

  if ( cara_vk.arr_frame_infos[0].arr_cmd_pools ) {
    for ( uint_fast8_t idx_framebuffer = 0;
        idx_framebuffer < cara_max_nr_fif; idx_framebuffer++ ) {
      for ( uint_fast8_t idx_command_pool = 0;
          idx_command_pool <= cara_single.proc_map->nr_processors; idx_command_pool++ ) {
        if ( cara_vk.arr_frame_infos[idx_framebuffer].arr_cmd_pools[idx_command_pool] ) {
          //fprintf( stdout, "framebufs[%u].arr_cmd_pools[%u] = %#018lx\n",
          //    idx_framebuffer,
          //    idx_command_pool,
          //    ptr2ui(cara_vk.arr_framebufs[idx_framebuffer].arr_cmd_pools[idx_command_pool]) );
          vkDestroyCommandPool( cara_vk.log_dev,
              cara_vk.arr_frame_infos[idx_framebuffer].arr_cmd_pools[idx_command_pool],
              //&cara_vk.vk_alloc_callbacks );
              nullptr );
        }
      }
    }
    ma_free( cara_single.mem_arena, worker_info->idx_self, cara_vk.arr_frame_infos[0].arr_cmd_pools );
    cara_vk.arr_frame_infos[0].arr_cmd_pools = nullptr;
  }

  if ( cara_vk.presentation_surface ) {
    //vkDestroySurfaceKHR(
    //cara_vk.vk_instance,
    //cara_vk.presentation_surface,
    //&cara_vk.vulkan_alloc_callbacks
    //);
    SDL_Vulkan_DestroySurface(
        cara_vk.vk_instance,
        cara_vk.presentation_surface,
        //&cara_vk.vk_alloc_callbacks );
        nullptr );
    cara_vk.presentation_surface = VK_NULL_HANDLE;
  }

  if ( cara_vk.window ) {
    SDL_DestroyWindow(cara_vk.window);
    cara_vk.window = nullptr;
  }

  if ( cara_vk.log_dev ) {
    //vkDestroyDevice( cara_vk.log_dev, &cara_vk.vk_alloc_callbacks );
    vkDestroyDevice( cara_vk.log_dev, nullptr );
    cara_vk.log_dev = VK_NULL_HANDLE;
  }

  if ( cara_vk.vk_instance ) {
    //vkDestroyInstance( cara_vk.vk_instance, &cara_vk.vk_alloc_callbacks );
    vkDestroyInstance( cara_vk.vk_instance, nullptr );
    cara_vk.vk_instance = VK_NULL_HANDLE;
  }

  //if ( cara_vk.vk_library ) {
  //  unload_shared_lib( cara_vk.vk_library );
  //  cara_vk.vk_library = nullptr;
  //}

}


