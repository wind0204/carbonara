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
    return 0;
  }
  if ( zero_all ) {
    memset( ptr + ofs_data, 0, node_size );
  }

  struct infos_arr_list * al = ptr + ofs_itself;

  assume( _is_power_of_two(node_size) );
  al->log2_node_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( elem_size );
  al->log2_nr_elems = al->log2_node_size - al->log2_elem_size;
  //uint32_t elem_cnt = node_size / elem_size;
  //al->elem_cnt_per_node = elem_cnt <= (typeof(al->elem_cnt_per_node)) ~0 ? elem_cnt : 0;

  al->list.head.data = ptr + ofs_data;

  infos_take_care_of_linked_list_after_putting_head( &al->list );

  return al;
}

//-------------------------------------------------------------------------------------------------
struct infos_arr_list * infos_al_create_prealloc( struct infos_arr_list * restrict al,
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
    return 0;
  }
  if ( zero_all ) {
    memset( ptr, 0, node_size );
  }

  assume( _is_power_of_two(node_size) );
  al->log2_node_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( elem_size );
  al->log2_nr_elems = al->log2_node_size - al->log2_elem_size;
  //uint32_t elem_cnt = node_size / elem_size;
  //al->elem_cnt_per_node = elem_cnt <= (typeof(al->elem_cnt_per_node)) ~0 ? elem_cnt : 0;

  al->list.head.data = ptr;

  infos_take_care_of_linked_list_after_putting_head( &al->list );

  return al;

}



//-------------------------------------------------------------------------------------------------
enum err_msg infos_al_grow(
    struct infos_arr_list * restrict al,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast16_t nr_nodes,
    bool zero_all
    )
{
  assume( nr_nodes > 0 );

  uint_fast8_t out_of_align_diff1 = 0;
  uint_fast8_t sum_ooa_diff = 0;
  uint_fast32_t sum_new_nodes_len = nr_nodes * (1 << al->log2_node_size);
  assume( 1 << al->log2_node_size >= alignof(struct infos_singly_linked_list_node) );
  //if ( sum_new_nodes_len % alignof(struct infos_singly_linked_list_node) ) {
  //  out_of_align_diff1 = _align_to_power_of_two(
  //      sum_new_nodes_len, alignof(struct infos_singly_linked_list_node) )
  //    - sum_new_nodes_len;
  //  sum_ooa_diff += out_of_align_diff1;
  //}

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  void * restrict ptr = local_malloc( userdata, CACHELINE_LEN,
      sum_ooa_diff + sum_new_nodes_len + sizeof(struct infos_singly_linked_list_node) );
  if ( !ptr ) {
    //ptr = default_malloc( userdata, CACHELINE_LEN,
    //    sum_ooa_diff + sum_new_nodes_len + sizeof(struct infos_singly_linked_list_node) );
    if ( !ptr ) {
      return err_msg_out_of_memory;
    }
  }
  if ( zero_all ) {
    memset( ptr, 0, sum_new_nodes_len );
  }

  // Just in case custom_malloc allocates an aligned string for SSE/AVX or
  // a 64-byte aligned string for cache line separation
  uint32_t ofs_data = 0;
  uint32_t ofs_node = ofs_data + sum_new_nodes_len + out_of_align_diff1;

  struct infos_singly_linked_list_node * old_tail = infos_ll_get_last_node(&al->list.head);
  struct infos_singly_linked_list_node * new_node = ptr + ofs_node;

  new_node->next = 0;
  new_node->data = ptr + ofs_data;

  old_tail->next = new_node;
  al->list.nr_nodes += nr_nodes;

  return 0;
}



//struct infos_linked_list_node * infos_al_get_node(struct infos_arr_list * al, uint16_t idx_node) {
//  return infos_ll_get_node(&al->list, idx_node);
//}



//-------------------------------------------------------------------------------------------------
void infos_al_destroy_except_preallocated_struct( struct infos_arr_list * restrict al,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr) )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;
  local_free( userdata, al->list.head.data );

  struct infos_singly_linked_list_node * restrict node = al->list.head.next;
  while ( node ) {
    local_free( userdata, node->data );
    node = node->next;
  }

}

//-------------------------------------------------------------------------------------------------
void infos_al_destroy( struct infos_arr_list * restrict al, void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr) )
{
  struct infos_singly_linked_list_node * restrict node = al->list.head.next;

  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;

  while ( node ) {
    struct infos_singly_linked_list_node * restrict next_node = node->next;
    //uint_fast8_t out_of_align_diff1 = 0;
    //uint_fast8_t sum_ooa_diff = 0;
    //if ( al->node_size % alignof(struct infos_singly_linked_list_node) ) {
    //  out_of_align_diff1 = _align_to_power_of_two( al->node_size,
    //      alignof(struct infos_singly_linked_list_node) )
    //    - al->node_size;
    //  sum_ooa_diff += out_of_align_diff1;
    //}
    local_free( userdata, node->data );
    node = next_node;
  }

  //uint_fast8_t out_of_align_diff;  // distance between the first node's data and the first struct

  ////if ( params->node_size % alignof(struct infos_arr_list) ) {
  //out_of_align_diff = _align_to_power_of_two( al->node_size, alignof(struct infos_arr_list) )
  //  - al->node_size;

  local_free( userdata, al->list.head.data );
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
  assume( log2_node_size >= 31 - 25 );  // assume(
                                        // log2_node_size >= 31 - _lzcnt_u32(ALIGN_FOR_AVX512) );
                                        // clang ignored the expression above when building Release
                                        // build saying there is a (potential) side effect.

  // Just in case custom_malloc allocates an aligned string for SSE/AVX or
  // a 64-byte aligned string for cache line separation, put data first up front.
  uint_fast8_t out_of_align_diff = 0;  // distance between the array of structs
                                       // infos_singly_linked_list_fh and the struct
                                       // infos_multi_arr_list

  // Put either an array infos_singly_linked_list_fh or infos_multi_arr_list right after the data
  uint64_t ofs_itself, ofs_list_fh;
  if ( alignof(struct infos_multi_arr_list) > alignof(struct infos_singly_linked_list_fh) ) {
    if ( (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size)) %
        alignof(struct infos_multi_arr_list) ) {
      out_of_align_diff = _align_to_power_of_two(
          UINT64_C(1) << (log2_nr_arr_lists + log2_node_size),
          alignof(struct infos_multi_arr_list)
          ) - (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size));
    }
    ofs_itself = ( UINT64_C(1) << log2_nr_arr_lists << log2_node_size ) + out_of_align_diff;
    ofs_list_fh = ofs_itself + sizeof(struct infos_multi_arr_list);

  } else {
    if ( (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size)) %
        alignof(struct infos_singly_linked_list_fh) ) {
      out_of_align_diff = _align_to_power_of_two(
          UINT64_C(1) << (log2_nr_arr_lists + log2_node_size),
          alignof(struct infos_singly_linked_list_fh)
          ) - (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size));
    }
    ofs_list_fh = ( UINT64_C(1) << log2_nr_arr_lists << log2_node_size ) + out_of_align_diff;
    ofs_itself = ofs_list_fh + sizeof(struct infos_singly_linked_list_fh);
  }

  void * restrict ptr;
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
  ptr = local_malloc( userdata, CACHELINE_LEN,
      out_of_align_diff + (UINT64_C(1) << log2_nr_arr_lists) * ((UINT64_C(1) << log2_node_size) +
        sizeof(struct infos_singly_linked_list_fh)) + sizeof(struct infos_multi_arr_list) );
  if ( !ptr ) {
    return 0;
  }
  constexpr uint64_t ofs_data = 0;  // the first node's data should be in the front.
  if ( zero_all ) {
    memset( ptr + ofs_data, 0, UINT64_C(1) << log2_node_size << log2_nr_arr_lists );
  }

  struct infos_multi_arr_list * restrict mal = ptr + ofs_itself;

  mal->log2_node_size = log2_node_size;
  mal->log2_elem_size = log2_elem_size;
  mal->log2_nr_lists = log2_nr_arr_lists;
  mal->log2_nr_elems = log2_node_size - log2_elem_size;
  //uint32_t elem_cnt = node_size / elem_size;
  //mal->elem_cnt_per_node = elem_cnt <= (typeof(mal->elem_cnt_per_node)) ~0 ? elem_cnt : 0;

  mal->lists = ptr + ofs_list_fh;
  for ( uint64_t i = 0; i < UINT64_C(1) << log2_nr_arr_lists; i++ ) {
    mal->lists[i].head.data = ptr + ofs_data + (i << log2_node_size);
    infos_take_care_of_linked_list_after_putting_head( &mal->lists[i] );
  }

  return mal;
}

//-------------------------------------------------------------------------------------------------
uint_fast8_t infos_multi_al_init_prealloc(
    struct infos_multi_arr_list * restrict mal,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast8_t log2_node_size, uint_fast8_t log2_elem_size, uint_fast8_t log2_nr_arr_lists,
    bool zero_all
    )
{
  assume( log2_nr_arr_lists > 0 );
  assume( log2_node_size >= 31 - 25 );  // assume(
                                        // log2_node_size >= 31 - _lzcnt_u32(ALIGN_FOR_AVX512) );
                                        // clang ignored the expression above when building Release
                                        // build saying there is a (potential) side effect.

  uint_fast8_t out_of_align_diff = 0;  // distance between the first node's data and the next struct
  //if ( nr_arr_lists * node_size % alignof(struct infos_singly_linked_list_fh) ) {
  //  out_of_align_diff = _align_to_power_of_two(
  //      (nr_arr_lists * node_size), alignof(struct infos_singly_linked_list_fh)
  //      ) - (nr_arr_lists * node_size);
  //}
  if (unlikely( log2_nr_arr_lists + log2_node_size >=
      31 - _lzcnt_u32(alignof(struct infos_singly_linked_list_fh)) )) {
    out_of_align_diff = _align_to_power_of_two(
        UINT64_C(1) << (log2_nr_arr_lists + log2_node_size),
        alignof(struct infos_singly_linked_list_fh)
        ) - (UINT64_C(1) << (log2_nr_arr_lists + log2_node_size));
  }

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
  void * restrict ptr = local_malloc( userdata, CACHELINE_LEN, out_of_align_diff +
      (UINT64_C(1) << log2_nr_arr_lists) *
      ( (UINT64_C(1) << log2_node_size) + sizeof(struct infos_singly_linked_list_fh) )
      );
  if ( !ptr ) {
    return err_msg_no_error;
  }
  if ( zero_all ) {
    memset( ptr, 0, UINT64_C(1) << log2_node_size << log2_nr_arr_lists );
  }

  mal->log2_node_size = log2_node_size;
  mal->log2_elem_size = log2_elem_size;
  mal->log2_nr_lists = log2_nr_arr_lists;
  mal->log2_nr_elems = log2_node_size - log2_elem_size;
  //uint32_t elem_cnt = node_size / elem_size;
  //mal->elem_cnt_per_node = elem_cnt <= (typeof(mal->elem_cnt_per_node)) ~0 ? elem_cnt : 0;

  mal->lists = ptr + ( UINT64_C(1) << log2_node_size << log2_nr_arr_lists ) + out_of_align_diff;
  for ( uint32_t i = 0; i < UINT64_C(1) << log2_nr_arr_lists; i++ ) {
    mal->lists[i].head.data = ptr + ( i << log2_node_size );
    infos_take_care_of_linked_list_after_putting_head( &mal->lists[i] );
  }

  return 0;
}



//-------------------------------------------------------------------------------------------------
enum err_msg infos_multi_al_grow( struct infos_multi_arr_list * restrict mal,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t arr_len,
    const uint32_t idx_al[arr_len],
    bool zero_all
    )
{
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  uint_fast8_t out_of_align_diff1 = 0;
  assume( 1 << mal->log2_node_size >= alignof(struct infos_singly_linked_list_node) );
  //if ( (1 << mal->log2_node_size) % alignof(struct infos_singly_linked_list_node) ) {
  //  out_of_align_diff1 = _align_to_power_of_two( (1 << mal->log2_node_size),
  //      alignof(struct infos_singly_linked_list_node) ) - (1 << mal->log2_node_size);
  //}

  // Just in case custom_malloc allocates an aligned string for SSE/AVX or
  // a 64-byte aligned string for cache line separation
  uint32_t ofs_data = 0;
  uint32_t ofs_node = ofs_data + (1 << mal->log2_node_size) + out_of_align_diff1;
  for ( uint32_t i = 0; i < arr_len; i++ ) {
    assume( CACHELINE_LEN >= ALIGN_FOR_AVX512 );
    void * restrict ptr = local_malloc( userdata, CACHELINE_LEN,
        out_of_align_diff1 +
        (1 << mal->log2_node_size) + sizeof(struct infos_singly_linked_list_node) );
    if ( !ptr ) {
      return err_msg_out_of_memory;
    }
    if ( !ptr ) {
      //ptr = default_malloc( userdata, CACHELINE_LEN,
      //    out_of_align_diff1 +
      //    (1 << mal->log2_node_size) + sizeof(struct infos_singly_linked_list_node) );
      if ( !ptr ) {
        return err_msg_out_of_memory;
      }
    }
    if ( zero_all ) {
      memset( ptr, 0, (1 << mal->log2_node_size) );
    }

    struct infos_singly_linked_list_node * old_tail =
      infos_ll_get_last_node(&mal->lists[idx_al[i]].head);
    struct infos_singly_linked_list_node * new_node = ptr + ofs_node;

    new_node->next = 0;
    new_node->data = ptr + ofs_data;

    old_tail->next = new_node;
    mal->lists[idx_al[i]].nr_nodes++;
  }

  return 0;
}



//-------------------------------------------------------------------------------------------------
void infos_multi_al_destroy_except_preallocated_struct( struct infos_multi_arr_list * mal,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;

  for ( uint32_t i = 0; i < (1 << mal->log2_nr_lists); i++ ) {
    struct infos_singly_linked_list_node * restrict node = mal->lists[i].head.next;
    while ( node ) {
      struct infos_singly_linked_list_node * restrict next_node = node->next;

      //uint_fast8_t out_of_align_diff1 = 0;
      //if ( mal->node_size % alignof(struct infos_singly_linked_list_node) ) {
      //  out_of_align_diff1 =
      //    _align_to_power_of_two( mal->node_size, alignof(struct infos_singly_linked_list_node) )
      //    - mal->node_size;
      //}
      local_free( userdata, node->data );

      node = next_node;
    }

  }

  //uint_fast8_t out_of_align_diff = 0;  // distance between the first node's data and the next struct
  //if ( mal->nr_arr_lists * mal->node_size % alignof(struct infos_singly_linked_list_fh) ) {
  //  out_of_align_diff = _align_to_power_of_two(
  //      mal->nr_arr_lists * mal->node_size, alignof(struct infos_singly_linked_list_fh)
  //      ) - mal->nr_arr_lists * mal->node_size;
  //}

  local_free( userdata, mal->lists[0].head.data );
}

//-------------------------------------------------------------------------------------------------
void infos_multi_al_destroy( struct infos_multi_arr_list * restrict mal,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;

  for ( uint32_t i = 0; i < (1 << mal->log2_nr_lists); i++ ) {
    struct infos_singly_linked_list_node * restrict node = mal->lists[i].head.next;
    while ( node ) {
      struct infos_singly_linked_list_node * restrict next_node = node->next;

      //uint_fast8_t out_of_align_diff1 = 0;
      //if ( mal->node_size % alignof(struct infos_singly_linked_list_node) ) {
      //  out_of_align_diff1 = _align_to_power_of_two( mal->node_size, alignof(struct infos_singly_linked_list_node) )
      //    - mal->node_size;
      //}
      //local_free( userdata, node->data );

      node = next_node;
    }

  }

  //uint_fast8_t out_of_align_diff = 0;  // distance between the first node's data and the first struct
  //uint64_t ofs_itself, ofs_list_fh;
  //if ( alignof(struct infos_multi_arr_list) > alignof(struct infos_singly_linked_list_fh) ) {
  //  if ( mal->nr_arr_lists * mal->node_size % alignof(struct infos_multi_arr_list) ) {
  //    out_of_align_diff = _align_to_power_of_two(
  //        mal->nr_arr_lists * mal->node_size, alignof(struct infos_multi_arr_list)
  //        ) - mal->nr_arr_lists * mal->node_size;
  //  }
  //} else {
  //  if ( mal->nr_arr_lists * mal->node_size % alignof(struct infos_singly_linked_list_fh) ) {
  //    out_of_align_diff = _align_to_power_of_two(
  //        mal->nr_arr_lists * mal->node_size, alignof(struct infos_singly_linked_list_fh)
  //        ) - mal->nr_arr_lists * mal->node_size;
  //  }
  //}
  
  local_free( userdata, mal->lists[0].head.data );
}



//-------------------------------------------------------------------------------------------------
void * infos_al_get_elem( const struct infos_arr_list * al, uint32_t idx_elem )
{
  const struct infos_singly_linked_list_node * restrict node = &al->list.head;
  uint64_t node_idx = idx_elem << al->log2_elem_size >> al->log2_node_size;
  idx_elem = idx_elem - (node_idx << al->log2_node_size >> al->log2_elem_size);
  for ( uint32_t i = 0; i < node_idx; i++ ) {
    if ( !node->next ) {
      return 0;
    }
    node = node->next;
  }

  return node->data + (idx_elem << al->log2_elem_size);
}

//-------------------------------------------------------------------------------------------------
void * infos_multi_al_get_elem( const struct infos_multi_arr_list * mal,
    uint_fast16_t idx_al, uint32_t idx_elem )
{
  struct infos_singly_linked_list_node * restrict node = &mal->lists[idx_al].head;
  uint64_t node_idx = idx_elem << mal->log2_elem_size >> mal->log2_node_size;
  idx_elem = idx_elem - (node_idx << mal->log2_node_size >> mal->log2_elem_size);
  for ( uint32_t i = 0; i < node_idx; i++ ) {
    if ( !node->next ) {
      return 0;
    }
    node = node->next;
  }

  return node->data + (idx_elem << mal->log2_elem_size);
}


//-------------------------------------------------------------------------------------------------
void * infos_al_get_elem_cond32( const struct infos_arr_list * al,
    uint32_t first_slot_to_check, uint32_t cond )
{
  typeof(first_slot_to_check) idx_elem = first_slot_to_check;
  const struct infos_singly_linked_list_node * restrict node = &al->list.head;
  uint64_t node_idx = idx_elem << al->log2_elem_size >> al->log2_node_size;
  idx_elem = idx_elem - (node_idx << al->log2_node_size >> al->log2_elem_size);
  for ( uint32_t i = 0; i < node_idx; i++ ) {
    if ( !node->next ) {
      return 0;
    }
    node = node->next;
  }

  const typeof(&cond) restrict p_key =
    __builtin_assume_aligned( node->data, ALIGN_FOR_AVX512 );
  // A quick return is possible thanks to first_slot_to_check
  if ( p_key[idx_elem] == cond ) {
    return p_key + idx_elem;
  }
  uint32_t cur_vec_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX;
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
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
  }
  const uint64_t last_idx = UINT64_C(1) << al->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride; idx < last_idx; idx += stride ) {
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

  return nullptr;
}

//-------------------------------------------------------------------------------------------------
void * infos_al_get_elem_cond64( const struct infos_arr_list * al,
    uint32_t first_slot_to_check, uint64_t cond )
{
  typeof(first_slot_to_check) idx_elem = first_slot_to_check;
  const struct infos_singly_linked_list_node * restrict node = &al->list.head;
  uint64_t node_idx = idx_elem << al->log2_elem_size >> al->log2_node_size;
  idx_elem = idx_elem - (node_idx << al->log2_node_size >> al->log2_elem_size);
  for ( uint32_t i = 0; i < node_idx; i++ ) {
    if ( !node->next ) {
      return 0;
    }
    node = node->next;
  }

  const typeof(&cond) restrict p_key =
    __builtin_assume_aligned( node->data, ALIGN_FOR_AVX512 );
  // A quick return is possible thanks to first_slot_to_check
  if ( p_key[idx_elem] == cond ) {
    return p_key + idx_elem;
  }
  uint32_t cur_vec_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX;
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
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
  }

  const uint64_t last_idx = UINT64_C(1) << al->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride + stride; idx < last_idx; idx += stride ) {
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

  return nullptr;
}


//-------------------------------------------------------------------------------------------------
void * infos_multi_al_get_elem_cond32( const struct infos_multi_arr_list * mal,
    uint_fast16_t idx_al, uint32_t first_slot_to_check, uint32_t cond )
{
  typeof(first_slot_to_check) idx_elem = first_slot_to_check;
  const struct infos_singly_linked_list_node * restrict node = &mal->lists[idx_al].head;
  uint64_t node_idx = idx_elem << mal->log2_elem_size >> mal->log2_node_size;
  idx_elem = idx_elem - (node_idx << mal->log2_node_size >> mal->log2_elem_size);
  for ( uint32_t i = 0; i < node_idx; i++ ) {
    if ( !node->next ) {
      return 0;
    }
    node = node->next;
  }

  const typeof(&cond) restrict p_key =
    __builtin_assume_aligned( node->data, ALIGN_FOR_AVX512 );
  // A quick return is possible thanks to first_slot_to_check
  if ( p_key[idx_elem] == cond ) {
    return p_key + idx_elem;
  }
  uint32_t cur_vec_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX;
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
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
  }
  const uint64_t last_idx = UINT64_C(1) << mal->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride; idx < last_idx; idx += stride ) {
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

  return nullptr;
}

//-------------------------------------------------------------------------------------------------
void * infos_multi_al_get_elem_cond64( const struct infos_multi_arr_list * mal,
    uint_fast16_t idx_al, uint32_t first_slot_to_check, uint64_t cond )
{
  typeof(first_slot_to_check) idx_elem = first_slot_to_check;
  const struct infos_singly_linked_list_node * restrict node = &mal->lists[idx_al].head;
  uint64_t node_idx = idx_elem << mal->log2_elem_size >> mal->log2_node_size;
  idx_elem = idx_elem - (node_idx << mal->log2_node_size >> mal->log2_elem_size);
  for ( uint32_t i = 0; i < node_idx; i++ ) {
    if ( !node->next ) {
      return 0;
    }
    node = node->next;
  }

  const typeof(&cond) restrict p_key =
    __builtin_assume_aligned( node->data, ALIGN_FOR_AVX512 );
  // A quick return is possible thanks to first_slot_to_check
  if ( p_key[idx_elem] == cond ) {
    return p_key + idx_elem;
  }
  uint32_t cur_vec_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX;
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
    return p_key +
      ( cur_vec_idx * ALIGN_FOR_AVX / sizeof(cond) + __tzcnt_u32(mask2) );
  }

  const uint64_t last_idx = UINT64_C(1) << mal->log2_nr_elems;
  constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(cond);
  uint32_t cur_batch_idx = sizeof(cond) * idx_elem / ALIGN_FOR_AVX512;
  for ( uint64_t idx = cur_batch_idx * stride + stride; idx < last_idx; idx += stride ) {
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

  return nullptr;
}



