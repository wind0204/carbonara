#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>

#include "err_msg.h"
#include "info_struct.h"
#include "carbonara-singleton.h"
#include "memorable_arena.h"
#include "carbonara-job.h"


void jf_maint_job_list(struct cara_worker_state * p_ws);
void jf_maint_memo_maps(struct cara_worker_state * p_ws);



void jf_maint_job_list(struct cara_worker_state * p_ws) {
  const uint_fast8_t nr_arrays = cara_single.job_list->active_block_cnt;
  uint64_t nr_elems = nr_arrays << cara_single.job_list->log2_elem_cnt;

  const uint_fast16_t nr_elems_taken_by_workers = atomic_load_explicit(
      &cara_single.nr_job_batch_descriptors_taken, memory_order_acquire);
  if ( nr_elems_taken_by_workers > (nr_elems/2 + nr_elems/4) ) {
#ifdef DEBUG_MODE
    fprintf(stdout, "[Maint-JD] Adding another array to job_list... "
        "nr_elems:%lu, nr_elems_taken_by_workers:%lu\n", nr_elems, nr_elems_taken_by_workers);
#endif
    void * last_arr;
    enum err_msg err = infos_al_grow( cara_single.job_list,
        &p_ws->malloc_helper, cara_malloc, &last_arr, nullptr, 1, false, 0 );
    if ( err ) {
      fprintf(stderr, "[Maint-JD] infos_al_grow failed. %s\n", arr_emsg_txt[err] );
      return;
    }

    const uint64_t nr_elems_per_arr = UINT64_C(1) << cara_single.job_list->log2_elem_cnt;
    for ( uint64_t idx_elem = 0; idx_elem < nr_elems_per_arr; idx_elem++ ) {
      struct cara_job_batch_desc_64 * restrict batch =
        last_arr + ( idx_elem << cara_single.job_list->log2_elem_size );
      atomic_init( &batch->preferred_core, ~0 );
      atomic_init( &batch->progress, ~0 );
    }
  }
}

/*
 *
 *
 * Just pack them in the job batch description together.
 *
void jf_maint_job_arg_pool(struct cara_worker_state * p_ws) {
  uint_fast8_t nr_nodes = cara_single.job_arg_pool->list->nr_nodes;
  uint_fast16_t nr_elems = cara_single.job_arg_pool->node_size / cara_single.job_arg_pool->elem_size;

  nr_elems *= nr_nodes;

  uint_fast16_t nr_elems_taken_by_workers = atomic_load_explicit(
      &cara_single.nr_job_arg_groups_taken, memory_order_acquire);
  if ( nr_elems_taken_by_workers > (nr_elems/2 + nr_elems/4) ) {
#ifdef DEBUG_MODE
    fprintf(stdout, "Adding another array to job_arg_pool... nr_elems:%lu, nr_elems_taken_by_workers:%lu\n", nr_elems, nr_elems_taken_by_workers);
#endif
    uint8_t ret = infos_al_grow(cara_single.job_arg_pool, 1);
    if ( ret ) {
      fprintf(stderr, "failed to allocate memory. %s\n", __FUNCTION__);
    }
  }
}
*/

void jf_maint_memo_maps( [[maybe_unused]] struct cara_worker_state * p_ws ) {
  struct ma_sub_arena * sub_arena = cara_single.mem_arena->head;
  uint_fast64_t nr_mem_chunks_unallocated = 0;
  uint_fast64_t threshold_nr_mem_chunks_unallocated = 0;
  uint16_t idx_pool = 0;
  while ( sub_arena ) {

    const uint32_t nr_mem_chunks = sub_arena->capacity;
    threshold_nr_mem_chunks_unallocated += nr_mem_chunks/16;
    nr_mem_chunks_unallocated += sub_arena->smm.total_space_left;

    idx_pool++;
    sub_arena = sub_arena->next;
  }

  if ( nr_mem_chunks_unallocated <= threshold_nr_mem_chunks_unallocated ) {
#ifdef DEBUG_MODE
    fprintf(stdout, "Adding another sub_arena to the arena... "
        "nr_mem_chunks_unallocated:%lu, threshold_nr_mem_chunks_unallocated:%lu\n",
        nr_mem_chunks_unallocated, threshold_nr_mem_chunks_unallocated);
#endif
    // Make a new memo_map
    typeof(idx_pool) old_value = idx_pool;
    if ( !atomic_compare_exchange_strong_explicit(&cara_single.mem_arena->nr_nodes,
          &old_value, old_value + 1, memory_order_acq_rel, memory_order_acquire) ) {
      // Someone else has already added a new memo map.
      return;
    }

    unsigned int nr_added_pools = ma_add_sub_arena(
        cara_single.proc_info,
        cara_single.mem_arena,
        cara_single.len_each_arena,
        old_value + 1 );
    if ( !nr_added_pools ) {
      atomic_store_explicit(
          &cara_single.mem_arena->nr_nodes,
          old_value,
          memory_order_relaxed );
      errno = errno;
      fprintf(stderr, "mm_add_pool failed. errno = %d\n", errno);
      return;
    }

    // A variable used for communication between threads, 0 is assigned when unlocked.
    atomic_store_explicit( &cara_single.mem_arena->tail->lock, 0, memory_order_release );

    //struct mm_memo_map * new_memo = mm_get_memo_map(idx_pool);

  }
}


