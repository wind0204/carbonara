#if !defined UTIL_SPALL_H_
#define UTIL_SPALL_H_

#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
    || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  #ifdef _MSC_VER
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
#endif

#if defined(ENABLE_PROFILING)
  #include <spall.h>

  extern SpallProfile spall_ctx;
  extern thread_local SpallBuffer * p_spall_buf;

  #define TRACE_BEGIN( name_tag ) spall_buffer_begin( &spall_ctx, p_spall_buf,  \
      (name_tag),                                                               \
      sizeof((name_tag)) - 1,                                                   \
      _rdtsc()                                                                  \
      );
  #define TRACE_END() spall_buffer_end( &spall_ctx, p_spall_buf, _rdtsc() );
#else  // defined(ENABLE_PROFILING)
  #define TRACE_BEGIN( name_tag )
  #define TRACE_END()
#endif  // defined(ENABLE_PROFILING)


#endif  // UTIL_SPALL_H_
