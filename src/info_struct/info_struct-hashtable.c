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
  new_table->log2_nr_keys = new_table->keys.log2_node_size - new_table->keys.log2_elem_size;

  // Prepare arrays of values
  ret = infos_multi_al_init_prealloc( &new_table->values,
      userdata,
      custom_malloc,
      new_table->keys.log2_nr_elems + ( 31 - _lzcnt_u32(item_size) ),
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
    struct infos_hashtable * restrict table,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  if ( ! table ) {
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
    ofs_itself = ofs_arr_sm + (UINT64_C(1) << table->keys.log2_nr_lists) *
      _sizeof_arr_elem(_typeof_member(struct infos_hashtable, arr_idx_next_unoccupied_elem));
  }

  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;

  infos_multi_al_destroy_except_preallocated_struct( &table->keys, userdata, local_free );
  infos_multi_al_destroy_except_preallocated_struct( &table->values, userdata, local_free );
  local_free( userdata, (void*)table - ofs_itself );
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert(
    struct infos_hashtable * restrict table,
    uint32_t key,
    uint32_t bucket_idx,
    const void * restrict data )
{
  struct infos_singly_linked_list_node * node = &table->keys.lists[bucket_idx].head;
  struct infos_singly_linked_list_node * value_arr = &table->values.lists[bucket_idx].head;
  //uint_fast8_t node_idx = 0;
  auto idx_first_empty_element = table->arr_idx_next_unoccupied_elem[bucket_idx];
  while ( idx_first_empty_element >= UINT64_C(1) << table->keys.log2_nr_elems ) {
    idx_first_empty_element -= UINT64_C(1) << table->keys.log2_nr_elems;
    if ( !node->next ) {
      // Make another node.
      uint32_t indices_buckets[] = { bucket_idx };
      if ( infos_multi_al_grow(&table->keys, 0, 0, 1, indices_buckets, -7) ) {
        return err_msg_out_of_memory;
      }
    }
    node = node->next;
  }

  typeof(&key) restrict p_key;
  void * restrict p_value;
  p_key = node->data + idx_first_empty_element * sizeof(key);
  p_value = value_arr->data + ( idx_first_empty_element << table->values.log2_elem_size );

  *p_key = key;
  memcpy(p_value, data, UINT64_C(1) << table->keys.log2_elem_size);
  table->arr_idx_next_unoccupied_elem[bucket_idx]++;

  return 0;
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert_u32_4(
    struct infos_hashtable * restrict table,
    uint32_t key,
    uint32_t bucket_idx,
    const uint32_t data )
{
  struct infos_singly_linked_list_node * node = &table->keys.lists[bucket_idx].head;
  struct infos_singly_linked_list_node * value_arr = &table->values.lists[bucket_idx].head;
  //uint_fast8_t node_idx = 0;
  auto idx_first_empty_element = table->arr_idx_next_unoccupied_elem[bucket_idx];
  while ( idx_first_empty_element >= UINT64_C(1) << table->keys.log2_nr_elems ) {
    idx_first_empty_element -= UINT64_C(1) << table->keys.log2_nr_elems;
    if ( !node->next ) {
      // Make another node.
      uint32_t indices_buckets[] = { bucket_idx };
      if ( infos_multi_al_grow(&table->keys, 0, 0, 1, indices_buckets, -7) ) {
        return err_msg_out_of_memory;
      }
    }
    node = node->next;
  }

  typeof(&key) restrict p_key;
  void * restrict p_value;
  p_key = node->data + idx_first_empty_element * sizeof(key);
  p_value = value_arr->data + idx_first_empty_element * sizeof(data);

  *p_key = key;
  deconst_typeof(&data) p_value2 = p_value;
  *p_value2 = data;
  table->arr_idx_next_unoccupied_elem[bucket_idx]++;

  return 0;
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert_u64_4(
    struct infos_hashtable * restrict table,
    uint32_t key,
    uint32_t bucket_idx,
    const uint64_t data )
{
  struct infos_singly_linked_list_node * node = &table->keys.lists[bucket_idx].head;
  struct infos_singly_linked_list_node * value_arr = &table->values.lists[bucket_idx].head;
  //uint_fast8_t node_idx = 0;
  auto idx_first_empty_element = table->arr_idx_next_unoccupied_elem[bucket_idx];
  while ( idx_first_empty_element >= UINT64_C(1) << table->keys.log2_nr_elems ) {
    idx_first_empty_element -= UINT64_C(1) << table->keys.log2_nr_elems;
    if ( !node->next ) {
      // Make another node.
      uint32_t indices_buckets[] = { bucket_idx };
      if ( infos_multi_al_grow(&table->keys, 0, 0, 1, indices_buckets, -7) ) {
        return err_msg_out_of_memory;
      }
    }
    node = node->next;
  }

  typeof(&key) restrict p_key;
  void * restrict p_value;
  p_key = node->data + idx_first_empty_element * sizeof(key);
  p_value = value_arr->data + idx_first_empty_element * sizeof(data);

  *p_key = key;
  deconst_typeof(&data) p_value2 = p_value;
  *p_value2 = data;
  table->arr_idx_next_unoccupied_elem[bucket_idx]++;

  return 0;
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_remove_3(struct infos_hashtable * table,
    uint32_t key, uint32_t bucket_idx )
{
  struct infos_singly_linked_list_node * node = &table->keys.lists[bucket_idx].head;
  struct infos_singly_linked_list_node * const value_arr = &table->values.lists[bucket_idx].head;
  //uint_fast8_t node_idx = 0;
  uint64_t idx_next_empty_elem = table->arr_idx_next_unoccupied_elem[bucket_idx];
  while (-7) {
    typeof(&key) const restrict p_key = __builtin_assume_aligned( node->data, ALIGN_FOR_AVX512 );
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(key);
    simde__m256i pack1, pack2, res1, res2;
    const simde__m256i key_pack = simde_mm256_set1_epi32( key );
    void * const restrict p_value = __builtin_assume_aligned( value_arr->data, ALIGN_FOR_AVX512 );
    const uint64_t last_idx = (idx_next_empty_elem < UINT64_C(1) << table->log2_nr_keys) ?
      idx_next_empty_elem : UINT64_C(1) << table->log2_nr_keys;
    for ( uint64_t idx = 0; idx < last_idx; idx += stride ) {
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
        idx = idx + __tzcnt_u32(mask2);
found_the_key_to_remove:
        // Move the last pair to where this pair is
        struct infos_singly_linked_list_node * cur_node = node;
        struct infos_singly_linked_list_node * cur_value_arr = value_arr;;
        uint64_t last_elem_idx = idx_next_empty_elem - 1;
        while (-7) {
          if ( last_elem_idx < UINT64_C(1) << table->log2_nr_keys ) {
            p_key[idx] = ((typeof(&key)) cur_node->data) [last_elem_idx];
            memcpy( p_value + (idx << table->values.log2_elem_size),
                cur_value_arr + (last_elem_idx << table->values.log2_elem_size),
                UINT64_C(1) << table->values.log2_elem_size );
            break;
          }
          cur_node = cur_node->next;
          cur_value_arr = cur_value_arr->next;
          last_elem_idx -= UINT64_C(1) << table->log2_nr_keys;
        }

        table->arr_idx_next_unoccupied_elem[bucket_idx]--;
        return 0;
        //return p_value + idx * table->buckets.elem_size;
      }
    }

    if ( !node->next ) {
      return err_msg_no_such_entry;
    }
    node = node->next;
    //node_idx++;
    idx_next_empty_elem -= UINT64_C(1) << table->log2_nr_keys;
  }

}


//-------------------------------------------------------------------------------------------------
void * infos_hashtable_search_3( const struct infos_hashtable * table,
    uint32_t key,
    uint32_t bucket_idx )
{
  const struct infos_singly_linked_list_node * node = &table->keys.lists[bucket_idx].head;
  const struct infos_singly_linked_list_node * value_arr = &table->values.lists[bucket_idx].head;
  // uint_fast8_t node_idx = 0;
  uint64_t idx_next_empty_elem = table->arr_idx_next_unoccupied_elem[bucket_idx];
  while (-7) {
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(key);
    simde__m256i pack1, pack2, res1, res2;
    const simde__m256i key_pack = simde_mm256_set1_epi32( key );
    const typeof(&key) restrict p_key =
      __builtin_assume_aligned( node->data, ALIGN_FOR_AVX512 );
    void * restrict const p_value = __builtin_assume_aligned( value_arr->data, ALIGN_FOR_AVX512 );
    const uint64_t last_idx = (idx_next_empty_elem < UINT64_C(1) << table->log2_nr_keys) ?
      idx_next_empty_elem : UINT64_C(1) << table->log2_nr_keys;
    for ( uint64_t idx = 0; idx < last_idx; idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      if ( mask ) {
        return p_value +
          (( idx + __tzcnt_u32(mask) ) << table->values.log2_elem_size);
      }
      if ( mask2 ) {
        return p_value + ALIGN_FOR_AVX +
          (( idx + __tzcnt_u32(mask2) ) << table->values.log2_elem_size);
      }
      //for ( uint32_t i = idx; i < idx + stride; i++ ) {
      //  if ( p_key[i] == key ) {
      //    return p_value + idx * table->buckets.elem_size;
      //  }
      //}
    }

    if ( !node->next ) {
      return nullptr;
    }
    node = node->next;
    // node_idx++;
    idx_next_empty_elem -= UINT64_C(1) << table->log2_nr_keys;
  }
}


