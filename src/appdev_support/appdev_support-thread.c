#include <emmintrin.h>
#include <stdint.h>
#if defined(__linux)
#include <pthread.h>
#include <time.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <errno.h>  // Definition of for E* constants
#include <stdio.h>
#include <limits.h>

#include "appdev_support-thread.h"
#include "err_msg.h"
#include "util-c_lang-basic.h"


//-------------------------------------------------------------------------------------------------
#if defined(__linux)
// POSIX thread library
static int min_thread_priority;
static int max_thread_priority;
static pthread_attr_t thread_attr = {0};

#elif defined(_WIN32)
// Non-POSIX thread library

#endif


//-------------------------------------------------------------------------------------------------
#if defined(__clang__)
__attribute__ ((minsize))
#elif defined(__GNUC__) || defined(__GNUG__)
__attribute__ ((optimize("s")))
#endif
enum err_msg init_appdev_support_thread(void)
{
  if ( thread_attr.__align ) {
    return emsg_already_done;
  }

#if defined(__linux)
  // POSIX thread library
  int err;

  min_thread_priority = sched_get_priority_min(SCHED_FIFO);
  max_thread_priority = sched_get_priority_max(SCHED_FIFO);

  if ( (err = pthread_attr_init(&thread_attr)) ) {
    switch ( err ) {
      case ENOMEM:
        return emsg_out_of_memory;
      default:
        return emsg_unknown;
    }
  }

  // A stack overflow would occur in the initialization of SDL with 16384+2048
  // A stack overflow would occur in vkEnumerateInstanceExtensionProperties with 16384+8192
  // A stack overflow would occur in vf_load_instance_level_functions with 16384+16384
  // A stack overflow would occur in vf_load_instance_level_functions between
  //  (16384+1*32768)-(16384+2*32768)
  // A stack overflow would occur in vkCmdPipelineBarrier2KHR+Validation with 16384 + 3*16384
  // A stack deficiency would occur in vkCreateGraphicsPipelines with 2*32768
  //  I needed about 96KiB on the current mesa driver + shaders
  constexpr size_t default_stack_size = 128*1024;
  size_t desired_stack_size = default_stack_size;
  if ( desired_stack_size < PTHREAD_STACK_MIN ) {
    desired_stack_size = PTHREAD_STACK_MIN;
  }
  pthread_attr_setstacksize(&thread_attr,desired_stack_size);
  pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);
  //pthread_attr_setschedpolicy(&thread_attr, SCHED_FIFO);
  pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

#elif defined(_WIN32)
  // Non-POSIX thread library
#endif


  return 0;
}


//-------------------------------------------------------------------------------------------------
void remove_appdev_support_thread(void)
{
#if defined(__linux)
  if ( pthread_attr_destroy(&thread_attr) != 0 ) {
    //return err_msg_unknown;
  }
#elif defined(_WIN32)
#endif
}


//-------------------------------------------------------------------------------------------------
enum err_msg create_thread_( const struct create_thread_params params[static 1] )
{
  int err;
#if defined(__linux)
  // POSIX thread library
  assume( params->priority > 0 );

  struct sched_param sched_param;
  sched_param.sched_priority = (max_thread_priority - min_thread_priority) / params->priority
    + min_thread_priority;

  pthread_attr_setschedparam(&thread_attr, &sched_param);

  err = pthread_create(params->thread, &thread_attr, params->func, params->arg);

  if ( err ) {
    switch ( err ) {
      case EAGAIN:
        return emsg_out_of_resource;
      case EINVAL:
        return emsg_invalid_request;
      case EPERM:
        return emsg_permission_denied;
      default:
        return emsg_unknown;
    }
  }

#elif defined(_WIN32)
  // TODO: the Windows counterpart of POSIX thread
#endif
  return 0;
}

enum err_msg join_thread(thread thread, void ** retval) {
#if defined(__linux)
  // POSIX thread library
  int err;

  err = pthread_join(thread, retval);
  switch ( err ) {
    case EDEADLK:
      return emsg_deadlock_detected;
    case EINVAL:
      return emsg_invalid_request;
    case ESRCH:
      return emsg_wrong_name_given;
    default:
      return emsg_unknown;
    case emsg_no_error:
      break;
  }

#elif defined(_WIN32)
  // TODO: the Windows counterpart of POSIX thread
#endif
  return emsg_no_error;
}
