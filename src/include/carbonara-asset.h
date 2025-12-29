#include "util-c_lang-basic.h"
#if !defined CARBONARA_ASSET_H
#define CARBONARA_ASSET_H

/*
 * NOTE For function declaration include "carbonara-asset-func.h"
 */

#include <stdatomic.h>
#include <stdint.h>
#include <SDL3/SDL_asyncio.h>

#include "info_struct.h"

//-------------------------------------------------------------------------------------------------

enum cara_tex_fmt : unsigned char {
  tf_astc = 1,
  tf_s3tc = 2,
};


//-------------------------------------------------------------------------------------------------
enum cara_img_id : uint32_t {
  // Albedo map
  ii_alb_stone = 0x000,
  ii_alb_sand,
};
enum cara_io_job_type : uint32_t {
  // 2D Image
  iojt_ld_albedo_map = 0x00,
  // SPIR-V code
  iojt_ld_spv_frag = 0x40,
  iojt_ld_spv_vert,
};
// 2D Image
struct img_load_job {
  enum cara_io_job_type asset_type;
  enum cara_img_id img_id;
  SDL_AsyncIO * p_io;
  struct range_u32 * p_malloc_range;
  //uint32_t cur;  // Assign size+1 here when closing is done. XXX We don't really need it
  uint8_t nr_miplvls;
  uint8_t lowest_miplv;  // lowest_miplv = highest resolution
  //uint32_t size;
};
// SPIR-V code
struct spv_load_job {
  enum cara_io_job_type asset_type;
  uint32_t spv_id;
  SDL_AsyncIO * p_io;
  //uint32_t cur;  // Assign size+1 here when closing is done. XXX We don't really need it
  //uint32_t size;
};

union cara_io_job {
  uint32_t type;
  struct img_load_job img;
  struct spv_load_job spv;
};
static_assert( CACHELINE_LEN % sizeof(union cara_io_job) == 0 );  // NOTE We can also consider
                                                                // having more than one single size
constexpr unsigned int nr_io_jobs_in_cacheline = CACHELINE_LEN / sizeof(union cara_io_job);
constexpr unsigned int nr_io_jobs_in_job_grp = nr_io_jobs_in_cacheline;

constexpr unsigned int cara_max_nr_io_jobs = 1024;
struct cara_io_center {
  struct infos_multi_arr_list job_lists;
  //union cara_io_job cur_jobs_taken[cara_max_nr_io_jobs];
  atomic_uint_least8_t vram_batch_cntr;  ///< Use this counter to determine which command buffer to
                                         ///<  submit when we have to upload data to VRAM
};
struct cara_io_worker {
  unsigned int jgrp_idx_plus1[ cara_max_nr_io_jobs / nr_io_jobs_in_job_grp ];  ///< Job group index
                                     ///< is the first index of `nr_io_jobs_in_job_grp`-index pair.
  unsigned int nr_job_grps_taken;
  //unsigned int nr_vram_load_jobs_to_get_done[2];  // set_idx 0 = even; 1 = odd;
  //unsigned int nr_vram_load_jobs_done[2];  // set_idx 0 = even; 1 = odd;
  uint8_t local_vram_batch_cntr;  ///< Use this counter to determine if another thread has increased
                                  ///<  the value of `vram_batch_cntr`
  alignas( CACHELINE_LEN )
  atomic_uint_least64_t nr_vram_jobs_to_get_done_and_nr_vram_jobs_done[2];  ///< union64
                                                      ///< [set_idx].arr_u32[0] = jobs to get done;
                                                      ///< [set_idx].arr_u32[1] = jobs done;
  atomic_uint_least8_t vram_batch_cntr;  ///< Use this counter to determine which secondary command
                                     ///<  buffer to record in when we have to upload to VRAM; There
                                     ///<  can be an increment that has been done by another thread
};
static_assert( _sizeof_member(struct cara_io_worker, local_vram_batch_cntr) ==
    _sizeof_member(struct cara_io_worker, vram_batch_cntr) );


#endif  // CARBONARA_ASSET_H
