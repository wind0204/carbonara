#include <spall.h>
#include "util-spall.h"

#if defined( ENABLE_PROFILING )

SpallProfile spall_ctx;
thread_local SpallBuffer * p_spall_buf;

#endif

