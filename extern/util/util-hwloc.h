#if !defined UTIL_HWLOC_H_
#define UTIL_HWLOC_H_



#include <stdint.h>



#define HWLOC_SUBBITMAP_ZERO 0UL
#define HWLOC_BITS_PER_LONG (HWLOC_SIZEOF_UNSIGNED_LONG * 8)
#define HWLOC_BITMAP_PREALLOC_BITS 512
#define HWLOC_BITMAP_PREALLOC_ULONGS (HWLOC_BITMAP_PREALLOC_BITS/HWLOC_BITS_PER_LONG)
#define HWLOC_SIZEOF_UNSIGNED_LONG sizeof(unsigned long)
#define HWLOC_BITMAP_MAGIC 0x20091007



struct u_hwloc_bitmap_s {
  unsigned ulongs_count; /* how many ulong bitmasks are valid, >= 1 */
  unsigned ulongs_allocated; /* how many ulong bitmasks are allocated, >= ulongs_count */
  unsigned long *ulongs;
  int infinite; /* set to 1 if all bits beyond ulongs are set */
#ifdef HWLOC_DEBUG
  int magic;
#endif
};


/**
 * @param set the pointer to a string of memory with length of
 * 64 + sizeof(struct __hwloc_bitmap_s) bytes
 */
uint8_t u_hwloc_bitmap_init(struct u_hwloc_bitmap_s * set);
void u_hwloc_bitmap_free(struct u_hwloc_bitmap_s * set);



/**
 * Get the number of cacheline sets in a CPU cache via
 * the pointer to a union hwloc_obj_attr_u, if it's fully
 * associated it will return 1, if it's unknown it will assume
 * the cache's direct-mapped and return ( ptr->cache.size / ptr->cache.linesize )
 */
#define GET_NR_SETS_FROM_HWLOC_CACHE_ATTR(ptr_hwloc_obj_attr) (\
    ((ptr_hwloc_obj_attr)->cache).associativity>1 ? ((ptr_hwloc_obj_attr)->cache).size / \
      (((ptr_hwloc_obj_attr)->cache).linesize * ((ptr_hwloc_obj_attr)->cache).associativity) : ( \
        ((ptr_hwloc_obj_attr)->cache).associativity == -1 ? 1 : \
        ((ptr_hwloc_obj_attr)->cache).size / ((ptr_hwloc_obj_attr)->cache).linesize ) )
// hwloc_obj_attr_u.cache.associativity is -1 if fully associative, 0 if unknown


#endif  // UTIL_HWLOC_H_
