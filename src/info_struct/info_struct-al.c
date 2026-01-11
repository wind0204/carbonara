#include <immintrin.h>
#ifdef _MSC_VER
  #include <intrin.h>
#else
  #include <x86intrin.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>

#include "err_msg.h"
#include "info_struct.h"
#include "util-c_lang-basic.h"



//-------------------------------------------------------------------------------------------------
struct infos_arr_list * infos_al_create(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t node_size,
    uint_fast16_t elem_size,
    bool zero_all
    )
{
  assume( _is_aligned_to_power_of_two(node_size, ALIGN_FOR_AVX512) );

  // Just in case custom_malloc allocated an aligned string for SSE/AVX or
  // a 64-byte aligned string for cache line separation, put data first up front.
  uint32_t ofs_data = 0;  // the first node's data should be in the front.
  uint_fast8_t out_of_align_diff;  // distance between the first node's data and the first struct

  //if ( node_size % alignof(struct infos_arr_list) ) {
  out_of_align_diff = _align_to_power_of_two( node_size, alignof(struct infos_arr_list) )
    - node_size;
  uint32_t ofs_itself = node_size + out_of_align_diff;

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  void * restrict ptr;
  assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
  ptr = local_malloc( userdata, CACHELINE_LEN,
      out_of_align_diff + node_size + sizeof(struct infos_arr_list) );
  if ( !ptr ) {
    return nullptr;
  }
  if ( zero_all ) {
    memset( ptr + ofs_data, 0, node_size );
  }

  struct infos_arr_list * p_al = ptr + ofs_itself;

  assume( _is_power_of_two(node_size) );
  p_al->log2_block_size = get_log2( node_size );
  assume( _is_power_of_two( elem_size ) );
  p_al->log2_elem_size = 31 - _lzcnt_u32( elem_size );
  p_al->log2_nr_elems = p_al->log2_block_size - p_al->log2_elem_size;

  p_al->list.head.arr_blocks[0] = ptr;
  p_al->nr_active_blocks = 1;

  return p_al;
}

//-------------------------------------------------------------------------------------------------
struct infos_arr_list * infos_al_create_prealloc( struct infos_arr_list * restrict p_al,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t node_size,
    uint_fast16_t elem_size,
    bool zero_all
    )
{
  assume( _is_aligned_to_power_of_two(node_size, ALIGN_FOR_AVX512) );

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  void * restrict ptr;
  assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
  ptr = local_malloc( userdata, CACHELINE_LEN, node_size );
  if ( !ptr ) {
    return nullptr;
  }
  if ( zero_all ) {
    memset( ptr, 0, node_size );
  }

  assume( _is_power_of_two(node_size) );
  p_al->log2_block_size = get_log2( node_size );
  assume( _is_power_of_two( elem_size ) );
  p_al->log2_elem_size = 31 - _lzcnt_u32( elem_size );
  p_al->log2_nr_elems = p_al->log2_block_size - p_al->log2_elem_size;

  p_al->list.head.arr_blocks[0] = ptr;
  p_al->nr_active_blocks = 1;

  return p_al;

}



//-------------------------------------------------------------------------------------------------
enum err_msg infos_al_grow(
    struct infos_arr_list * restrict p_al,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void ** pp_first_block_among_new_blocks,
    struct linked_list_of_lists_node ** pp_last_node,
    uint_fast16_t nr_blocks_to_generate,
    bool fill_new_blocks, uint64_t qw_filler
    )
{
  assume( nr_blocks_to_generate > 0 );

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  typeof(p_al->nr_active_blocks) block_id = p_al->nr_active_blocks;
  typeof(nr_blocks_to_generate) nr_blocks_generated = 0;
  struct linked_list_of_lists_node * restrict cur = &p_al->list.head;
  if ( pp_first_block_among_new_blocks ) {
    *pp_first_block_among_new_blocks = nullptr;
  }
  if ( pp_last_node ) {
    *pp_last_node = nullptr;
  }
  while(-7) {
    while ( block_id < _arr_elem_cnt(cur->arr_blocks) ) {
      size_t block_len = UINT64_C(1) << p_al->log2_block_size;
      assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
      void * restrict new_malloc = local_malloc( userdata, CACHELINE_LEN, block_len );
      if ( !new_malloc ) {
        return emsg_out_of_memory;
      }
      cur->arr_blocks[block_id] = new_malloc;
      if ( fill_new_blocks ) {
        //packed_aligned_avx2_memset( new_malloc, 0, block_len );
        const simde__m256i key_pack = simde_mm256_set1_epi64x( qw_filler );
        constexpr uint64_t stride = ALIGN_FOR_AVX;
        void * const restrict p_vectors = __builtin_assume_aligned( new_malloc, ALIGN_FOR_AVX );
        for ( uint64_t byte_idx = 0;
            byte_idx < UINT64_C(1) << p_al->log2_block_size;
            byte_idx += stride ) {
          simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
        }
      }
      if ( pp_first_block_among_new_blocks && !*pp_first_block_among_new_blocks ) {
        *pp_first_block_among_new_blocks = new_malloc;
      }
      nr_blocks_generated++;
      if ( nr_blocks_generated == nr_blocks_to_generate ) {
        goto out_of_nested_loop;
      }
      block_id++;
    }
    block_id -= _arr_elem_cnt(cur->arr_blocks);
    if ( !cur->next ) {
      size_t node_len = sizeof(struct linked_list_of_lists_node);
      assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
      void * restrict new_malloc = local_malloc( userdata, CACHELINE_LEN, node_len );
      if ( !new_malloc ) {
        return emsg_out_of_memory;
      }
      cur->next = new_malloc;

      size_t len_avx_aligned_part = _align_to_power_of_two_downward_l2( node_len, ALIGN_FOR_AVX );
      packed_aligned_avx2_memset( new_malloc, 0, len_avx_aligned_part );
      unsigned int * ptr_cap = new_malloc + node_len;
      for ( unsigned int * ptr = new_malloc + len_avx_aligned_part; ptr < ptr_cap; ptr++ ) {
        *ptr = 0;
      }
      if ( pp_first_block_among_new_blocks && !*pp_first_block_among_new_blocks ) {
        *pp_first_block_among_new_blocks = new_malloc;
      }
    }
    cur = cur->next;
  }
out_of_nested_loop:

  p_al->nr_active_blocks += nr_blocks_generated;
  return emsg_no_error;
}



//-------------------------------------------------------------------------------------------------
void infos_al_destroy_except_preallocated_struct( struct infos_arr_list * restrict p_al,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr) )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;
  local_free( userdata, p_al->list.head.arr_blocks );

  struct linked_list_of_lists_node * restrict node = p_al->list.head.next;
  typeof(p_al->nr_active_blocks) block_id = _arr_elem_cnt(node->arr_blocks);
  while ( node ) {
    constexpr uint_fast16_t local_cap = _arr_elem_cnt(node->arr_blocks);
    for ( uint_fast16_t local_idx = 0; local_idx < local_cap; local_idx++, block_id++ ) {
      if ( block_id >= p_al->nr_active_blocks ) {
        local_free( userdata, node );
        goto out_of_nested_loop;
      }
      void * block = node->arr_blocks[local_idx];
      local_free( userdata, block );
    }
    typeof(node) next = node;
    local_free( userdata, node );
    node = next;
  }
out_of_nested_loop:

  node = &p_al->list.head;
  block_id = 0;
  constexpr uint_fast16_t local_cap = _arr_elem_cnt(node->arr_blocks);
  for ( uint_fast16_t local_idx = 1; local_idx < local_cap; local_idx++, block_id++ ) {
    if ( block_id >= p_al->nr_active_blocks ) {
      return;
    }
    void * block = node->arr_blocks[local_idx];
    local_free( userdata, block );
  }
  local_free( userdata, node->arr_blocks[0] );
}

//-------------------------------------------------------------------------------------------------
void infos_al_destroy( struct infos_arr_list * restrict p_al, void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr) )
{
  infos_al_destroy_except_preallocated_struct( p_al, userdata, custom_free );
}



//-------------------------------------------------------------------------------------------------
struct infos_multi_arr_list * infos_multi_al_create(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast8_t log2_node_size, uint_fast8_t log2_elem_size, uint_fast8_t log2_nr_arr_lists,
    bool zero_all
    )
{
  assume( log2_nr_arr_lists > 0 );
  assume( log2_node_size >= 31 - _lzcnt_u32(ALIGN_FOR_AVX512) );

  // Just in case custom_malloc allocates an aligned string for SSE/AVX or
  // a 64-byte aligned string for cache line separation, put data first up front.
  uint_fast8_t out_of_align_diff = 0;  // The distance between the array of structs
                                       // linked_list_of_lists_fh and the struct
                                       // infos_multi_arr_list

  // Put either an array of linked_list_of_lists_fh or the infos_multi_arr_list right after data
  uint64_t ofs_itself, ofs_list_fh, ofs_something;
  if ( alignof(struct infos_multi_arr_list) > alignof(struct linked_list_of_lists_fh) ) {
    if ( (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size)) %
        alignof(struct infos_multi_arr_list) ) {
      out_of_align_diff = _align_to_power_of_two(
          UINT64_C(1) << (log2_nr_arr_lists + log2_node_size),
          alignof(struct infos_multi_arr_list) ) -
        (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size));
    }
    ofs_itself = ( UINT64_C(1) << log2_nr_arr_lists << log2_node_size ) + out_of_align_diff;
    ofs_list_fh = ofs_itself + sizeof(struct infos_multi_arr_list);
    ofs_something = ofs_list_fh + sizeof(struct linked_list_of_lists_fh);

  } else {
    if ( (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size)) %
        alignof(struct linked_list_of_lists_fh) ) {
      out_of_align_diff = _align_to_power_of_two(
          UINT64_C(1) << (log2_nr_arr_lists + log2_node_size),
          alignof(struct linked_list_of_lists_fh) ) -
        (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size));
    }
    ofs_list_fh = ( UINT64_C(1) << log2_nr_arr_lists << log2_node_size ) + out_of_align_diff;
    ofs_itself = ofs_list_fh + sizeof(struct linked_list_of_lists_fh);
    ofs_something = ofs_itself + sizeof(struct infos_multi_arr_list);
  }

  // A quick and easy assert for the placing of infos_multi_arr_list::nr_active_blocks
  uint64_t alignof_count = _alignof_arr_elem(
      _typeof_member(struct infos_multi_arr_list, arr_nr_active_blocks) );
  assume( alignof(struct infos_multi_arr_list) >= alignof_count &&
      alignof(struct linked_list_of_lists_fh) >= alignof_count );

  void * restrict ptr;
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
  ptr = local_malloc( userdata, CACHELINE_LEN,
      out_of_align_diff + (UINT64_C(1) << log2_nr_arr_lists) * (
        (UINT64_C(1) << log2_node_size) +
        sizeof(struct linked_list_of_lists_fh) +
        sizeof(struct infos_multi_arr_list) +
        _sizeof_arr_elem(_typeof_member(struct infos_multi_arr_list, arr_nr_active_blocks)) )
      );
  if ( !ptr ) {
    return nullptr;
  }
  constexpr uint64_t ofs_data = 0;  // the first node's data should be in the front.
  if ( zero_all ) {
    memset( ptr + ofs_data, 0, UINT64_C(1) << log2_node_size << log2_nr_arr_lists );
  }

  struct infos_multi_arr_list * restrict p_mal = ptr + ofs_itself;

  p_mal->log2_block_size = log2_node_size;
  p_mal->log2_elem_size = log2_elem_size;
  p_mal->log2_nr_lists = log2_nr_arr_lists;
  p_mal->log2_nr_elems = log2_node_size - log2_elem_size;

  p_mal->lists = ptr + ofs_list_fh;
  p_mal->arr_nr_active_blocks = ptr + ofs_something;
  for ( uint64_t list_idx = 0; list_idx < UINT64_C(1) << log2_nr_arr_lists; list_idx++ ) {
    p_mal->lists[list_idx].head.arr_blocks[0] = ptr + (list_idx << log2_node_size);
    p_mal->lists[list_idx].head.next = nullptr;
    p_mal->arr_nr_active_blocks[list_idx] = 1;
  }

  return p_mal;
}

//-------------------------------------------------------------------------------------------------
uint_fast8_t infos_multi_al_init_prealloc(
    struct infos_multi_arr_list * restrict p_mal,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast8_t log2_node_size, uint_fast8_t log2_elem_size, uint_fast8_t log2_nr_arr_lists,
    bool zero_all
    )
{
  assume( log2_nr_arr_lists > 0 );
  assume( log2_node_size >= 31 - _lzcnt_u32(ALIGN_FOR_AVX512) );

  uint_fast8_t out_of_align_diff = 0;  // distance between the first node's data and the next struct
  //if ( nr_arr_lists * node_size % alignof(struct linked_list_of_lists_fh) ) {
  //  out_of_align_diff = _align_to_power_of_two(
  //      (nr_arr_lists * node_size), alignof(struct linked_list_of_lists_fh)
  //      ) - (nr_arr_lists * node_size);
  //}
  if (unlikely( log2_nr_arr_lists + log2_node_size <
      31 - _lzcnt_u32(alignof(struct linked_list_of_lists_fh)) )) {
    out_of_align_diff = _align_to_power_of_two(
        UINT64_C(1) << (log2_nr_arr_lists + log2_node_size),
        alignof(struct linked_list_of_lists_fh)
        ) - (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size));
  }

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
  void * restrict ptr = local_malloc( userdata, CACHELINE_LEN, out_of_align_diff +
      (UINT64_C(1) << log2_nr_arr_lists) * (
        (UINT64_C(1) << log2_node_size) +
        sizeof(struct linked_list_of_lists_fh) +
        _sizeof_arr_elem(_typeof_member(struct infos_multi_arr_list, arr_nr_active_blocks)) )
      );
  if ( !ptr ) {
    return emsg_no_error;
  }
  if ( zero_all ) {
    memset( ptr, 0, UINT64_C(1) << log2_node_size << log2_nr_arr_lists );
  }

  p_mal->log2_block_size = log2_node_size;
  p_mal->log2_elem_size = log2_elem_size;
  p_mal->log2_nr_lists = log2_nr_arr_lists;
  p_mal->log2_nr_elems = log2_node_size - log2_elem_size;

  p_mal->lists = ptr + ( UINT64_C(1) << log2_node_size << log2_nr_arr_lists ) + out_of_align_diff;
  p_mal->arr_nr_active_blocks = (void*) ( p_mal->lists + (UINT64_C(1) << log2_nr_arr_lists) );
  for ( uint32_t list_idx = 0; list_idx < UINT64_C(1) << log2_nr_arr_lists; list_idx++ ) {
    p_mal->lists[list_idx].head.arr_blocks[0] = ptr + ( list_idx << log2_node_size );
    p_mal->lists[list_idx].head.next = nullptr;
    p_mal->arr_nr_active_blocks[list_idx] = 1;
  }

  return 0;
}



//-------------------------------------------------------------------------------------------------
enum err_msg infos_multi_al_grow( struct infos_multi_arr_list * restrict p_mal,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t arr_len,
    const uint32_t arr_al_indices[arr_len],
    bool fill_new_blocks, uint64_t qw_filler
    )
{
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
  //void * restrict new_malloc =
  //  local_malloc( userdata, CACHELINE_LEN, (1 << p_mal->log2_block_size) );
  for ( uint64_t idx = 0; idx < arr_len; idx++ ) {

    struct linked_list_of_lists_node * restrict node = &p_mal->lists[arr_al_indices[idx]].head;
    uint_fast8_t node_idx = 0;
    for ( ; node_idx <=
        (p_mal->arr_nr_active_blocks[arr_al_indices[idx]] - 1) / _arr_elem_cnt(node->arr_blocks);
        node_idx++ ) {
      node = node->next;
    }
    unsigned int block_idx = (p_mal->arr_nr_active_blocks[arr_al_indices[idx]] - 1) %
      _arr_elem_cnt(node->arr_blocks);

    // We need to make a new node
    if ( block_idx == _arr_elem_cnt(node->arr_blocks) ) {
      size_t node_len = sizeof(struct linked_list_of_lists_node);
      assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
      void * restrict new_node = local_malloc( userdata, CACHELINE_LEN, node_len );
      if ( !new_node ) {
        return emsg_out_of_memory;
      }
      node->next = new_node;

      size_t len_avx_aligned_part = _align_to_power_of_two_downward_l2( node_len, ALIGN_FOR_AVX );
      packed_aligned_avx2_memset( new_node, 0, len_avx_aligned_part );
      unsigned int * ptr_cap = new_node + node_len;
      for ( unsigned int * ptr = new_node + len_avx_aligned_part;
          ptr < ptr_cap;
          ptr++ ) {
        *ptr = 0;
      }
      block_idx = 0;
    }

    size_t block_len = UINT64_C(1) << p_mal->log2_block_size;
    assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
    void * restrict malloc_for_this_list = local_malloc( userdata, CACHELINE_LEN, block_len );
    //void * restrict malloc_for_this_list = new_malloc +
    //  ( arr_al_indices[al_idx] << p_mal->log2_block_size );
    if ( !malloc_for_this_list ) {
      return emsg_out_of_memory;
    }
    node->arr_blocks[block_idx] = malloc_for_this_list;
    p_mal->arr_nr_active_blocks[arr_al_indices[idx]]++;
    if ( fill_new_blocks ) {
      //packed_aligned_avx2_memset( malloc_for_this_list, 0, block_len );
      const simde__m256i key_pack = simde_mm256_set1_epi64x( qw_filler );
      constexpr uint64_t stride = ALIGN_FOR_AVX;
      void * const restrict p_vectors =
        __builtin_assume_aligned( malloc_for_this_list, ALIGN_FOR_AVX );
      for ( uint64_t byte_idx = 0;
          byte_idx < UINT64_C(1) << p_mal->log2_block_size;
          byte_idx += stride ) {
        simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
      }
    }

  }  // for ( uint64_t idx = 0; idx < arr_len; idx++ )

  return 0;
}



//-------------------------------------------------------------------------------------------------
void infos_multi_al_destroy_except_preallocated_struct( struct infos_multi_arr_list * p_mal,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;

  for ( uint32_t list_idx = 0; list_idx < UINT64_C(1) << p_mal->log2_nr_lists; list_idx++ ) {
    struct linked_list_of_lists_node * restrict node = p_mal->lists[list_idx].head.next;
    typeof(p_mal->arr_nr_active_blocks[0]) block_id = 0;
    while ( node ) {
      constexpr uint_fast16_t local_cap = _arr_elem_cnt(node->arr_blocks);
      for ( uint_fast16_t local_idx = 0; local_idx < local_cap; local_idx++, block_id++ ) {
        if ( block_id >= p_mal->arr_nr_active_blocks[list_idx] ) {
          local_free( userdata, node );
          goto out_of_nested_loop;
        }
        void * block = node->arr_blocks[local_idx];
        local_free( userdata, block );
      }
      typeof(node) next = node;
      local_free( userdata, node );
      node = next;
    }
out_of_nested_loop:

    node = &p_mal->lists[list_idx].head;
    block_id = 0;
    constexpr uint_fast16_t local_cap = _arr_elem_cnt(node->arr_blocks);
    for ( uint_fast16_t local_idx = 1; local_idx < local_cap; local_idx++, block_id++ ) {
      if ( block_id >= p_mal->arr_nr_active_blocks[list_idx] ) {
        break;
      }
      void * block = node->arr_blocks[local_idx];
      local_free( userdata, block );
    }

  }  // for ( uint32_t list_idx = 0; list_idx < UINT64_C(1) << p_mal->log2_nr_lists; list_idx++ )


  local_free( userdata, p_mal->lists[0].head.arr_blocks[0] );
}

//-------------------------------------------------------------------------------------------------
void infos_multi_al_destroy( struct infos_multi_arr_list * restrict p_mal,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  infos_multi_al_destroy_except_preallocated_struct( p_mal, userdata, custom_free );
}



//-------------------------------------------------------------------------------------------------
void * infos_al_get_elem( struct infos_arr_list * restrict p_al,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    bool fill_new_blocks, uint64_t qw_filler,
    uint32_t elem_idx )
{
  void * block;
  const struct linked_list_of_lists_node * restrict node = &p_al->list.head;
  typeof(elem_idx) block_idx = elem_idx >> p_al->log2_nr_elems;
  typeof(block_idx) nr_blocks_to_generate = block_idx >= p_al->nr_active_blocks ?
    block_idx + 1 - p_al->nr_active_blocks : 0;
  if ( nr_blocks_to_generate ) {
    void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
    local_malloc = custom_malloc ? custom_malloc : default_malloc;
    enum err_msg err = infos_al_grow( p_al, userdata, local_malloc,
        nullptr, nullptr, nr_blocks_to_generate, fill_new_blocks, qw_filler );
    if ( err ) {
      return nullptr;
    }
  }
  while(-7) {
    if ( block_idx < _arr_elem_cnt(node->arr_blocks) ) {
      block = node->arr_blocks[block_idx];
      break;
    }
    block_idx -= _arr_elem_cnt(node->arr_blocks);
    assume( node->next );
    node = node->next;
  }

  return block +
    ( _reiterate_under_power_of_two_l2(elem_idx, p_al->log2_nr_elems) << p_al->log2_elem_size );
}

//-------------------------------------------------------------------------------------------------
void * infos_multi_al_get_elem( struct infos_multi_arr_list * p_mal,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    bool fill_new_blocks, uint64_t qw_filler,
    uint_fast16_t list_idx, uint32_t elem_idx )
{
  void * block;
  const struct linked_list_of_lists_node * restrict node = &p_mal->lists[list_idx].head;
  typeof(elem_idx) block_idx = elem_idx >> p_mal->log2_nr_elems;
  typeof(block_idx) nr_blocks_to_generate = block_idx >= p_mal->arr_nr_active_blocks[list_idx] ?
    block_idx + 1 - p_mal->arr_nr_active_blocks[list_idx] : 0;
  if ( nr_blocks_to_generate ) {
    void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
    local_malloc = custom_malloc ? custom_malloc : default_malloc;
    enum err_msg err = infos_multi_al_grow( p_mal, userdata, local_malloc,
        1, (uint32_t[]){list_idx}, fill_new_blocks, qw_filler );
    if ( err ) {
      return nullptr;
    }
  }
  while(-7) {
    if ( block_idx < _arr_elem_cnt(node->arr_blocks) ) {
      block = node->arr_blocks[block_idx];
      break;
    }
    block_idx -= _arr_elem_cnt(node->arr_blocks);
    assume( node->next );
    node = node->next;
  }

  return block +
    ( _reiterate_under_power_of_two_l2(elem_idx, p_mal->log2_nr_elems) << p_mal->log2_elem_size );
}


//-------------------------------------------------------------------------------------------------
void * infos_al_get_elem_cond32( const struct infos_arr_list * p_al,
    uint32_t first_slot_to_check, uint32_t cond )
{
  uint64_t block_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check, p_al->log2_nr_elems );
  uint64_t node_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check,
      p_al->log2_nr_elems + ct_log2(_arr_elem_cnt(p_al->list.head.arr_blocks)) );
  uint64_t node_base_idx = 0;
  const struct linked_list_of_lists_node * restrict node = &p_al->list.head;
  void * block;
  while(-7) {
    if ( node_base_idx == node_base_idx_first_slot_to_check ) {
      uint64_t block_idx =
        (block_base_idx_first_slot_to_check - node_base_idx_first_slot_to_check) >>
        p_al->log2_nr_elems;
      block = node->arr_blocks[block_idx];
      break;
    }
    node = node->next;
    assume( node );
    node_base_idx += UINT64_C(1) << (
        p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
  }

  const typeof(&cond) restrict p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

  // A quicker catch is achievable with first_slot_to_check
  typeof(first_slot_to_check) elem_idx = first_slot_to_check - block_base_idx_first_slot_to_check;
  if ( p_key[elem_idx] == cond ) {
    return p_key + elem_idx;
  }
  uint32_t cur_vec_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX;
  simde__m256i pack1, pack2, res1, res2;
  const simde__m256i key_pack = simde_mm256_set1_epi32( cond );
  pack1 = simde_mm256_load_si256( (void*)(p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond)) );
  res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
  unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
  if ( mask ) {
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask) );
  }
  void * p_sibling = (void*)( p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) +
      (cur_vec_idx & 1 ? -1 : 1) * ALIGN_FOR_AVX / sizeof(cond) );
  pack2 = simde_mm256_load_si256( p_sibling );
  res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
  unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
  if ( mask2 ) {
    return p_sibling + ( __tzcnt_u32(mask2) );
  }

  const uint64_t last_idx_plus1 = UINT64_C(1) << p_al->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride; idx < last_idx_plus1; idx += stride ) {
    pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
    pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
    res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
    res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
    mask = simde_mm256_movemask_ps( (simde__m256) res1 );
    mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
    if ( mask ) {
      return p_key + ( idx + __tzcnt_u32(mask) ); }
    if ( mask2 ) {
      return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
    }
  }

  if ( cur_batch_idx ) {
    for ( uint64_t idx = 0; idx < cur_batch_idx * stride; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        return p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }
  }

  uint64_t block_base_idx = block_base_idx_first_slot_to_check;
  block_base_idx += UINT64_C(1) << p_al->log2_nr_elems;
  const uint64_t block_base_idx_cap =
    (UINT64_C(1) << p_al->log2_nr_elems) * p_al->nr_active_blocks;
  uint64_t block_idx = (block_base_idx - node_base_idx) >> p_al->log2_nr_elems;
  if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
    node = node->next;
    if ( !node ) {
      goto from_zero_to_start;
    }
    node_base_idx += UINT64_C(1) << (
        p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
    block_idx = 0;
  }
  do {  // while ( block_base_idx < block_base_idx_cap )
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_al->log2_nr_elems;
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( block_base_idx < block_base_idx_cap );

from_zero_to_start:

  block_base_idx = 0;
  if ( block_base_idx == block_base_idx_first_slot_to_check ) {
    return nullptr;
  }
  block_idx = 0;
  node = &p_al->list.head;
  node_base_idx = 0;
  while (-7) {
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_al->log2_nr_elems;
    if ( block_base_idx == block_base_idx_first_slot_to_check ) {
      break;
    }
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
      block_idx = 0;
    }
  }

  return nullptr;
}

//-------------------------------------------------------------------------------------------------
void * infos_al_get_elem_cond64( const struct infos_arr_list * p_al,
    uint32_t first_slot_to_check, uint64_t cond )
{
  uint64_t block_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check, p_al->log2_nr_elems );
  uint64_t node_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check,
      p_al->log2_nr_elems + ct_log2(_arr_elem_cnt(p_al->list.head.arr_blocks)) );
  uint64_t node_base_idx = 0;
  const struct linked_list_of_lists_node * restrict node = &p_al->list.head;
  void * block;
  while(-7) {
    if ( node_base_idx == node_base_idx_first_slot_to_check ) {
      uint64_t block_idx =
        (block_base_idx_first_slot_to_check - node_base_idx_first_slot_to_check) >>
        p_al->log2_nr_elems;
      block = node->arr_blocks[block_idx];
      break;
    }
    node = node->next;
    assume( node );
    node_base_idx += UINT64_C(1) << (
        p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
  }

  const typeof(&cond) restrict p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

  // A quicker catch is achievable with first_slot_to_check
  typeof(first_slot_to_check) elem_idx = first_slot_to_check - block_base_idx_first_slot_to_check;
  if ( p_key[elem_idx] == cond ) {
    return p_key + elem_idx;
  }
  uint32_t cur_vec_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX;
  simde__m256i pack1, pack2, res1, res2;
  const simde__m256i key_pack = simde_mm256_set1_epi64x( cond );
  pack1 = simde_mm256_load_si256( (void*)(p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond)) );
  res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
  unsigned int mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
  if ( mask ) {
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask) );
  }
  void * p_sibling = (void*)( p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) +
      (cur_vec_idx & 1 ? -1 : 1) * ALIGN_FOR_AVX / sizeof(cond) );
  pack2 = simde_mm256_load_si256( p_sibling );
  res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
  unsigned int mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
  if ( mask2 ) {
    return p_sibling + ( __tzcnt_u32(mask2) );
  }

  const uint64_t last_idx_plus1 = UINT64_C(1) << p_al->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride; idx < last_idx_plus1; idx += stride ) {
    pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
    pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
    res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
    res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
    mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
    mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
    if ( mask ) {
      return p_key + ( idx + __tzcnt_u32(mask) ); }
    if ( mask2 ) {
      return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
    }
  }

  if ( cur_batch_idx ) {
    for ( uint64_t idx = 0; idx < cur_batch_idx * stride; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
      mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
      mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
      if ( mask ) {
        return p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }
  }

  uint64_t block_base_idx = block_base_idx_first_slot_to_check;
  block_base_idx += UINT64_C(1) << p_al->log2_nr_elems;
  const uint64_t block_base_idx_cap =
    (UINT64_C(1) << p_al->log2_nr_elems) * p_al->nr_active_blocks;
  uint64_t block_idx = (block_base_idx - node_base_idx) >> p_al->log2_nr_elems;
  if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
    node = node->next;
    if ( !node ) {
      goto from_zero_to_start;
    }
    node_base_idx += UINT64_C(1) << (
        p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
    block_idx = 0;
  }
  do {  // while ( block_base_idx < block_base_idx_cap )
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
      mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
      mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_al->log2_nr_elems;
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( block_base_idx < block_base_idx_cap );

from_zero_to_start:

  block_base_idx = 0;
  if ( block_base_idx == block_base_idx_first_slot_to_check ) {
    return nullptr;
  }
  block_idx = 0;
  node = &p_al->list.head;
  node_base_idx = 0;
  while (-7) {
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
      mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
      mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_al->log2_nr_elems;
    if ( block_base_idx == block_base_idx_first_slot_to_check ) {
      break;
    }
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_al->log2_nr_elems + _arr_elem_cnt(p_al->list.head.arr_blocks) );
      block_idx = 0;
    }
  }

  return nullptr;
}


//-------------------------------------------------------------------------------------------------
void * infos_multi_al_get_elem_cond32( const struct infos_multi_arr_list * p_mal,
    uint_fast16_t list_idx, uint32_t first_slot_to_check, uint32_t cond )
{
  uint64_t block_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check, p_mal->log2_nr_elems );
  uint64_t node_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check,
      p_mal->log2_nr_elems + ct_log2(_arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks)) );
  uint64_t node_base_idx = 0;
  const struct linked_list_of_lists_node * restrict node = &p_mal->lists[list_idx].head;
  void * block;
  while(-7) {
    if ( node_base_idx == node_base_idx_first_slot_to_check ) {
      uint64_t block_idx =
        (block_base_idx_first_slot_to_check - node_base_idx_first_slot_to_check) >>
        p_mal->log2_nr_elems;
      block = node->arr_blocks[block_idx];
      break;
    }
    node = node->next;
    assume( node );
    node_base_idx += UINT64_C(1) << (
        p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
  }

  const typeof(&cond) restrict p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

  // A quicker catch is achievable with first_slot_to_check
  typeof(first_slot_to_check) elem_idx = first_slot_to_check - block_base_idx_first_slot_to_check;
  if ( p_key[elem_idx] == cond ) {
    return p_key + elem_idx;
  }
  uint32_t cur_vec_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX;
  simde__m256i pack1, pack2, res1, res2;
  const simde__m256i key_pack = simde_mm256_set1_epi32( cond );
  pack1 = simde_mm256_load_si256( (void*)(p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond)) );
  res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
  unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
  if ( mask ) {
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask) );
  }
  void * p_sibling = (void*)( p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) +
      (cur_vec_idx & 1 ? -1 : 1) * ALIGN_FOR_AVX / sizeof(cond) );
  pack2 = simde_mm256_load_si256( p_sibling );
  res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
  unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
  if ( mask2 ) {
    return p_sibling + ( __tzcnt_u32(mask2) );
  }

  const uint64_t last_idx_plus1 = UINT64_C(1) << p_mal->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride; idx < last_idx_plus1; idx += stride ) {
    pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
    pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
    res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
    res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
    mask = simde_mm256_movemask_ps( (simde__m256) res1 );
    mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
    if ( mask ) {
      return p_key + ( idx + __tzcnt_u32(mask) ); }
    if ( mask2 ) {
      return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
    }
  }

  if ( cur_batch_idx ) {
    for ( uint64_t idx = 0; idx < cur_batch_idx * stride; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        return p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }
  }

  uint64_t block_base_idx = block_base_idx_first_slot_to_check;
  block_base_idx += UINT64_C(1) << p_mal->log2_nr_elems;
  const uint64_t block_base_idx_cap =
    (UINT64_C(1) << p_mal->log2_nr_elems) * p_mal->arr_nr_active_blocks[list_idx];
  uint64_t block_idx = (block_base_idx - node_base_idx) >> p_mal->log2_nr_elems;
  if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
    node = node->next;
    if ( !node ) {
      goto from_zero_to_start;
    }
    node_base_idx += UINT64_C(1) << (
        p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
    block_idx = 0;
  }
  do {  // while ( block_base_idx < block_base_idx_cap )
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_mal->log2_nr_elems;
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
      block_idx = 0;
    }
  } while ( block_base_idx < block_base_idx_cap );

from_zero_to_start:

  block_base_idx = 0;
  if ( block_base_idx == block_base_idx_first_slot_to_check ) {
    return nullptr;
  }
  block_idx = 0;
  node = &p_mal->lists[list_idx].head;
  node_base_idx = 0;
  while (-7) {
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_mal->log2_nr_elems;
    if ( block_base_idx == block_base_idx_first_slot_to_check ) {
      break;
    }
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
      block_idx = 0;
    }
  }

  return nullptr;
}

//-------------------------------------------------------------------------------------------------
void * infos_multi_al_get_elem_cond64( const struct infos_multi_arr_list * p_mal,
    uint_fast16_t list_idx, uint32_t first_slot_to_check, uint64_t cond )
{
  uint64_t block_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check, p_mal->log2_nr_elems );
  uint64_t node_base_idx_first_slot_to_check = _align_to_power_of_two_downward_l2(
      first_slot_to_check,
      p_mal->log2_nr_elems + ct_log2(_arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks)) );
  uint64_t node_base_idx = 0;
  const struct linked_list_of_lists_node * restrict node = &p_mal->lists[list_idx].head;
  void * block;
  while(-7) {
    if ( node_base_idx == node_base_idx_first_slot_to_check ) {
      uint64_t block_idx =
        (block_base_idx_first_slot_to_check - node_base_idx_first_slot_to_check) >>
        p_mal->log2_nr_elems;
      block = node->arr_blocks[block_idx];
      break;
    }
    node = node->next;
    assume( node );
    node_base_idx += UINT64_C(1) << (
        p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
  }

  const typeof(&cond) restrict p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

  // A quicker catch is achievable with first_slot_to_check
  typeof(first_slot_to_check) elem_idx = first_slot_to_check - block_base_idx_first_slot_to_check;
  if ( p_key[elem_idx] == cond ) {
    return p_key + elem_idx;
  }
  uint32_t cur_vec_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX;
  simde__m256i pack1, pack2, res1, res2;
  const simde__m256i key_pack = simde_mm256_set1_epi64x( cond );
  pack1 = simde_mm256_load_si256( (void*)(p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond)) );
  res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
  unsigned int mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
  if ( mask ) {
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask) );
  }
  void * p_sibling = (void*)( p_key + cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) +
      (cur_vec_idx & 1 ? -1 : 1) * ALIGN_FOR_AVX / sizeof(cond) );
  pack2 = simde_mm256_load_si256( p_sibling );
  res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
  unsigned int mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
  if ( mask2 ) {
    return p_sibling + ( __tzcnt_u32(mask2) );
  }

  const uint64_t last_idx_plus1 = UINT64_C(1) << p_mal->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * elem_idx / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride; idx < last_idx_plus1; idx += stride ) {
    pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
    pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
    res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
    res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
    mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
    mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
    if ( mask ) {
      return p_key + ( idx + __tzcnt_u32(mask) ); }
    if ( mask2 ) {
      return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
    }
  }

  if ( cur_batch_idx ) {
    for ( uint64_t idx = 0; idx < cur_batch_idx * stride; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
      mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
      mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
      if ( mask ) {
        return p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }
  }

  uint64_t block_base_idx = block_base_idx_first_slot_to_check;
  block_base_idx += UINT64_C(1) << p_mal->log2_nr_elems;
  const uint64_t block_base_idx_cap =
    (UINT64_C(1) << p_mal->log2_nr_elems) * p_mal->arr_nr_active_blocks[list_idx];
  uint64_t block_idx = (block_base_idx - node_base_idx) >> p_mal->log2_nr_elems;
  if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
    node = node->next;
    if ( !node ) {
      goto from_zero_to_start;
    }
    node_base_idx += UINT64_C(1) << (
        p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
    block_idx = 0;
  }
  do {  // while ( block_base_idx < block_base_idx_cap )
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
      mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
      mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_mal->log2_nr_elems;
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
      block_idx = 0;
    }
  } while ( block_base_idx < block_base_idx_cap );

from_zero_to_start:

  block_base_idx = 0;
  if ( block_base_idx == block_base_idx_first_slot_to_check ) {
    return nullptr;
  }
  block_idx = 0;
  node = &p_mal->lists[list_idx].head;
  node_base_idx = 0;
  while (-7) {
    block = node->arr_blocks[block_idx];
    typeof(&cond) restrict il_p_key = __builtin_assume_aligned( block, ALIGN_FOR_AVX512 );

    for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(il_p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(il_p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
      mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
      mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
      if ( mask ) {
        return il_p_key + ( idx + __tzcnt_u32(mask) ); }
      if ( mask2 ) {
        return il_p_key + ( idx + ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
      }
    }

    block_idx++;
    block_base_idx += UINT64_C(1) << p_mal->log2_nr_elems;
    if ( block_base_idx == block_base_idx_first_slot_to_check ) {
      break;
    }
    if ( block_idx >= _arr_elem_cnt(node->arr_blocks) ) {
      node = node->next;
      assume( node );
      node_base_idx += UINT64_C(1) << (
          p_mal->log2_nr_elems + _arr_elem_cnt(p_mal->lists[list_idx].head.arr_blocks) );
      block_idx = 0;
    }
  }

  return nullptr;
}



