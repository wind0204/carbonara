#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "info_struct.h"
#include "util-c_lang-basic.h"



//-------------------------------------------------------------------------------------------------
// from info_struct-tree.c
extern void init_infos_tree( uint64_t proc_info );
// from info_struct-tq.c
extern void init_infos_tq( uint64_t proc_info );



//-------------------------------------------------------------------------------------------------
void init_info_struct(uint64_t proc_info)
{
  init_infos_tree( proc_info );
  //init_infos_tq( proc_info );
}



//-------------------------------------------------------------------------------------------------
void * default_malloc( [[maybe_unused]] void * userdata, size_t alignment, size_t size )
{
  assume( _is_power_of_two(alignment) );
  size = _align_to_power_of_two( size, alignment );

  void * ret = aligned_alloc( alignment, size );
  return ret;
}

//-------------------------------------------------------------------------------------------------
void * default_realloc(
    [[maybe_unused]] void * userdata,
    void * orig_ptr, size_t size, [[maybe_unused]] size_t alignment )
{
  return realloc( orig_ptr, size );
}

//-------------------------------------------------------------------------------------------------
void default_free( [[maybe_unused]] void * restrict userdata, void * restrict ptr )
{
  free( ptr );
}





