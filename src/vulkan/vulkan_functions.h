#if !defined VULKAN_FUNCTIONS_H_
#define VULKAN_FUNCTIONS_H_



//#include "vulkan/vulkan.h"

//#ifdef CARBONARA_TARGET_PLATFORM_WIN32
//#include "vulkan/vulkan_win32.h"
////#elif defined CARBONARA_TARGET_PLATFORM_XCB
//#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB_OR_XLIB
//#include <xcb/xcb.h>
//#include <X11/Xlib.h>
////#include "vulkan/vulkan_wayland.h"
////#include "vulkan/vulkan_xcb.h"
////#include "vulkan/vulkan_xlib.h"
//#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
//#include "vulkan/vulkan_android.h"
//#endif

#include "vulkan/vulkan_core.h"
#include "appdev_support-shared_library.h"



#define EXPORTED_VULKAN_FUNCTION( name ) extern PFN_##name name;
#define GLOBAL_LEVEL_VULKAN_FUNCTION( name ) extern PFN_##name name;
#define INSTANCE_LEVEL_VULKAN_FUNCTION( name ) extern PFN_##name name;
#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( name, extension ) extern PFN_##name name;
#define DEVICE_LEVEL_VULKAN_FUNCTION( name ) extern PFN_##name name;
#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( name, extension ) extern PFN_##name name;
#include "list-vulkan_functions.inl"



//-------------------------------------------------------------------------------------------------
/**
 * \brief Load vkGetInstanceProcAddr exported from a vulkan library.
 *
 * \param vulkan_library The library from which functions will be exported
 * \return 0 if it was successful, 1 otherwise.
 */
int vf_load_func_exported_from_vulkan_loader_library(shared_lib * vulkan_library);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Load global-level functions which are needed for instance creation.
 *
 * \return 0 if it was successful, 1 otherwise.
 */
int vf_load_global_level_functions(void);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Load instance-level functions which are needed for logical device creation.
 *
 * \param vk_instance VkInstance object for the application
 * \param enabled_extensions The names of the extensions to enable
 * \param nr_enabled_extensions The number of the names of the extensions to enable
 * \return 0 if it was successful, 1 otherwise.
 */
int vf_load_instance_level_functions(VkInstance vk_instance,
    const char * const * enabled_extensions, uint_fast8_t nr_enabled_extensions);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Load device-level functions.
 *
 * \param vk_device VkDevice object for which the function will work
 * \param enabled_extensions The names of the extensions to enable
 * \param nr_enabled_extensions The number of the names of the extensions to enable
 * \return 0 if it was successful, 1 otherwise.
 */
int vf_load_device_level_functions(VkDevice vk_device,
    const char * const * enabled_extensions, uint_fast8_t nr_enabled_extensions);



#endif // VULKAN_FUNCTIONS_H_
