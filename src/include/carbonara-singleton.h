#if !defined SALAMI_SINGLETON_H_
#define SALAMI_SINGLETON_H_


#include <stdint.h>
#include <stdatomic.h>
#include <SDL3/SDL_video.h>

#include "info_struct.h"
#include "memorable_arena.h"
#include "compute-magic.h"



//-------------------------------------------------------------------------------------------------
extern struct carbonara cara_single;


//-------------------------------------------------------------------------------------------------

// A simple function call stack for functions that take a single pointer-sized argument and return
//  a pointer-sized return value ((void(*)) func(void*))
struct func_call_stack_element {
  void (*func) (void*);
  void * arg;
};

struct carbonara {
  struct processor_map * proc_map;
  // struct infos_arr_list * job_arg_pool;
  struct infos_arr_list * job_list;  // contains cara_job_batch_desc_64
  memorable_arena_t * mem_arena;
  struct infos_stack * foremans_quit_stack;
  struct infos_bst * bst_action_times;
  uint32_t * periodic_jobs_periods_5us;
  uint16_t * periodic_jobs;
  static_assert( alignof(void*) >= alignof(uint64_t), "Sort the struct members!" );
  uint64_t proc_info;
  uint64_t nr_pc_in_5us;
  static_assert( alignof(uint64_t) >= alignof(struct unsigned_division_magic_64), "Sort the struct members!" );
  struct unsigned_division_magic_64 magic_divider;  ///< a magic divider for 5us unit timestamp
  //struct infos_linked_list grower_managers;
  static_assert( alignof(struct unsigned_division_magic_64) >= alignof(atomic_uint_least32_t),
      "Sort the struct members!" );
  atomic_uint_least32_t sign_to_end;  ///< Assign less than 4 to this field when you wanna keep the program running
  atomic_uint_least32_t sign_to_start;  ///< Assign less than 4 to this field when you wanna keep the program running
  uint32_t len_each_arena;
  static_assert( alignof(uint32_t) >= alignof(atomic_uint_least8_t), "Sort the struct members!" );
  atomic_uint_least8_t nr_workers_resigned;

  // 👆 Static

  // Not static
  alignas(CACHELINE_LEN)
  atomic_uint_least64_t cur_time_in_5us_general;  ///< Timestamp in 5us unit
  atomic_uint_least64_t cur_time_in_5us_for_render;
  atomic_uint_least64_t cur_time_in_5us_for_physics;
  atomic_uint_least64_t cur_time_in_5us_for_sim;
  atomic_uint_least8_t render_now;

  // Quite dynamic
  alignas(CACHELINE_LEN)
  atomic_uint_least16_t nr_job_batch_descriptors_taken;
  // atomic_uint_fast16_t nr_job_arg_groups_taken;
};


#endif // SALAMI_SINGLETON_H_
