#include <stdatomic.h>
#include <stdint.h>
#include <unistd.h>

#include "carbonara-job.h"
#include "carbonara-singleton.h"
#include "info_struct.h"
#include "list-jobs.h"
#include "util-c_lang-basic.h"

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
  //jf_render_frame,
#pragma GCC diagnostic pop
};


//-------------------------------------------------------------------------------------------------
struct cara_job_batch_desc_64 * cara_get_job( const struct ms_worker_info * worker_info )
{
  struct infos_arr_list * p_al = cara_single.job_list;
  const uint64_t nr_elems_in_block = UINT64_C(1) << p_al->log2_elem_cnt;
  //const uint64_t nr_valid_elems = nr_elems_in_block * p_al->active_blocks_cnt;
  //uint64_t base_idx = 0;  // The base idx of current vacancy block
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict node = &p_al->list.head;
  do {  // while ( base_idx < nr_valid_elems )
    //if ( base_idx + block_idx * nr_elems_in_block >= nr_valid_elems ) {
    //  goto out_of_nested_loop;
    //}
    void * block = node->arr_blocks[block_idx];

    for ( uint_fast32_t local_elem_idx = 0;
        local_elem_idx < nr_elems_in_block;
        local_elem_idx++ ) {

      struct cara_job_batch_desc_64 * restrict p_batch = block + local_elem_idx * sizeof(*p_batch);
      uint_fast8_t try_to_take = 1;

      const uint_fast8_t preferred_core =
        atomic_load_explicit( &p_batch->preferred_core, memory_order_acquire );
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
            if ( p_batch->job_flags[idx_job] & job_flag_foremans_task ) {
              goto continue_to_next_elem;
            }
          }
        }

        uint8_t old_value = 0;
        if ( preferred_core == UINT8_MAX - 1 ) {
          struct cara_job_batch_desc_64 * restrict const peer =
            infos_al_get_elem_unsafe( cara_single.job_list, p_batch->idx_peer_job );
          // Try to take the p_batch and set preferred_core
          if ( atomic_compare_exchange_strong_explicit(&p_batch->progress,
                &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
            atomic_store_explicit(
                &peer->preferred_core, worker_info->idx_core, memory_order_release );
            return p_batch;
          }
        } else {
          // Try to take the p_batch
          if ( atomic_compare_exchange_strong_explicit(&p_batch->progress,
                &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
            return p_batch;
          }
        }  // if ( preferred_core == UINT8_MAX - 1 ) {} else
      }  // if ( try_to_take )
continue_to_next_elem:

    }  // for ( uint_fast32_t local_elem_idx = 0;
       // local_elem_idx < nr_elems_in_block; local_elem_idx++ )

    block_idx++;
    //base_idx += nr_elems_in_block;
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( block_idx < p_al->active_block_cnt );
//out_of_nested_loop:

  return nullptr;
}


//-------------------------------------------------------------------------------------------------
struct cara_job_batch_desc_64 * cara_get_unoccupied_job_batch_desc(void)
{
  struct infos_arr_list * p_al = cara_single.job_list;
  const uint64_t nr_elems_in_block = UINT64_C(1) << p_al->log2_elem_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict node = &p_al->list.head;
  do {  // while ( base_idx < nr_valid_elems )
    void * block = node->arr_blocks[block_idx];

    for ( uint_fast32_t local_elem_idx = 0;
        local_elem_idx < nr_elems_in_block;
        local_elem_idx++ ) {

      struct cara_job_batch_desc_64 * restrict p_batch = block + local_elem_idx * sizeof(*p_batch);
      // Try to take the job batch descriptor if every job in it is done.
      uint8_t old_value = ~0;
      if ( atomic_load_explicit(&p_batch->progress, memory_order_acquire) == old_value ) {
        if ( atomic_compare_exchange_strong_explicit(&p_batch->progress,
              &old_value, 1, memory_order_acq_rel, memory_order_acquire) ) {
          atomic_fetch_add_explicit( &cara_single.nr_job_batch_descriptors_taken, 1,
              memory_order_release );
          return p_batch;
        }
      }
    }  // for ( uint_fast32_t local_elem_idx = 0;
       // local_elem_idx < nr_elems_in_block; local_elem_idx++ )

    block_idx++;
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( block_idx < p_al->active_block_cnt );

  return nullptr;

}


//-------------------------------------------------------------------------------------------------
void cara_init_job_list_node( struct infos_arr_list * p_al )
{
  const uint64_t nr_elems_in_block = UINT64_C(1) << p_al->log2_elem_cnt;
  //const uint64_t nr_valid_elems = nr_elems_in_block * p_al->active_blocks_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict node = &p_al->list.head;
  do {  // while ( base_idx < nr_valid_elems )
    void * block = node->arr_blocks[block_idx];

    for ( uint_fast32_t local_elem_idx = 0;
        local_elem_idx < nr_elems_in_block;
        local_elem_idx++ ) {

      struct cara_job_batch_desc_64 * restrict p_batch = block + local_elem_idx * sizeof(*p_batch);
      atomic_init( &p_batch->progress, ~0 );
      atomic_init( &p_batch->preferred_core, ~0 );
      for ( uint_fast8_t idx_job = 0; idx_job < CARBONARA_MAX_NR_JOBS_IN_A_JOB_DESC_64;
          idx_job++ ) {
        p_batch->job_flags[idx_job] = 0;
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

    }  // for ( uint_fast32_t local_elem_idx = 0;
       // local_elem_idx < nr_elems_in_vblock; local_elem_idx++ )

    block_idx++;
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( block_idx < p_al->active_block_cnt );

}



