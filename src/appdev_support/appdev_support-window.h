#if !defined APPDEV_SUPPORT_WINDOW_H_
#define APPDEV_SUPPORT_WINDOW_H_


#include <vulkan/vulkan.h>

#ifdef CARBONARA_TARGET_PLATFORM_WIN32
#include <windows.h>
//#elif defined CARBONARA_TARGET_PLATFORM_XCB
#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB
#include <wayland-client.h>
#include "xcb/xcb.h"
#elif defined CARBONARA_TARGET_PLATFORM_XLIB
#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
#endif


//-------------------------------------------------------------------------------------------------
enum window_state_flag : unsigned char {
  window_state_fullscreen = 1,
};

struct ms_window {
#ifdef CARBONARA_TARGET_PLATFORM_WIN32
  // Not implemented yet.
  HINSTANCE           h_instance; ///< GetModuleHandle()
  HWND                h_wnd;      ///< CreateWindow()

#elif defined CARBONARA_TARGET_PLATFORM_XLIB
  // Not implemented
  Display *           xlib_display; ///< XOpenDisplay()
  Window              xlib_window;  ///< XCreateWindow()

//#elif defined CARBONARA_TARGET_PLATFORM_XCB
#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB
  union {
    struct {
      struct wl_display * wl_display;
      struct wl_surface * wl_surface;
      struct wl_compositor *compositor;
      struct xdg_surface *xdg_surface;
      struct xdg_wm_base *xdg_wm_base;
      struct xdg_toplevel *xdg_toplevel;
      uint8_t xdg_surface_has_been_configured;
      uint8_t closed;
    } wayland;
    struct {
      xcb_connection_t *  xcb_connection; ///< xcb_connect()
      xcb_window_t        xcb_window;     ///< xcb_generate_id()
    } xcb;
  } unix_window;

#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
  // Not implemented yet.
#endif
  uint8_t flags;
};


//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a window.
 *
 * \param window a struct ms_window to populate
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
uint8_t create_window( struct ms_window * window );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Destroy a window.
 *
 * \param window a struct ms_window to empty out.
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
uint8_t destroy_window( struct ms_window * window );

//-------------------------------------------------------------------------------------------------
VkResult create_presentation_surface( VkInstance vk_instance,
    struct ms_window * ms_window, VkSurfaceKHR * ptr_vk_surface );


#endif // APPDEV_SUPPORT_WINDOW_H_
