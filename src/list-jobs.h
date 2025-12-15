#include "util-c_lang-basic.h"
#if !defined LIST_JOBS_H
# define LIST_JOBS_H

#include <stdint.h>

#include "carbonara.h"
#include "carbonara-job.h"


//-------------------------------------------------------------------------------------------------
/**
 * \brief carbonara-job.c:known_jobs
 */
enum job_func_idx : _typeof_member(struct cara_job_batch_desc_64, job_func_idx[0]) {
  jf_idx_none,  // #0

  // carbonara-maint.c
  jf_idx_maint_job_list,  // #1
  jf_idx_maint_memo_maps,

  // carbonara-rainbow.c
  jf_idx_start_rainbow_scene,  // #3
  jf_idx_update_rainbow_scene,
  jf_idx_end_rainbow_scene,

  // carbonara.c
  jf_idx_start_kill_chain,  // #6
  jf_idx_kickstart,
  jf_idx_police_patrol,
};

//-------------------------------------------------------------------------------------------------
// carbonara-maint.c
extern void jf_maint_job_list( struct cara_worker_state * p_ws );
extern void jf_maint_job_arg_pool( struct cara_worker_state * p_ws );
extern void jf_maint_memo_maps( struct cara_worker_state * p_ws );
extern void jf_maint_collective_grower( struct cara_worker_state * p_ws );

//-------------------------------------------------------------------------------------------------
// carbonara-rainbow.c
extern void jf_start_rainbow_scene( struct cara_worker_state * p_ws );
extern void jf_update_rainbow_scene( struct cara_worker_state * p_ws );
extern void jf_end_rainbow_scene( struct cara_worker_state * p_ws );

//-------------------------------------------------------------------------------------------------
// carbonara.c
extern void jf_start_kill_chain( struct cara_worker_state * p_ws );
extern void jf_kickstart( struct cara_worker_state * p_ws );
extern void jf_police_patrol( struct cara_worker_state * p_ws );


#endif
