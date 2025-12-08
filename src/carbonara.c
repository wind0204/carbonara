#include <hwloc/bitmap.h>  // FIXME remove this
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
    || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  #ifdef _MSC_VER
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
#endif


#if defined(__linux)
  #include <linux/prctl.h>  // Definition of PR_* constants
  #include <sys/prctl.h>  // prctl function, delete it when you've migrated to SDL threads.
#elif defined(_WIN32)
#endif


#if defined(ENABLE_PROFILING)
  #include <spall.h>
  #include <util-spall.h>
  static SpallBuffer spall_buf_for_main;
#else  // defined(ENABLE_PROFILING)
  #define SPALL_BEGIN( name_tag )
  #define SPALL_END()
#endif  // defined(ENABLE_PROFILING)


#include <SDL3/SDL_video.h>
#include <SDL3/SDL_asyncio.h>
#include <SDL3/SDL.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>  // include SDL3/SDL_main.h in the source where "main" function is
                            // supposed to be defined in.

#include <sys/cdefs.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

//#include <image_DXT.h>
#define TC_TEXTURE_LOAD_IMPLEMENTATION
#define TC_TEXTURE_LOAD_VULKAN_FORMATS
#include <tc_texture_load.h>


#include "carbonara-job.h"
#include "carbonara-singleton.h"
#include "carbonara-config.h"
#include "carbonara-mm.h"
#include "list-jobs.h"
#include "compute-magic.h"
#include "carbonara.h"
#include "appdev_support-processor_map.h"
#include "appdev_support-thread.h"
#include "memorable_arena.h"
#include "err_msg.h"
#include "carbonara-graphics.h"
#include "info_struct.h"
#include "util-hwloc.h"
#include "util-c_lang-basic.h"
#include "appdev_support-timer.h"
//#include "vulkan_functions.h"
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
    || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  #include "util-c_lang-x86.h"
#endif


//-------------------------------------------------------------------------------------------------

struct carbonara cara_single;
static thread * threads;
static char * asset_buf1;
static char * asset_buf2;
static char * asset_buf3;
static char * asset_buf4;
static char * asset_buf5;
static char * asset_buf6;


//-------------------------------------------------------------------------------------------------

// The maximum number of periodically executed jobs
constexpr size_t max_nr_pjobs = 512 - 1;

static constexpr unsigned int test_width = 1600;
static constexpr unsigned int test_height = 900;


//-------------------------------------------------------------------------------------------------

static enum err_msg verbosee_integrity_check_bc7_2048( const char * data );


//-------------------------------------------------------------------------------------------------
static void handle_sigint( int sig );
static void handle_sigsegv( int sig );
static void worker_loop( struct cara_worker_state * worker_state );

/**
 * \brief Launch a new worker.
 */
static void * launch_worker( void * arg );

/**
 * \brief Initialize the SDL part of the engine
 */
static uint8_t init_sdl(void);


//-------------------------------------------------------------------------------------------------
void jf_police_patrol( [[maybe_unused]] struct cara_worker_state * ws )
{
  const unsigned int mxcsr = _mm_getcsr();
  if ( (mxcsr & _MM_FLUSH_ZERO_ON) ) {
    fprintf( stderr, "[Debug] F" );
  }
  if ( (mxcsr & _MM_DENORMALS_ZERO_ON)) {
    fprintf( stderr, "[Debug] D" );
  }
  if ( (mxcsr & _MM_ROUND_MASK) != _MM_ROUND_NEAREST ) {
    fprintf( stderr, "[Debug] R" );
  }
  assume( !(mxcsr & _MM_FLUSH_ZERO_ON) );
  assume( !(mxcsr & _MM_DENORMALS_ZERO_ON) );
  assume( (mxcsr & _MM_ROUND_MASK) == _MM_ROUND_NEAREST );

  //uint16_t fpu_control_word;
}


//-------------------------------------------------------------------------------------------------
void SDL_AppQuit( [[maybe_unused]] void *appstate, SDL_AppResult result )
{
  SPALL_BEGIN("stuff_done_in_SDL_AppQuit");

  // You do not need to call SDL_Quit in this function, as SDL will call it after this function
  // returns and before the process terminates, but it is safe to explicitly do so.
  switch ( result ) {
    case SDL_APP_FAILURE:
      break;
    case SDL_APP_SUCCESS:
      clear_processor_map(cara_single.proc_map);
      ma_free(cara_single.mem_arena, 0, cara_single.proc_map);

      remove_appdev_support_thread();

      ma_end_arena(cara_single.mem_arena);
      break;
    default:
      unreachable();
  }

  // TODO something.


  SPALL_END();  // stuff_done_in_SDL_AppQuit

#ifdef ENABLE_PROFILING
  // Remember to quit or flush your buffers before you exit!
  // If you don't, you might not get the last few events waiting to be written
  spall_buffer_quit(&spall_ctx, &spall_buf_for_main);

  // Freeing our buffer's memory now that spall_buffer_quit has flushed it for us.
  free( spall_buf_for_main.data );

  spall_quit(&spall_ctx);
#endif
}


//-------------------------------------------------------------------------------------------------
SDL_AppResult SDL_AppEvent( [[maybe_unused]] void *appstate, [[maybe_unused]] SDL_Event *event )
{
  // If this function returns SDL_APP_FAILURE, SDL will call SDL_AppQuit and terminate the process
  // with an exit code that reports an error to the platform.
  // If it returns SDL_APP_SUCCESS, SDL calls SDL_AppQuit and terminates with an exit code that
  // reports success to the platform.

  // TODO a kind of "swapchain" buffers or a circular buffer?

  return SDL_APP_CONTINUE;
}


//-------------------------------------------------------------------------------------------------
SDL_AppResult SDL_AppIterate( [[maybe_unused]] void *appstate )
{
  static uint64_t a = 0;


  const uint_least32_t the_sign = atomic_load_explicit(
      &cara_single.sign_to_end, memory_order_acquire );
  if ( unlikely(the_sign > 128) ) {
    return SDL_APP_SUCCESS;
  }

  /*

  static uint_fast8_t is_frame_id_even = 0;

  //if ( atomic_load_explicit(&cara_vulkan.tmp_var_ready, memory_order_acquire) == 0 ) {
  //  hold_up_thread( 1000 * 100 );
  //  return SDL_APP_CONTINUE;
  //}


  //
  // TODO Check timestamps
  //


//-------------------------------------------------------------------------------------------------
  // TODO Write code here for now but move them out of SDL_AppIterate later.
  VkResult result;

  //
  // Prepare a VkCommandBufferBeginInfo
  //

  // Primary command buffers can call (execute) commands recorded in the secondary command buffers.
  // Executed secondary command buffers don't inherit the state from the primary command buffers
  // that executed them. What's more, the state of the primary command buffer is also undefined
  // after the execution of the secondary command buffer is recorded (when we record a primary
  // command buffer, and execute a secondary command buffer in it, and we want to continue
  // recording the primary command buffer, we need to set its state once again).
  // There is only one exception to the state inheritance rule - when the primary command buffer is
  // inside a render pass and we execute a secondary command buffer from it, the primary command
  // buffer's render pass and subpass states are preserved.
  VkCommandBufferBeginInfo cmd_buf_begin_info;
  cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_buf_begin_info.pNext = nullptr;
  cmd_buf_begin_info.flags =
    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |  // Each recording of the command buffer will
                                                   // only be submitted once, and the command
                                                   // buffer will be reset and recorded again
                                                   // between each submission.
    0;
  cmd_buf_begin_info.pInheritanceInfo = nullptr;
             // a pointer to a VkCommandBufferInheritanceInfo structure, used if `commandBuffer`
             // is a secondary command buffer. If this is a primary command buffer, then this
             // value is ignored.


  //
  // Start uploading data to the GPU
  //

  // Begin recording on the primary buffer
  VkCommandBuffer pri_trans_buf = cara_vk.trans_bufs.primary_cmd_buf;
  // TODO synchronization needed
  result = vkBeginCommandBuffer( pri_trans_buf, &cmd_buf_begin_info );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command(transfer) buffer recording operation. "
        "'%s'\n", string_VkResult(result) );
    return SDL_APP_FAILURE;
  }

  // TODO Record a transfer command or execute a secondary command buffer


  //
  // Acquire a swapchain image
  //
  uint32_t img_idx;
    // Host access to `swapchain` must be externally synchronized
    // Host access to `semaphore` must be externally synchronized
    // Host access to `fence` must be externally synchronized
  // TODO synchronization needed
  result = vkAcquireNextImageKHR( cara_vk.log_dev,
      cara_vk.swapchain,
      100000000,        // It specifies how long the function waits, in nanoseconds, if no image is
                        // available.
      cara_vk.wsi_semaphore_acquire, // VK_NULL_HANDLE or a semaphore to signal.
      VK_NULL_HANDLE,                         // VK_NULL_HANDLE or a fence to signal.
                        // WARNING semaphore and fence must not both be equal to VK_NULL_HANDLE
      &img_idx );
  // When we acquire an image, we still may not use it immediately for our purposes. We need to
  // wait for all previously submitted operations that referenced this image to finish. For this
  // purpose, a fence can be used, using which an application can check when it is safe to modify
  // an image. But we can also tell the driver that it should wait before it starts processing new
  // commands that use a given image. For this purpose, a semaphore is used, which, in general, is
  // a better option.
  switch ( result ) {
    default:
      //
      // TODO Recreate the swapchain? ( VK_ERROR_OUT_OF_DATE_KHR )
      //
      // images used by the presentation engine must have a VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      // layout. But if we want to render into an image, it must have a
      // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout. The operation of changing the layout is
      // called a transition (refer to the Setting an image memory barrier recipe from Chapter 4,
      // Resources and Memory).
      //
      break;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
      // You might want to change the viewport and scissor
      // TODO vkCmdSetViewport()
      // TODO vkCmdSetScissor()
      break;
  }



  result = vkEndCommandBuffer( pri_trans_buf );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(t) '%s'\n",
        string_VkResult(result) );
    return SDL_APP_FAILURE;
  }


  //
  // TODO Create a VkImageView for the swapchain image
  //


  //
  // Render the scene
  //
  is_frame_id_even = 1 ^ is_frame_id_even;
  atomic_store_explicit(
      &cara_single.is_frame_id_even, is_frame_id_even, memory_order_release );

  // The commented lines below are about getting a secondary command buffer:
  //constexpr uint_fast8_t worker_idx = foobar;
  //VkCommandBuffer cmd_buf = cara_get_arr_command_buffers_for_graphics(
  //    cara_single.proc_map->nr_processors, is_frame_id_even )[
  //  worker_idx * cara_max_nr_subpasses];
  VkCommandBuffer pri_cmd_buf =
    cara_vk.arr_frame_infos[is_frame_id_even].primary_cmd_buf;
fprintf( stderr, "[DEBUG] cmd_buf=%lu; cara_vk.arr_framebuffers[%u].primary_command_buffer=%lu\n",
  ptr2ui(pri_cmd_buf),
  is_frame_id_even,
  ptr2ui(cara_vk.arr_frame_infos[is_frame_id_even].primary_cmd_buf) );
  // TODO synchronization needed
  result = vkBeginCommandBuffer( pri_cmd_buf, &cmd_buf_begin_info );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command buffer recording operation. '%s'\n",
        string_VkResult(result) );
    return SDL_APP_FAILURE;
  }

  // TODO Draw background
  // TODO Draw a triangle
  // For performance reasons, we should submit as many batches as possible in as few function calls as possible.
  // We shouldn't submit command buffers if they were already submitted and their execution hasn't ended yet. We can do this only when command buffers were recorded with a VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT flag, but we should avoid using this flag for performance reasons.
  // TODO Draw a pyramid
  // TODO Pan the camera
  // TODO Move the pyramid
  // TODO Create multiple light sources
  // TODO Cast shadows
  // TODO Move the light sources


  VkSemaphoreSubmitInfoKHR wait_semaphores[1];
  for ( unsigned int i = 0; i < _arr_elem_cnt(wait_semaphores); i++ ) {
    wait_semaphores[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    wait_semaphores[i].pNext = nullptr;
    wait_semaphores[i].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    wait_semaphores[i].deviceIndex = 0;
  }
  wait_semaphores[0].semaphore = cara_vk.main_semaphore_before_A[is_frame_id_even];
  wait_semaphores[0].value = csb_rast_done;

  VkSemaphoreSubmitInfoKHR signal_semaphores[2];
  for ( unsigned int i = 0; i < _arr_elem_cnt(signal_semaphores); i++ ) {
    signal_semaphores[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    signal_semaphores[i].pNext = nullptr;
    signal_semaphores[i].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    signal_semaphores[i].deviceIndex = 0;
  }
  signal_semaphores[0].semaphore = cara_vk.main_semaphore_after_A[is_frame_id_even];
  signal_semaphores[0].value = csb_rast_done;
  signal_semaphores[1].semaphore = cara_vk.arr_wsi_semaphore_present[img_idx];
  signal_semaphores[1].value = 0;  // Ignored when it's a binary semaphore

  VkCommandBufferSubmitInfoKHR cmd_buf_submit_info;
  cmd_buf_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
  cmd_buf_submit_info.pNext = nullptr;
  cmd_buf_submit_info.commandBuffer = pri_cmd_buf;
  cmd_buf_submit_info.deviceMask = 0;

  VkSubmitInfo2 submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
  submit_info.pNext = nullptr;
  submit_info.flags = 0;
  submit_info.waitSemaphoreInfoCount = _arr_elem_cnt( wait_semaphores );
  submit_info.pWaitSemaphoreInfos = wait_semaphores;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &cmd_buf_submit_info;
  submit_info.signalSemaphoreInfoCount = _arr_elem_cnt( signal_semaphores );
  submit_info.pSignalSemaphoreInfos = signal_semaphores;

  // End recording
  // TODO synchronization needed
  result = vkEndCommandBuffer( pri_cmd_buf );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(c) '%s'\n",
        string_VkResult(result) );
    return SDL_APP_FAILURE;
  }

  // TODO synchronization needed
  result = vkQueueSubmit2KHR( cara_vk.graphics_q, 1, &submit_info, VK_NULL_HANDLE );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
        string_VkResult(result) );
    //infos_pm_destroy( &cara_vk.vmm.uploads, &ws->worker_info->idx_self, cara_free );
    return SDL_APP_FAILURE;
  }



  // Explicitly reset the command buffers instead of implicitly resetting by beginning to record
  constexpr bool release_resources = false;  // XXX what is this?
  // For transfer commands
  // TODO Host access to `commandPool` must be externally synchronized
  result = vkResetCommandPool(
      cara_vk.log_dev,
      cara_vk.trans_bufs.arr_cmd_pools[
        cara_single.proc_map->nr_processors ],
      release_resources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0 );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during command buffer reset. '%s'\n",
        string_VkResult(result) );
    return SDL_APP_FAILURE;
  }
  // For graphics commands
  // TODO Host access to `commandPool` must be externally synchronized
  result = vkResetCommandPool(
      cara_vk.log_dev,
      cara_vk.arr_frame_infos[is_frame_id_even].arr_cmd_pools[
        cara_single.proc_map->nr_processors ],
      release_resources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0 );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during command buffer reset. '%s'\n",
        string_VkResult(result) );
    return SDL_APP_FAILURE;
  }



//-------------------------------------------------------------------------------------------------
  // This part stays in SDL_AppIterate or the graphics leader

  //
  // TODO Present the rendered image
  //

  // TODO Before we can submit an image, we need to change its layout to a VK_IMAGE_LAYOUT_PRESENT_SRC_KHR or the presentation engine may be not able to correctly display such an image.

  // TODO Present one image per swapchain
  VkPresentInfoKHR present_info;
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &cara_vk.arr_wsi_semaphore_present[img_idx];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &cara_vk.swapchain;
  present_info.pImageIndices = &img_idx;
  present_info.pResults = nullptr;  // Applications that do not need per-swapchain results can
                                    // use NULL for pResults.

  */
  if ( a % 1000000 == 0 ) {
    fprintf( stdout, "[TEST] a = %lu\n", a );
    if ( a > 1000000 ) {
      raise(SIGHUP);
    }
  }
  a++;
  return SDL_APP_CONTINUE;
  /*

  // TODO Host access to queue must be externally synchronized
  result = vkQueuePresentKHR( cara_vk.graphics_q, &present_info );
  switch( result ) {
  case VK_SUCCESS:
    return SDL_APP_CONTINUE;
  default:
    fprintf( stderr, "[Carbonara] Error presenting an image. '%s'\n",
        string_VkResult(result) );
    return SDL_APP_FAILURE;
  }
  */

}


//-------------------------------------------------------------------------------------------------
SDL_AppResult SDL_AppInit(
    [[maybe_unused]] void **appstate,
    [[maybe_unused]] int argc,
    [[maybe_unused]] char *argv[] )
{
  // If this function returns SDL_APP_FAILURE, SDL will call SDL_AppQuit and terminate the process
  // with an exit code that reports an error to the platform.
  // If it returns SDL_APP_SUCCESS, SDL calls SDL_AppQuit and terminates with an exit code that
  // reports success to the platform.
  fprintf( stdout, "[TEST] %s %d-%d\n", Carbonara_APP_NAME, Carbonara_VERSION_MAJOR, Carbonara_VERSION_MINOR );
  //atomic_init( &cara_vk.tmp_var_ready, 0 );

#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64)\
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = init_util_c_lang();

#if defined UTILIZE_AVX2
  if ( !x86_proc_info.avx2 ) {
    fprintf( stderr, "[Carbonara] The CPU doesn't support AVX2 instructions.\n" );
    if ( x86_proc_info.sse42 ) {
      fprintf( stderr, "[Carbonara] Please run the SSE4.2 version instead.\n" );
    } else {
      fprintf( stderr,
          "[Carbonara] SSE4.2 is not supported either, The CPU is not supported!\n" );
    }
    //return 1;
    return SDL_APP_FAILURE;
  }

#elif defined UTILIZE_SSE4_2
  if ( x86_proc_info.avx2 ) {
    fprintf( stdout,
        "[Carbonara] The CPU does support AVX2, why did you executed SSE4.2 version?\n" );
    if ( x86_proc_info.sse42 ) {
      fprintf( stdout, "[Carbonara] launching the SSE4.2 version anyway. "
          "You might experience suboptimal performance.\n" );
    } else {
      fprintf( stderr,
          "[Carbonara] It's weird that SSE4.2 is not supported while AVX2 is. aborting!\n" );
      //return 1;
      return SDL_APP_FAILURE;
    }
  } else {
    if ( !x86_proc_info.sse42 ) {
      fprintf( stderr, "[Carbonara] SSE4.2 is not supported by the CPU. aborting!\n" );
      //return 1;
      return SDL_APP_FAILURE;
    }
  }
#endif

#if defined UTILIZE_BMI2
  if ( !x86_proc_info.bmi2 ) {
    fprintf( stderr, "[Carbonara] The CPU doesn't support BMI2 instructions. aboritng!\n" );
    //return 1;
    return SDL_APP_FAILURE;
  }
#endif

#endif

  enum err_msg err;
  err = init_appdev_support_thread();
  if ( err ) {
    fprintf( stderr, "Failed to initialize multi-threading utilities. (reason: %s)\n",
        err_msg_text[err] );
    //return 1;
    return SDL_APP_FAILURE;
  }

  init_appdev_support_processor_map();
  //init_graphics();
  init_info_struct( x86_proc_info.whole );

  atomic_init( &cara_single.sign_to_end, 0 );
  atomic_init( &cara_single.sign_to_start, 0 );
  err = init_carbonara( 16*1024*1024, x86_proc_info.whole );
  if ( err ) {
    fprintf( stderr, "Failed to launch. (reason: %s)\n", err_msg_text[err] );
    //return 1;
    return SDL_APP_FAILURE;
  }


  while( 128 >= atomic_load_explicit(&cara_single.sign_to_start, memory_order_acquire) ) {
    hold_up_thread( 1000*750 );
  }


  return SDL_APP_CONTINUE;
}


//-------------------------------------------------------------------------------------------------
static uint8_t init_sdl(void)
{
#ifdef DEBUG_MODE
  // By default the application and gpu categories are enabled at the INFO level, the
  // assert category is enabled at the WARN level, test is enabled at the VERBOSE level
  // and all other categories are enabled at the ERROR level.
  SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_AUDIO, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_ASSERT, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_RENDER, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG);
  SDL_SetLogPriority(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_DEBUG);
#endif

  if ( SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0") == 0 ) {
    fprintf( stderr, "[Carbonara] SDL didn't accept SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE! "
        "SDL_Error: %s\n", SDL_GetError() );
    return 1;
  }

  if ( SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1") == 0 ) {
    fprintf( stderr, "[Carbonara] SDL didn't accept SDL_HINT_NO_SIGNAL_HANDLERS! SDL_Error: %s\n",
        SDL_GetError() );
    return 1;
  }

  if ( SDL_SetHint(SDL_HINT_APP_NAME, Carbonara_APP_NAME) == 0 ) {
    fprintf( stderr, "[Carbonara] SDL didn't change SDL_HINT_APP_NAME! SDL_Error: %s\n",
        SDL_GetError() );
    return 1;
  }

  if ( SDL_Init( SDL_INIT_AUDIO |
        SDL_INIT_VIDEO |  // NOTE SDL_INIT_VIDEO initiates SDL_INIT_EVENTS as well
        SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK |
        0 ) == 0 ) {
    fprintf( stderr, "[Carbonara] SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
    return 1;
  }

  return 0;
}


//-------------------------------------------------------------------------------------------------
enum err_msg init_carbonara( uint32_t len_each_arena, uint64_t proc_info )
{
  //atomic_init(&cara_single.nr_job_arg_groups_taken, 0);
  atomic_init(&cara_single.nr_job_batch_descriptors_taken, 0);



  //
  // Prepare processor_map
  //
  cara_single.proc_map = prepare_processor_map();

  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = proc_info;
  x86_proc_info.large_page_size_class = cara_single.proc_map->large_page_size_class;
  assume(x86_proc_info.large_page_size_class != 0);
  proc_info = x86_proc_info.whole;

  cara_single.proc_info = proc_info;


  //
  // Prepare the time system and the performance traces
  //
  enum err_msg err;
#ifdef ENABLE_PROFILING
  err = init_appdev_support_timer(x86_proc_info.whole, 0);
  if ( err ) {
    fprintf( stderr, "[ADS-timer] err_msg from init_appdev_support_timer = \"%s\"\n",
        err_msg_text[err] );
    switch ( err ) {
      case err_msg_not_found:
        fprintf( stderr, "  We could not find /dev/cpu/0/msr\n" );
        break;
      case err_msg_permission_denied:
        fprintf( stderr, "  We could not determine the non-turbo frequency of the processor.\n" );
        break;
      default:
        unreachable();
    }
    fprintf( stderr, "Please contact the developer to get support.\n" );
    //return 1;
    return err;
  }

  const __float128 tsc_freq = nr_tsc_in_1s;
  const __float128 timestamp_to_ns = 1.0 / ( (1.0 / 1000000000.0) * tsc_freq );
  //const __float128 pc_freq = SDL_GetPerformanceFrequency();
  //const __float128 timestamp_to_ns = (1.0 / 1000000000.0) * pc_freq;

  constexpr char fpath_spall[] = "/dev/shm/Carb.spall";
  //constexpr char fpath_spall[] = "Carb.spall";
  /*
     init_file takes 2 things:
     - the name of your trace, ex: "hello_world.spall"
     - the timestamp_unit used to convert from your timer's timestamps to nanoseconds
     */
  if ( !spall_init_file(fpath_spall, (double)timestamp_to_ns, &spall_ctx) ) {
    fprintf( stderr, "[Tracer] failed to setup SPALL with '%s'\n", fpath_spall );
  }

  // The buffer for the "main" thread
  //constexpr size_t spall_buf_sz = 1 * 1024 * 1024;
  constexpr size_t spall_buf_sz = 512 * 1024;
  spall_buf_for_main = (SpallBuffer) {
    .pid = 0,
      .tid = UINT16_MAX,
      .length = spall_buf_sz,
      .data = malloc(spall_buf_sz),
  };

  // From advanced_threads_example_linux.c:
  /*
     Here's another neat trick:
     We're touching the pages ahead of time here, so we get a smoother trace.
     By pre-faulting all the pages in our event buffer, we avoid waiting for pages to load
     while user code runs. This can make a noticable difference for data consistency, especially
     with bigger buffers.
     */
  memset(spall_buf_for_main.data, 0, spall_buf_for_main.length);

  if ( !spall_buffer_init(&spall_ctx, &spall_buf_for_main) ) {
    fprintf( stderr, "[Worker] failed to init the spall buffer.\n" );
  }
  p_spall_buf = &spall_buf_for_main;
#endif  // ENABLE_PROFILING


  //
  // Init memory arena
  //

  SPALL_BEGIN( "init_carb-after_prepare_processor_map" );

  //size_t large_page_size = page_sizes[x86_proc_info.large_page_size_class];
  cara_single.mem_arena = aligned_alloc(CACHELINE_LEN, sizeof(memorable_arena_t));
  if ( !cara_single.mem_arena ) {
    fprintf( stderr, "[Carbonara] Error in allocation: %s\n", err_msg_text[err_msg_out_of_memory] );
    return err_msg_out_of_memory;
  }
  cara_single.mem_arena->id = 0;
  cara_single.len_each_arena = len_each_arena;
  err = ma_init_arena(
      proc_info,
      cara_single.mem_arena,
      cara_single.len_each_arena,
      cara_single.proc_map->nr_processors
      );
  // cara_single.mem_arena->head->arr_lrm
  if ( err ) {
    fprintf( stderr, "[Carbonara] Error in allocation: %s\n", err_msg_text[err] );
    return err;
  }

  struct ma_sub_arena * ma_head = ma_get_sub_arena(cara_single.mem_arena, 1);


  //
  // Temporary struct ms_worker_info and struct cara_worker_state
  //
  struct ms_worker_info tmp_worker_info;
  tmp_worker_info.cpuset = (hwloc_bitmap_t){0};
  tmp_worker_info.deployment_flags = 0;
  tmp_worker_info.idx_core = 0;
  tmp_worker_info.idx_missing_sibling = 0;
  tmp_worker_info.idx_one_sibling = 2;
  tmp_worker_info.idx_self = 0;
  tmp_worker_info.puid = 0;
  struct cara_worker_state tmp_ws;
  tmp_ws.current_batch = nullptr;
  tmp_ws.worker_info = &tmp_worker_info;
  tmp_ws.malloc_helper.worker_idx = 0;
  tmp_ws.malloc_helper.node_recently_used = cara_single.mem_arena->head;



  //
  // Prepare job list
  //
  constexpr uint_fast8_t elem_size = CACHELINE_LEN;
  assume( elem_size == sizeof(struct cara_job_batch_desc_64) );
  const uint32_t node_size_b4_rev = elem_size * cara_single.proc_map->nr_processors;
  uint32_t node_size = _align_to_power_of_two( node_size_b4_rev, ALIGN_FOR_AVX512 );
  node_size = _align_to_the_close_power_of_two( node_size );

  cara_single.job_list = infos_al_create(
      &tmp_ws.malloc_helper,
      cara_malloc,
      node_size, elem_size, 0
      );

  cara_init_job_list_node(cara_single.job_list->list.head.data);


  //
  // Keep the processor_map in the mm_memo_map
  //
  struct processor_map * p_orig_proc_map = cara_single.proc_map;
  cara_single.proc_map = ma_malloc(
      ma_head,
      _get_nr_units_to_cover(
        sizeof(struct processor_map) + p_orig_proc_map->nr_processors * (
          sizeof(struct ms_worker_info) + sizeof(struct u_hwloc_bitmap_s) ),
        CACHELINE_LEN ),
      0,
      0 );
  memcpy(
      cara_single.proc_map,
      p_orig_proc_map,
      sizeof(struct processor_map) + p_orig_proc_map->nr_processors * (
        sizeof(struct ms_worker_info) + sizeof(struct u_hwloc_bitmap_s))
      );

  cara_single.proc_map->processors = (struct ms_worker_info *)
    (cara_single.proc_map + 1);
  const uint_fast16_t offset_bitmap_loc = sizeof(struct processor_map) +
    p_orig_proc_map->nr_processors * sizeof(struct ms_worker_info);
  for ( unsigned int ui = 0; ui < p_orig_proc_map->nr_processors; ui++ ) {
    cara_single.proc_map->processors[ui].cpuset =
      (void*) cara_single.proc_map +
      offset_bitmap_loc + ui * sizeof(struct u_hwloc_bitmap_s);
    //assume ( ! cara_single.proc_map->processors[ui].user_data );
  }
  free(p_orig_proc_map);


#ifdef DEBUG_MODE
  fprintf( stdout, "[MA] capacity = %u\n", ma_head->capacity );
  fprintf( stdout, "[MA] nr_free_chunks = %u\n", ma_head->smm.total_space_left );
  //fprintf( stdout, "[MA] bitmap = mm+%lu\n", (uint8_t*)mm->bitmap - (uint8_t*)mm );
  //fprintf( stdout, "[MA] tip_dynamic = mm+%lu\n", (uint8_t*)mm->tip_dynamic - (uint8_t*)mm );
  //fprintf( stdout, "[MA] tip_grower = mm+%lu\n", (uint8_t*)mm->tip_grower - (uint8_t*)mm );
  //fprintf( stdout, "[MA] len_bitmap_aligned_part = %ld\n", mm->len_bitmap_aligned_part );
  //fprintf( stdout, "[MA] len_unaligned_part_front = %u\n", mm->len_unaligned_part_front );
  //fprintf( stdout, "[MA] len_unaligned_part_back = %u\n", mm->len_unaligned_part_back );
  fprintf( stdout, "[MA] addr. of the memo_map = [%#018lx]\n", ptr2ui(ma_head) );
#endif


  //
  // Write the very first job batch
  //
  struct cara_job_batch_desc_64 * batch = infos_al_get_elem(cara_single.job_list, 0);
  for ( uint_fast8_t job_idx = 1; job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
      job_idx++ ) {
    batch->job_func_idx[job_idx] = 0;
    batch->job_flags[job_idx] = 0;
  }
  batch->job_func_idx[0] = jf_idx_kickstart;
  batch->job_flags[0] = job_flag_foremans_task;
  /*
  batch->cache_location[0] = 0;
  batch->cache_pattern[0] = 0;
  batch->job_idx_func_arg_group[0] = UINT16_MAX;
  */
  atomic_store_explicit( &batch->progress, 0, memory_order_release );
  atomic_store_explicit( &batch->preferred_core, UINT8_MAX, memory_order_release );
  atomic_fetch_add_explicit( &cara_single.nr_job_batch_descriptors_taken, 1,
      memory_order_release );


  //
  // Prepare timers
  //

  // NOTE The core crystal clock frequency of the ART(Always Running Timer) in an amd64 CPU is
  // usually around 25MHz, 24MHz ~19.2MHz, which means the resolution is 40ns, 41.666ns or
  // 52.083ns and the least common multiple is 5000ns i.e. 5us.
  //__float128 pc_freq = SDL_GetPerformanceFrequency();
  __float128 pc_freq = get_performance_frequency();
  const uint64_t nr_pc_in_5us = (typeof(nr_pc_in_5us)) (pc_freq / 1000000 * 5);
  cara_single.nr_pc_in_5us = (typeof(nr_pc_in_5us)) nr_pc_in_5us;
  unsigned_division_magic_64(&cara_single.magic_divider, nr_pc_in_5us);

  const uint32_t leap_sec_freq = get_performance_frequency() / nr_pc_in_5us;
  // Do we want to deal with offsetting performance counter every leaf second?
  const uint32_t pc_ofs_per_sec = get_performance_frequency() % nr_pc_in_5us;
  if ( pc_ofs_per_sec ) {
    fprintf( stdout, "Please tell the developer about the issue:  "
        "pc_ofs_per_sec = %u;  leap_sec_freq = %u;\n", pc_ofs_per_sec, leap_sec_freq );
    return err_msg_system_not_supported;
  }
  assume( !pc_ofs_per_sec );

  atomic_init( &cara_single.cur_time_in_5us_general, 0 );

  cara_single.periodic_jobs_periods_5us = cara_malloc( &tmp_ws.malloc_helper,
      alignof(*cara_single.periodic_jobs_periods_5us),
      sizeof(*cara_single.periodic_jobs_periods_5us)*max_nr_pjobs );
  cara_single.periodic_jobs = cara_malloc( &tmp_ws.malloc_helper,
      alignof(*cara_single.periodic_jobs),
      sizeof(*cara_single.periodic_jobs)*max_nr_pjobs );

  constexpr uint64_t nr_5us_in_1ms = 1000 / 5;
  const uint64_t job_spawners_time_to_act[] = {
    1 + nr_5us_in_1ms * 500 * 1,
    1 + nr_5us_in_1ms * 500 * 2,
#if !defined(ARE_WE_SHIPPING_IT)
    1 + nr_5us_in_1ms * 500 * 3,
#endif
  };


  //
  // Prepare periodic jobs
  //
  unsigned int pjob_idx = 0;
  cara_single.periodic_jobs[pjob_idx] = jf_idx_maint_job_list;
  cara_single.periodic_jobs_periods_5us[pjob_idx] = 1000 * nr_5us_in_1ms;
  pjob_idx++;
  cara_single.periodic_jobs[pjob_idx] = jf_idx_maint_memo_maps;
  cara_single.periodic_jobs_periods_5us[pjob_idx] = 1000 * nr_5us_in_1ms;
#if !defined(ARE_WE_SHIPPING_IT)
  pjob_idx++;
  cara_single.periodic_jobs[pjob_idx] = jf_idx_police_patrol;
  cara_single.periodic_jobs_periods_5us[pjob_idx] = 2000 * nr_5us_in_1ms;
#endif

  struct infos_bst * bst = infos_bst_create( &tmp_ws.malloc_helper, cara_malloc,
      job_spawners_time_to_act,
      sizeof(job_spawners_time_to_act)/sizeof(*job_spawners_time_to_act),
      max_nr_pjobs,
      sizeof(*job_spawners_time_to_act) );
  cara_single.tree_of_times_to_act = bst;



  //
  // Prepare signal handlers
  //
  signal(SIGHUP, handle_sigint);
  signal(SIGINT, handle_sigint);
  signal(SIGQUIT, handle_sigint);
  signal(SIGTERM, handle_sigint);
  signal(SIGSEGV, handle_sigsegv);


  SPALL_END();  // after prepare_processor_map


  //
  // init SDL3 and vulkan
  //
  SPALL_BEGIN("init_sdl");
  if ( init_sdl() ) {
    fprintf( stderr, "[Carbonara] Could not initialize SDL.\n" );
    return err_msg_unknown;
  }
  SPALL_END();


  SPALL_BEGIN("init_vulkan");
  // TODO Set a prefered set of vendor_id_pref + device_id_pref if there is a saved preference data
  if ( init_vulkan(0, 0, cara_single.mem_arena, &tmp_ws) ) {
    fprintf( stderr, "[Carbonara] Could not initialize Vulkan.\n" );
    return err_msg_unknown;
  }
  const int ret = SDL_ShowWindow( cara_vk.window );
  if ( !ret ) {
    fprintf( stderr, "[Carbonara] Could not unhide the window. (%s)\n", SDL_GetError() );
  }
  SPALL_END();


  //
  // Load synchronously using an async api
  //

  // NOTE The proper way to synchronously using SDL AsyncIO is using SDL_LoadFileAsync or SDL_WaitAsyncIOResult instead of looping with SDL_GetAsyncIOResult (And when shutting down, call SDL_SignalAsyncIOQueue to unblock any sleeping threads despite there being no new tasks completed.)
  //
  // TODO do it asynchronously
  //
  SPALL_BEGIN( "Load stone.dds" );
  cara_vk.stone_texture = nullptr;
  cara_vk.sand_texture = nullptr;
  cara_vk.spv_simple_vert = nullptr;
  cara_vk.spv_simple_frag = nullptr;
  cara_vk.spv_depth_only = nullptr;
  cara_vk.spv_depth_only_frag = nullptr;
  cara_vk.asset_size = 0;
  constexpr char fpath_stone[] = "/home/dewr/Products/carbonara/assets/textures/stone-01.dds";
  constexpr char fpath_sand[] = "/home/dewr/Products/carbonara/assets/textures/sand-01.dds";
  constexpr unsigned int dds_header_size =
    4 + sizeof(TCTex_DDS_HEADER) + sizeof(TCTex_DDS_HEADER_DXT10);
  constexpr unsigned int len_mipmapped_2048 = 16 + 16 + (4*4)*4/4 + (8*8)*4/4 + (16*16)*4/4 +
    (32*32)*4/4 + (64*64)*4/4 + (128*128)*4/4 + (256*256)*4/4 + (512*512)*4/4 + (1024*1024)*4/4 + (2048*2048)*4/4;
  constexpr uint32_t magic_dw = (('D' << 8*0) | ('D' << 8*1) | ('S' << 8*2) | (' ' << 8*3));

  // How many queues should I maintain?
  SDL_AsyncIOQueue * my_io_queue = SDL_CreateAsyncIOQueue();

  // This call is not asynchronous; it will open the file before returning, under the assumption
  // that doing so is generally a fast operation. Future reads and writes to the opened file will
  // be async, however.
  SDL_AsyncIO * new_async_io = SDL_AsyncIOFromFile( fpath_stone, "r" );

  // TODO load the header first and/or cook the asset into a mini-header format
  auto file_size = SDL_GetAsyncIOSize( new_async_io );
  if ( file_size < 0 ) {
    fprintf( stderr, "[Carbonara] SDL_GetAsyncIOSize failed! reason:'%s'\n", SDL_GetError() );
    goto done_with_stone;
  }

  assume( file_size == dds_header_size + len_mipmapped_2048 );
  asset_buf1 = cara_malloc(
      &tmp_ws.malloc_helper, CACHELINE_LEN, len_mipmapped_2048 + dds_header_size );
  if ( !SDL_ReadAsyncIO(
        new_async_io,
        asset_buf1,
        0,
        UINT64_C(0) + file_size,
        my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed!\n" );
    goto done_with_stone;
  }
  while ( !SDL_CloseAsyncIO(new_async_io, false, my_io_queue, ui2ptr(555)) ) {
    hold_up_thread( 1000 * 100 );
  }

  bool loading_is_done = false;
  bool closing_is_done = false;
do_it_synchronoously_for_now:
  SDL_AsyncIOOutcome async_io_outcome;
  while ( !SDL_GetAsyncIOResult( my_io_queue, &async_io_outcome ) ) {
    // One millisecond
    hold_up_thread( 1000 * 1000 );
  }

  switch ( ptr2ui(async_io_outcome.userdata) ) {
    case 33:
      fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
          async_io_outcome.bytes_requested,
          async_io_outcome.bytes_transferred,
          async_io_outcome.offset );
      if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
        fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
            async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
        if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
          fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
        }
        goto done_with_stone;
      }
      loading_is_done = true;
      if ( !closing_is_done ) {
        goto do_it_synchronoously_for_now;
      }
      break;
    case 555:
      if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
        fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
            async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
      }
      closing_is_done = true;
      if ( loading_is_done ) {
        break;
      }
      [[fallthrough]];
    default:
      goto do_it_synchronoously_for_now;
  }

  uint32_t * p_dds_head = (void*)asset_buf1;
  if ( *p_dds_head != magic_dw ) {
    fprintf( stderr, "[Carbonara-Debug] DDS header magic value is wrong! "
        "%#010x instead of %#010x\n", *p_dds_head, magic_dw );
  } else {
    cara_vk.stone_texture = asset_buf1;
    cara_vk.asset_size = file_size;
  }

done_with_stone:
  // Stone done; Now round 2: sand.
  //

  new_async_io = SDL_AsyncIOFromFile( fpath_sand, "r" );
  file_size = SDL_GetAsyncIOSize( new_async_io );
  if ( file_size < 0 ) {
    fprintf( stderr, "[Carbonara] SDL_GetAsyncIOSize failed! reason:'%s'\n", SDL_GetError() );
    goto done_with_stone;
  }

  asset_buf2 = cara_malloc(
      &tmp_ws.malloc_helper, CACHELINE_LEN, len_mipmapped_2048 + dds_header_size );
  if ( !SDL_ReadAsyncIO(
        new_async_io,
        asset_buf2,
        0,
        UINT64_C(0) + file_size,
        my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed!\n" );
    goto done_with_sand;
  }
  while ( !SDL_CloseAsyncIO(new_async_io, false, my_io_queue, ui2ptr(555)) ) {
    hold_up_thread( 1000 * 100 );
  }

  loading_is_done = false;
  closing_is_done = false;
do_it_synchronoously_for_now2:
  while ( !SDL_GetAsyncIOResult( my_io_queue, &async_io_outcome ) ) {
    // One millisecond
    hold_up_thread( 1000 * 1000 );
  }

  switch ( ptr2ui(async_io_outcome.userdata) ) {
    case 33:
      fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
          async_io_outcome.bytes_requested,
          async_io_outcome.bytes_transferred,
          async_io_outcome.offset );
      if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
        fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
            async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
        if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
          fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
        }
        goto done_with_sand;
      }
      loading_is_done = true;
      if ( !closing_is_done ) {
        goto do_it_synchronoously_for_now2;
      }
      break;
    case 555:
      if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
        fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
            async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
      }
      closing_is_done = true;
      if ( loading_is_done ) {
        break;
      }
      [[fallthrough]];
    default:
      goto do_it_synchronoously_for_now2;
  }

  uint32_t * p_dds_head2 = (void*)asset_buf2;
  if ( *p_dds_head2 != magic_dw ) {
    fprintf( stderr, "[Carbonara-Debug] DDS header magic value is wrong! "
        "%#010x instead of %#010x\n", *p_dds_head2, magic_dw );
  } else {
    cara_vk.sand_texture = asset_buf2;
    assume( cara_vk.asset_size == file_size );
  }
done_with_sand:
  SPALL_END();
  // TODO Get the test code out of here.
  /*
  constexpr uint32_t magic_dw = (('D' << 8*0) | ('D' << 8*1) | ('S' << 8*2) | (' ' << 8*3));
  DDS_header * p_dds_head = (void*)cara_vk.stone_texture;
  if ( p_dds_head->dwMagic != magic_dw ) {
    fprintf( stderr, "[Carbonara-Debug] DDS header magic value is wrong! %#010x instead of %#010x\n",
        p_dds_head->dwMagic, magic_dw );
  } else {
    fprintf( stdout, "[Carbonara-Debug] dwSize = %u; dwFlags = %#010x; extent = %u*%u;\n",
        p_dds_head->dwSize, p_dds_head->dwFlags, p_dds_head->dwWidth, p_dds_head->dwHeight );
    if ( p_dds_head->dwFlags ) {
      fprintf( stdout, "[Carbonara-Debug] dwFlags = 0" );
      if ( p_dds_head->dwFlags & DDSD_DEPTH ) fprintf( stdout, " | DDSD_DEPTH" );
      if ( p_dds_head->dwFlags & DDSD_LINEARSIZE ) fprintf( stdout, " | DDSD_LINEARSIZE" );
      if ( p_dds_head->dwFlags & DDSD_MIPMAPCOUNT ) fprintf( stdout, " | DDSD_MIPMAPCOUNT" );
      if ( p_dds_head->dwFlags & DDSD_PIXELFORMAT ) fprintf( stdout, " | DDSD_PIXELFORMAT" );
      if ( p_dds_head->dwFlags & DDSD_PITCH ) fprintf( stdout, " | DDSD_PITCH" );
      if ( p_dds_head->dwFlags & DDSD_WIDTH ) fprintf( stdout, " | DDSD_WIDTH" );
      if ( p_dds_head->dwFlags & DDSD_HEIGHT ) fprintf( stdout, " | DDSD_HEIGHT" );
      if ( p_dds_head->dwFlags & DDSD_CAPS ) fprintf( stdout, " | DDSD_CAPS" );
      fprintf( stdout, "\n" );
    }
    if ( p_dds_head->dwFlags & DDSD_MIPMAPCOUNT ) {
      fprintf( stdout, "[Carbonara-Debug] p_dds_head->dwMipMapCount = %u\n", p_dds_head->dwMipMapCount );
    }
  }
  */

  new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/simple_test.hlsl.vert.spv", "r" );
  file_size = SDL_GetAsyncIOSize( new_async_io );
  asset_buf3 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        new_async_io,
        asset_buf3,
        0,
        UINT64_C(0) + file_size,
        my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed!\n" );
  }
  if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 33 );
  } else {  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(new_async_io, false, my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    } else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 555 );
    cara_vk.spv_simple_vert = asset_buf3;
    cara_vk.len_simple_vert = file_size;
  }

  new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/simple_test.hlsl.frag.spv", "r" );
  file_size = SDL_GetAsyncIOSize( new_async_io );
  asset_buf4 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        new_async_io,
        asset_buf4,
        0,
        UINT64_C(0) + file_size,
        my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed!\n" );
  }
  if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 33 );
  } else {  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(new_async_io, false, my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    } else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 555 );
    cara_vk.spv_simple_frag = asset_buf4;
    cara_vk.len_simple_frag = file_size;
  }

  new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/depth_only_prepass.hlsl.vert.spv", "r" );
  file_size = SDL_GetAsyncIOSize( new_async_io );
  asset_buf5 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        new_async_io,
        asset_buf5,
        0,
        UINT64_C(0) + file_size,
        my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed!\n" );
  }
  if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 33 );
  } else {  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(new_async_io, false, my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    } else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 555 );
    cara_vk.spv_depth_only = asset_buf5;
    cara_vk.len_depth_only = file_size;
  }

  new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/depth_only_prepass.hlsl.frag.spv", "r" );
  file_size = SDL_GetAsyncIOSize( new_async_io );
  asset_buf6 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        new_async_io,
        asset_buf6,
        0,
        UINT64_C(0) + file_size,
        my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed!\n" );
  }
  if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 33 );
  } else {  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(new_async_io, false, my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    } else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( ptr2ui(async_io_outcome.userdata) == 555 );
    cara_vk.spv_depth_only_frag = asset_buf6;
    cara_vk.len_depth_only_frag = file_size;
  }



  //
  // Spawn threads
  //
  void * string = ma_malloc(
      ma_head,
      _get_nr_units_to_cover(
        //len_new_stack + sizeof(thread) * cara_single.proc_map->nr_processors,
        sizeof(thread) * cara_single.proc_map->nr_processors,
        CACHELINE_LEN ),
      &tmp_ws.malloc_helper,
      cara_malloc );

  // In the mm_malloc call above we asked for space for the array of threads.
  atomic_init( &cara_single.nr_workers_resigned, 0 );
  threads = string;

  for ( uint_fast8_t proc_idx = 0;
      proc_idx < cara_single.proc_map->nr_processors;
      proc_idx++ ) {
    SPALL_BEGIN( "spawn_a_worker" );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    err = create_thread(
        .thread = &threads[proc_idx],
        .func = launch_worker,
        .arg = (void*)proc_idx,
        .priority = 2,
        );
#pragma GCC diagnostic pop
    SPALL_END();  // spawn a worker

    if ( err ) {
      switch ( err ) {
        case err_msg_out_of_resource:
          fprintf( stderr, "[Carbonara] Error in worker creation: %s\n", err_msg_text[err] );
          return 4;
        case err_msg_invalid_request:
          fprintf( stderr, "[Carbonara] Error in worker creation: %s\n", err_msg_text[err] );
          return 4;
        case err_msg_permission_denied:
          fprintf( stderr, "[Carbonara] Error in worker creation: %s\n", err_msg_text[err] );
          return 2;
        default:
          fprintf( stderr, "[Carbonara] Error in worker creation: Unknown error code %u\n", err );
          return 4;
      }
    }
  }

  //void * retval;
  //for ( uint_fast8_t proc_idx = 0; proc_idx < cara_single.proc_map->nr_processors; proc_idx++ ) {
  ////for ( uint_fast8_t proc_idx = 0; proc_idx < 1; proc_idx++ )
  //  uint_fast8_t il_err = join_thread(threads[proc_idx], &retval);
  //  if ( il_err ) {
  //    switch ( il_err ) {
  //      default:
  //        fprintf( stderr, "[Carbonara] ERROR: There was an error occured while joining thread #%u = %u\n", proc_idx, il_err );
  //    }
  //  } else {
#ifdef DEBUG_MODE
  //    fprintf( stdout, "[Worker] retval from thread #%u = %ld\n", proc_idx, (long int)retval );
#endif
  //  }
  //}


  // Release the temporary malloc_helper as user #0
  //ma_free(cara_single.mem_arena, 0, malloc_helper);

  return 0;
}


//-------------------------------------------------------------------------------------------------
void * launch_worker( void * arg )
{
  const uint_fast8_t proc_idx = ptr2ui(arg);
  struct ms_worker_info * worker = &cara_single.proc_map->processors[proc_idx];

  //struct cara_malloc_helper_dynamic_bound * malloc_helper_dynamic_bound;
  static thread_local struct cara_worker_state worker_state;
  struct cara_malloc_helper * malloc_helper;
  malloc_helper = &worker_state.malloc_helper;
  malloc_helper->worker_idx = worker->idx_self;
  malloc_helper->node_recently_used = cara_single.mem_arena->head;
  //malloc_helper->alignment = CACHELINE_LEN;
  //worker_state->mhelper_grower = malloc_helper_grower;

#ifdef ENABLE_PROFILING
  //constexpr size_t spall_buf_sz = 1 * 1024 * 1024;
  constexpr size_t spall_buf_sz = 512 * 1024;
  // From advanced_threads_example_linux.c:
  /*
     Fun fact: You don't actually *need* a buffer, you can just pass NULL!
     Passing a buffer clumps flushing overhead, so individual functions are faster and less noisy

     If you notice big variance in events, you can try bumping the buffer size so you do fewer
     flushes while your code runs, or you can shrink it if you need to save some memory
     */
  void * string = cara_malloc( malloc_helper, CACHELINE_LEN, spall_buf_sz );

  static thread_local SpallBuffer spall_buffer;
  spall_buffer = (SpallBuffer) {
    .pid = 0,
      .tid = proc_idx,
      //.tid = pthread_self(),
      .length = spall_buf_sz,
      .data = string,
      //.data = malloc(spall_buf_sz),
  };

  // From advanced_threads_example_linux.c:
  /*
     Here's another neat trick:
     We're touching the pages ahead of time here, so we get a smoother trace.
     By pre-faulting all the pages in our event buffer, we avoid waiting for pages to load
     while user code runs. This can make a noticable difference for data consistency, especially
     with bigger buffers.
     */
  memset(spall_buffer.data, 0, spall_buffer.length);

  if ( !spall_buffer_init(&spall_ctx, &spall_buffer) ) {
    fprintf( stderr, "[Worker] failed to init the spall buffer.\n" );
  }

  p_spall_buf = &spall_buffer;

  SPALL_BEGIN( "worker_loop-init" );
#endif  // ENABLE_PROFILING

  // The F stands for Foreman
  constexpr char thread_name_template[] = "Carb#00(F)";
  constexpr char format_string[] = "Carb#%02u(F)";
  char thread_name[sizeof(thread_name_template)];

  char format_tester[sizeof(thread_name_template)];
  sprintf( format_tester, format_string, 0u );
  assume ( strcmp(format_tester, thread_name_template) == 0 );

  size_t loc_left_paren = strchr(thread_name_template, '(') - thread_name_template;
  sprintf( thread_name, format_string, proc_idx );
  thread_name[loc_left_paren] = worker->deployment_flags & worker_deployment_flag_foreman ? '(' : 0;
  if ( prctl(PR_SET_NAME, thread_name) < 0 ) {
    fprintf( stderr, "[Worker] (%u) failed to set thread name.\n", proc_idx );
  }

#ifdef DEBUG_MODE
    //worker->user_data = (void*) wtime_usec;
    fprintf( stdout, "[Worker] (%u) My idx_self = proc_idx = %u\n", proc_idx, worker->idx_self );
    fprintf( stdout, "[Worker] (%u)   thread name = %s \n", proc_idx, thread_name );
    //fprintf( stdout, "[Worker] (%u)   cache_location = %u\n", proc_idx, worker->cache_location );
  //fprintf( stdout, "[Worker] (%u)   cache_pattern = %#010x\n", proc_idx, worker->cache_pattern );
    fprintf( stdout, "[Worker] (%u)   deployment_flags = %#04hhx\n",
        proc_idx, worker->deployment_flags );
    fprintf( stdout, "[Worker] (%u)   deployment_flags = %u\n", proc_idx, worker->idx_core );
    //fprintf( stdout, "[Worker] (%u)   wtime_starter = %lu\n", proc_idx, wtime_usec );
#endif
  int err = hwloc_set_cpubind( cara_single.proc_map->topology, worker->cpuset,
      HWLOC_CPUBIND_THREAD | HWLOC_CPUBIND_STRICT );
  if ( err ) {
    switch ( errno ) {
      case ENOSYS:
        // The action is not supported
        fprintf( stdout, "[Worker] (%u)   bind_cpu: The action is not supported.\n", proc_idx );
        break;
      case EXDEV:
        // The binding cannot be enforced
        fprintf( stdout,
            "[Worker] (%u)   bind_cpu: The binding cannot be enforced.\n", proc_idx );
        break;
      default:
        fprintf( stdout, "[Worker] (%u)   bind_cpu: An unknown error occurred. (errno=%d)\n",
            proc_idx, errno );
        break;
    }
  }
  assume( !err );

  worker_state.current_batch = 0;
  worker_state.worker_info = worker;

  // The loop.
  worker_loop( &worker_state );

  return 0;
}


//-------------------------------------------------------------------------------------------------
__attribute__((noreturn))
static void worker_loop( struct cara_worker_state * worker_state )
{
  struct ms_worker_info * const worker =
    &cara_single.proc_map->processors[worker_state->worker_info->idx_self];
  struct cara_malloc_helper * const alloc_helper = &worker_state->malloc_helper;

  // pc_std is not read by anyone—It will probably be removed by the optimizer.
  // It can be used for leap-second adding process if needed.
  [[maybe_unused]] uint64_t std_pc = 0;
  uint64_t pc_accumultor = 0;
  uint64_t prev_pc;  // An accumulator and the previous timestamp

  if ( worker->deployment_flags & worker_deployment_flag_foreman ) {
    // Prepare a stack of functions to call at the end
    // FIXME Just 4 functions?
    constexpr uint_fast8_t stack_len =
      sizeof(struct func_call_stack_element) * 4 + sizeof(struct infos_stack);
    struct infos_stack * const quit_stack = infos_stack_create(
        cara_malloc( alloc_helper, alignof(max_align_t), stack_len ),
        stack_len );
    cara_single.foremans_quit_stack = quit_stack;

    prev_pc = get_performance_counter();
  }


  SPALL_END();  // The init. part of worker_loop


  uint64_t cur_time_in_5us_general = 0;
  while(-7) {
    const uint64_t starting_pc = get_performance_counter();

    SPALL_BEGIN( "worker_loop" );

    if ( worker->deployment_flags & worker_deployment_flag_foreman ) {
      const uint64_t new_pc = get_performance_counter();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
      std_pc += new_pc - prev_pc;
      pc_accumultor += new_pc - prev_pc;
      prev_pc = new_pc;
      if ( pc_accumultor >= cara_single.nr_pc_in_5us ) {
#pragma GCC diagnostic pop
        const uint64_t nr_5us = div_via_mul_64(&cara_single.magic_divider, pc_accumultor);
        pc_accumultor -= cara_single.nr_pc_in_5us * nr_5us;
        cur_time_in_5us_general += nr_5us;

        // XXX Do I really need to share this with other processing units?
        atomic_store_explicit( &cara_single.cur_time_in_5us_general, cur_time_in_5us_general, memory_order_release );
        //atomic_fetch_add_explicit( &cara_single.cur_time_in_5us_general, 1, memory_order_release );
      }

      // See if we should spawn a periodic job or two
      const uint_fast8_t height = cara_single.tree_of_times_to_act->max_active_depth < 1 ?
        1 : cara_single.tree_of_times_to_act->max_active_depth;
      uint32_t cur_idx = infos_bst_locate_root( height );
      uint_fast8_t cur_height = height;

      uint64_t * restrict const keys =
        __builtin_assume_aligned( cara_single.tree_of_times_to_act->keys, ALIGN_FOR_AVX );

      while(-7) {
        // It is due?
        if ( keys[cur_idx] <= cur_time_in_5us_general ) {
          // Iterate from cur_idx through the leftmost(0).
          for ( typeof(cur_idx) node_idx = 0; node_idx <= cur_idx; node_idx++ ) {
            if ( keys[node_idx] == 0 ) {
              continue;
            }

            // The index into the array of job_func index
            auto pjob_idx = infos_bst_fetch_sepd_auto(
                cara_single.tree_of_times_to_act, node_idx );

            // Write an updated due time.
            keys[cur_idx] = 1 |
              (cur_time_in_5us_general + cara_single.periodic_jobs_periods_5us[pjob_idx]);
            // Spawn a job.
            struct cara_job_batch_desc_64 * batch =
              cara_get_unoccupied_job_batch_desc();
            while ( !batch ) {
              batch = cara_get_unoccupied_job_batch_desc();
            }
            for ( uint_fast8_t job_idx = 1; job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
                job_idx++ ) {
              batch->job_func_idx[job_idx] = 0;
              batch->job_flags[job_idx] = 0;
            }
            batch->job_func_idx[0] = cara_single.periodic_jobs[pjob_idx];

            batch->job_flags[0] = 0;
            // memset(batch->job_func_args, 0, sizeof(batch->job_func_args));
            atomic_store_explicit(&batch->progress, 0, memory_order_release);
            atomic_store_explicit(&batch->preferred_core, ~UINT64_C(0), memory_order_release);
          }

        }
        //min_idx_untouched = cur_idx + 1;

        if ( cur_height == 0 )
          break;

        cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
        cur_height--;
      }

      infos_insert_sort_bst(cara_single.tree_of_times_to_act);

    } else {
      // This worker is not a foreman.

      cur_time_in_5us_general = atomic_load_explicit(
          &cara_single.cur_time_in_5us_general, memory_order_consume );
    }


    // Look for a job to get done.
    worker_state->current_batch = cara_get_job(worker);

    if ( worker_state->current_batch ) {
      uint_fast8_t cur_job_func_arg = 0;
      /*
      struct cara_job_func_args_64 * job_func_args_group = nullptr;
      if ( worker_state->current_batch->job_idx_func_arg_group[0] != UINT16_MAX ) {
        job_func_args_group = infos_al_get_elem(cara_single.job_arg_pool,
            worker_state->current_batch->job_idx_func_arg_group[0]);
      }
      */

      //
      uint8_t * restrict const job_func_args = worker_state->current_batch->job_func_args;

/*#ifdef DEBUG_MODE  //?!
      fprintf( stdout, "[Worker] (%u) I got a job batch descriptor. %lu\n",
          proc_idx,
          (ptr2ui(worker_state->current_batch) - ptr2ui(cara_single.job_list->list->head->data))
          >> mm_singleton.len_cacheline_exp );
#endif*/
      for ( uint_fast8_t job_idx = 0; job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
          job_idx++ ) {
        if ( worker_state->current_batch->job_func_idx[job_idx] ) {
#ifdef DEBUG_MODE  //?!
          fprintf( stdout, "[Worker] (%u) I'm on it! job_func_idx=%u\n",
              worker_state->worker_info->idx_self,
              worker_state->current_batch->job_func_idx[job_idx] );
#endif

          uint_fast8_t len_first_arg;
          uint_fast8_t total_len_args = 0;
          switch ( job_fsig_db[worker_state->current_batch->job_func_idx[job_idx]] ) {
            case job_fsig_i64:
            case job_fsig_ui64:
            case job_fsig_double:
              len_first_arg = 8;
              break;
            case job_fsig_i32:
            case job_fsig_ui32:
            case job_fsig_float:
              len_first_arg = 4;
              break;
            case job_fsig_i16:
            case job_fsig_ui16:
              len_first_arg = 2;
              break;
            case job_fsig_i8:
            case job_fsig_ui8:
              len_first_arg = 1;
              break;
            case job_fsig_ptr:
              len_first_arg = sizeof(void*);
              break;
            case job_fsig_void:
              len_first_arg = 0;
              break;
          }
          if ( len_first_arg ) {
            // The first argument of each function is the biggest.
            cur_job_func_arg = _align_to_power_of_two(cur_job_func_arg, len_first_arg);
            switch ( job_fsig_db[worker_state->current_batch->job_func_idx[job_idx]] ) {
              case job_fsig_i64:
              case job_fsig_ui64:
              case job_fsig_double:
                total_len_args = 8;
                break;
              case job_fsig_i32:
              case job_fsig_ui32:
              case job_fsig_float:
                total_len_args = 4;
                break;
              case job_fsig_i16:
              case job_fsig_ui16:
                total_len_args = 2;
                break;
              case job_fsig_i8:
              case job_fsig_ui8:
                total_len_args = 1;
                break;
              case job_fsig_ptr:
                total_len_args = sizeof(void*);
                break;
              case job_fsig_void:
                total_len_args = 0;
                break;
            }
            /*
            if ( cur_job_func_arg + total_len_args > sizeof(job_func_args_group->data) ) {
              atomic_store_explicit(&job_func_args_group->occupied, 0, memory_order_release);
              atomic_fetch_sub_explicit(&cara_single.nr_job_arg_groups_taken, 1,
                  memory_order_relaxed);

              job_func_args_group = infos_al_get_elem(cara_single.job_arg_pool,
                  worker_state->current_batch->idx_job_func_arg_group[1]);
              cur_job_func_arg = 0;
            }
            */
            // XXX Just keep it slim, no second function argument group!
          }

          switch ( job_fsig_db[worker_state->current_batch->job_func_idx[job_idx]] ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
            case job_fsig_void:
              ( (void (*)(struct cara_worker_state *))
                known_jobs[worker_state->current_batch->job_func_idx[job_idx]] )
                ( worker_state );
              break;
            case job_fsig_ui8:
              ( (void (*)(struct cara_worker_state *, uint8_t))
                known_jobs[worker_state->current_batch->job_func_idx[job_idx]] )
                ( worker_state, job_func_args[cur_job_func_arg] );
              break;
            default:
#if defined(ARE_WE_SHIPPING_IT)
              unreachable();
#else
              fprintf( stderr, "an unknown job functions signature (%u)\n",
                  worker_state->current_batch->job_func_idx[job_idx] );
              break;
#endif
#pragma GCC diagnostic pop
          }
          cur_job_func_arg += total_len_args;
#ifdef DEBUG_MODE  //?!
          fprintf( stdout, "[Worker] (%u) done. job_func_idx=%u\n",
              worker_state->worker_info->idx_self,
              worker_state->current_batch->job_func_idx[job_idx] );
#endif

          //typeof(worker_state->current_batch->progress) cur_progress = worker_state->current_batch->progress +
          //  UINT8_MAX/CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
          //if ( cur_progress <= 1 ) {
          //  cur_progress = UINT8_MAX;
          //}
          //atomic_store_explicit(&worker_state->current_batch->progress, cur_progress, memory_order_release);

        }
      }

      // Mark empty and unoccupied in the job batch descriptor
      atomic_store_explicit( &worker_state->current_batch->progress,
          UINT8_MAX, memory_order_release );
      atomic_fetch_sub_explicit( &cara_single.nr_job_batch_descriptors_taken,
          1, memory_order_release );

      /*
      if ( job_func_args_group ) {
        atomic_store_explicit(&job_func_args_group->occupied, 0, memory_order_release);
        atomic_fetch_sub_explicit(&cara_single.nr_job_arg_groups_taken, 1,
            memory_order_release);
      }
      */
    } else {

      SPALL_BEGIN( "hold_up_thread" );

      if ( worker->deployment_flags & worker_deployment_flag_yielder ) {
        // Take a break for 1280 usec.
        //hold_up_thread( 1000*1280 );
        delay_precise( starting_pc + cara_single.nr_pc_in_5us * 256 );
        // TODO Use MONITORX/UMONITOR instead when the instruction is supported
        // C0.2 improves performance of the other SMT thread(s) on the same core.
        //  MWAITX's C1 state might offer a similar benefit too.
      } else {
        // Take a break for about 640 usec.
        //hold_up_thread( 1000*640 );
        delay_precise( starting_pc + cara_single.nr_pc_in_5us * 128 );
        // TODO Use MONITORX/UMONITOR instead when the instruction is supported
        // C0.2 improves performance of the other SMT thread(s) on the same core.
        //  MWAITX's C1 state might offer a similar benefit too.
      }

      SPALL_END();  // hold_up_thread
    }

    SPALL_END();  // worker_loop
  }

}


//-------------------------------------------------------------------------------------------------
__attribute__((noreturn))
void jf_start_kill_chain( struct cara_worker_state * ws )
{
  // Undo the job-take immediately.
  atomic_fetch_sub_explicit(&cara_single.nr_job_batch_descriptors_taken, 1,
      memory_order_release);
  atomic_store_explicit(&ws->current_batch->progress, 0, memory_order_release);

#ifdef DEBUG_MODE
  fprintf( stdout, "[Worker] (%u) Following a kill-chain...\n", ws->worker_info->idx_self );
#endif

  // Every worker quits quickly except the foreman.
  if ( !(ws->worker_info->deployment_flags & worker_deployment_flag_foreman) ) {

#ifdef ENABLE_PROFILING
    // Remember to quit or flush your buffers before you exit!
    // If you don't, you might not get the last few events waiting to be written
    spall_buffer_quit(&spall_ctx, p_spall_buf);

    // Freeing our buffer's memory now that spall_buffer_quit has flushed it for us.
    ma_free( cara_single.mem_arena, ws->worker_info->idx_self, p_spall_buf->data );
#endif

    atomic_fetch_add_explicit( &cara_single.nr_workers_resigned,
        1, memory_order_release );

#ifdef DEBUG_MODE
    fprintf( stdout, "[Worker] (%u) resigned.\n", ws->worker_info->idx_self );
#endif

    pthread_exit(0);
  }
//-------------------------------------------------------------------------------------------------
  // Only the foreman worker survives past this line.

  SPALL_BEGIN("foreman's_last_job");

  SPALL_BEGIN("wait_for_fellow_workers");
  // Foreman waits for other workers to exit first.
  while ( atomic_load_explicit(&cara_single.nr_workers_resigned, memory_order_acquire) <
      cara_single.proc_map->nr_processors - 1 ) {
    // Take a break for about 320 usec.
    const uint64_t starting_pc = get_performance_counter();
    delay_precise( starting_pc + cara_single.nr_pc_in_5us * 64 );
  }
  SPALL_END();  // wait_for_fellow_workers

  SPALL_BEGIN("reap_what_we_have_sown");
  struct func_call_stack_element * p_func_call_stack_element;
  while (( p_func_call_stack_element = infos_stack_pop(
          cara_single.foremans_quit_stack,
          sizeof(struct func_call_stack_element)
          ) )) {
    if ( p_func_call_stack_element->arg ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
      ( (void(*)(void*)) p_func_call_stack_element->func ) ( p_func_call_stack_element->arg );
#pragma GCC diagnostic pop
    } else {
      //p_func_call_stack_element->func();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types-strict"
#pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types"
      void (*void_func) () = p_func_call_stack_element->func;
#pragma GCC diagnostic pop
      void_func();
    }
  }

  SPALL_END();  // reap_what_we_have_sown
  SPALL_END();  // foreman's_last_job

#ifdef ENABLE_PROFILING
  // Remember to quit or flush your buffers before you exit!
  // If you don't, you might not get the last few events waiting to be written
  spall_buffer_quit(&spall_ctx, p_spall_buf);

  // Freeing our buffer's memory now that spall_buffer_quit has flushed it for us.
  ma_free( cara_single.mem_arena, ws->worker_info->idx_self, p_spall_buf->data );
#endif

  // Assign a number bigger than 128 to sign_to_end
  atomic_fetch_or_explicit( &cara_single.sign_to_end, ~UINT64_C(0), memory_order_release );

  pthread_exit(0);
}


//-------------------------------------------------------------------------------------------------
static enum err_msg verbosee_integrity_check_bc7_2048( const char * data )
{
  TCTex_Texture tc_texture;
  if ( !tctex_load_mem(&tc_texture, data, cara_vk.asset_size) ) {
    fprintf( stderr, "[Carbonara-Debug] tctex_load_mem failed on the asset! (%s)\n",
        tc_texture.errmsg );
    return err_msg_corrupted_data_detected;
  } else {
    const TCTex_DDS_HEADER* header = (void*)data+4;
    fprintf( stdout, "[Carbonara-Debug] ""dimension=%u size=%u*%u nmiplevels=%u "
        "offset0=%u header.dwPitchOrLinearSize=%u\n",
        tc_texture.dimension,
        tc_texture.size.x,
        tc_texture.size.y,
        tc_texture.nmiplevels,
        tc_texture.offset0,
        header->dwPitchOrLinearSize
        );
    if ( tc_texture.iformat != TCTEX_IFORMAT_COMPRESSED_BC7_UNORM ) {
      fprintf( stderr, "[Carbonara-Debug] The format was not BC7_UNORM! (%u)\n",
          tc_texture.iformat );
      return err_msg_ext_not_supported;
    }
  }
  constexpr unsigned int max_nr_miplevels = 24;  // 1 - 2048
  TCTex_MipMapInfo arr_mmi[max_nr_miplevels];
  uint32_t nr_textures = tctex_get_mipmaps( &tc_texture, arr_mmi, max_nr_miplevels, 0 );
  fprintf( stdout, "[Carbonara-Debug] nr_textures = %u\n", nr_textures );

  return 0;
}

//-------------------------------------------------------------------------------------------------
__attribute__ ((target ("lzcnt")))
void jf_kickstart( struct cara_worker_state * ws )
{
  VkDeviceMemory * malloc = nullptr;
  struct func_call_stack_element func_call_stack_element;

  atomic_init( &cara_single.is_frame_id_even, 0 );
  uint_fast8_t is_frame_id_even = 0;



  // TODO move the code below away!
  fprintf( stdout, "[Carbonara-Debug] tctex_load_mem for stone...\n" );
  if ( verbosee_integrity_check_bc7_2048(cara_vk.stone_texture) ) {
    fprintf( stderr, "[Carbonara-Debug] integrity check failed for stone.\n" );
    goto stop_everything_after_failure;
  }
  fprintf( stdout, "[Carbonara-Debug] tctex_load_mem for sand...\n" );
  if ( verbosee_integrity_check_bc7_2048(cara_vk.sand_texture) ) {
    fprintf( stderr, "[Carbonara-Debug] integrity check failed for sand.\n" );
    goto stop_everything_after_failure;
  }

  SPALL_BEGIN("check_out_vulkan");
  if ( ma_create_staging_buf(&cara_vk.vmm, cara_vk.log_dev, 0) ) {
    fprintf( stderr, "[Carbonara-Debug] ma_create_vk_trans_bufs failed..\n" );
    goto stop_everything_after_failure;
  }

  enum err_msg err = ma_prepare_upload_mgr( &cara_vk.vmm, &ws->malloc_helper, cara_malloc );
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] ma_prepare_upload_mgr failed. (%u)\n", err );
    goto stop_everything_after_failure;
  }
  constexpr unsigned int dds_header_size =
    4 + sizeof(TCTex_DDS_HEADER) + sizeof(TCTex_DDS_HEADER_DXT10);
  constexpr unsigned int nr_miplevels = 12;
  constexpr unsigned int len_mipmapped_2048 = 16 + 16 + (4*4)*4/4 + (8*8)*4/4 + (16*16)*4/4 +
    (32*32)*4/4 + (64*64)*4/4 + (128*128)*4/4 + (256*256)*4/4 + (512*512)*4/4 + (1024*1024)*4/4 + (2048*2048)*4/4;
  unsigned int nr_bytes_to_copy =
    _align_to_power_of_two( len_mipmapped_2048, cara_vk.vmm.buf_granularity );
  const struct range_u32 * rent_for_stone = infos_pm_rent(
      &cara_vk.vmm.uploads, &ws->malloc_helper, cara_malloc, nr_bytes_to_copy, true );
  const struct range_u32 * rent_for_sand = infos_pm_rent(
      &cara_vk.vmm.uploads, &ws->malloc_helper, cara_malloc, nr_bytes_to_copy, true );
  if ( !rent_for_stone || !rent_for_sand ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_rent failed.. (errno=%d)\n", errno );
    //infos_pm_destroy( &cara_vk.vmm.uploads, &ws->worker_info->idx_self, cara_free );
    goto stop_everything_after_failure;
  }

  SPALL_BEGIN("memcpy_2_textures");
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit specifies that the host cache management commands
  // vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges are not needed to manage
  // availability and visibility on the host.
  memcpy( cara_vk.vmm.p_staging_buf + rent_for_stone->start,
      cara_vk.stone_texture + dds_header_size, nr_bytes_to_copy );
  memcpy( cara_vk.vmm.p_staging_buf + rent_for_sand->start,
      cara_vk.sand_texture + dds_header_size, nr_bytes_to_copy );
  SPALL_END();  // memcpy_2_textures

  VkImageCreateInfo img_create_info;
  img_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  img_create_info.pNext = nullptr;
  img_create_info.flags = 0;
  img_create_info.imageType = VK_IMAGE_TYPE_2D;
  img_create_info.format = VK_FORMAT_BC7_UNORM_BLOCK;
  img_create_info.extent = (VkExtent3D) { 2048, 2048, 1 };
  img_create_info.mipLevels = nr_miplevels;
  img_create_info.arrayLayers = 8;
  img_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  img_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  img_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    VK_IMAGE_USAGE_SAMPLED_BIT |
    0;
  img_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  img_create_info.queueFamilyIndexCount = 0;
  img_create_info.pQueueFamilyIndices = nullptr;
  img_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  SPALL_BEGIN("create_img");
  VkResult result = vkCreateImage( cara_vk.log_dev,
      &img_create_info, &cara_vk.vk_alloc_callbacks, &cara_vk.vki_8_2048_tex
      //&img_create_info, nullptr, &cara_vk.vki_8_2048_tex
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkCreateImage an 8-pack failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  // Obtain the requirements
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements( cara_vk.log_dev, cara_vk.vki_8_2048_tex, &req );

  // Choose the best memory type
  VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
  preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  required_props = 0;
  tolerable_props = 0;
  const uint32_t type_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
  // Save the best type for later?
  cara_vk.vmm.arr_best_mtype_idx[vk_buf_type_image_arr_8_2048] = type_idx;

  // Allocate memory
  VkMemoryAllocateInfo buf_malloc_info;
  buf_malloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  buf_malloc_info.pNext = nullptr;
  buf_malloc_info.allocationSize = req.size;
  buf_malloc_info.memoryTypeIndex = type_idx;

  malloc = infos_multi_al_get_elem( &cara_vk.vmm.mallocs, type_idx, 0 );
  SPALL_BEGIN("alloc_mem");
  result = vkAllocateMemory( cara_vk.log_dev,
      &buf_malloc_info, &cara_vk.vk_alloc_callbacks, malloc
      //&buf_malloc_info, nullptr, malloc
      );
  if( result != VK_SUCCESS ) {
    goto stop_everything_after_failure;
  }
  SPALL_END();
  cara_vk.vmm.arr_usages[type_idx] += req.size;

  // Bind the image to the first allocation of type_idx type
  SPALL_BEGIN("bind_img_mem");
  result = vkBindImageMemory( cara_vk.log_dev, cara_vk.vki_8_2048_tex, *malloc, 0 );
  if( result != VK_SUCCESS ) {
    goto stop_everything_after_failure;
  }
  SPALL_END();

  VkCommandBufferBeginInfo cmd_buf_begin_info;
  cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_buf_begin_info.pNext = nullptr;
  cmd_buf_begin_info.flags =
    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |  // Each recording of the command buffer will
                                                   // only be submitted once, and the command
                                                   // buffer will be reset and recorded again
                                                   // between each submission.
    0;
  cmd_buf_begin_info.pInheritanceInfo = nullptr;
             // a pointer to a VkCommandBufferInheritanceInfo structure, used if `commandBuffer`
             // is a secondary command buffer. If this is a primary command buffer, then this
             // value is ignored.


  // Begin recording on the primary buffer
  VkCommandBuffer trans_buf = cara_vk.trans_buf.primary_cmd_buf;
#ifdef DEBUG_MODE
  fprintf( stderr, "[Vulkan-Debug] TP cmd_buf=%#018lx\n", ptr2ui(trans_buf) );
#endif
  SPALL_BEGIN("begin_trans_buf");
  // TODO synchronization needed
  result = vkBeginCommandBuffer( trans_buf, &cmd_buf_begin_info );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command(transfer) buffer recording operation. "
        "'%s'\n", string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  VkImageSubresourceRange img_subres_range;
  img_subres_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  img_subres_range.baseMipLevel = 0;    // 2048(0)
  img_subres_range.levelCount = nr_miplevels;
  img_subres_range.baseArrayLayer = 0;  // Stone-
  img_subres_range.layerCount = 2;      // Stone and sand
  

  //
  // Make barriers between zero and transfer
  //

  //VkBufferMemoryBarrier2KHR buf_bar[2];
  //const struct range_u32 * arr_p_rents[2] = { rent_for_stone, rent_for_sand };
  //for ( unsigned int layer_idx = 0; layer_idx < 2; layer_idx++ ) {
  //  buf_bar[layer_idx].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
  //  buf_bar[layer_idx].pNext = nullptr;
  //  buf_bar[layer_idx].srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  //  buf_bar[layer_idx].dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  //  buf_bar[layer_idx].srcAccessMask = VK_ACCESS_2_NONE_KHR;
  //  buf_bar[layer_idx].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
  //  buf_bar[layer_idx].srcQueueFamilyIndex = cara_vk.trans_q_fam_idx;
  //  buf_bar[layer_idx].dstQueueFamilyIndex = cara_vk.trans_q_fam_idx;
  //  buf_bar[layer_idx].buffer = cara_vk.vmm.staging_buf;
  //  buf_bar[layer_idx].offset = arr_p_rents[layer_idx]->start;
  //  buf_bar[layer_idx].size = nr_bytes_to_copy;
  //}

  VkImageMemoryBarrier2KHR img_bar;
  img_bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
  img_bar.pNext = nullptr;
  img_bar.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  img_bar.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  img_bar.srcAccessMask = VK_ACCESS_2_NONE_KHR;
  img_bar.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
  img_bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  img_bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  //img_bar.srcQueueFamilyIndex = cara_vk.trans_q_fam_idx;
  //img_bar.dstQueueFamilyIndex = cara_vk.trans_q_fam_idx;
  img_bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  img_bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  img_bar.image = cara_vk.vki_8_2048_tex;
  img_bar.subresourceRange = img_subres_range;

  VkDependencyInfoKHR dependency_info;
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
  dependency_info.pNext = nullptr;
  dependency_info.dependencyFlags = 0;
  dependency_info.memoryBarrierCount = 0;
  dependency_info.pMemoryBarriers = nullptr;
  //dependency_info.bufferMemoryBarrierCount = _arr_elem_cnt( buf_bar );
  //dependency_info.pBufferMemoryBarriers = buf_bar;
  dependency_info.bufferMemoryBarrierCount = 0;
  dependency_info.pBufferMemoryBarriers = nullptr;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &img_bar;

  SPALL_BEGIN("barrier_0");
  vkCmdPipelineBarrier2KHR( trans_buf, &dependency_info );
  SPALL_END();

  VkBufferImageCopy arr_regions[nr_miplevels * 2];
  for ( unsigned int mip_lv = 0, width = 2048, offset = 0;
      mip_lv < nr_miplevels; mip_lv++, width/=2 ) {
    arr_regions[mip_lv].bufferOffset = rent_for_stone->start + offset;
    arr_regions[mip_lv].bufferRowLength = 0;
    arr_regions[mip_lv].bufferImageHeight = 0;
    arr_regions[mip_lv].imageSubresource = (VkImageSubresourceLayers) {
      VK_IMAGE_ASPECT_COLOR_BIT, mip_lv, 0, 1};
    arr_regions[mip_lv].imageOffset = (VkOffset3D) {0, 0, 0};
    arr_regions[mip_lv].imageExtent = (VkExtent3D) {width, width, 1};
    arr_regions[mip_lv+nr_miplevels].bufferOffset = rent_for_sand->start + offset;
    arr_regions[mip_lv+nr_miplevels].bufferRowLength = 0;
    arr_regions[mip_lv+nr_miplevels].bufferImageHeight = 0;
    arr_regions[mip_lv+nr_miplevels].imageSubresource = (VkImageSubresourceLayers) {
      VK_IMAGE_ASPECT_COLOR_BIT, mip_lv, 1, 1};
    arr_regions[mip_lv+nr_miplevels].imageOffset = (VkOffset3D) {0, 0, 0};
    arr_regions[mip_lv+nr_miplevels].imageExtent = (VkExtent3D) {width, width, 1};
    switch ( width ) {
      case 4:
        [[fallthrough]];  // 4*4 = 16 which happens to be the texel block size of BC7
      case 1:
        [[fallthrough]];
      case 2:
        offset += 16;     // The minimum size of the map is 16 bytes probably because of the
        break;            //  texel block size of BC7
      default:
        offset += ( width * width ) * 4 / 4;  // 1 byte stores the information of 1 pixel in BC7
    }
  }

  SPALL_BEGIN("cp_buf2img");
  vkCmdCopyBufferToImage(
      trans_buf,
      cara_vk.vmm.staging_buf,
      cara_vk.vki_8_2048_tex,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      _arr_elem_cnt( arr_regions ),
      arr_regions
      );
  SPALL_END();


  //
  // Make barriers between transfer and sampling
  //

  // No need to prepare a VkMemoryBarrier2KHR object?
  //VkMemoryBarrier2KHR mem_bar;
  //mem_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;
  //mem_bar.pNext = nullptr;
  //mem_bar.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  //mem_bar.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  //mem_bar.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR |
  //  VK_ACCESS_2_TRANSFER_READ_BIT_KHR |
  //  0;
  //mem_bar.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
  //  0;

  //img_bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
  //img_bar.pNext = nullptr;
  img_bar.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  img_bar.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  img_bar.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
  img_bar.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR;
  img_bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  img_bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  //img_bar.srcQueueFamilyIndex = cara_vk.trans_q_fam_idx;
  //img_bar.dstQueueFamilyIndex = cara_vk.graphics_q_fam_idx;
  img_bar.srcQueueFamilyIndex = cara_vk.trans_q_fam_idx;
  img_bar.dstQueueFamilyIndex = cara_vk.graphics_q_fam_idx;
  img_bar.image = cara_vk.vki_8_2048_tex;
  img_bar.subresourceRange = img_subres_range;

  dependency_info.dependencyFlags =
    VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR;
  dependency_info.memoryBarrierCount = 0;
  dependency_info.pMemoryBarriers = nullptr;
  dependency_info.bufferMemoryBarrierCount = 0;
  dependency_info.pBufferMemoryBarriers = nullptr;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &img_bar;

  SPALL_BEGIN("barrier_1");
  vkCmdPipelineBarrier2KHR( trans_buf, &dependency_info );
  SPALL_END();


  //
  // Prepare to submit the command buffer
  //
  VkSemaphoreSubmitInfoKHR semaphore_sgnl_info;
  semaphore_sgnl_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
  semaphore_sgnl_info.pNext = nullptr;
  semaphore_sgnl_info.semaphore = cara_vk.arr_semaphore_before_A[is_frame_id_even];
  semaphore_sgnl_info.value = csb_data_transfer_done;
  semaphore_sgnl_info.stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  semaphore_sgnl_info.deviceIndex = 0;

  VkCommandBufferSubmitInfoKHR cmd_buf_submit_info;
  cmd_buf_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
  cmd_buf_submit_info.pNext = nullptr;
  cmd_buf_submit_info.commandBuffer = trans_buf;
  cmd_buf_submit_info.deviceMask = 0;

  VkSubmitInfo2 submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
  submit_info.pNext = nullptr;
  submit_info.flags = 0;
  submit_info.waitSemaphoreInfoCount = 0;
  submit_info.pWaitSemaphoreInfos = nullptr;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &cmd_buf_submit_info;
  submit_info.signalSemaphoreInfoCount = 1;
  submit_info.pSignalSemaphoreInfos = &semaphore_sgnl_info;

  // End recording
  SPALL_BEGIN("end_trans_buf");
  result = vkEndCommandBuffer( trans_buf );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(1t) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  // Submit
  SPALL_BEGIN("submit_trans");
  result = vkQueueSubmit2KHR( cara_vk.transfer_q, 1, &submit_info, VK_NULL_HANDLE );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();


  // Begin recording on the primary command buffer for graphics
  VkCommandBuffer cmd_buf = cara_vk.arr_frame_infos[is_frame_id_even].primary_cmd_buf;
#ifdef DEBUG_MODE
  fprintf( stderr, "[Vulkan-Debug] GP cmd_buf=%#018lx\n", ptr2ui(cmd_buf) );
#endif
  SPALL_BEGIN("begin_cmd_buf");
  // TODO synchronization needed
  result = vkBeginCommandBuffer( cmd_buf, &cmd_buf_begin_info );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command(graphics) buffer recording operation. "
        "'%s'\n", string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  //
  // Make barriers between transfer and sampling
  //
  img_bar.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  img_bar.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  SPALL_BEGIN("barrier_2");
  vkCmdPipelineBarrier2KHR( cmd_buf, &dependency_info );
  SPALL_END();


  // Create a VkImageView
  VkImageViewCreateInfo iv_create_info;
  iv_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  iv_create_info.pNext = nullptr;
  iv_create_info.flags = 0;
  iv_create_info.image = cara_vk.vki_8_2048_tex;
  iv_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  iv_create_info.format = VK_FORMAT_BC7_UNORM_BLOCK;
  iv_create_info.components = (VkComponentMapping) {
    VK_COMPONENT_SWIZZLE_IDENTITY,
    VK_COMPONENT_SWIZZLE_IDENTITY,
    VK_COMPONENT_SWIZZLE_IDENTITY,
    VK_COMPONENT_SWIZZLE_IDENTITY  // What about VK_COMPONENT_SWIZZLE_ONE
  };
  iv_create_info.subresourceRange = img_subres_range;

  SPALL_BEGIN("create_iv");
  result = vkCreateImageView( cara_vk.log_dev, &iv_create_info,
      &cara_vk.vk_alloc_callbacks, &cara_vk.iv_8_2048_tex
      //nullptr, &cara_vk.iv_8_2048_tex
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkCreateImageView failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  //
  // Prepare a descriptor set layout
  //

  // Prepare a sampler
  //  ```glsl
  //  layout (set=m, binding=n) uniform sampler <variable name>;
  //  ```
  VkSamplerCreateInfo sampler_create_info;
  sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create_info.pNext = nullptr;
  sampler_create_info.flags = 0;
  sampler_create_info.magFilter = VK_FILTER_LINEAR;
  sampler_create_info.minFilter = VK_FILTER_LINEAR;
  sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  sampler_create_info.mipLodBias = .0f;
  sampler_create_info.anisotropyEnable = VK_TRUE;
  sampler_create_info.maxAnisotropy = cara_vk.max_anisotropy;
  sampler_create_info.compareEnable = VK_FALSE;
  //sampler_create_info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  sampler_create_info.minLod = .0f;
  sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
  sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;  // To prevent segfault
  sampler_create_info.unnormalizedCoordinates = VK_FALSE;
  SPALL_BEGIN("create_sampler");
  result = vkCreateSampler( cara_vk.log_dev, &sampler_create_info,
      &cara_vk.vk_alloc_callbacks, &cara_vk.sampler_8_2048_tex
      //nullptr, &cara_vk.sampler_8_2048_tex
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during sampler creation. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  //
  // Prepare a sampled image (We have already created a VkImageView)
  //  ```glsl
  //  //layout (set=m, binding=n) uniform texture2D <variable name>;
  //  layout (set=m, binding=n) uniform sampler2D my_texture;
  //  frag_color = texture( my_texture, uv );
  //  ```
  //  ```hlsl
  //  Texture2D my_texture;
  //  SamplerState my_sampler;
  //  my_texture.Sample(my_sampler, uv);
  //  ```
  //

  //
  // Prepare a combined image sampler (We have already created a VkSampler)
  //  To create a variable that represents a combined image sampler inside GLSL shaders, we
  //  need to use a sampler keyword (possibly with a prefix) with an appropriate dimensionality.
  //  Don't confuse samplers and combined image samplers—both use a sampler keyword inside shaders,
  //  but combined image samplers additionally have a dimensionality specified like in the following
  //  example:
  //  ```glsl
  //  layout (set=m, binding=n) uniform sampler2D my_texture;
  //  frag_color = texture( my_texture, uv );
  //  ```
  //  ```hlsl
  //  Texture2DArray<float4> texture_arr : register(t0);  // t0 is a texture slot
  //  SamplerState my_sampler : register(s0);             // s0 is a sampler slot
  //
  //  float2 uv_coords = float2(0.5f, 0.5f);  // Example UV coordinates
  //  int layer_idx = 0;                      // Example array slice index
  //
  //  // Combine UV and array index into a float3
  //  float3 sample_coords = float3(uv_coords.x, uv_coords.y, layer_idx);
  //
  //  // Sample the texture array
  //  float4 color = texture_arr.Sample(my_sampler, sample_coords);
  //  ```
  //

  //
  // Descriptor sets gather many resources (descriptors) in one object. They are later bound to a
  //  pipeline to establish an interface between our application and the shaders. But for the
  //  hardware to know what resources are grouped in a set, how many resources of each type there
  //  are, and what their order is, we need to create a descriptor set layout.
  //
  
  //
  // When we want to create layouts, we need to know what resources (descriptor types) will be used
  //  and what their order will be. The order is specified through bindings—they define the index
  //  (position) of a resource within a given set and are also used inside shaders (with a set
  //  number through a layout qualifier) to specify a resource we want to access:
  //  ```glsl
  //  layout (set=m, binding=n)  // variable definition
  //  ```
  //  We can choose any values for bindings, but we should keep in mind that unused indices may
  //  consume memory and impact the performance of our application.
  //

  //
  // Descriptor set layouts (along with push constant ranges) also form a pipeline layout, which
  // defines what type of resources can be accessed by a given pipeline. Created layouts, apart from
  // pipeline layout creation, are also required during descriptor set allocation.
  //
  VkDescriptorSetLayoutCreateInfo dsl_create_info;
  dsl_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  dsl_create_info.pNext = nullptr;
  dsl_create_info.flags = 0;
  //dsl_create_info.bindingCount = _arr_elem_cnt( arr_bindings );
  //dsl_create_info.pBindings = arr_bindings;
  dsl_create_info.bindingCount = 0;
  dsl_create_info.pBindings = nullptr;

  // Bindings for cs_main_render
  VkDescriptorSetLayoutBinding arr_bindings_main_render[1];
  for ( unsigned int i = 0; i < _arr_elem_cnt( arr_bindings_main_render ); i++ ) {
    arr_bindings_main_render[i].binding = i;
    arr_bindings_main_render[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    arr_bindings_main_render[i].descriptorCount = 1;
    arr_bindings_main_render[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    arr_bindings_main_render[i].pImmutableSamplers = &cara_vk.sampler_8_2048_tex;
  }

  // Bindings for cs_copy_to_swapchain_img
  VkDescriptorSetLayoutBinding arr_bindings_cp2sc[1];
  for ( unsigned int i = 0; i < _arr_elem_cnt( arr_bindings_cp2sc ); i++ ) {
    arr_bindings_cp2sc[i].binding = i;
    arr_bindings_cp2sc[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    arr_bindings_cp2sc[i].descriptorCount = 1;
    arr_bindings_cp2sc[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    arr_bindings_cp2sc[i].pImmutableSamplers = nullptr;
  }

  SPALL_BEGIN("create_dsls");
  result = vkCreateDescriptorSetLayout( cara_vk.log_dev, &dsl_create_info,
      &cara_vk.vk_alloc_callbacks, &cara_vk.dsl_depth_only
      //nullptr, &cara_vk.dsl_depth_only
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during DSL creation.(D) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  dsl_create_info.bindingCount = _arr_elem_cnt( arr_bindings_main_render );
  dsl_create_info.pBindings = arr_bindings_main_render;
  result = vkCreateDescriptorSetLayout( cara_vk.log_dev, &dsl_create_info,
      &cara_vk.vk_alloc_callbacks, &cara_vk.dsl_main_render
      //nullptr, &cara_vk.dsl_main_render
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during DSL creation.(M) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  dsl_create_info.bindingCount = _arr_elem_cnt( arr_bindings_cp2sc );
  dsl_create_info.pBindings = arr_bindings_cp2sc;
  result = vkCreateDescriptorSetLayout( cara_vk.log_dev, &dsl_create_info,
      &cara_vk.vk_alloc_callbacks, &cara_vk.dsl_copy2sc
      //nullptr, &cara_vk.dsl_copy2sc
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during DSL creation.(C) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  //
  // Create a descriptor pool
  //
  VkDescriptorPoolSize arr_pool_sizes[] = {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },  // The maximal number of combined samplers
                                                       //  that can be allocated in the pool.
  };
  VkDescriptorPoolCreateInfo dp_create_info;
  dp_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  dp_create_info.pNext = nullptr;
  dp_create_info.flags = 0;
  dp_create_info.maxSets = 3;
  dp_create_info.poolSizeCount = _arr_elem_cnt( arr_pool_sizes );
  dp_create_info.pPoolSizes = arr_pool_sizes;
  SPALL_BEGIN("create_dp");
  result = vkCreateDescriptorPool( cara_vk.log_dev, &dp_create_info,
      &cara_vk.vk_alloc_callbacks, &cara_vk.dp_main_render
      //nullptr, &cara_vk.dp_main_render
      );
  // When we have created a pool, we can allocate descriptor sets from it. But we must remember that
  //  we can't do this in multiple threads at the same time.
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during descriptor pool creation. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();


  //
  // Allocate a descriptor set
  //
  VkDescriptorSetAllocateInfo main_render_ds_alloc_info;
  main_render_ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  main_render_ds_alloc_info.pNext = nullptr;
  main_render_ds_alloc_info.descriptorPool = cara_vk.dp_main_render;
  main_render_ds_alloc_info.descriptorSetCount = 1;
  main_render_ds_alloc_info.pSetLayouts = &cara_vk.dsl_main_render;
  SPALL_BEGIN("alloc_ds");
  result = vkAllocateDescriptorSets( cara_vk.log_dev, &main_render_ds_alloc_info,
      &cara_vk.ds_main_render );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during descriptor set allocation. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  //
  // Write on the descriptor
  //
  VkDescriptorImageInfo desc_img_info;
  desc_img_info.sampler = cara_vk.sampler_8_2048_tex;
  desc_img_info.imageView = cara_vk.iv_8_2048_tex;
  desc_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkWriteDescriptorSet arr_desc_writes[1];
  arr_desc_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  arr_desc_writes[0].pNext = nullptr;
  arr_desc_writes[0].dstSet = cara_vk.ds_main_render;
  arr_desc_writes[0].dstBinding = 0;
  arr_desc_writes[0].dstArrayElement = 0;
  arr_desc_writes[0].descriptorCount = 1;
  arr_desc_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  arr_desc_writes[0].pImageInfo = &desc_img_info; // For samplers and all kinds of image
                                                  //  descriptors.
  arr_desc_writes[0].pBufferInfo = nullptr;       // For uniform and storage buffers and their
                                                  //  dynamic variations.
  arr_desc_writes[0].pTexelBufferView = nullptr;  // For uniform and storage texel buffers.
  SPALL_BEGIN("update_ds");
  vkUpdateDescriptorSets( cara_vk.log_dev,
      _arr_elem_cnt( arr_desc_writes ), arr_desc_writes,  // VkWriteDescriptorSet
      0, nullptr );  // Copying from another descriptor set can be faster than normal writing.
  SPALL_END();


  //
  // Create a shader module
  //

  // A VkShaderModule is an opaque handle to a shader module object in the Vulkan API.
  // It serves as a container for shader code written in the SPIR-V binary format.
  VkShaderModuleCreateInfo sm_create_info;
  sm_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  sm_create_info.pNext = nullptr;
  sm_create_info.flags = 0;
  sm_create_info.codeSize = cara_vk.len_simple_vert;
  sm_create_info.pCode = cara_vk.spv_simple_vert;
  SPALL_BEGIN("create_shader_modules");
  // Shader compilation and linkage is performed during the pipeline object creation, not when a
  //  shader module is created.
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &cara_vk.vk_alloc_callbacks,
      &cara_vk.sm_simple_vert );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(V). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_simple_frag;
  sm_create_info.pCode = cara_vk.spv_simple_frag;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &cara_vk.vk_alloc_callbacks,
      &cara_vk.sm_simple_frag );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(F). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_depth_only;
  sm_create_info.pCode = cara_vk.spv_depth_only;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &cara_vk.vk_alloc_callbacks,
      &cara_vk.sm_depth_only );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(D). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_depth_only_frag;
  sm_create_info.pCode = cara_vk.spv_depth_only_frag;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &cara_vk.vk_alloc_callbacks,
      &cara_vk.sm_depth_only_frag );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(D). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  //
  // Shader stages
  //
  VkPipelineShaderStageCreateInfo main_render_plss_create_info[2];
  for ( unsigned int ss_idx = 0; ss_idx < _arr_elem_cnt(main_render_plss_create_info); ss_idx++ ) {
    main_render_plss_create_info[ss_idx].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    main_render_plss_create_info[ss_idx].pNext = nullptr;
    main_render_plss_create_info[ss_idx].flags = 0;
    main_render_plss_create_info[ss_idx].pSpecializationInfo = nullptr;
  }
  constexpr unsigned int vertex = 0;
  constexpr unsigned int fragment = 1;
  main_render_plss_create_info[vertex].stage = VK_SHADER_STAGE_VERTEX_BIT;
  main_render_plss_create_info[vertex].module = cara_vk.sm_simple_vert;
  main_render_plss_create_info[vertex].pName = "vert_main";
  main_render_plss_create_info[fragment].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  main_render_plss_create_info[fragment].module = cara_vk.sm_simple_frag;
  main_render_plss_create_info[fragment].pName = "frag_main";
  // Shader stages for the depth-only prepass
  // NOTE Fragment shader doesn't really do any work in depth-only prepass;  Do I really need to
  //  specify them here for the early-fragment tests and late-fragment tests?
  VkPipelineShaderStageCreateInfo depth_only_plss_create_info[2];
  for ( unsigned int ss_idx = 0; ss_idx < _arr_elem_cnt(depth_only_plss_create_info); ss_idx++ ) {
    depth_only_plss_create_info[ss_idx].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    depth_only_plss_create_info[ss_idx].pNext = nullptr;
    depth_only_plss_create_info[ss_idx].flags = 0;
    depth_only_plss_create_info[ss_idx].pSpecializationInfo = nullptr;
  }
  depth_only_plss_create_info[vertex].stage = VK_SHADER_STAGE_VERTEX_BIT;
  depth_only_plss_create_info[vertex].module = cara_vk.sm_depth_only;
  depth_only_plss_create_info[vertex].pName = "vert_main";
  depth_only_plss_create_info[fragment].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  depth_only_plss_create_info[fragment].module = cara_vk.sm_depth_only_frag;
  depth_only_plss_create_info[fragment].pName = "frag_main";
  // Shader stages for cs_copy_to_swapchain_img
  VkPipelineShaderStageCreateInfo copy2sc_plss_create_info[2];
  for ( unsigned int ss_idx = 0; ss_idx < _arr_elem_cnt(copy2sc_plss_create_info); ss_idx++ ) {
    copy2sc_plss_create_info[ss_idx].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    copy2sc_plss_create_info[ss_idx].pNext = nullptr;
    copy2sc_plss_create_info[ss_idx].flags = 0;
    copy2sc_plss_create_info[ss_idx].pSpecializationInfo = nullptr;
  }
  copy2sc_plss_create_info[vertex].stage = VK_SHADER_STAGE_VERTEX_BIT;
  copy2sc_plss_create_info[vertex].module = cara_vk.sm_simple_vert;
  copy2sc_plss_create_info[vertex].pName = "vert_main";
  copy2sc_plss_create_info[fragment].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  copy2sc_plss_create_info[fragment].module = cara_vk.sm_simple_frag;
  copy2sc_plss_create_info[fragment].pName = "frag_main";

  //
  // Vertex input binding description
  //
  VkVertexInputBindingDescription main_render_vib_desc;
  main_render_vib_desc.binding = 0;
  main_render_vib_desc.stride = sizeof(float)*3 + sizeof(float)*3;  // position and color
  main_render_vib_desc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
  VkVertexInputAttributeDescription main_render_arr_via_desc[] = {
    // location, binding, format, offset
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*3 },
  };
  VkPipelineVertexInputStateCreateInfo main_render_plvis_create_info;
  main_render_plvis_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  main_render_plvis_create_info.pNext = nullptr;
  main_render_plvis_create_info.flags = 0;
  main_render_plvis_create_info.vertexBindingDescriptionCount = 1;
  main_render_plvis_create_info.pVertexBindingDescriptions = &main_render_vib_desc;
  main_render_plvis_create_info.vertexAttributeDescriptionCount = _arr_elem_cnt( main_render_arr_via_desc );
  main_render_plvis_create_info.pVertexAttributeDescriptions = main_render_arr_via_desc;
  // Vertex input binding description for depth-only prepass
  VkVertexInputBindingDescription depth_only_vib_desc;
  depth_only_vib_desc.binding = 0;
  depth_only_vib_desc.stride = sizeof(float)*3;  // position
  depth_only_vib_desc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
  VkVertexInputAttributeDescription depth_only_arr_via_desc[] = {
    // location, binding, format, offset
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
  };
  VkPipelineVertexInputStateCreateInfo depth_only_plvis_create_info;
  depth_only_plvis_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  depth_only_plvis_create_info.pNext = nullptr;
  depth_only_plvis_create_info.flags = 0;
  depth_only_plvis_create_info.vertexBindingDescriptionCount = 1;
  depth_only_plvis_create_info.pVertexBindingDescriptions = &depth_only_vib_desc;
  depth_only_plvis_create_info.vertexAttributeDescriptionCount =
    _arr_elem_cnt( depth_only_arr_via_desc );
  depth_only_plvis_create_info.pVertexAttributeDescriptions = depth_only_arr_via_desc;
  // Vertex input binding description for cs_copy_to_swapchain_img
  VkVertexInputBindingDescription copy2sc_vib_desc;
  copy2sc_vib_desc.binding = 0;
  copy2sc_vib_desc.stride = sizeof(float)*3;  // position
  copy2sc_vib_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  VkVertexInputAttributeDescription copy2sc_arr_via_desc[] = {
    // location, binding, format, offset
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
  };
  VkPipelineVertexInputStateCreateInfo copy2sc_plvis_create_info;
  copy2sc_plvis_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  copy2sc_plvis_create_info.pNext = nullptr;
  copy2sc_plvis_create_info.flags = 0;
  copy2sc_plvis_create_info.vertexBindingDescriptionCount = 1;
  copy2sc_plvis_create_info.pVertexBindingDescriptions = &copy2sc_vib_desc;
  copy2sc_plvis_create_info.vertexAttributeDescriptionCount =
    _arr_elem_cnt( copy2sc_arr_via_desc );
  copy2sc_plvis_create_info.pVertexAttributeDescriptions = copy2sc_arr_via_desc;

  //
  // An input assembly state is required for the graphics pipeline creation.
  //
  VkPipelineInputAssemblyStateCreateInfo plias_create_info;
  plias_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  plias_create_info.pNext = nullptr;
  plias_create_info.flags = 0;
  plias_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  plias_create_info.primitiveRestartEnable = VK_FALSE;

  //
  // Viewport ans scissor test
  //
  // The viewport and scissor test state is optional, though commonly used—we don't need to provide
  //  it when rasterization is disabled.
  VkViewport viewport;
  // In Vulkan, the scissor test is always enabled.
  VkRect2D scissor;
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = test_width;
  viewport.height = test_height;
  viewport.minDepth = .0f;
  viewport.maxDepth = 1.0f;
  scissor.offset = (VkOffset2D) {0};
  scissor.extent = (VkExtent2D) { test_width, test_height };
  // If we want to change some of the viewport or scissor test parameters, we need to recreate a
  //  pipeline. But during the pipeline creation, we can specify that the viewport and scissor test
  //  parameters are dynamic. This way, we don't need to recreate a pipeline to change these
  //  parameters—we specify them during command buffer recording. But we need to remember that the
  //  number of viewports (and scissor tests) is always specified during the pipeline creation. We
  //  can't change it later.
  VkPipelineViewportStateCreateInfo plvps_create_info;
  plvps_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  plvps_create_info.pNext = nullptr;
  plvps_create_info.flags = 0;
  plvps_create_info.viewportCount = 1;
  plvps_create_info.pViewports = &viewport;
  plvps_create_info.scissorCount = 1;
  plvps_create_info.pScissors = &scissor;

  //
  // VkPipelineRasterizationStateCreateInfo
  //
  // A rasterization state is always required during the graphics pipeline creation.
  // In OpenGL, by default, counterclockwise faces were considered front and culling was disabled.
  //  In Vulkan, there is no default state so how we define these parameters is up to us.
  VkPipelineRasterizationStateCreateInfo plrs_create_info;
  plrs_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  plrs_create_info.pNext = nullptr;
  plrs_create_info.flags = 0;
  plrs_create_info.depthClampEnable = VK_FALSE;  // Use a true value if a depth value for fragments
                                                 //  whose depth is outside of the min/max range
                                                 //  specified in a viewport state should be clamped
                                                 //  within this range, or use a false value if
                                                 //  fragments with depth outside of the this range
                                                 //  should be clipped (discarded); when the
                                                 //  depthClampEnable feature is not enabled only a
                                                 //  false value can be specified.
  plrs_create_info.rasterizerDiscardEnable = VK_FALSE;  // It controls whether primitives are
                                                        // discarded immediately before the
                                                        // rasterization stage.
  plrs_create_info.polygonMode = VK_POLYGON_MODE_FILL;
  plrs_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
  plrs_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  plrs_create_info.depthBiasEnable = VK_FALSE;
  plrs_create_info.depthBiasConstantFactor = .0f;
  plrs_create_info.depthBiasClamp = .0f;  // It must be 0.0 when depthBiasEnable is VK_FALSE, etc.
  plrs_create_info.depthBiasSlopeFactor = .0f;
  plrs_create_info.lineWidth = 1.0f;

  //
  // MSAA
  //
  VkPipelineMultisampleStateCreateInfo plmss_create_info;
  plmss_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  plmss_create_info.pNext = nullptr;
  plmss_create_info.flags = 0;
  plmss_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  plmss_create_info.sampleShadingEnable = VK_FALSE;
  //plmss_create_info.minSampleShading = .5f;
  plmss_create_info.pSampleMask = nullptr;
  plmss_create_info.alphaToCoverageEnable = VK_FALSE;
  plmss_create_info.alphaToOneEnable = VK_FALSE;

  //
  // Depth/Stencil
  //
  VkPipelineDepthStencilStateCreateInfo pldss_create_info;
  pldss_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  pldss_create_info.pNext = nullptr;
  pldss_create_info.flags = 0;
  pldss_create_info.depthTestEnable = VK_TRUE;
  pldss_create_info.depthWriteEnable = VK_TRUE;
  pldss_create_info.depthCompareOp = VK_COMPARE_OP_GREATER;
  pldss_create_info.depthBoundsTestEnable = VK_FALSE;
  pldss_create_info.stencilTestEnable = VK_FALSE;
  VkStencilOpState stencil_op_state_front;
  stencil_op_state_front.failOp = VK_STENCIL_OP_KEEP;
  stencil_op_state_front.passOp = VK_STENCIL_OP_KEEP;
  stencil_op_state_front.depthFailOp = VK_STENCIL_OP_KEEP;
  stencil_op_state_front.compareOp = VK_COMPARE_OP_EQUAL;
  stencil_op_state_front.compareMask = ~UINT32_C(0);
  stencil_op_state_front.writeMask = ~UINT32_C(0);
  stencil_op_state_front.reference = 0;
  VkStencilOpState stencil_op_state_back;
  stencil_op_state_back.failOp = VK_STENCIL_OP_KEEP;
  stencil_op_state_back.passOp = VK_STENCIL_OP_KEEP;
  stencil_op_state_back.depthFailOp = VK_STENCIL_OP_KEEP;
  stencil_op_state_back.compareOp = VK_COMPARE_OP_NEVER;
  stencil_op_state_back.compareMask = 0;
  stencil_op_state_back.writeMask = 0;
  stencil_op_state_back.reference = 0;
  pldss_create_info.front = stencil_op_state_front;
  pldss_create_info.back = stencil_op_state_back;
  pldss_create_info.minDepthBounds = .0f;
  pldss_create_info.maxDepthBounds = 1.0f;

  //
  // VkPipelineColorBlendAttachmentState
  //
  // The blending state is optional and is not required if rasterization is disabled or when there
  //  are no color attachments in a subpass, in which a given graphics pipeline is used.
  VkPipelineColorBlendAttachmentState plcbas;
  plcbas.blendEnable = VK_FALSE;
  plcbas.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  plcbas.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  plcbas.colorBlendOp = VK_BLEND_OP_ADD;
  plcbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  plcbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  plcbas.alphaBlendOp = VK_BLEND_OP_ADD;
  plcbas.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT |
    VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT |
    0;
  VkPipelineColorBlendStateCreateInfo plcbs_create_info;
  plcbs_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  plcbs_create_info.pNext = nullptr;
  plcbs_create_info.flags = 0;
  plcbs_create_info.logicOpEnable = VK_FALSE;
  plcbs_create_info.logicOp = VK_LOGIC_OP_NO_OP;
  plcbs_create_info.attachmentCount = 1;
  plcbs_create_info.pAttachments = &plcbas;
  plcbs_create_info.blendConstants[0] = .0f;
  plcbs_create_info.blendConstants[1] = .0f;
  plcbs_create_info.blendConstants[2] = .0f;
  plcbs_create_info.blendConstants[3] = .0f;

  //
  // Dynamic states
  //
  // Define the dynamic parts of the pipeline
  VkDynamicState dynamic_states[] = {
    VK_DYNAMIC_STATE_VIEWPORT,  // vkCmdSetViewport()
    VK_DYNAMIC_STATE_SCISSOR,  // vkCmdSetScissor()
  };
  VkPipelineDynamicStateCreateInfo plds_create_info;
  plds_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  plds_create_info.pNext = nullptr;
  plds_create_info.flags = 0;
  plds_create_info.dynamicStateCount = _arr_elem_cnt( dynamic_states );
  plds_create_info.pDynamicStates = dynamic_states;

  //
  // Create a pipeline layout
  //
  VkPushConstantRange arr_pc_ranges[1];
  arr_pc_ranges[0].offset = 0;  assume( arr_pc_ranges[0].offset % 4 == 0 );
  arr_pc_ranges[0].size = 64;  assume( arr_pc_ranges[0].size % 4 == 0 );
  arr_pc_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  VkPipelineLayoutCreateInfo pll_create_info;
  pll_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pll_create_info.pNext = nullptr;
  pll_create_info.flags = 0;
  pll_create_info.setLayoutCount = 1;
  pll_create_info.pSetLayouts = &cara_vk.dsl_depth_only;
  pll_create_info.pushConstantRangeCount = _arr_elem_cnt( arr_pc_ranges );
  pll_create_info.pPushConstantRanges = arr_pc_ranges;
  // Usually, multiple pipelines will access different resources. During command buffer recording,
  // we bind a given pipeline and descriptor sets. Only after that can we issue drawing commands.
  // When we switch from one pipeline to another, we need to bind new descriptor sets according to
  // the pipeline's needs. But frequently binding different descriptor sets may impact the
  // performance of our application. That's why it is good to create pipelines with similar (or
  // compatible) layouts and bind descriptor sets that do not change too often (that are common for
  // many pipelines) to indices near the 0 (or near the start of a layout). This way, when we switch
  // pipelines, descriptor sets near the start of the pipeline layout (from index 0 to some index N)
  // can still be used and don't need to be updated. It is only necessary to bind the different
  // descriptor sets--those that are placed at greater indices (after the given index N). But one
  // additional condition must be met-- to be similar (or compatible), the pipeline layouts must use
  // the same push constant ranges.
  SPALL_BEGIN("create_pipeline");
  result = vkCreatePipelineLayout( cara_vk.log_dev, &pll_create_info,
      &cara_vk.vk_alloc_callbacks, &cara_vk.pll_depth_only
      //nullptr, &cara_vk.pipeline_layout
      );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating the pipeline layout. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  //
  // Create a graphics pipeline
  //
  VkGraphicsPipelineCreateInfo arr_gpl_create_infos[cara_nr_subpasses];
  for ( unsigned int sp_idx = 0; sp_idx < cara_nr_subpasses; sp_idx++ ) {
    arr_gpl_create_infos[sp_idx].sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    arr_gpl_create_infos[sp_idx].pNext = nullptr;
    arr_gpl_create_infos[sp_idx].flags = 0;
    arr_gpl_create_infos[sp_idx].basePipelineHandle = VK_NULL_HANDLE;  // It is a rare
                        // situation when each created pipeline doesn't have any common state with
                        // other pipelines. That's why, to speed up the pipeline creation, it is
                        // possible to specify that a pipeline can be a parent of other pipelines
                        // (allow derivatives) or that the pipeline will be a child of (derived
                        // from) another pipeline. To use this feature and shorten the time needed
                        // to create a pipeline, we can use basePipelineHandle or basePipelineIndex
  }

  SPALL_BEGIN("rp_n_fb");
  err = create_renderpass_and_framebuffer( &cara_vk );
  SPALL_END();
  if ( err ) {
    goto stop_everything_after_failure;
  }

  // cs_depth_only pipeline
  arr_gpl_create_infos[cs_depth_only].stageCount = _arr_elem_cnt( depth_only_plss_create_info );
  arr_gpl_create_infos[cs_depth_only].pStages = depth_only_plss_create_info;
  arr_gpl_create_infos[cs_depth_only].pVertexInputState = &depth_only_plvis_create_info;
  arr_gpl_create_infos[cs_depth_only].pInputAssemblyState = &plias_create_info;
  arr_gpl_create_infos[cs_depth_only].pTessellationState = nullptr;
  arr_gpl_create_infos[cs_depth_only].pViewportState = &plvps_create_info;
  arr_gpl_create_infos[cs_depth_only].pRasterizationState = &plrs_create_info;
  arr_gpl_create_infos[cs_depth_only].pMultisampleState = &plmss_create_info;  // NOTE
                        // pMultisampleState is a pointer to a VkPipelineMultisampleStateCreateInfo
                        // structure defining multisample state used when rasterization is enabled.
                        // If the VK_EXT_extended_dynamic_state3 extension is enabled, it can be
                        // NULL if the pipeline is created with all of
                        // VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
                        // VK_DYNAMIC_STATE_SAMPLE_MASK_EXT, and
                        // VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT dynamic states set, and
                        // either the alphaToOne feature is not enabled or
                        // VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT is set, in which case
                        // VkPipelineMultisampleStateCreateInfo::sampleShadingEnable is assumed to
                        // be VK_FALSE.
  arr_gpl_create_infos[cs_depth_only].pDepthStencilState = &pldss_create_info;
  arr_gpl_create_infos[cs_depth_only].pColorBlendState = nullptr;  // No color attachment
                        // TODO. Write &plcbs_create_info here for cs_main_render
  arr_gpl_create_infos[cs_depth_only].pDynamicState = &plds_create_info;
  arr_gpl_create_infos[cs_depth_only].layout = cara_vk.pll_depth_only;
  arr_gpl_create_infos[cs_depth_only].renderPass = cara_vk.render_pass;
  arr_gpl_create_infos[cs_depth_only].subpass = cs_depth_only;
  arr_gpl_create_infos[cs_depth_only].basePipelineIndex = -1;  // We can't use both
                        // basePipelineHandle and basePipelineIndex members; we can provide value
                        // only for one of them. If we want to specify a handle, we must provide a
                        // -1 value for the basePipelineIndex field. If we want to specify an index,
                        // we need to provide a VK_NULL_HANDLE value for basePipelineHandle

  // cs_main_render pipeline
  arr_gpl_create_infos[cs_main_render].stageCount = _arr_elem_cnt( main_render_plss_create_info );
  arr_gpl_create_infos[cs_main_render].pStages = main_render_plss_create_info;
  arr_gpl_create_infos[cs_main_render].pVertexInputState = &main_render_plvis_create_info;
  arr_gpl_create_infos[cs_main_render].pInputAssemblyState = &plias_create_info;
  arr_gpl_create_infos[cs_main_render].pTessellationState = nullptr;
  arr_gpl_create_infos[cs_main_render].pViewportState = &plvps_create_info;
  arr_gpl_create_infos[cs_main_render].pRasterizationState = &plrs_create_info;
  arr_gpl_create_infos[cs_main_render].pMultisampleState = &plmss_create_info;
  arr_gpl_create_infos[cs_main_render].pDepthStencilState = &pldss_create_info;
  arr_gpl_create_infos[cs_main_render].pColorBlendState = &plcbs_create_info;
  arr_gpl_create_infos[cs_main_render].pDynamicState = &plds_create_info;
  arr_gpl_create_infos[cs_main_render].layout = cara_vk.pll_main_render;
  arr_gpl_create_infos[cs_main_render].renderPass = cara_vk.render_pass;
  arr_gpl_create_infos[cs_main_render].subpass = cs_main_render;
  arr_gpl_create_infos[cs_main_render].basePipelineIndex = cs_depth_only;

  // cs_copy_to_swapchain_img pipeline
  arr_gpl_create_infos[cs_copy_to_swapchain_img].stageCount =
    _arr_elem_cnt( copy2sc_plss_create_info );
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pStages = copy2sc_plss_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pVertexInputState = &copy2sc_plvis_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pInputAssemblyState = &plias_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pTessellationState = nullptr;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pViewportState = &plvps_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pRasterizationState = &plrs_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pMultisampleState = &plmss_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pDepthStencilState = &pldss_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pColorBlendState = &plcbs_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].pDynamicState = &plds_create_info;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].layout = cara_vk.pll_copy2sc;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].renderPass = cara_vk.render_pass;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].subpass = cs_copy_to_swapchain_img;
  arr_gpl_create_infos[cs_copy_to_swapchain_img].basePipelineIndex = cs_main_render;
  // NOTE No pipeline cache utilized...
  SPALL_BEGIN("create_G_pl");
  vkCreateGraphicsPipelines( cara_vk.log_dev,
      VK_NULL_HANDLE,  // Creating a pipeline object is a complicated and time-consuming process
                       // from the driver's perspective. A pipeline object is not a simple wrapper
                       // for parameters set during the creation. It involves preparing the states
                       // of all programmable and fixed pipeline stages, setting an interface
                       // between shaders and descriptor resources, compiling and linking shader
                       // programs, and performing error checking (that is, checking if shaders are
                       // linked properly). Results of these operations may be stored in a cache.
                       // This cache can then be reused to speed up the creation of pipeline objects
                       // with similar properties.
      _arr_elem_cnt(arr_gpl_create_infos), arr_gpl_create_infos,
      &cara_vk.vk_alloc_callbacks,
      cara_vk.arr_pipelines
      );
  SPALL_END();




  //
  // Bind descriptor sets to the pipeline layout
  //
  SPALL_BEGIN("bind_ds");
  vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, cara_vk.pll_depth_only,
      0,  // The first set number
      1, &cara_vk.ds_main_render,  // Descriptor sets
      0, nullptr );  // Dynamic offsets
  SPALL_END();


  //
  // Begin a renderpass
  //
  VkImageView arr_ivs[3];
  arr_ivs[fb_depth] = cara_vk.iv_depth;
  arr_ivs[fb_color] = cara_vk.iv_main_render_tgt;
  arr_ivs[fb_swapchain] = cara_vk.iv_swapchain_img;
  VkRenderPassAttachmentBeginInfoKHR rp_att_begin_info;
  rp_att_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR;
  rp_att_begin_info.pNext = nullptr;
  rp_att_begin_info.attachmentCount = _arr_elem_cnt( arr_ivs );
  rp_att_begin_info.pAttachments = arr_ivs;
  constexpr VkClearValue clear_val = (VkClearValue) { .depthStencil.depth = 1.0f };
  VkRenderPassBeginInfo rp_begin_info;
  rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin_info.pNext = &rp_att_begin_info;
  rp_begin_info.renderPass = cara_vk.render_pass;
  rp_begin_info.pNext = nullptr;
  rp_begin_info.framebuffer = cara_vk.framebuffer;
  rp_begin_info.renderArea = (VkRect2D) { {0, 0}, {test_width, test_height} };
  rp_begin_info.clearValueCount = 1;  // clearValueCount must be greater than the largest attachment
                                      //  index in renderPass specifying a loadOp (or stencilLoadOp,
                                      //  if the attachment has a depth/stencil format) of
                                      //  VK_ATTACHMENT_LOAD_OP_CLEAR.
  rp_begin_info.pClearValues = &clear_val;


  // TODO acquire one swapchain img


  // VK_SUBPASS_CONTENTS_INLINE  specifies that the contents of the subpass will be recorded inline
  //  in the primary command buffer, and secondary command buffers must not be executed within the
  //  subpass.
  //vkCmdBeginRenderPass( cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE );

  // TODO cs_depth_only

  //vkCmdNextSubpass( cmd_buf, VK_SUBPASS_CONTENTS_INLINE );

  // TODO cs_main_render_pass

  //vkCmdNextSubpass( cmd_buf, VK_SUBPASS_CONTENTS_INLINE );

  // TODO cs_copy_to_swapchain_img

  // End the render pass
  //vkCmdEndRenderPass( cmd_buf );  // Recording this function in a command buffer performs multiple
                                  // operations. Execution and memory dependencies are introduced
                                  // (like the ones in memory barriers) and image layout transitions
                                  // are performed—images are transitioned from layouts specified
                                  // for the last subpass to the value of a final layout (refer to
                                  // the Specifying attachment descriptions recipe). Also
                                  // multisample resolving is performed on color attachments for
                                  // which resolving was specified in the last subpass. Additionally
                                  // for attachments whose contents should be preserved after the
                                  // render pass, attachment data may be transferred from the cache
                                  // to the image's memory.






  //
  // Prepare to submit the command buffer
  //
  VkSemaphoreSubmitInfoKHR semaphore_wait_info;
  semaphore_wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
  semaphore_wait_info.pNext = nullptr;
  semaphore_wait_info.semaphore = cara_vk.arr_semaphore_before_A[is_frame_id_even];
  semaphore_wait_info.value = csb_data_transfer_done;
  semaphore_wait_info.stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  semaphore_wait_info.deviceIndex = 0;

  semaphore_sgnl_info.semaphore = cara_vk.arr_semaphore_after_A[is_frame_id_even];
  semaphore_sgnl_info.value = csb_present_swapchain_img;
  semaphore_sgnl_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
  semaphore_sgnl_info.deviceIndex = 0;

  cmd_buf_submit_info.commandBuffer = cmd_buf;;

  submit_info.waitSemaphoreInfoCount = 1;
  submit_info.pWaitSemaphoreInfos = &semaphore_wait_info;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &cmd_buf_submit_info;
  submit_info.signalSemaphoreInfoCount = 1;
  submit_info.pSignalSemaphoreInfos = &semaphore_sgnl_info;

  // End recording
  SPALL_BEGIN("end_cmd_buf");
  result = vkEndCommandBuffer( cmd_buf );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(1c) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  // Submit
  SPALL_BEGIN("submit_cmd_buf");
  result = vkQueueSubmit2KHR( cara_vk.graphics_q, 1, &submit_info, VK_NULL_HANDLE );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  SPALL_BEGIN("infos_pm_return");
  // FIXME Actually, return them at the next even/odd frame id.
  err = infos_pm_return( &cara_vk.vmm.uploads,
      &ws->malloc_helper, cara_malloc, rent_for_stone->start );
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_return for stone failed.. (err_msg=%u)\n", err );
    goto stop_everything_after_failure;
  }
  err = infos_pm_return( &cara_vk.vmm.uploads,
      &ws->malloc_helper, cara_malloc, rent_for_sand->start );
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_return for sand failed.. (err_msg=%u)\n", err );
    goto stop_everything_after_failure;
  }
  SPALL_END();


  // Explicitly reset the command buffers instead of implicitly resetting by beginning to record
  constexpr bool release_resources = false;  // XXX what is this?
  // For transfer commands

  uint64_t wait_val = csb_data_transfer_done;
  VkSemaphoreWaitInfo wait_info;
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.pNext = nullptr;
  wait_info.flags = 0;
  wait_info.semaphoreCount = 1;
  wait_info.pSemaphores = &cara_vk.arr_semaphore_before_A[is_frame_id_even];
  wait_info.pValues = &wait_val;
  SPALL_BEGIN("wait_transfer");
  vkWaitSemaphores( cara_vk.log_dev, &wait_info, 1000*1000*UINT64_C(20) );
  SPALL_END();

  SPALL_BEGIN("reset_trans_cmd_pool");
  // TODO Host access to `commandPool` must be externally synchronized
  result = vkResetCommandPool(
      cara_vk.log_dev,
      cara_vk.trans_buf.arr_cmd_pools[
        cara_single.proc_map->nr_processors ],
      release_resources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0 );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during command buffer reset.(t) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();

  wait_val = csb_present_swapchain_img;
  wait_info.pSemaphores = &cara_vk.arr_semaphore_after_A[is_frame_id_even];
  SPALL_BEGIN("wait_graphics");
  vkWaitSemaphores( cara_vk.log_dev, &wait_info, 1000*1000*UINT64_C(20) );
  SPALL_END();

  SPALL_BEGIN("reset_graphics_cmd_pool");
  result = vkResetCommandPool(
      cara_vk.log_dev,
      cara_vk.arr_frame_infos[is_frame_id_even].arr_cmd_pools[
        cara_single.proc_map->nr_processors ],
      release_resources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0 );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during command buffer reset.(g) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_END();  // reset_graphics_cmd_pool
  SPALL_END();  // check_out_vulkan



  //TODO move the code above out of this function definition!









#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
  func_call_stack_element.func = (void(*)(void*))destroy_vulkan_objects;
#pragma GCC diagnostic pop
  func_call_stack_element.arg = ws->worker_info;
  infos_stack_push( cara_single.foremans_quit_stack,
      1, (const void*) &func_call_stack_element,
      31 - _lzcnt_u32(sizeof(struct func_call_stack_element)) );

  //for ( uint_fast8_t worker_idx = 0;
  //    worker_idx < cara_single.proc_map->nr_processors; worker_idx++ ) {
  //  uint_fast16_t idx_job_func_arg_group = cara_get_unoccupied_job_func_args();
  //  while ( idx_job_func_arg_group == UINT16_MAX ) {
  //    idx_job_func_arg_group = cara_get_unoccupied_job_func_args();
  //  }
  //  struct cara_job_func_args_64 * arg_group =
  //    infos_al_get_elem(cara_single.job_arg_pool, idx_job_func_arg_group);
  //  arg_group->data[0] = cara_single.proc_map->nr_processors;

  //  struct cara_job_batch_desc_64 * batch = cara_get_unoccupied_job_batch_desc();
  //  while ( !batch ) {
  //    batch = cara_get_unoccupied_job_batch_desc();
  //  }
  //  for ( uint_fast8_t job_idx = 1; job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
  //      job_idx++ ) {
  //    batch->job_func_idx[job_idx] = 0;
  //    batch->job_flags[job_idx] = 0;
  //  }
  //  batch->job_func_idx[0] = jf_idx_prepare_vulkan;
  //  batch->job_flags[0] = 0;
  //  batch->cache_location[0] = 0;
  //  batch->cache_pattern[0] = 0;
  //  batch->idx_job_func_arg_group[0] = idx_job_func_arg_group;
  //  atomic_store_explicit(&batch->progress, 0, memory_order_release);
  //  atomic_store_explicit(&batch->preferred_core, UINT8_MAX, memory_order_release);
  //}

  //VkSurfaceKHR presentation_surface;
  //if ( !SDL_Vulkan_CreateSurface(vulkan_n_sdl.sdl_window, vk_instance, &presentation_surface) ) {
  //  fprintf( stderr, "[Carbonara] SDL_Vulkan_CreateSurface failed to create a vulkan surface.\n" );
  //  fprintf( stderr, "[Carbonara] SDL_Error: %s\n", SDL_GetError() );
  //  goto lead_quit;
  //}

  struct cara_job_batch_desc_64 * batch = cara_get_unoccupied_job_batch_desc();
  while ( !batch ) {
    batch = cara_get_unoccupied_job_batch_desc();
  }
  for ( uint_fast8_t job_idx = 1;
      job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
      job_idx++ ) {
    batch->job_func_idx[job_idx] = 0;
    batch->job_flags[job_idx] = 0;
  }
  batch->job_func_idx[0] = jf_idx_start_rainbow_scene;
  batch->job_flags[0] = 0;
  /*
  batch->cache_location[0] = 0;
  batch->cache_pattern[0] = 0;
  batch->idx_job_func_arg_group[0] = UINT16_MAX;
  atomic_store_explicit(&batch->progress, 0, memory_order_release);
  atomic_store_explicit(&batch->preferred_core, UINT8_MAX, memory_order_release);
  */


  // Assign a number bigger than 128 to sign_to_start
  atomic_store_explicit( &cara_single.sign_to_start, ~UINT64_C(0), memory_order_release );


  return;

stop_everything_after_failure:

  if ( cara_vk.sm_simple_frag ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_simple_frag, &cara_vk.vk_alloc_callbacks );
    cara_vk.sm_simple_frag = VK_NULL_HANDLE;
  }
  if ( cara_vk.sm_simple_vert ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_simple_vert, &cara_vk.vk_alloc_callbacks );
    cara_vk.sm_simple_vert = VK_NULL_HANDLE;
  }
  if ( cara_vk.sm_depth_only ) {
    vkDestroyShaderModule( cara_vk.log_dev, cara_vk.sm_depth_only, &cara_vk.vk_alloc_callbacks );
    cara_vk.sm_depth_only = VK_NULL_HANDLE;
  }

  if ( cara_vk.pll_depth_only ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_depth_only,
        &cara_vk.vk_alloc_callbacks
        //nullptr
        );
    cara_vk.pll_depth_only = VK_NULL_HANDLE;
  }
  if ( cara_vk.pll_main_render ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_main_render,
        &cara_vk.vk_alloc_callbacks
        //nullptr
        );
    cara_vk.pll_main_render = VK_NULL_HANDLE;
  }

  if ( cara_vk.ds_main_render ) {
    vkResetDescriptorPool( cara_vk.log_dev, cara_vk.dp_main_render, 0 );
    cara_vk.ds_main_render = VK_NULL_HANDLE;
  }

  if ( cara_vk.dp_main_render ) {
    vkDestroyDescriptorPool( cara_vk.log_dev, cara_vk.dp_main_render, &cara_vk.vk_alloc_callbacks );
    //vkDestroyDescriptorPool( cara_vk.log_dev, cara_vk.dp_first, nullptr );
    cara_vk.dp_main_render = VK_NULL_HANDLE;
  }

  if ( cara_vk.dsl_depth_only ) {
    vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_depth_only, &cara_vk.vk_alloc_callbacks );
    //vkDestroyDescriptorSetLayout( cara_vk.log_dev, cara_vk.dsl_depth_only, nullptr );
    cara_vk.dsl_depth_only = VK_NULL_HANDLE;
  }

  if ( cara_vk.sampler_8_2048_tex ) {
    vkDestroySampler( cara_vk.log_dev, cara_vk.sampler_8_2048_tex, &cara_vk.vk_alloc_callbacks );
    //vkDestroySampler( cara_vk.log_dev, cara_vk.sampler_8_2048_tex, nullptr );
    cara_vk.sampler_8_2048_tex = VK_NULL_HANDLE;
  }

  if ( cara_vk.iv_8_2048_tex ) {
    vkDestroyImageView( cara_vk.log_dev, cara_vk.iv_8_2048_tex, &cara_vk.vk_alloc_callbacks );
    //vkDestroyImageView( cara_vk.log_dev, cara_vk.iv_8_2048_tex, nullptr );
    cara_vk.iv_8_2048_tex = VK_NULL_HANDLE;
  }

  if ( malloc ) {
    vkFreeMemory( cara_vk.log_dev, *malloc, &cara_vk.vk_alloc_callbacks );
    //vkFreeMemory( cara_vk.log_dev, *malloc, nullptr );
    *malloc = 0;
    cara_vk.vmm.arr_usages[cara_vk.vmm.arr_best_mtype_idx[vk_buf_type_image_arr_8_2048]] -=
      req.size;
  }

  if ( cara_vk.sampler_8_2048_tex ) {
    vkDestroyImage( cara_vk.log_dev, cara_vk.vki_8_2048_tex, &cara_vk.vk_alloc_callbacks );
    //vkDestroyImage( cara_vk.log_dev, cara_vk.vki_8_2048_tex, nullptr );
    cara_vk.vki_8_2048_tex = VK_NULL_HANDLE;
  }

  batch = 0;
  while ( !batch ) {
    batch = cara_get_unoccupied_job_batch_desc();
  }
  for ( uint_fast8_t job_idx = 1; job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
      job_idx++ ) {
    batch->job_func_idx[job_idx] = 0;
    batch->job_flags[job_idx] = 0;
  }
  batch->job_func_idx[0] = jf_idx_start_kill_chain;
  batch->job_flags[0] = 0;
  /*
  batch->cache_location[0] = 0;
  batch->cache_pattern[0] = 0;
  batch->idx_job_func_arg_group[0] = UINT16_MAX;
  */
  atomic_store_explicit(&batch->progress, 0, memory_order_release);
  atomic_store_explicit(&batch->preferred_core, UINT8_MAX, memory_order_release);

//  struct func_call_stack_element * p_func_call_stack_element;
//  while (( p_func_call_stack_element = infos_stack_pop(
//          cara_single.foremans_quit_stack,
//          sizeof(struct func_call_stack_element)
//          ) )) {
//    if ( p_func_call_stack_element->arg ) {
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
//      ( (void(*)(void*))p_func_call_stack_element->func ) ( p_func_call_stack_element->arg );
//#pragma GCC diagnostic pop
//    } else {
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types-strict"
//#pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types"
//      void (*void_func) () = p_func_call_stack_element->func;
//#pragma GCC diagnostic pop
//      void_func();
//    }
//  }
  return;
}


//-------------------------------------------------------------------------------------------------
static void handle_sigsegv( int sig )
{
  // TODO Produce some dump in /dev/shm/carbonara.dump

  signal(sig, SIG_DFL);
  kill(getpid(), sig);
}

//-------------------------------------------------------------------------------------------------
static void handle_sigint( int sig )
{
//#ifdef DEBUG_MODE
  fprintf( stdout, "[Debug] Caught a signal (%d).\n\n", sig );
//#endif
  switch ( sig ) {
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
      break;
    default:
      fprintf( stderr, "\n  Caught an *unusual* signal! (%d).\n\n", sig );
      break;
  }
  struct cara_job_batch_desc_64 * batch = cara_get_unoccupied_job_batch_desc();
  while ( !batch ) {
    batch = cara_get_unoccupied_job_batch_desc();
  }
  for ( uint_fast8_t job_idx = 1; job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
      job_idx++ ) {
    batch->job_func_idx[job_idx] = 0;
    batch->job_flags[job_idx] = 0;
  }
  batch->job_func_idx[0] = jf_idx_start_kill_chain;
  batch->job_flags[0] = 0;
  /*
  batch->cache_location[0] = 0;
  batch->cache_pattern[0] = 0;
  batch->idx_job_func_arg_group[0] = UINT16_MAX;
  */
  atomic_store_explicit(&batch->progress, 0, memory_order_release);
  atomic_store_explicit(&batch->preferred_core, UINT8_MAX, memory_order_release);

  return;
}


