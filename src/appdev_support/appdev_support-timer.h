#if !defined APPDEV_SUPPORT_TIMER_H_
#define APPDEV_SUPPORT_TIMER_H_

#include <stdint.h>

#include "err_msg.h"


//-------------------------------------------------------------------------------------------------
extern uint64_t nr_tsc_in_1s;
extern uint64_t nr_tsc_in_5us;
extern double nr_tsc_in_pc;
extern void (*delay_precise_busywait)( uint64_t starting_tsc, uint64_t len_in_tsc );
extern void (*issue_mwait)( uint64_t starting_tsc, uint64_t len_in_tsc, void* monitored_zone );

//-------------------------------------------------------------------------------------------------
enum err_msg init_appdev_support_timer( uint64_t proc_info, uint8_t dur_last_test );

//-------------------------------------------------------------------------------------------------
uint64_t get_nr_tsc_in_1s_when_os_helps(void);

//-------------------------------------------------------------------------------------------------
/**
 * \brief delay the thread.
 * \param nsec The length of time to hold up in nano seconds.
 * \warning \a nsec MUST be less than 1,000,000,000
 */
void hold_up_thread( uint64_t nsec );

//-------------------------------------------------------------------------------------------------
/**
 * \brief delay the thread with enhanced precision.
 * \param target_pc The target performance counter at which the thread should be woken up.
 */
void delay_precise( uint_fast32_t target_pc );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Get monotonic timestamp in nano seconds.
 */
uint64_t get_performance_counter();

/**
 * \brief Explicitly show the frequency in performance counter per second.
 */
__attribute__((always_inline))
static inline uint64_t get_performance_frequency()
{
  return 1000000000;
}



#endif // APPDEV_SUPPORT_TIMER_H_
