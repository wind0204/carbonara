#include <errno.h>
#include <limits.h>
#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "appdev_support-processor_map.h"
#include "util-c_lang-basic.h"
#include "err_msg.h"
#include "util-hwloc.h"

#ifdef DEBUG_MODE
#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
constexpr size_t default_max_nr_threads = 20;

//-------------------------------------------------------------------------------------------------
static uint_fast16_t is_there_an_ascendant_with_less_sets( hwloc_obj_t cache_obj );

//-------------------------------------------------------------------------------------------------
uint_fast8_t clear_processor_map( struct processor_map * processor_map )
{
  for ( uint_fast16_t idx_proc = 0; idx_proc < processor_map->nr_processors; idx_proc++ ) {
    struct ms_worker_info * pu = &processor_map->processors[idx_proc];
    u_hwloc_bitmap_free( (struct u_hwloc_bitmap_s *)pu->cpuset );
  }

  hwloc_topology_destroy(processor_map->topology);
  //free(processor_map);
  return 0;
}

//-------------------------------------------------------------------------------------------------
struct processor_map * prepare_processor_map(void)
{
  // pick a compact set of 2+ processors with at least
  // one in the package closest to an NIC and one in the package closest to the GPU
  // The processors in a same core are in a same processor_group
  // The processor_group not close to the GPU doesn't have CLOSE_TO_GPU flag on.
  // The processor_group not close to an NIC doesn't have CLOSE_TO_NIC flag on.

  hwloc_topology_t topology;
  hwloc_obj_t hwloc_obj, package_gpu;  //, package_nic;
  int err;

  err = hwloc_topology_init(&topology);
  if ( err == -1 ) {
    fprintf(stderr, "ERROR: Error code returned by hwloc_topology_init was %d\n", errno);
    //return 4;
    errno = err_msg_unknown;
    return 0;
  }

  err = hwloc_topology_load(topology);
  if ( err == -1 ) {
    fprintf(stderr, "ERROR: Error code returned by hwloc_topology_load was %d\n", errno);
    hwloc_topology_destroy(topology);
    errno = err_msg_unknown;
    return 0;
  }

  // TODO: The user should be able to choose which NIC and GPU to use
  unsigned int pkg_idx = 0;
  package_gpu = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PACKAGE, pkg_idx);
  if ( package_gpu == NULL ) {
    fprintf(stderr, "ERROR: Couldn't find any CPU package.\n");
    //return 3;
    errno = err_msg_unknown;
    return 0;
  }
  while ( package_gpu ) {
    pkg_idx++;
    package_gpu = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PACKAGE, pkg_idx);
  }
#ifdef DEBUG_MODE
  fprintf(stdout, "The number of available processor packages : %u\n", pkg_idx);
#endif
  // TODO: Find the package that is closest to the GPU
  // instead of just choosing the first one like this:
  package_gpu = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PACKAGE, 0);

  // TODO: Find the package that is closest to the NIC
  // instead of just choosing the same package like this:
  //package_nic = package_gpu;

  uint_fast8_t nr_l1_caches, nr_proc_units;

  nr_l1_caches = hwloc_get_nbobjs_inside_cpuset_by_type(topology,
      package_gpu->cpuset,
      HWLOC_OBJ_L1CACHE);
  if ( nr_l1_caches < 1 ) {
    fprintf(stderr, "ERROR: Couldn't find any L1 cache.\n");
    //return 3;
    errno = err_msg_unknown;
    return 0;
  }

  nr_proc_units = hwloc_get_nbobjs_inside_cpuset_by_type(topology,
      package_gpu->cpuset,
      HWLOC_OBJ_PU);
  if ( nr_proc_units < 1 ) {
    fprintf(stderr, "ERROR: Couldn't find any processor.\n");
    //return 3;
    errno = err_msg_unknown;
    return 0;
  }

#ifdef DEBUG_MODE
  fprintf(stdout, "nr_proc_units = %d\nnr_l1_caches = %d\n", nr_proc_units, nr_l1_caches);
#endif 

  //
  // Try to use at least half of the cores on the cpu package by spreading workers
  // using only one logical processor in each core.
  //
  // If the package closest to NIC is not the same package with the one closest to GPU
  // use only 1 processor in the package closest to NIC.
  // (2 when there are more than 8 processors available on the package closest to GPU,
  //  otherwise just 1)
  // It should probably have to have its own memory pool separately, maybe.
  //
  // DEFAULT_MAX_NR_THREADS = 20;
  // This number is applied to the main worker threads.
  // Not to the extra threads in the CPU package that is closer to NIC.
  // The user should be able to set the maximum number of threads to utilize.
  // The number of the processors utilized on the main package must be at least twice as big
  // as the the one on the package closest to NIC.
  //

  // Obtain the basic information
  uint_fast16_t min_nr_cache_sets = UINT16_MAX;
  uint_fast16_t max_nr_cache_sets = 1;
  uint_fast16_t cacheline_len;
  uint_fast8_t max_cache_level = 1;
  uint_fast8_t highest_lv_priv_d_cache = 0;
  uint32_t len_highest_priv_d_cache = 0;  // The length of the highest-level data cache that is
                                          // private for each core, in units of cache-line-sized
                                          // chunks
  hwloc_obj = hwloc_get_obj_below_by_type(topology,
      HWLOC_OBJ_PACKAGE, package_gpu->logical_index,
      HWLOC_OBJ_CORE, 0);
  hwloc_obj = hwloc_obj->parent;
  if ( !hwloc_obj_type_is_cache(hwloc_obj->type) ) {
    fprintf(stderr, "ERROR: A CPU core is not a child of an L1 cache in the hwloc tree. "
        "(hwloc_obj->type: %u)\n", hwloc_obj->type);
    hwloc_topology_destroy(topology);
    errno = err_msg_unknown;
    return 0;
  }

  //struct hwloc_cache_attr_s * cache = &hwloc_obj->attr->cache;
  union hwloc_obj_attr_u * cache = hwloc_obj->attr;
  cacheline_len = cache->cache.linesize;
  uint_fast8_t min_weight = hwloc_bitmap_weight(hwloc_obj->cpuset);

  // The length of the last-level data cache divided by associativity set count
  [[maybe_unused]] uint_fast8_t len_ll_dcache_cycle = 0;

  while(-7) {
    //struct hwloc_cache_attr_s * cache = &hwloc_obj->attr->cache;
    union hwloc_obj_attr_u * il_cache = hwloc_obj->attr;
    uint_fast16_t nr_cache_sets = GET_NR_SETS_FROM_HWLOC_CACHE_ATTR(il_cache);
    if ( nr_cache_sets < min_nr_cache_sets ) {
      min_nr_cache_sets = nr_cache_sets;
    }
    if ( nr_cache_sets > max_nr_cache_sets ) {
      max_nr_cache_sets = nr_cache_sets;
    }
    
    if ( hwloc_obj->arity == 1 && hwloc_obj_type_is_dcache(hwloc_obj->type) ) {
      if ( il_cache->cache.depth > highest_lv_priv_d_cache ) {
        highest_lv_priv_d_cache = il_cache->cache.depth;
        len_highest_priv_d_cache = il_cache->cache.size / il_cache->cache.linesize;
        //len_highest_priv_d_cache = il_cache->cache.size / cacheline_len;
      }
    }

#ifdef DEBUG_MODE
    char str[2] = {' ', 0};
    if ( hwloc_obj_type_is_dcache(hwloc_obj->type) ) {
      str[0] = 'd';
    } else if ( hwloc_obj_type_is_icache(hwloc_obj->type) ) {
      str[0] = 'i';
    } else {
      str[0] = ' ';
    }
    fprintf( stdout, "CACHE(%s) size=%lu linesize=%u (%d)-way depth:%u weight:%d\n",
        str, il_cache->cache.size, il_cache->cache.linesize, il_cache->cache.associativity,
        il_cache->cache.depth, hwloc_bitmap_weight(hwloc_obj->cpuset) );
    //fprintf(stdout, "CACHE(%s) size=%lu linesize=%u (%d)-way depth:%u weight:%d\n", str, il_cache->cache.size, cacheline_len, il_cache->cache.associativity, il_cache->cache.depth, hwloc_bitmap_weight(hwloc_obj->cpuset));
#endif
    if ( !_is_power_of_two(nr_cache_sets) ) {
      fprintf(stderr,
          "ERROR: The number of sets in the associative cache(L%u) is not a power of two. "
          "%lu is not.\n",
          il_cache->cache.depth, nr_cache_sets);
      hwloc_topology_destroy(topology);
      errno = err_msg_system_not_supported;
      return 0;
    }
    uint_fast8_t weight = hwloc_bitmap_weight(hwloc_obj->cpuset);
    if ( weight > min_weight && il_cache->cache.associativity > 1 ) {
      len_ll_dcache_cycle = il_cache->cache.size / il_cache->cache.associativity;
    }

    hwloc_obj = hwloc_obj->parent;
    if ( !hwloc_obj_type_is_cache(hwloc_obj->type) ) {
      max_cache_level = il_cache->cache.depth;
      break;
    }
  }
  assume( max_cache_level >= 2 );
  if ( max_cache_level < 2 ) {
    fprintf( stdout, "[AS] It's weird that the processor doesn't have an L2 data cache.\n" );
  }


  uint_fast8_t nr_cores = hwloc_get_nbobjs_inside_cpuset_by_type(topology,
      package_gpu->cpuset,
      HWLOC_OBJ_CORE);


  // Prepare a struct processor_map

  //uint_fast8_t nr_assigned_procs = 0;
  uint_fast8_t max_nr_threads = default_max_nr_threads;
  uint_fast8_t nr_threads_to_utilize = nr_proc_units;
  uint_fast8_t nr_yielders = 0;
  uint_fast8_t is_hyperthreading = nr_proc_units == 2 * nr_l1_caches ? -7 : 0;

  // Leave one logical core unassigned on each package for the OS and other apps when
  // there are 2+ physical cores available. (Leave two when 16+ physical cores are available
  // assigning no workers to one physical core )

  // Try to yield more often on 2 specific logical cores when there are 8+ threads available,
  // and on 4 specific cores when there are 16+ logical cores available in the system.

  if ( nr_proc_units >= 4 && is_hyperthreading ) {
    nr_threads_to_utilize--;
  }
  if ( nr_proc_units >= 8 ) {
    nr_yielders += 2;
  }
  if ( nr_proc_units >= 16 ) {
    nr_threads_to_utilize--;
    nr_yielders += 2;
  }
  if ( nr_threads_to_utilize > max_nr_threads ) {
    nr_threads_to_utilize = max_nr_threads;
  }

  if ( is_hyperthreading == 0 ) {
    nr_yielders = nr_yielders / 2;
  }

  static_assert( alignof(struct processor_map) >= alignof(struct ms_worker_info) &&
      alignof(struct ms_worker_info) >= alignof(struct u_hwloc_bitmap_s),
      "should be aligned.");
  struct processor_map * processor_map = malloc( sizeof(struct processor_map) +
      nr_threads_to_utilize * (
        //sizeof(struct ms_worker_info) + sizeof(struct __hwloc_bitmap_s) + HWLOC_BITMAP_PREALLOC_BITS/8
        sizeof(struct ms_worker_info) + sizeof(struct u_hwloc_bitmap_s)
        )
      );
  processor_map->processors = (struct ms_worker_info *)(processor_map + 1);
  if ( !processor_map ) {
    fprintf(stderr, "ERROR: err_msg_out_of_memory\n");
    hwloc_topology_destroy(processor_map->topology);
    errno = err_msg_out_of_memory;
    return 0;
  }

  processor_map->nr_processors = nr_threads_to_utilize;
  processor_map->cacheline_len_exp = LOG2_CACHELINE_LEN;
  processor_map->cacheline_len = CACHELINE_LEN;
  if ( cacheline_len != CACHELINE_LEN ) {
    // ALLOW_DIFFERENT_CACHELINE_SIZE has never been implemented to be honest.
    fprintf(stderr, "This program is not built with ALLOW_DIFFERENT_CACHELINE_SIZE!\n");
    errno = err_msg_system_not_supported;
    return 0;
  }
  processor_map->len_highest_priv_d_cache = len_highest_priv_d_cache;
  processor_map->processors = (void*)(processor_map + 1);
  processor_map->set_index_mask = (min_nr_cache_sets - 1) << _tzcnt_u16(CACHELINE_LEN);
  processor_map->topology = topology;
  processor_map->flags.whole = 0;

  hwloc_obj_t numa_0 = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, 0);
  processor_map->large_page_size_class = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code-loop-increment"
  for ( typeof(numa_0->attr->numanode.page_types_len) il_i = 0;
      il_i < numa_0->attr->numanode.page_types_len; il_i++ ) {
#pragma GCC diagnostic pop
    size_t size = numa_0->attr->numanode.page_types[il_i].size;
    switch ( size ) {
      case UINT64_C(1024) * 4:
        processor_map->large_page_size_class = 1;
        goto large_page_size_determined;
      case UINT64_C(1024) * 16:
        processor_map->large_page_size_class = 2;
        goto large_page_size_determined;
      case UINT64_C(1024) * 64:
        processor_map->large_page_size_class = 3;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024:
        processor_map->large_page_size_class = 4;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 2:
        processor_map->large_page_size_class = 5;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 16:
        processor_map->large_page_size_class = 6;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 64:
        processor_map->large_page_size_class = 7;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 128:
        processor_map->large_page_size_class = 8;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 256:
        processor_map->large_page_size_class = 9;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 512:
        processor_map->large_page_size_class = 10;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 1024:
        processor_map->large_page_size_class = 11;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 1024 * 2:
        processor_map->large_page_size_class = 12;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 1024 * 4:
        processor_map->large_page_size_class = 13;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 1024 * 64:
        processor_map->large_page_size_class = 14;
        goto large_page_size_determined;
      case UINT64_C(1024) * 1024 * 1024 * 128:
        processor_map->large_page_size_class = 15;
        goto large_page_size_determined;
      default:
        fprintf(stderr, "Unknown largepage(hugepage) size (%zu bytes)\n", size);
        errno = err_msg_system_not_supported;
        return 0;
    }
  }
large_page_size_determined:;

#ifdef DEBUG_MODE
  fprintf(stdout, "min_nr_cache_sets = %lu\n", min_nr_cache_sets);
  fprintf(stdout, "max_nr_cache_sets = %lu\n", max_nr_cache_sets);
  fprintf(stdout, "len_cacheline = %d\n", processor_map->cacheline_len);
  fprintf(stdout, "set_index_mask = %#08x\n", processor_map->set_index_mask);
  fprintf(stdout, "numa_0->attr->numanode.page_types_len = %u\n", numa_0->attr->numanode.page_types_len);
  for ( typeof(numa_0->attr->numanode.page_types_len) il_i = 0;
      il_i < numa_0->attr->numanode.page_types_len; il_i++ ) {
    fprintf(stdout, "  numa_0->attr->numanode.page_types[%u].size = %lu\n", il_i, numa_0->attr->numanode.page_types[il_i].size);
  }
  fprintf(stdout, "  processor_map->large_page_size_class = %hhu\n", processor_map->large_page_size_class);
#endif

  uint_fast16_t ofs_bitmap_loc = sizeof(struct processor_map) +
    nr_threads_to_utilize * sizeof(struct ms_worker_info);
  for ( uint_fast16_t proc_idx = 0; proc_idx < nr_threads_to_utilize; proc_idx++ ) {
    struct ms_worker_info * pu = &processor_map->processors[proc_idx];
    pu->cpuset = (void*)processor_map + ofs_bitmap_loc +
      proc_idx * sizeof(struct u_hwloc_bitmap_s);
      //idx_proc * ( HWLOC_BITMAP_PREALLOC_BITS/8 + sizeof(struct __hwloc_bitmap_s) );
    //pu->user_data = 0;
  }
  for ( uint_fast16_t proc_idx = 0; proc_idx < processor_map->nr_processors; proc_idx++ ) {
    struct ms_worker_info * pu = &processor_map->processors[proc_idx];
    ((struct u_hwloc_bitmap_s *)&pu->cpuset)->ulongs = 0;
  }


  //
  // Assign processor units to processor slots
  //

  uint_fast8_t foreman_idx_plus1 = 0;
  uint_fast8_t nr_yielders_assigned = 0;
  uint_fast16_t proc_idx = 0;

  for ( uint_fast8_t i = 0; i < nr_cores; i++ ) {
    // Iterate cores but do not start with the first core.
    const uint_fast8_t core_idx = _iterate_from_mid(i, nr_cores);
    hwloc_obj = hwloc_get_obj_below_by_type(topology,
        HWLOC_OBJ_PACKAGE, package_gpu->logical_index,
        HWLOC_OBJ_CORE, core_idx);

    // Hyperthreading is being applied
    if ( hwloc_obj->arity > 1 ) {
      uint_fast8_t nr_workers_to_assign_to_this_core = 2;

      uint_fast8_t assign_a_yielder = 0;
      uint_fast8_t assign_a_foreman = 0;
      if ( !foreman_idx_plus1 ) {
        assign_a_foreman = 1;
      } else if ( nr_yielders_assigned < nr_yielders ) {
        assign_a_yielder = 1;
      }

      // If there is more than enough number of processors available, we should probably spread
      //  thin.
      if ( (nr_proc_units - (nr_proc_units/nr_cores)*i) > (nr_threads_to_utilize - proc_idx) ) {
        nr_workers_to_assign_to_this_core = 1;
        assign_a_yielder = 0;
      }


      // The code pairs two logical processors in a same core using this variable
      struct ms_worker_info * prev_pu = nullptr;
      struct ms_worker_info * pu;
      int cpu_id = hwloc_bitmap_first(hwloc_obj->cpuset);
      int cpu_id_prev = 0;  // cpu_id + 1 of the prev_pu;
      assume( nr_workers_to_assign_to_this_core > 0 );
      for ( uint_fast8_t j = 0; j < nr_workers_to_assign_to_this_core; j++ ) {
        pu = &processor_map->processors[proc_idx];
        //pu->cpuset = hwloc_bitmap_alloc();
        if ( u_hwloc_bitmap_init((struct u_hwloc_bitmap_s *)pu->cpuset) ) {
          goto abort_preparation;
        }
        hwloc_bitmap_set(pu->cpuset, cpu_id);

        pu->idx_core = core_idx;
        pu->puid = cpu_id;

        if ( assign_a_foreman ) {
          pu->deployment_flags |= worker_deployment_flag_foreman;
          foreman_idx_plus1 = proc_idx + 1;
        }

        //pu->sum_mem_occupying = 0;
        if ( assign_a_yielder ) {
          pu->deployment_flags |= worker_deployment_flag_yielder;
          nr_yielders_assigned++;
          if ( nr_yielders_assigned >= nr_yielders ) {
            assign_a_yielder = 0;
          }
        }
        pu->idx_missing_sibling = 0;
        pu->idx_one_sibling = 0;
        if ( cpu_id_prev ) {
          pu->idx_one_sibling = cpu_id_prev;
        }

        if ( prev_pu ) {
          prev_pu->idx_one_sibling = cpu_id + 1;
          prev_pu = 0;
        } else {
          prev_pu = pu;
        }

        cpu_id_prev = cpu_id + 1;
        cpu_id = hwloc_bitmap_next(hwloc_obj->cpuset, cpu_id);
        proc_idx++;
        if ( cpu_id == -1 ) {
          break;
        }
      }

      // There's a logical processor left unassigned from the last CPU core?
      if ( cpu_id != -1 ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
        // This worker will be allowed to utilize more cache sets.
        pu->idx_missing_sibling = cpu_id + 1;
#pragma GCC diagnostic pop
        pu->deployment_flags |= worker_deployment_flag_missing_a_sibling;
      }

    } else {  // if ( hwloc_obj->arity > 1 )
      // Hyperthreading is not being applied
      struct ms_worker_info * pu = &processor_map->processors[proc_idx];
      //pu->cpuset = hwloc_bitmap_alloc();
      if ( u_hwloc_bitmap_init((struct u_hwloc_bitmap_s *)pu->cpuset) ) {
        goto abort_preparation;
      }
      hwloc_bitmap_copy(pu->cpuset, hwloc_obj->cpuset);
      //pu->sum_mem_occupying = 0;
      pu->deployment_flags = 0;
      pu->idx_missing_sibling = 0;
      pu->idx_one_sibling = 0;
      pu->idx_core = core_idx;

      if ( !foreman_idx_plus1 ) {
        foreman_idx_plus1 = 1 + proc_idx;
        pu->deployment_flags |= worker_deployment_flag_foreman;
      } else if ( nr_yielders_assigned < nr_yielders ) {
        // Determine if it's gonna be yielder or not
        pu->deployment_flags |= worker_deployment_flag_yielder;
        nr_yielders_assigned++;
      }

      proc_idx++;
    }

    if ( proc_idx == nr_threads_to_utilize ) {
      break;
    }
  }

  if ( !is_hyperthreading ) {
    processor_map->flags.no_hyperthread = 1;
  }



#ifdef DEBUG_MODE
    fprintf(stdout, "\n");
#endif
  for ( uint_fast16_t worker_idx = 0; worker_idx < processor_map->nr_processors; worker_idx++ ) {
    struct ms_worker_info * wi = processor_map->processors + worker_idx;
    wi->idx_self = worker_idx;

#ifdef DEBUG_MODE
    //char * string = malloc(32);
    char string[32];
    hwloc_bitmap_snprintf(string, 32, wi->cpuset);
    fprintf(stdout, "%spu->cpuset = %s (%d) (puid:%u)\n",
        wi->deployment_flags&worker_deployment_flag_foreman?"F":"_", string,
        hwloc_bitmap_first(wi->cpuset), wi->puid);
    //free(string);

    //fprintf(stdout, "cache_pattern = %#010x", il_pu->cache_pattern);
    fprintf(stdout, "idx_core = %d\n", wi->idx_core);
    fprintf(stdout, "idx_proc = %d\n", wi->idx_self);
    //fprintf(stdout, "pattern_len = %d\n", il_pu->pattern_len);
    //fprintf(stdout, "cache_stride = %d\n", il_pu->cache_stride);
    //fprintf(stdout, "cache_location = %d\n", il_pu->cache_location);
    fprintf(stdout, "\n");
#endif

  }
  //u_hwloc_bitmap_free(virtual_cpuset);
  //free(virtual_cpuset);

  return processor_map;

abort_preparation:
  fprintf(stderr, "ERROR: err_msg_out_of_memory\n");
  clear_processor_map(processor_map);
  errno = err_msg_out_of_memory;
  return 0;
}


//-------------------------------------------------------------------------------------------------
[[maybe_unused]]
static uint_fast16_t is_there_an_ascendant_with_less_sets(hwloc_obj_t cache_obj) {
  uint_fast16_t nr_cache_sets_child = GET_NR_SETS_FROM_HWLOC_CACHE_ATTR(cache_obj->attr);
  while(-7) {
    cache_obj = cache_obj->parent;
    if ( !cache_obj || !hwloc_obj_type_is_cache(cache_obj->type) ) {
      return 0;
    }

    uint_fast16_t nr_cache_sets = GET_NR_SETS_FROM_HWLOC_CACHE_ATTR(cache_obj->attr);
    if ( nr_cache_sets_child < nr_cache_sets ) {
      return nr_cache_sets;
    } 
  }
}


