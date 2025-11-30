#if !defined APPDEV_SUPPORT_THREAD_H_
#define APPDEV_SUPPORT_THREAD_H_


#if defined(__linux)
#include <pthread.h>
#elif defined(_WIN32)
//#include <Windows.h>
#include <windows.h>
#endif

#include <stdint.h>

#include "err_msg.h"


//-------------------------------------------------------------------------------------------------
#if defined(__linux)
typedef pthread_t thread;
#elif defined(_WIN32)
typedef HANDLE thread;
#endif

struct create_thread_params {
  thread * restrict thread;
  void * (* func)(void*);
  void * restrict arg;
  int priority;  ///< param priority The lower it is, the higher the thread's priority will be.
                 /// The appropriate range is 2-4 and the minimum value is 1 (highest priority).
};


//-------------------------------------------------------------------------------------------------
/**
 * \brief You must call this function before using the thread-related functions
 *
 * @return an enum err_msg.
 */
enum err_msg init_appdev_support_thread(void);

//-------------------------------------------------------------------------------------------------
void remove_appdev_support_thread(void);


//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a thread
 * param priority The lower it is, the higher the thread's priority will be.
 *   The appropriate range is 2-4 and the minimum value is 1 (highest priority).
 * param thread a pointer to typeof(thread) that will be populated.
 * param func The start routine.
 * param arg The argument to pass over to the start routine.
 * \return an enum err_msg on failure or 0 on success.
 */
enum err_msg create_thread_( const struct create_thread_params params[static 1] );
#define create_thread(...) CWPS_WITH_DEF_VAL_WO_HEADER(create_thread, \
    V(.priority=3), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * TODO Would there be no retval in win32?
 */
enum err_msg join_thread( thread thread, void ** retval );

#endif // APPDEV_SUPPORT_THREAD_H_
