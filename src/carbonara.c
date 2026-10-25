#include <SDL3/SDL_asyncio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_hints.h>
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
  #define SPALL_BEGIN(name_tag)
  #define SPALL_END()
#endif  // defined(ENABLE_PROFILING)



#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>  // include SDL3/SDL_main.h in the source where "main" function is
                            // supposed to be defined in.

#include <sys/cdefs.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
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
#include "carbonara-math.h"
#include "carbonara-renderer.h"
#include "carbonara.h"
#include "list-jobs.h"
#include "compute-magic.h"
#include "appdev_support-processor_map.h"
#include "appdev_support-thread.h"
#include "memorable_arena.h"
#include "err_msg.h"
#include "carbonara-graphics.h"
#include "carbonara-asset.h"
#include "carbonara-asset-func.h"
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
static char * asset_buf7;
static char * asset_buf8;
static constexpr struct v_pos arr_vert_pos[] = {
  {-cara_1m/2, .y=0, cara_1m/2},  // 0
  {cara_1m/2, .y=0, cara_1m/2},
  {0, .y=(two_m), 0},
  {cara_1m/2, .y=0, -cara_1m/2},
  {-cara_1m/2, .y=0, -cara_1m/2},  // 4

  {-two_m, .y=0, two_m},  // 5
  {-two_m, .y=0, -two_m},
  {two_m, .y=0, -two_m},
  {two_m, .y=0, two_m},  // 8
};
static constexpr struct uv_coords arr_uv_coords[] = {
  {.u=.25f, 0},  // 0
  {.u=.75f, 0},
  {.u=.5f, .5f},
  {.u=.75f, .75f},
  {.u=.25f, .75f},  // 4

  {.u=0, 0},  // 5
  {.u=0, fourm/2},  // Four meters = 2048 texels in mip level 0
  {.u=fourm/2, fourm/2},
  {.u=fourm/2, 0},  // 8
};
static constexpr struct v_idx arr_v_indices[] = {
  {0, 2, 1},
  {3, 2, 4},
  {4, 2, 0},
  {3, 1, 2},
  {4, 0, 3},
  {0, 1, 3},

  {5, 6, 7},
  {5, 7, 8},
};
constexpr _typeof_member(VkDescriptorPoolSize, descriptorCount)
  max_nr_img_descriptors_in_set = 2048;


//-------------------------------------------------------------------------------------------------

// The maximum number of periodically executed jobs
constexpr size_t max_nr_pjobs = 512 - 1;



//-------------------------------------------------------------------------------------------------

//static enum err_msg verbosee_integrity_check_bc7_2048( const char * data );
static enum err_msg init_renderer( struct cara_worker_state * p_ws );


//-------------------------------------------------------------------------------------------------
static void handle_sigint( int sig );
static void handle_sigsegv( int sig );
static void worker_loop( struct cara_worker_state * worker_state );
static void foreman_loop( struct cara_worker_state * worker_state );
void jf_render_frame( uint64_t cur_time_in_5us );

/**
 * \brief Launch a new worker.
 */
static void * launch_worker( void * arg );

/**
 * \brief Initialize the SDL part of the engine
 */
static uint8_t init_sdl( void );


//-------------------------------------------------------------------------------------------------
void jf_police_patrol( [[maybe_unused]] struct cara_worker_state * p_ws )
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
      [[fallthrough]];
    case SDL_APP_SUCCESS:
      destroy_vk_objs( cara_single.proc_map->nr_processors );
      remove_appdev_support_thread();
      clear_processor_map( cara_single.proc_map );
      ma_free( cara_single.mem_arena, 0, cara_single.proc_map );
      ma_end_arena( cara_single.mem_arena );
      break;
    default:
      unreachable();
  }

  // TODO something.


  SPALL_END();  // stuff_done_in_SDL_AppQuit

#ifdef ENABLE_PROFILING
  // Remember to quit or flush your buffers before you exit!
  // If you don't, you might not get the last few events waiting to be written
  spall_buffer_quit( &spall_ctx, &spall_buf_for_main );

  // Freeing our buffer's memory now that spall_buffer_quit has flushed it for us.
  free( spall_buf_for_main.data );

  spall_quit( &spall_ctx );
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
#if defined( DEBUG_MODE )
  fprintf( stdout, "event->type = %#010x\n", event->type );
  switch ( event->type ) {
    case SDL_EVENT_WINDOW_EXPOSED:
      fprintf( stdout, "  SDL_EVENT_WINDOW_EXPOSED\n" );
      fprintf( stdout, "  event->window.data1 = %d\n", event->window.data1 );
      break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      fprintf( stdout, "  SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED\n" );
      fprintf( stdout, "  event->window.data1*data2 = %d*%d\n",
          event->window.data1, event->window.data2 );
      break;
  }
#endif

  switch ( event->type ) {
    case SDL_EVENT_WINDOW_EXPOSED:
      //atomic_store_explicit( &cara_single.render_now, 1, memory_order_release );
      break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      union extent_2d new_size = {
        .parts.width = event->window.data1,
        .parts.height = event->window.data2,
      };
      atomic_store_explicit(
          &cara_vk.win_size.atomic_whole, new_size.whole, memory_order_release );
      break;
  }

  return SDL_APP_CONTINUE;
}


//-------------------------------------------------------------------------------------------------
SDL_AppResult SDL_AppIterate( [[maybe_unused]] void *appstate )
{
  static uint64_t a = 0;

  SPALL_BEGIN("SDL_AppIterate");

  constexpr unsigned int cap = 3000000;
  //constexpr unsigned int cap = 1000;
  if ( a % cap == 0 ) {
    fprintf( stdout, "[TEST] a = %lu\n", a );
    if ( a > cap ) {
      raise( SIGHUP );
    }
  }
  a++;

  const uint_least32_t sign_to_end = atomic_load_explicit(
      &cara_single.sign_to_end, memory_order_acquire );
  if (unlikely( sign_to_end > 0 )) {
    switch( sign_to_end ) {
      case 1:
        SPALL_END();
        return SDL_APP_SUCCESS;
      case 2:
        SPALL_END();
        return SDL_APP_FAILURE;
      default:
        unreachable();
    }
  }

  atomic_store_explicit( &cara_single.render_now, 1, memory_order_release );

  SPALL_END();
  return SDL_APP_CONTINUE;
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
    }
    else {
      fprintf( stderr,
          "[Carbonara] SSE4.2 is not supported either, The CPU is not supported!\n" );
    }
    return SDL_APP_FAILURE;
  }

#elif defined UTILIZE_SSE4_2
  if ( x86_proc_info.avx2 ) {
    fprintf( stdout,
        "[Carbonara] The CPU does support AVX2, why did you executed SSE4.2 version?\n" );
    if ( x86_proc_info.sse42 ) {
      fprintf( stdout, "[Carbonara] launching the SSE4.2 version anyway. "
          "You might see a different performance.\n" );
    }
    else {
      fprintf( stderr,
          "[Carbonara] It's weird that SSE4.2 is not supported while AVX2 is. aborting!\n" );
      return SDL_APP_FAILURE;
    }
  }
  else {
    if ( !x86_proc_info.sse42 ) {
      fprintf( stderr, "[Carbonara] SSE4.2 is not supported by the CPU. aborting!\n" );
      return SDL_APP_FAILURE;
    }
  }
#endif

#if defined UTILIZE_BMI2
  if ( !x86_proc_info.bmi2 ) {
    fprintf( stderr, "[Carbonara] The CPU doesn't support BMI2 instructions. aboritng!\n" );
    return SDL_APP_FAILURE;
  }
#endif

#endif

  enum err_msg err;
  err = init_appdev_support_thread();
  if ( err ) {
    fprintf( stderr, "Failed to initialize multi-threading utilities. (reason: %s)\n",
        arr_emsg_txt[err] );
    return SDL_APP_FAILURE;
  }

  //init_graphics();
  init_info_struct( x86_proc_info.whole );

  atomic_init( &cara_single.sign_to_end, 0 );
  atomic_init( &cara_single.sign_to_start, 0 );
  err = init_carbonara( 16*1024*1024, x86_proc_info.whole );
  if ( err ) {
    fprintf( stderr, "Failed to launch. (reason: %s)\n", arr_emsg_txt[err] );
    return SDL_APP_FAILURE;
  }

  while( atomic_load_explicit(&cara_single.sign_to_start, memory_order_acquire) == 1 ) {
    hold_up_thread( 1000*750 );
  }

  const int ret = SDL_ShowWindow( cara_vk.window );
  if ( !ret ) {
    fprintf( stderr, "[Carbonara] Could not unhide the window. (%s)\n", SDL_GetError() );
  }


  return SDL_APP_CONTINUE;
}


//-------------------------------------------------------------------------------------------------
static uint8_t init_sdl( void )
{
#ifdef DEBUG_MODE
  // By default the application and gpu categories are enabled at the INFO level, the
  // assert category is enabled at the WARN level, test is enabled at the VERBOSE level
  // and all other categories are enabled at the ERROR level.
  SDL_SetLogPriority( SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_AUDIO, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_ASSERT, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_RENDER, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG );
  SDL_SetLogPriority( SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_DEBUG );
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
  //atomic_init( &cara_single.nr_job_arg_groups_taken, 0 );
  atomic_init( &cara_single.nr_job_batch_descriptors_taken, 0 );



  //
  // Prepare processor_map
  //
  cara_single.proc_map = prepare_processor_map();

  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = proc_info;
  x86_proc_info.large_page_size_class = cara_single.proc_map->large_page_size_class;
  assume( x86_proc_info.large_page_size_class != 0 );
  proc_info = x86_proc_info.whole;

  cara_single.proc_info = proc_info;


  //
  // Prepare the time system and the performance traces
  //
  enum err_msg err;
#ifdef ENABLE_PROFILING
  err = init_appdev_support_timer( x86_proc_info.whole, 0 );
  if ( err ) {
    fprintf( stderr, "[ADS-timer] err_msg from init_appdev_support_timer = \"%s\"\n",
        arr_emsg_txt[err] );
    switch ( err ) {
      case emsg_not_found:
        fprintf( stderr, "  We could not find /dev/cpu/0/msr\n" );
        break;
      case emsg_permission_denied:
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
  memset( spall_buf_for_main.data, 0, spall_buf_for_main.length );

  if ( !spall_buffer_init(&spall_ctx, &spall_buf_for_main) ) {
    fprintf( stderr, "[Worker] failed to init the spall buffer.\n" );
  }
  p_spall_buf = &spall_buf_for_main;
#endif  // ENABLE_PROFILING


  //
  // Init memory arena
  //

  SPALL_BEGIN("init_carb-after_prepare_processor_map");

  //size_t large_page_size = page_sizes[x86_proc_info.large_page_size_class];
  cara_single.mem_arena = aligned_alloc( CACHELINE_LEN, sizeof(memorable_arena_t) );
  if ( !cara_single.mem_arena ) {
    fprintf( stderr, "[Carbonara] Error in allocation: %s\n", arr_emsg_txt[emsg_out_of_memory] );
    return emsg_out_of_memory;
  }
  cara_single.mem_arena->id = 0;
  cara_single.len_each_arena = len_each_arena;
  err = ma_init_arena(
      proc_info,
      cara_single.mem_arena,
      cara_single.len_each_arena,
      cara_single.proc_map->nr_processors + 1  // +1 for SDL
      );
  // cara_single.mem_arena->head->arr_lrm
  if ( err ) {
    fprintf( stderr, "[Carbonara] Error in allocation: %s\n", arr_emsg_txt[err] );
    return err;
  }

  struct ma_sub_arena * ma_head = ma_get_sub_arena(cara_single.mem_arena, 1);


  //
  // Temporary struct ms_worker_info and struct cara_worker_state
  //
  struct cara_worker_state tmp_ws = {
    .current_batch = nullptr,
    .worker_idx = cara_single.proc_map->nr_processors,
    .malloc_helper.worker_idx = 0,
    .malloc_helper.node_recently_used = cara_single.mem_arena->head,
  };

  struct cara_malloc_helper * malloc_helper = malloc( sizeof(struct cara_malloc_helper) );
  malloc_helper->worker_idx = cara_single.proc_map->nr_processors;
  malloc_helper->node_recently_used = cara_single.mem_arena->head;

  vk_alloc_callbacks.pfnAllocation = cara_malloc_vk;
  vk_alloc_callbacks.pfnFree = cara_free_vk;
  vk_alloc_callbacks.pfnInternalAllocation = nullptr;
  vk_alloc_callbacks.pfnInternalFree = nullptr;
  vk_alloc_callbacks.pfnReallocation = cara_realloc_vk;
  vk_alloc_callbacks.pUserData = (void*) malloc_helper;
  // We can't arbitrarily mix two different memory allocation systems, use cara_malloc
  //vk_alloc_callbacks.pfnAllocation = default_malloc_vk;
  //vk_alloc_callbacks.pfnFree = vk_free_wrapper;
  //vk_alloc_callbacks.pfnInternalAllocation = nullptr;
  //vk_alloc_callbacks.pfnInternalFree = nullptr;
  //vk_alloc_callbacks.pfnReallocation = default_realloc_vk;
  //vk_alloc_callbacks.pUserData = nullptr;



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

  cara_init_job_list_node( cara_single.job_list );


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

  cara_single.proc_map->processors = (struct ms_worker_info *) ( cara_single.proc_map + 1 );
  const uint_fast16_t offset_bitmap_loc = sizeof(struct processor_map) +
    p_orig_proc_map->nr_processors * sizeof(struct ms_worker_info);
  for ( unsigned int ui = 0; ui < p_orig_proc_map->nr_processors; ui++ ) {
    cara_single.proc_map->processors[ui].cpuset =
      (void*) cara_single.proc_map +
      offset_bitmap_loc + ui * sizeof(struct u_hwloc_bitmap_s);
    //assume ( ! cara_single.proc_map->processors[ui].user_data );
  }
  free( p_orig_proc_map );


#ifdef DEBUG_MODE
  fprintf( stdout, "[MA] capacity = %u\n", ma_head->capacity );
  fprintf( stdout, "[MA] nr_free_chunks = %u\n", ma_head->smm.total_space_left );
  //fprintf( stdout, "[MA] bitmap = mm+%lu\n", (uint8_t*)mm->bitmap - (uint8_t*)mm );
  //fprintf( stdout, "[MA] tip_dynamic = mm+%lu\n", (uint8_t*)mm->tip_dynamic - (uint8_t*)mm );
  //fprintf( stdout, "[MA] tip_grower = mm+%lu\n", (uint8_t*)mm->tip_grower - (uint8_t*)mm );
  //fprintf( stdout, "[MA] len_bitmap_aligned_part = %ld\n", mm->len_bitmap_aligned_part );
  //fprintf( stdout, "[MA] len_unaligned_part_front = %u\n", mm->len_unaligned_part_front );
  //fprintf( stdout, "[MA] len_unaligned_part_back = %u\n", mm->len_unaligned_part_back );
  fprintf( stdout, "[MA] addr. of the memo_map = [%#018lx]\n", (uintptr_t)ma_head );
#endif


  //
  // Write the very first job batch (jf_kickstart)
  //
  struct cara_job_batch_desc_64 * batch = infos_al_get_elem_unsafe( cara_single.job_list, 0 );
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
  unsigned_division_magic_64( &cara_single.magic_divider, nr_pc_in_5us );

  const uint32_t leap_sec_freq = get_performance_frequency() / nr_pc_in_5us;
  // Do we want to deal with offsetting performance counter every leaf second?
  const uint32_t pc_ofs_per_sec = get_performance_frequency() % nr_pc_in_5us;
  if ( pc_ofs_per_sec ) {
    fprintf( stdout, "Please tell the developer about the issue:  "
        "pc_ofs_per_sec = %u;  leap_sec_freq = %u;\n", pc_ofs_per_sec, leap_sec_freq );
    return emsg_system_not_supported;
  }
  assume( !pc_ofs_per_sec );

  atomic_init( &cara_single.cur_time_in_5us_general, 0 );
  atomic_init( &cara_single.cur_time_in_5us_for_render, 0 );
  atomic_init( &cara_single.cur_time_in_5us_for_physics, 0 );
  atomic_init( &cara_single.cur_time_in_5us_for_sim, 0 );
  atomic_init( &cara_single.render_now, 0 );

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
#if !defined( ARE_WE_SHIPPING_IT )
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
#if !defined( ARE_WE_SHIPPING_IT )
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
  signal( SIGHUP, handle_sigint );
  signal( SIGINT, handle_sigint );
  signal( SIGQUIT, handle_sigint );
  signal( SIGTERM, handle_sigint );
  signal( SIGSEGV, handle_sigsegv );


  SPALL_END();  // after prepare_processor_map


  //
  // init SDL3 and vulkan
  //
  SPALL_BEGIN("init_sdl");
  if ( init_sdl() ) {
    fprintf( stderr, "[Carbonara] Could not initialize SDL.\n" );
    return emsg_unknown;
  }
  SPALL_END();


  SPALL_BEGIN("init_vulkan");
  // TODO Set a prefered set of vendor_id_pref + device_id_pref if there is a saved preference data
  if ( init_vulkan(0, 0, cara_single.mem_arena, &tmp_ws) ) {
    fprintf( stderr, "[Carbonara] Could not initialize Vulkan.\n" );
    return emsg_unknown;
  }
  SPALL_END();


  //
  // Load synchronously using an async api
  //

  // NOTE The proper way to synchronously using SDL AsyncIO is using SDL_LoadFileAsync or SDL_WaitAsyncIOResult instead of looping with SDL_GetAsyncIOResult (And when shutting down, call SDL_SignalAsyncIOQueue to unblock any sleeping threads despite there being no new tasks completed.)
  //
  // TODO do it asynchronously
  //
  SPALL_BEGIN("Load stone.dds");
  cara_vk.alb_stone = nullptr;
  cara_vk.alb_sand = nullptr;
  cara_vk.spv_simple_vert = nullptr;
  cara_vk.spv_simple_frag = nullptr;
  cara_vk.spv_z_only = nullptr;
  cara_vk.spv_z_only_frag = nullptr;
  cara_vk.spv_swapchain_vert = nullptr;
  cara_vk.spv_swapchain_frag = nullptr;
  cara_vk.asset_size = 0;
  constexpr char fpath_stone[] = "/home/dewr/Products/carbonara/assets/textures/stone-01.dds";
  constexpr char fpath_sand[] = "/home/dewr/Products/carbonara/assets/textures/sand-01.dds";
  constexpr unsigned int dds_header_size =
    4 + sizeof(TCTex_DDS_HEADER) + sizeof(TCTex_DDS_HEADER_DXT10);
  constexpr unsigned int len_mipmapped_2048 = 16 + 16 + (4*4)*4/4 + (8*8)*4/4 + (16*16)*4/4 +
    (32*32)*4/4 + (64*64)*4/4 + (128*128)*4/4 + (256*256)*4/4 + (512*512)*4/4 + (1024*1024)*4/4 + (2048*2048)*4/4;
  constexpr uint32_t magic_dw = (('D' << 8*0) | ('D' << 8*1) | ('S' << 8*2) | (' ' << 8*3));

  // How many queues should I maintain?
  SDL_AsyncIOQueue * p_my_io_queue = SDL_CreateAsyncIOQueue();

  // This call is not asynchronous; it will open the file before returning, under the assumption
  // that doing so is generally a fast operation. Future reads and writes to the opened file will
  // be async, however.
  SDL_AsyncIO * p_new_async_io = SDL_AsyncIOFromFile( fpath_stone, "r" );

  // TODO load the header first and/or cook the asset into a mini-header format
  auto file_size = SDL_GetAsyncIOSize( p_new_async_io );
  if ( file_size < 0 ) {
    fprintf( stderr, "[Carbonara] SDL_GetAsyncIOSize failed! reason:'%s'\n", SDL_GetError() );
    goto done_with_stone;
  }

  assume( file_size == dds_header_size + len_mipmapped_2048 );
  asset_buf1 = cara_malloc(
      &tmp_ws.malloc_helper, CACHELINE_LEN, len_mipmapped_2048 + dds_header_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf1,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
    goto done_with_stone;
  }

do_it_synchronoously_for_now:
  SDL_AsyncIOOutcome async_io_outcome;
  while ( !SDL_GetAsyncIOResult( p_my_io_queue, &async_io_outcome ) ) {
    // One millisecond
    hold_up_thread( 1000 * 1000 );
  }

  switch ( (uintptr_t)async_io_outcome.userdata ) {
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
      while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
        hold_up_thread( 1000 * 100 );
      }
      goto do_it_synchronoously_for_now;
    case 555:
      if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
        fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
            async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
      }
      break;
    default:
      goto do_it_synchronoously_for_now;
  }

  uint32_t * p_dds_head = (void*)asset_buf1;
  if ( *p_dds_head != magic_dw ) {
    fprintf( stderr, "[Carbonara-Debug] DDS header magic value is wrong! "
        "%#010x instead of %#010x\n", *p_dds_head, magic_dw );
  }
  else {
    cara_vk.alb_stone = asset_buf1;
    cara_vk.asset_size = file_size;
  }

done_with_stone:
  // Stone done; Now round 2: sand.
  //

  p_new_async_io = SDL_AsyncIOFromFile( fpath_sand, "r" );
  file_size = SDL_GetAsyncIOSize( p_new_async_io );
  if ( file_size < 0 ) {
    fprintf( stderr, "[Carbonara] SDL_GetAsyncIOSize failed! reason:'%s'\n", SDL_GetError() );
    goto done_with_stone;
  }

  asset_buf2 = cara_malloc(
      &tmp_ws.malloc_helper, CACHELINE_LEN, len_mipmapped_2048 + dds_header_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf2,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
    goto done_with_sand;
  }

do_it_synchronoously_for_now2:
  while ( !SDL_GetAsyncIOResult( p_my_io_queue, &async_io_outcome ) ) {
    // One millisecond
    hold_up_thread( 1000 * 1000 );
  }

  switch ( (uintptr_t)async_io_outcome.userdata ) {
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
      while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
        hold_up_thread( 1000 * 100 );
      }
      goto do_it_synchronoously_for_now2;
    case 555:
      if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
        fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
            async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
      }
      break;
    default:
      goto do_it_synchronoously_for_now2;
  }

  uint32_t * p_dds_head2 = (void*)asset_buf2;
  if ( *p_dds_head2 != magic_dw ) {
    fprintf( stderr, "[Carbonara-Debug] DDS header magic value is wrong! "
        "%#010x instead of %#010x\n", *p_dds_head2, magic_dw );
  }
  else {
    cara_vk.alb_sand = asset_buf2;
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
  }
  else {
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

  //
  // Load SPIR-V codes
  //

  // simple_test.hlsl.vert.spv
  p_new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/simple_test.hlsl.vert.spv", "r" );
  file_size = SDL_GetAsyncIOSize( p_new_async_io );
  asset_buf3 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf3,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
  }
  if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 33 );
  }  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
  else {
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    }
    else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 555 );
    cara_vk.spv_simple_vert = asset_buf3;
    cara_vk.len_simple_vert = file_size;
  }

  // simple_test.hlsl.frag.spv
  p_new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/simple_test.hlsl.frag.spv", "r" );
  file_size = SDL_GetAsyncIOSize( p_new_async_io );
  asset_buf4 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf4,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
  }
  if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 33 );
  }  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
  else {
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    } else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 555 );
    cara_vk.spv_simple_frag = asset_buf4;
    cara_vk.len_simple_frag = file_size;
  }

  // depth_only_prepass.hlsl.vert.spv
  p_new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/depth_only_prepass.hlsl.vert.spv", "r" );
  file_size = SDL_GetAsyncIOSize( p_new_async_io );
  asset_buf5 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf5,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
  }
  if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 33 );
  }  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
  else {
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    }
    else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 555 );
    cara_vk.spv_z_only = asset_buf5;
    cara_vk.len_z_only = file_size;
  }

  // depth_only_prepass.hlsl.frag.spv
  p_new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/depth_only_prepass.hlsl.frag.spv", "r" );
  file_size = SDL_GetAsyncIOSize( p_new_async_io );
  asset_buf6 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf6,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
  }
  if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 33 );
  }  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
  else {
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    }
    else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 555 );
    cara_vk.spv_z_only_frag = asset_buf6;
    cara_vk.len_z_only_frag = file_size;
  }

  // copy_to_swapchain_img.hlsl.vert.spv
  p_new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/copy_to_swapchain_img.hlsl.vert.spv", "r" );
  file_size = SDL_GetAsyncIOSize( p_new_async_io );
  asset_buf7 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf7,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
  }
  if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 33 );
  }  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
  else {
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    }
    else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 555 );
    cara_vk.spv_swapchain_vert = asset_buf7;
    cara_vk.len_swapchain_vert = file_size;
  }

  // copy_to_swapchain_img.hlsl.frag.spv
  p_new_async_io = SDL_AsyncIOFromFile(
      "/home/dewr/Products/carbonara/spirv/copy_to_swapchain_img.hlsl.frag.spv", "r" );
  file_size = SDL_GetAsyncIOSize( p_new_async_io );
  asset_buf8 = cara_malloc( &tmp_ws.malloc_helper, CACHELINE_LEN, file_size );
  if ( !SDL_ReadAsyncIO(
        p_new_async_io,
        asset_buf8,
        0,
        UINT64_C(0) + file_size,
        p_my_io_queue,
        ui2ptr(33)) ) {
    fprintf( stderr, "[Carbonara] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
  }
  if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
    fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a read request failed!\n" );
  }
  if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
    fprintf( stderr, "[Carbonara] AsyncIO task was not done! result=%s\n",
        async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    if ( async_io_outcome.result == SDL_ASYNCIO_FAILURE ) {
      fprintf( stderr, "[Carbonara] The reason behind the failure:'%s'\n", SDL_GetError() );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 33 );
  }  // if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE )
  else {
    fprintf( stdout, "[Carbonara-Debug] requested:%lu, transferred:%lu, offset:%lu\n",
        async_io_outcome.bytes_requested,
        async_io_outcome.bytes_transferred,
        async_io_outcome.offset );
    while ( !SDL_CloseAsyncIO(p_new_async_io, false, p_my_io_queue, ui2ptr(555)) ) {
      hold_up_thread( 1000 * 100 );
    }
    if ( !SDL_WaitAsyncIOResult(p_my_io_queue, &async_io_outcome, 1000*10) ) {
      fprintf( stderr, "[Carbonara] SDL_WaitAsyncIOResult after a close request failed!\n" );
    }
    else if ( async_io_outcome.result != SDL_ASYNCIO_COMPLETE ) {
      fprintf( stdout, "[Carbonara-Debug] closed? result=%s\n",
          async_io_outcome.result == SDL_ASYNCIO_CANCELED ? "cancelled" : "failure" );
    }
    assume( (uintptr_t)async_io_outcome.userdata == 555 );
    cara_vk.spv_swapchain_frag = asset_buf8;
    cara_vk.len_swapchain_frag = file_size;
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
    SPALL_BEGIN("spawn_a_worker");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    err = create_thread(
        .thread = &threads[proc_idx],
        .func = launch_worker,
        .arg = (void*)proc_idx,  // NOLINT(performance-no-int-to-ptr)
        .priority = 2,
        );
#pragma GCC diagnostic pop
    SPALL_END();  // spawn a worker

    if ( err ) {
      switch ( err ) {
        case emsg_out_of_resource:
          fprintf( stderr, "[Carbonara] Error in worker creation: %s\n", arr_emsg_txt[err] );
          return 4;
        case emsg_invalid_request:
          fprintf( stderr, "[Carbonara] Error in worker creation: %s\n", arr_emsg_txt[err] );
          return 4;
        case emsg_permission_denied:
          fprintf( stderr, "[Carbonara] Error in worker creation: %s\n", arr_emsg_txt[err] );
          return 2;
        default:
          fprintf( stderr, "[Carbonara] Error in worker creation: Unknown error code %u\n", err );
          return 4;
      }
    }
  }


  return 0;
}


//-------------------------------------------------------------------------------------------------
void * launch_worker( void * arg )
{
  const uint_fast8_t proc_idx = (uintptr_t)arg;
  struct ms_worker_info * worker = &cara_single.proc_map->processors[proc_idx];

  //struct cara_malloc_helper_dynamic_bound * malloc_helper_dynamic_bound;
  static thread_local struct cara_worker_state worker_state;
  struct cara_malloc_helper * malloc_helper;
  malloc_helper = &worker_state.malloc_helper;
  malloc_helper->worker_idx = worker->idx_self;
  malloc_helper->node_recently_used = cara_single.mem_arena->head;

  vk_alloc_callbacks.pfnAllocation = cara_malloc_vk;
  vk_alloc_callbacks.pfnFree = cara_free_vk;
  vk_alloc_callbacks.pfnInternalAllocation = nullptr;
  vk_alloc_callbacks.pfnInternalFree = nullptr;
  vk_alloc_callbacks.pfnReallocation = cara_realloc_vk;
  vk_alloc_callbacks.pUserData = (void*)malloc_helper;

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
  memset( spall_buffer.data, 0, spall_buffer.length );

  if ( !spall_buffer_init(&spall_ctx, &spall_buffer) ) {
    fprintf( stderr, "[Worker] failed to init the spall buffer.\n" );
  }

  p_spall_buf = &spall_buffer;

  SPALL_BEGIN("worker_loop-init");
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
    //worker->user_data = (void*)wtime_usec;
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

  worker_state.current_batch = nullptr;
  worker_state.worker_idx = worker->idx_self;
  
  SPALL_END();  // worker_loop-init

  // The loop.
  if ( worker->deployment_flags & worker_deployment_flag_foreman ) {
    foreman_loop( &worker_state );
  }
  else {
    worker_loop( &worker_state );
  }

  return 0;
}


//-------------------------------------------------------------------------------------------------
__attribute__((noreturn))
static void worker_loop( struct cara_worker_state * p_ws )
{
  struct ms_worker_info * const worker =
    &cara_single.proc_map->processors[p_ws->worker_idx];

  // pc_std is not read by line of code—It will probably be removed by the optimizer.
  // It can be used for leap-second adding process if needed.
  [[maybe_unused]] uint64_t std_pc = 0;
  //uint64_t pc_accumultor = 0;


  //uint64_t cur_time_in_5us_general = 0;
  while(-7) {
    const uint64_t starting_pc = get_performance_counter();
    SPALL_BEGIN("worker_loop");

    // This worker is not a foreman.
    //cur_time_in_5us_general = atomic_load_explicit(
    //    &cara_single.cur_time_in_5us_general, memory_order_consume );

    // Look for a job.
    p_ws->current_batch = cara_get_job( worker );

    if ( p_ws->current_batch ) {
      for ( uint_fast8_t job_idx = 0;
          job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
          job_idx++ ) {

        if ( p_ws->current_batch->job_func_idx[job_idx] ) {
#ifdef DEBUG_MODE  //?!
          fprintf( stdout, "[Worker] (%u) I'm on it! job_func_idx=%u\n",
              p_ws->worker_idx,
              p_ws->current_batch->job_func_idx[job_idx] );
#endif
          known_jobs[p_ws->current_batch->job_func_idx[job_idx]]( p_ws );
#ifdef DEBUG_MODE  //?!
          fprintf( stdout, "[Worker] (%u) done. job_func_idx=%u\n",
              p_ws->worker_idx,
              p_ws->current_batch->job_func_idx[job_idx] );
#endif
          //typeof(worker_state->current_batch->progress) cur_progress = worker_state->current_batch->progress +
          //  UINT8_MAX/CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
          //if ( cur_progress <= 1 ) {
          //  cur_progress = UINT8_MAX;
          //}
          //atomic_store_explicit(&worker_state->current_batch->progress, cur_progress, memory_order_release);
        }  // if ( p_ws->current_batch->job_func_idx[job_idx] )

      }  // for ( uint_fast8_t job_idx = 0;
         //     job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64; job_idx++ )

      // Mark empty and unoccupied in the job batch descriptor
      atomic_store_explicit( &p_ws->current_batch->progress,
          UINT8_MAX, memory_order_release );
      atomic_fetch_sub_explicit( &cara_single.nr_job_batch_descriptors_taken,
          1, memory_order_release );

    }  // if ( worker_state->current_batch )
    else {
      // We're jobless.
      SPALL_BEGIN("hold_up_thread");
      if ( worker->deployment_flags & worker_deployment_flag_yielder ) {
        // Take a break for 1280 usec.
        //hold_up_thread( 1000*1280 );
        delay_precise( starting_pc + cara_single.nr_pc_in_5us * 256 );
        // TODO Use MONITORX/UMONITOR instead when the instruction is supported
        // C0.2 improves performance of the other SMT thread(s) on the same core.
        //  MWAITX's C1 state might offer a similar benefit too.
      }
      else {
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
static void foreman_loop( struct cara_worker_state * p_ws )
{
  struct ms_worker_info * const worker =
    &cara_single.proc_map->processors[p_ws->worker_idx];
  struct cara_malloc_helper * const alloc_helper = &p_ws->malloc_helper;

  // pc_std is not read by line of code—It will probably be removed by the optimizer.
  // It can be used for leap-second adding process if needed.
  [[maybe_unused]] uint64_t std_pc = 0;
  uint64_t pc_accumultor = 0;
  uint64_t prev_pc;  // An accumulator and the previous timestamp

  // Prepare a stack of functions to call at the end
  // FIXME Just 4 functions?
  constexpr uint_fast8_t stack_len =
    sizeof(struct func_call_stack_element) * 4 + sizeof(struct infos_stack);
  struct infos_stack * const quit_stack = infos_stack_create(
      cara_malloc( alloc_helper, alignof(max_align_t), stack_len ),
      stack_len );
  cara_single.foremans_quit_stack = quit_stack;

  prev_pc = get_performance_counter();


  uint64_t cur_time_in_5us_general = 0;
  uint64_t prev_time_in_5us_render = 0;
  while(-7) {
    const uint64_t starting_pc = get_performance_counter();

    SPALL_BEGIN("foreman_loop");

    const uint64_t new_pc = starting_pc;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
    std_pc += new_pc - prev_pc;
    pc_accumultor += new_pc - prev_pc;
    prev_pc = new_pc;
    if ( pc_accumultor >= cara_single.nr_pc_in_5us ) {
#pragma GCC diagnostic pop
      const uint64_t nr_5us = div_via_mul_64( &cara_single.magic_divider, pc_accumultor );
      pc_accumultor -= cara_single.nr_pc_in_5us * nr_5us;
      cur_time_in_5us_general += nr_5us;

      // XXX Do I really need to share this with other processing units?
      //atomic_store_explicit( &cara_single.cur_time_in_5us_general, cur_time_in_5us_general, memory_order_release );
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
          // memset( batch->job_func_args, 9, sizeof(batch->job_func_args) );
          atomic_store_explicit( &batch->progress, 9, memory_order_release );
          atomic_store_explicit( &batch->preferred_core, ~UINT64_C(0), memory_order_release );
        }

      }  // if ( keys[cur_idx] <= cur_time_in_5us_general )

      //min_idx_untouched = cur_idx + 1;

      if ( cur_height == 0 )
        break;

      cur_idx = infos_bst_tr_down_left( cur_idx, cur_height );
      cur_height--;
    }

    infos_insert_sort_bst( cara_single.tree_of_times_to_act );



    bool take_a_break = true;


    // Look for a job to get done.
    p_ws->current_batch = cara_get_job( worker );

    if ( p_ws->current_batch ) {
      take_a_break = false;
      for ( uint_fast8_t job_idx = 0;
          job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
          job_idx++ ) {

        if ( p_ws->current_batch->job_func_idx[job_idx] ) {
#ifdef DEBUG_MODE
          fprintf( stdout, "[Worker] (%u) I'm on it! job_func_idx=%u\n",
              p_ws->worker_idx,
              p_ws->current_batch->job_func_idx[job_idx] );
#endif
          known_jobs[p_ws->current_batch->job_func_idx[job_idx]]( p_ws );
#ifdef DEBUG_MODE
          fprintf( stdout, "[Worker] (%u) done. job_func_idx=%u\n",
              p_ws->worker_idx,
              p_ws->current_batch->job_func_idx[job_idx] );
#endif
          //typeof(p_ws->current_batch->progress) cur_progress = p_ws->current_batch->progress +
          //  UINT8_MAX/CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
          //if ( cur_progress <= 1 ) {
          //  cur_progress = UINT8_MAX;
          //}
          //atomic_store_explicit( &p_ws->current_batch->progress,
          //    cur_progress, memory_order_release );
        }  // if ( p_ws->current_batch->job_func_idx[job_idx] )

      }  // for ( uint_fast8_t job_idx = 0;
         //     job_idx < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64; job_idx++ )

      // Mark empty and unoccupied in the job batch descriptor
      atomic_store_explicit( &p_ws->current_batch->progress,
          UINT8_MAX, memory_order_release );
      atomic_fetch_sub_explicit( &cara_single.nr_job_batch_descriptors_taken,
          1, memory_order_release );
    }


    if ( atomic_load_explicit( &cara_single.render_now, memory_order_acquire ) == 1 ) {
      auto delta_in_5us = cur_time_in_5us_general - prev_time_in_5us_render;
      typeof(delta_in_5us) refresh_rate = 60;  // My system happens to be refreshing @ 59.9 Hz
      // FIXME It's just a quick hack, try to find an optimal solution for render pacing
      if ( delta_in_5us > 1000 * 1000 / 5 / refresh_rate - 64 ) {
        atomic_store_explicit( &cara_single.render_now, 0, memory_order_release );
        take_a_break = false;
        prev_time_in_5us_render = cur_time_in_5us_general;
        //atomic_store_explicit( &cara_single.cur_time_in_5us_for_render,
        //    cur_time_in_5us_general, memory_order_release );
        jf_render_frame( cur_time_in_5us_general );
      }
    }


    if ( take_a_break ) {

      SPALL_BEGIN("hold_up_thread");

      if ( worker->deployment_flags & worker_deployment_flag_yielder ) {
        // Take a break for 1280 usec.
        //hold_up_thread( 1000*1280 );
        delay_precise( starting_pc + cara_single.nr_pc_in_5us * 256 );
        // TODO Use MONITORX/UMONITOR instead when the instruction is supported
        // C0.2 improves performance of the other SMT thread(s) on the same core.
        //  MWAITX's C1 state might offer a similar benefit too.
      }
      else {
        // Take a break for about 640 usec.
        //hold_up_thread( 1000*640 );
        delay_precise( starting_pc + cara_single.nr_pc_in_5us * 128 );
        // TODO Use MONITORX/UMONITOR instead when the instruction is supported
        // C0.2 improves performance of the other SMT thread(s) on the same core.
        //  MWAITX's C1 state might offer a similar benefit too.
      }

      SPALL_END();  // hold_up_thread
    }


    SPALL_END();  // foreman_loop
  }

}


//-------------------------------------------------------------------------------------------------
__attribute__((noreturn))
void jf_start_kill_chain( struct cara_worker_state * p_ws )
{
  // Undo the job-take immediately.
  atomic_fetch_sub_explicit( &cara_single.nr_job_batch_descriptors_taken,
      1, memory_order_release );
  atomic_store_explicit( &p_ws->current_batch->progress, 0, memory_order_release );

#ifdef DEBUG_MODE
  fprintf( stdout, "[Worker] (%u) Following a kill-chain...\n", p_ws->worker_idx );
#endif

  // Every worker quits quickly except the foreman.
  if ( !(cara_single.proc_map->processors[p_ws->worker_idx].deployment_flags &
        worker_deployment_flag_foreman) ) {

#ifdef ENABLE_PROFILING
    // Remember to quit or flush your buffers before you exit!
    // If you don't, you might not get the last few events waiting to be written
    spall_buffer_quit( &spall_ctx, p_spall_buf );

    // Freeing our buffer's memory now that spall_buffer_quit has flushed it for us.
    ma_free( cara_single.mem_arena, p_ws->worker_idx, p_spall_buf->data );
#endif

    atomic_fetch_add_explicit( &cara_single.nr_workers_resigned,
        1, memory_order_release );

#ifdef DEBUG_MODE
    fprintf( stdout, "[Worker] (%u) resigned.\n", p_ws->worker_idx );
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
  while ( (p_func_call_stack_element = infos_stack_pop(cara_single.foremans_quit_stack,
          sizeof(struct func_call_stack_element))) ) {
    p_func_call_stack_element->func ( p_func_call_stack_element->arg );
  }

  SPALL_END();  // reap_what_we_have_sown
  SPALL_END();  // foreman's_last_job

#ifdef ENABLE_PROFILING
  // Remember to quit or flush your buffers before you exit!
  // If you don't, you might not get the last few events waiting to be written
  spall_buffer_quit( &spall_ctx, p_spall_buf );

  // Freeing our buffer's memory now that spall_buffer_quit has flushed it for us.
  ma_free( cara_single.mem_arena, p_ws->worker_idx, p_spall_buf->data );
#endif

  atomic_fetch_or_explicit( &cara_single.sign_to_end, 1, memory_order_release );

  pthread_exit(0);
}


//-------------------------------------------------------------------------------------------------
//static enum err_msg verbosee_integrity_check_bc7_2048( const char * data )
//{
//  TCTex_Texture tc_texture;
//  if ( !tctex_load_mem(&tc_texture, data, cara_vk.asset_size) ) {
//    fprintf( stderr, "[Carbonara-Debug] tctex_load_mem failed on the asset! (%s)\n",
//        tc_texture.errmsg );
//    return emsg_corrupted_data_detected;
//  }
//  else {
//    const TCTex_DDS_HEADER* header = (void*)data+4;
//    fprintf( stdout, "[Carbonara-Debug] ""dimension=%u size=%u*%u nmiplevels=%u "
//        "offset0=%u header.dwPitchOrLinearSize=%u\n",
//        tc_texture.dimension,
//        tc_texture.size.x,
//        tc_texture.size.y,
//        tc_texture.nmiplevels,
//        tc_texture.offset0,
//        header->dwPitchOrLinearSize
//        );
//    if ( tc_texture.iformat != TCTEX_IFORMAT_COMPRESSED_BC7_UNORM ) {
//      fprintf( stderr, "[Carbonara-Debug] The format was not BC7_UNORM! (%u)\n",
//          tc_texture.iformat );
//      return emsg_ext_not_supported;
//    }
//  }
//  constexpr unsigned int max_nr_miplevels = 24;  // 1 - 2048
//  TCTex_MipMapInfo arr_mmi[max_nr_miplevels];
//  uint32_t nr_textures = tctex_get_mipmaps( &tc_texture, arr_mmi, max_nr_miplevels, 0 );
//  fprintf( stdout, "[Carbonara-Debug] nr_textures = %u\n", nr_textures );
//
//  return 0;
//}


//-------------------------------------------------------------------------------------------------
void jf_kickstart( [[maybe_unused]] struct cara_worker_state * p_ws )
{
  enum err_msg err = init_renderer( p_ws );
  if ( err ) {
    goto stop_everything_after_failure;
  }
  union extent_2d new_size = {
    .parts.width = initial_win_width,
    .parts.height = initial_win_height,
  };
  atomic_init( &cara_vk.win_size.atomic_whole, new_size.whole );


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
  atomic_store_explicit( &batch->progress, 0, memory_order_release );
  atomic_store_explicit( &batch->preferred_core, UINT8_MAX, memory_order_release );
  */


  atomic_store_explicit( &cara_single.sign_to_start, 1, memory_order_release );


  return;

stop_everything_after_failure:

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
  atomic_store_explicit( &batch->progress, 0, memory_order_release );
  atomic_store_explicit( &batch->preferred_core, UINT8_MAX, memory_order_release );

  return;
}


//-------------------------------------------------------------------------------------------------
static void handle_sigsegv( int sig )
{
  // TODO Produce some dump in /dev/shm/carbonara.dump

  signal( sig, SIG_DFL );
  kill( getpid(), sig );
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
  atomic_store_explicit( &batch->progress, 0, memory_order_release );
  atomic_store_explicit( &batch->preferred_core, UINT8_MAX, memory_order_release );

  return;
}

//-------------------------------------------------------------------------------------------------
static enum err_msg init_renderer( struct cara_worker_state * p_ws )
{
  SPALL_BEGIN("renderer_setup");
  uint32_t heap_idx = UINT_MAX_(heap_idx);
  atomic_init( &cara_vk.frame_id, 0 );
  constexpr uint32_t frame_id = 0;


  //for ( unsigned int fif_idx = 1; fif_idx < cara_max_nr_fif; fif_idx++ ) {
  //  VkCommandBufferBeginInfo cmd_buf_begin_info = {
  //    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  //    .pNext = nullptr,
  //    .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
  //    .pInheritanceInfo = nullptr,
  //  };
  //  VkResult result = vkResetCommandPool(
  //      cara_vk.log_dev,
  //      cara_vk.dummy_pool[fif_idx],
  //      0 );
  //  if( result != VK_SUCCESS ) {
  //    fprintf( stderr, "[Carbonara] Error occurred during command buffer reset.(d) '%s'\n",
  //        string_VkResult(result) );
  //    goto stop_everything_after_failure;
  //  }
  //  result = vkBeginCommandBuffer( cara_vk.dummy_cmdbuf[fif_idx], &cmd_buf_begin_info );
  //  if( result != VK_SUCCESS ) {
  //    fprintf( stderr, "[Carbonara] Could not begin command(transfer) buffer recording operation. "
  //        "'%s'\n", string_VkResult(result) );
  //    goto stop_everything_after_failure;
  //  }
  //  result = vkEndCommandBuffer( cara_vk.dummy_cmdbuf[fif_idx] );
  //  if( result != VK_SUCCESS ) {
  //    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(d) '%s'\n",
  //        string_VkResult(result) );
  //    goto stop_everything_after_failure;
  //  }
  //}

  //// TODO Remove this code!
  //fprintf( stdout, "[Carbonara-Debug] tctex_load_mem for stone...\n" );
  //if ( verbosee_integrity_check_bc7_2048(cara_vk.alb_stone) ) {
  //  fprintf( stderr, "[Carbonara-Debug] integrity check failed for stone.\n" );
  //  goto stop_everything_after_failure;
  //}
  //fprintf( stdout, "[Carbonara-Debug] tctex_load_mem for sand...\n" );
  //if ( verbosee_integrity_check_bc7_2048(cara_vk.alb_sand) ) {
  //  fprintf( stderr, "[Carbonara-Debug] integrity check failed for sand.\n" );
  //  goto stop_everything_after_failure;
  //}

  //struct func_call_stack_element func_call_stack_element;


  if ( ma_create_staging_buf(&cara_vk.vmm, cara_vk.log_dev, 0) ) {
    fprintf( stderr, "[Carbonara-Debug] ma_create_vk_trans_bufs failed..\n" );
    goto stop_everything_after_failure;
  }

  enum err_msg err = ma_prepare_upload_mgr( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc );
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] ma_prepare_upload_mgr failed. (%u)\n", err );
    goto stop_everything_after_failure;
  }

  constexpr unsigned int dds_header_size =
    4 + sizeof(TCTex_DDS_HEADER) + sizeof(TCTex_DDS_HEADER_DXT10);
  constexpr unsigned int nr_miplvls = 12;
  constexpr unsigned int len_mipmapped_2048 = 16 + 16 + (4*4)*4/4 + (8*8)*4/4 + (16*16)*4/4 +
    (32*32)*4/4 + (64*64)*4/4 + (128*128)*4/4 + (256*256)*4/4 + (512*512)*4/4 + (1024*1024)*4/4 + (2048*2048)*4/4;
  unsigned int nr_bytes_to_alloc =
    _align_to_power_of_two( len_mipmapped_2048, cara_vk.vmm.buf_granularity );
  const struct range_u32 rent_for_stone = infos_pm_rent(
      &cara_vk.vmm.uploads, &p_ws->malloc_helper, cara_malloc,
      _align_to_power_of_two(nr_bytes_to_alloc, cara_vk.vmm.len_non_coherent_atom), true );
  const struct range_u32 rent_for_sand = infos_pm_rent(
      &cara_vk.vmm.uploads, &p_ws->malloc_helper, cara_malloc,
      _align_to_power_of_two(nr_bytes_to_alloc, cara_vk.vmm.len_non_coherent_atom), true );
  if (  is_range_u32_invalid(rent_for_stone) || is_range_u32_invalid(rent_for_sand) ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_rent failed.. (errno=%d)\n", errno );
    goto stop_everything_after_failure;
  }

  SPALL_BEGIN("memcpy_2_textures");
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit specifies that the host cache management commands
  // vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges are not needed to manage
  // availability and visibility on the host.
  memcpy( cara_vk.vmm.p_staging_buf + rent_for_stone.start,
      cara_vk.alb_stone + dds_header_size, len_mipmapped_2048 );
  memcpy( cara_vk.vmm.p_staging_buf + rent_for_sand.start,
      cara_vk.alb_sand + dds_header_size, len_mipmapped_2048 );
  SPALL_END();  // memcpy_2_textures

  for ( unsigned int albmap_idx = 0; albmap_idx < 2; albmap_idx++ ) {  // Stone=0 Sand=1
    VkImageCreateInfo img_create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_BC7_UNORM_BLOCK,
      .extent = (VkExtent3D) { .width = 2048, .height = 2048, .depth = 1 },
      .mipLevels = nr_miplvls,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        0,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    SPALL_BEGIN("create_img");
    VkResult result = vkCreateImage( cara_vk.log_dev,
        &img_create_info, &vk_alloc_callbacks, &cara_vk.albmaps[albmap_idx]
        );
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara-Debug] vkCreateImage an 8-pack failed.. (result=%s)\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
    SPALL_END();

    // Obtain the requirements
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements( cara_vk.log_dev, cara_vk.albmaps[albmap_idx], &req );

    // Choose the best memory type
    VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
    preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    required_props = 0;
    tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    uint32_t mtype_idx = ma_pick_best_mtype(
        &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
    heap_idx = cara_vk.vmm.arr_heap_indices[mtype_idx];
    // Save the best type for later?
    cara_vk.vmm.arr_mtype_used[vk_buf_type_texarr_2048] = mtype_idx;

    // Bind the image to a vulkan memory object
    struct vk_mbind_info mbind_info;
    err = ma_bind_img_to_malloc( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc, cara_vk.log_dev,
        cara_vk.albmaps[albmap_idx], mtype_idx, req.size, req.alignment, &mbind_info );
    if ( err ) {
      fprintf( stderr, "Error occured in ma_bind_img_to_malloc. (%s)\n", arr_emsg_txt[err] );
      goto stop_everything_after_failure;
    }
    cara_vk.mbi_albmaps[albmap_idx] = mbind_info;
  }


  // Copy the vertex buffer and the index buffer to the staging buffer
  unsigned int vbuf_len =
    _align_to_power_of_two( sizeof(arr_vert_pos), cara_vk.vmm.buf_granularity );
  typeof(vbuf_len) abuf_len =
    _align_to_power_of_two( sizeof(arr_uv_coords), cara_vk.vmm.buf_granularity );
  typeof(vbuf_len) ibuf_len =
    _align_to_power_of_two( sizeof(arr_v_indices), cara_vk.vmm.buf_granularity );
  const struct range_u32 rent_for_vb = infos_pm_rent(
      &cara_vk.vmm.uploads, &p_ws->malloc_helper, cara_malloc,
      _align_to_power_of_two(vbuf_len, cara_vk.vmm.len_non_coherent_atom), true );
  const struct range_u32 rent_for_ab = infos_pm_rent(
      &cara_vk.vmm.uploads, &p_ws->malloc_helper, cara_malloc,
      _align_to_power_of_two(abuf_len, cara_vk.vmm.len_non_coherent_atom), true );
  const struct range_u32 rent_for_ib = infos_pm_rent(
      &cara_vk.vmm.uploads, &p_ws->malloc_helper, cara_malloc,
      _align_to_power_of_two(ibuf_len, cara_vk.vmm.len_non_coherent_atom), true );
  if (  is_range_u32_invalid(rent_for_vb) || is_range_u32_invalid(rent_for_ib) ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_rent failed.. (errno=%d)\n", errno );
    goto stop_everything_after_failure;
  }
  memcpy( cara_vk.vmm.p_staging_buf + rent_for_vb.start, arr_vert_pos, sizeof(arr_vert_pos) );
  memcpy( cara_vk.vmm.p_staging_buf + rent_for_ab.start, arr_uv_coords, sizeof(arr_uv_coords) );
  memcpy( cara_vk.vmm.p_staging_buf + rent_for_ib.start, arr_v_indices, sizeof(arr_v_indices) );

  VkBufferCreateInfo buf_create_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = vbuf_len,
    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
  };
  SPALL_BEGIN("vkCreateBuffer_x2");
  vkCreateBuffer( cara_vk.log_dev, &buf_create_info, &vk_alloc_callbacks, &cara_vk.vert_buf );
  buf_create_info.size = abuf_len;
  buf_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  vkCreateBuffer( cara_vk.log_dev, &buf_create_info, &vk_alloc_callbacks, &cara_vk.attr_buf );
  buf_create_info.size = ibuf_len;
  buf_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  vkCreateBuffer( cara_vk.log_dev, &buf_create_info, &vk_alloc_callbacks, &cara_vk.ind_buf );
  SPALL_END();

  // Bind the vertex buffer to a malloc
  VkMemoryRequirements req;
  VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
  struct vk_mbind_info mbind_info;
  vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.vert_buf, &req );
  preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  required_props = 0;
  tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  uint32_t mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
  heap_idx = cara_vk.vmm.arr_heap_indices[mtype_idx];
  //cara_vk.vmm.arr_mtype_used[vk_buf_type_vert_buf] = mtype_idx;

  // Bind the buffer to a vulkan memory object
  err = ma_bind_buf_to_malloc( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc,
      cara_vk.log_dev, cara_vk.vert_buf, mtype_idx, req.size, req.alignment, &mbind_info );
  if ( err ) {
    fprintf( stderr, "Error occured in ma_bind_buf_to_malloc. (%s)\n", arr_emsg_txt[err] );
    goto stop_everything_after_failure;
  }
  cara_vk.mbi_vbuf = mbind_info;

  // Bind the attributes buffer to a malloc
  vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.attr_buf, &req );
  preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  required_props = 0;
  tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
  heap_idx = cara_vk.vmm.arr_heap_indices[mtype_idx];
  //cara_vk.vmm.arr_mtype_used[vk_buf_type_ind_buf] = mtype_idx;

  // Bind the buffer to a vulkan memory object
  err = ma_bind_buf_to_malloc( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc,
      cara_vk.log_dev, cara_vk.attr_buf, mtype_idx, req.size, req.alignment, &mbind_info );
  if ( err ) {
    fprintf( stderr, "Error occured in ma_bind_buf_to_malloc. (%s)\n", arr_emsg_txt[err] );
    goto stop_everything_after_failure;
  }
  cara_vk.mbi_abuf = mbind_info;

  // Bind the index buffer to a malloc
  vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.ind_buf, &req );
  preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  required_props = 0;
  tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
  heap_idx = cara_vk.vmm.arr_heap_indices[mtype_idx];
  //cara_vk.vmm.arr_mtype_used[vk_buf_type_ind_buf] = mtype_idx;

  // Bind the buffer to a vulkan memory object
  err = ma_bind_buf_to_malloc( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc,
      cara_vk.log_dev, cara_vk.ind_buf, mtype_idx, req.size, req.alignment, &mbind_info );
  if ( err ) {
    fprintf( stderr, "Error occured in ma_bind_buf_to_malloc. (%s)\n", arr_emsg_txt[err] );
    goto stop_everything_after_failure;
  }
  cara_vk.mbi_ibuf = mbind_info;



  VkCommandBufferBeginInfo cmd_buf_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |  // Each recording of the command buffer will
                                                     // only be submitted once, and the command
                                                     // buffer will be reset and recorded again
                                                     // between each submission.
      0,
    .pInheritanceInfo = nullptr,
               // a pointer to a VkCommandBufferInheritanceInfo structure, used if `commandBuffer`
               // is a secondary command buffer. If this is a primary command buffer, then this
               // value is ignored.
  };


  // Begin recording on the primary buffer
  VkCommandBuffer trans_cmdbuf =
    cara_vk.arr_trans_cmdbuf_sets[nr_al_cmd_pool_sets + frame_id % cara_max_nr_fif].
      primary_cmd_buf;
#ifdef DEBUG_MODE
  fprintf( stderr, "[Vulkan-Debug] TP cmd_buf=%#018lx\n", (uintptr_t)trans_cmdbuf );
#endif
  SPALL_BEGIN("begin_trans_buf");
  // TODO synchronization needed
  VkResult result = vkBeginCommandBuffer( trans_cmdbuf, &cmd_buf_begin_info );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command(transfer) buffer recording operation. "
        "'%s'\n", string_VkResult(result) );
    goto stop_everything_after_failure;
  }



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
  //  buf_bar[layer_idx].size = nr_bytes_to_alloc;
  //}

  // Barriers for vert_buf and ind_buf
  VkBufferMemoryBarrier2KHR buf_bar[3];
  buf_bar[0].buffer = cara_vk.vert_buf;
  buf_bar[0].size = vbuf_len;
  buf_bar[1].buffer = cara_vk.ind_buf;
  buf_bar[1].size = ibuf_len;
  buf_bar[2].buffer = cara_vk.attr_buf;
  buf_bar[2].size = abuf_len;
  for ( unsigned int i = 0; i < _arr_elem_cnt(buf_bar); i++ ) {
    buf_bar[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
    buf_bar[i].pNext = nullptr;
    buf_bar[i].srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    buf_bar[i].dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
    buf_bar[i].srcAccessMask = VK_ACCESS_2_NONE_KHR;
    buf_bar[i].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    buf_bar[i].srcQueueFamilyIndex = cara_vk.transfer_q_fam_idx;
    buf_bar[i].dstQueueFamilyIndex = cara_vk.transfer_q_fam_idx;
    buf_bar[i].offset = 0;
  }
  VkImageMemoryBarrier2KHR arr_img_bars[2];
  for ( unsigned int albmap_idx = 0; albmap_idx < 2; albmap_idx++ ) {
    arr_img_bars[albmap_idx].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
    arr_img_bars[albmap_idx].pNext = nullptr;
    arr_img_bars[albmap_idx].srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    arr_img_bars[albmap_idx].dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
    arr_img_bars[albmap_idx].srcAccessMask = VK_ACCESS_2_NONE_KHR;
    arr_img_bars[albmap_idx].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    arr_img_bars[albmap_idx].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    arr_img_bars[albmap_idx].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    arr_img_bars[albmap_idx].srcQueueFamilyIndex = cara_vk.transfer_q_fam_idx;
    arr_img_bars[albmap_idx].dstQueueFamilyIndex = cara_vk.transfer_q_fam_idx;
    // If the values of srcQueueFamilyIndex and dstQueueFamilyIndex are equal, no ownership
    //   transfer is performed, and the barrier operates as if they were both set to
    //   VK_QUEUE_FAMILY_IGNORED.
    arr_img_bars[albmap_idx].image = cara_vk.albmaps[albmap_idx];
    arr_img_bars[albmap_idx].subresourceRange = (VkImageSubresourceRange) {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = nr_miplvls,
      .baseArrayLayer = 0,
      .layerCount = 1,
    };
  }

  VkDependencyInfoKHR dependency_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
    .pNext = nullptr,
    .dependencyFlags = 0,
    .memoryBarrierCount = 0,
    .pMemoryBarriers = nullptr,
    .bufferMemoryBarrierCount = _arr_elem_cnt( buf_bar ),
    .pBufferMemoryBarriers = buf_bar,
    .imageMemoryBarrierCount = _arr_elem_cnt(arr_img_bars),
    .pImageMemoryBarriers = arr_img_bars,
  };

  SPALL_BEGIN("barrier_0");
  vkCmdPipelineBarrier2KHR( trans_cmdbuf, &dependency_info );
  SPALL_END();


  // Copy from the staging buffer to the device-local buffer
  VkBufferCopy buf_region = {
    .srcOffset = rent_for_vb.start,
    .dstOffset = 0,
    .size = vbuf_len,
  };
  SPALL_BEGIN("vkCmdCopyBuffer_x2");
  vkCmdCopyBuffer( trans_cmdbuf, cara_vk.vmm.staging_buf, cara_vk.vert_buf, 1, &buf_region );
  buf_region.srcOffset = rent_for_ab.start;
  buf_region.size = abuf_len;
  vkCmdCopyBuffer( trans_cmdbuf, cara_vk.vmm.staging_buf, cara_vk.attr_buf, 1, &buf_region );
  buf_region.srcOffset = rent_for_ib.start;
  buf_region.size = ibuf_len;
  vkCmdCopyBuffer( trans_cmdbuf, cara_vk.vmm.staging_buf, cara_vk.ind_buf, 1, &buf_region );
  SPALL_END();

  // Copy the texture data to the device-local buffer
  VkBufferImageCopy arr_regions[nr_miplvls];
  auto base_ofs = rent_for_stone.start;
  for ( unsigned int mip_lv = 0, width = 2048, offset = 0;
      mip_lv < nr_miplvls; mip_lv++, width/=2 ) {
    arr_regions[mip_lv].bufferOffset = base_ofs + offset;
    arr_regions[mip_lv].bufferRowLength = 0;
    arr_regions[mip_lv].bufferImageHeight = 0;
    arr_regions[mip_lv].imageSubresource = (VkImageSubresourceLayers) {
      .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel=mip_lv,
      .baseArrayLayer=0,
      .layerCount=1
    };
    arr_regions[mip_lv].imageOffset = (VkOffset3D) {0, 0, 0};
    arr_regions[mip_lv].imageExtent = (VkExtent3D) {width, width, 1};
    assume( _is_power_of_two(width) );
    switch ( width ) {
      case 4:
        [[fallthrough]];  // 4*4 = 16 which happens to be the texel block size of BC7
      case 2:
        [[fallthrough]];
      case 1:
        offset += 16;     // The minimum size of the map is 16 bytes probably because of the
        break;            //  texel block size of BC7
      default:
        offset += ( width * width ) * 4 / 4;  // 1 byte stores the information of 1 pixel in BC7
    }
  }
  SPALL_BEGIN("vkCmdCopyBufferToImage");
  vkCmdCopyBufferToImage(
      trans_cmdbuf,
      cara_vk.vmm.staging_buf,
      cara_vk.albmaps[0],
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      _arr_elem_cnt( arr_regions ),
      arr_regions
      );
  SPALL_END();

  base_ofs = rent_for_sand.start;
  for ( unsigned int mip_lv = 0, width = 2048, offset = 0;
      mip_lv < nr_miplvls; mip_lv++, width/=2 ) {
    arr_regions[mip_lv].bufferOffset = base_ofs + offset;
    assume( _is_power_of_two(width) );
    switch ( width ) {
      case 4:
        [[fallthrough]];  // 4*4 = 16 which happens to be the texel block size of BC7
      case 2:
        [[fallthrough]];
      case 1:
        offset += 16;     // The minimum size of the map is 16 bytes probably because of the
        break;            //  texel block size of BC7
      default:
        offset += ( width * width ) * 4 / 4;  // 1 byte stores the information of 1 pixel in BC7
    }
  }
  SPALL_BEGIN("vkCmdCopyBufferToImage");
  vkCmdCopyBufferToImage(
      trans_cmdbuf,
      cara_vk.vmm.staging_buf,
      cara_vk.albmaps[1],
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      _arr_elem_cnt( arr_regions ),
      arr_regions
      );
  SPALL_END();



  //
  // Make barriers between transfer and sampling
  //

  //if ( cara_vk.graphics_q == cara_vk.transfer_q ) 
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
  for ( unsigned int i = 0; i < _arr_elem_cnt(buf_bar); i++ ) {
    buf_bar[i].srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    buf_bar[i].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    buf_bar[i].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    buf_bar[i].srcQueueFamilyIndex = cara_vk.transfer_q_fam_idx;
    buf_bar[i].dstQueueFamilyIndex = cara_vk.graphics_q_fam_idx;
  }
  buf_bar[0].dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;
  buf_bar[1].dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT_KHR;
  buf_bar[2].dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;

  for ( unsigned int albmap_idx = 0; albmap_idx < 2; albmap_idx++ ) {
    arr_img_bars[albmap_idx].srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    arr_img_bars[albmap_idx].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    arr_img_bars[albmap_idx].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    arr_img_bars[albmap_idx].dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR;
    arr_img_bars[albmap_idx].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    arr_img_bars[albmap_idx].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    arr_img_bars[albmap_idx].srcQueueFamilyIndex = cara_vk.transfer_q_fam_idx;
    arr_img_bars[albmap_idx].dstQueueFamilyIndex = cara_vk.graphics_q_fam_idx;
    // If the values of srcQueueFamilyIndex and dstQueueFamilyIndex are equal, no ownership
    //   transfer is performed, and the barrier operates as if they were both set to
    //   VK_QUEUE_FAMILY_IGNORED.
  }

  // Prior to the intro. of VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR,
  //  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT is the only valid stage to wait on or wait for these
  //  operations, as the acquire and release operations do not occur in a defined stage.
  if ( cara_vk.transfer_q_fam_idx == cara_vk.graphics_q_fam_idx ) {
    dependency_info.dependencyFlags |=
    // Can't use VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR when the queue
    //  associated with srcQueueFamilyIndex doesn't have access to dstStageMask
      VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR;
    for ( unsigned int albmap_idx = 0; albmap_idx < 2; albmap_idx++ ) {
      arr_img_bars[albmap_idx].srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
      arr_img_bars[albmap_idx].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
    }
    buf_bar[0].srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
    buf_bar[0].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
    buf_bar[1].srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
    buf_bar[1].dstStageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR;
    buf_bar[2].srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
    buf_bar[2].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
  }

  dependency_info.dependencyFlags = 0;
  dependency_info.memoryBarrierCount = 0;
  dependency_info.pMemoryBarriers = nullptr;
  dependency_info.bufferMemoryBarrierCount = _arr_elem_cnt( buf_bar );
  dependency_info.pBufferMemoryBarriers = buf_bar;
  dependency_info.imageMemoryBarrierCount = _arr_elem_cnt( arr_img_bars );
  dependency_info.pImageMemoryBarriers = arr_img_bars;

  SPALL_BEGIN("barrier_1");
  vkCmdPipelineBarrier2KHR( trans_cmdbuf, &dependency_info );
  SPALL_END();

  VkCommandBuffer main_cmd_buf =
    cara_vk.arr_graphics_cmdbuf_sets[frame_id % cara_max_nr_fif].primary_cmd_buf;
  SPALL_BEGIN("vkBeginCommandBuffer");
  // TODO synchronization needed
  result = vkBeginCommandBuffer( main_cmd_buf, &cmd_buf_begin_info );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command(G) buffer recording operation. "
        "'%s'\n", string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  SPALL_BEGIN("barrier?");
  vkCmdPipelineBarrier2KHR( main_cmd_buf, &dependency_info );
  SPALL_END();
  SPALL_BEGIN("vkEndCommandBuffer");
  result = vkEndCommandBuffer( main_cmd_buf );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(G) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }


  // End recording
  SPALL_BEGIN("end_trans_buf");
  result = vkEndCommandBuffer( trans_cmdbuf );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(1t) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  //
  // Prepare to submit the command buffer
  //
  VkSemaphoreSubmitInfoKHR sempho_sgnl_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .semaphore = cara_vk.arr_semphos_tr[frame_id % cara_max_nr_fif],
    .value = csb_data_trans_done + frame_id / cara_max_nr_fif * cara_nr_signs_tr,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR,
    .deviceIndex = 0,
  };
  VkCommandBufferSubmitInfoKHR cmd_buf_submit_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .commandBuffer = trans_cmdbuf,
    .deviceMask = 0,
  };
  VkSubmitInfo2 submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
    .pNext = nullptr,
    .flags = 0,
    .waitSemaphoreInfoCount = 0,
    .pWaitSemaphoreInfos = nullptr,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_buf_submit_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &sempho_sgnl_info,
  };
  SPALL_BEGIN("vkQueueSubmit2KHR");
  result = vkQueueSubmit2KHR( cara_vk.transfer_q, 1, &submit_info, VK_NULL_HANDLE );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  cmd_buf_submit_info.commandBuffer = main_cmd_buf;
  sempho_sgnl_info.semaphore = cara_vk.arr_semphos_gr[frame_id % cara_max_nr_fif];
  sempho_sgnl_info.value = csb_present_swapchain_img +
    frame_id / cara_max_nr_fif * cara_nr_signs_gr;
  sempho_sgnl_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR;
  VkSemaphoreSubmitInfoKHR sempho_wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .semaphore = cara_vk.arr_semphos_tr[frame_id % cara_max_nr_fif],
    .value = csb_data_trans_done + frame_id / cara_max_nr_fif * cara_nr_signs_tr,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR,
    .deviceIndex = 0,
  };
  submit_info.waitSemaphoreInfoCount = 1;
  submit_info.pWaitSemaphoreInfos = &sempho_wait_info;
  SPALL_BEGIN("vkQueueSubmit2KHR");
  result = vkQueueSubmit2KHR( cara_vk.graphics_q, 1, &submit_info, VK_NULL_HANDLE );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }



  // result = vkBeginCommandBuffer( cmd_buf, &cmd_buf_begin_info );
  ////
  //// Make barriers between transfer and sampling
  ////
  //img_bar.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  //img_bar.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  //SPALL_BEGIN("barrier_2");
  //vkCmdPipelineBarrier2KHR( cmd_buf, &dependency_info );
  //SPALL_END();


  // Create a VkImageView for the texture array
  VkImageViewCreateInfo iv_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = VK_FORMAT_BC7_UNORM_BLOCK,
    .components = (VkComponentMapping) {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY  // What about VK_COMPONENT_SWIZZLE_ONE
    },
    .subresourceRange = (VkImageSubresourceRange) {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = nr_miplvls,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  iv_create_info.image = cara_vk.albmaps[0];
  SPALL_BEGIN("vkCreateImageView");
  result = vkCreateImageView( cara_vk.log_dev, &iv_create_info,
      &vk_alloc_callbacks, &cara_vk.albmap_ivs[0]
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkCreateImageView failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  iv_create_info.image = cara_vk.albmaps[1];
  SPALL_BEGIN("vkCreateImageView");
  result = vkCreateImageView( cara_vk.log_dev, &iv_create_info,
      &vk_alloc_callbacks, &cara_vk.albmap_ivs[1]
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkCreateImageView failed.. (result=%s)\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }


  //
  // Prepare a descriptor set layout
  //

  // Prepare a sampler for the image
  //  ```glsl
  //  layout (set=m, binding=n) uniform sampler <variable name>;
  //  ```
  VkSamplerCreateInfo sampler_create_info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .mipLodBias = .0f,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = cara_vk.max_anisotropy,
    .compareEnable = VK_FALSE,
    //.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    .minLod = .0f,
    .maxLod = nr_miplvls - 1,
    .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // Assign a value to prevent segfault
    .unnormalizedCoordinates = VK_FALSE,
  };
  SPALL_BEGIN("vkCreateSampler_i2048");
  result = vkCreateSampler( cara_vk.log_dev, &sampler_create_info,
      &vk_alloc_callbacks, &cara_vk.albmap_sampler
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during sampler creation. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_create_info.anisotropyEnable = VK_FALSE;
  sampler_create_info.maxLod = .0f;
  sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  SPALL_BEGIN("vkCreateSampler_SS");
  result = vkCreateSampler( cara_vk.log_dev, &sampler_create_info,
      &vk_alloc_callbacks, &cara_vk.cbuf_sampler
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during sampler creation. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  //
  // Prepare a sampled image (We have already created a VkImageView)
  //  ```glsl
  //  layout (set=m, binding=n) uniform sampler2D my_texture;
  //  frag_color = texture( my_texture, uv );
  //  ```
  //  ```hlsl
  //  Texture2D my_texture;
  //  SamplerState my_sampler;
  //  my_texture.Sample( my_sampler, uv );
  //  ```
  //

  //
  // Prepare a combined image sampler (We have already created a VkSampler)
  //  To create a variable that represents a combined image sampler inside GLSL shaders, we
  //  need to use a sampler keyword (possibly with a prefix) with an appropriate dimensionality.
  //  Don't confuse samplers and combined image samplers—both use a sampler keyword inside shaders,
  //  but combined image samplers additionally have a dimensionality specified like in the following
  //  example:
  //  ```hlsl
  //  Texture2D render_tgt : register(t0);
  //  SamplerState tex_sampler : register(s0);
  //  [shader("pixel")]
  //  float4 frag_main( VertexShaderOutput input ) : SV_TARGET
  //  {
  //    float4 output = render_tgt.Sample( tex_sampler, input.tex_uv );
  //    return output;
  //  }
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
  VkDescriptorSetLayoutCreateInfo dsl_create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
  };

  // Bindings for cs_draw_opaq_objs
  VkDescriptorSetLayoutBinding dsl_binding_main_render_img = {
    .binding = 0,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    .descriptorCount =
      cara_vk.desc_indexing_props.maxDescriptorSetUpdateAfterBindSampledImages,
    .pImmutableSamplers = nullptr,
  };
  VkDescriptorSetLayoutBinding dsl_binding_main_render_sampler = {
    .binding = 0,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
    .descriptorCount = 1,
    .pImmutableSamplers = &cara_vk.albmap_sampler,
  };

  // Bindings for cs_copy_to_swapchain_img
  VkDescriptorSetLayoutBinding arr_bindings_cp2sc[1];
  for ( unsigned int i = 0; i < _arr_elem_cnt( arr_bindings_cp2sc ); i++ ) {
    arr_bindings_cp2sc[i].binding = i;
    arr_bindings_cp2sc[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    arr_bindings_cp2sc[i].descriptorCount = 1;
    arr_bindings_cp2sc[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    arr_bindings_cp2sc[i].pImmutableSamplers = nullptr;
  }

  dsl_create_info.bindingCount = 0;
  dsl_create_info.pBindings = nullptr;
  SPALL_BEGIN("create_dsls");
  result = vkCreateDescriptorSetLayout( cara_vk.log_dev, &dsl_create_info,
      &vk_alloc_callbacks, &cara_vk.dsl_z_only
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during DSL creation.(Z) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  // We're going to use update-after-bind, so we need to make sure the flag is set correctly in the
  //   set layout.
  // These sets need to be allocated with UPDATE_AFTER_BIND pools later.
  dsl_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
  dsl_create_info.bindingCount = 1;
  dsl_create_info.pBindings = &dsl_binding_main_render_img;
  // Typically, if you're using descriptor indexing, you will want to use all four of these, but all of these are separate feature bits.
  const VkDescriptorBindingFlagsEXT flags =
    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
    VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT;
  // In unextended Vulkan, there is no way to pass down flags to a binding, so we're going to do so
  //   via a pNext.
  // Each pBinding has a corresponding pBindingFlags.
  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    .pNext = nullptr,
    .bindingCount = 1,
    .pBindingFlags = &flags,
  };
  dsl_create_info.pNext = &binding_flags;
  result = vkCreateDescriptorSetLayout( cara_vk.log_dev, &dsl_create_info,
      &vk_alloc_callbacks, &cara_vk.dsl_main_render_img
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during DSL creation.(MI) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  dsl_create_info.flags = 0;
  dsl_create_info.pNext = nullptr;
  dsl_create_info.bindingCount = 1;
  dsl_create_info.pBindings = &dsl_binding_main_render_sampler;
  result = vkCreateDescriptorSetLayout( cara_vk.log_dev, &dsl_create_info,
      &vk_alloc_callbacks, &cara_vk.dsl_main_render_sampler
      );
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during DSL creation.(MS) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  dsl_create_info.bindingCount = _arr_elem_cnt( arr_bindings_cp2sc );
  dsl_create_info.pBindings = arr_bindings_cp2sc;
  result = vkCreateDescriptorSetLayout( cara_vk.log_dev, &dsl_create_info,
      &vk_alloc_callbacks, &cara_vk.dsl_cp2sc
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during DSL creation.(S) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }


  //
  // Create a descriptor pool
  //
  //VkDescriptorPoolSize arr_pool_sizes[] = {
  //  { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },  // The maximal number of combined samplers
  //                                                     //  that can be allocated in the pool.
  //                                                     //  1+2 refers to {tex8_2048, fb_cbuf}
  //};
  VkDescriptorPoolSize arr_pool_sizes[] = {
    {
      .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .descriptorCount = max_nr_img_descriptors_in_set  // The maximal number of images that can be
                                                        //   allocated in the pool.
    },
  };
  VkDescriptorPoolCreateInfo dp_create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
    .maxSets = 1,
    .poolSizeCount = _arr_elem_cnt( arr_pool_sizes ),
    .pPoolSizes = arr_pool_sizes,
  };
  SPALL_BEGIN("vkCreateDescriptorPool");
  result = vkCreateDescriptorPool( cara_vk.log_dev, &dp_create_info,
      &vk_alloc_callbacks, &cara_vk.descriptor_pool_main_img
      //nullptr, &cara_vk.dp_main_render
      );
  SPALL_END();
  dp_create_info.flags = 0;
  // When we have created a pool, we can allocate descriptor sets from it. But we must remember that
  //  we can't do this in multiple threads at the same time.
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during descriptor pool creation.(images) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  // The common case for bindless is to have an array of sampled images, not combined image sampler.
  // It is more efficient to use a single sampler instead, and we can just use a single immutable
  //   sampler for this purpose.
  // Create the sampler, descriptor set layout and allocate an immutable descriptor set.
  arr_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  arr_pool_sizes[0].descriptorCount = 1;
  SPALL_BEGIN("vkCreateDescriptorPool");
  result = vkCreateDescriptorPool( cara_vk.log_dev, &dp_create_info,
      &vk_alloc_callbacks, &cara_vk.descriptor_pool_main_sampler
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during descriptor pool creation.(sampler) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  // The descriptor sets for cbuf
  arr_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  arr_pool_sizes[0].descriptorCount = cara_max_nr_fif;
  dp_create_info.maxSets = cara_max_nr_fif;
  SPALL_BEGIN("vkCreateDescriptorPool");
  result = vkCreateDescriptorPool( cara_vk.log_dev, &dp_create_info,
      &vk_alloc_callbacks, &cara_vk.descriptor_pool_cbuf
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during descriptor pool creation.(sampler) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }


  //
  // Allocate a descriptor set
  //
  VkDescriptorSetAllocateInfo ds_alloc_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = cara_vk.descriptor_pool_main_sampler,
    .descriptorSetCount = 1,
    .pSetLayouts = &cara_vk.dsl_main_render_sampler,
  };
  SPALL_BEGIN("vkAllocateDescriptorSets");
  result = vkAllocateDescriptorSets( cara_vk.log_dev, &ds_alloc_info,
      &cara_vk.ds_main_render_sampler );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during descriptor set allocation. '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }

  ds_alloc_info.pSetLayouts = &cara_vk.dsl_cp2sc;
  ds_alloc_info.descriptorPool = cara_vk.descriptor_pool_cbuf;
  for ( unsigned int fif_idx = 0; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    SPALL_BEGIN("2nd_vkAllocateDescriptorSets");
    result =
      vkAllocateDescriptorSets( cara_vk.log_dev, &ds_alloc_info, &cara_vk.ds_cp2sc[fif_idx] );
    SPALL_END();
    if ( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara] Error occurred during descriptor set allocation.(cbuf) '%s'\n",
          string_VkResult(result) );
      goto stop_everything_after_failure;
    }
  }

  // Just like descriptor flags, for each descriptor set we allocate, we can describe how large the
  //   descriptor array should be.
  VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variable_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
    .pNext = nullptr,
    .descriptorSetCount = 1,
    .pDescriptorCounts = &max_nr_img_descriptors_in_set,
  };
  ds_alloc_info.pNext = &variable_info;
  ds_alloc_info.pSetLayouts = &cara_vk.dsl_main_render_img;
  ds_alloc_info.descriptorPool = cara_vk.descriptor_pool_main_img;
  SPALL_BEGIN("vkAllocateDescriptorSets");
  result = vkAllocateDescriptorSets( cara_vk.log_dev, &ds_alloc_info, &cara_vk.ds_main_render_img );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during descriptor set allocation.(images) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }


  //
  // Write on the descriptor
  //
  VkDescriptorImageInfo desc_img_info = {
    .sampler = cara_vk.albmap_sampler,  // Actually not needed for immutable samplers
    .imageView = cara_vk.albmap_ivs[0],
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet arr_desc_writes[1];
  arr_desc_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  arr_desc_writes[0].pNext = nullptr;
  arr_desc_writes[0].dstSet = cara_vk.ds_main_render_img;
  arr_desc_writes[0].dstBinding = 0;
  arr_desc_writes[0].dstArrayElement = 0;
  arr_desc_writes[0].descriptorCount = 1;
  arr_desc_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  arr_desc_writes[0].pImageInfo = &desc_img_info; // For samplers and all kinds of image
                                                  //  descriptors.
  arr_desc_writes[0].pBufferInfo = nullptr;       // For uniform and storage buffers and their
                                                  //  dynamic variations.
  arr_desc_writes[0].pTexelBufferView = nullptr;  // For uniform and storage texel buffers.
  SPALL_BEGIN("vkUpdateDescriptorSets");
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
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &vk_alloc_callbacks,
      &cara_vk.sm_simple_vert );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(V). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_simple_frag;
  sm_create_info.pCode = cara_vk.spv_simple_frag;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &vk_alloc_callbacks,
      &cara_vk.sm_simple_frag );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(F). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_z_only;
  sm_create_info.pCode = cara_vk.spv_z_only;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &vk_alloc_callbacks,
      &cara_vk.sm_z_only );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(Dv). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_z_only_frag;
  sm_create_info.pCode = cara_vk.spv_z_only_frag;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &vk_alloc_callbacks,
      &cara_vk.sm_z_only_frag );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(Df). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_swapchain_vert;
  sm_create_info.pCode = cara_vk.spv_swapchain_vert;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &vk_alloc_callbacks,
      &cara_vk.sm_cp2sc_vert );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(Cv). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  sm_create_info.codeSize = cara_vk.len_swapchain_frag;
  sm_create_info.pCode = cara_vk.spv_swapchain_frag;
  result = vkCreateShaderModule( cara_vk.log_dev, &sm_create_info, &vk_alloc_callbacks,
      &cara_vk.sm_cp2sc_frag );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating a shader module(Cf). '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }


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
  depth_only_plss_create_info[vertex].module = cara_vk.sm_z_only;
  depth_only_plss_create_info[vertex].pName = "vert_main";
  depth_only_plss_create_info[fragment].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  depth_only_plss_create_info[fragment].module = cara_vk.sm_z_only_frag;
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
  copy2sc_plss_create_info[vertex].module = cara_vk.sm_cp2sc_vert;
  copy2sc_plss_create_info[vertex].pName = "vert_main";
  copy2sc_plss_create_info[fragment].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  copy2sc_plss_create_info[fragment].module = cara_vk.sm_cp2sc_frag;
  copy2sc_plss_create_info[fragment].pName = "frag_main";


  //
  // Vertex input binding description
  //

  // Vertex input binding description for depth-only prepass
  VkVertexInputBindingDescription vib_desc_depth_only = {
    .binding = 0,
    //.stride = sizeof(float)*3 + sizeof(float)*3,
    //.stride = sizeof(float)*3 + sizeof(float)*2 + sizeof(uint32_t),  // position, uv, texid
    .stride = sizeof(float)*3,  // position
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  VkVertexInputAttributeDescription arr_via_desc_depth_only[] = {
    // location, binding, format, offset
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
  };
  VkPipelineVertexInputStateCreateInfo plvis_create_info_depth_only = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &vib_desc_depth_only,
    .vertexAttributeDescriptionCount = _arr_elem_cnt( arr_via_desc_depth_only ),
    .pVertexAttributeDescriptions = arr_via_desc_depth_only,
  };
  // Vertex input binding description for cs_draw_opaq_objs
  VkVertexInputBindingDescription arr_vib_desc_main_render[] = {
    {
      .binding = 0,
      .stride = sizeof(float)*3,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    {
      .binding = 1,
      //.stride = sizeof(float)*2 + sizeof(uint32_t),  // uv, texid
      .stride = sizeof(float)*2,  // uv
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
  };
  VkVertexInputAttributeDescription arr_via_desc_main_render[] = {
    // location, binding, format, offset
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 },
    //{ 2, 1, VK_FORMAT_R32_UINT, sizeof(float)*2 },
  };
  VkPipelineVertexInputStateCreateInfo plvis_create_info_main_render = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = _arr_elem_cnt( arr_vib_desc_main_render ),
    .pVertexBindingDescriptions = arr_vib_desc_main_render,
    .vertexAttributeDescriptionCount = _arr_elem_cnt( arr_via_desc_main_render ),
    .pVertexAttributeDescriptions = arr_via_desc_main_render,
  };
  // Vertex input binding description for cs_copy_to_swapchain_img
  //VkVertexInputBindingDescription vib_desc_cp2sc = {
  //  .binding = 0,
  //  .stride = sizeof(float)*3 + sizeof(float)*3,
  //  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  //};
  //VkVertexInputAttributeDescription arr_via_desc_cp2sc[] = {
  //  // location, binding, format, offset
  //  { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
  //  { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float)*3 },
  //};
  VkPipelineVertexInputStateCreateInfo plvis_create_info_cp2sc = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    //.vertexBindingDescriptionCount = 1,
    //.pVertexBindingDescriptions = &vib_desc_cp2sc,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = nullptr,
    //.vertexAttributeDescriptionCount = _arr_elem_cnt( arr_via_desc_cp2sc ),
    //.pVertexAttributeDescriptions = arr_via_desc_cp2sc,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = nullptr,
  };

  //
  // An input assembly state is required for the graphics pipeline creation.
  //
  VkPipelineInputAssemblyStateCreateInfo plias_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };


  //
  // Viewport ans scissor test
  //

  // The viewport and scissor test state is optional, though commonly used—we don't need to provide
  //  it when rasterization is disabled.
  VkViewport viewport = {
    .x = 0,
    .y = 0,
    .width = initial_win_width,
    .height = initial_win_height,
    .minDepth = .0f,
    .maxDepth = 1.0f,
  };
  // In Vulkan, the scissor test is always enabled.
  VkRect2D scissor = {
    .offset = (VkOffset2D) {0},
    .extent = (VkExtent2D) { .width = initial_win_width, .height = initial_win_height },
  };
  // If we want to change some of the viewport or scissor test parameters, we need to recreate a
  //  pipeline? Nah, during the pipeline creation, we can specify that the viewport and scissor test
  //  parameters are dynamic. This way, we don't need to recreate a pipeline to change these
  //  parameters—we specify them during command buffer recording. But we need to remember that the
  //  number of viewports (and scissor tests) is always specified during the pipeline creation. We
  //  can't change it later.
  VkPipelineViewportStateCreateInfo plvps_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 1,
    .pViewports = &viewport,
    .scissorCount = 1,
    .pScissors = &scissor,
  };


  //
  // VkPipelineRasterizationStateCreateInfo
  //

  // A rasterization state is always required during the graphics pipeline creation.
  // In OpenGL, by default, counterclockwise faces were considered front and culling was disabled.
  //  In Vulkan, there is no default state so how we define these parameters is up to us.
  VkPipelineRasterizationStateCreateInfo plrs_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,  // Use a true value if a depth value for fragments whose depth is
                                   //  outside of the min/max range specified in a viewport state
                                   //  should be clamped within this range, or use a false value if
                                   //  fragments with depth outside of the this range should be
                                   //  clipped (discarded); when the depthClampEnable feature is not
                                   //  enabled only a false value can be specified.
    .rasterizerDiscardEnable = VK_FALSE,  // It controls whether primitives are discarded
                                          // immediately before the rasterization stage.
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,  // TODO Consider culling them yourself
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = .0f,
    .depthBiasClamp = .0f,  // It must be 0.0 when depthBiasEnable is VK_FALSE
    .depthBiasSlopeFactor = .0f,
    .lineWidth = 1.0f,
  };
  VkPipelineRasterizationStateCreateInfo plrs_create_info_nocull = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE,  // No culling will be done by the vulkan driver
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = .0f,
    .depthBiasClamp = .0f,  // It must be 0.0 when depthBiasEnable is VK_FALSE
    .depthBiasSlopeFactor = .0f,
    .lineWidth = 1.0f,
  };


  //
  // MSAA
  //
  VkPipelineMultisampleStateCreateInfo plmss_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    //.minSampleShading = .5f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
  };


  //
  // Depth/Stencil
  //
  VkPipelineDepthStencilStateCreateInfo arr_pldss_create_info[
    (unsigned int)cara_nr_subpasses + cara_nr_subpasses_in_2nd_rp ];
  for ( unsigned int sp_idx = 0;
      sp_idx < (unsigned int)cara_nr_subpasses + cara_nr_subpasses_in_2nd_rp;
      sp_idx++ ) {
    arr_pldss_create_info[sp_idx].sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    arr_pldss_create_info[sp_idx].pNext = nullptr;
    arr_pldss_create_info[sp_idx].flags = 0;
    arr_pldss_create_info[sp_idx].depthBoundsTestEnable = VK_FALSE;
    arr_pldss_create_info[sp_idx].stencilTestEnable = VK_FALSE;
    arr_pldss_create_info[sp_idx].front = (VkStencilOpState) {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .depthFailOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_EQUAL,
      .compareMask = ~UINT32_C(0),
      .writeMask = ~UINT32_C(0),
      .reference = 0,
    };
    arr_pldss_create_info[sp_idx].back = (VkStencilOpState) {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .depthFailOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_NEVER,
      .compareMask = 0,
      .writeMask = 0,
      .reference = 0,
    };
    arr_pldss_create_info[sp_idx].minDepthBounds = .0f;
    arr_pldss_create_info[sp_idx].maxDepthBounds = 1.0f;
  }
  arr_pldss_create_info[cs_z_only].depthTestEnable = VK_TRUE;
  arr_pldss_create_info[cs_z_only].depthWriteEnable = VK_TRUE;
  arr_pldss_create_info[cs_z_only].depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  arr_pldss_create_info[cs_opaq_objs].depthTestEnable = VK_TRUE;
  arr_pldss_create_info[cs_opaq_objs].depthWriteEnable = VK_FALSE;
  arr_pldss_create_info[cs_opaq_objs].depthCompareOp = VK_COMPARE_OP_EQUAL;
  unsigned int base_idx = cara_nr_subpasses;
  arr_pldss_create_info[base_idx + cs2_copy_to_sc].depthTestEnable = VK_FALSE;
  arr_pldss_create_info[base_idx + cs2_copy_to_sc].depthWriteEnable = VK_FALSE;
  arr_pldss_create_info[base_idx + cs2_copy_to_sc].depthCompareOp = VK_COMPARE_OP_ALWAYS;


  //
  // VkPipelineColorBlendAttachmentState
  //

  // The blending state is optional and is not required if rasterization is disabled or when there
  //  are no color attachments in a subpass, in which a given graphics pipeline is used.
  VkPipelineColorBlendAttachmentState plcbas = {
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT |
      0,
  };
  VkPipelineColorBlendStateCreateInfo plcbs_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_NO_OP,
    .attachmentCount = 1,
    .pAttachments = &plcbas,
    .blendConstants[0] = .0f,
    .blendConstants[1] = .0f,
    .blendConstants[2] = .0f,
    .blendConstants[3] = .0f,
  };

  //
  // Dynamic states
  //

  // Define the dynamic parts of the pipeline
  VkDynamicState dynamic_states[] = {
    VK_DYNAMIC_STATE_VIEWPORT,  // vkCmdSetViewport()
    VK_DYNAMIC_STATE_SCISSOR,  // vkCmdSetScissor()
  };
  VkPipelineDynamicStateCreateInfo plds_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .dynamicStateCount = _arr_elem_cnt( dynamic_states ),
    .pDynamicStates = dynamic_states,
  };


  //
  // Create a pipeline layout
  //
  VkPushConstantRange arr_pc_ranges[1];
  arr_pc_ranges[0].offset = 0;  assume( arr_pc_ranges[0].offset % 4 == 0 );
  arr_pc_ranges[0].size = 64;  assume( arr_pc_ranges[0].size % 4 == 0 );
  arr_pc_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  VkPipelineLayoutCreateInfo pll_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = 1,
    .pSetLayouts = &cara_vk.dsl_z_only,
    .pushConstantRangeCount = _arr_elem_cnt( arr_pc_ranges ),
    .pPushConstantRanges = arr_pc_ranges,
  };
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
  SPALL_BEGIN("create_plls");
  result = vkCreatePipelineLayout( cara_vk.log_dev, &pll_create_info,
      &vk_alloc_callbacks, &cara_vk.pll_z_only
      );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating the pipeline layout.(D) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  arr_pc_ranges[0].offset = 0;  assume( arr_pc_ranges[0].offset % 4 == 0 );
  arr_pc_ranges[0].size = 64;  assume( arr_pc_ranges[0].size % 4 == 0 );
  VkDescriptorSetLayout arr_setlayouts[] = {
    cara_vk.dsl_main_render_img, cara_vk.dsl_main_render_sampler };
  pll_create_info.setLayoutCount = _arr_elem_cnt( arr_setlayouts );
  pll_create_info.pSetLayouts = arr_setlayouts;
  result = vkCreatePipelineLayout( cara_vk.log_dev, &pll_create_info,
      &vk_alloc_callbacks, &cara_vk.pll_main_render
      );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating the pipeline layout.(M) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  arr_pc_ranges[0].offset = 0;  assume( arr_pc_ranges[0].offset % 4 == 0 );
  arr_pc_ranges[0].size = 64;  assume( arr_pc_ranges[0].size % 4 == 0 );
  pll_create_info.setLayoutCount = 1;
  pll_create_info.pSetLayouts = &cara_vk.dsl_cp2sc;
  result = vkCreatePipelineLayout( cara_vk.log_dev, &pll_create_info,
      &vk_alloc_callbacks, &cara_vk.pll_cp2sc
      );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating the pipeline layout.(S) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }


  //
  // Create renderpasses and framebuffers
  //
  SPALL_BEGIN("1st_fb");
  err = create_renderpass_and_framebuffer_first_time( &cara_vk );
  SPALL_END();
  if ( err ) {
    goto stop_everything_after_failure;
  }


  //
  // Create a graphics pipeline
  //
  VkGraphicsPipelineCreateInfo arr_gpl_create_infos[
    (unsigned int)cara_nr_subpasses + cara_nr_subpasses_in_2nd_rp ];
  for ( unsigned int sp_idx = 0;
      sp_idx < (unsigned int)cara_nr_subpasses + cara_nr_subpasses_in_2nd_rp;
      sp_idx++ ) {
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

  // cs_depth_only pipeline
  arr_gpl_create_infos[cs_z_only].stageCount = _arr_elem_cnt( depth_only_plss_create_info );
  arr_gpl_create_infos[cs_z_only].stageCount = _arr_elem_cnt( depth_only_plss_create_info );
  arr_gpl_create_infos[cs_z_only].pStages = depth_only_plss_create_info;
  arr_gpl_create_infos[cs_z_only].pVertexInputState = &plvis_create_info_depth_only;
  arr_gpl_create_infos[cs_z_only].pInputAssemblyState = &plias_create_info;
  arr_gpl_create_infos[cs_z_only].pTessellationState = nullptr;
  arr_gpl_create_infos[cs_z_only].pViewportState = &plvps_create_info;
  arr_gpl_create_infos[cs_z_only].pRasterizationState = &plrs_create_info;
  arr_gpl_create_infos[cs_z_only].pMultisampleState = &plmss_create_info;  // NOTE
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
  arr_gpl_create_infos[cs_z_only].pDepthStencilState = &arr_pldss_create_info[cs_z_only];
  arr_gpl_create_infos[cs_z_only].pColorBlendState = nullptr;  // No color attachment
  arr_gpl_create_infos[cs_z_only].pDynamicState = &plds_create_info;
  arr_gpl_create_infos[cs_z_only].layout = cara_vk.pll_z_only;
  arr_gpl_create_infos[cs_z_only].renderPass = cara_vk.arr_renderpasses[0];
  arr_gpl_create_infos[cs_z_only].subpass = cs_z_only;
  arr_gpl_create_infos[cs_z_only].basePipelineIndex = -1;  // We can't use both
                        // basePipelineHandle and basePipelineIndex members; we can provide value
                        // only for one of them. If we want to specify a handle, we must provide a
                        // -1 value for the basePipelineIndex field. If we want to specify an index,
                        // we need to provide a VK_NULL_HANDLE value for basePipelineHandle

  // cs_draw_opaq_objs pipeline
  arr_gpl_create_infos[cs_opaq_objs].stageCount = _arr_elem_cnt( main_render_plss_create_info );
  arr_gpl_create_infos[cs_opaq_objs].pStages = main_render_plss_create_info;
  arr_gpl_create_infos[cs_opaq_objs].pVertexInputState = &plvis_create_info_main_render;
  arr_gpl_create_infos[cs_opaq_objs].pInputAssemblyState = &plias_create_info;
  arr_gpl_create_infos[cs_opaq_objs].pTessellationState = nullptr;
  arr_gpl_create_infos[cs_opaq_objs].pViewportState = &plvps_create_info;
  arr_gpl_create_infos[cs_opaq_objs].pRasterizationState = &plrs_create_info;
  arr_gpl_create_infos[cs_opaq_objs].pMultisampleState = &plmss_create_info;
  arr_gpl_create_infos[cs_opaq_objs].pDepthStencilState = &arr_pldss_create_info[cs_opaq_objs];
  arr_gpl_create_infos[cs_opaq_objs].pColorBlendState = &plcbs_create_info;
  arr_gpl_create_infos[cs_opaq_objs].pDynamicState = &plds_create_info;
  arr_gpl_create_infos[cs_opaq_objs].layout = cara_vk.pll_main_render;
  arr_gpl_create_infos[cs_opaq_objs].renderPass = cara_vk.arr_renderpasses[0];
  arr_gpl_create_infos[cs_opaq_objs].subpass = cs_opaq_objs;
  arr_gpl_create_infos[cs_opaq_objs].basePipelineIndex = cs_z_only;

  // cs2_copy_to_sc pipeline
  VkGraphicsPipelineCreateInfo * cp2sc_gpl_create_info =
    &arr_gpl_create_infos[(unsigned int)cara_nr_subpasses + cs2_copy_to_sc];
  cp2sc_gpl_create_info->stageCount = _arr_elem_cnt( copy2sc_plss_create_info );
  cp2sc_gpl_create_info->pStages = copy2sc_plss_create_info;
  cp2sc_gpl_create_info->pVertexInputState = &plvis_create_info_cp2sc;
  cp2sc_gpl_create_info->pInputAssemblyState = &plias_create_info;
  cp2sc_gpl_create_info->pTessellationState = nullptr;
  cp2sc_gpl_create_info->pViewportState = &plvps_create_info;
  cp2sc_gpl_create_info->pRasterizationState = &plrs_create_info_nocull;
  cp2sc_gpl_create_info->pMultisampleState = &plmss_create_info;
  cp2sc_gpl_create_info->pDepthStencilState =
    &arr_pldss_create_info[(unsigned int)cara_nr_subpasses + cs2_copy_to_sc];
  cp2sc_gpl_create_info->pColorBlendState = &plcbs_create_info;
  cp2sc_gpl_create_info->pDynamicState = &plds_create_info;
  cp2sc_gpl_create_info->layout = cara_vk.pll_cp2sc;
  cp2sc_gpl_create_info->renderPass = cara_vk.arr_renderpasses[1];
  cp2sc_gpl_create_info->subpass = cs2_copy_to_sc;
  cp2sc_gpl_create_info->basePipelineIndex = cs_opaq_objs;
  // NOTE You might want to utilized a pipeline cache if you have a lot of pipelines to create
  SPALL_BEGIN("create_G_pl");
  result = vkCreateGraphicsPipelines( cara_vk.log_dev,
      VK_NULL_HANDLE,  // Pipeline Cache Object
                       // Creating a pipeline object is a complicated and time-consuming process
                       // from the driver's perspective. A pipeline object is not a simple wrapper
                       // for parameters set during the creation. It involves preparing the states
                       // of all programmable and fixed pipeline stages, setting an interface
                       // between shaders and descriptor resources, compiling and linking shader
                       // programs, and performing error checking (that is, checking if shaders are
                       // linked properly). Results of these operations may be stored in a cache.
                       // This cache can then be reused to speed up the creation of pipeline objects
                       // with similar properties.
      //cara_nr_subpasses, arr_gpl_create_infos,
      (unsigned int)cara_nr_subpasses + cara_nr_subpasses_in_2nd_rp, arr_gpl_create_infos,
      &vk_alloc_callbacks,
      cara_vk.arr_pipelines
      );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred creating pipelines. (1st) '%s'\n",
        string_VkResult(result) );
    goto stop_everything_after_failure;
  }
  //SPALL_BEGIN("create_G_pl");
  //result = vkCreateGraphicsPipelines( cara_vk.log_dev,
  //    VK_NULL_HANDLE,  // Pipeline Cache Object
  //                     // Creating a pipeline object is a complicated and time-consuming process
  //                     // from the driver's perspective. A pipeline object is not a simple wrapper
  //                     // for parameters set during the creation. It involves preparing the states
  //                     // of all programmable and fixed pipeline stages, setting an interface
  //                     // between shaders and descriptor resources, compiling and linking shader
  //                     // programs, and performing error checking (that is, checking if shaders are
  //                     // linked properly). Results of these operations may be stored in a cache.
  //                     // This cache can then be reused to speed up the creation of pipeline objects
  //                     // with similar properties.
  //    cara_nr_subpasses_in_2nd_rp, arr_gpl_create_infos + cara_nr_subpasses,
  //    &vk_alloc_callbacks,
  //    cara_vk.arr_pipelines2
  //    );
  //SPALL_END();
  //if ( result != VK_SUCCESS ) {
  //  fprintf( stderr, "[Carbonara] Error occurred creating pipelines. (2nd) '%s'\n",
  //      string_VkResult(result) );
  //  goto stop_everything_after_failure;
  //}



  SPALL_BEGIN("infos_pm_return");
  // FIXME Actually, return them at the next even/odd frame id.
  err = infos_pm_return( &cara_vk.vmm.uploads,
      &p_ws->malloc_helper, cara_malloc, rent_for_stone );
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_return for stone failed.. (err_msg=%u)\n", err );
    goto stop_everything_after_failure;
  }
  err = infos_pm_return( &cara_vk.vmm.uploads,
      &p_ws->malloc_helper, cara_malloc, rent_for_sand );
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_return for sand failed.. (err_msg=%u)\n", err );
    goto stop_everything_after_failure;
  }
  err = infos_pm_return( &cara_vk.vmm.uploads,
      &p_ws->malloc_helper, cara_malloc, rent_for_vb );
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_return for vb failed.. (err_msg=%u)\n", err );
    goto stop_everything_after_failure;
  }
  err = infos_pm_return( &cara_vk.vmm.uploads,
      &p_ws->malloc_helper, cara_malloc, rent_for_ib );
  SPALL_END();
  if ( err ) {
    fprintf( stderr, "[Carbonara-Debug] infos_pm_return for ib failed.. (err_msg=%u)\n", err );
    goto stop_everything_after_failure;
  }


  SPALL_END();  // renderer_setup

  // NOTE call destroy_vulkan_objects() in SDL's main thread to close SDL_Window correctly
  //func_call_stack_element.func = (void(*)(void*))destroy_vulkan_objects;
  //func_call_stack_element.arg = ws->worker_info->idx_self;
  //infos_stack_push( cara_single.foremans_quit_stack,
  //    1, (const void*) &func_call_stack_element,
  //    31 - _lzcnt_u32(sizeof(struct func_call_stack_element)) );



  atomic_store_explicit( &cara_vk.frame_id, cara_max_nr_fif, memory_order_release );

  // Signal semaphores so that the very first rendering function won't be hanging forever.
  // fif_idx 0 is signaled in a submit in init_renderer()
  for ( unsigned int fif_idx = 1; fif_idx < cara_max_nr_fif; fif_idx++ ) {
    VkSemaphoreSignalInfo sgnl_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
      .pNext = nullptr,
      .semaphore = cara_vk.arr_semphos_gr[fif_idx],
      .value = csb_present_swapchain_img,
    };
    vkSignalSemaphore( cara_vk.log_dev, &sgnl_info );
  }

  // Signal arr_semaphore_tr[1] so that the very first rendering function won't wait forever.
  // [0] will be signaled after the commands for vbuf/ibuf/albmap upload.
  VkSemaphoreSignalInfo sgnl_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
    .pNext = nullptr,
    .semaphore = cara_vk.arr_semphos_tr[1],
    .value = csb_data_trans_done,
  };
  vkSignalSemaphore( cara_vk.log_dev, &sgnl_info );


  return emsg_no_error;

stop_everything_after_failure:

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

  if ( cara_vk.pll_z_only ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_z_only,
        &vk_alloc_callbacks
        //nullptr
        );
    cara_vk.pll_z_only = VK_NULL_HANDLE;
  }
  if ( cara_vk.pll_main_render ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_main_render,
        &vk_alloc_callbacks
        //nullptr
        );
    cara_vk.pll_main_render = VK_NULL_HANDLE;
  }
  if ( cara_vk.pll_cp2sc ) {
    vkDestroyPipelineLayout( cara_vk.log_dev, cara_vk.pll_cp2sc,
        &vk_alloc_callbacks
        //nullptr
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
      err = ma_report_vk_malloc_rel( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc,
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
      else {
        cara_vk.mbi_albmaps[img_id].range = invalid_range;
        cara_vk.mbi_albmaps[img_id].malloc = VK_NULL_HANDLE;
        vkDestroyImage( cara_vk.log_dev, cara_vk.albmaps[img_id], &vk_alloc_callbacks );
        cara_vk.albmaps[img_id] = VK_NULL_HANDLE;
      }
    }
  }

  if ( cara_vk.vert_buf ) {
    vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.vert_buf, &req );
    preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    required_props = 0;
    tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
    err = ma_report_vk_malloc_rel( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc,
        mtype_idx,
        cara_vk.mbi_vbuf );
    if ( err ) {
      fprintf( stderr, "[Carbonara] Failure occurred in ma_report_vk_malloc_rel!! %s:%d '%s'\n",
          __FILE__, __LINE__, arr_emsg_txt[err] );
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
    else {
      cara_vk.mbi_vbuf.range = invalid_range;
      cara_vk.mbi_vbuf.malloc = VK_NULL_HANDLE;
      vkDestroyBuffer( cara_vk.log_dev, cara_vk.vert_buf, &vk_alloc_callbacks );
      cara_vk.vert_buf = VK_NULL_HANDLE;
    }
  }

  if ( cara_vk.attr_buf ) {
    vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.attr_buf, &req );
    preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    required_props = 0;
    tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
    err = ma_report_vk_malloc_rel( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc,
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
    vkGetBufferMemoryRequirements( cara_vk.log_dev, cara_vk.ind_buf, &req );
    preferred_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    required_props = 0;
    tolerable_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    mtype_idx = ma_pick_best_mtype(
      &cara_vk.vmm, &req, required_props, preferred_props, tolerable_props );
    err = ma_report_vk_malloc_rel( &cara_vk.vmm, &p_ws->malloc_helper, cara_malloc,
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
    else {
      cara_vk.mbi_ibuf.range = invalid_range;
      cara_vk.mbi_ibuf.malloc = VK_NULL_HANDLE;
      vkDestroyBuffer( cara_vk.log_dev, cara_vk.ind_buf, &vk_alloc_callbacks );
      cara_vk.ind_buf = VK_NULL_HANDLE;
    }
  }


  return emsg_unknown;

}


//-------------------------------------------------------------------------------------------------
enum err_msg cara_deal_with_asyncio(
    SDL_AsyncIOQueue * restrict p_aioq,
    struct cara_worker_state * restrict p_ws
    )
{
  SPALL_BEGIN("deal_with_aio");
  constexpr unsigned int mask = 0b1;
  static_assert( mask + 1 == nr_al_cmd_pool_sets && _is_power_of_two(mask + 1) );
  typeof(p_ws->io_work.local_vram_batch_cntr) cur_cntr =
    atomic_load_explicit( &p_ws->io_work.vram_batch_cntr, memory_order_acquire );
  unsigned int set_idx = mask & cur_cntr;

  if ( cur_cntr != p_ws->io_work.local_vram_batch_cntr ) {
    VkResult result;
    SPALL_BEGIN("vkResetCommandPool-A");
    result = vkResetCommandPool(
        cara_vk.log_dev,
        cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools[p_ws->worker_idx],
        0 );
    SPALL_END();
    if( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara-IO] Could not reset a command pool. '%s'\n",
          string_VkResult(result) );
      SPALL_END();  // deal_with_aio
      return emsg_unknown;
    }

    VkCommandBufferBeginInfo cmd_buf_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,  // TODO FIXME
                 // a pointer to a VkCommandBufferInheritanceInfo structure, used if commandBuffer
                 // is a secondary command buffer. If this is a primary command buffer, then this
                 // value is ignored.
    };
    SPALL_BEGIN("vkBeginCommandBuffer-A");
    result = vkBeginCommandBuffer(
        cara_get_arr_cmd_bufs_for_transfer(cara_single.proc_map->nr_processors, set_idx)[
          p_ws->worker_idx ],
        &cmd_buf_begin_info );
    SPALL_END();
    if( result != VK_SUCCESS ) {
      fprintf( stderr, "[Carbonara-IO] Could not begin command buffer recording operation. '%s'\n",
          string_VkResult(result) );
      SPALL_END();  // deal_with_aio
      return emsg_unknown;
    }

    p_ws->io_work.local_vram_batch_cntr = cur_cntr;
  }  // if ( cur_cntr != p_ws->io_work.local_vram_batch_cntr )

  union union64 counts = {.u = atomic_load_explicit(
      &p_ws->io_work.nr_vram_jobs_to_get_done_and_nr_vram_jobs_done[set_idx],
      memory_order_acquire )};
  uint32_t nr_vram_load_jobs_to_get_done = counts.arr_u32[0];
  uint32_t nr_vram_load_jobs_done = counts.arr_u32[1];

  unsigned int nr_handles_failed_to_close = 0;
  union cara_io_job * arr_p_jobs_failed_to_close[
    _arr_elem_cnt(p_ws->io_work.jgrp_idx_plus1) * nr_io_jobs_in_job_grp ];
  SDL_AsyncIOOutcome io_outcome;
  while ( !SDL_GetAsyncIOResult(p_aioq, &io_outcome) ) {
    union cara_io_job * p_job = io_outcome.userdata;
    switch ( p_job->type / 0x40 ) {
//-------------------------------------------------------------------------------------------------
      // Load image
      case 0x00 / 0x40:
        switch ( io_outcome.result ) {
          case SDL_ASYNCIO_CANCELED:
            fprintf( stderr, "[Carbonara-IO] loading of image #%u has been cancelled\n"
                "  SDL_GetError() = '%s'\n", p_job->img.img_id, SDL_GetError() );
            // Try again
            if ( io_outcome.type == SDL_ASYNCIO_TASK_CLOSE ) {
              bool succ_close = SDL_CloseAsyncIO( io_outcome.asyncio, false, p_aioq, p_job );
              if ( !succ_close ) {
                arr_p_jobs_failed_to_close[nr_handles_failed_to_close] = p_job;
                nr_handles_failed_to_close++;
                break;
              }
              //p_job->img.cur = io_outcome.bytes_requested + 1;
            }  // if ( io_outcome.type == SDL_ASYNCIO_TASK_CLOSE )
            else {  // Try to read again
              if ( !SDL_ReadAsyncIO( io_outcome.asyncio,
                    cara_vk.vmm.p_staging_buf + p_job->img.p_malloc_range->start,
                    io_outcome.offset, io_outcome.bytes_requested, p_aioq, p_job ) ) {
                fprintf( stderr, "[Carbonara-IO] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
              }
              SPALL_END();  // deal_with_aio
              return emsg_unknown;
            }
            break;

          case SDL_ASYNCIO_FAILURE:
            fprintf( stderr, "[Carbonara-IO] failed to load image #%u\n"
                "  SDL_GetError() = '%s'\n", p_job->img.img_id, SDL_GetError() );
            // Try again
            if ( io_outcome.type == SDL_ASYNCIO_TASK_CLOSE ) {
              bool succ_close = SDL_CloseAsyncIO( io_outcome.asyncio, false, p_aioq, p_job );
              if ( !succ_close ) {
                arr_p_jobs_failed_to_close[nr_handles_failed_to_close] = p_job;
                nr_handles_failed_to_close++;
                break;
              }
              //p_job->img.cur = io_outcome.bytes_requested + 1;
            }  // if ( io_outcome.type == SDL_ASYNCIO_TASK_CLOSE )
            else {  // Try to read again
              if ( !SDL_ReadAsyncIO( io_outcome.asyncio,
                    cara_vk.vmm.p_staging_buf + p_job->img.p_malloc_range->start,
                    io_outcome.offset, io_outcome.bytes_requested, p_aioq, p_job ) ) {
                fprintf( stderr, "[Carbonara-IO] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
              }
              SPALL_END();  // deal_with_aio
              return emsg_unknown;
            }
            break;

          case SDL_ASYNCIO_COMPLETE:
            if ( io_outcome.bytes_transferred != io_outcome.bytes_requested ) {
              fprintf( stderr, "[Carbonara-IO] something's wrong with image #%u\n"
                  "  image size = %lu  vs. bytes transferred = %lu\n",
                  p_job->img.img_id, io_outcome.bytes_requested, io_outcome.bytes_transferred );
              if ( !SDL_ReadAsyncIO( io_outcome.asyncio,
                    cara_vk.vmm.p_staging_buf + p_job->img.p_malloc_range->start,
                    io_outcome.offset, io_outcome.bytes_requested, p_aioq, p_job ) ) {
                fprintf( stderr, "[Carbonara-IO] SDL_ReadAsyncIO failed! '%s'\n", SDL_GetError() );
              }
              SPALL_END();  // deal_with_aio
              return emsg_unknown;
            }
            bool succ_close = SDL_CloseAsyncIO( io_outcome.asyncio, false, p_aioq, p_job );
            if (unlikely( !succ_close )) {
              //p_job->img.cur = io_outcome.bytes_requested;
              arr_p_jobs_failed_to_close[nr_handles_failed_to_close] = p_job;
              nr_handles_failed_to_close++;
            }
            else {
              //p_job->img.cur = io_outcome.bytes_requested + 1;
            }

            if ( p_job->type == iojt_ld_albedo_bc7_ml0_12_uc ) {
              VkCommandBuffer cmd_buf = cara_get_arr_cmd_bufs_for_transfer(
                  cara_single.proc_map->nr_processors, set_idx )[p_ws->worker_idx];
              unsigned int mip_lv, width, offset;
              for ( mip_lv = 0, width = 2048;
                  mip_lv < p_job->img.lowest_miplv;
                  mip_lv++ ) {
                width /= 2;
              }
              constexpr unsigned int nr_miplvls_in_2048 = 12;
              VkBufferImageCopy arr_regions[nr_miplvls_in_2048];
              unsigned int region_idx = 0;
              for ( offset = p_job->img.p_malloc_range->start; region_idx < p_job->img.nr_miplvls;
                  region_idx++, mip_lv++, width/=2 ) {
                arr_regions[region_idx].bufferOffset = offset;
                arr_regions[region_idx].bufferRowLength = 0;
                arr_regions[region_idx].bufferImageHeight = 0;
                arr_regions[region_idx].imageSubresource = (VkImageSubresourceLayers) {
                  VK_IMAGE_ASPECT_COLOR_BIT, mip_lv, p_job->img.img_id % cara_nr_tex_in_texarr, 1 };
                arr_regions[region_idx].imageOffset = (VkOffset3D) {0, 0, 0};
                arr_regions[region_idx].imageExtent = (VkExtent3D) {width, width, 1};
                assume( _is_power_of_two(width) );
                switch ( width ) {
                  case 4:
                    [[fallthrough]];  // 4*4 = 16 which happens to be the texel block size of BC7
                  case 2:
                    [[fallthrough]];
                  case 1:
                    offset += 16;     // The minimum size of the map is 16 bytes probably because of
                    break;            // the texel block size of BC7
                  default:
                    offset += ( width * width ) * 4 / 4;  // Each byte stores the information of 1
                                                          //  pixel in BC7
                }
              }  // for ( offset = p_job->img.p_malloc_range->start;
                 //     region_idx < p_job->img.nr_miplvls; region_idx++, mip_lv++, width/=2 )
              SPALL_BEGIN("vkCmdCopyBufferToImage");
              vkCmdCopyBufferToImage(
                  cmd_buf,
                  cara_vk.vmm.staging_buf,
                  //cara_vk.albmaps[p_job->img.img_id / cara_nr_tex_in_texarr],
                  cara_vk.albmaps[p_job->img.img_id],
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  p_job->img.nr_miplvls,  //_arr_elem_cnt( arr_regions ),
                  arr_regions
                  );
              SPALL_END();
              nr_vram_load_jobs_done++;
              if ( nr_vram_load_jobs_done >= nr_vram_load_jobs_to_get_done ) {
                union union64 new_counts;
                new_counts.arr_u32[0] = nr_vram_load_jobs_to_get_done;
                new_counts.arr_u32[1] = nr_vram_load_jobs_done;
                atomic_store_explicit(
                    &p_ws->io_work.nr_vram_jobs_to_get_done_and_nr_vram_jobs_done[set_idx],
                    new_counts.u,
                    memory_order_release );
              }
            }  // if ( p_job->type == iojt_ld_albedo_bc7_ml0_12_uc )

            break;  // case SDL_ASYNCIO_COMPLETE:
        }  // switch ( io_outcome.result )
        break;  // case Load image
//-------------------------------------------------------------------------------------------------
      // Load SPIR-V code
      case 0x40 / 0x40:
        // TODO Call vkCreateShaderModule vkCreatePipelineLayout vkCreateGraphicsPipelines
        break;  // case Load SPIR-V code
//-------------------------------------------------------------------------------------------------
      default:
        fprintf( stderr, "[Carbonara-IO] Unknown io_job_type %#010x\n", p_job->type );
        SPALL_END();  // deal_with_aio
        return emsg_corrupted_data_detected;
//-------------------------------------------------------------------------------------------------
    }  // switch ( p_job->type / 0x40 )
  }  // while ( !SDL_GetAsyncIOResult(p_aioq, &io_outcome) )

  // FIXME Why are we insisting on closing every single handle in this instance?
  for ( unsigned int idx = 0; idx < nr_handles_failed_to_close; idx++ ) {
    while ( !SDL_CloseAsyncIO(arr_p_jobs_failed_to_close[idx]->img.p_io,
          false, p_aioq, arr_p_jobs_failed_to_close[idx]) ) {
      hold_up_thread( 1000 * 1 );  // 1 micro-second
    }
  }

  SPALL_END();  // deal_with_aio
  return emsg_no_error;
}


//-------------------------------------------------------------------------------------------------
void jf_render_frame( uint64_t cur_time_in_5us )
{
  SPALL_BEGIN("jf_render_frame");
  //
  // TODO Check timestamps
  //

  static uint32_t frame_id = cara_max_nr_fif - 1;
  static_assert( sizeof(frame_id) == _sizeof_member(struct cara_vk_renderer, frame_id) );

//-------------------------------------------------------------------------------------------------
  // TODO Write renderer here for now but move them out of SDL_AppIterate later.

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
  VkCommandBufferBeginInfo cmd_buf_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |  // Each recording of the command buffer will
                                                     // only be submitted once, and the command
                                                     // buffer will be reset and recorded again
                                                     // between each submission.
      0,
    .pInheritanceInfo = nullptr,
             // a pointer to a VkCommandBufferInheritanceInfo structure, used if `commandBuffer`
             // is a secondary command buffer. If this is a primary command buffer, then this
             // value is ignored.
  };



  // Wait for the previous command buffer to be read through.
  uint64_t wait_val =
    csb_present_swapchain_img + ((frame_id + 1) / cara_max_nr_fif - 1) * cara_nr_signs_gr;
  VkSemaphoreWaitInfo wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .pNext = nullptr,
    .flags = 0,
    .semaphoreCount = 1,
    .pSemaphores = &cara_vk.arr_semphos_gr[(frame_id + 1) % cara_max_nr_fif],
    .pValues = &wait_val,
  };
  SPALL_BEGIN("wait_graphics");
  result = vkWaitSemaphores( cara_vk.log_dev, &wait_info, UINT64_MAX );
  SPALL_END();
  if (unlikely( result == VK_TIMEOUT )) {
    fprintf( stderr, "vkWaitSemaphores timed out\n" );
  }



  //
  // Acquire a swapchain image
  //
  uint32_t img_idx;
  enum err_msg err;

  union extent_2d cur_win_dims;
  cur_win_dims.whole = atomic_load_explicit( &cara_vk.win_size.atomic_whole, memory_order_acquire );
  if ( cur_win_dims.parts.width > cara_vk.max_win_size.parts.width ||
      cur_win_dims.parts.height > cara_vk.max_win_size.parts.height ) {
    // Recreate the swapchain
    err = expand_swapchain_and_framebuffer( &cara_vk, cur_win_dims );
    if ( err ) {
      atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
    }
  }
  //static uint64_t prev_sc_creation_time = 0;
  //if ( cur_win_dims.parts.width != cara_vk.dims_swapchain_imgs.width ||
  //    cur_win_dims.parts.height != cara_vk.dims_swapchain_imgs.height ) {
  //  auto fb_width = cara_vk.dims_swapchain_imgs.width;
  //  auto fb_height = cara_vk.dims_swapchain_imgs.height;
  //  float aspect_ratio = (float)fb_width / (float) fb_height;

  //  typeof(cur_time_in_5us) tolerance = (typeof(cur_time_in_5us)) (1000.0 / 60 * 1000 / 5);
  //  if ( fabsf(aspect_ratio - (
  //        (float)cur_win_dims.parts.width / (float)cur_win_dims.parts.height )) > 0.18 ) {
  //    tolerance *= 2;
  //  }

  //  if ( cur_time_in_5us - prev_sc_creation_time > tolerance ) {
  //    // Recreate the swapchain
  //    err = expand_swapchain_and_framebuffer( &cara_vk, cur_win_dims );
  //    if ( err ) {
  //      atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  //    }
  //    prev_sc_creation_time = cur_time_in_5us;
  //  }
  //}

  //VkCommandBuffer dummy_cmdbuf = cara_vk.dummy_cmdbuf[(frame_id + 1) / cara_max_nr_fif];
  SPALL_BEGIN("acquire_swapchain_img");
acquire_swapchain_image:
  VkResult result_2 = vkAcquireNextImageKHR( cara_vk.log_dev,
      cara_vk.swapchain,
      100000000,
      cara_vk.arr_sc_acquire_semphos[(frame_id + 1) % cara_max_nr_fif],
      VK_NULL_HANDLE,
      &img_idx );
  SPALL_END();
  // When we acquire an image, we still may not use it immediately for our purposes. We need to
  // wait for all previously submitted operations that referenced this image to finish. For this
  // purpose, a fence can be used, using which an application can check when it is safe to modify
  // an image. But we can also tell the driver that it should wait before it starts processing new
  // commands that use a given image. For this purpose, a semaphore is used, which, in general, is
  // a better option.

  // If the window is resized to a smaller size, the display engine may not force you to change your
  // swapchain image sizes. It may inform you of this through the VK_SUBOPTIMAL_KHR error code
  // (though it may not give you even that if performance of presenting is not affected). However,
  // if the window is resized to be larger, the display engine may throw VK_ERROR_OUT_OF_DATE_KHR.
  // That is not something you can ignore. Nor is it something a display engine can promise to never
  // give you.

  //VkSemaphoreSubmitInfoKHR sempho_wait_info = {
  //  .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
  //  .pNext = nullptr,
  //  .semaphore = cara_vk.arr_sc_acquire_semphos[(frame_id + 1) % cara_max_nr_fif],
  //  //.value = VK_TRUE,
  //  .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
  //  .deviceIndex = 0,
  //};
  //VkSemaphoreSubmitInfoKHR sempho_sgnl_info = {
  //  .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
  //  .pNext = nullptr,
  //  .semaphore = cara_vk.arr_semphos_gr[(frame_id + 1) % cara_max_nr_fif],
  //  .value = csb_swapchain_img_acquired + (frame_id + 1) / cara_max_nr_fif * cara_nr_signs_gr,
  //  .stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR,
  //  .deviceIndex = 0,
  //};
  //VkCommandBufferSubmitInfoKHR arr_cmd_buf_submit_infos[] = {
  //  {
  //    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
  //    .pNext = nullptr,
  //    .commandBuffer = dummy_cmdbuf,
  //    .deviceMask = 0,
  //  },
  //};
  //VkSubmitInfo2 submit_info = {
  //  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
  //  .pNext = nullptr,
  //  .flags = 0,
  //  .waitSemaphoreInfoCount = 1,
  //  .pWaitSemaphoreInfos = &sempho_wait_info,
  //  .commandBufferInfoCount = _arr_elem_cnt(arr_cmd_buf_submit_infos),
  //  .pCommandBufferInfos = arr_cmd_buf_submit_infos,
  //  .signalSemaphoreInfoCount = 1,
  //  .pSignalSemaphoreInfos = &sempho_sgnl_info,
  //};
  //SPALL_BEGIN("submit_dummy_cmd");
  //result = vkQueueSubmit2KHR( cara_vk.graphics_q, 1, &submit_info, VK_NULL_HANDLE );
  //SPALL_END();
  //if ( result != VK_SUCCESS ) {
  //  fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
  //      string_VkResult(result) );
  //  atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  //}

  switch ( result_2 ) {
    default:
#if defined( DEBUG_MODE )
      fprintf( stdout, "[Renderer-Debug] vkAcquireNextImageKHR result=%d\n", result_2 );
      fprintf( stdout, "  cara_vk.win_size.parts.width and height = %u*%u\n",
          cara_vk.win_size.parts.width, cara_vk.win_size.parts.height );
#endif
      // Recreate the swapchain ( VK_ERROR_OUT_OF_DATE_KHR )
      cur_win_dims.whole =
        atomic_load_explicit( &cara_vk.win_size.atomic_whole, memory_order_acquire );
      err = expand_swapchain_and_framebuffer( &cara_vk, cur_win_dims );
      if ( err ) {
        atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
      }

      goto acquire_swapchain_image;
    case VK_SUBOPTIMAL_KHR:
#if defined( DEBUG_MODE )
      //fprintf( stdout, "[Renderer-Debug] vkAcquireNextImageKHR=VK_SUBOPTIMAL_KHR\n" );
      //fprintf( stdout, "  cara_vk.win_size.parts.width and height = %u*%u\n",
      //    cara_vk.win_size.parts.width, cara_vk.win_size.parts.height );
#endif
      break;
    case VK_SUCCESS:
  }


  // frame_id++
  frame_id = frame_id + 1;
  atomic_store_explicit( &cara_vk.frame_id, frame_id, memory_order_release );



  //
  // Start uploading data to the GPU
  //

  wait_val = csb_data_trans_done + (frame_id / cara_max_nr_fif - 1) * cara_nr_signs_tr;
  wait_info.pSemaphores = &cara_vk.arr_semphos_tr[frame_id % cara_max_nr_fif];
  // This goes right before beginning to record the transfer command buffer
  SPALL_BEGIN("wait_transfer");
  result = vkWaitSemaphores( cara_vk.log_dev, &wait_info, UINT64_MAX );
  SPALL_END();
  if (unlikely( result == VK_TIMEOUT )) {
  }

  // Host access to `commandPool` must be externally synchronized
  SPALL_BEGIN("vkResetCommandPool-T");
  result = vkResetCommandPool(
      cara_vk.log_dev,
      cara_vk.arr_trans_cmdbuf_sets[nr_al_cmd_pool_sets + frame_id % cara_max_nr_fif].
        arr_cmd_pools[cara_single.proc_map->nr_processors],
      0 );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during command buffer reset.(t) '%s'\n",
        string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }

  // Begin recording on the primary buffer
  VkCommandBuffer pri_trans_buf =
    cara_vk.arr_trans_cmdbuf_sets[nr_al_cmd_pool_sets + frame_id % cara_max_nr_fif].
      primary_cmd_buf;
  result = vkBeginCommandBuffer( pri_trans_buf, &cmd_buf_begin_info );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command(transfer) buffer recording operation. "
        "'%s'\n", string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }

  result = vkEndCommandBuffer( pri_trans_buf );
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(t) '%s'\n",
        string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }

  //
  // Prepare to submit the command buffer
  //
  VkCommandBufferSubmitInfoKHR cmd_buf_submit_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .commandBuffer = pri_trans_buf,
    .deviceMask = 0,
  };
  VkSemaphoreSubmitInfoKHR sempho_sgnl_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .semaphore = cara_vk.arr_semphos_tr[frame_id % cara_max_nr_fif],
    .value = csb_data_trans_done + frame_id / cara_max_nr_fif * cara_nr_signs_tr,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR,
    .deviceIndex = 0,
  };
  VkSubmitInfo2 submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
    .pNext = nullptr,
    .flags = 0,
    .waitSemaphoreInfoCount = 0,
    .pWaitSemaphoreInfos = nullptr,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_buf_submit_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &sempho_sgnl_info,
  };
  SPALL_BEGIN("vkQueueSubmit2KHR");
  result = vkQueueSubmit2KHR( cara_vk.transfer_q, 1, &submit_info, VK_NULL_HANDLE );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
        string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }



  // The commented lines below are about getting a secondary command buffer:
  //constexpr uint_fast8_t worker_idx = foobar;
  //VkCommandBuffer main_cmd_buf = cara_get_arr_command_buffers_for_graphics(
  //    cara_single.proc_map->nr_processors, frame_id % cara_max_nr_fif )[
  //  worker_idx * cara_max_nr_subpasses];
  VkCommandBuffer main_cmd_buf =
    cara_vk.arr_graphics_cmdbuf_sets[frame_id % cara_max_nr_fif].primary_cmd_buf;


  //// XXX No recording yet... just mark that work has been done.
  //VkSemaphoreSignalInfo sgnl_info = {
  //  .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
  //  .pNext = nullptr,
  //  .semaphore = cara_vk.arr_semphos_tr[frame_id % cara_max_nr_fif],
  //  .value = csb_data_trans_done + frame_id / cara_max_nr_fif * cara_nr_signs_tr,
  //};
  //vkSignalSemaphore( cara_vk.log_dev, &sgnl_info );

  //
  // TODO Update the data in VRAM
  //

  // TODO vkBeginCommandBuffer
  // TODO Update the vertex buffers
  // TODO Update the index buffers
  // TODO Update the bone matrices
  // TODO vkEndCommandBuffer
  // TODO vkQueueSubmit2KHR



  // We already did it right before vkAcquireNextImageKHR
  //// Wait for the previous command buffer to be read through.
  //wait_val = csb_present_swapchain_img + (frame_id / cara_max_nr_fif - 1) * cara_nr_signs_gr;
  //wait_info.pSemaphores = &cara_vk.arr_semphos_gr[frame_id % cara_max_nr_fif];
  //SPALL_BEGIN("wait_graphics");
  //result = vkWaitSemaphores( cara_vk.log_dev, &wait_info, UINT64_MAX );
  //SPALL_END();
  //if (unlikely( result == VK_TIMEOUT )) {
  //  fprintf( stderr, "vkWaitSemaphores timed out\n" );
  //}

  SPALL_BEGIN("vkResetCommandPool-G");
  result = vkResetCommandPool(
      cara_vk.log_dev,
      cara_vk.arr_graphics_cmdbuf_sets[frame_id % cara_max_nr_fif].arr_cmd_pools[
        cara_single.proc_map->nr_processors ],
        0 );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred during command buffer reset.(g) '%s'\n",
        string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }
  // Begin recording on the primary command buffer for graphics
  SPALL_BEGIN("begin_cmd_buf");
  // TODO synchronization needed
  result = vkBeginCommandBuffer( main_cmd_buf, &cmd_buf_begin_info );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Could not begin command(graphics) buffer recording operation. "
        "'%s'\n", string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }


  //
  // Begin a renderpass
  //
  VkImageView arr_att_ivs[2];
  arr_att_ivs[fb_zbuf] = cara_vk.zbuf_n_cbuf_iv[frame_id % cara_max_nr_fif][fb_zbuf];
  arr_att_ivs[fb_cbuf] = cara_vk.zbuf_n_cbuf_iv[frame_id % cara_max_nr_fif][fb_cbuf];
  VkRenderPassAttachmentBeginInfo rp_att_begin_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
    .pNext = nullptr,
    .attachmentCount = _arr_elem_cnt( arr_att_ivs ),
    .pAttachments = arr_att_ivs,
  };
  constexpr VkClearValue clear_val = { .depthStencil.depth = .0f };
  VkRenderPassBeginInfo rp_begin_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .pNext = &rp_att_begin_info,
    .renderPass = cara_vk.arr_renderpasses[0],
    .framebuffer = cara_vk.arr_framebuffers[frame_id % cara_max_nr_fif][0],
    .renderArea = (VkRect2D) {
      .offset = { .x = 0, .y =  0 },
      .extent = { .width = cara_vk.dims_swapchain_imgs.width,
        .height = cara_vk.dims_swapchain_imgs.height },
    },
    .clearValueCount = 1,  // clearValueCount must be greater than the largest attachment index in
                           //  renderPass specifying a loadOp (or stencilLoadOp, if the attachment
                           //  has a depth/stencil format) of VK_ATTACHMENT_LOAD_OP_CLEAR.
    .pClearValues = &clear_val,
  };

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


  // VK_SUBPASS_CONTENTS_INLINE  specifies that the contents of the subpass will be recorded inline
  //  in the primary command buffer, and secondary command buffers must not be executed within the
  //  subpass.
  SPALL_BEGIN("vkCmdBeginRenderPass");
  vkCmdBeginRenderPass( main_cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE );
  SPALL_END();

  auto fb_width = cara_vk.dims_swapchain_imgs.width;
  auto fb_height = cara_vk.dims_swapchain_imgs.height;
  fb_width = min( fb_width, cur_win_dims.parts.width );
  fb_height = min( fb_height, cur_win_dims.parts.height );

  VkViewport viewport = {
    .x = 0, .y = 0,
    .width = (float)fb_width,
    .height = (float)fb_height,
    .minDepth = 0,
    .maxDepth = 1.0f,
  };
  vkCmdSetViewport( main_cmd_buf, 0, 1, &viewport );

  VkRect2D scissor = {
    .offset = (VkOffset2D) { .x = 0, .y = 0 },
    .extent = (VkExtent2D) { .width = fb_width, .height = fb_height },
  };
  vkCmdSetScissor( main_cmd_buf, 0, 1, &scissor );

  VkDeviceSize buf_ofs = 0;
  vkCmdBindVertexBuffers( main_cmd_buf, 0, 1, &cara_vk.vert_buf, &buf_ofs );
  vkCmdBindVertexBuffers( main_cmd_buf, 1, 1, &cara_vk.attr_buf, &buf_ofs );
  vkCmdBindIndexBuffer( main_cmd_buf, cara_vk.ind_buf, 0, VK_INDEX_TYPE_UINT16 );

  SPALL_BEGIN("vkCmdBindPipeline");
  vkCmdBindPipeline( main_cmd_buf,
      VK_PIPELINE_BIND_POINT_GRAPHICS, cara_vk.arr_pipelines[cs_z_only] );
  SPALL_END();

  // SUBPASS cs_depth_only
  constexpr uint64_t nr_5us_in_1s = 1000*1000 / 5;
  constexpr double pi_per_sec = M_PI / nr_5us_in_1s;
  const struct mat4x4 rotate = rot_aya_4x4( (float) ((double)cur_time_in_5us * pi_per_sec * .5) );
  const struct mat4x4 translate = translate_4x4( 0, cara_1m * -1.75f, cara_1m * 4 );
  const struct mat4x4 pp = get_perspective_proj_mat_vk(
      (float)(M_PI / 3.0), (float)fb_width/(float)fb_height, cara_1m * .25f, powf(2,127), true );
  const struct mat4x4 consts = mul_4x4_4x4( mul_4x4_4x4( rotate, translate ), pp );
  vkCmdPushConstants( main_cmd_buf, cara_vk.pll_z_only, VK_SHADER_STAGE_VERTEX_BIT,
      0, 4*4*4, &consts );
  vkCmdDrawIndexed( main_cmd_buf, 3*6 + 3*2, 1, 0, 0, 0 );


  //VkImageMemoryBarrier2KHR img_bar = {
  //  .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
  //  .pNext = nullptr,
  //  .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR |
  //    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
  //  .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR |
  //    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
  //// Prior to the intro. of VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR,
  ////  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT is the only valid stage to wait on or wait for these
  ////  operations, as the acquire and release operations do not occur in a defined stage.
  //  .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
  //  .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
  //  .oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
  //  .newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
  //  .srcQueueFamilyIndex = cara_vk.graphics_q_fam_idx,
  //  .dstQueueFamilyIndex = cara_vk.graphics_q_fam_idx,
  //// If the values of srcQueueFamilyIndex and dstQueueFamilyIndex are equal, no ownership transfer
  ////  is performed, and the barrier operates as if they were both set to VK_QUEUE_FAMILY_IGNORED.
  //  .image = cara_vk.zbuf_n_cbuf[frame_id % cara_max_nr_fif][fb_zbuf],
  //  .subresourceRange = (VkImageSubresourceRange) {
  //    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
  //    .baseMipLevel = 0,
  //    .levelCount = 1,
  //    .baseArrayLayer = 0,
  //    .layerCount = 1,
  //  },
  //};

  //VkDependencyInfoKHR dependency_info = {
  //  .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
  //  .pNext = nullptr,
  //  .dependencyFlags = VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR,
  //  .memoryBarrierCount = 0,
  //  .pMemoryBarriers = nullptr,
  //  .bufferMemoryBarrierCount = 0,
  //  .pBufferMemoryBarriers = nullptr,
  //  .imageMemoryBarrierCount = 1,
  //  .pImageMemoryBarriers = &img_bar,
  //};

  //SPALL_BEGIN("vkCmdPipelineBarrier2KHR");
  //vkCmdPipelineBarrier2KHR( main_cmd_buf, &dependency_info );
  //SPALL_END();



  SPALL_BEGIN("vkCmdNextSubpass");
  vkCmdNextSubpass( main_cmd_buf, VK_SUBPASS_CONTENTS_INLINE );
  SPALL_END();

  //
  // Bind descriptor sets to the pipeline layout
  //
  SPALL_BEGIN("vkCmdBindDescriptorSets");
  vkCmdBindDescriptorSets( main_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, cara_vk.pll_main_render,
      0,  // The first set number
      1, &cara_vk.ds_main_render_img,  // Descriptor sets
      0, nullptr );  // Dynamic offsets
                     // If any of the sets being bound include dynamic uniform or storage buffers,
                     // then pDynamicOffsets includes one element for each array element in each
                     // dynamic descriptor type binding in each set. Values are taken from
                     // pDynamicOffsets in an order such that all entries for set N come before set
                     // N+1; within a set, entries are ordered by the binding numbers in the
                     // descriptor set layouts; and within a binding array, elements are in order
  SPALL_END();
  SPALL_BEGIN("vkCmdBindDescriptorSets");
  vkCmdBindDescriptorSets( main_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, cara_vk.pll_main_render,
      1,  // The first set number
      1, &cara_vk.ds_main_render_sampler,  // Descriptor sets
      0, nullptr );  // Dynamic offsets
                     // If any of the sets being bound include dynamic uniform or storage buffers,
                     // then pDynamicOffsets includes one element for each array element in each
                     // dynamic descriptor type binding in each set. Values are taken from
                     // pDynamicOffsets in an order such that all entries for set N come before set
                     // N+1; within a set, entries are ordered by the binding numbers in the
                     // descriptor set layouts; and within a binding array, elements are in order
  SPALL_END();
  SPALL_BEGIN("vkCmdBindPipeline");
  vkCmdBindPipeline( main_cmd_buf,
      VK_PIPELINE_BIND_POINT_GRAPHICS, cara_vk.arr_pipelines[cs_opaq_objs] );
  SPALL_END();

  // SUBPASS cs_draw_opaq_objs
  vkCmdPushConstants( main_cmd_buf, cara_vk.pll_main_render, VK_SHADER_STAGE_VERTEX_BIT,
      0, 4*4*4, &consts );
  //vkCmdDrawIndexed( main_cmd_buf, 3*6 + 3*2, 1, 0, 0, 0 );
  //
  static _typeof_member(VkDescriptorPoolSize, descriptorCount) desc_ofs = 0;
  typeof(desc_ofs) local_desc_idx = 0;
  VkDescriptorImageInfo desc_img_info = {
    .sampler = VK_NULL_HANDLE,
    .imageView = cara_vk.albmap_ivs[local_desc_idx],
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet write_ds = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = cara_vk.ds_main_render_img,
    .dstBinding = 0,
    .dstArrayElement = desc_ofs,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    .pImageInfo = &desc_img_info,
    .pBufferInfo = nullptr,
    .pTexelBufferView = nullptr,
  };
  // One way we can use VK_EXT_descriptor_indexing is to treat the update-after-bind descriptor
  //   set as a ring buffer where we write descriptors,
  vkUpdateDescriptorSets( cara_vk.log_dev, 1, &write_ds, 0, nullptr);
  //vkCmdDrawIndexed( main_cmd_buf, 3*6, 1, 0, 0, desc_ofs );
  vkCmdDrawIndexed( main_cmd_buf, 3*6, 1, 0, 0, 0 );  // streamed + persistent

  desc_ofs = ( desc_ofs + 1 ) % max_nr_img_descriptors_in_set;
  local_desc_idx++;

  // One way we can use VK_EXT_descriptor_indexing is to treat the update-after-bind descriptor
  //   set as a ring buffer where we write descriptors,
  write_ds.dstArrayElement = desc_ofs;
  desc_img_info.imageView = cara_vk.albmap_ivs[local_desc_idx];
  vkUpdateDescriptorSets( cara_vk.log_dev, 1, &write_ds, 0, nullptr);
  //vkCmdDrawIndexed( main_cmd_buf, 3*2, 1, 3*6, 0, desc_ofs );
  vkCmdDrawIndexed( main_cmd_buf, 3*2, 1, 3*6, 0, 1 );  // streamed + persistent

  desc_ofs = ( desc_ofs + 1 ) % max_nr_img_descriptors_in_set;
  local_desc_idx++;







  SPALL_BEGIN("vkCmdEndRenderPass_1");
  // End the render pass
  vkCmdEndRenderPass(main_cmd_buf);  // Recording this function in a command buffer performs
                                  // multiple operations. Execution and memory dependencies are
                                  // introduced (like the ones in memory barriers) and image layout
                                  // transitions are performed—images are transitioned from layouts
                                  // specified for the last subpass to the value of a final layout
                                  // (refer to the Specifying attachment descriptions recipe). Also
                                  // multisample resolving is performed on color attachments for
                                  // which resolving was specified in the last subpass. Additionally
                                  // for attachments whose contents should be preserved after the
                                  // render pass, attachment data may be transferred from the cache
                                  // to the image's memory.
  SPALL_END();




  //
  // 2nd Renderpass
  //

  // The first thing we need to record after acquiring a swapchain image is to change the swapchain
  //  image's layout to a VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout.
  // This operation should be performed implicitly using appropriate render pass parameters (initial
  //  and sub-pass layouts). But if queues used for the presentation and graphics operations come
  //  from two different families, we must perform ownership transfer. This cannot be done
  //  implicitly—for this we need to set up an image memory barrier.
  // I guess setting VkAttachmentDescription2.initialLayout and VkAttachmentReference2.layout are
  //  enough to make the implicit transition happen
  //VkImageMemoryBarrier2KHR img_bar = {
  //  .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
  //  .pNext = nullptr,
  //  .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
  //  .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
  //  .srcAccessMask = VK_ACCESS_2_NONE_KHR,
  //  .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
  //  .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  //  .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  //  .srcQueueFamilyIndex = cara_vk.graphics_q_fam_idx,
  //  .dstQueueFamilyIndex = cara_vk.graphics_q_fam_idx,
  //// If the values of srcQueueFamilyIndex and dstQueueFamilyIndex are equal, no ownership transfer
  ////  is performed, and the barrier operates as if they were both set to VK_QUEUE_FAMILY_IGNORED.
  //  .image = cara_vk.arr_swapchain_imgs[img_idx],
  //  .subresourceRange = (VkImageSubresourceRange) {
  //    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
  //    .baseMipLevel = 0,
  //    .levelCount = 1,
  //    .baseArrayLayer = 0,
  //    .layerCount = 1,
  //  },
  //};

  //VkDependencyInfoKHR dependency_info;
  //dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
  //dependency_info.pNext = nullptr;
  //dependency_info.dependencyFlags =
  //  VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR |
  //  0;
  //dependency_info.memoryBarrierCount = 0;
  //dependency_info.pMemoryBarriers = nullptr;
  //dependency_info.bufferMemoryBarrierCount = 0;
  //dependency_info.pBufferMemoryBarriers = nullptr;
  //dependency_info.imageMemoryBarrierCount = 1;
  //dependency_info.pImageMemoryBarriers = &img_bar;

  //SPALL_BEGIN("barrier_b4_copy2swapchain");
  //vkCmdPipelineBarrier2KHR( main_cmd_buf, &dependency_info );
  //SPALL_END();

  arr_att_ivs[fb_zbuf] = cara_vk.zbuf_n_cbuf_iv[frame_id % cara_max_nr_fif][fb_zbuf];
  arr_att_ivs[fb_zbuf+1] = cara_vk.arr_swapchain_iv[img_idx];
  rp_begin_info.renderPass = cara_vk.arr_renderpasses[1];
  rp_begin_info.framebuffer = cara_vk.arr_framebuffers[frame_id % cara_max_nr_fif][1];
  SPALL_BEGIN("vkCmdBeginRenderPass");
  vkCmdBeginRenderPass( main_cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE );
  SPALL_END();

  auto sb_width = cara_vk.dims_swapchain_imgs.width;
  auto sb_height = cara_vk.dims_swapchain_imgs.height;
  //sb_width = min( sb_width, cur_win_dims.parts.width );
  //sb_height = min( sb_height, cur_win_dims.parts.height );

  VkViewport viewport2 = {
    .x = 0, .y = 0,
    .width = (float)sb_width,
    .height = (float)sb_height,
    .minDepth = 0,
    .maxDepth = 1.0f,
  };
  vkCmdSetViewport( main_cmd_buf, 0, 1, &viewport2 );

  VkRect2D scissor2 = {
    .offset = (VkOffset2D) { .x = 0, .y = 0 },
    .extent = (VkExtent2D) { .width = sb_width, .height = sb_height },
  };
  vkCmdSetScissor( main_cmd_buf, 0, 1, &scissor2 );

  //VkDeviceSize buf_ofs = 0;
  //vkCmdBindVertexBuffers( main_cmd_buf, 0, 1, &cara_vk.vert_buf, &buf_ofs );
  //vkCmdBindVertexBuffers( main_cmd_buf, 1, 1, &cara_vk.attr_buf, &buf_ofs );
  //vkCmdBindIndexBuffer( main_cmd_buf, cara_vk.ind_buf, 0, VK_INDEX_TYPE_UINT16 );



  //
  // Write on the descriptor
  //
  constexpr unsigned int nr_desc_sets_for_2nd_rp = 1;
  VkDescriptorImageInfo desc_img_info_cbuf = {
    .sampler = cara_vk.cbuf_sampler,
    .imageView = cara_vk.zbuf_n_cbuf_iv[frame_id % cara_max_nr_fif][fb_cbuf],
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet arr_desc_writes[nr_desc_sets_for_2nd_rp];
  arr_desc_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  arr_desc_writes[0].pNext = nullptr;
  arr_desc_writes[0].dstSet = cara_vk.ds_cp2sc[frame_id % cara_max_nr_fif];
  arr_desc_writes[0].dstBinding = 0;
  arr_desc_writes[0].dstArrayElement = 0;
  arr_desc_writes[0].descriptorCount = 1;
  arr_desc_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  arr_desc_writes[0].pImageInfo = &desc_img_info_cbuf;
  arr_desc_writes[0].pBufferInfo = nullptr;
  arr_desc_writes[0].pTexelBufferView = nullptr;
  SPALL_BEGIN("vkUpdateDescriptorSets");
  vkUpdateDescriptorSets( cara_vk.log_dev,
      _arr_elem_cnt( arr_desc_writes ), arr_desc_writes,  // VkWriteDescriptorSet
      0, nullptr );  // Copying from another descriptor set can be faster than normal writing.
  SPALL_END();


  //
  // Bind descriptor sets to the pipeline layout
  //
  SPALL_BEGIN("vkCmdBindDescriptorSets");
  vkCmdBindDescriptorSets( main_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, cara_vk.pll_cp2sc,
      0,  // The first set number
      1, &cara_vk.ds_cp2sc[frame_id % cara_max_nr_fif],  // Descriptor sets
      0, nullptr );  // Dynamic offsets
                     // If any of the sets being bound include dynamic uniform or storage buffers,
                     // then pDynamicOffsets includes one element for each array element in each
                     // dynamic descriptor type binding in each set. Values are taken from
                     // pDynamicOffsets in an order such that all entries for set N come before set
                     // N+1; within a set, entries are ordered by the binding numbers in the
                     // descriptor set layouts; and within a binding array, elements are in order
  SPALL_END();
  SPALL_BEGIN("vkCmdBindPipeline");
  vkCmdBindPipeline( main_cmd_buf,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      cara_vk.arr_pipelines[(unsigned int)cara_nr_subpasses + cs2_copy_to_sc] );
  SPALL_END();

  // TODO cs2_copy_to_sc
  vkCmdDraw( main_cmd_buf, 3, 1, 0, 0 );

  //SPALL_BEGIN("vkCmdNextSubpass");
  //vkCmdNextSubpass( main_cmd_buf, VK_SUBPASS_CONTENTS_INLINE );
  //SPALL_END();

  // We might still have to explicitly set up a barrier if the queue used for presenting was not the
  //  same queue used for draw commands.
  //img_bar.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
  //img_bar.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
  //img_bar.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR;
  //img_bar.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  //img_bar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  SPALL_BEGIN("vkCmdEndRenderPass_2");
  // End the render pass
  vkCmdEndRenderPass( main_cmd_buf );  // Recording this function in a command buffer performs
                                  // multiple operations. Execution and memory dependencies are
                                  // introduced (like the ones in memory barriers) and image layout
                                  // transitions are performed—images are transitioned from layouts
                                  // specified for the last subpass to the value of a final layout
                                  // (refer to the Specifying attachment descriptions recipe). Also
                                  // multisample resolving is performed on color attachments for
                                  // which resolving was specified in the last subpass. Additionally
                                  // for attachments whose contents should be preserved after the
                                  // render pass, attachment data may be transferred from the cache
                                  // to the image's memory.
  SPALL_END();



  // End recording
  SPALL_BEGIN("end_cmd_buf");
  result = vkEndCommandBuffer( main_cmd_buf );
  SPALL_END();
  if( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara] Error occurred ending command buffer recording.(1c) '%s'\n",
        string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }

  //
  // Prepare to submit the command buffer
  //
  VkSemaphoreSubmitInfoKHR sempho_wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .semaphore = cara_vk.arr_sc_acquire_semphos[frame_id % cara_max_nr_fif],
    //.value = VK_TRUE,
    //.semaphore = cara_vk.arr_semphos_gr[frame_id % cara_max_nr_fif],
    //.value = csb_swapchain_img_acquired + frame_id / cara_max_nr_fif * cara_nr_signs_gr,
    .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
    .deviceIndex = 0,
  };
  VkSemaphoreSubmitInfoKHR arr_sgnl_infos[] = {
    {  // WSI functions don't accept timeline semaphores, so a special binary semaphore goes here
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext = nullptr,
      .semaphore = cara_vk.arr_sc_present_semphos[img_idx],
      //.value = VK_TRUE,
      .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      .deviceIndex = 0,
    },
    {  // This semaphore will be used to indicate the command buffer is read through.
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext = nullptr,
      .semaphore = cara_vk.arr_semphos_gr[frame_id % cara_max_nr_fif],
      .value = csb_present_swapchain_img + frame_id / cara_max_nr_fif * cara_nr_signs_gr,
      .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      .deviceIndex = 0,
    },
  };
  VkCommandBufferSubmitInfoKHR arr_cmd_buf_submit_infos_2[] = {
    {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
      .pNext = nullptr,
      .commandBuffer = main_cmd_buf,
      .deviceMask = 0,
    },
  };
  submit_info = (VkSubmitInfo2) {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
    .pNext = nullptr,
    .flags = 0,
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos = &sempho_wait_info,
    .commandBufferInfoCount = _arr_elem_cnt(arr_cmd_buf_submit_infos_2),
    .pCommandBufferInfos = arr_cmd_buf_submit_infos_2,
    .signalSemaphoreInfoCount = _arr_elem_cnt(arr_sgnl_infos),
    .pSignalSemaphoreInfos = arr_sgnl_infos,
  };
  // Host access to `queue` must be externally synchronized
  // Host access to `fence` must be externally synchronized
  SPALL_BEGIN("submit_gr_cmds");
  result = vkQueueSubmit2KHR( cara_vk.graphics_q, 1, &submit_info, VK_NULL_HANDLE );
  SPALL_END();
  if ( result != VK_SUCCESS ) {
    fprintf( stderr, "[Carbonara-Debug] vkQueueSubmit2KHR failed.. (result=%s)\n",
        string_VkResult(result) );
    atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }



//-------------------------------------------------------------------------------------------------
  // This part stays in SDL_AppIterate or the graphics leader

  //
  // Present the rendered image
  //

  // TODO Before we can submit an image, we need to change its layout to a VK_IMAGE_LAYOUT_PRESENT_SRC_KHR or the presentation engine may be not able to correctly display such an image.

  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = nullptr,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &cara_vk.arr_sc_present_semphos[img_idx],
    .swapchainCount = 1,
    .pSwapchains = &cara_vk.swapchain,
    .pImageIndices = &img_idx,
    .pResults = nullptr,  // Applications that do not need per-swapchain results can use NULL for
                          //  pResults.
  };

  result = vkQueuePresentKHR( cara_vk.graphics_q, &present_info );


  // Destroy old swapchain if there's any
  for ( unsigned int i = 0; i < cara_vk.nr_old_swapchains; i++ ) {
    struct dead_swapchain * p_dead = infos_al_get_elem_unsafe( &cara_vk.old_swapchains, i );

    if ( p_dead->destruct_time > frame_id ) {
      continue;
    }

    SPALL_BEGIN("destroy_swapchain");
    vkDestroySwapchainKHR( cara_vk.log_dev, p_dead->swapchain, &vk_alloc_callbacks );
    SPALL_END();

    SPALL_BEGIN("destroy_framebuffer");
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
    SPALL_END();

    SPALL_BEGIN("destroy_zbuf_cbuf");
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
        err = ma_report_vk_malloc_rel( &cara_vk.vmm,
            &vk_alloc_callbacks, vk_malloc_wrapper,
            ma_pick_best_mtype(&cara_vk.vmm, &req,
              required_props, preferred_props, tolerable_props),
            p_dead->arr_mbi_zbuf_n_cbuf[fif_idx][fb_idx] );
        //err = ma_report_vk_malloc_rel( &cara_vk.vmm,
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
    SPALL_END();  // destroy_zbuf_cbuf

    struct dead_swapchain * p_last = infos_al_get_elem_unsafe(
        &cara_vk.old_swapchains, cara_vk.nr_old_swapchains - 1 );
    *p_dead = *p_last;
    i--;
    cara_vk.nr_old_swapchains--;

  }



  switch( result ) {
    case VK_SUBOPTIMAL_KHR:
      [[fallthrough]];
    case VK_SUCCESS:
      break;
    default:
      fprintf( stderr, "[Carbonara] Error presenting an image. '%s'\n", string_VkResult(result) );
      atomic_fetch_or_explicit( &cara_single.sign_to_end, 2, memory_order_release );
  }

  SPALL_END();
}
