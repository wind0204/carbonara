#include <stdatomic.h>
#include <stdint.h>
#include <unistd.h>

#include "carbonara-job.h"
#include "carbonara-singleton.h"
#include "list-jobs.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif


//-------------------------------------------------------------------------------------------------

static_assert(true);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types-strict"
// Each element of this array is a job functiona and the index of each element is
// an enum job_func_indices declared in list-jobs.h
void (* const known_jobs[])( struct cara_worker_state * p_ws ) = {
  0,  // #0
  jf_maint_job_list,
  jf_maint_memo_maps,
  jf_start_rainbow_scene,
  jf_update_rainbow_scene,
  jf_end_rainbow_scene,
  jf_start_kill_chain,
  jf_kickstart,

  jf_police_patrol,  // #8
  jf_render_frame,
#pragma GCC diagnostic pop
};


//-------------------------------------------------------------------------------------------------
struct cara_job_batch_desc_64 * cara_get_job( const struct ms_worker_info * worker_info )
{
    //struct cara_job_batch_desc_64 * buffer[NR_PTRS_IN_BUFFER_FOR_GET_JOB]) {
  /*
  uint8_t cnt_buffer_index = 0;
  for ( uint_fast8_t idx_buf_ptr = 0;
      idx_buf_ptr < NR_PTRS_IN_BUFFER_FOR_GET_JOB; idx_buf_ptr++ ) {
    buffer[idx_buf_ptr]->progress = 0;
    buffer[idx_buf_ptr]->preferred_core = 0;
    for ( uint_fast8_t i = 0; i < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64; i++ ) {
      buffer[idx_buf_ptr]->idx_job_func[i] = 0;
      buffer[idx_buf_ptr]->job_flags[i] = 0;
      for ( uint_fast8_t j = 0; j < CARBONARA_MAX_LEN_JOB_FUNC_ARGS; j++ )
        buffer[idx_buf_ptr]->job_func_args[i*CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64 + j] = 0;
    }
    for ( uint_fast8_t i = 0; i < sizeof(buffer[0]->user_data); i++ )
      buffer[idx_buf_ptr]->user_data[i] = 0;
  }
  */

  /*
  uint32_t clsets = worker_info->cache_pattern >> worker_info->cache_location;
  uint32_t reversed_clsets = ~clsets;
  uint32_t clsets_sibling = 0;
  if ( worker_info->idx_one_sibling && !worker_info->idx_missing_sibling ) {
    struct ms_worker_info * sibling_info;
    sibling_info = &cara_single.proc_map->processors[worker_info->idx_one_sibling - 1];
    clsets_sibling = sibling_info->cache_pattern >> sibling_info->cache_location;
  }
  */
  const uint64_t nr_elems = UINT64_C(1) << cara_single.job_list->log2_nr_elems;
  const struct infos_singly_linked_list_node * node = &cara_single.job_list->list.head;
  // Try to find a job that doesn't have anything to do with the datasets on the cache
  // line sets the worker is not assigned to
  while ( node ) {
    for ( uint64_t idx_elem = 0; idx_elem < nr_elems; idx_elem++ ) {
      struct cara_job_batch_desc_64 * restrict batch =
        node->data + (idx_elem << cara_single.job_list->log2_elem_size);
      uint_fast8_t try_to_take = 1;

      const uint_fast8_t preferred_core =
        atomic_load_explicit( &batch->preferred_core, memory_order_acquire );
      if ( preferred_core != UINT8_MAX ) {  // The job is from a duo job pair.
        if ( preferred_core != UINT8_MAX - 1 &&  // preferred_core of the pair has been determined.
            worker_info->idx_core != preferred_core ) {  // The worker is not assigned to the core
          try_to_take = 0;
        }
      }

      if ( try_to_take ) {
        if ( !(worker_info->deployment_flags & job_flag_foremans_task) ) {
          for ( uint_fast8_t idx_job = 0; idx_job < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
              idx_job++ ) {
            if ( batch->job_flags[idx_job] & job_flag_foremans_task ) {
              try_to_take = 0;
              break;
            }
          }
        }

        if ( try_to_take ) {
          uint8_t old_value = 0;
          if ( preferred_core == UINT8_MAX - 1 ) {
            struct cara_job_batch_desc_64 * restrict const peer =
              infos_al_get_elem( cara_single.job_list, batch->idx_peer_job );
            // Try to take the batch and set preferred_core
            if ( atomic_compare_exchange_strong_explicit(&batch->progress,
                  &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
              atomic_store_explicit(
                  &peer->preferred_core, worker_info->idx_core, memory_order_release );
              return batch;
            }
          } else {
            // Try to take the batch
            if ( atomic_compare_exchange_strong_explicit(&batch->progress,
                  &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
              return batch;
            }
          }
        }
      }
    }

    node = node->next;
  }

  /*
  // Try to find a job that at least doesn't involve the datasets on the cache line sets the
  // sibling of this worker is assigned to
  if ( clsets_sibling ) {
    for ( uint_fast8_t i = 0; i < 2; i++ ) {
      for ( uint_fast8_t idx_buf_ptr = 0;
          idx_buf_ptr < NR_PTRS_IN_BUFFER_FOR_GET_JOB; idx_buf_ptr++ ) {
        struct cara_job_batch_desc_64 * restrict batch = buffer[idx_buf_ptr];

        if ( !batch ) {
          continue;
        }

        uint8_t try_to_take = 1;
        for ( uint_fast8_t idx_job = 0; idx_job < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
            idx_job++ ) {
          if ( batch->cache_pattern[idx_job] >> batch->cache_location[idx_job] & clsets_sibling ) {
            try_to_take = 0;
            buffer[idx_buf_ptr] = 0;
            break;
          }
        }

        if ( try_to_take ) {
          uint8_t old_value = 0;
          // Try to take the batch
          if ( atomic_compare_exchange_strong_explicit(&batch->progress,
                &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
            return batch;
          }
        }
      }
    }
  }
  */

  return 0;
}


//-------------------------------------------------------------------------------------------------
struct cara_job_batch_desc_64 * cara_get_unoccupied_job_batch_desc(void)
{
  const uint64_t nr_elems = UINT64_C(1) << cara_single.job_list->log2_nr_elems;
  const struct infos_singly_linked_list_node * node = &cara_single.job_list->list.head;
  while ( node ) {
    for ( uint_fast16_t idx_elem = 0; idx_elem < nr_elems; idx_elem++ ) {
      struct cara_job_batch_desc_64 * restrict batch =
        node->data + (idx_elem << cara_single.job_list->log2_elem_size);
      // Try to take the job batch descriptor if every job in it is done.
      uint8_t old_value = ~0;
      if ( atomic_load_explicit(&batch->progress, memory_order_acquire) == old_value ) {
        if ( atomic_compare_exchange_strong_explicit(&batch->progress,
              &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
          atomic_fetch_add_explicit( &cara_single.nr_job_batch_descriptors_taken, 1,
              memory_order_release );
          return batch;
        }
      }

    }

    node = node->next;
  }

  return 0;
}

//struct cara_job_batch_desc_64 * cara_get_unoccupied_job_batch_desc_colored(uint32_t cacheline_sets) {
//  uint_fast8_t nr_0s = _tzcnt_u32(cara_single.proc_map->set_index_mask);
//  uint_fast8_t nr_elems;
//  uint_fast8_t elem_size;
//  nr_elems = cara_single.job_list->elem_cnt_per_node;
//  elem_size = cara_single.job_list->elem_size;
//  struct infos_singly_linked_list_node * node = &cara_single.job_list->list.head;
//  while ( node ) {
//    for ( uint_fast8_t idx_elem = 0; idx_elem < nr_elems; idx_elem++ ) {
//      struct cara_job_batch_desc_64 * restrict batch = node->data + idx_elem*elem_size;
//
//      // Try to take the job batch descriptor if the jobs inside are all done.
//      uint8_t old_value = ~0;
//      if ( atomic_load_explicit(&batch->progress, memory_order_acquire) == old_value &&
//          0x80000000u >> ((ptr2ui(batch) & cara_single.proc_map->set_index_mask) >> nr_0s) & cacheline_sets ) {
//        if ( atomic_compare_exchange_strong_explicit(&batch->progress,
//              &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
//          atomic_fetch_add_explicit(&cara_single.nr_job_batch_descriptors_taken, 1,
//              memory_order_release);
//          return batch;
//        }
//      }
//
//    }
//
//    node = node->next;
//  }
//
//  return 0;
//}


//-------------------------------------------------------------------------------------------------
void cara_init_job_list_node( void * data )
{
  const uint64_t nr_elems = UINT64_C(1) << cara_single.job_list->log2_nr_elems;
  for ( uint_fast16_t idx_elem = 0; idx_elem < nr_elems; idx_elem++ ) {
    struct cara_job_batch_desc_64 * restrict batch =
      data + (idx_elem << cara_single.job_list->log2_elem_size);

    atomic_init( &batch->progress, ~0 );
    atomic_init( &batch->preferred_core, ~0 );
    for ( uint_fast8_t idx_job = 0; idx_job < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
        idx_job++ ) {
      batch->job_flags[idx_job] = 0;
    }
    //for ( uint_fast8_t idx_job = 0; idx_job < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
    //    idx_job++ ) {
    //  batch->jobs[idx_job].idx_job_func = 0;
    //  batch->cache_locations[idx_job] = 0;
    //  batch->cache_patterns[idx_job] = 0;
    //  for ( uint_fast8_t idx_byte = 0; idx_byte < __sizeof_member(struct cara_job_batch_desc_64, user_data);
    //      idx_byte++ ) {
    //    batch->user_data[idx_byte] = 0;
    //  }
    //}
  }

}

/*
void cara_init_job_arg_pool_node(void * data) {

  uint8_t nr_elems;
  nr_elems = cara_single.job_arg_pool->node_size / cara_single.job_arg_pool->elem_size;
  typeof(cara_single.job_arg_pool->elem_size) elem_size = cara_single.job_arg_pool->elem_size;
  for ( uint_fast8_t idx_elem = 0; idx_elem < nr_elems; idx_elem++ ) {
    struct cara_job_func_args_64 * restrict func_arg_group = data + idx_elem*elem_size;

    atomic_init(&func_arg_group->occupied, 0);
  }

}
*/

/*
uint16_t cara_get_unoccupied_job_func_args(void) {
  uint8_t nr_elems;
  uint8_t elem_size;
  nr_elems = cara_single.job_arg_pool->node_size / cara_single.job_arg_pool->elem_size;
  elem_size = cara_single.job_arg_pool->elem_size;
  struct infos_linked_list_node * node = cara_single.job_arg_pool->list->head;
  uint_fast8_t idx_node = 0;
  while ( node ) {
    for ( uint_fast8_t idx_elem = 0; idx_elem < nr_elems; idx_elem++ ) {
      struct cara_job_func_args_64 * restrict arg_group = node->data + idx_elem*elem_size;
      // Try to take the job function argument group.
      uint8_t old_value = 0;
      if ( arg_group->occupied == old_value ) {
        if ( atomic_compare_exchange_strong_explicit(&arg_group->occupied,
              &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
          atomic_fetch_add_explicit(&cara_single.nr_job_arg_groups_taken, 1,
              memory_order_release);
          return nr_elems * idx_node + idx_elem;
        }
      }

    }

    node = node->next;
    idx_node++;
  }

  return UINT16_MAX;
}
*/

/*
uint16_t cara_get_unoccupied_job_func_args_colored(uint32_t cacheline_sets) {
  uint_fast8_t nr_0s = _tzcnt_u32(mm_singleton.set_index_mask);
  uint8_t nr_elems;
  uint8_t elem_size;
  nr_elems = cara_single.job_arg_pool->node_size / cara_single.job_arg_pool->elem_size;
  elem_size = cara_single.job_arg_pool->elem_size;
  struct infos_linked_list_node * node = cara_single.job_arg_pool->list->head;
  uint_fast8_t idx_node = 0;
  while ( node ) {
    for ( uint_fast8_t idx_elem = 0; idx_elem < nr_elems; idx_elem++ ) {
      struct cara_job_func_args_64 * restrict arg_group = node->data + idx_elem*elem_size;

      // Try to take the job function argument group.
      uint8_t old_value = 0;
      if ( arg_group->occupied == old_value &&
          0x80000000u >> ((ptr2ui(arg_group) & mm_singleton.set_index_mask) >> nr_0s) & cacheline_sets ) {
        if ( atomic_compare_exchange_strong_explicit(&arg_group->occupied,
              &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
          atomic_fetch_add_explicit(&cara_single.nr_job_arg_groups_taken, 1,
              memory_order_release);
          return nr_elems * idx_node + idx_elem;
        }
      }

    }

    node = node->next;
    idx_node++;
  }

  return UINT16_MAX;
}
*/


