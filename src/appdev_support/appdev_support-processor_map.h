#if !defined APPDEV_SUPPORT_PROCESSOR_MAP_H_
#define APPDEV_SUPPORT_PROCESSOR_MAP_H_


#include <stdint.h>

#include "hwloc.h"


//-------------------------------------------------------------------------------------------------
extern struct ms_worker_info unbound_worker;
//#define MS_MAX_NR_CACHE_SETS __sizeof_member(struct ms_worker_info, cache_pattern)*8

enum worker_deployment_flags : unsigned char {
  worker_deployment_flag_yielder = 0x01,    ///< It's gonna yield CPU resource more often.
  worker_deployment_flag_close2nic = 0x02,  ///< It's closer to the NIC
  worker_deployment_flag_missing_a_sibling = 0x04,  ///< The application is not utilizing the other
                                                    ///< logical processor from the same core.
  worker_deployment_flag_foreman = 0x08,  ///< A representative who interacts with certain program
                                          ///<  paths and APIs.
};

struct ms_worker_info {
  //void * user_data;
  /*
  uint32_t cache_pattern; ///< Each bit represents a cache set with right color.
                          ///< WARNING This may be offset by cache_location
                          ///< The information about the first cache set is written on the most significant bit.
                          ///< It's kept in big-endian byte order.
  */
  hwloc_bitmap_t cpuset;  ///< struct u_hwloc_bitmap_s for denoting the logical processor to pin the
                          ///< worker thread to.
  //uint32_t sum_mem_occupying;  ///< The sum of memory occupied by this processor, in units of 16-byte chunks
                                ///< Local memory strings don't count. only those from shared memory buffers do.
  /*
  uint8_t cache_location; ///< Index of the cache set the pattern starts
  uint8_t len_cache_stride;   ///< The length of stride between memory areas represented by cache_pattern
                              ///< in units of cache lines.
  uint8_t len_pattern;    ///< Determines the effective range in cache_pattern
  */
  uint16_t idx_missing_sibling;   ///< 1 + the id of a sibling processor in the same CPU core
                                  ///< that the application is not utilizing
  uint16_t idx_one_sibling;   ///< 1 + the id of a sibling processor in the same CPU core
                              ///< that is being utilized by the application
  uint8_t idx_self;  ///< The processor index, the first worker's index is 0
  uint8_t puid;  ///< The ID of the processor unit learned from hwloc
  uint8_t deployment_flags;   ///< enum_processor_deployment_flags

  uint8_t idx_core;  ///< The CPU core index
};

struct processor_map {
  struct ms_worker_info * processors;
  hwloc_topology_t topology;
  uint32_t len_highest_priv_d_cache; ///< The length of the highest-level data cache that is private for each
                                     ///< core, in units of cache-line-sized chunks
  uint32_t set_index_mask;  ///< The bit mask for obtaining of the index of a cache set on set-associative cache
                            ///< that corresponds the target address.
  uint8_t cacheline_len_exp;  ///< It's written in a way that 2**len_cache_line_exp is the length of cache line
                               ///< e.g. 6 for 64-byte long cache lines
  uint8_t cacheline_len;  ///< The length of a cache line
  uint8_t nr_processors;  ///< The number of the worker threads currently assigned.
  union {
    struct {
      uint8_t no_hyperthread : 1;
    };
    uint8_t whole;
  } flags;
  uint8_t large_page_size_class;  ///< 4K(1), 16K(2), 64K(3), 1M, 2M, 16M, 64M, 128M, 256M, 512M, 1G, 2G, 4G, 64G, 128G(15)
  uint8_t len_ll_dcache_cycle;  ///< The length of the data that would let you access the first cache set of the
                                ///< last-level data cache again, in units of 128KiB.
                                ///< 0 will be assigned if the last-level data cache is not shared among the cores
                                ///< or we can't tell that the last-level data cache is set associative.
                                ///< e.g. You need to access a sequence of 64byte*4096(=256KiB) to reach the first
                                ///< set again in a L3 data cache if it's 8-way set associative with 2MiB of total
                                ///< length. (1024*1024*2 / 8 = 4096)
                                ///< I think for an algorithm that accesses a big dataset with a sparse access
                                ///< pattern it's better to align the size of the dataset to this value and limit
                                ///< the size of the sub-dataset to a multiple of this value that is less than
                                ///< 2MiB--or other large page size-- in order to prevent an imbalance between
                                ///< the access rates of the cache sets of the last-level data cache since the
                                ///< last-level data cache would stop using virtual addresses and start using
                                ///< physical addresses and each page would not be arbitrarily dispersed with 4KiB
                                ///< granularity anymore because the default behaviour of some of the memory
                                ///< allocation code of yours might favor the 2MiB-sized page.--No problem with
                                ///< 1GiB-sized pages --

};

//-------------------------------------------------------------------------------------------------
/**
 * \brief You must call this function before using the global struct unbound_worker
 *
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
uint_fast8_t init_appdev_support_processor_map(void);
//void remove_appdev_support_processor_map(void);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain a struct processor_map in which the information of a decent number of
 * worker threads is stored, among which one thread elected as a foreman.
 * \return The pointer to the singleton of struct processor_map, or 0 when there was an error
 *
 * errno err_msg_unknown means there was a weird error or a failure in a library call
 * errno err_msg_system_not_supported means the system is not supported, e.g.
 *     a wrong number of cache sets or processors
 */
struct processor_map * prepare_processor_map(void);

//-------------------------------------------------------------------------------------------------
/**
 * \brief clear up everything but leave the allocation without releasing
 */
uint_fast8_t clear_processor_map( struct processor_map * pmap );


#endif // APPDEV_SUPPORT_PROCESSOR_MAP_H_
