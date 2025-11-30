
//
//  NOTE This code is deprecated.
//

#include <stdio.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "appdev_support-window.h"
#include "appdev_support-shared_library.h"
#include "vulkan/vk_enum_string_helper.h"

#if !defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB_OR_XLIB
#if !defined CARBONARA_TARGET_PLATFORM_ANDROID
#if !defined CARBONARA_TARGET_PLATFORM_WIN32
#error No CARBONARA_TARGET_PLATFORM_PLATFORMNAME defined
#endif
#endif
#endif

#ifdef CARBONARA_TARGET_PLATFORM_WIN32
  // TODO implement it
#elif defined CARBONARA_TARGET_PLATFORM_XLIB
  // Not implemented
#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB
#define RANDNAME_PATTERN "/carbonara-XXXXXX"
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <wayland-client.h>
#include <string.h>
//#elif defined CARBONARA_TARGET_PLATFORM_XCB
#include <xcb/xproto.h>
#include <xcb/xcb.h>
#include "xdg-shell-client-protocol.h"
#define XDG_VERSION 6
struct WMSizeHints
{
  uint32_t flags;
  int32_t  x, y;
  int32_t  width, height;
  int32_t  min_width, min_height;
  int32_t  max_width, max_height;
  int32_t  width_inc, height_inc;
  int32_t  min_aspect_num, min_aspect_den;
  int32_t  max_aspect_num, max_aspect_den;
  int32_t  base_width, base_height;
  uint32_t win_gravity;
};

enum WMSizeHintsFlag : unsigned short
{
  WM_SIZE_HINT_US_POSITION   = 1U << 0,
  WM_SIZE_HINT_US_SIZE       = 1U << 1,
  WM_SIZE_HINT_P_POSITION    = 1U << 2,
  WM_SIZE_HINT_P_SIZE        = 1U << 3,
  WM_SIZE_HINT_P_MIN_SIZE    = 1U << 4,
  WM_SIZE_HINT_P_MAX_SIZE    = 1U << 5,
  WM_SIZE_HINT_P_RESIZE_INC  = 1U << 6,
  WM_SIZE_HINT_P_ASPECT      = 1U << 7,
  WM_SIZE_HINT_BASE_SIZE     = 1U << 8,
  WM_SIZE_HINT_P_WIN_GRAVITY = 1U << 9
};

#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
  // TODO implement it
#endif


#include "util-c_lang-basic.h"
#include "vulkan_functions.h"
#include "err_msg.h"




shared_lib library = 0;
#ifdef CARBONARA_TARGET_PLATFORM_WIN32
  // TODO implement it
#elif defined CARBONARA_TARGET_PLATFORM_XLIB
  // Not implemented
#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}


/*
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
  // struct client_state *state = data;
  xdg_surface_ack_configure(xdg_surface, serial);

  struct wl_buffer *buffer = draw_frame(state);
  wl_surface_attach(state->wl_surface, buffer, 0, 0);
  wl_surface_commit(state->wl_surface);
}
*/
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
  struct ms_window *state = data;
  xdg_surface_ack_configure(xdg_surface, serial);
  if (state->unix_window.wayland.xdg_surface_has_been_configured) {
    // TODO ms_window_resize(state);
  }
  state->unix_window.wayland.xdg_surface_has_been_configured = 1;
}
static constexpr struct xdg_surface_listener xdg_surface_listener = {
  .configure = xdg_surface_configure,
};

static constexpr struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
    const char *interface, uint32_t version) {
  struct ms_window *state = data;
  if (strcmp(interface, "wl_compositor") == 0) {
    state->unix_window.wayland.compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
  /* wl_shell is Deprecated
  } else if (strcmp(interface, "wl_shell") == 0) {
    shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
   */
  } else if (strcmp(interface, "xdg_wm_base") == 0) {
    state->unix_window.wayland.xdg_wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(state->unix_window.wayland.xdg_wm_base, &xdg_wm_base_listener, 0);
  }
//  else if (!strcmp(interface, "zxdg_shell_v6"))
//   _zxdg_shell_v6 = wl_registry_bind(registry, id, &zxdg_shell_v6_interface, 1);
}

static void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
  fprintf(stdout, "Got a registry losing event for %d\n", id);
}

static constexpr struct wl_registry_listener registry_listener = {
  global_registry_handler,
  global_registry_remover
};
//#elif defined CARBONARA_TARGET_PLATFORM_XCB
xcb_connection_t * (*fp_xcb_connect) (const char *displayname, int *screenp);
void (*fp_xcb_disconnect) (xcb_connection_t * c);
int (*fp_xcb_connection_has_error) (xcb_connection_t * c);
const xcb_setup_t * (*fp_xcb_get_setup) (xcb_connection_t * c);
xcb_screen_iterator_t (*fp_xcb_setup_roots_iterator) (const xcb_setup_t * R);
void (*fp_xcb_screen_next) (xcb_screen_iterator_t * i);
xcb_window_t (*fp_xcb_generate_id) (xcb_connection_t * c);
xcb_void_cookie_t (*fp_xcb_create_window_checked) (
    xcb_connection_t *  connection,   /* Pointer to the xcb_connection_t structure */
    uint8_t             depth,        /* Depth of the screen */
    xcb_window_t        wid,          /* Id of the window */
    xcb_window_t        parent,       /* Id of an existing window that should be the parent of the new window */
    int16_t             x,            /* X position of the top-left corner of the window (in pixels) */
    int16_t             y,            /* Y position of the top-left corner of the window (in pixels) */
    uint16_t            width,        /* Width of the window (in pixels) */
    uint16_t            height,       /* Height of the window (in pixels) */
    uint16_t            border_width, /* Width of the window's border (in pixels) */
    uint16_t            _class,
    xcb_visualid_t      visual,
    uint32_t            value_mask,
    const uint32_t *    value_list
    );
xcb_void_cookie_t (*fp_xcb_map_window) (xcb_connection_t * c, xcb_window_t window);
int (*fp_xcb_flush) (xcb_connection_t * c);
xcb_void_cookie_t (*fp_xcb_destroy_window) (xcb_connection_t * c, xcb_window_t window);
xcb_void_cookie_t (*fp_xcb_unmap_window) (xcb_connection_t * c, xcb_window_t window);
xcb_generic_error_t * (*fp_xcb_request_check) (xcb_connection_t * c, xcb_void_cookie_t cookie);
xcb_intern_atom_cookie_t (*fp_xcb_intern_atom) (xcb_connection_t *c,
    uint8_t only_if_exists, uint16_t name_len, const char *name);
xcb_intern_atom_reply_t * (*fp_xcb_intern_atom_reply) (xcb_connection_t *c,
    xcb_intern_atom_cookie_t cookie, xcb_generic_error_t **e);
xcb_void_cookie_t (*fp_xcb_change_property_checked) (
    xcb_connection_t *c,
    uint8_t mode,
    xcb_window_t window,
    xcb_atom_t property,
    xcb_atom_t type,
    uint8_t format,
    uint32_t data_len,
    const void *data
    );
#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
  // TODO implement it
#endif





uint8_t create_window(struct ms_window * window) {
  uint8_t ret = 0;
#ifdef CARBONARA_TARGET_PLATFORM_WIN32
  // TODO implement it
#elif defined CARBONARA_TARGET_PLATFORM_XLIB
  // Not implemented
#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB
  if ( getenv("WAYLAND_DISPLAY") ) {
    struct wl_display * wl_display = nullptr;
    wl_display = wl_display_connect(0);
    if ( wl_display == 0 ) {
      fprintf(stderr, "Can't connect to display\n");
      return err_msg_conn_refused;
    }
#if DEBUG_MODE
    fprintf(stdout, "Connected to display\n");
#endif

    struct wl_registry *registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(registry, &registry_listener, window);

    wl_display_dispatch(wl_display);
    wl_display_roundtrip(wl_display);

    if ( window->unix_window.wayland.compositor == 0 ) {
      fprintf(stderr, "Can't find a wayland compositor\n");
      ret = err_msg_not_found;
      goto destroy_and_return;
    }
#if DEBUG_MODE
    fprintf(stdout, "Found a wayland compositor\n");
#endif

    if ( window->unix_window.wayland.xdg_wm_base == 0 ) {
      fprintf(stderr, "Haven't got an XDG WM Base\n");
      ret = err_msg_unknown;
      goto destroy_and_return;
    }
#if DEBUG_MODE
    fprintf(stdout, "Created an XDG WM Base\n");
#endif

    struct wl_surface * wl_surface = wl_compositor_create_surface(window->unix_window.wayland.compositor);
    if ( wl_surface == 0 ) {
      fprintf(stderr, "Can't create a wayland surface\n");
      ret = err_msg_unknown;
      goto destroy_and_return;
    }
#if DEBUG_MODE
    fprintf(stdout, "Created a wayland surface\n");
#endif

    /* wl_shell is deprecated.

    if ( shell ) {
      shell_surface = wl_shell_get_shell_surface(shell, wl_surface);
      if (shell_surface == NULL) {
        fprintf(stderr, "Can't create shell surface\n");
        return err_msg_unknown;
      } 
      fprintf(stdout, "Created shell surface\n");
      
      wl_shell_surface_set_toplevel(shell_surface);

      struct wl_shell_surface *shell_surface;
      shell_surface = wl_shell_get_shell_surface(shell, wl_surface);
      wl_shell_surface_set_toplevel(shell_surface);
    }
    */

    window->unix_window.wayland.xdg_surface = xdg_wm_base_get_xdg_surface(window->unix_window.wayland.xdg_wm_base, wl_surface);
    if ( window->unix_window.wayland.xdg_surface == 0 ) {
      fprintf(stderr, "Can't create an XDG surface\n");
      ret = err_msg_unknown;
      goto destroy_and_return;
    }
#if DEBUG_MODE
    fprintf(stdout, "Created an XDG surface\n");
#endif

    xdg_surface_add_listener(window->unix_window.wayland.xdg_surface, &xdg_surface_listener, window);





    window->unix_window.wayland.xdg_toplevel = xdg_surface_get_toplevel(window->unix_window.wayland.xdg_surface);
    xdg_toplevel_set_title(window->unix_window.wayland.xdg_toplevel, "Carbonara 0-1");
    wl_surface_commit(wl_surface);

    window->unix_window.wayland.wl_surface = wl_surface;
    window->unix_window.wayland.wl_display = wl_display;

    return 0;

destroy_and_return:
    if ( wl_display ) wl_display_disconnect(wl_display);  // wl_display_flush(wl_display)
    if ( window->unix_window.wayland.compositor ) wl_compositor_destroy(window->unix_window.wayland.compositor);
    if ( wl_surface ) wl_surface_destroy(wl_surface);
    // XXX Should I call a function to clean up xdg_wm_base? I imagine I could do something like this:
    // if ( window->xdg_wm_base ) wl_registry_destroy(window->registry_xdg_wm_base);
    if ( window->unix_window.wayland.xdg_surface ) xdg_surface_destroy( window->unix_window.wayland.xdg_surface );

    return ret;




    // if ( getenv("WAYLAND_DISPLAY") )
  } else {  // in case of XCB

    if ( !library ) {
      char * name_shared_library = "libxcb.so";
      library = load_shared_lib(name_shared_library);
      if ( !library ) {
        fprintf(stderr, "ERROR: Failed to load '%s'\n", name_shared_library);
        return err_msg_failed_to_load_library;
      }
      fp_xcb_connect = get_func_sym_from_shared_lib( library, "xcb_connect");
      fp_xcb_disconnect = get_func_sym_from_shared_lib( library, "xcb_disconnect");
      fp_xcb_connection_has_error = get_func_sym_from_shared_lib( library, "xcb_connection_has_error");
      fp_xcb_get_setup = get_func_sym_from_shared_lib( library, "xcb_get_setup");
      fp_xcb_setup_roots_iterator = get_func_sym_from_shared_lib( library, "xcb_setup_roots_iterator");
      fp_xcb_screen_next = get_func_sym_from_shared_lib( library, "xcb_screen_next");
      fp_xcb_generate_id = get_func_sym_from_shared_lib( library, "xcb_generate_id");
      fp_xcb_create_window_checked = get_func_sym_from_shared_lib( library, "xcb_create_window_checked");
      fp_xcb_map_window = get_func_sym_from_shared_lib( library, "xcb_map_window");
      fp_xcb_flush = get_func_sym_from_shared_lib( library, "xcb_flush");
      fp_xcb_destroy_window = get_func_sym_from_shared_lib( library, "xcb_destroy_window");
      fp_xcb_unmap_window = get_func_sym_from_shared_lib( library, "xcb_unmap_window");
      fp_xcb_request_check = get_func_sym_from_shared_lib( library, "xcb_request_check");
      fp_xcb_intern_atom = get_func_sym_from_shared_lib( library, "xcb_intern_atom");
      fp_xcb_intern_atom_reply = get_func_sym_from_shared_lib( library, "xcb_intern_atom_reply");
      fp_xcb_change_property_checked = get_func_sym_from_shared_lib( library, "xcb_change_property_checked");
    }

    xcb_connection_t * xcb_conn;
    xcb_window_t xcb_window;
    int retval, idx_preferred_screen;
    xcb_conn = fp_xcb_connect(0, &idx_preferred_screen);
    if ( (retval = fp_xcb_connection_has_error(xcb_conn)) ) {
      fp_xcb_disconnect(xcb_conn);
      switch ( retval ) {
        case XCB_CONN_ERROR:
          //because of socket errors, pipe errors or other stream errors.
          ret = err_msg_conn_refused;
          break;
        case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
          // when extension not supported.
          ret = err_msg_ext_not_supported;
          break;
        case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
          // when memory not available.
          ret = err_msg_out_of_memory;
          break;
        case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
          // exceeding request length that server accepts.
          ret = err_msg_too_long;
          break;
        case XCB_CONN_CLOSED_PARSE_ERR:
          // error during parsing display string.
          ret = err_msg_parse_error;
          break;
        case XCB_CONN_CLOSED_INVALID_SCREEN:
          // because the server does not have a screen matching the display.
          ret = err_msg_wrong_name_given;
          break;
        default:
          ret = err_msg_unknown;
          break;
      }
      fprintf(stderr, "failed to connect to an X server: %s\n", err_msg_text[ret]);
      return ret;
    }

    const xcb_setup_t *setup = fp_xcb_get_setup (xcb_conn);
    xcb_screen_iterator_t iter = fp_xcb_setup_roots_iterator (setup);
    while(-7) {
#if DEBUG_MODE
      fprintf(stdout, "index: %u  root: %u\n", iter.index, iter.data->root);
      fprintf(stdout, "  width_in_millimeters: %hu\n", iter.data->width_in_millimeters);
      fprintf(stdout, "  height_in_millimeters: %hu\n", iter.data->height_in_millimeters);
      fprintf(stdout, "  width_in_pixels: %hu\n", iter.data->width_in_pixels);
      fprintf(stdout, "  height_in_pixels: %hu\n", iter.data->height_in_pixels);
      fprintf(stdout, "\n");
#endif
      if ( iter.rem == 0 ) {
        break;
      }
      fp_xcb_screen_next (&iter);
    }

    iter = fp_xcb_setup_roots_iterator (setup);
    for ( uint_fast8_t idx_screen = 0; idx_screen < idx_preferred_screen; idx_screen++ ) {
      fp_xcb_screen_next (&iter);
    }
    xcb_screen_t * screen = iter.data;

#if DEBUG_MODE
    fprintf(stdout, "This screen is picked:\n");
    fprintf(stdout, "  root: %u\n", screen->root);
    fprintf(stdout, "  width_in_millimeters: %hu\n", screen->width_in_millimeters);
    fprintf(stdout, "  height_in_millimeters: %hu\n", screen->height_in_millimeters);
    fprintf(stdout, "  width_in_pixels: %hu\n", screen->width_in_pixels);
    fprintf(stdout, "  height_in_pixels: %hu\n", screen->height_in_pixels);
    fprintf(stdout, "  white pixel: 0x%08x\n", screen->white_pixel);
    fprintf(stdout, "  black pixel: 0x%08x\n\n", screen->black_pixel);
#endif

    xcb_window = fp_xcb_generate_id(xcb_conn);
    uint32_t values[] = {
      //1, // XCB_CW_OVERRIDE_REDIRECT
      XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
      | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW
      | XCB_EVENT_MASK_POINTER_MOTION
      //| XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT
      //| XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_VISIBILITY_CHANGE
      //| XCB_EVENT_MASK_PROPERTY_CHANGE
      | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
      | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
      //| XCB_EVENT_MASK_RESIZE_REDIRECT
    };
    xcb_void_cookie_t cookie = fp_xcb_create_window_checked(
        xcb_conn,                       /* Connection          */
        XCB_COPY_FROM_PARENT,           /* depth               */
        xcb_window,                     /* window Id           */
        screen->root,                   /* parent window       */
        0, 0,                           /* x, y                */
        640, 480,                       /* width, height       */
        0,                              /* border_width        */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,  /* class               */
        screen->root_visual,            /* visual              */
        XCB_CW_EVENT_MASK,
        values );
    xcb_generic_error_t * xcb_error = fp_xcb_request_check(xcb_conn, cookie);
    if ( xcb_error ) {
      fprintf(stderr, "Failed to create an X window! (xcb error code = %hhu)\n",
          xcb_error->error_code);
      return err_msg_unknown;
    }

    xcb_atom_t window_type_atom, window_type_dock_atom;
    xcb_intern_atom_cookie_t atom_cookie = fp_xcb_intern_atom(
        xcb_conn, 0, util_strlen("_NET_WM_WINDOW_TYPE"), "_NET_WM_WINDOW_TYPE");
    xcb_intern_atom_reply_t *atom_reply = fp_xcb_intern_atom_reply(xcb_conn, atom_cookie, 0);
    if (!atom_reply) {
      fprintf(stderr, "Unable to set window type.\n");
    } else {
      window_type_atom = atom_reply->atom;
      free(atom_reply);

      atom_cookie = fp_xcb_intern_atom(
          xcb_conn, 0, util_strlen("_NET_WM_WINDOW_TYPE_DIALOG"), "_NET_WM_WINDOW_TYPE_DIALOG");
      atom_reply = fp_xcb_intern_atom_reply(xcb_conn, atom_cookie, 0);
      if (!atom_reply) {
        fprintf(stderr, "Unable to set window type.\n");
      } else {
        window_type_dock_atom = atom_reply->atom;
        free(atom_reply);
        cookie = fp_xcb_change_property_checked(
            xcb_conn,               // The connection.
            XCB_PROP_MODE_REPLACE,  // A bitmask of #xcb_prop_mode_t values.
            xcb_window,             // The window whose property you want to change.
            window_type_atom,       // The property you want to change (an atom).
            XCB_ATOM_ATOM,          // The type of the property you want to change (an atom).
            32,                     // Specifies whether the data should be viewed as a list of 8-bit, 16-bit or
                                    // 32-bit quantities. Possible values are 8, 16 and 32. This information allows
                                    // the X server to correctly perform byte-swap operations as necessary.
            sizeof(xcb_atom_t)/(32/8),  // Specifies the number of elements (see \a format).
            &window_type_dock_atom      // The property data.
            );
        xcb_generic_error_t * xcb_error = fp_xcb_request_check(xcb_conn, cookie);
        if ( xcb_error ) {
          fprintf(stderr, "Failed to set window type! (xcb error code = %hhu)\n",
              xcb_error->error_code);
          return err_msg_unknown;
        }
      }
    }

    struct WMSizeHints hints = {
      .flags       = WM_SIZE_HINT_P_WIN_GRAVITY | WM_SIZE_HINT_P_MIN_SIZE | WM_SIZE_HINT_P_MAX_SIZE,
      .win_gravity = XCB_GRAVITY_STATIC,
      .min_width = 640,
      .max_width = 640,
      .min_height = 480,
      .max_height = 480,
    };

    cookie = fp_xcb_change_property_checked(
        xcb_conn,
        XCB_PROP_MODE_REPLACE,
        xcb_window,
        XCB_ATOM_WM_NORMAL_HINTS,
        XCB_ATOM_WM_SIZE_HINTS,
        32,
        sizeof(struct WMSizeHints) / (32/8),
        &hints);
    xcb_error = fp_xcb_request_check(xcb_conn, cookie);
    if ( xcb_error ) {
      fprintf(stderr, "Failed to set change window properties! (xcb error code = %hhu)\n",
          xcb_error->error_code);
      return err_msg_unknown;
    }


    if ( window->flags & window_state_fullscreen ) {
      xcb_atom_t window_state_atom, window_state_fullscreen_atom;
      atom_cookie = fp_xcb_intern_atom( xcb_conn, 0,
          util_strlen("_NET_WM_STATE"), "_NET_WM_STATE");
      atom_reply = fp_xcb_intern_atom_reply(xcb_conn, atom_cookie, 0);
      if (!atom_reply) {
        fprintf(stderr, "Unable to set window state.\n");
      } else {
        window_state_atom = atom_reply->atom;
        free(atom_reply);

        atom_cookie = fp_xcb_intern_atom(
            xcb_conn, 0, util_strlen("_NET_WM_STATE_FULLSCREEN"), "_NET_WM_STATE_FULLSCREEN");
        atom_reply = fp_xcb_intern_atom_reply(xcb_conn, atom_cookie, 0);
        if (!atom_reply) {
          fprintf(stderr, "Unable to enter fullscreen mode.\n");
        } else {
          window_state_fullscreen_atom = atom_reply->atom;
          free(atom_reply);
          cookie = fp_xcb_change_property_checked(
              xcb_conn,               // The connection.
              XCB_PROP_MODE_REPLACE,  // A bitmask of #xcb_prop_mode_t values.
              xcb_window,             // The window whose property you want to change.
              window_state_atom,      // The property you want to change (an atom).
              XCB_ATOM_ATOM,          // The type of the property you want to change (an atom).
              32,                     // Specifies whether the data should be viewed as a list of 8-bit, 16-bit or
                                      // 32-bit quantities. Possible values are 8, 16 and 32. This information allows
                                      // the X server to correctly perform byte-swap operations as necessary.
              sizeof(xcb_atom_t)/(32/8),    // Specifies the number of elements (see \a format).
              &window_state_fullscreen_atom // The property data.
              );
          xcb_generic_error_t * xcb_error = fp_xcb_request_check(xcb_conn, cookie);
          if ( xcb_error ) {
            fprintf(stderr, "Failed to set window state! (xcb error code = %hhu)\n",
                xcb_error->error_code);
            return err_msg_unknown;
          }
        }
      }
    }

    // Unhide the window
    fp_xcb_map_window(xcb_conn, xcb_window);
    fp_xcb_flush(xcb_conn);

    window->unix_window.xcb.xcb_connection = xcb_conn;
    window->unix_window.xcb.xcb_window = xcb_window;
  }



#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
  // TODO implement it
#endif


  return ret;
}





VkResult create_presentation_surface(VkInstance vk_instance,
    struct ms_window * window, VkSurfaceKHR * vk_surface) {
  VkResult vk_result;

#ifdef CARBONARA_TARGET_PLATFORM_WIN32
  VkWin32SurfaceCreateInfoKHR surface_create_info = {
    VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // VkStructureType              sType
    0,                                                // const void *                 pNext
    0,                                                // VkWin32SurfaceCreateFlagsKHR flags
    window->h_instance,                            // HINSTANCE                    hinstance
    window->h_wnd                                  // HWND                         hwnd
  };

  vk_result = vkCreateWin32SurfaceKHR( vk_instance, &surface_create_info, 0, vk_surface );

#elif defined CARBONARA_TARGET_PLATFORM_XLIB
  VkXlibSurfaceCreateInfoKHR surface_create_info = {
    VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,   // VkStructureType              sType
    0,                                                // const void *                 pNext
    0,                                                // VkXlibSurfaceCreateFlagsKHR  flags
    window->xlib_display,                          // Display *                    dpy
    window->xlib_window                            // Window                       window
  };

  vk_result = vkCreateXlibSurfaceKHR( vk_instance, &surface_create_info, 0, vk_surface );

#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB
  if ( getenv("WAYLAND_DISPLAY") ) {
    VkWaylandSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,  // VkStructureType            sType
      0,                                                // const void *                 pNext
      0,                                                // VkWaylandSurfaceCreateFlagsKHR   flags
      window->unix_window.wayland.wl_display,           // struct wl_display*           display
      window->unix_window.wayland.wl_surface            // struct wl_surface*           surface
    };

    vk_result = vkCreateWaylandSurfaceKHR( vk_instance, &surface_create_info, 0, vk_surface );
    } else {  // #elif defined CARBONARA_TARGET_PLATFORM_XCB
    VkXcbSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,    // VkStructureType              sType
      0,                                                // const void *                 pNext
      0,                                                // VkXcbSurfaceCreateFlagsKHR   flags
      window->unix_window.xcb.xcb_connection,           // xcb_connection_t *           connection
      window->unix_window.xcb.xcb_window                // xcb_window_t                 window
    };

    vk_result = vkCreateXcbSurfaceKHR( vk_instance, &surface_create_info, 0, vk_surface );
  }

#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
  vk_result = VK_ERROR_FORMAT_NOT_SUPPORTED;

#endif

  if( vk_result != VK_SUCCESS ||
      vk_surface == VK_NULL_HANDLE ) {
    fprintf(stderr, "Could not create presentation surface. (%s)\n", string_VkResult(vk_result));
  }

  return vk_result;
}





uint8_t destroy_window(struct ms_window * window) {
#ifdef CARBONARA_TARGET_PLATFORM_WIN32
  return 1;
#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB
  if ( getenv("WAYLAND_DISPLAY") ) {
    if ( window->unix_window.wayland.wl_display ) {
      if ( window->unix_window.wayland.wl_surface ) {
        wl_display_disconnect(window->unix_window.wayland.wl_display);
        window->unix_window.wayland.wl_display = 0;
        window->unix_window.wayland.wl_surface = 0;
        return 0;
      }
    }
  } else {
//#elif defined CARBONARA_TARGET_PLATFORM_XCB
    if ( window->unix_window.xcb.xcb_connection ) {
      if ( window->unix_window.xcb.xcb_window ) {
        // hide the window
        fp_xcb_unmap_window(window->unix_window.xcb.xcb_connection, window->unix_window.xcb.xcb_window);
        // calling xcb_destroy_window on a root window does nothing.
        fp_xcb_destroy_window(window->unix_window.xcb.xcb_connection, window->unix_window.xcb.xcb_window);

        window->unix_window.xcb.xcb_window = 0;
      }
      fp_xcb_flush(window->unix_window.xcb.xcb_connection);
      fp_xcb_disconnect(window->unix_window.xcb.xcb_connection);

      window->unix_window.xcb.xcb_connection = 0;
      return 0;
    }
  }
#endif
  return 1;
}
