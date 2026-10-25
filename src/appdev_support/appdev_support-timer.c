#if defined(__linux)
#include <bits/posix_opt.h>
#include <linux/perf_event.h>  // Definition of PERF_* constants
#include <sys/syscall.h>  // Definition of SYS_* and __NR_* constants
#include <sys/mman.h>  // mmap
#elif defined(_WIN32)
#endif

#if !defined( _POSIX_MONOTONIC_CLOCK )
  #error POSIX CLOCK_MONOTONIC is not supported on the system.
#endif

#include <stdint.h>
#include <cpuid.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <SDL3/SDL_timer.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
    || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  #include "util-c_lang-x86.h"
#endif
#include "compute-magic.h"
#include "appdev_support-timer.h"
#include "err_msg.h"
#include "util-c_lang-x86.h"
#include "util-c_lang-basic.h"


//-------------------------------------------------------------------------------------------------
static constexpr char zeros[CACHELINE_LEN] = {0};
static uint64_t nr_cpu_cycles_per_1s;
static uint32_t max_tsc_diff;
static uint64_t nr_tsc_in_1ms;
static struct unsigned_division_magic_64 nano_divider = {0};  ///< a magic divider for one billion

uint64_t nr_tsc_in_5us;
uint64_t nr_tsc_in_1s;
double nr_tsc_in_pc;

// The length of time in seconds for the first test of timestamp counter frequency.
constexpr size_t TSC_FREQ_TEST_DURATION = 5;

//-------------------------------------------------------------------------------------------------
/**
 * \brief I made it use two arguments for the people who keep the computer over a decade.
 */
void (*delay_precise_busywait)( uint64_t starting_tsc, uint64_t len_in_tsc ) = 0;
void (*issue_mwait)( uint64_t starting_tsc, uint64_t len_in_tsc, void* monitored_zone ) = 0;

static void set_func_ptr(void);

static void busywait_pause( uint64_t starting_tsc, uint64_t len_in_tsc );
static void busywait_tpause( uint64_t starting_tsc, uint64_t len_in_tsc );
static void busywait_mwaitx( uint64_t starting_tsc, uint64_t len_in_tsc );
static void issue_umwait( uint64_t starting_tsc, uint64_t len_in_tsc, void* monitored_zone );
static void issue_mwaitx( uint64_t starting_tsc, uint64_t len_in_tsc, void* monitored_zone );

//-------------------------------------------------------------------------------------------------
static enum err_msg get_nr_tsc_in_1s_without_help_from_os(
    uint64_t proc_info, uint8_t dur_last_test );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Prepare the timing function
 */
enum err_msg init_appdev_support_timer(
    uint64_t proc_info, uint8_t dur_last_test)
{
  unsigned_division_magic_64(&nano_divider, 1000000000);

  set_func_ptr();

  return get_nr_tsc_in_1s_without_help_from_os(proc_info, dur_last_test);
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Read MSR information from /dev/cpu/0/msr
 * \a msr The register address for a specific information.
 * \a val The pointer to the variable to be written upon.
 * \return 0 if the file was not found, -1 if you couldn't read /dev/cpu/0/msr
 */
static int rdmsr(int msr, uint64_t *val) {
  const int fd = open("/dev/cpu/0/msr", O_RDONLY);

  // You need access to /dev/cpu/0/msr to determine this. (lsmod | grep msr)
  if (fd < 0) {
    switch ( errno ) {
      case EACCES:
        return -1;
      default:
        return 0;
    }
  }

  const int ret = pread(fd, val, sizeof(uint64_t), msr);

  close(fd);

  return ret;
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Determine nr_tsc_in_1s|1ms|5us|pc for the usage of rdtsc, mwaitx, umwait, tpause
 * \return enum err_msg or 0
 **/
static enum err_msg get_nr_tsc_in_1s_without_help_from_os(
    uint64_t proc_info, uint8_t prev_test_dur)
{
/*
  tsc_res_ns = 0;
  struct timespec res;
  int ret = clock_getres(CLOCK_MONOTONIC_RAW, &res);

  if ( ret ) {
    switch ( errno ) {
      case EINVAL:
        // The clockid specified is invalid for one of two reasons.
        // Either the System-V style hard coded positive value is out of
        // range, or the dynamic clock ID does not refer to a valid
        // instance of a clock object.
        return err_msg_invalid_request;
      default:
        return err_msg_unknown;
    }
  }

  if ( res.tv_sec ) return err_msg_too_long;

  tsc_res_ns = res.tv_nsec;
#if DEBUG_MODE
  fprintf(stdout, "[Timer] tsc_res_ns = %u\n", tsc_res_ns);
#endif
*/


  // As luck would have it, hwloc can't always provide the base frequency of the CPU

  // TODO Check how PCSX2 handles it

  uint32_t eax, ebx, ecx, edx;

  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = proc_info;

  uint64_t max_nonturbo_ratio = 0;
  uint8_t bus_freq = 0;
  if ( x86_proc_info.maxleaf >= 0x15 ) {
    intel_dialect_cpuid(0x15, eax, ebx, ecx, edx);
    if ( ebx && ecx ) {
      nr_cpu_cycles_per_1s = ((uint64_t)ecx * ebx) / eax;
      nr_tsc_in_1s = nr_cpu_cycles_per_1s;
      nr_tsc_in_1ms = (typeof(nr_tsc_in_1ms)) ( (__float128)nr_tsc_in_1s * (1.0/1000.0) );
      nr_tsc_in_5us = (typeof(nr_tsc_in_1ms)) ( (__float128)nr_tsc_in_1s * (5.0/1000000.0) );
      nr_tsc_in_pc = (typeof(nr_tsc_in_pc)) ( (__float128)nr_tsc_in_1s *
          ((__float128)1.0/SDL_GetPerformanceFrequency()) );
      goto tsc_freq_determined;
    }
  }

  switch ( x86_proc_info.model ) {
    // Westmere
    case 0x25:
    case 0x2C:
    case 0x2F:
    // Nehalem
    case 0x1E:
    case 0x1F:
    case 0x1A:
    case 0x2E:
      bus_freq = 133;
      break;

    // Goldmont
    case 0x5C:
    // Denverton
    case 0x5F:
      bus_freq = 100;
      break;

    default:
      if ( !x86_proc_info.avx ) {
        if ( x86_proc_info.maxleaf < 0x16 ) {
          // return err_msg_parse_error;
#ifdef DEBUG_MODE
        fprintf(stdout, "[Timer] Assume bus frequency 100Hz when running without AVX support "
            "and CPUID.0x16 is not supported either.\n");
#endif
          bus_freq = 100;
          break;
        }
#ifdef DEBUG_MODE
        fprintf(stdout,
            "[Timer] Utilizing CPUID.0x16, instead of CPUID.1.ECX[28] (AVX) or CPU model name.\n");
#endif
        ecx = 0;
        intel_dialect_cpuid(0x16, eax, ebx, ecx, edx);
        if ( ecx == 0 ) {
#ifdef DEBUG_MODE
          fprintf(stdout,
              "[Timer] CPUID.0x16 did not return bus frequency. "
              "Assume the lowest frequency (100Hz)\n");
#endif
          ecx = 100;
        }
        bus_freq = ecx & 0xffff;
        break;
      }

#ifdef DEBUG_MODE
        fprintf(stdout, "[Timer] Time to guess. Assume the lowest bus frequency. (100Hz)\n");
#endif
      bus_freq = 100;
  }

  // Try to grab `max_nonturbo_ratio << 8` from /dev/cpu/0/msr just in case we were previleged
  if ( rdmsr( 0xCE, &max_nonturbo_ratio ) <= 0 ) {
    // We couldn't read /dev/cpu/0/msr

    // Use our small database of x86 processors, or CPUID.16H.EAX[15-00]
    switch ( x86_proc_info.model ) {
      // i7-4790S
      case 0x3C:
        max_nonturbo_ratio = 32 << 8;
        break;

      default:
        // Try to utilize CPUID.16H.EAX[15-00]
        eax = 0;
        intel_dialect_cpuid(0x16, eax, ebx, ecx, edx);
        if ( eax == 0 ) {
#ifdef DEBUG_MODE
        fprintf(stdout, "[Timer] Assume the lowest non-turbo ratio. 20?\n");
#endif
          max_nonturbo_ratio = 20 << 8;
          break;
        }
#ifdef DEBUG_MODE
        fprintf(stdout, "[Timer] Utilizing CPUID.0x16 instead of MSR.0xCE[15:8]\n");
#endif
        max_nonturbo_ratio = ((eax & 0xffff) / bus_freq) << 8;
    }
  }
  max_nonturbo_ratio = (max_nonturbo_ratio >> 8) & 0xff;

  nr_cpu_cycles_per_1s = max_nonturbo_ratio * bus_freq * UINT64_C(1000000);
//base_freq_determined:

  // TODO Stop being verbose.
  fprintf(stdout, "[Timer] cpu_base_freq = %lu, max_nonturbo_ratio = %lu, bus_freq = %hhu\n",
      nr_cpu_cycles_per_1s, max_nonturbo_ratio, bus_freq);

  struct timespec spec, orig_spec;
  uint64_t new_tsc, starting_tsc;
  uint_fast8_t test_duration = prev_test_dur ?
    min(prev_test_dur + 1, TSC_FREQ_TEST_DURATION) : TSC_FREQ_TEST_DURATION;

//start_calibration:
  // TODO Stop being verbose.
  fprintf( stdout, "[Timer] Determining the frequency of the timestamp counter...\n" );
  fflush( stdout );
  uint64_t target_tsc = test_duration * nr_cpu_cycles_per_1s -
    nr_cpu_cycles_per_1s / (UINT64_C(1000)*1000*1000);
  uint64_t max_diff=0, min_diff=1000;
  { unsigned int IA32_TSC_AUX;
  starting_tsc = __rdtscp(&IA32_TSC_AUX); }
  target_tsc = target_tsc + starting_tsc;
  uint64_t cur_tsc=starting_tsc;
  clock_gettime(CLOCK_MONOTONIC_RAW, &orig_spec);
  hold_up_thread( test_duration * UINT64_C(1000) * 1000 * 1000 - 1000000 );
  while(-7) {
    { unsigned int IA32_TSC_AUX;
    new_tsc = __rdtscp(&IA32_TSC_AUX); }
    const uint64_t diff = new_tsc - cur_tsc;
    if ( diff < min_diff ) min_diff = diff;
    if ( diff > max_diff && diff < nr_cpu_cycles_per_1s / 2 ) max_diff = diff;

    cur_tsc = new_tsc;

    if ( new_tsc >= target_tsc ) {
      clock_gettime(CLOCK_MONOTONIC_RAW, &spec);
      if ( spec.tv_nsec - orig_spec.tv_nsec
          + (spec.tv_sec - orig_spec.tv_sec) * UINT64_C(1000) * 1000 * 1000
          >= ( UINT64_C(1000) * 1000 * 1000 * test_duration ) )
        break;
    }
  }

#ifdef DEBUG_MODE
  __float128 time_elapsed;
  time_elapsed = ( spec.tv_nsec - orig_spec.tv_nsec
      + (spec.tv_sec - orig_spec.tv_sec) * UINT64_C(1000) * 1000 * 1000 ) ;
  time_elapsed /= 1000*1000*1000;  // nano seceonds to seconds
#endif

//  if ( test_duration >= 2 && time_elapsed - test_duration > 0.0001
//      || time_elapsed - test_duration < -0.0001 ) {
//    test_duration--;
//#ifdef DEBUG_MODE
//    fprintf( stdout, "[Timer] time_elapsed = %lf second;  tsc_diff = %lu\n",
//      (double)time_elapsed, new_tsc - starting_tsc );
//#endif
//    goto start_calibration;
//  }
  //cpu_base_freq = (tsc - orig_tsc) / time_elapsed;
  //nr_cpu_cycles_per_1s = (typeof(nr_cpu_cycles_per_1s))
  //  ( (typeof(time_elapsed))(new_tsc - starting_tsc) / time_elapsed );
  typeof(nr_cpu_cycles_per_1s) bigger, smaller, test_result;
  test_result = (new_tsc - starting_tsc) / test_duration;

  fprintf( stdout, "[Timer] Time-stamp counter frequency test result: %lu Hz\n", test_result );
  if ( test_result > nr_cpu_cycles_per_1s ) {
    bigger = test_result;
    smaller = nr_cpu_cycles_per_1s;
  } else {
    bigger = nr_cpu_cycles_per_1s;
    smaller = test_result;
  }

  typeof(bigger) max_cpu_freq_err = 270u;
  if ( bigger - smaller > max_cpu_freq_err ) {
    fprintf( stderr, "[Timer] The measured frequency was too far from the estimated frequency"
        " (%lu - %lu > %lu)\n", bigger, smaller, max_cpu_freq_err );
    return emsg_corrupted_data_detected;
  }

  fprintf( stdout, "[Timer] Time-stamp counter frequency: %lu Hz\n", nr_cpu_cycles_per_1s );
#ifdef DEBUG_MODE
  fprintf( stdout, "[Timer] time_elapsed = %lf second;  tsc_diff = %lu\n",
      (double)time_elapsed, new_tsc - starting_tsc );
#endif

//#ifdef DEBUG_MODE
  fprintf( stdout, "[Timer] max_diff = %lu; min_diff = %lu;\n",
      max_diff, min_diff );
//#endif
  max_tsc_diff = max_diff;

  nr_tsc_in_1s = (typeof(nr_tsc_in_1s)) ( (__float128)(new_tsc - starting_tsc) *
    (1.0 / (__float128)test_duration) );
  nr_tsc_in_1ms = (typeof(nr_tsc_in_1ms)) ( (__float128)(new_tsc - starting_tsc) *
    (1.0 / (__float128)test_duration) * (1.0/1000.0) );
  nr_tsc_in_5us = (typeof(nr_tsc_in_5us)) ( (__float128)(new_tsc - starting_tsc) *
    (1.0 / (__float128)test_duration) * (1.0/1000000.0 * 5.0) );
  nr_tsc_in_pc = (typeof(nr_tsc_in_pc)) ( (__float128)(new_tsc - starting_tsc) *
    (1.0 / (__float128)test_duration) * ((__float128)1.0/SDL_GetPerformanceFrequency()) );
tsc_freq_determined:
#ifdef DEBUG_MODE
  fprintf( stdout, "[Timer] nr_tsc_in_1s = %lu; nr_tsc_in_pc = %f\n",
      nr_tsc_in_1s, nr_tsc_in_pc );
#endif

  return 0;
}

//-------------------------------------------------------------------------------------------------

// From advanced_threads_example_linux.c:
/*
   This is supporting code to read the RDTSC multiplier from perf on Linux,
   so we can convert from RDTSC's clock to microseconds.

   Using the RDTSC directly like this reduces profiler overhead a lot,
   which can save you a ton of time and improve the quality of your trace.

   For my i7-8559U, it takes ~20 ns to call clock_gettime, or ~6 ns to use rdtsc directly, which adds up!
   */
static long perf_event_open( struct perf_event_attr *hw_event,
    pid_t pid, int cpu, int group_fd, unsigned long flags )
{
  return syscall( __NR_perf_event_open, hw_event, pid, cpu, group_fd, flags );
}

static uint64_t mul_u64_u32_shr( uint64_t cyc, uint32_t mult, uint32_t shift )
{
  __uint128_t x = cyc;
  x *= mult;
  x >>= shift;
  return x;
}

uint64_t get_nr_tsc_in_1s_when_os_helps(void)
{
  struct perf_event_attr pe = {
    .type = PERF_TYPE_HARDWARE,
    .size = sizeof(struct perf_event_attr),
    .config = PERF_COUNT_HW_INSTRUCTIONS,
    .disabled = 1,
    .exclude_kernel = 1,
    .exclude_hv = 1,
  };

  const int fd = perf_event_open(&pe, 0, -1, -1, 0);
  if (fd == -1) {
    perror("[performance] perf_event_open failed");
    return 1;
  }
  void *addr = mmap(NULL, 4*1024, PROT_READ, MAP_SHARED, fd, 0);
  if (!addr) {
    perror("[performance] mmap failed");
    return 1;
  }
  struct perf_event_mmap_page *pc = addr;
  if (pc->cap_user_time != 1) {
    fprintf(stderr, "[performance] Perf system doesn't support user time\n");
    return 1;
  }

  uint64_t tsc_freq = mul_u64_u32_shr(1000000, pc->time_mult, pc->time_shift);
  return tsc_freq;
}


//-------------------------------------------------------------------------------------------------
inline uint64_t get_performance_counter()
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  uint64_t pc = now.tv_sec;
  pc *= 1000000000;
  pc += now.tv_nsec;

  return pc;
}


//-------------------------------------------------------------------------------------------------
void hold_up_thread( uint64_t nsec )
{
#if defined(__linux)
    struct timespec request;
    //request.tv_sec = ( nsec / 1000000000 );
    request.tv_sec = div_via_mul_64( &nano_divider, nsec );
    request.tv_nsec = ( nsec - request.tv_sec * 1000000000 );

    // Wait CLOCK_MONOTONIC to reach the target value.
    nanosleep(&request, NULL);
    // POSIX.1 specifies that nanosleep() should measure time against the CLOCK_REALTIME clock.
    // However, Linux measures the time using the CLOCK_MONOTONIC clock. This probably does not
    // matter, since the POSIX.1 specification for clock_settime(2) says that discontinuous
    // changes in CLOCK_REALTIME should not affect nanosleep()

#elif defined(_WIN32)
    // TODO windows and mac
#elif 0
#endif
//#if defined(__linux)
//  // POSIX clock_nanosleep
//  struct timespec remain, request;
//  struct timespec * p_req, * p_rem;
//
//  p_req = &request;
//  p_rem = &remain;
//  request.tv_sec = 0;
//  request.tv_nsec = nsec;
//
//  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, p_req, p_rem);
//  while ( ret ) {
//    if ( likely(ret == EINTR) ) {
//      _swap(&p_req, &p_rem);
//      ret = clock_nanosleep(CLOCK_MONOTONIC, 0, p_req, p_rem);
//
//    } else {
//      fprintf( stderr, "fail to clock_nanosleep! error number is %d", ret);
//      switch ( ret ) {
//        case EINVAL:
//          fprintf( stderr, "(EINVAL)\n");
//          break;
//        case ENOTSUP:
//          fprintf( stderr, "(ENOTSUP)\n");
//          break;
//        default:
//          fprintf( stderr, "(Error Code = %d)\n", ret);
//          break;
//      }
//      return;
//
//    }
//  }
//
//#elif defined(_WIN32)
//  // Non-POSIX sleep
//  Sleep(nsec/1000/1000);
//#elif 0 // TODO mac
//#endif
}

//-------------------------------------------------------------------------------------------------
void delay_precise( uint64_t target_pc )
{
  uint64_t cur_pc = get_performance_counter();
  const register uint64_t starting_tsc = _rdtsc();
#if !defined( ARE_WE_SHIPPING_IT )
  if ( target_pc < cur_pc ) {
    fprintf( stdout, "target_pc(%lu) < cur_pc(%lu)\n", target_pc, cur_pc );
  }
#endif
  double diff = target_pc < cur_pc ? .0 : (double)(target_pc - cur_pc);
  const register uint64_t len_in_tsc = (uint64_t)(diff * nr_tsc_in_pc);

  // The number of nanoseconds in 160us
  constexpr uint64_t short_nap_len = 1000 * 160;

  // Keep a record of max overshoot
  uint64_t max_nap_len = short_nap_len;
  
  while ( cur_pc + max_nap_len < target_pc ) {
    hold_up_thread( short_nap_len );

    const uint64_t new_pc = get_performance_counter();
    const uint64_t prev_nap_len = new_pc - cur_pc;
    if ( prev_nap_len > max_nap_len ) {
      max_nap_len = prev_nap_len;
    }
    cur_pc = new_pc;
  }

  if ( cur_pc < target_pc && (target_pc - cur_pc) > (max_nap_len - short_nap_len) ) {
    const uint64_t mini_nap_len = (target_pc - cur_pc) - (max_nap_len - short_nap_len);
    hold_up_thread( mini_nap_len );
    cur_pc = get_performance_counter();
  }

  while ( cur_pc + short_nap_len < target_pc ) {
    hold_up_thread( short_nap_len );
    cur_pc = get_performance_counter();
  }

  delay_precise_busywait( starting_tsc, len_in_tsc );
}

//-------------------------------------------------------------------------------------------------
static void set_func_ptr(void)
{
  uint32_t eax, ebx, ecx, edx;

  intel_dialect_cpuid( 0x7, eax, ebx, ecx, edx );
  if ( ecx & 1<<5 ) {  // Bit 05: WAITPKG
    delay_precise_busywait = busywait_tpause;
    issue_mwait = issue_umwait;
    return;
  }

  intel_dialect_cpuid( 0x80000001, eax, ebx, ecx, edx );
  if ( ecx & 1<<29 ) {  // Bit 29: MONITORX

    intel_dialect_cpuid( 0x5, eax, ebx, ecx, edx );
    if ( ecx & 1<<0 && ecx & 1<<1 ) {  // Bit 00: "EMX(enumerate MONITOR/MWAIT extensions)",
                                       // Bit 01: "IBE(interrupt break-event)"
      if ( (ebx & 0xffff) != CACHELINE_LEN ) {
        fprintf( stdout, "[Timer] Monitor line size is not equal to %d! (it's %u instead)\n",
            CACHELINE_LEN, ebx & 0xffff );
      } else {
        delay_precise_busywait = busywait_mwaitx;
        issue_mwait = issue_mwaitx;
        return;
      }
    }
  }

  delay_precise_busywait = busywait_pause;
}

//-------------------------------------------------------------------------------------------------
static void busywait_pause( uint64_t starting_tsc, uint64_t len_in_tsc )
{
  uint64_t new_tsc = _rdtsc();
  while( new_tsc - starting_tsc < len_in_tsc ) {
    _mm_pause();
    new_tsc = _rdtsc();
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__ ((target ("waitpkg")))
static void busywait_tpause( uint64_t starting_tsc, uint64_t len_in_tsc )
{
  uint64_t new_tsc = _rdtsc();
  const uint64_t real_target_tsc = starting_tsc + len_in_tsc;
  unsigned char cf = ~0;
  while( new_tsc - starting_tsc < len_in_tsc && cf ) {
    cf = _tpause(
        0b1,  // Bit 0 selects between a lower power (cleared) or faster wakeup (set) optimized
              //  state
        real_target_tsc );
    new_tsc = _rdtsc();
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__ ((target ("mwaitx")))
static void busywait_mwaitx( uint64_t starting_tsc, uint64_t len_in_tsc )
{
  uint64_t new_tsc = _rdtsc();
  const uint64_t real_target_tsc = starting_tsc + len_in_tsc;
  while ( new_tsc - starting_tsc < len_in_tsc ) {
    _mm_monitorx( (void*)zeros, 0, 0 );
    _mm_mwaitx(
        0b11,  // Bit 0: When set, allows interrupts to wake MWAITX, even when eFLAGS.IF = 0.
               // Bit 1: When set, EBX contains the maximum wait time expressed in Software P0
               //  clocks, the same clocks counted by the TSC.
        0xf0,  // Optimized C-state request is communicated through EAX[7:4]. The processor C-state
               //  is EAX[7:4]+1, so to request C0 is to place the value F in EAX[7:4] and to
               //  request C1 is to place the value 0 in EAX[7:4].  C0 for quick wake-ups
        real_target_tsc - new_tsc );
    new_tsc = _rdtsc();
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__ ((target ("mwaitx")))
static void issue_mwaitx( uint64_t starting_tsc, uint64_t len_in_tsc, void* monitored_zone )
{
  uint64_t new_tsc = _rdtsc();
  const uint64_t real_target_tsc = starting_tsc + len_in_tsc;
  while ( new_tsc - starting_tsc < len_in_tsc ) {
    _mm_monitorx( monitored_zone, 0, 0 );
    _mm_mwaitx(
        0b11,  // Bit 0: When set, allows interrupts to wake MWAITX, even when eFLAGS.IF = 0.
               // Bit 1: When set, EBX contains the maximum wait time expressed in Software P0
               //  clocks, the same clocks counted by the TSC.
        0x00,  // Optimized C-state request is communicated through EAX[7:4]. The processor C-state
               //  is EAX[7:4]+1, so to request C0 is to place the value F in EAX[7:4] and to
               //  request C1 is to place the value 0 in EAX[7:4].  C0 for quick wake-ups
        real_target_tsc - new_tsc );
    new_tsc = _rdtsc();
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__ ((target ("waitpkg")))
static void issue_umwait( uint64_t starting_tsc, uint64_t len_in_tsc, void* monitored_zone )
{
  uint64_t new_tsc = _rdtsc();
  const uint64_t real_target_tsc = starting_tsc + len_in_tsc;
  unsigned char cf = ~0;
  while ( new_tsc - starting_tsc < len_in_tsc && cf ) {
    _umonitor( monitored_zone );
    cf = _umwait(
        0,  // Bit 0 of ctrl selects between a lower power (cleared) or faster wakeup (set)
            //  optimized state.
        real_target_tsc );
    new_tsc = _rdtsc();
  }
}


