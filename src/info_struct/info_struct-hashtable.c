#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
    || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  #ifdef _MSC_VER
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
#endif
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>

#include "err_msg.h"
#include "info_struct.h"
#include "util-c_lang-basic.h"



//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize an array of uint32_t's for bookkeeping the components of
 *  struct infos_multi_arr_list.
 * \param arr_cursors An array of cursors which indicates the unoccupied element closest to origin
 */
__attribute__((always_inline))
static inline void infos_mal_simple_mgr_init(
    uint32_t * restrict arr_cursors,
    struct infos_multi_arr_list * restrict mal )
{
  for ( uint_fast8_t al_idx = 0; al_idx < (1 << mal->log2_nr_lists);
      al_idx++ ) {
    arr_cursors[al_idx] = 0;
    // memset( params->mal->lists[idx_manager].head.data, 0, params->mal->node_size );
  }
}

//-------------------------------------------------------------------------------------------------
struct infos_hashtable * infos_hashtable_create(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void (*custom_free)(void * restrict userdata, void * restrict ptr),
    uint32_t key_arr_len, uint_fast16_t item_size, uint8_t log2_nr_buckets )
{
  //uint64_t nr_buckets = UINT64_C(1) << log2_nr_buckets;

  // Prepare an array of uint32_t arr_idx_next_unoccupied_elem
  uint64_t ofs_itself, ofs_arr_sm;
  if ( alignof(struct infos_hashtable) >
      _alignof_arr_elem(_typeof_member(struct infos_hashtable, arr_idx_next_unoccupied_elem)) ) {
    ofs_itself = 0;
    ofs_arr_sm = ofs_itself + sizeof(struct infos_hashtable);
  } else {
    ofs_arr_sm = 0;
    ofs_itself = ofs_arr_sm + (UINT64_C(1) << log2_nr_buckets) *
      _sizeof_arr_elem(_typeof_member(struct infos_hashtable, arr_idx_next_unoccupied_elem));
  }

  void * ptr;
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  ptr = local_malloc( userdata,
      alignof(max_align_t),
      _align_to_power_of_two( sizeof(struct infos_hashtable) + (UINT64_C(1) << log2_nr_buckets) *
        _sizeof_arr_elem(_typeof_member(struct infos_hashtable, arr_idx_next_unoccupied_elem)),
        alignof(max_align_t) )
      );

  if ( !ptr ) {
    return nullptr;
  }
  struct infos_hashtable * new_table = ptr + ofs_itself;
  new_table->arr_idx_next_unoccupied_elem = ptr + ofs_arr_sm;

  // Prepare key buckets
  uint_fast8_t ret = infos_multi_al_init_prealloc( &new_table->keys,
      userdata,
      custom_malloc,
      get_log2( key_arr_len ),
      31 - _lzcnt_u32( sizeof(uint32_t) ),
      log2_nr_buckets,
      true );
  if ( ret ) {  // Failed to prepare a struct infos_multi_arr_list
    local_free( userdata, ptr );
    return nullptr;
  }

  infos_mal_simple_mgr_init( new_table->arr_idx_next_unoccupied_elem, &new_table->keys );
  new_table->log2_nr_keys = new_table->keys.log2_block_size - new_table->keys.log2_elem_size;

  // Prepare arrays of values
  ret = infos_multi_al_init_prealloc( &new_table->values,
      userdata,
      custom_malloc,
      new_table->keys.log2_elem_cnt + ( 31 - _lzcnt_u32(item_size) ),
      31 - _lzcnt_u32(item_size),
      log2_nr_buckets,
      false );
  if ( ret ) {  // Failed to prepare a struct infos_multi_arr_list
    infos_multi_al_destroy_except_preallocated_struct( &new_table->keys,
        userdata, local_free );
    local_free( userdata, ptr );
    return nullptr;
  }

  return new_table;
}


//-------------------------------------------------------------------------------------------------
void infos_hashtable_destroy(
    struct infos_hashtable * restrict p_table,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  if ( ! p_table ) {
    return;
  }

  // Get the offset
  uint64_t ofs_itself, ofs_arr_sm;
  if ( alignof(struct infos_hashtable) >
      _alignof_arr_elem(_typeof_member(struct infos_hashtable, arr_idx_next_unoccupied_elem)) ) {
    ofs_itself = 0;
    ofs_arr_sm = ofs_itself + sizeof(struct infos_hashtable);
  } else {
    ofs_arr_sm = 0;
    ofs_itself = ofs_arr_sm + (UINT64_C(1) << p_table->keys.log2_nr_lists) *
      _sizeof_arr_elem(_typeof_member(struct infos_hashtable, arr_idx_next_unoccupied_elem));
  }

  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;

  infos_multi_al_destroy_except_preallocated_struct( &p_table->keys, userdata, local_free );
  infos_multi_al_destroy_except_preallocated_struct( &p_table->values, userdata, local_free );
  local_free( userdata, (void*)p_table - ofs_itself );
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert_6(
    struct infos_hashtable * restrict p_table,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t key,
    uint32_t bucket_idx,
    const void * restrict p_value )
{
  auto idx_first_empty_element = p_table->arr_idx_next_unoccupied_elem[bucket_idx];

  typeof(&key) restrict p_key_slot = infos_multi_al_get_elem(
      &p_table->keys, userdata, custom_malloc, true, 0, bucket_idx, idx_first_empty_element );
  void * restrict p_value_slot = infos_multi_al_get_elem(
      &p_table->values, userdata, custom_malloc, true, 0, bucket_idx, idx_first_empty_element );

  *p_key_slot = key;
  memcpy(p_value_slot, p_value, UINT64_C(1) << p_table->values.log2_elem_size);
  p_table->arr_idx_next_unoccupied_elem[bucket_idx]++;

  return 0;
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert_u32_6(
    struct infos_hashtable * restrict p_table,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t key,
    uint32_t bucket_idx,
    const uint32_t value )
{
  auto idx_first_empty_element = p_table->arr_idx_next_unoccupied_elem[bucket_idx];

  typeof(&key) restrict p_key_slot = infos_multi_al_get_elem(
      &p_table->keys, userdata, custom_malloc, true, 0, bucket_idx, idx_first_empty_element );
  void * restrict p_value_slot = infos_multi_al_get_elem(
      &p_table->values, userdata, custom_malloc, true, 0, bucket_idx, idx_first_empty_element );

  *p_key_slot = key;
  deconst_typeof(&value) p_value2 = p_value_slot;
  *p_value2 = value;
  p_table->arr_idx_next_unoccupied_elem[bucket_idx]++;

  return 0;
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert_u64_6(
    struct infos_hashtable * restrict p_table,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t key,
    uint32_t bucket_idx,
    const uint64_t p_value )
{
  auto idx_first_empty_element = p_table->arr_idx_next_unoccupied_elem[bucket_idx];

  typeof(&key) restrict p_key_slot = infos_multi_al_get_elem(
      &p_table->keys, userdata, custom_malloc, true, 0, bucket_idx, idx_first_empty_element );
  void * restrict p_value_slot = infos_multi_al_get_elem(
      &p_table->values, userdata, custom_malloc, true, 0, bucket_idx, idx_first_empty_element );

  *p_key_slot = key;
  deconst_typeof(&p_value) p_value2 = p_value_slot;
  *p_value2 = p_value;
  p_table->arr_idx_next_unoccupied_elem[bucket_idx]++;

  return 0;
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_remove_3( struct infos_hashtable * p_table,
    uint32_t key,
    uint32_t bucket_idx )
{
  uint64_t last_elem_idx = p_table->arr_idx_next_unoccupied_elem[bucket_idx] - 1;

  const uint64_t nr_elems_in_kblock = UINT64_C(1) << p_table->keys.log2_elem_cnt;
  const uint64_t base_kidx_cap = nr_elems_in_kblock * p_table->keys.arr_nr_active_blocks[bucket_idx];
  uint64_t base_kidx = 0;  // The base idx of current vacancy block
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict k_node = &p_table->keys.lists[bucket_idx].head;
  do {  // while ( base_kidx < base_kidx_cap )
    void * kblock = k_node->arr_blocks[block_idx];

    typeof(&key) const restrict p_key = __builtin_assume_aligned( kblock, ALIGN_FOR_AVX512 );
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(key);
    simde__m256i pack1, pack2, res1, res2;
    const simde__m256i key_pack = simde_mm256_set1_epi32( key );
    const uint64_t last_idx = (last_elem_idx < UINT64_C(1) << p_table->log2_nr_keys) ?
      last_elem_idx : (UINT64_C(1) << p_table->log2_nr_keys) - 1;
    for ( uint64_t idx = 0; idx <= last_idx; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        idx = idx + __tzcnt_u32(mask);
        goto found_the_key_to_remove;
      }
      if ( mask2 ) {
        idx = idx + ALIGN_FOR_AVX / sizeof(key) + __tzcnt_u32(mask2);
found_the_key_to_remove:
        typeof(&key) const p_last_key = infos_multi_al_get_elem_unsafe(
            &p_table->keys,
            bucket_idx,
            p_table->arr_idx_next_unoccupied_elem[bucket_idx] - 1 );
        p_key[idx] = *p_last_key;
        *p_last_key = 0;
        const void * restrict p_last_value = infos_multi_al_get_elem_unsafe(
            &p_table->values,
            bucket_idx,
            p_table->arr_idx_next_unoccupied_elem[bucket_idx] - 1 );
        void * restrict p_value = infos_multi_al_get_elem_unsafe(
            &p_table->values,
            bucket_idx,
            idx + base_kidx );
        memcpy( p_value, p_last_value, UINT64_C(1) << p_table->values.log2_elem_size );
        p_table->arr_idx_next_unoccupied_elem[bucket_idx]--;
        return 0;
      }
    }  // for ( uint64_t idx = 0; idx <= last_idx; idx += stride )

    block_idx++;
    base_kidx += nr_elems_in_kblock;
    last_elem_idx -= nr_elems_in_kblock;
    if ( block_idx >= _arr_elem_cnt(k_node->arr_blocks) ) {
      k_node = k_node->next;
      assume( k_node );
      //node_base_idx += UINT64_C(1) << ( p_table->keys.log2_elem_cnt +
      //    _arr_elem_cnt(p_table->keys.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_kidx < base_kidx_cap );

  return emsg_no_such_entry;

}


//-------------------------------------------------------------------------------------------------
void * infos_hashtable_search_3( const struct infos_hashtable * p_table,
    uint32_t key,
    uint32_t bucket_idx )
{
  uint64_t last_elem_idx = p_table->arr_idx_next_unoccupied_elem[bucket_idx] - 1;

  const uint64_t nr_elems_in_kblock = UINT64_C(1) << p_table->keys.log2_elem_cnt;
  const uint64_t base_kidx_cap = nr_elems_in_kblock * p_table->keys.arr_nr_active_blocks[bucket_idx];
  uint64_t base_kidx = 0;  // The base idx of current vacancy block
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict k_node = &p_table->keys.lists[bucket_idx].head;
  do {  // while ( base_kidx < base_kidx_cap )
    void * kblock = k_node->arr_blocks[block_idx];

    typeof(&key) const restrict p_key = __builtin_assume_aligned( kblock, ALIGN_FOR_AVX512 );
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(key);
    simde__m256i pack1, pack2, res1, res2;
    const simde__m256i key_pack = simde_mm256_set1_epi32( key );
    const uint64_t last_idx = (last_elem_idx < UINT64_C(1) << p_table->log2_nr_keys) ?
      last_elem_idx : (UINT64_C(1) << p_table->log2_nr_keys) - 1;
    for ( uint64_t idx = 0; idx <= last_idx; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        void * restrict p_value = infos_multi_al_get_elem_unsafe(
            &p_table->values,
            bucket_idx,
            base_kidx + idx );
        return p_value;
      }
      if ( mask2 ) {
        void * restrict p_value = infos_multi_al_get_elem_unsafe(
            &p_table->values,
            bucket_idx,
            base_kidx + idx + (ALIGN_FOR_AVX >> p_table->values.log2_elem_size) );
        return p_value;
      }
    }

    block_idx++;
    base_kidx += nr_elems_in_kblock;
    last_elem_idx -= nr_elems_in_kblock;
    if ( block_idx >= _arr_elem_cnt(k_node->arr_blocks) ) {
      k_node = k_node->next;
      assume( k_node );
      //node_base_idx += UINT64_C(1) << ( p_table->keys.log2_elem_cnt +
      //    _arr_elem_cnt(p_table->keys.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_kidx < base_kidx_cap );

  return nullptr;

}


