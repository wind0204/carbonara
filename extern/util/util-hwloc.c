#include <stdlib.h>

#include "hwloc/bitmap.h"

#include "err_msg.h"
#include "util-hwloc.h"

uint8_t u_hwloc_bitmap_init(struct u_hwloc_bitmap_s * set) {
  //set = malloc(sizeof(struct hwloc_bitmap_s));

  set->ulongs_count = HWLOC_BITMAP_PREALLOC_ULONGS;
  set->ulongs_allocated = HWLOC_BITMAP_PREALLOC_ULONGS;
  set->ulongs = (unsigned long *) malloc(HWLOC_BITMAP_PREALLOC_ULONGS * sizeof(unsigned long));
  if (!set->ulongs) {
    //free(set);
    return err_msg_out_of_memory;
  }

  set->ulongs[0] = HWLOC_SUBBITMAP_ZERO;
  set->infinite = 0;
#ifdef HWLOC_DEBUG
  set->magic = HWLOC_BITMAP_MAGIC;
#endif

  hwloc_bitmap_zero((hwloc_bitmap_t)set);

  return 0;
}

void u_hwloc_bitmap_free(struct u_hwloc_bitmap_s * set) {
  //set->ulongs_count = 1;
  free( set->ulongs );
  set->ulongs = 0;
  set->ulongs_count = 0;
  set->ulongs_allocated = 0;
#ifdef HWLOC_DEBUG
  hwloc_bitmap->magic = 0;
#endif
}
