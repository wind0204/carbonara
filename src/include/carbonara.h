#if !defined CARBONARA_H_
#define CARBONARA_H_


#include <stdint.h>

//#include "appdev_support-processor_map.h"
#include "carbonara-mm.h"
#include "err_msg.h"




//-------------------------------------------------------------------------------------------------
//struct carbonara_linked_buffer {
//  struct infos_singly_linked_list buffers;
//  uint32_t total_length;
//};

struct cara_worker_state {
  alignas( CACHELINE_LEN )
  struct cara_job_batch_desc_64 * current_batch;
  //struct carbonara_malloc_helper_dynamic_bound * mhelper_dynamic_bound;
  //struct carbonara_linked_buffer * buffer;
  static_assert( alignof(void*) >= alignof(struct cara_malloc_helper), "Sort the struct members!" );
  struct cara_malloc_helper malloc_helper;
  static_assert( alignof(struct cara_malloc_helper) >= alignof(uint16_t) );
  uint16_t worker_idx;
};

struct carbonara_scene {
  uint16_t idx_first_job;
  uint8_t flags;
};



//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize the engine of carbonara.
 *
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
enum err_msg init_carbonara(uint32_t len_each_arena, uint64_t proc_info);
//uint8_t init_carbonara(struct carbonara_engine * carbonara);



#endif  // CARBONARA_H_
