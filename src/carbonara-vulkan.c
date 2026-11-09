#include <SDL3/SDL_error.h>
#include <stdatomic.h>
#include <stdint.h>

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

#include "info_struct.h"
#include "appdev_support-processor_map.h"
#include "carbonara-config.h"
#include "carbonara-mm.h"
#include "carbonara-singleton.h"
#include "carbonara-graphics.h"
#include "util-c_lang-basic.h"
//#include "vulkan_functions.h"  // Just use volk
#include "memorable_arena.h"
#include "err_msg.h"
#include "util-spall.h"

#ifdef DEBUG_MODE
#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
#if !defined( ARE_WE_SHIPPING_IT )
  [[maybe_unused]]  // Just use vkconfig-gui
  static const char * enabled_layer_names[] = {
    "VK_LAYER_KHRONOS_validation",
#if defined( DEBUG_MODE )
    "VK_LAYER_LUNARG_api_dump",
#endif
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
  VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,  // VkPhysicalDeviceVulkan13Features::synchronization2
  //VK_KHR_MAINTENANCE3_EXTENSION_NAME,   // Required for descriptor indexing; it has been promoted
                                          //  to Vulkan 1.1
  VK_KHR_MAINTENANCE_8_EXTENSION_NAME,  // To make src/dst sync scopes to work when doing a queue
                                        //  family ownership transfer.
  VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,  //\
                                            //VkPhysicalDeviceVulkan12Features::scalarBlockLayout
  VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,  // Descriptor indexing
};

// I just picked 1.2 simply for vkCmdDrawIndexedIndirectCount as a beginner with no clue.
static constexpr unsigned int vulkan_version_minor = 2;
static constexpr unsigned int vulkan_version_major = 1;

// Singleton
struct cara_vk_renderer cara_vk;

// I guess I wanted to watch the memory usage of vulkan programmatically
thread_local VkAllocationCallbacks vk_alloc_callbacks;


//-------------------------------------------------------------------------------------------------
static enum err_msg create_2nd_renderpass( struct cara_vk_renderer * renderer );


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
        worker_state->worker_idx, arr_extension_props );
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
          worker_state->worker_idx, arr_extension_props );
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
          worker_state->worker_idx, arr_extension_props );
      //unload_shared_lib( vulkan_library );
      return 1;
    }
  }

  ma_free( cara_single.mem_arena,
      worker_state->worker_idx, arr_extension_props );



  //
  // Prepare VkAllocationCallbacks after a clean-up
  //
  auto the_texture = cara_vk.alb_stone;
  auto the_texture2 = cara_vk.alb_sand;
  auto len_2048_bc7 = cara_vk.asset_size;
  auto vert_shader = cara_vk.spv_simple_vert;
  auto len_vert_shader = cara_vk.len_simple_vert;
  auto frag_shader = cara_vk.spv_simple_frag;
  auto len_frag_shader = cara_vk.len_simple_frag;
  auto depth_only_shader = cara_vk.spv_z_only;
  auto len_depth_only_shader = cara_vk.len_z_only;
  auto depth_only_frag_shader = cara_vk.spv_z_only_frag;
  auto len_depth_only_frag_shader = cara_vk.len_z_only_frag;
  auto sc_frag_shader = cara_vk.spv_swapchain_vert;
  auto len_sc_frag_shader = cara_vk.len_swapchain_vert;
  auto sc_vert_shader = cara_vk.spv_swapchain_frag;
  auto len_sc_vert_shader = cara_vk.len_swapchain_frag;
  memset( &cara_vk, 0, sizeof(struct cara_vk_renderer) );
  cara_vk.alb_stone = the_texture;
  cara_vk.alb_sand = the_texture2;
  cara_vk.asset_size = len_2048_bc7;
  cara_vk.spv_simple_vert = vert_shader;
  cara_vk.len_simple_vert = len_vert_shader;
  cara_vk.spv_simple_frag = frag_shader;
  cara_vk.len_simple_frag = len_frag_shader;
  cara_vk.spv_z_only = depth_only_shader;
  cara_vk.len_z_only = len_depth_only_shader;
  cara_vk.spv_z_only_frag = depth_only_frag_shader;
  cara_vk.len_z_only_frag = len_depth_only_frag_shader;
  cara_vk.spv_swapchain_vert = sc_vert_shader;
  cara_vk.len_swapchain_vert = len_sc_vert_shader;
  cara_vk.spv_swapchain_frag = sc_frag_shader;
  cara_vk.len_swapchain_frag = len_sc_frag_shader;


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
    // Just use vkconfig-gui
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
      &vk_alloc_callbacks,
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
    vkDestroyInstance(vk_instance, &vk_alloc_callbacks);
    //vkDestroyInstance(vk_instance, nullptr);
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
    vkDestroyInstance( vk_instance, &vk_alloc_callbacks );
    //vkDestroyInstance(vk_instance, nullptr);
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
          worker_state->worker_idx, arr_phys_devices);
      vkDestroyInstance(vk_instance, &vk_alloc_callbacks);
      //vkDestroyInstance(vk_instance, nullptr);
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
          worker_state->worker_idx, arr_extension_props );
      ma_free( cara_single.mem_arena,
          worker_state->worker_idx, arr_phys_devices);
      vkDestroyInstance( vk_instance, &vk_alloc_callbacks );
      //vkDestroyInstance(vk_instance, nullptr);
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
        worker_state->worker_idx, arr_extension_props );
  }



  //
  // Check the properties and supported features of each device
  //
  VkPhysicalDeviceProperties2 * arr_device_props;
  VkPhysicalDeviceDescriptorIndexingPropertiesEXT arr_descriptor_indexing_props[nr_phys_devices];

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

    VkPhysicalDeviceVulkan11Features vk11_features = {0};
    vk11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vk11_features.pNext = nullptr;
    VkPhysicalDeviceVulkan12Features vk12_features = {0};
    vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12_features.pNext = &vk11_features;
    VkPhysicalDeviceVulkan13Features vk13_features = {0};
    vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13_features.pNext = &vk12_features;

    // pNext chain includes a VkPhysicalDeviceVulkan12Features structure, then it must not include
    //  a VkPhysicalDeviceTimelineSemaphoreFeatures structure.  The features in
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

    // There are lot of properties associated with descriptor_indexing, grab them here.
    arr_descriptor_indexing_props[idx_phys_dev].sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT;
    arr_descriptor_indexing_props[idx_phys_dev].pNext = nullptr;
    arr_device_props[idx_phys_dev].pNext = &arr_descriptor_indexing_props[idx_phys_dev];

    vkGetPhysicalDeviceProperties2( arr_phys_devices[idx_phys_dev],
        &arr_device_props[idx_phys_dev] );

    if ( !vk11_features.shaderDrawParameters ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "shaderDrawParameters"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

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


    //
    // Descriptor indexing for update-after-bind
    //

    if ( !device_features.features.shaderSampledImageArrayDynamicIndexing ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "shaderSampledImageArrayDynamicIndexing"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

    // These are required to support the 4 descriptor binding flags we use in this renderer.
    if ( !vk12_features.descriptorBindingSampledImageUpdateAfterBind ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "descriptorBindingSampledImageUpdateAfterBind"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
    if ( !vk12_features.descriptorBindingPartiallyBound ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "descriptorBindingPartiallyBound"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
    if ( !vk12_features.descriptorBindingUpdateUnusedWhilePending ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "descriptorBindingUpdateUnusedWhilePending"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }
    if ( !vk12_features.descriptorBindingVariableDescriptorCount ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "descriptorBindingVariableDescriptorCount"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

    // Enables use of runtimeDescriptorArrays in SPIR-V shaders.
    if ( !vk12_features.runtimeDescriptorArray ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "runtimeDescriptorArray"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }


    //
    // Sparse binding for mip-level memory management
    //

    // Sparse resources can be bound non-contiguously to one or more VkDeviceMemory allocations.
    // Sparse resources can be re-bound to different memory allocations over the lifetime of the
    //   resource.
    // Sparse resources can have descriptors generated and used orthogonally with memory binding
    //   commands.

    // The sparseBinding feature is the base, and provides the following capabilities:
    // - Resources can be bound at some defined (sparse block) granularity.
    // - The entire resource must be bound to memory before use regardless of regions actually
    //   accessed. (This property should be modified via sparseResidency.+)
    // - No specific mapping of image region to memory offset is defined, i.e. the location that
    //   each texel corresponds to in memory is implementation-dependent.
    // - Sparse buffers have a well-defined mapping of buffer range to memory range, where an offset
    //   into a range of the buffer that is bound to a single contiguous range of memory corresponds
    //   to an identical offset within that range of memory.
    // - Requested via the VK_IMAGE_CREATE_SPARSE_BINDING_BIT and
    //   VK_BUFFER_CREATE_SPARSE_BINDING_BIT bits.
    // - A sparse image created using VK_IMAGE_CREATE_SPARSE_BINDING_BIT (but not
    //   VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) supports all formats that non-sparse usage supports,
    //   and supports both VK_IMAGE_TILING_OPTIMAL and VK_IMAGE_TILING_LINEAR tiling.
    if ( !device_features.features.sparseBinding ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "sparseBinding"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

    // Sparse Residency builds on (and requires) the sparseBinding feature.
    // It includes the following capabilities:
    // - Resources do not have to be completely bound to memory before use on the device.
    // - Images have a prescribed sparse image block layout, allowing specific rectangular regions
    //   of the image to be bound to specific offsets in memory allocations.
    // - Consistency of access to unbound regions of the resource is defined by the absence or
    //   presence of VkPhysicalDeviceSparseProperties::residencyNonResidentStrict. If this property
    //   is present, accesses to unbound regions of the resource are well defined and behave as if
    //   the data bound is populated with all zeros; writes are discarded. When this property is
    //   absent, accesses are considered safe, but reads will return undefined values.
    // - Requested via the VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT and
    //   VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT bits.
    // - Sparse residency support is advertised on a finer grain via the following features:
    //   * The sparseResidencyBuffer feature provides support for creating VkBuffer objects with the
    //     VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT.
    //   * The sparseResidencyImage2D feature provides support for creating 2D single-sampled
    //     VkImage objects with VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT.
    //   * The sparseResidencyImage3D feature provides support for creating 3D VkImage objects with
    //     VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT.
    //   * The sparseResidency2Samples feature provides support for creating 2D VkImage objects with
    //     2 samples and VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT.
    //   * The sparseResidency4Samples feature provides support for creating 2D VkImage objects with
    //     ...
    //   * Implementations supporting sparseResidencyImage2D are only required to support sparse 2D,
    //     single-sampled images. Support for sparse 3D and MSAA images is optional and can be
    //     enabled via sparseResidencyImage3D, sparseResidency2Samples, sparseResidency4Samples,
    //     sparseResidency8Samples, and sparseResidency16Samples.
    // - A sparse image created using VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT supports all
    //   non-compressed color formats with power-of-two element size that non-sparse usage supports.
    //   Additional formats may also be supported and can be queried via
    //   vkGetPhysicalDeviceSparseImageFormatProperties. VK_IMAGE_TILING_LINEAR tiling is not
    //   supported.
    if ( !device_features.features.sparseResidencyImage2D ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "sparseResidencyImage2D"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

    // shaderResourceResidency is required for the fragment shader to be able to detect which parts
    //   of the image are bind to a memory allocation
    // The HLSL equivalent of the OpenGL extension GL_ARB_sparse_texture2 is the Tiled Resources
    //   feature
    if ( !device_features.features.shaderResourceResidency ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "shaderResourceResidency"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

    // The sparseResidencyAliased feature provides the following capability that can be enabled per
    //   resource:
    // - Allows physical memory ranges to be shared between multiple locations in the same sparse
    //   resource or between multiple sparse resources, with each binding of a memory location
    //   observing a consistent interpretation of the memory contents.
//    if ( !device_features.features.sparseResidencyAliased ) {
//#ifdef DEBUG_MODE
//      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
//          arr_device_props[idx_phys_dev].properties.deviceName,
//          "sparseResidencyAliased"
//          );
//#endif
//      arr_phys_devices[idx_phys_dev] = 0;
//      continue;
//    }


    // Timeline Semaphores
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

    // FIXME Why did I write this? I do not remember why
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

    // Dynamically bind a framebuffer on vkCmdBeginRenderPass
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

    // The vulkan implementation supports a VkImageMemoryBarrier for a depth/stencil image with only
    //  one of VK_IMAGE_ASPECT_DEPTH_BIT or VK_IMAGE_ASPECT_STENCIL_BIT set, and whether
    //  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
    //  VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, or VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL can
    //  be used.
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

    // Scalar Layout
    if ( !vk12_features.scalarBlockLayout ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "scalarBlockLayout"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

    // Better synchronization methods
    if ( !vk13_features.synchronization2 ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s doesn't support %s.\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          "synchronization2"
          );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
      continue;
    }

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


    // TODO Player might want to choose a specific device.
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
    }
    else if ( arr_device_props[idx_phys_dev].properties.limits.maxDrawIndirectCount <
        (1<<16) - 1 ) {
#ifdef DEBUG_MODE
      fprintf( stdout, "[Vulkan] %s's maxDrawIndirectCount is small. (%u)\n",
          arr_device_props[idx_phys_dev].properties.deviceName,
          arr_device_props[idx_phys_dev].properties.limits.maxDrawIndirectCount );
#endif
      arr_phys_devices[idx_phys_dev] = 0;
    }
    else if ( arr_device_props[idx_phys_dev].properties.limits.maxDrawIndexedIndexValue <
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
    SDL_CreateWindow( Carbonara_APP_NAME, initial_win_width, initial_win_height,
      SDL_WINDOW_RESIZABLE |
      SDL_WINDOW_HIDDEN |
      //SDL_WINDOW_BORDERLESS |
      0 );
  if ( !window ) {
    fprintf( stderr, "[Vulkan] Failed to create a window. %s\n", SDL_GetError() );
    goto error_after_window_early;
  }

  VkSurfaceKHR presentation_surface;
  if ( !SDL_Vulkan_CreateSurface( window,
        vk_instance,
        &vk_alloc_callbacks,
        &presentation_surface) ) {
    fprintf( stderr, "[Vulkan] Failed to create a vulkan surface.\n" );
    SDL_DestroyWindow( window );
error_after_window_early:
    vkDestroyInstance(vk_instance, &vk_alloc_callbacks);
    ma_free( cara_single.mem_arena, worker_state->worker_idx, arr_device_props );
    ma_free( cara_single.mem_arena,
        worker_state->worker_idx, arr_phys_devices );
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
  constexpr VkQueueFlags required_q_flags =
    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

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
          worker_state->worker_idx, arr_qfam_props );
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
    for ( uint_fast8_t q_fam_props_idx = 0;
        q_fam_props_idx < nr_q_fams[idx_phys_dev]; q_fam_props_idx++ ) {
      VkQueueFlags q_flags = arr_qfam_props[q_fam_props_idx].queueFamilyProperties.queueFlags;

      // We need a queue family that supports presentation to the specific surface created via SDL
      // for VK_QUEUE_GRAPHICS_BIT
      VkBool32 supports_presentation;
      vkGetPhysicalDeviceSurfaceSupportKHR( arr_phys_devices[idx_phys_dev],
          q_fam_props_idx, presentation_surface, &supports_presentation );
      if ( VK_QUEUE_GRAPHICS_BIT &
          arr_qfam_props[q_fam_props_idx].queueFamilyProperties.queueFlags &&
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
          worker_state->worker_idx, arr_qfam_props );
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

    ma_free(cara_single.mem_arena, worker_state->worker_idx, arr_qfam_props);
  }



  //
  // Pick a physical device to use
  //
  uint_fast8_t idx_phys_deviced_picked = UINT_FAST8_MAX;
  VkDeviceQueueCreateInfo * arr_q_create_info = nullptr;
  VkQueueFamilyProperties2 * arr_qfam_props = nullptr;
  uint_fast8_t nr_q_fams_picked = 0;
  const uint_fast8_t prefer_discrete_gpus = nr_discrete_gpus;
  constexpr unsigned int graphics = 3;
  constexpr unsigned int compute = 2;
  constexpr unsigned int transfer = 1;
  constexpr unsigned int loader = 0;
  unsigned int arr_q_fam_idx[4];
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
      ma_free( cara_single.mem_arena, worker_state->worker_idx, arr_qfam_props );
      continue;
    }

    // Populate arr_q_create_info
    unsigned int nr_graphics_q_fams_found = 0, nr_trans_q_fams_found = 0, nr_comp_q_fams_found = 0;
    constexpr unsigned int nr_memory_slots = 8;
    unsigned int q_fams_with_graphics[nr_memory_slots];
    unsigned int q_fams_with_transfer[nr_memory_slots];
    unsigned int q_fams_with_compute[nr_memory_slots];
    for ( unsigned int slot_idx = 0; slot_idx < nr_memory_slots; slot_idx++ ) {
      q_fams_with_graphics[slot_idx] = UINT_MAX_(q_fams_with_graphics[0]);
      q_fams_with_transfer[slot_idx] = UINT_MAX_(q_fams_with_transfer[0]);
      q_fams_with_compute[slot_idx] = UINT_MAX_(q_fams_with_compute[0]);
    }
    for ( uint_fast8_t q_fam_props_idx = 0;
        q_fam_props_idx < nr_q_fams[idx_phys_dev]; q_fam_props_idx++ ) {
      VkBool32 supports_presentation;
      vkGetPhysicalDeviceSurfaceSupportKHR(arr_phys_devices[idx_phys_dev],
          q_fam_props_idx, presentation_surface, &supports_presentation);
      VkQueueFlags q_flags = arr_qfam_props[q_fam_props_idx].queueFamilyProperties.queueFlags;
      // We need a queue family that supports presentation to the specific surface created via SDL
      // for VK_QUEUE_GRAPHICS_BIT
      if ( VK_QUEUE_GRAPHICS_BIT & q_flags && supports_presentation == VK_TRUE ) {
        q_fams_with_graphics[nr_graphics_q_fams_found] = q_fam_props_idx;
        nr_graphics_q_fams_found++;
        assume( nr_graphics_q_fams_found < nr_memory_slots );
      }
      if ( VK_QUEUE_COMPUTE_BIT & q_flags ) {
        q_fams_with_compute[nr_comp_q_fams_found] = q_fam_props_idx;
        nr_comp_q_fams_found++;
        assume( nr_comp_q_fams_found < nr_memory_slots );
      }
      if ( VK_QUEUE_TRANSFER_BIT & q_flags ) {
        q_fams_with_transfer[nr_trans_q_fams_found] = q_fam_props_idx;
        nr_trans_q_fams_found++;
        assume( nr_trans_q_fams_found < nr_memory_slots );
      }
    }

    for ( unsigned int i = 0; i < _arr_elem_cnt(arr_q_fam_idx); i++ ) {
      arr_q_fam_idx[i] = UINT_MAX_(arr_q_fam_idx[0]);
    }
    unsigned int loader_ded_lv = 0;
    unsigned int trans_ded_lv = 0;
    constexpr unsigned int ded_lv_highest = 3;
    constexpr unsigned int ded_lv_moderate = 2;
    constexpr unsigned int ded_lv_low = 1;
    bool try_to_make_a_separate_loader_q = true;
    // Look for a queue family with VK_QUEUE_TRANSFER_BIT capability
    if ( dedicated_qfam_availability[idx_phys_dev] & very_dedicated_transfer ) {
      loader_ded_lv = trans_ded_lv = ded_lv_highest;
look_for_very_dedicated_transfer_q_fam:
      for ( unsigned int idx = 0; idx < nr_trans_q_fams_found; idx++ ) {
        VkQueueFlags q_flags =
          arr_qfam_props[q_fams_with_transfer[idx]].queueFamilyProperties.queueFlags;
        if ( q_flags & VK_QUEUE_GRAPHICS_BIT || q_flags & VK_QUEUE_COMPUTE_BIT ) {
          continue;
        }
        if ( arr_q_fam_idx[loader] == UINT_MAX_(arr_q_fam_idx[loader]) ) {
          arr_q_fam_idx[loader] = q_fams_with_transfer[idx];
          if ( try_to_make_a_separate_loader_q &&
              arr_qfam_props[q_fams_with_transfer[idx]].queueFamilyProperties.queueCount < 2 ) {
            continue;
          }
          arr_q_fam_idx[transfer] = q_fams_with_transfer[idx];
          goto look_for_dedicated_compute_q_fam;
        }
        else {
          arr_q_fam_idx[transfer] = q_fams_with_transfer[idx];
          goto look_for_dedicated_compute_q_fam;
        }
      }
      if ( arr_q_fam_idx[transfer] == UINT_MAX_(arr_q_fam_idx[transfer]) ) {
        trans_ded_lv = ded_lv_moderate;
        goto look_for_moderately_dedicated_transfer_q_fam;
      }
    }
    else if ( dedicated_qfam_availability[idx_phys_dev] & dedicated_transfer ) {
      loader_ded_lv = trans_ded_lv = ded_lv_moderate;
look_for_moderately_dedicated_transfer_q_fam:
      for ( unsigned int idx = 0; idx < nr_trans_q_fams_found; idx++ ) {
        VkQueueFlags q_flags =
          arr_qfam_props[q_fams_with_transfer[idx]].queueFamilyProperties.queueFlags;
        if ( q_flags & VK_QUEUE_GRAPHICS_BIT ) {
          continue;
        }
        if ( arr_q_fam_idx[loader] == UINT_MAX_(arr_q_fam_idx[loader]) ) {
          arr_q_fam_idx[loader] = q_fams_with_transfer[idx];
          if ( try_to_make_a_separate_loader_q &&
              arr_qfam_props[q_fams_with_transfer[idx]].queueFamilyProperties.queueCount < 2 ) {
            continue;
          }
          arr_q_fam_idx[transfer] = q_fams_with_transfer[idx];
          goto look_for_dedicated_compute_q_fam;
        }
        else {
          if ( arr_q_fam_idx[loader] == q_fams_with_transfer[idx] &&
            arr_qfam_props[q_fams_with_transfer[idx]].queueFamilyProperties.queueCount < 2 &&
            try_to_make_a_separate_loader_q ) {
            continue;
          }
          arr_q_fam_idx[transfer] = q_fams_with_transfer[idx];
          goto look_for_dedicated_compute_q_fam;
        }
      }
      if ( arr_q_fam_idx[transfer] == UINT_MAX_(arr_q_fam_idx[transfer]) ) {
        trans_ded_lv = ded_lv_low;
        goto look_for_generalist_transfer_q_fam;
      }
    }
    else {
      loader_ded_lv = trans_ded_lv = ded_lv_low;
look_for_generalist_transfer_q_fam:
      for ( unsigned int idx = 0; idx < nr_trans_q_fams_found; idx++ ) {
        if ( arr_q_fam_idx[loader] == UINT_MAX_(arr_q_fam_idx[loader]) ) {
          arr_q_fam_idx[loader] = q_fams_with_transfer[idx];
          if ( try_to_make_a_separate_loader_q &&
              arr_qfam_props[q_fams_with_transfer[idx]].queueFamilyProperties.queueCount < 2 ) {
            continue;
          }
          arr_q_fam_idx[transfer] = q_fams_with_transfer[idx];
          goto look_for_dedicated_compute_q_fam;
        }
        else {
          if ( arr_q_fam_idx[loader] == q_fams_with_transfer[idx] &&
            arr_qfam_props[q_fams_with_transfer[idx]].queueFamilyProperties.queueCount < 2 &&
            try_to_make_a_separate_loader_q ) {
            continue;
          }
          arr_q_fam_idx[transfer] = q_fams_with_transfer[idx];
          goto look_for_dedicated_compute_q_fam;
        }
      }
      if ( arr_q_fam_idx[transfer] == UINT_MAX_(arr_q_fam_idx[transfer]) ) {
        if ( loader_ded_lv == ded_lv_highest && trans_ded_lv == ded_lv_highest ) {
          trans_ded_lv = ded_lv_moderate;
          goto look_for_moderately_dedicated_transfer_q_fam;
        }
        if ( loader_ded_lv == ded_lv_moderate && trans_ded_lv == ded_lv_moderate ) {
          trans_ded_lv = ded_lv_low;
          goto look_for_generalist_transfer_q_fam;
        }
        for ( unsigned int i = 0; i < _arr_elem_cnt(arr_q_fam_idx); i++ ) {
          arr_q_fam_idx[i] = UINT_MAX_(arr_q_fam_idx[0]);
        }
        if ( loader_ded_lv == ded_lv_highest ) {
          loader_ded_lv = trans_ded_lv = ded_lv_moderate;
          goto look_for_moderately_dedicated_transfer_q_fam;
        }
        else if ( loader_ded_lv == ded_lv_moderate ) {
          loader_ded_lv = trans_ded_lv = ded_lv_low;
          goto look_for_generalist_transfer_q_fam;
        }
        else if ( loader_ded_lv == ded_lv_low ) {
          loader_ded_lv = trans_ded_lv = ded_lv_highest;
          try_to_make_a_separate_loader_q = false;
          goto look_for_generalist_transfer_q_fam;
        }
      }
    }  // look_for_generalist_transfer_q_fam:

look_for_dedicated_compute_q_fam:
    if ( !(required_q_flags & VK_QUEUE_COMPUTE_BIT) ) {
      goto look_for_graphics_q_fam;
    }
    if ( dedicated_qfam_availability[idx_phys_dev] & dedicated_compute ) {
      // Look for a dedicated compute queue family
      for ( unsigned int idx = 0; idx < nr_comp_q_fams_found; idx++ ) {
        VkQueueFlags q_flags =
          arr_qfam_props[q_fams_with_compute[idx]].queueFamilyProperties.queueFlags;
        if ( q_flags & VK_QUEUE_GRAPHICS_BIT ) {
          continue;
        }
        if ( arr_q_fam_idx[compute] == UINT_MAX_(arr_q_fam_idx[compute]) ) {
          if ( arr_q_fam_idx[loader] == q_fams_with_compute[idx] ) {
            if ( arr_qfam_props[q_fams_with_compute[idx]].queueFamilyProperties.queueCount < 2 &&
                try_to_make_a_separate_loader_q ) {
              continue;
            }
          }
          arr_q_fam_idx[compute] = q_fams_with_compute[idx];
          goto look_for_graphics_q_fam;
        }
      }
      if ( arr_q_fam_idx[compute] == UINT_MAX_(arr_q_fam_idx[compute]) ) {
        goto look_for_generalist_compute_q_fam;
      }
    }
    else {
look_for_generalist_compute_q_fam:
      for ( unsigned int idx = 0; idx < nr_comp_q_fams_found; idx++ ) {
        if ( arr_q_fam_idx[compute] == UINT_MAX_(arr_q_fam_idx[compute]) ) {
          if ( arr_q_fam_idx[loader] == q_fams_with_compute[idx] ) {
            if ( arr_qfam_props[q_fams_with_compute[idx]].queueFamilyProperties.queueCount < 2 &&
                try_to_make_a_separate_loader_q ) {
              continue;
            }
          }
          arr_q_fam_idx[compute] = q_fams_with_compute[idx];
          goto look_for_graphics_q_fam;
        }
      }
      if ( arr_q_fam_idx[compute] == UINT_MAX_(arr_q_fam_idx[compute]) ) {
        if ( loader_ded_lv == ded_lv_highest && trans_ded_lv == ded_lv_highest ) {
          trans_ded_lv = ded_lv_moderate;
          arr_q_fam_idx[transfer] = UINT_MAX_( arr_q_fam_idx[0] );
          goto look_for_moderately_dedicated_transfer_q_fam;
        }
        if ( loader_ded_lv == ded_lv_moderate && trans_ded_lv == ded_lv_moderate ) {
          trans_ded_lv = ded_lv_low;
          arr_q_fam_idx[transfer] = UINT_MAX_( arr_q_fam_idx[0] );
          goto look_for_generalist_transfer_q_fam;
        }
        for ( unsigned int i = 0; i < _arr_elem_cnt(arr_q_fam_idx); i++ ) {
          arr_q_fam_idx[i] = UINT_MAX_(arr_q_fam_idx[0]);
        }
        if ( loader_ded_lv == ded_lv_highest ) {
          loader_ded_lv = trans_ded_lv = ded_lv_moderate;
          goto look_for_moderately_dedicated_transfer_q_fam;
        }
        else if ( loader_ded_lv == ded_lv_moderate ) {
          loader_ded_lv = trans_ded_lv = ded_lv_low;
          goto look_for_generalist_transfer_q_fam;
        }
        else if ( loader_ded_lv == ded_lv_low ) {
          loader_ded_lv = trans_ded_lv = ded_lv_highest;
          try_to_make_a_separate_loader_q = false;
          goto look_for_very_dedicated_transfer_q_fam;
        }
        else {
          fprintf( stdout, "[Vulkan] Could not choose a queue family with "
              " VK_QUEUE_COMPUTE_BIT capability from '%s'\n",
              arr_device_props[idx_phys_dev].properties.deviceName );
          // arr_phys_devices[idx_phys_dev] = 0;
          ma_free( cara_single.mem_arena, worker_state->worker_idx, arr_qfam_props );
          continue;
        }
      }
    }  // look_for_generalist_compute_q_fam:

    // Look for a queue family with VK_QUEUE_GRAPHICS_BIT capability
look_for_graphics_q_fam:
    for ( unsigned int idx = 0; idx < nr_graphics_q_fams_found; idx++ ) {
      if ( arr_q_fam_idx[loader] == q_fams_with_graphics[idx] ) {
        if ( arr_qfam_props[q_fams_with_graphics[idx]].queueFamilyProperties.queueCount < 2 &&
            try_to_make_a_separate_loader_q ) {
          continue;
        }
      }
      arr_q_fam_idx[graphics] = q_fams_with_graphics[idx];
      break;
    }

    if ( arr_q_fam_idx[graphics] == UINT_MAX_(arr_q_fam_idx[graphics]) ) {
      if ( loader_ded_lv == ded_lv_highest && trans_ded_lv == ded_lv_highest ) {
        trans_ded_lv = ded_lv_moderate;
        arr_q_fam_idx[transfer] = UINT_MAX_( arr_q_fam_idx[0] );
        arr_q_fam_idx[compute] = UINT_MAX_( arr_q_fam_idx[0] );
        arr_q_fam_idx[graphics] = UINT_MAX_( arr_q_fam_idx[0] );
        goto look_for_moderately_dedicated_transfer_q_fam;
      }
      if ( loader_ded_lv == ded_lv_moderate && trans_ded_lv == ded_lv_moderate ) {
        trans_ded_lv = ded_lv_low;
        arr_q_fam_idx[transfer] = UINT_MAX_( arr_q_fam_idx[0] );
        arr_q_fam_idx[compute] = UINT_MAX_( arr_q_fam_idx[0] );
        arr_q_fam_idx[graphics] = UINT_MAX_( arr_q_fam_idx[0] );
        goto look_for_generalist_transfer_q_fam;
      }
      for ( unsigned int i = 0; i < _arr_elem_cnt(arr_q_fam_idx); i++ ) {
        arr_q_fam_idx[i] = UINT_MAX_(arr_q_fam_idx[0]);
      }
      if ( loader_ded_lv == ded_lv_highest ) {
        loader_ded_lv = trans_ded_lv = ded_lv_moderate;
        goto look_for_moderately_dedicated_transfer_q_fam;
      }
      else if ( loader_ded_lv == ded_lv_moderate ) {
        loader_ded_lv = trans_ded_lv = ded_lv_low;
        goto look_for_generalist_transfer_q_fam;
      }
      else if ( loader_ded_lv == ded_lv_low ) {
        loader_ded_lv = trans_ded_lv = ded_lv_highest;
        try_to_make_a_separate_loader_q = false;
        goto look_for_very_dedicated_transfer_q_fam;
      }
      else {
        fprintf( stdout, "[Vulkan] Could not choose a queue family with "
            " VK_QUEUE_GRAPHICS_BIT capability from '%s'\n",
            arr_device_props[idx_phys_dev].properties.deviceName );
        // arr_phys_devices[idx_phys_dev] = 0;
        ma_free( cara_single.mem_arena, worker_state->worker_idx, arr_qfam_props );
        continue;
      }
    }

    nr_q_fams_picked = 0;
    typeof(arr_q_fam_idx) arr_uniq_q_fams;
    unsigned int nr_uniq_q_fams = 0;
    for ( unsigned int i = 0; i < _arr_elem_cnt(arr_q_fam_idx); i++ ) {
      if ( arr_q_fam_idx[i] == UINT_MAX_(arr_q_fam_idx[0]) ) {
        // We didn't even look for this capability in the first place
        continue;
      }
      for ( unsigned int j = 0; j < nr_uniq_q_fams; j++ ) {
        if ( arr_q_fam_idx[i] == arr_uniq_q_fams[j] ) {
          goto we_have_already_recorded_that_q_fam;
        }
      }
      arr_uniq_q_fams[nr_uniq_q_fams] = arr_q_fam_idx[i];
      nr_uniq_q_fams++;
we_have_already_recorded_that_q_fam:
    }
    nr_q_fams_picked = nr_uniq_q_fams;


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

    static_assert( alignof(VkDeviceQueueCreateInfo) >= alignof(float), "should be aligned." );
    arr_q_create_info = (void*) ma_malloc(
        cara_single.mem_arena->head,
        _get_nr_units_to_cover(
          sizeof(VkDeviceQueueCreateInfo) * nr_q_fams_picked,
          CACHELINE_LEN ),
        nullptr, nullptr
       );

    // Unlock the sub_arena after allocation
    atomic_store_explicit(&cara_single.mem_arena->head->lock, 0, memory_order_release);

    for ( uint_fast8_t q_create_info_idx = 0;
        q_create_info_idx < nr_q_fams_picked; q_create_info_idx++ ) {
      unsigned int nr_queues_from_same_q_fam = 0;
      for ( uint_fast8_t i = 0; i < _arr_elem_cnt(arr_q_fam_idx); i++ ) {
        if ( arr_uniq_q_fams[q_create_info_idx] == arr_q_fam_idx[i] ) {
          nr_queues_from_same_q_fam++;
        }
      }
      float priorities[] = { 1.0f, 1.0f, 1.0f, 1.0f };
      if ( arr_uniq_q_fams[q_create_info_idx] == arr_q_fam_idx[transfer] &&
          nr_queues_from_same_q_fam > 1 ) {
        constexpr typeof(priorities[0]) priority_of_asset_loader = 0.5f;
        priorities[nr_queues_from_same_q_fam - 1] = priority_of_asset_loader;
      }
      arr_q_create_info[q_create_info_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      arr_q_create_info[q_create_info_idx].pNext = 0;
      arr_q_create_info[q_create_info_idx].flags = 0;
      arr_q_create_info[q_create_info_idx].queueFamilyIndex = arr_uniq_q_fams[q_create_info_idx];
      arr_q_create_info[q_create_info_idx].queueCount = nr_queues_from_same_q_fam;
      arr_q_create_info[q_create_info_idx].pQueuePriorities = priorities;
      // `pQueuePriorities` is a pointer to an array of `queueCount` normalized floating-point
      //  values, specifying priorities of work that will be submitted to each created queue.
      //  Follow the link below to learn about the priority
      //  https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#devsandqueues-priority
    }

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
  VkPhysicalDeviceVulkan11Features vk11_features = {0};
  vk11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  VkPhysicalDeviceVulkan12Features vk12_features = {0};
  vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  VkPhysicalDeviceVulkan13Features vk13_features = {0};
  vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  device_features.pNext = &vk13_features;
  vk13_features.pNext = &vk12_features;
  vk12_features.pNext = &vk11_features;
  vk11_features.pNext = nullptr;

  vk11_features.shaderDrawParameters = VK_TRUE;

  vk12_features.timelineSemaphore = VK_TRUE;
  vk12_features.drawIndirectCount = VK_TRUE;
  vk12_features.imagelessFramebuffer = VK_TRUE;
  vk12_features.separateDepthStencilLayouts = VK_TRUE;
  vk12_features.scalarBlockLayout = VK_TRUE;

  vk12_features.descriptorIndexing = VK_TRUE;
  device_features.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
  vk12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  vk12_features.descriptorBindingPartiallyBound = VK_TRUE;
  vk12_features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
  vk12_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
  vk12_features.runtimeDescriptorArray = VK_TRUE;

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

  //device_features.features.multiDrawIndirect = VK_TRUE;

  device_features.features.samplerAnisotropy = VK_TRUE;

  device_features.features.sparseResidencyImage2D = VK_TRUE;
  device_features.features.sparseBinding = VK_TRUE;
  device_features.features.shaderResourceResidency = VK_TRUE;

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
      &device_create_info, &vk_alloc_callbacks, &new_log_dev );
      //&device_create_info, nullptr, &new_log_dev );
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
  for ( uint_fast8_t q_create_info_idx = 0;
      q_create_info_idx < nr_q_fams_picked; q_create_info_idx++ ) {
    const VkDeviceQueueCreateInfo * const q_create_info = arr_q_create_info + q_create_info_idx;
    VkQueue new_queue[4];
    for ( unsigned int q_idx = 0; q_idx < q_create_info->queueCount; q_idx++ ) {
      vkGetDeviceQueue( new_log_dev, q_create_info->queueFamilyIndex, q_idx, &new_queue[q_idx] );
    }
    unsigned int nr_queues_assigned = 0;

    if ( arr_q_create_info[q_create_info_idx].queueFamilyIndex == arr_q_fam_idx[graphics] ) {
      cara_vk.graphics_q = new_queue[nr_queues_assigned];
      cara_vk.graphics_q_fam_idx = q_create_info->queueFamilyIndex;
      nr_queues_assigned++;
    }
    if ( arr_q_create_info[q_create_info_idx].queueFamilyIndex == arr_q_fam_idx[compute] ) {
      cara_vk.compute_q = new_queue[nr_queues_assigned];
      cara_vk.compute_q_fam_idx = q_create_info->queueFamilyIndex;
      nr_queues_assigned++;
    }
    if ( arr_q_create_info[q_create_info_idx].queueFamilyIndex == arr_q_fam_idx[transfer] ) {
      cara_vk.transfer_q = new_queue[nr_queues_assigned];
      cara_vk.transfer_q_fam_idx = q_create_info->queueFamilyIndex;
      nr_queues_assigned++;
    }
    if ( arr_q_create_info[q_create_info_idx].queueFamilyIndex == arr_q_fam_idx[loader] ) {
      cara_vk.loader_q = new_queue[nr_queues_assigned];
      cara_vk.loader_q_fam_idx = q_create_info->queueFamilyIndex;
      nr_queues_assigned++;
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
    if ( nr_supported_present_modes == 0 ) {
      fprintf( stderr, "(nr_supported_present_modes=%u)\n", nr_supported_present_modes );
    }
    else {
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

  // VK_PRESENT_MODE_FIFO_KHR is a v-synced mode and the only mode guaranteed to be available
  //   everywhere.
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
      worker_state->worker_idx, arr_available_present_modes );

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
  }
  else if ( nr_swapchain_imgs < surface_capabilities.minImageCount ) {
    nr_swapchain_imgs = surface_capabilities.minImageCount;
  }

  VkExtent2D image_size = {
    .width = initial_win_width,
    .height = initial_win_height,
  };
  if ( surface_capabilities.currentExtent.width == 0xffffffff ) {
    if ( image_size.width < surface_capabilities.minImageExtent.width ) {
      image_size.width = surface_capabilities.minImageExtent.width;
    }
    else if ( image_size.width > surface_capabilities.maxImageExtent.width ) {
      image_size.width = surface_capabilities.maxImageExtent.width;
    }
  }
  if ( surface_capabilities.currentExtent.height == 0xffffffff ) {
    if ( image_size.height < surface_capabilities.minImageExtent.height ) {
      image_size.height = surface_capabilities.minImageExtent.height;
    }
    else if ( image_size.height > surface_capabilities.maxImageExtent.height ) {
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
    }
    else {
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
        worker_state->worker_idx, arr_supported_surface_formats );
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


  // The combination of the 4 component color format VK_FORMAT_B8G8R8A8_SRGB with a non-linear(sRGB)
  //   colorspace VK_COLORSPACE_SRGB_NONLINEAR_KHR is guaranteed to be available everywhere.
  constexpr VkFormat desired_img_formats_for_present[] = {
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_B8G8R8_SRGB,
    VK_FORMAT_R8G8B8_SRGB,
  };
  constexpr VkColorSpaceKHR desired_color_spaces[] = {
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
  };
  VkFormat present_img_fmt = desired_img_formats_for_present[0];
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
          present_img_fmt = surface_format->format;
          color_space = surface_format->colorSpace;
          idx_supported_format = UINT_FAST8_MAX;
          goto end_of_nested_loop;
        }

      }  // for ( idx_supported_format = 0;
         //     idx_supported_format < nr_supported_surface_formats; idx_supported_format++ )

    }  // for ( uint_fast8_t idx_pair = 0;
       //     idx_pair < _arr_elem_cnt(desired_img_formats_for_present); idx_pair++ )
end_of_nested_loop:

    if ( idx_supported_format != UINT_FAST8_MAX ) {
      fprintf( stderr,
          "[Vulkan] Could not find a compatible surface format and colorspace supported "
          "by the newly created presentation surface on the selected GPU and screen.\n" );
      ma_free( cara_single.mem_arena,
          worker_state->worker_idx, arr_supported_surface_formats );
      goto error_after_logical_device;
    }
  }
  ma_free( cara_single.mem_arena,
      worker_state->worker_idx, arr_supported_surface_formats );

#ifdef DEBUG_MODE
  fprintf( stdout, "[Vulkan] Going with {colorSpace = %#08x + format = %#08x}\n",
      color_space, present_img_fmt );
#endif


  VkSwapchainKHR swapchain, old_swapchain;
  old_swapchain = VK_NULL_HANDLE;
  //if ( old_swapchain != VK_NULL_HANDLE ) {
  //  vkDestroySwapchainKHR( new_log_dev, old_swapchain, &cara_vk.vulkan_alloc_callbacks );
  //  old_swapchain = VK_NULL_HANDLE;
  //}

  VkSwapchainCreateInfoKHR swapchain_create_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .surface = presentation_surface,
    .minImageCount = nr_swapchain_imgs,
    .imageFormat = present_img_fmt,
    .imageColorSpace = color_space,
    .imageExtent = image_size,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .preTransform = surface_capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = chosen_present_mode,
    .clipped = VK_TRUE,  // Applications should set this value to VK_TRUE if they do not expect to
                         // read back the content of presentable images before presenting them or
                         // after reacquiring them, and if their fragment shaders do not have any
                         // side effects that require them to run for all pixels in the presentable
                         // image.
    .oldSwapchain = old_swapchain,
  };

  result = vkCreateSwapchainKHR(
      new_log_dev,
      &swapchain_create_info,
      &vk_alloc_callbacks,
      &swapchain );
  if ( result != VK_SUCCESS || swapchain == VK_NULL_HANDLE ) {
    fprintf( stderr, "[Vulkan] Could not create a swapchain.\n" );
    goto error_after_logical_device;
  }

  cara_vk.nr_swapchain_imgs_we_request = nr_swapchain_imgs;
  result = vkGetSwapchainImagesKHR( new_log_dev, swapchain, &nr_swapchain_imgs, nullptr );
  if ( result != VK_SUCCESS || nr_swapchain_imgs == 0 ) {
    fprintf( stderr, "[Vulkan] Could not get the number of swapchain images.\n" );
    goto error_after_logical_device;
  }
#ifdef DEBUG_MODE
  fprintf( stdout, "[Vulkan] The number of vulkan images in the swapchain is %u.\n",
      nr_swapchain_imgs );
#endif

  VkImage * arr_swapchain_images;
  VkImageView * arr_swapchain_img_views;
  _carbonara_malloc_array( &worker_state->malloc_helper, &arr_swapchain_images, nr_swapchain_imgs );
  _carbonara_malloc_array( &worker_state->malloc_helper,
      &arr_swapchain_img_views, nr_swapchain_imgs );

  result = vkGetSwapchainImagesKHR( new_log_dev, swapchain,
      &nr_swapchain_imgs, arr_swapchain_images );
  if ( result != VK_SUCCESS && result != VK_INCOMPLETE ) {
    fprintf( stderr, "[Vulkan] Could not enumerate swapchain images.\n" );
    goto error_after_swapchain_images;
  }

  // Create VkImageViews for swapchain images
  VkImageViewCreateInfo iv_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .format = present_img_fmt,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .components = (VkComponentMapping) {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY
    },
    .subresourceRange = (VkImageSubresourceRange) {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    },
  };
  for ( unsigned int sci_idx = 0; sci_idx < nr_swapchain_imgs; sci_idx++ ) {
    arr_swapchain_img_views[sci_idx] = VK_NULL_HANDLE;
  }

  TRACE_BEGIN("vk_create_swapchain_ivs");
  for ( unsigned int sci_idx = 0; sci_idx < nr_swapchain_imgs; sci_idx++ ) {
    iv_create_info.image = arr_swapchain_images[sci_idx];
    result = vkCreateImageView( new_log_dev, &iv_create_info, &vk_alloc_callbacks,
        &arr_swapchain_img_views[sci_idx] );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImageView. '%s'\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }
  }
  TRACE_END();



  //
  // Prepare command buffers
  //

  // Prepare graphics command buffers  (1 buffer per 1 subpass in the first renderpass)
  cara_vk.arr_graphics_cmdbuf_sets[0].arr_cmd_pools =
    cara_malloc( &worker_state->malloc_helper,
      alignof(VkCommandPool),
      cara_max_nr_fif * (
        sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
        sizeof(VkCommandBuffer) *
        (cara_nr_subpasses * cara_single.proc_map->nr_processors) )
      );
  memset( cara_vk.arr_graphics_cmdbuf_sets[0].arr_cmd_pools,
      0,
      cara_max_nr_fif * (
        sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
        sizeof(VkCommandBuffer) *
        (cara_nr_subpasses * cara_single.proc_map->nr_processors) )
      );
  for ( unsigned int cmd_center_idx = 1;
      cmd_center_idx < cara_max_nr_fif; cmd_center_idx++ ) {
    cara_vk.arr_graphics_cmdbuf_sets[cmd_center_idx].arr_cmd_pools =
      cara_vk.arr_graphics_cmdbuf_sets[cmd_center_idx-1].arr_cmd_pools +
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
  cmd_pool_create_info.queueFamilyIndex = cara_vk.graphics_q_fam_idx;

  // Prepare a template
  VkCommandBufferAllocateInfo cmd_buf_alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
  };
  for ( uint_fast8_t fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;  // - Primary command buffers can
                                                        // be directly submitted to queues. They can
                                                        // also execute (call) secondary command
                                                        // buffers.
                                                        // - Secondary command buffers can only be
                                                        // executed from primary command buffers,
                                                        // and we are not allowed to submit them.
    cmd_buf_alloc_info.commandBufferCount = cara_nr_subpasses;

    for ( uint_fast8_t worker_idx = 0;
        worker_idx < cara_single.proc_map->nr_processors; worker_idx++ ) {
      result = vkCreateCommandPool(
          new_log_dev,
          &cmd_pool_create_info,
          &vk_alloc_callbacks,
          &cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[worker_idx]
          );
      if ( result != VK_SUCCESS ) {
        fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool for graphics commands. "
            "(%s)\n", string_VkResult(result) );
        goto error_after_swapchain_images;
      }

      cmd_buf_alloc_info.commandPool = cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[worker_idx];
      result = vkAllocateCommandBuffers(
          new_log_dev,
          &cmd_buf_alloc_info,
          cara_get_arr_cmd_bufs_for_graphics(cara_single.proc_map->nr_processors, fif_idx) +
            worker_idx * cara_nr_subpasses
          );
      if ( result != VK_SUCCESS ) {
        fprintf( stderr, "[Vulkan] Failed to allocate command buffers. (%s)\n",
            string_VkResult(result) );
        goto error_after_swapchain_images;
      }
#ifdef DEBUG_MODE
      fprintf( stderr, "[Vulkan-Debug] G fb_idx=%u worker_idx=%u new_cmd_buf=%#018lx\n",
          fif_idx,
          worker_idx,
          (uintptr_t)( cara_get_arr_cmd_bufs_for_graphics(
              cara_single.proc_map->nr_processors, fif_idx )[worker_idx * cara_nr_subpasses] )
          );
#endif
    }

    // Prepare one primary command buffer for graphics commands
    result = vkCreateCommandPool(
        new_log_dev,
        &cmd_pool_create_info,
        &vk_alloc_callbacks,
        &cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[
          cara_single.proc_map->nr_processors ]
        );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool. (%s)\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }

    cmd_buf_alloc_info.commandBufferCount = 1;
    cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buf_alloc_info.commandPool =
      cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[cara_single.proc_map->nr_processors];

    result = vkAllocateCommandBuffers(
        new_log_dev,
        &cmd_buf_alloc_info,
        &cara_vk.arr_graphics_cmdbuf_sets[fif_idx].primary_cmd_buf
        );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Failed to allocate a command buffer. (%s)\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }
#ifdef DEBUG_MODE
    fprintf( stderr, "[Vulkan-Debug] GP fb_idx=%u new_cmd_buf=%#018lx\n",
        fif_idx,
        (uintptr_t)cara_vk.arr_graphics_cmdbuf_sets[fif_idx].primary_cmd_buf );
#endif

  }  // for ( uint_fast8_t fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ )


  for ( uint_fast8_t set_idx = 0;
      set_idx < nr_al_cmd_pool_sets + cara_max_nr_fif;
      set_idx++ ) {
    // Prepare transfer command buffers
    cmd_pool_create_info.queueFamilyIndex = set_idx < nr_al_cmd_pool_sets ?
      cara_vk.loader_q_fam_idx : cara_vk.transfer_q_fam_idx;

    static_assert( alignof(VkCommandPool) >= alignof(VkCommandBuffer),
        "should be aligned in order." );
    cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools =
      cara_malloc( &worker_state->malloc_helper,
        alignof(VkCommandPool),
        sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
        sizeof(VkCommandBuffer) * cara_single.proc_map->nr_processors );
    memset( cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools,
        0,
        sizeof(VkCommandPool) * (1 + cara_single.proc_map->nr_processors) +
        sizeof(VkCommandBuffer) * cara_single.proc_map->nr_processors );

    for ( uint_fast8_t worker_idx = 0;
        worker_idx < cara_single.proc_map->nr_processors; worker_idx++ ) {
      result = vkCreateCommandPool(
          new_log_dev,
          &cmd_pool_create_info,
          &vk_alloc_callbacks,
          &cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[worker_idx]
          );
      if ( result != VK_SUCCESS ) {
        fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool for transfer commands."
            "(%s)\n", string_VkResult(result) );
        goto error_after_swapchain_images;
      }

      cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
      cmd_buf_alloc_info.commandBufferCount = 1;
      cmd_buf_alloc_info.commandPool =
        cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[worker_idx];
      result = vkAllocateCommandBuffers(
          new_log_dev,
          &cmd_buf_alloc_info,
          cara_get_arr_cmd_bufs_for_transfer(cara_single.proc_map->nr_processors, set_idx) +
            worker_idx
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
        &vk_alloc_callbacks,
        &cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cara_single.proc_map->nr_processors]
        );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool. (%s)\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }

    cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buf_alloc_info.commandBufferCount = 1;
    cmd_buf_alloc_info.commandPool =
      cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cara_single.proc_map->nr_processors];

    result = vkAllocateCommandBuffers(
        new_log_dev,
        &cmd_buf_alloc_info,
        &cara_vk.arr_trans_cmdbuf_sets[set_idx].primary_cmd_buf
        );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Vulkan] Failed to allocate a command buffer. (%s)\n",
          string_VkResult(result) );
      goto error_after_swapchain_images;
    }
  }  // for ( uint_fast8_t set_idx = 0; set_idx < cara_max_nr_fif; set_idx++ )


  //for ( unsigned int fif_idx = 1; fif_idx < cara_max_nr_fif; fif_idx++ ) {
  //  cmd_pool_create_info.queueFamilyIndex = cara_vk.graphics_q_fam_idx;
  //  result = vkCreateCommandPool(
  //      new_log_dev,
  //      &cmd_pool_create_info,
  //      &vk_alloc_callbacks,
  //      &cara_vk.dummy_pool[fif_idx]
  //      );
  //  if ( result != VK_SUCCESS || cara_vk.dummy_pool[fif_idx] == VK_NULL_HANDLE ) {
  //    fprintf( stderr, "[Vulkan] Could not prepare a Vulkan command pool. (%s)\n",
  //        string_VkResult(result) );
  //    goto error_after_swapchain_images;
  //  }
  //  cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  //  cmd_buf_alloc_info.commandBufferCount = 1;
  //  cmd_buf_alloc_info.commandPool = cara_vk.dummy_pool[fif_idx];
  //  result = vkAllocateCommandBuffers(
  //      new_log_dev,
  //      &cmd_buf_alloc_info,
  //      &cara_vk.dummy_cmdbuf[fif_idx]
  //      );
  //  if ( result != VK_SUCCESS ) {
  //    fprintf( stderr, "[Vulkan] Failed to allocate a command buffer. (%s)\n",
  //        string_VkResult(result) );
  //    goto error_after_swapchain_images;
  //  }
  //}


  //
  // Prepare a few sign bearers
  //

  // Timeline semaphores
  constexpr VkSemaphoreTypeCreateInfo sempho_type_create_info = {
    VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
    nullptr,
    VK_SEMAPHORE_TYPE_TIMELINE,
    0
  };
  VkSemaphoreCreateInfo sempho_create_info;
  sempho_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sempho_create_info.pNext = &sempho_type_create_info;
  sempho_create_info.flags = 0;

  VkSemaphore semaphore;
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &sempho_create_info,
        &vk_alloc_callbacks,
        //nullptr,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (main_semaphore_before_A)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_semphos_tr[fif_idx] = semaphore;
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &sempho_create_info,
        &vk_alloc_callbacks,
        //nullptr,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (main_semaphore_after_A)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_semphos_gr[fif_idx] = semaphore;
  }

  // We need binary semaphores for WSI functions
  sempho_create_info.pNext = nullptr;
  cara_vk.arr_sc_present_semphos = cara_malloc( &worker_state->malloc_helper,
        alignof(VkSemaphore), sizeof(VkSemaphore) * nr_swapchain_imgs );
  if ( !cara_vk.arr_sc_present_semphos ) {
    fprintf( stderr, "[Vulkan] %s\n", arr_emsg_txt[emsg_out_of_memory] );
    goto error_after_swapchain_images;
  }
  memset( cara_vk.arr_sc_present_semphos, 0, sizeof(VkSemaphore) * nr_swapchain_imgs );
  for ( unsigned int swapchain_img_idx = 0;
      swapchain_img_idx < nr_swapchain_imgs; swapchain_img_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &sempho_create_info,
        &vk_alloc_callbacks,
        //nullptr,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (wsi_semaphore_present)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_sc_present_semphos[swapchain_img_idx] = semaphore;
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    result = vkCreateSemaphore( new_log_dev,
        &sempho_create_info,
        &vk_alloc_callbacks,
        //nullptr,
        &semaphore
        );
    if ( result != VK_SUCCESS || !semaphore ) {
      fprintf( stderr, "[Vulkan] Failed to create a semaphore. (%s)\n", string_VkResult(result) );
      fprintf( stderr, "[Vulkan] (wsi_semaphore_acquire)\n" );
      goto error_after_swapchain_images;
    }
    cara_vk.arr_sc_acquire_semphos[fif_idx] = semaphore;
  }

  VkPhysicalDeviceMemoryProperties2 memory_properties;
  memory_properties.pNext = nullptr;
  memory_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  vkGetPhysicalDeviceMemoryProperties2(
      arr_phys_devices[idx_phys_deviced_picked], &memory_properties );
  if ( ma_init_vk_mem_mgr(&cara_vk.vmm, &memory_properties,
        &arr_device_props[idx_phys_deviced_picked].properties.limits,
        &worker_state->malloc_helper, cara_malloc) != emsg_no_error ) {
    cara_vk.vmm.arr_sizes = nullptr;
    goto error_after_swapchain_images;
  }

  if ( !infos_al_create_prealloc(&cara_vk.old_swapchains,
        &vk_alloc_callbacks, vk_malloc_wrapper,
        4096,
        _align_to_the_close_power_of_two(sizeof(struct dead_swapchain)),
        true) ) {
    fprintf( stderr, "[Vulkan] A failure occurred in infos_al_create_prealloc.\n" );
    goto error_after_swapchain_images;
  }



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
    else if ( !(format_props.formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        ) {
      all_formats_are_supported = false;
    }
#endif
    // TODO Check what we do in the s3tc format path below and copy them
  }
  all_formats_are_supported = false;

  if ( all_formats_are_supported ) {
    cara_vk.tex_fmt_chosen = tf_astc;
  }
  else {
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
    all_formats_are_supported = true;
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
        all_formats_are_supported = false;
      }
#ifdef TEXTURE_BILINEAR_FILTERING
      else if ( !(format_props.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ) {
        all_formats_are_supported = false;
      }
#endif
      else {  // TODO This code should be in ASTC path too? or we never care about mobile devices
        uint32_t nr_elems;
        vkGetPhysicalDeviceSparseImageFormatProperties( arr_phys_devices[idx_phys_deviced_picked],
            s3tc_formats_used[idx_fmt], VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_OPTIMAL, &nr_elems, nullptr );
        VkSparseImageFormatProperties arr_sif_props[nr_elems];
        vkGetPhysicalDeviceSparseImageFormatProperties( arr_phys_devices[idx_phys_deviced_picked],
            s3tc_formats_used[idx_fmt], VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_OPTIMAL, &nr_elems, arr_sif_props );
        bool found_props_for_color_aspect = false;
        for ( typeof(nr_elems) i = 0; i < nr_elems; i++ ) {
          auto aspect_mask = arr_sif_props[i].aspectMask;
          [[maybe_unused]] auto flags = arr_sif_props[i].flags;
          [[maybe_unused]] auto sparse_img_block_dims = arr_sif_props[i].imageGranularity;
#if defined(DEBUG_MODE)
          fprintf( stdout, "[Vulkan-Debug] arr_sif_props[%u] aspect_mask = %u, flags = %#010x,"
              "sparse_img_block_dims = { w:%u h:%u d:%u }\n", i, aspect_mask, flags,
              sparse_img_block_dims.width, sparse_img_block_dims.height, sparse_img_block_dims.depth
              );
#endif
          if ( (aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT) == 0 ) {
#if defined(DEBUG_MODE)
            fprintf( stdout, "[Vulkan-Debug] skipping a VkSparseImageFormatProperties with its"
                "aspectMask marked zero on VK_IMAGE_ASPECT_COLOR_BIT\n" );
#endif
            continue;
          }
          found_props_for_color_aspect = true;
        }
        if ( !found_props_for_color_aspect ) {
          all_formats_are_supported = false;
        }
      }  // if ( all_formats_are_supported )

    }  // for ( uint_fast8_t idx_fmt = 0; idx_fmt < _arr_elem_cnt(s3tc_formats_used); idx_fmt++ )

    if ( all_formats_are_supported ) {
      cara_vk.tex_fmt_chosen = tf_s3tc;
    }
    else {
      fprintf( stderr,
          "[Vulkan] %s doesn't support optimal operations on compressed texture data.\n",
          arr_device_props[idx_phys_deviced_picked].properties.deviceName );
error_after_swapchain_images:
      for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
        if( cara_vk.arr_sc_acquire_semphos[fif_idx] != VK_NULL_HANDLE ) {
          vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_sc_acquire_semphos[fif_idx], nullptr );
          cara_vk.arr_sc_acquire_semphos[fif_idx] = VK_NULL_HANDLE;
        }
      }
      if( cara_vk.arr_sc_present_semphos != nullptr ) {
        for ( unsigned int swapchain_img_idx = 0;
            swapchain_img_idx < nr_swapchain_imgs; swapchain_img_idx++ ) {
          if( cara_vk.arr_sc_present_semphos[swapchain_img_idx] != VK_NULL_HANDLE ) {
            vkDestroySemaphore( cara_vk.log_dev,
                cara_vk.arr_sc_present_semphos[swapchain_img_idx],
                nullptr );
            cara_vk.arr_sc_present_semphos[swapchain_img_idx] = VK_NULL_HANDLE;
          }
        }
        ma_free( cara_single.mem_arena, worker_state->worker_idx,
            cara_vk.arr_sc_present_semphos );
        cara_vk.arr_sc_present_semphos = nullptr;
      }
      for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
        if( cara_vk.arr_semphos_tr[fif_idx] != VK_NULL_HANDLE ) {
          vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_semphos_tr[fif_idx], nullptr );
          cara_vk.arr_semphos_tr[fif_idx] = VK_NULL_HANDLE;
        }
        if( cara_vk.arr_semphos_gr[fif_idx] != VK_NULL_HANDLE ) {
          vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_semphos_gr[fif_idx], nullptr );
          cara_vk.arr_semphos_gr[fif_idx] = VK_NULL_HANDLE;
        }
      }
      ma_free( cara_single.mem_arena,
          worker_state->worker_idx,
          arr_swapchain_images );
      if ( arr_swapchain_img_views ) {
        for ( unsigned int i = 0; i < nr_swapchain_imgs; i++ ) {
          if ( arr_swapchain_img_views[i] != VK_NULL_HANDLE ) {
            vkDestroyImageView( cara_vk.log_dev, arr_swapchain_img_views[i], &vk_alloc_callbacks );
          }
        }
        ma_free( cara_single.mem_arena,
            worker_state->worker_idx,
            arr_swapchain_img_views );
      }
error_after_logical_device:
      for ( uint_fast8_t set_idx = 0;
          set_idx < nr_al_cmd_pool_sets + cara_max_nr_fif;
          set_idx++ ) {
        if ( cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools ) {
          for ( uint_fast8_t cmd_pool_idx = 0;
              cmd_pool_idx <= cara_single.proc_map->nr_processors; cmd_pool_idx++ ) {
            if ( cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cmd_pool_idx] ) {
              vkDestroyCommandPool( cara_vk.log_dev,
                  cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cmd_pool_idx],
                  &vk_alloc_callbacks
                  );

              cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cmd_pool_idx] = 0;
            }
          }
          ma_free( cara_single.mem_arena, worker_state->worker_idx,
              cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools );
          cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools = nullptr;
        }
      }

      for ( uint_fast8_t fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
        if ( cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools ) {
          for ( uint_fast8_t cmd_pool_idx = 0;
              cmd_pool_idx <= cara_single.proc_map->nr_processors; cmd_pool_idx++ ) {
            if ( cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[cmd_pool_idx] ) {
              vkDestroyCommandPool( new_log_dev,
                  cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[cmd_pool_idx],
                  &vk_alloc_callbacks
                  );

              cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[cmd_pool_idx] = 0;
            }
          }
          ma_free( cara_single.mem_arena, worker_state->worker_idx,
              cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools );
          cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools = nullptr;
        }

        vkDestroyCommandPool( cara_vk.log_dev,
            cara_vk.dummy_pool[fif_idx],
            &vk_alloc_callbacks
            );
      }

      vkDestroyDevice(
          new_log_dev,
          &vk_alloc_callbacks
          //nullptr
          );
error_after_arr_q_create_info:
      ma_free( cara_single.mem_arena,
          worker_state->worker_idx,
          arr_q_create_info );
      ma_free( cara_single.mem_arena,
          worker_state->worker_idx,
          arr_qfam_props );
error_after_presentation_surface:
      //vkDestroySurfaceKHR( vk_instance, presentation_surface,
      //    &cara_vk.vulkan_alloc_callbacks );
      SDL_Vulkan_DestroySurface(
          vk_instance,
          presentation_surface,
          &vk_alloc_callbacks
          //nullptr
          );
      SDL_DestroyWindow(window);
      //ma_free(cara_single.mem_arena, worker_state->worker_info->idx_self, window);
      vkDestroyInstance(
          vk_instance,
          &vk_alloc_callbacks
          //nullptr
          );
      ma_free(cara_single.mem_arena, worker_state->worker_idx, arr_device_props);
      ma_free( cara_single.mem_arena,
          worker_state->worker_idx, arr_phys_devices );
      //unload_shared_lib( vulkan_library );
      return 1;
    }
  }

  // Choose a depth buffer format
  VkFormat arr_zbuf_cands[] = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,  // VK_FORMAT_D24_UNORM_S8_UINT is a mandatory format, meaning it's
                                  //   supported on every GPU supporting Vulkan.
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
      chosen_fmt = arr_zbuf_cands[idx_fmt];
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
        chosen_fmt = arr_zbuf_cands[idx_fmt];
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
  cara_vk.arr_swapchain_iv = arr_swapchain_img_views;
  cara_vk.max_win_size.parts.width = image_size.width;
  cara_vk.max_win_size.parts.height = image_size.height;
  cara_vk.max_win_size_avail.parts.width = surface_capabilities.maxImageExtent.width;
  cara_vk.max_win_size_avail.parts.height = surface_capabilities.maxImageExtent.height;
  cara_vk.vk_surface_transform_flags = surface_capabilities.currentTransform;
  cara_vk.present_mode = chosen_present_mode;
  cara_vk.max_anisotropy = min(
      arr_device_props[idx_phys_deviced_picked].properties.limits.maxSamplerAnisotropy, 16.0f );
  cara_vk.swapchain_img_fmt = present_img_fmt;
  cara_vk.swapchain_color_space = color_space;
  cara_vk.presentation_surface = presentation_surface;


  memcpy( &cara_vk.desc_indexing_props, &arr_descriptor_indexing_props[idx_phys_deviced_picked],
      sizeof(VkPhysicalDeviceDescriptorIndexingPropertiesEXT) );
#if defined(DEBUG_MODE)
  fprintf( stdout, "[Vulkan-Debug] limits related to descriptor indexing:\n" );

  // Similar to maxDescriptorSetSampledImages but counts descriptors from descriptor sets created
  // with or without the VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT bit set.
  // maxDescriptorSetSampledImages is the maximum number of sampled images that can be included in a
  // pipeline layout. Descriptors with a type of VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
  // VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, or VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER count against this
  // limit.
  fprintf( stdout, "  maxDescriptorSetUpdateAfterBindSampledImages: %u\n",
      cara_vk.desc_indexing_props.maxDescriptorSetUpdateAfterBindSampledImages );

  // A boolean value indicating whether implicit LOD calculations for image operations have
  // well-defined results when the image and/or sampler objects used for the instruction are not
  // uniform within a quad.
  fprintf( stdout, "  quadDivergentImplicitLod: %u\n",
      cara_vk.desc_indexing_props.quadDivergentImplicitLod );

  //fprintf( stdout, "  shaderSampledImageArrayNonUniformIndexingNative: %u\n",
  //    cara_vk.desc_indexing_props.shaderSampledImageArrayNonUniformIndexingNative );

  // maxPerStageUpdateAfterBindResources is similar to maxPerStageResources but counts descriptors
  // from descriptor sets created with or without the
  // VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT bit set.
  // maxPerStageResources is the maximum number of resources that can be accessible to a single
  // shader stage in a pipeline layout.
  fprintf( stdout, "  maxPerStageUpdateAfterBindResources: %u\n",
      cara_vk.desc_indexing_props.maxPerStageUpdateAfterBindResources );

  // The maximum number of descriptors (summed over all descriptor types) that can be created across
  // all pools that are created with the VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT bit set.
  // Pool creation may fail when this limit is exceeded, or when the space this limit represents is
  // unable to satisfy a pool creation due to fragmentation.
  fprintf( stdout, "  maxUpdateAfterBindDescriptorsInAllPools: %u\n",
      cara_vk.desc_indexing_props.maxUpdateAfterBindDescriptorsInAllPools );
#endif


  ma_free(cara_single.mem_arena, worker_state->worker_idx, arr_q_create_info);
  ma_free(cara_single.mem_arena, worker_state->worker_idx, arr_qfam_props);
  ma_free(cara_single.mem_arena, worker_state->worker_idx, arr_device_props);
  ma_free(cara_single.mem_arena, worker_state->worker_idx, arr_phys_devices);
  //atomic_store_explicit( &cara_vk.tmp_var_ready, -7, memory_order_release);

  return 0;
}




//-------------------------------------------------------------------------------------------------
void destroy_vk_objs( _typeof_member(struct ms_worker_info, idx_self) worker_idx )
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

  for ( unsigned int i = 0; i < cara_vk.nr_old_swapchains; i++ ) {
    struct dead_swapchain * p_dead = infos_al_get_elem_unsafe( &cara_vk.old_swapchains, i );

    vkDestroySwapchainKHR( cara_vk.log_dev, p_dead->swapchain, &vk_alloc_callbacks );

    // Destroy the old framebuffers and replace them with the newly created ones
    for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
      for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
        // vkDestroyFramebuffer
        if ( p_dead->arr_framebuffers[fif_idx][rp_idx] ) {
          vkDestroyFramebuffer( cara_vk.log_dev,
              p_dead->arr_framebuffers[fif_idx][rp_idx], &vk_alloc_callbacks );
          //p_dead->arr_framebuffers[fif_idx][rp_idx] = VK_NULL_HANDLE;
        }
      }
    }

    for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
      for ( unsigned int fb_idx = 0; fb_idx <= fb_cbuf; fb_idx++ ) {
        if ( p_dead->arr_zbuf_n_cbuf[fif_idx][fb_idx] == VK_NULL_HANDLE ) {
          continue;
        }
        //if ( p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx].malloc == VK_NULL_HANDLE ) {
        //  continue;
        //}

        // Call vkDestroyImage
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements( cara_vk.log_dev,
            p_dead->arr_zbuf_n_cbuf[fif_idx][fb_idx], &req );
        VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
        preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        required_props = 0;
        tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        enum err_msg err = ma_report_vk_malloc_rel( &cara_vk.vmm,
            &vk_alloc_callbacks, vk_malloc_wrapper,
            ma_pick_best_mtype(&cara_vk.vmm, &req,
              required_props, preferred_props, tolerable_props),
            p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx] );
        //enum err_msg err = ma_report_vk_malloc_rel( &cara_vk.vmm,
        //    &vk_alloc_callbacks, vk_malloc_wrapper,
        //    p_dead->mtype[fif_idx][fb_idx],
        //    p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx] );
        if ( err ) {
          fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d "
              "'%s'\n", __FILE__, __LINE__, arr_emsg_txt[err] );
          switch ( err ) {
            case emsg_already_done:
              [[fallthrough]];
            case emsg_unknown:  // VkDeviceMemory object is somehow missing
              vkDestroyImage( cara_vk.log_dev,
                  p_dead->arr_zbuf_n_cbuf[fif_idx][fb_idx], &vk_alloc_callbacks );
              p_dead->arr_zbuf_n_cbuf[fif_idx][fb_idx] = VK_NULL_HANDLE;
              p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx].range = invalid_range;
              p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
              break;
            default:
              break;
          }
        }  // if ( err )
        else {
          vkDestroyImage( cara_vk.log_dev,
              p_dead->arr_zbuf_n_cbuf[fif_idx][fb_idx], &vk_alloc_callbacks );
          p_dead->arr_zbuf_n_cbuf[fif_idx][fb_idx] = VK_NULL_HANDLE;
          p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx].range = invalid_range;
          p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
        }

        // vkDestroyImageView as well
        if ( p_dead->arr_zbuf_n_cbuf_iv[fif_idx][fb_idx] != VK_NULL_HANDLE ) {
          vkDestroyImageView( cara_vk.log_dev,
              p_dead->arr_zbuf_n_cbuf_iv[fif_idx][fb_idx], &vk_alloc_callbacks );
          p_dead->arr_zbuf_n_cbuf_iv[fif_idx][fb_idx] = VK_NULL_HANDLE;
        }

      }
    }

    struct dead_swapchain * p_last = infos_al_get_elem_unsafe(
        &cara_vk.old_swapchains, cara_vk.nr_old_swapchains - 1 );
    *p_dead = *p_last;
    i--;
    cara_vk.nr_old_swapchains--;

  }

  infos_al_destroy_except_preallocated_struct( &cara_vk.old_swapchains,
      &vk_alloc_callbacks, vk_free_wrapper );

  if ( cara_vk.sm_simple_frag ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_simple_frag, &vk_alloc_callbacks );
    cara_vk.sm_simple_frag = VK_NULL_HANDLE;
  }
  if ( cara_vk.sm_simple_vert ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_simple_vert, &vk_alloc_callbacks );
    cara_vk.sm_simple_vert = VK_NULL_HANDLE;
  }
  if ( cara_vk.sm_z_only ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_z_only, &vk_alloc_callbacks );
    cara_vk.sm_z_only = VK_NULL_HANDLE;
  }
  if ( cara_vk.sm_z_only_frag ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_z_only_frag,
        &vk_alloc_callbacks );
    cara_vk.sm_z_only_frag = VK_NULL_HANDLE;
  }
  if ( cara_vk.sm_cp2sc_vert ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_cp2sc_vert, &vk_alloc_callbacks );
    cara_vk.sm_cp2sc_vert = VK_NULL_HANDLE;
  }
  if ( cara_vk.sm_cp2sc_frag ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_cp2sc_frag, &vk_alloc_callbacks );
    cara_vk.sm_cp2sc_frag = VK_NULL_HANDLE;
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
      // vkDestroyFramebuffer
      if ( cara_vk.arr_framebuffers[fif_idx][rp_idx] ) {
        vkDestroyFramebuffer( cara_vk.log_dev,
            cara_vk.arr_framebuffers[fif_idx][rp_idx], &vk_alloc_callbacks );
        cara_vk.arr_framebuffers[fif_idx][rp_idx] = VK_NULL_HANDLE;
      }
    }
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int fb_idx = fb_zbuf; fb_idx <= fb_cbuf; fb_idx++ ) {
      // vkDestroyImage
      if ( cara_vk.zbuf_n_cbuf[fif_idx][fb_idx] == VK_NULL_HANDLE ) {
        continue;
      }
      // Should I just write down the memory type when binding to a memory object?
      VkMemoryRequirements req;
      vkGetImageMemoryRequirements( cara_vk.log_dev, cara_vk.zbuf_n_cbuf[fif_idx][fb_idx], &req );
      VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
      preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      required_props = 0;
      tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      enum err_msg err = ma_report_vk_malloc_rel( &cara_vk.vmm,
          &vk_alloc_callbacks, vk_malloc_wrapper,
          ma_pick_best_mtype(&cara_vk.vmm, &req, required_props, preferred_props, tolerable_props),
          cara_vk.mbi_zbuf_n_cbuf[fif_idx][fb_idx] );
      if ( err ) {
        fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d "
            "'%s'\n", __FILE__, __LINE__, arr_emsg_txt[err] );
        switch ( err ) {
          case emsg_already_done:
            [[fallthrough]];
          case emsg_unknown:  // VkDeviceMemory object is somehow missing
            vkDestroyImage( cara_vk.log_dev, cara_vk.zbuf_n_cbuf[fif_idx][fb_idx], &vk_alloc_callbacks );
            cara_vk.zbuf_n_cbuf[fif_idx][fb_idx] = VK_NULL_HANDLE;
            cara_vk.mbi_zbuf_n_cbuf[fif_idx][fb_idx].range = invalid_range;
            cara_vk.mbi_zbuf_n_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
            break;
          default:
            break;
        }
      }  // if ( err )
      else {
        vkDestroyImage( cara_vk.log_dev, cara_vk.zbuf_n_cbuf[fif_idx][fb_idx], &vk_alloc_callbacks );
        cara_vk.zbuf_n_cbuf[fif_idx][fb_idx] = VK_NULL_HANDLE;
        cara_vk.mbi_zbuf_n_cbuf[fif_idx][fb_idx].range = invalid_range;
        cara_vk.mbi_zbuf_n_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
      }

      // vkDestroyImageView
      if ( cara_vk.zbuf_n_cbuf_iv[fif_idx][fb_idx] == VK_NULL_HANDLE ) {
        continue;
      }
      vkDestroyImageView( cara_vk.log_dev,
          cara_vk.zbuf_n_cbuf_iv[fif_idx][fb_idx], &vk_alloc_callbacks );
      cara_vk.zbuf_n_cbuf_iv[fif_idx][fb_idx] = VK_NULL_HANDLE;
    }
  }

  for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
    if ( cara_vk.arr_renderpasses[rp_idx] ) {
      vkDestroyRenderPass( cara_vk.log_dev, cara_vk.arr_renderpasses[rp_idx], &vk_alloc_callbacks );
      cara_vk.arr_renderpasses[rp_idx] = VK_NULL_HANDLE;
    }
  }

  if ( cara_vk.pll_z_only ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_z_only,
        &vk_alloc_callbacks
        );
    cara_vk.pll_z_only = VK_NULL_HANDLE;
  }
  if ( cara_vk.pll_main_render ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_main_render,
        &vk_alloc_callbacks
        );
    cara_vk.pll_main_render = VK_NULL_HANDLE;
  }
  if ( cara_vk.pll_cp2sc ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_cp2sc,
        &vk_alloc_callbacks
        );
    cara_vk.pll_cp2sc = VK_NULL_HANDLE;
  }

  for ( unsigned int sp_idx = 0;
      sp_idx < (unsigned int)cara_nr_subpasses + cara_nr_subpasses_in_2nd_rp;
      sp_idx++ ) {
    if ( cara_vk.arr_pipelines[sp_idx] ) {
      vkDestroyPipeline( cara_vk.log_dev, cara_vk.arr_pipelines[sp_idx], &vk_alloc_callbacks );
      cara_vk.arr_pipelines[sp_idx] = VK_NULL_HANDLE;
    }
  }
  //for ( unsigned int sp_idx = 0; sp_idx < cara_nr_subpasses_in_2nd_rp; sp_idx++ ) {
  //  if ( cara_vk.arr_pipelines2[sp_idx] ) {
  //    vkDestroyPipeline( cara_vk.log_dev, cara_vk.arr_pipelines2[sp_idx], &vk_alloc_callbacks );
  //    cara_vk.arr_pipelines2[sp_idx] = VK_NULL_HANDLE;
  //  }
  //}

  if ( cara_vk.ds_main_render_img ) {
    vkResetDescriptorPool( cara_vk.log_dev, cara_vk.descriptor_pool_main_img, 0 );
    cara_vk.ds_main_render_img = VK_NULL_HANDLE;
  }
  if ( cara_vk.ds_main_render_sampler ) {
    vkResetDescriptorPool( cara_vk.log_dev, cara_vk.descriptor_pool_main_sampler, 0 );
    cara_vk.ds_main_render_sampler = VK_NULL_HANDLE;
  }
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    if ( cara_vk.ds_cp2sc[fif_idx] ) {
      vkResetDescriptorPool( cara_vk.log_dev, cara_vk.descriptor_pool_cbuf, 0 );
      cara_vk.ds_cp2sc[fif_idx] = VK_NULL_HANDLE;
      break;
    }
  }

  if ( cara_vk.descriptor_pool_main_img ) {
    vkDestroyDescriptorPool( cara_vk.log_dev, cara_vk.descriptor_pool_main_img, &vk_alloc_callbacks );
    cara_vk.descriptor_pool_main_img = VK_NULL_HANDLE;
  }
  if ( cara_vk.descriptor_pool_main_sampler ) {
    vkDestroyDescriptorPool(
        cara_vk.log_dev, cara_vk.descriptor_pool_main_sampler, &vk_alloc_callbacks );
    cara_vk.descriptor_pool_main_sampler = VK_NULL_HANDLE;
  }
  if ( cara_vk.descriptor_pool_cbuf ) {
    vkDestroyDescriptorPool( cara_vk.log_dev, cara_vk.descriptor_pool_cbuf, &vk_alloc_callbacks );
    cara_vk.descriptor_pool_cbuf = VK_NULL_HANDLE;
  }

  if ( cara_vk.dsl_z_only ) {
    vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_z_only,
        &vk_alloc_callbacks );
    cara_vk.dsl_z_only = VK_NULL_HANDLE;
  }
  if ( cara_vk.dsl_main_render_img ) {
    vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_main_render_img,
        &vk_alloc_callbacks );
    cara_vk.dsl_main_render_img = VK_NULL_HANDLE;
  }
  if ( cara_vk.dsl_main_render_sampler ) {
    vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_main_render_sampler,
        &vk_alloc_callbacks );
    cara_vk.dsl_main_render_sampler = VK_NULL_HANDLE;
  }
  if ( cara_vk.dsl_cp2sc ) {
    vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_cp2sc,
        &vk_alloc_callbacks );
    cara_vk.dsl_cp2sc = VK_NULL_HANDLE;
  }

  if ( cara_vk.cbuf_sampler ) {
    vkDestroySampler( cara_vk.log_dev, cara_vk.cbuf_sampler, &vk_alloc_callbacks );
    cara_vk.cbuf_sampler = VK_NULL_HANDLE;
  }
  if ( cara_vk.albmap_sampler ) {
    vkDestroySampler( cara_vk.log_dev, cara_vk.albmap_sampler, &vk_alloc_callbacks );
    cara_vk.albmap_sampler = VK_NULL_HANDLE;
  }
  for ( unsigned int img_id = 0; img_id < _arr_elem_cnt(cara_vk.albmaps); img_id++ ) {
    if ( cara_vk.albmap_ivs[img_id] ) {
      vkDestroyImageView( cara_vk.log_dev, cara_vk.albmap_ivs[img_id], &vk_alloc_callbacks );
      cara_vk.albmap_ivs[img_id] = VK_NULL_HANDLE;
    }
    if ( cara_vk.albmaps[img_id] ) {
      enum err_msg err = ma_report_vk_malloc_rel( &cara_vk.vmm,
          &vk_alloc_callbacks, vk_malloc_wrapper,
          cara_vk.vmm.arr_mtype_used[vk_buf_type_texarr_2048],
          cara_vk.mbi_albmaps[img_id] );
      if ( err ) {
        fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d "
            "'%s'\n", __FILE__, __LINE__, arr_emsg_txt[err] );
        switch ( err ) {
          case emsg_already_done:
            [[fallthrough]];
          case emsg_unknown:  // VkDeviceMemory object is somehow missing
            cara_vk.mbi_albmaps[img_id].range = invalid_range;
            cara_vk.mbi_albmaps[img_id].malloc = VK_NULL_HANDLE;
            vkDestroyImage( cara_vk.log_dev, cara_vk.albmaps[img_id], &vk_alloc_callbacks );
            cara_vk.albmaps[img_id] = VK_NULL_HANDLE;
            break;
          default:
            break;
        }
      }  // if ( err )
      else {  // No error occurred
        cara_vk.mbi_albmaps[img_id].range = invalid_range;
        cara_vk.mbi_albmaps[img_id].malloc = VK_NULL_HANDLE;
        vkDestroyImage( cara_vk.log_dev, cara_vk.albmaps[img_id], &vk_alloc_callbacks );
        cara_vk.albmaps[img_id] = VK_NULL_HANDLE;
      }
    }
  }

  if ( cara_vk.vert_buf ) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.vert_buf, &req );
    VkMemoryPropertyFlags preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryPropertyFlags required_props = 0;
    VkMemoryPropertyFlags tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    uint32_t mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
    enum err_msg err = ma_report_vk_malloc_rel( &cara_vk.vmm,
        &vk_alloc_callbacks, vk_malloc_wrapper,
        mtype_idx,
        cara_vk.mbi_vbuf );
    if ( err ) {
      fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d "
          "'%s'\n", __FILE__, __LINE__, arr_emsg_txt[err] );
      switch ( err ) {
        case emsg_already_done:
          [[fallthrough]];
        case emsg_unknown:  // VkDeviceMemory object is somehow missing
          cara_vk.mbi_vbuf.range = invalid_range;
          cara_vk.mbi_vbuf.malloc = VK_NULL_HANDLE;
          vkDestroyBuffer( cara_vk.log_dev, cara_vk.vert_buf, &vk_alloc_callbacks );
          cara_vk.vert_buf = VK_NULL_HANDLE;
          break;
        default:
          break;
      }
    }  // if ( err )
    else {  // No error occurred
      cara_vk.mbi_vbuf.range = invalid_range;
      cara_vk.mbi_vbuf.malloc = VK_NULL_HANDLE;
      vkDestroyBuffer( cara_vk.log_dev, cara_vk.vert_buf, &vk_alloc_callbacks );
      cara_vk.vert_buf = VK_NULL_HANDLE;
    }
  }

  if ( cara_vk.attr_buf ) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.attr_buf, &req );
    VkMemoryPropertyFlags preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryPropertyFlags required_props = 0;
    VkMemoryPropertyFlags tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    uint32_t mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
    enum err_msg err = ma_report_vk_malloc_rel( &cara_vk.vmm,
        &vk_alloc_callbacks, vk_malloc_wrapper,
        mtype_idx,
        cara_vk.mbi_abuf );
    if ( err ) {
      fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d '%s'\n",
          __FILE__, __LINE__, arr_emsg_txt[err] );
      switch ( err ) {
        case emsg_already_done:
          [[fallthrough]];
        case emsg_unknown:  // VkDeviceMemory object is somehow missing
          cara_vk.mbi_abuf.range = invalid_range;
          cara_vk.mbi_abuf.malloc = VK_NULL_HANDLE;
          vkDestroyBuffer( cara_vk.log_dev, cara_vk.attr_buf, &vk_alloc_callbacks );
          cara_vk.attr_buf = VK_NULL_HANDLE;
          break;
        default:
          break;
      }
    }  // if ( err )
    else {
      cara_vk.mbi_abuf.range = invalid_range;
      cara_vk.mbi_abuf.malloc = VK_NULL_HANDLE;
      vkDestroyBuffer( cara_vk.log_dev, cara_vk.attr_buf, &vk_alloc_callbacks );
      cara_vk.attr_buf = VK_NULL_HANDLE;
    }
  }

  if ( cara_vk.ind_buf ) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.ind_buf, &req );
    VkMemoryPropertyFlags preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryPropertyFlags required_props = 0;
    VkMemoryPropertyFlags tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    uint32_t mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
    enum err_msg err = ma_report_vk_malloc_rel( &cara_vk.vmm,
        &vk_alloc_callbacks, vk_malloc_wrapper,
        mtype_idx,
        cara_vk.mbi_ibuf );
    if ( err ) {
      fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d "
          "'%s'\n", __FILE__, __LINE__, arr_emsg_txt[err] );
      switch ( err ) {
        case emsg_already_done:
          [[fallthrough]];
        case emsg_unknown:  // VkDeviceMemory object is somehow missing
          cara_vk.mbi_ibuf.range = invalid_range;
          cara_vk.mbi_ibuf.malloc = VK_NULL_HANDLE;
          vkDestroyBuffer( cara_vk.log_dev, cara_vk.ind_buf, &vk_alloc_callbacks );
          cara_vk.ind_buf = VK_NULL_HANDLE;
          break;
        default:
          break;
      }
    }  // if ( err )
    else {  // No error occurred
      cara_vk.mbi_ibuf.range = invalid_range;
      cara_vk.mbi_ibuf.malloc = VK_NULL_HANDLE;
      vkDestroyBuffer( cara_vk.log_dev, cara_vk.ind_buf, &vk_alloc_callbacks );
      cara_vk.ind_buf = VK_NULL_HANDLE;
    }
  }

  // FIXME Host access to `semaphore` must be externally synchronized (vkDestroySemaphore)
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    if( cara_vk.arr_sc_acquire_semphos[fif_idx] != VK_NULL_HANDLE ) {
      vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_sc_acquire_semphos[fif_idx],
          &vk_alloc_callbacks );
      cara_vk.arr_sc_acquire_semphos[fif_idx] = VK_NULL_HANDLE;
    }
  }
  if( cara_vk.arr_sc_present_semphos != nullptr ) {
    for ( unsigned int swapchain_img_idx = 0;
        swapchain_img_idx < cara_vk.nr_swapchain_imgs;
        swapchain_img_idx++ ) {
      if( cara_vk.arr_sc_present_semphos[swapchain_img_idx] != VK_NULL_HANDLE ) {
        vkDestroySemaphore( cara_vk.log_dev,
            cara_vk.arr_sc_present_semphos[swapchain_img_idx],
            &vk_alloc_callbacks );
        cara_vk.arr_sc_present_semphos[swapchain_img_idx] = VK_NULL_HANDLE;
      }
    }
    ma_free( cara_single.mem_arena, worker_idx, cara_vk.arr_sc_present_semphos );
    cara_vk.arr_sc_present_semphos = nullptr;
  }
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    if( cara_vk.arr_semphos_tr[fif_idx] != VK_NULL_HANDLE ) {
      vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_semphos_tr[fif_idx], &vk_alloc_callbacks );
      cara_vk.arr_semphos_tr[fif_idx] = VK_NULL_HANDLE;
    }
    if( cara_vk.arr_semphos_gr[fif_idx] != VK_NULL_HANDLE ) {
      vkDestroySemaphore( cara_vk.log_dev, cara_vk.arr_semphos_gr[fif_idx], &vk_alloc_callbacks );
      cara_vk.arr_semphos_gr[fif_idx] = VK_NULL_HANDLE;
    }
  }

  if ( cara_vk.swapchain ) {
    vkDestroySwapchainKHR( cara_vk.log_dev,
        cara_vk.swapchain,
        &vk_alloc_callbacks
        );
    cara_vk.swapchain = VK_NULL_HANDLE;

    if ( cara_vk.arr_swapchain_iv ) {
      for ( unsigned int i = 0; i < cara_vk.nr_swapchain_imgs; i++ ) {
        if ( cara_vk.arr_swapchain_iv[i] == VK_NULL_HANDLE ) {
          continue;
        }
        vkDestroyImageView( cara_vk.log_dev, cara_vk.arr_swapchain_iv[i], &vk_alloc_callbacks );
      }
      ma_free( cara_single.mem_arena, worker_idx, cara_vk.arr_swapchain_iv );
      cara_vk.arr_swapchain_iv = nullptr;
    }

    if ( cara_vk.arr_swapchain_imgs ) {
      ma_free( cara_single.mem_arena, worker_idx, cara_vk.arr_swapchain_imgs );
      cara_vk.arr_swapchain_imgs = nullptr;
    }
  }

  if ( cara_vk.vmm.arr_usages ) {
    // This function will put nullptr in the struct
    ma_destroy_vk_mem_mgr( &cara_vk.vmm, cara_vk.log_dev,
        ui2ptr(worker_idx), cara_free, &vk_alloc_callbacks, vk_malloc_wrapper );
  }

  for ( uint_fast8_t set_idx = 0;
      set_idx < nr_al_cmd_pool_sets + cara_max_nr_fif;
      set_idx++ ) {
    if ( cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools ) {
      for ( uint_fast8_t cmd_pool_idx = 0;
          cmd_pool_idx <= cara_single.proc_map->nr_processors; cmd_pool_idx++ ) {
        if ( cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cmd_pool_idx] ) {
          vkDestroyCommandPool( cara_vk.log_dev,
              cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cmd_pool_idx],
              &vk_alloc_callbacks
              );

          cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[cmd_pool_idx] = 0;
        }
      }
      ma_free( cara_single.mem_arena, worker_idx,
          cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools );
      cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools = nullptr;
    }
  }

  if ( cara_vk.arr_graphics_cmdbuf_sets[0].arr_cmd_pools ) {
    for ( uint_fast8_t fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
      for ( uint_fast8_t idx_command_pool = 0;
          idx_command_pool <= cara_single.proc_map->nr_processors; idx_command_pool++ ) {
        if ( cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[idx_command_pool] ) {
          //fprintf( stdout, "framebufs[%u].arr_cmd_pools[%u] = %#018lx\n",
          //    idx_framebuffer,
          //    idx_command_pool,
          //    (uintptr_t)cara_vk.arr_framebufs[idx_framebuffer].arr_cmd_pools[idx_command_pool] );
          vkDestroyCommandPool( cara_vk.log_dev,
              cara_vk.arr_graphics_cmdbuf_sets[fif_idx].arr_cmd_pools[idx_command_pool],
              &vk_alloc_callbacks
              //nullptr
              );
        }
      }
      vkDestroyCommandPool( cara_vk.log_dev,
          cara_vk.dummy_pool[fif_idx],
          &vk_alloc_callbacks
          );
    }
    ma_free( cara_single.mem_arena, worker_idx, cara_vk.arr_graphics_cmdbuf_sets[0].arr_cmd_pools );
    cara_vk.arr_graphics_cmdbuf_sets[0].arr_cmd_pools = nullptr;
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
        &vk_alloc_callbacks
        //nullptr
        );
    cara_vk.presentation_surface = VK_NULL_HANDLE;
  }

  if ( cara_vk.window ) {
    SDL_DestroyWindow(cara_vk.window);
    cara_vk.window = nullptr;
  }

  if ( cara_vk.log_dev ) {
    vkDestroyDevice( cara_vk.log_dev, &vk_alloc_callbacks );
    //vkDestroyDevice( cara_vk.log_dev, nullptr );
    cara_vk.log_dev = VK_NULL_HANDLE;
  }

  if ( cara_vk.vk_instance ) {
    vkDestroyInstance( cara_vk.vk_instance, &vk_alloc_callbacks );
    //vkDestroyInstance( cara_vk.vk_instance, nullptr );
    cara_vk.vk_instance = VK_NULL_HANDLE;
  }

  //if ( cara_vk.vk_library ) {
  //  unload_shared_lib( cara_vk.vk_library );
  //  cara_vk.vk_library = nullptr;
  //}

}



//-------------------------------------------------------------------------------------------------
static enum err_msg create_2nd_renderpass( struct cara_vk_renderer * renderer )
{
  //
  // Define attachments
  //

  constexpr unsigned int nr_attachments_for_2nd_rp = 2;
  constexpr unsigned int local_fb_zbuf = 0;
  constexpr unsigned int local_fb_swapchain = 1;

  // The color attachment
  VkAttachmentDescription2 arr_att_descs[nr_attachments_for_2nd_rp];
  // NOTE Consider chaining VkAttachmentDescriptionStencilLayout in the pNext field in order to
  // set stencilInitialLayout and stencilFinalLayout
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( arr_att_descs ); att_idx++ ) {
    arr_att_descs[att_idx].flags = 0;
    arr_att_descs[att_idx].samples = VK_SAMPLE_COUNT_1_BIT;
    arr_att_descs[att_idx].pNext = nullptr;
    arr_att_descs[att_idx].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    arr_att_descs[att_idx].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // An UNDEFINED layout can
                                                             // always be used for an initial/old
                                                             // layout—It means that we don't need
                                                             // images content when a memory barrier
                                                             // is set up.
  }
  // The first thing we need to record after acquiring a swapchain image is to change the swapchain
  //  image's layout to a VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout.
  // This operation should be performed implicitly using appropriate render pass parameters (initial
  //  and sub-pass layouts). But if queues used for the presentation and graphics operations come
  //  from two different families, we must perform ownership transfer. This cannot be done
  //  implicitly—for this we need to set up an image memory barrier.
  // I guess setting VkAttachmentDescription2.initialLayout and VkAttachmentReference2.layout are
  //  enough to make the implicit transition happen

  // The depth buffer
  arr_att_descs[local_fb_zbuf].format = renderer->zbuf_fmt;
  arr_att_descs[local_fb_zbuf].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  arr_att_descs[local_fb_zbuf].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  arr_att_descs[local_fb_zbuf].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  arr_att_descs[local_fb_zbuf].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  arr_att_descs[local_fb_zbuf].finalLayout = has_stencil_buf( renderer->zbuf_fmt ) ?
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  // The swapchain image
  arr_att_descs[local_fb_swapchain].format = renderer->swapchain_img_fmt;
  arr_att_descs[local_fb_swapchain].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  arr_att_descs[local_fb_swapchain].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  arr_att_descs[local_fb_swapchain].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  arr_att_descs[local_fb_swapchain].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  arr_att_descs[local_fb_swapchain].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  //\
                                                                  // We are rendering
                                                                  // directly into a swapchain image
                                                                  // and we want to display it on
                                                                  // screen; We should use a
                                                                  // PRESENT_SRC layout.

  // Prepare the array of VkAttachmentReference2
  VkAttachmentReference2 arr_att_refs[nr_attachments_for_2nd_rp];
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( arr_att_refs ); att_idx++ ) {
    arr_att_refs[att_idx].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    arr_att_refs[att_idx].pNext = nullptr;
  }

  // The depth buffer
  arr_att_refs[local_fb_zbuf].attachment = local_fb_zbuf;
  arr_att_refs[local_fb_zbuf].layout = has_stencil_buf( renderer->zbuf_fmt ) ?
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  arr_att_refs[local_fb_zbuf].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  // The swapchain image
  arr_att_refs[local_fb_swapchain].attachment = local_fb_swapchain;
  arr_att_refs[local_fb_swapchain].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  arr_att_refs[local_fb_swapchain].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;


  //
  // Define subpasses
  //

  // Operations performed in a render pass are grouped into subpasses. Each subpass represents a
  //  stage or a phase of our rendering commands in which a subset of render pass's attachments are
  //  used (into which we render or from which we read data).
  // A render pass always requires at least one subpass that is automatically started when we begin
  //  a render pass. And for each subpass, we need to prepare a description.
  VkSubpassDescription2 arr_subpass_desc[cara_nr_subpasses_in_2nd_rp];
  for ( unsigned int sp_idx = 0; sp_idx < _arr_elem_cnt( arr_subpass_desc ); sp_idx++ ) {
    arr_subpass_desc[sp_idx].flags = 0;
    arr_subpass_desc[sp_idx].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    arr_subpass_desc[sp_idx].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    arr_subpass_desc[sp_idx].pNext = nullptr;
    arr_subpass_desc[sp_idx].viewMask = 0;
  }
  arr_subpass_desc[cs2_copy_to_sc].inputAttachmentCount = 0;
  arr_subpass_desc[cs2_copy_to_sc].pInputAttachments = nullptr;
  arr_subpass_desc[cs2_copy_to_sc].colorAttachmentCount = 1;
  arr_subpass_desc[cs2_copy_to_sc].pColorAttachments = &arr_att_refs[local_fb_swapchain];
  arr_subpass_desc[cs2_copy_to_sc].pResolveAttachments = nullptr;
  arr_subpass_desc[cs2_copy_to_sc].pDepthStencilAttachment = &arr_att_refs[local_fb_zbuf];
  arr_subpass_desc[cs2_copy_to_sc].preserveAttachmentCount = 0;
  arr_subpass_desc[cs2_copy_to_sc].pPreserveAttachments = nullptr;


  //
  // Subpass dependencies
  //

  // When operations in a given subpass depend on the results of operations in one of the earlier
  //  subpasses in the same render pass, we need to specify subpass dependencies. This is also
  //  required if there are dependencies between operations recorded within a render pass and those
  //  performed before it, or between operations that are executed after a render pass and those
  //  performed within the render pass. It is also possible to define dependencies within a single
  //  subpass.
  VkSubpassDependency2 sp_dep[1];
  for ( unsigned int dep_idx = 0; dep_idx < _arr_elem_cnt(sp_dep); dep_idx++ ) {
    sp_dep[dep_idx].pNext = nullptr;
    sp_dep[dep_idx].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    sp_dep[dep_idx].viewOffset = 0;
  }
  // cs_opaq_objs - cs2_copy_to_sc
  sp_dep[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  sp_dep[0].dstSubpass = cs2_copy_to_sc;
  sp_dep[0].srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
  sp_dep[0].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  sp_dep[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  sp_dep[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  sp_dep[0].dependencyFlags = 0;

  // We don't need to set up dependencies for the layout transitions—these are performed
  // automatically based on the information provided for the render pass attachment and subpass
  // descriptions. What's more, when we specify different attachment layouts for different subpasses
  // but in both subpasses the given attachment is used only for reading, we also don't need to
  // specify a dependency.

  //
  // Call vkCreateRenderPass
  //
  VkRenderPassCreateInfo2 rp_create_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
    .pNext = nullptr,
    .flags = 0,
    .attachmentCount = _arr_elem_cnt( arr_att_descs ),
    .pAttachments = arr_att_descs,
    .subpassCount = _arr_elem_cnt( arr_subpass_desc ),
    .pSubpasses = arr_subpass_desc,
    .dependencyCount = _arr_elem_cnt( sp_dep ),
    .pDependencies = sp_dep,
    .correlatedViewMaskCount = 0,
    .pCorrelatedViewMasks = nullptr,
  };
  TRACE_BEGIN("vkCreateRenderPass2");
  VkResult result;
  result = vkCreateRenderPass2( renderer->log_dev, &rp_create_info,
      &vk_alloc_callbacks,
      &renderer->arr_renderpasses[1] );
  TRACE_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a render pass. '%s'\n",
        string_VkResult(result) );
    return emsg_unknown;
  }
  return emsg_no_error;
}




//-------------------------------------------------------------------------------------------------
enum err_msg create_renderpass_and_framebuffer_first_time( struct cara_vk_renderer * renderer )
{
  //
  // Define attachments
  //

  // The color attachment
  VkAttachmentDescription2 arr_att_descs[2];
  // NOTE Consider chaining VkAttachmentDescriptionStencilLayout in the pNext field in order to
  // set stencilInitialLayout and stencilFinalLayout
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( arr_att_descs ); att_idx++ ) {
    arr_att_descs[att_idx].flags = 0;
    arr_att_descs[att_idx].samples = VK_SAMPLE_COUNT_1_BIT;
    arr_att_descs[att_idx].pNext = nullptr;
    arr_att_descs[att_idx].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    arr_att_descs[att_idx].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // An UNDEFINED layout can
                                                             // always be used for an initial/old
                                                             // layout—It means that we don't need
                                                             // images content when a memory barrier
                                                             // is set up.
  }

  // The depth buffer
  arr_att_descs[fb_zbuf].format = renderer->zbuf_fmt;
  arr_att_descs[fb_zbuf].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  arr_att_descs[fb_zbuf].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  arr_att_descs[fb_zbuf].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  arr_att_descs[fb_zbuf].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  arr_att_descs[fb_zbuf].finalLayout = has_stencil_buf( renderer->zbuf_fmt ) ?
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  // The color buffer
  arr_att_descs[fb_cbuf].format = VK_FORMAT_R8G8B8A8_UNORM;
  arr_att_descs[fb_cbuf].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  arr_att_descs[fb_cbuf].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  arr_att_descs[fb_cbuf].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  arr_att_descs[fb_cbuf].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  arr_att_descs[fb_cbuf].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


  // Prepare the array of VkAttachmentReference2
  VkAttachmentReference2 arr_att_refs[cara_nr_framebuffers - 1];
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( arr_att_refs ); att_idx++ ) {
    arr_att_refs[att_idx].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    arr_att_refs[att_idx].pNext = nullptr;
  }

  // The depth buffer
  arr_att_refs[fb_zbuf].attachment = fb_zbuf;
  arr_att_refs[fb_zbuf].layout = has_stencil_buf( renderer->zbuf_fmt ) ?
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  arr_att_refs[fb_zbuf].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  // The color buffer
  arr_att_refs[fb_cbuf].attachment = fb_cbuf;
  arr_att_refs[fb_cbuf].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  arr_att_refs[fb_cbuf].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  //// The swapchain image
  //arr_att_refs[fb_swapchain].attachment = fb_swapchain;
  //arr_att_refs[fb_swapchain].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  //arr_att_refs[fb_swapchain].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  //
  // Define subpasses
  //

  // Operations performed in a render pass are grouped into subpasses. Each subpass represents a
  //  stage or a phase of our rendering commands in which a subset of render pass's attachments are
  //  used (into which we render or from which we read data).
  // A render pass always requires at least one subpass that is automatically started when we begin
  //  a render pass. And for each subpass, we need to prepare a description.
  VkSubpassDescription2 arr_subpass_desc[cara_nr_subpasses];
  for ( unsigned int sp_idx = 0; sp_idx < _arr_elem_cnt( arr_subpass_desc ); sp_idx++ ) {
    arr_subpass_desc[sp_idx].flags = 0;
    arr_subpass_desc[sp_idx].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    arr_subpass_desc[sp_idx].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    arr_subpass_desc[sp_idx].pNext = nullptr;
    arr_subpass_desc[sp_idx].viewMask = 0;
  }

  arr_subpass_desc[cs_z_only].inputAttachmentCount = 0;
  arr_subpass_desc[cs_z_only].pInputAttachments = nullptr;
  arr_subpass_desc[cs_z_only].colorAttachmentCount = 0;
  arr_subpass_desc[cs_z_only].pColorAttachments = nullptr;
  arr_subpass_desc[cs_z_only].pResolveAttachments = nullptr;
  arr_subpass_desc[cs_z_only].pDepthStencilAttachment = &arr_att_refs[fb_zbuf];
  arr_subpass_desc[cs_z_only].preserveAttachmentCount = 0;
  arr_subpass_desc[cs_z_only].pPreserveAttachments = nullptr;

  arr_subpass_desc[cs_opaq_objs].inputAttachmentCount = 0;
  arr_subpass_desc[cs_opaq_objs].pInputAttachments = nullptr;
  arr_subpass_desc[cs_opaq_objs].colorAttachmentCount = 1;
  arr_subpass_desc[cs_opaq_objs].pColorAttachments = &arr_att_refs[fb_cbuf];
  arr_subpass_desc[cs_opaq_objs].pResolveAttachments = nullptr;  // If any of the color
                    // attachments should be resolved (changed from multisampled to single-sampled),
                    // specify a pointer to the first element of the array with same number of
                    // elements as pColorAttachments or use a nullptr value if no color attachments
                    // need to be resolved; each member of the pResolveAttachments array corresponds
                    // to the color attachment at the same index and specifies to which attachment a
                    // given color attachment should be resolved at the end of the subpass; for each
                    // member of the array use specified values as described in points A and B;
                    // Use a VK_ATTACHMENT_UNUSED value for the attachment index if the given color
                    // attachment should not be resolved.
          // A: Index of the attachment in the list of all render pass attachments for `attachment`.
          // B: A layout given image should be automatically transitioned to at the beginning of
          //   the subpass for `layout`.
  arr_subpass_desc[cs_opaq_objs].pDepthStencilAttachment = &arr_att_refs[fb_zbuf];
  arr_subpass_desc[cs_opaq_objs].preserveAttachmentCount = 0;
  arr_subpass_desc[cs_opaq_objs].pPreserveAttachments = nullptr;


  //
  // Subpass dependencies
  //

  // When operations in a given subpass depend on the results of operations in one of the earlier
  //  subpasses in the same render pass, we need to specify subpass dependencies. This is also
  //  required if there are dependencies between operations recorded within a render pass and those
  //  performed before it, or between operations that are executed after a render pass and those
  //  performed within the render pass. It is also possible to define dependencies within a single
  //  subpass.
  VkSubpassDependency2 sp_dep[1];
  for ( unsigned int dep_idx = 0; dep_idx < _arr_elem_cnt(sp_dep); dep_idx++ ) {
    sp_dep[dep_idx].pNext = nullptr;
    sp_dep[dep_idx].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    sp_dep[dep_idx].viewOffset = 0;
  }
  // cs_z_only - cs_opaq_objs
  sp_dep[0].srcSubpass = cs_z_only;  // The index of a subpass from which ("producing")
                                     // operations should be finished before the second set of
                                     // ("consuming") operations (or a VK_SUBPASS_EXTERNAL value
                                     // for commands before the render pass).
  sp_dep[0].dstSubpass = cs_opaq_objs;  // The index of a subpass whose operations depend on
                                        // the previous set of commands (or a VK_SUBPASS_EXTERNAL
                                        // value for operations after the render pass).
  sp_dep[0].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;  // The set of pipeline stages
                                                        // which produce the result read by the
                                                        // "consuming" commands.
  sp_dep[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  sp_dep[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  sp_dep[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  sp_dep[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;  // For dependencyFlags, use a
                                   // VK_DEPENDENCY_BY_REGION_BIT value if the dependency is defined
                                   // by region—it means that operations generating data for a given
                                   // memory region must finish before operations reading data from
                                   // the same region can be executed; if this flag is not specified
                                   // dependency is global, which means that data for the whole
                                   // image must be generated before "consuming" commands can be
                                   // executed.

  // We don't need to set up dependencies for the layout transitions—these are performed
  // automatically based on the information provided for the render pass attachment and subpass
  // descriptions. What's more, when we specify different attachment layouts for different subpasses
  // but in both subpasses the given attachment is used only for reading, we also don't need to
  // specify a dependency.


  //
  // Call vkCreateRenderPass
  //
  VkRenderPassCreateInfo2 rp_create_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
    .pNext = nullptr,
    .flags = 0,
    .attachmentCount = _arr_elem_cnt( arr_att_descs ),
    .pAttachments = arr_att_descs,
    .subpassCount = _arr_elem_cnt( arr_subpass_desc ),
    .pSubpasses = arr_subpass_desc,
    .dependencyCount = _arr_elem_cnt( sp_dep ),
    .pDependencies = sp_dep,
    .correlatedViewMaskCount = 0,
    .pCorrelatedViewMasks = nullptr,
  };
  TRACE_BEGIN("vkCreateRenderPass2");
  VkResult result;
  result = vkCreateRenderPass2( renderer->log_dev, &rp_create_info,
      &vk_alloc_callbacks,
      //nullptr,
      &renderer->arr_renderpasses[0] );
  TRACE_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a render pass. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  enum err_msg err = create_2nd_renderpass( renderer );
  if ( err ) {
    goto stop_everything_after_failure;
  }


  //
  // Prepare framebuffers
  //

  // Framebuffers are used along with render passes. They specify what image resources should be
  // used for corresponding attachments defined in a render pass. They also define the size of a
  // renderable area. That's why when we want to record drawing operations, we not only need to
  // create a render pass, but also a framebuffer.

  // Framebuffers also define the size of a renderable area—the dimensions into which all rendering
  // will be confined. However, what we need to remember is that it is up to us to make sure that
  // the pixels/fragments outside of the specified range are not modified. For this purpose, we need
  // to specify the appropriate parameters (viewport and scissor test) during the pipeline creation
  // or when setting corresponding dynamic states
  // (refer to the Preparing viewport and scissor test state recipe from Chapter 8, Graphics and
  // Compute Pipelines and to the Setting a dynamic viewport and scissors state recipe from Chapter
  // 9, Command Recording and Drawing).
  VkFramebufferAttachmentImageInfo fb_att_img_info1[cara_nr_framebuffers - 1];
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( fb_att_img_info1 ); att_idx++ ) {
    fb_att_img_info1[att_idx].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
    fb_att_img_info1[att_idx].pNext = nullptr;
    fb_att_img_info1[att_idx].flags = 0;
    fb_att_img_info1[att_idx].width = initial_win_width;
    fb_att_img_info1[att_idx].height = initial_win_height;
    fb_att_img_info1[att_idx].layerCount = 1;
    fb_att_img_info1[att_idx].viewFormatCount = 1;
  }
  constexpr VkFormat rgba_unorm = VK_FORMAT_R8G8B8A8_UNORM;
  fb_att_img_info1[fb_cbuf].usage = //VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  fb_att_img_info1[fb_cbuf].pViewFormats = &rgba_unorm;

  const VkFormat depth_fmt = renderer->zbuf_fmt;
  fb_att_img_info1[fb_zbuf].usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  fb_att_img_info1[fb_zbuf].pViewFormats = &depth_fmt;

  VkFramebufferAttachmentImageInfo fb_att_img_info2[2];
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( fb_att_img_info2 ); att_idx++ ) {
    fb_att_img_info2[att_idx].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
    fb_att_img_info2[att_idx].pNext = nullptr;
    fb_att_img_info2[att_idx].flags = 0;
    fb_att_img_info2[att_idx].width = initial_win_width;
    fb_att_img_info2[att_idx].height = initial_win_height;
    fb_att_img_info2[att_idx].layerCount = 1;
    fb_att_img_info2[att_idx].viewFormatCount = 1;
  }
  fb_att_img_info2[0].usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  fb_att_img_info2[0].pViewFormats = &depth_fmt;
  fb_att_img_info2[1].usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  fb_att_img_info2[1].pViewFormats = &renderer->swapchain_img_fmt;

  VkFramebufferAttachmentsCreateInfo fb_atts_create_info = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
    .pNext = nullptr,
  };

  VkFramebufferCreateInfo fb_create_info = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .pNext = &fb_atts_create_info,
    .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,  // Framebuffers are created with a small amount
                                                   // of additional metadata about the image views
                                                   // that will be used in
                                                   // `VkFramebufferAttachmentsCreateInfo`, and the
                                                   // actual image views are provided at render pass
                                                   // begin time via
                                                   // `VkRenderPassAttachmentBeginInfo`.
    .attachmentCount = _arr_elem_cnt( fb_att_img_info1 ),
    .pAttachments = nullptr,
    .width = initial_win_width,
    .height = initial_win_height,
    .layers = 1,
  };
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    fb_create_info.renderPass = renderer->arr_renderpasses[0];
    fb_atts_create_info.attachmentImageInfoCount = _arr_elem_cnt( fb_att_img_info1 );
    fb_atts_create_info.pAttachmentImageInfos = fb_att_img_info1;
    TRACE_BEGIN("vkCreateFramebuffer");
    result = vkCreateFramebuffer( renderer->log_dev, &fb_create_info, &vk_alloc_callbacks,
        &renderer->arr_framebuffers[fif_idx][0] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateFramebuffer. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }

    fb_create_info.attachmentCount = _arr_elem_cnt( fb_att_img_info2 );
    fb_create_info.renderPass = renderer->arr_renderpasses[1];
    fb_atts_create_info.attachmentImageInfoCount = _arr_elem_cnt( fb_att_img_info2 );
    fb_atts_create_info.pAttachmentImageInfos = fb_att_img_info2;
    TRACE_BEGIN("vkCreateFramebuffer");
    result = vkCreateFramebuffer( renderer->log_dev, &fb_create_info, &vk_alloc_callbacks,
        &renderer->arr_framebuffers[fif_idx][1] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateFramebuffer. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }



  //
  // Create actual framebuffers (a depth buffer and a color buffer)
  //

  // Create a depth buffer and a color buffer
  VkImageCreateInfo img_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = (VkExtent3D) { initial_win_width, initial_win_height, 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    img_create_info.format = renderer->zbuf_fmt;
    img_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    TRACE_BEGIN("vkCreateImage");
    result = vkCreateImage( renderer->log_dev, &img_create_info, &vk_alloc_callbacks,
        &renderer->zbuf_n_cbuf[fif_idx][fb_zbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImage. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
    img_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    img_create_info.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    TRACE_BEGIN("vkCreateImage");
    result = vkCreateImage( renderer->log_dev, &img_create_info, &vk_alloc_callbacks,
        &renderer->zbuf_n_cbuf[fif_idx][fb_cbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImage. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }

  // vkBindImageMemory
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int fb_idx = fb_zbuf; fb_idx <= fb_cbuf; fb_idx++ ) {
      // Obtain the requirements
      VkMemoryRequirements req;
      TRACE_BEGIN("vkGetImageMemoryRequirements");
      vkGetImageMemoryRequirements( renderer->log_dev,
          renderer->zbuf_n_cbuf[fif_idx][fb_idx], &req );
      TRACE_END();

      // Choose the best memory type
      VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
      preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      required_props = 0;
      tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      const uint32_t mtype_idx = ma_pick_best_mtype(
          &renderer->vmm, &req, required_props, preferred_props, tolerable_props );
      //const uint32_t heap_idx = renderer->vmm.arr_heap_indices[mtype_idx];

      // Bind the image to a vulkan memory object
      struct vk_mbind_info mbind_info;
      err = ma_bind_img_to_malloc( &renderer->vmm,
          &vk_alloc_callbacks, vk_malloc_wrapper,
          renderer->log_dev, renderer->zbuf_n_cbuf[fif_idx][fb_idx],
          mtype_idx, req.size, req.alignment, &mbind_info );
      if ( err ) {
        fprintf( stderr, "Error occured in ma_bind_img_to_malloc. (%s)\n", arr_emsg_txt[err] );
        goto stop_everything_after_failure;
      }
      renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx] = mbind_info;
    }
  }

  // Create VkImageViews for depth buffers and color buffers
  VkImageViewCreateInfo iv_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .components = (VkComponentMapping) {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY
    },
    .subresourceRange = (VkImageSubresourceRange) {
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    },
  };
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    iv_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    iv_create_info.format = renderer->zbuf_fmt;
    iv_create_info.image = renderer->zbuf_n_cbuf[fif_idx][fb_zbuf];
    TRACE_BEGIN("vkCreateImageView");
    result = vkCreateImageView( renderer->log_dev, &iv_create_info, &vk_alloc_callbacks,
        &renderer->zbuf_n_cbuf_iv[fif_idx][fb_zbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImageView. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
    iv_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    iv_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    iv_create_info.image = renderer->zbuf_n_cbuf[fif_idx][fb_cbuf];
    TRACE_BEGIN("vkCreateImageView");
    result = vkCreateImageView( renderer->log_dev, &iv_create_info, &vk_alloc_callbacks,
        &renderer->zbuf_n_cbuf_iv[fif_idx][fb_cbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImageView. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }

  renderer->dims_swapchain_imgs = (VkExtent2D) {
    .width = initial_win_width,
    .height = initial_win_height,
  };


  return emsg_no_error;

stop_everything_after_failure:
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
      // vkDestroyFramebuffer
      if ( renderer->arr_framebuffers[fif_idx][rp_idx] ) {
        vkDestroyFramebuffer( renderer->log_dev,
            renderer->arr_framebuffers[fif_idx][rp_idx], &vk_alloc_callbacks );
        renderer->arr_framebuffers[fif_idx][rp_idx] = VK_NULL_HANDLE;
      }
    }
  }
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    // vkDestroyImage
    for ( unsigned int fb_idx = fb_zbuf; fb_idx <= fb_cbuf; fb_idx++ ) {
      if ( renderer->zbuf_n_cbuf[fif_idx][fb_idx] == VK_NULL_HANDLE ) {
        continue;
      }
      // Should I just write down the memory type when binding to a memory object?
      VkMemoryRequirements req;
      vkGetImageMemoryRequirements( renderer->log_dev,
          renderer->zbuf_n_cbuf[fif_idx][fb_idx], &req );
      VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
      preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      required_props = 0;
      tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      err = ma_report_vk_malloc_rel( &renderer->vmm,
          &vk_alloc_callbacks, vk_malloc_wrapper,
          ma_pick_best_mtype(&renderer->vmm, &req,
            required_props, preferred_props, tolerable_props),
          renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx] );
      if ( err ) {
        fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d "
            "'%s'\n", __FILE__, __LINE__, arr_emsg_txt[err] );
        switch ( err ) {
          case emsg_already_done:
            [[fallthrough]];
          case emsg_unknown:  // VkDeviceMemory object is somehow missing
            vkDestroyImage( renderer->log_dev,
                renderer->zbuf_n_cbuf[fif_idx][fb_idx], &vk_alloc_callbacks );
            renderer->zbuf_n_cbuf[fif_idx][fb_idx] = VK_NULL_HANDLE;
            renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx].range = invalid_range;
            renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
            break;
          default:
            break;
        }
      }  // if ( err )
      else {
        vkDestroyImage( renderer->log_dev,
            renderer->zbuf_n_cbuf[fif_idx][fb_idx], &vk_alloc_callbacks );
        renderer->zbuf_n_cbuf[fif_idx][fb_idx] = VK_NULL_HANDLE;
        renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx].range = invalid_range;
        renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
      }

      // vkDestroyImageView
      if ( renderer->zbuf_n_cbuf_iv[fif_idx][fb_idx] == VK_NULL_HANDLE ) {
        continue;
      }
      vkDestroyImageView( renderer->log_dev,
          renderer->zbuf_n_cbuf_iv[fif_idx][fb_idx], &vk_alloc_callbacks );
      renderer->zbuf_n_cbuf_iv[fif_idx][fb_idx] = VK_NULL_HANDLE;
    }
  }

  for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
    if ( renderer->arr_renderpasses[rp_idx] ) {
      vkDestroyRenderPass( renderer->log_dev, renderer->arr_renderpasses[rp_idx],
          &vk_alloc_callbacks );
      renderer->arr_renderpasses[rp_idx] = VK_NULL_HANDLE;
    }
  }

  return emsg_unknown;
}


//-------------------------------------------------------------------------------------------------
enum err_msg expand_swapchain_and_framebuffer(
    struct cara_vk_renderer * renderer,
    union extent_2d cur_dims )
{
  unsigned int new_width = cur_dims.parts.width;
  if ( renderer->max_win_size.parts.width > new_width ) {
    new_width = renderer->max_win_size.parts.width;
  }
  new_width = _align_to_power_of_two( new_width, framebuf_dims_granularity );

  unsigned int new_height = cur_dims.parts.height;
  if ( renderer->max_win_size.parts.height > new_height ) {
    new_height = renderer->max_win_size.parts.height;
  }
  new_height = _align_to_power_of_two( new_height, framebuf_dims_granularity );

  //if ( new_width > renderer->max_win_size_avail.parts.width ) {
  //  new_width = renderer->max_win_size_avail.parts.width;
  //}
  //if ( new_height > renderer->max_win_size_avail.parts.height ) {
  //  new_height = renderer->max_win_size_avail.parts.height;
  //}

  VkImage * arr_swapchain_images = nullptr;
  VkImageView * arr_swapchain_img_views = nullptr;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFramebuffer new_framebuffers[cara_max_nr_fif][cara_nr_renderpasses] = {0};
  VkImageView arr_new_zbuf_n_cbuf_iv[cara_max_nr_fif][2] = {0};
  VkImage arr_new_zbuf_n_cbuf_img[cara_max_nr_fif][2] = {0};
  struct vk_mbind_info arr_new_mbind_infos_zbuf_and_cbuf[cara_max_nr_fif][2] = {0};
  uint32_t heap_idx = UINT_MAX_(heap_idx);
  unsigned int nr_swapchain_imgs = 0;

  VkResult result;


  //
  // Change framebuffers
  //
  VkFramebufferAttachmentImageInfo fb_att_img_info1[cara_nr_framebuffers - 1];
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( fb_att_img_info1 ); att_idx++ ) {
    fb_att_img_info1[att_idx].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
    fb_att_img_info1[att_idx].pNext = nullptr;
    fb_att_img_info1[att_idx].flags = 0;
    fb_att_img_info1[att_idx].width = new_width;
    fb_att_img_info1[att_idx].height = new_height;
    fb_att_img_info1[att_idx].layerCount = 1;
    fb_att_img_info1[att_idx].viewFormatCount = 1;
  }
  constexpr VkFormat rgba_unorm = VK_FORMAT_R8G8B8A8_UNORM;
  fb_att_img_info1[fb_cbuf].usage = //VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  fb_att_img_info1[fb_cbuf].pViewFormats = &rgba_unorm;

  const VkFormat depth_fmt = renderer->zbuf_fmt;
  fb_att_img_info1[fb_zbuf].usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  fb_att_img_info1[fb_zbuf].pViewFormats = &depth_fmt;

  VkFramebufferAttachmentImageInfo fb_att_img_info2[2];
  for ( unsigned int att_idx = 0; att_idx < _arr_elem_cnt( fb_att_img_info2 ); att_idx++ ) {
    fb_att_img_info2[att_idx].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
    fb_att_img_info2[att_idx].pNext = nullptr;
    fb_att_img_info2[att_idx].flags = 0;
    fb_att_img_info2[att_idx].width = new_width;
    fb_att_img_info2[att_idx].height = new_height;
    fb_att_img_info2[att_idx].layerCount = 1;
    fb_att_img_info2[att_idx].viewFormatCount = 1;
  }
  fb_att_img_info2[0].usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  fb_att_img_info2[0].pViewFormats = &depth_fmt;
  fb_att_img_info2[1].usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  fb_att_img_info2[1].pViewFormats = &renderer->swapchain_img_fmt;


  VkFramebufferAttachmentsCreateInfo fb_atts_create_info = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
    .pNext = nullptr,
  };

  VkFramebufferCreateInfo fb_create_info = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .pNext = &fb_atts_create_info,
    .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,  // Framebuffers are created with a small amount
                                                   // of additional metadata about the image views
                                                   // that will be used in
                                                   // `VkFramebufferAttachmentsCreateInfo`, and the
                                                   // actual image views are provided at render pass
                                                   // begin time via
                                                   // `VkRenderPassAttachmentBeginInfo`.
    .attachmentCount = _arr_elem_cnt( fb_att_img_info1 ),
    .pAttachments = nullptr,
    .width = new_width,
    .height = new_height,
    .layers = 1,
  };
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    fb_create_info.renderPass = renderer->arr_renderpasses[0];
    fb_atts_create_info.attachmentImageInfoCount = _arr_elem_cnt( fb_att_img_info1 );
    fb_atts_create_info.pAttachmentImageInfos = fb_att_img_info1;
    TRACE_BEGIN("vkCreateFramebuffer");
    result = vkCreateFramebuffer( renderer->log_dev, &fb_create_info, &vk_alloc_callbacks,
        &new_framebuffers[fif_idx][0] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateFramebuffer. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }

    fb_create_info.attachmentCount = _arr_elem_cnt( fb_att_img_info2 );
    fb_create_info.renderPass = renderer->arr_renderpasses[1];
    fb_atts_create_info.attachmentImageInfoCount = _arr_elem_cnt( fb_att_img_info2 );
    fb_atts_create_info.pAttachmentImageInfos = fb_att_img_info2;
    TRACE_BEGIN("vkCreateFramebuffer");
    result = vkCreateFramebuffer( renderer->log_dev, &fb_create_info, &vk_alloc_callbacks,
        &new_framebuffers[fif_idx][1] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateFramebuffer. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }



  //
  // Create actual framebuffers (a depth buffer and a color buffer)
  //

  // Create a depth buffer and a color buffer
  VkImageCreateInfo img_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = (VkExtent3D) { .width = new_width, .height = new_height, .depth = 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    img_create_info.format = renderer->zbuf_fmt;
    img_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    TRACE_BEGIN("vkCreateImage");
    result = vkCreateImage( renderer->log_dev,
        &img_create_info, &vk_alloc_callbacks, &arr_new_zbuf_n_cbuf_img[fif_idx][fb_zbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImage. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
    img_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    img_create_info.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    TRACE_BEGIN("vkCreateImage");
    result = vkCreateImage( renderer->log_dev,
        &img_create_info, &vk_alloc_callbacks, &arr_new_zbuf_n_cbuf_img[fif_idx][fb_cbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImage. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }


  // vkBindImageMemory
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int fb_idx = fb_zbuf; fb_idx <= fb_cbuf; fb_idx++ ) {
      // Obtain the requirements
      VkMemoryRequirements req;
      vkGetImageMemoryRequirements( renderer->log_dev, arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx], &req );

      // Choose the best memory type
      VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
      preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      required_props = 0;
      tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      uint32_t mtype_idx = ma_pick_best_mtype(
          &renderer->vmm, &req, required_props, preferred_props, tolerable_props );
      //const uint32_t heap_idx = renderer->vmm.arr_heap_indices[mtype_idx];

      // Bind the image to a vulkan memory object
      struct vk_mbind_info mbind_info;
      enum err_msg err = ma_bind_img_to_malloc( &renderer->vmm,
          &vk_alloc_callbacks, vk_malloc_wrapper,
          renderer->log_dev, arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx],
          mtype_idx, req.size, req.alignment, &mbind_info );
      if ( err ) {
        fprintf( stderr, "Error occured in ma_bind_img_to_malloc. (%s)\n", arr_emsg_txt[err] );
        goto stop_everything_after_failure;
      }
      arr_new_mbind_infos_zbuf_and_cbuf[fif_idx][fb_idx] = mbind_info;
    }
  }


  // Create VkImageViews for depth buffers and color buffers
  VkImageViewCreateInfo iv_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .components = (VkComponentMapping) {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY
    },
    .subresourceRange = (VkImageSubresourceRange) {
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    iv_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    iv_create_info.format = renderer->zbuf_fmt;
    iv_create_info.image = arr_new_zbuf_n_cbuf_img[fif_idx][fb_zbuf];
    TRACE_BEGIN("vkCreateImageView");
    result = vkCreateImageView( renderer->log_dev,
        &iv_create_info, &vk_alloc_callbacks, &arr_new_zbuf_n_cbuf_iv[fif_idx][fb_zbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImageView. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
    iv_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    iv_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    iv_create_info.image = arr_new_zbuf_n_cbuf_img[fif_idx][fb_cbuf];
    TRACE_BEGIN("vkCreateImageView");
    result = vkCreateImageView( renderer->log_dev,
        &iv_create_info, &vk_alloc_callbacks, &arr_new_zbuf_n_cbuf_iv[fif_idx][fb_cbuf] );
    TRACE_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImageView. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }



  //
  // Prepare swapchain images
  //
  VkSwapchainKHR old_swapchain;
  old_swapchain = renderer->swapchain;
  struct dead_swapchain * p_dead = infos_al_get_elem( &renderer->old_swapchains,
      &vk_alloc_callbacks, vk_malloc_wrapper, false, 0,
      renderer->nr_old_swapchains );
  p_dead->swapchain = old_swapchain;
  p_dead->destruct_time = atomic_load_explicit( &renderer->frame_id, memory_order_acquire ) +
    renderer->nr_swapchain_imgs + 240/4;  // TODO Add `avg_fps/4`
  renderer->nr_old_swapchains++;

  VkSwapchainCreateInfoKHR swapchain_create_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .surface = renderer->presentation_surface,
    .minImageCount = renderer->nr_swapchain_imgs_we_request,
    .imageFormat = renderer->swapchain_img_fmt,
    .imageColorSpace = renderer->swapchain_color_space,
    .imageExtent = (VkExtent2D) { .width = new_width, .height = new_height },
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,  // NOTE Or perhaps..
                                                            // renderer->vk_surface_transform_flags
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = renderer->present_mode,
    .clipped = VK_TRUE,  // Applications should set this value to VK_TRUE if they do not expect to
                         // read back the content of presentable images before presenting them or
                         // after reacquiring them, and if their fragment shaders do not have any
                         // side effects that require them to run for all pixels in the presentable
                         // image.
    .oldSwapchain = old_swapchain,
  };

  TRACE_BEGIN("vkCreateSwapchainKHR");
  result = vkCreateSwapchainKHR(
      renderer->log_dev,
      &swapchain_create_info,
      &vk_alloc_callbacks,
      &swapchain );
  TRACE_END();
  if ( result != VK_SUCCESS || swapchain == VK_NULL_HANDLE ) {
    fprintf( stderr, "[Vulkan] Could not create a swapchain.\n" );
    goto stop_everything_after_failure;
  }

  result = vkGetSwapchainImagesKHR( renderer->log_dev, swapchain, &nr_swapchain_imgs, nullptr );
  if ( result != VK_SUCCESS || nr_swapchain_imgs == 0 ) {
    fprintf( stderr, "[Vulkan] Could not get the number of swapchain images.\n" );
    goto stop_everything_after_failure;
  }
#ifdef DEBUG_MODE
  fprintf( stdout,
      "[Vulkan] The number of vulkan images in the swapchain is %u.\n",
      nr_swapchain_imgs );
#endif

  _vk_malloc_array( &vk_alloc_callbacks, &arr_swapchain_images, nr_swapchain_imgs );
  _vk_malloc_array( &vk_alloc_callbacks, &arr_swapchain_img_views, nr_swapchain_imgs );

  result = vkGetSwapchainImagesKHR( renderer->log_dev, swapchain, &nr_swapchain_imgs,
      arr_swapchain_images );
  if ( result != VK_SUCCESS && result != VK_INCOMPLETE ) {
    fprintf( stderr, "[Vulkan] Could not enumerate swapchain images.\n" );
    goto stop_everything_after_failure;
  }

  // Create VkImageViews for swapchain images
  iv_create_info.format = renderer->swapchain_img_fmt;
  iv_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  for ( unsigned int i = 0; i < nr_swapchain_imgs; i++ ) {
    arr_swapchain_img_views[i] = VK_NULL_HANDLE;
  }

  TRACE_BEGIN("vk_create_swapchain_ivs");
  for ( unsigned int i = 0; i < nr_swapchain_imgs; i++ ) {
    iv_create_info.image = arr_swapchain_images[i];
    result = vkCreateImageView( renderer->log_dev, &iv_create_info, &vk_alloc_callbacks,
        &arr_swapchain_img_views[i] );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred in vkCreateImageView. '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }
  TRACE_END();




  //
  // Update the buffer size
  //
  if ( renderer->max_win_size.parts.width < new_width ) {
    renderer->max_win_size.parts.width = new_width;
  }
  if ( renderer->max_win_size.parts.height < new_height ) {
    renderer->max_win_size.parts.height = new_height;
  }
  renderer->dims_swapchain_imgs = (VkExtent2D) {
    .width = new_width,
    .height = new_height,
  };


  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int fb_idx = 0; fb_idx <= fb_cbuf; fb_idx++ ) {
      // vkDestroyImage
      p_dead->arr_zbuf_n_cbuf[fif_idx][fb_idx] = renderer->zbuf_n_cbuf[fif_idx][fb_idx];
      VkMemoryRequirements req;
      vkGetImageMemoryRequirements( cara_vk.log_dev,
          renderer->zbuf_n_cbuf[fif_idx][fb_idx], &req );
      VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
      preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      required_props = 0;
      tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      p_dead->mtype[fif_idx][fb_idx] = ma_pick_best_mtype( &renderer->vmm,
          &req, required_props, preferred_props, tolerable_props );
      p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx] = renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx];
      p_dead->arr_zbuf_n_cbuf_iv[fif_idx][fb_idx] = renderer->zbuf_n_cbuf_iv[fif_idx][fb_idx];

      // Replace with the newly created objects
      renderer->zbuf_n_cbuf[fif_idx][fb_idx] = arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx];
      renderer->mbi_zbuf_n_cbuf[fif_idx][fb_idx] =
        arr_new_mbind_infos_zbuf_and_cbuf[fif_idx][fb_idx];
      renderer->zbuf_n_cbuf_iv[fif_idx][fb_idx] = arr_new_zbuf_n_cbuf_iv[fif_idx][fb_idx];
    }

    for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
      p_dead->arr_framebuffers[fif_idx][rp_idx] = renderer->arr_framebuffers[fif_idx][rp_idx];
      renderer->arr_framebuffers[fif_idx][rp_idx] = new_framebuffers[fif_idx][rp_idx];
    }
  }

  vk_alloc_callbacks.pfnFree( vk_alloc_callbacks.pUserData, renderer->arr_swapchain_imgs );
  for ( unsigned int i = 0; i < renderer->nr_swapchain_imgs; i++ ) {
    if ( renderer->arr_swapchain_iv[i] == VK_NULL_HANDLE ) {
      continue;
    }
    vkDestroyImageView( renderer->log_dev, renderer->arr_swapchain_iv[i], &vk_alloc_callbacks );
  }
  vk_alloc_callbacks.pfnFree( vk_alloc_callbacks.pUserData, renderer->arr_swapchain_iv );
  renderer->arr_swapchain_imgs = arr_swapchain_images;
  renderer->arr_swapchain_iv = arr_swapchain_img_views;
  renderer->swapchain = swapchain;

  // Return 0
  return emsg_no_error;


stop_everything_after_failure:
  // Destroy the newly created ones
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
      // vkDestroyFramebuffer
      if ( new_framebuffers[fif_idx][rp_idx] ) {
        vkDestroyFramebuffer( renderer->log_dev,
            new_framebuffers[fif_idx][rp_idx], &vk_alloc_callbacks );
        new_framebuffers[fif_idx][rp_idx] = VK_NULL_HANDLE;
      }
    }
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int fb_idx = 0; fb_idx <= fb_cbuf; fb_idx++ ) {
      // vkDestroyImage
      if ( arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx] == VK_NULL_HANDLE ) {
        continue;
      }
      // Should I just write down the memory type when binding to a memory object?
      VkMemoryRequirements req;
      vkGetImageMemoryRequirements( renderer->log_dev,
          arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx], &req );
      VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
      preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      required_props = 0;
      tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      enum err_msg err = ma_report_vk_malloc_rel( &renderer->vmm,
          &vk_alloc_callbacks, vk_malloc_wrapper,
          ma_pick_best_mtype(&renderer->vmm, &req,
            required_props, preferred_props, tolerable_props),
          arr_new_mbind_infos_zbuf_and_cbuf[fif_idx][fb_idx] );
      if ( err ) {
        fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d "
            "'%s'\n", __FILE__, __LINE__, arr_emsg_txt[err] );
        switch ( err ) {
          case emsg_already_done:
            [[fallthrough]];
          case emsg_unknown:  // VkDeviceMemory object is somehow missing
            vkDestroyImage( renderer->log_dev,
                arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx], &vk_alloc_callbacks );
            arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx] = VK_NULL_HANDLE;
            arr_new_mbind_infos_zbuf_and_cbuf[fif_idx][fb_idx].range = invalid_range;
            arr_new_mbind_infos_zbuf_and_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
            break;
          default:
            break;
        }
      }  // if ( err )
      else {
        vkDestroyImage( renderer->log_dev,
            arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx], &vk_alloc_callbacks );
        arr_new_zbuf_n_cbuf_img[fif_idx][fb_idx] = VK_NULL_HANDLE;
        arr_new_mbind_infos_zbuf_and_cbuf[fif_idx][fb_idx].range = invalid_range;
        arr_new_mbind_infos_zbuf_and_cbuf[fif_idx][fb_idx].malloc = VK_NULL_HANDLE;
      }

      // vkDestroyImageView
      if ( arr_new_zbuf_n_cbuf_iv[fif_idx][fb_idx] != VK_NULL_HANDLE ) {
        vkDestroyImageView( renderer->log_dev,
            arr_new_zbuf_n_cbuf_iv[fif_idx][fb_idx], &vk_alloc_callbacks );
        arr_new_zbuf_n_cbuf_iv[fif_idx][fb_idx] = VK_NULL_HANDLE;
      }
    }
  }

  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    for ( unsigned int rp_idx = 0; rp_idx < cara_nr_renderpasses; rp_idx++ ) {
      // vkDestroyFramebuffer
      if ( renderer->arr_framebuffers[fif_idx][rp_idx] ) {
        vkDestroyFramebuffer( renderer->log_dev,
            renderer->arr_framebuffers[fif_idx][rp_idx], &vk_alloc_callbacks );
        renderer->arr_framebuffers[fif_idx][rp_idx] = VK_NULL_HANDLE;
      }
    }
  }

  if ( arr_swapchain_img_views ) {
    for ( unsigned int i = 0; i < nr_swapchain_imgs; i++ ) {
      if ( arr_swapchain_img_views[i] == VK_NULL_HANDLE ) {
        continue;
      }
      vkDestroyImageView( renderer->log_dev, arr_swapchain_img_views[i], &vk_alloc_callbacks );
    }
    vk_alloc_callbacks.pfnFree( vk_alloc_callbacks.pUserData, arr_swapchain_img_views );
  }

  if ( arr_swapchain_images ) {
    vk_alloc_callbacks.pfnFree( vk_alloc_callbacks.pUserData, arr_swapchain_images );
  }

  return emsg_unknown;
}
