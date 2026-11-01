/*
 *
 * NOTE This code has been abandoned. I've given up on maintaining this code and started using volk
 *
 */

#include <stdio.h>
#include <string.h>

#include "appdev_support-shared_library.h"
#include "vulkan_functions.h"



#define EXPORTED_VULKAN_FUNCTION( name ) PFN_##name name;
#define GLOBAL_LEVEL_VULKAN_FUNCTION( name ) PFN_##name name;
#define INSTANCE_LEVEL_VULKAN_FUNCTION( name ) PFN_##name name;
#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( name, extension ) PFN_##name name;
#define DEVICE_LEVEL_VULKAN_FUNCTION( name ) PFN_##name name;
#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( name, extension ) PFN_##name name;
#include "list-vulkan_functions.inl"

//-------------------------------------------------------------------------------------------------
int vf_load_func_exported_from_vulkan_loader_library(shared_lib * vulkan_library) {
#define EXPORTED_VULKAN_FUNCTION( name )                                      \
  name = (PFN_##name)get_func_sym_from_shared_lib( vulkan_library, #name );   \
  if( (name) == 0 ) {                                                         \
    fprintf(stderr, "Could not load the function exported from Vulkan: %s\n", \
        #name );                                                              \
    return -1;                                                                \
  }

#include "list-vulkan_functions.inl"

  return 0;
}



//-------------------------------------------------------------------------------------------------
// Vulkan functions can be divided into three levels, which are global, instance, and device.
//  Device-level functions are used to perform typical operations such as drawing, shader-modules
//  creation, image creation, or data copying. Instance-level functions allow us to create logical
//  devices. To do all this, and to load device and instance-level functions, we need to create an
//  Instance. This operation is performed with global-level functions, which we need to load first.
int vf_load_global_level_functions() {
#define GLOBAL_LEVEL_VULKAN_FUNCTION( name )                                  \
  name = (PFN_##name)vkGetInstanceProcAddr( 0, #name );                       \
  if( name == 0 ) {                                                           \
    fprintf(stderr, "Could not load the global-level Vulkan function '%s'\n", \
        #name );                                                              \
    return -1;                                                                \
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#include "list-vulkan_functions.inl"
#pragma GCC diagnostic pop

  return 0;
}



//-------------------------------------------------------------------------------------------------
// How can we tell if a function is instance- or device-level? All device-level functions have
//  their first parameter of type VkDevice, VkQueue, or VkCommandBuffer. So, if a function doesn't
//  have such a parameter and is not from the global level, it is from an instance level.
int vf_load_instance_level_functions(VkInstance vk_instance,
    const char * const * enabled_extensions,
    uint_fast8_t nr_enabled_extensions) {
  // Load core Vulkan API instance-level functions
#define INSTANCE_LEVEL_VULKAN_FUNCTION( name )                                \
  name = (PFN_##name)vkGetInstanceProcAddr( vk_instance, #name );             \
  if( name == 0 ) {                                                           \
    fprintf(stderr, "Could not load the instance-level Vulkan function"       \
        " named '%s'.\n", #name );                                            \
    return -1;                                                                \
  }

  // Load instance-level functions from enabled extensions
  //
  // Instance-level extensions are responsible for managing, creating, and destroying a
  // presentation surface. It is a (cross-platform) representation of an application's window.
#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( name, extension )      \
  for( uint_fast8_t idx_extension = 0;                                        \
      idx_extension < nr_enabled_extensions; idx_extension++ ) {              \
    const char * enabled_extension = enabled_extensions[idx_extension];       \
    if ( strcmp(enabled_extension, extension) == 0 ) {                        \
      name = (PFN_##name)vkGetInstanceProcAddr( vk_instance, #name );         \
      if( name == 0 ) {                                                       \
        fprintf(stderr, "Could not load the instance-level Vulkan function"   \
            " named '%s'.", #name );                                          \
        return -1;                                                            \
      }                                                                       \
    }                                                                         \
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#include "list-vulkan_functions.inl"
#pragma GCC diagnostic pop

  return 0;
}



//-------------------------------------------------------------------------------------------------
// If we want to avoid unnecessary jumps and acquire function pointers corresponding directly to a
//  given device, we should do that by using a vkGetDeviceProcAddr(). This way, we can avoid the
//  intermediate function call and improve the performance of our application. Such an approach also
//  has some drawbacks: We need to acquire function pointers for each device created in an
//  application. If we want to perform operations on many different devices, we need a separate list
//  of function pointers for each logical device. We can't use functions acquired from one device
//  to perform operations on a different device. But using C language's preprocessor, it is quite
//  easy to acquire function pointers specific to a given device.
int vf_load_device_level_functions(VkDevice vk_device,
    const char * const * enabled_extensions,
    uint_fast8_t nr_enabled_extensions) {
  // Load core Vulkan API device-level functions
#define DEVICE_LEVEL_VULKAN_FUNCTION( name )                                  \
  name = (PFN_##name)vkGetDeviceProcAddr( vk_device, #name );                 \
  if( name == 0 ) {                                                           \
    fprintf(stderr, "Could not load the device-level Vulkan function"         \
        " named '%s'.\n", #name );                                            \
    return -1;                                                                \
  }

  // Load device-level functions from enabled extensions
#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( name, extension )        \
  for( uint_fast8_t idx_extension = 0;                                        \
      idx_extension < nr_enabled_extensions; idx_extension++ ) {              \
    const char * enabled_extension = enabled_extensions[idx_extension];       \
    if ( strcmp(enabled_extension, extension) == 0 ) {                        \
      name = (PFN_##name)vkGetDeviceProcAddr( vk_device, #name );             \
      if( name == 0 ) {                                                       \
        fprintf(stderr, "Could not load the device-level Vulkan function"     \
            " named '%s'.", #name );                                          \
        return -1;                                                            \
      }                                                                       \
    }                                                                         \
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#include "list-vulkan_functions.inl"
#pragma GCC diagnostic pop

  return 0;
}
