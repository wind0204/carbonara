#include "info_struct.h"
#if !defined CARBONARA_JOB_H_
# define CARBONARA_JOB_H_

#include <stdatomic.h>
#if ATOMIC_CHAR_LOCK_FREE == 1
#error stdatomic is not really lock-free. it's most likely atomic char is implemented with \
  mutexes or semaphores.
#elif ATOMIC_CHAR_LOCK_FREE == 0
#error stdatomic is never lock-free.
#endif

#include <stdint.h>

#include "appdev_support-processor_map.h"
#include "util-c_lang-basic.h"
#include "carbonara-asset.h"
#include "carbonara-mm.h"


//-------------------------------------------------------------------------------------------------
constexpr size_t cara_len_job_func_param = 8;
//#define NR_PTRS_IN_BUFFER_FOR_GET_JOB 4

// Type casting to a different function signature can result undefined behavior.
/**
 * \brief It describes what kind of arguments the job function will take
 */
//enum job_fsig : unsigned char {
//  job_fsig_void = 0,  ///< The function won't take any argument except
//                            ///< one pointer to struct worker_state.
//  job_fsig_ui32,
//  job_fsig_ui64,
//  job_fsig_i32,
//  job_fsig_i64,
//  job_fsig_float,
//  job_fsig_double,
//  job_fsig_ptr,  ///< The function will take a void pointer as an argument.
//  job_fsig_ui8 = 8,
//  job_fsig_ui16,
//  job_fsig_i8,
//  job_fsig_i16,
//};
// Each element of this array is an enum job_func_sig declared in carbonara-job.h
//constexpr enum job_fsig job_fsig_db[] = {
//  0,
//  job_fsig_void,
//  job_fsig_void,
//  job_fsig_void,
//  job_fsig_void,
//  job_fsig_void,
//  job_fsig_void,
//  job_fsig_void,  // #7
//};

/**
 * \brief It describes the properties of the job
 */
enum job_flag : unsigned char {
  job_flag_contact_gpu = 0x01,
  job_flag_contact_nic = 0x02,
  job_flag_foremans_task = 0x04,
};


//-------------------------------------------------------------------------------------------------
struct cara_worker_state {
  struct cara_io_worker io_work;
  static_assert( alignof(struct cara_io_worker) >= alignof(void*) );
  struct cara_job_batch_desc_64 * current_batch;
  //struct carbonara_malloc_helper_dynamic_bound * mhelper_dynamic_bound;
  //struct carbonara_linked_buffer * buffer;
  static_assert( alignof(void*) >= alignof(struct cara_malloc_helper) );
  struct cara_malloc_helper malloc_helper;
  static_assert( alignof(struct cara_malloc_helper) >= alignof(uint16_t) );
  uint16_t worker_idx;
};

//-------------------------------------------------------------------------------------------------
extern void (* const known_jobs[])( struct cara_worker_state * p_ws );

//-------------------------------------------------------------------------------------------------
/**
 * \brief This struct describes a batch of jobs
 */
struct cara_job_batch_desc_64 {
  atomic_uint_least8_t progress;  ///< 1 when it's taken by a worker, 0 when it's not taken by any.
                                  ///< The value will be ~0 when every job in the batch is done and
                                  ///< when initialized by cara_init_job_list_node.
                                  ///< cara_get_unoccupied_job_batch_desc will look for progress=~0
  atomic_uint_least8_t preferred_core;  ///< Preferred CPU core value for ms_worker_info->idx_core
                                        ///< for a duo job pair.
                                  ///< ~0 is assigned if the jobs are not for duo.
                                  ///< UINT8_MAX-1 is assigned hen the jobs are for duo and the
                                  ///<  preferred core hasn't been determined.
  uint8_t job_func_params[6 * cara_len_job_func_param];  ///< The first argument for each
                                                              ///< function is the biggest and its
                                                              ///< position should be aligned
                                                              ///< accordingly.
  // The first element in this array is the enum job_func_idx of the first job to get done.
  uint8_t job_func_idx[6];  ///< enum job_func_idx  XXX Just 255 types? FIXME
  uint8_t job_flags[6];  ///< enum job_flag;
  //uint8_t pattern_len[6];
  /*
  uint8_t cache_location[6];  ///< This determines the cache line set matches with the first bit in cache_patterns[n].
  uint8_t cache_stride[6];
  uint8_t idx_starting_bit_in_pattern[6]; ///< It tells which bit in cache_pattern matches the
                                          ///< starting point of the data set of the job
  */
  uint8_t idx_peer_job;   ///< The index of the other partner job in the duo job pair.
                          ///< It's the index of an element in cara_single.job_list.
  uint8_t user_data[ 64 - 2*sizeof(atomic_uint_least8_t) - sizeof(uint8_t)
    - 6 * cara_len_job_func_param * sizeof(uint8_t)
    - 6 * 2 * sizeof(uint8_t) ];  ///< Use it on whatever purpose.
};
static_assert( sizeof(struct cara_job_batch_desc_64) == 64,
    "'64' in the name of the struct means it's 64 bytes long." );
constexpr size_t CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64 =
  _sizeof_member( struct cara_job_batch_desc_64, job_flags );


//-------------------------------------------------------------------------------------------------
void cara_init_job_list_node( struct infos_arr_list * p_al );

//-------------------------------------------------------------------------------------------------
struct cara_job_batch_desc_64 * cara_get_job( const struct ms_worker_info * worker_info );
//struct cara_job_batch_desc_64 * cara_get_job(const struct ms_worker_info * worker_info,
//    struct cara_job_batch_desc_64 * buffer[NR_PTRS_IN_BUFFER_FOR_GET_JOB]);

//-------------------------------------------------------------------------------------------------
struct cara_job_batch_desc_64 * cara_get_unoccupied_job_batch_desc(void);
//struct cara_job_batch_desc_64 * cara_get_unoccupied_job_batch_desc_colored(uint32_t cacheline_sets);



/**
 * This struct contains the arguments that will be fed to the job_functions in a job_batch
 */
/*
struct cara_job_func_args_64 {
  uint8_t data[64-1];
  atomic_uint_least8_t occupied;  ///< 1 when it's linked to a job batch descriptor, 0 when it's not linked to any.
};
*/

//void cara_init_job_arg_pool_node(void * data);
/*
 * @return UINT16_MAX when it couldn't find any argument group index to return
 */
//uint16_t cara_get_unoccupied_job_func_args(void);
/*
 * @return UINT16_MAX when it couldn't find any argument group index to return
 */
//uint16_t cara_get_unoccupied_job_func_args_colored(uint32_t cacheline_sets);



#endif  // CARBONARA_JOB_H_
