#include <immintrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>

#include "err_msg.h"
#include "info_struct.h"
#include "util-c_lang-basic.h"


#ifdef DEBUG_MODE
#include <stdio.h>
#endif



//-------------------------------------------------------------------------------------------------
/**
 * \brief It returns the index of the new allocation
 * \param length The length of new allocation in bytes
 * \return position
 * \warning If errno is non-zero when the function returns, it has failed to allocate.
 */
static uint32_t infos_smm_rent_impl( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length, uint8_t take_from_left );

//-------------------------------------------------------------------------------------------------
static enum err_msg infos_smm_return_impl( struct infos_simple_mem_mgr * restrict p_smm,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * userdata, uint32_t * nr_units_released, uint32_t target );

static enum err_msg remove_rent_and_update_vacancy(
  struct infos_simple_mem_mgr * restrict p_smm,
  void * restrict userdata,
  void * (*custom_malloc_non_null)(void * userdata, size_t alignment, size_t size),
  struct range_u32 * p_rent
  );

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_impl( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length, uint32_t alignment );

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_chunks_impl(
    struct infos_simple_mem_mgr * restrict p_smm,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict userdata, uint32_t nr_chunks, uint8_t log2_chunk_len, uint32_t alignment );



//-------------------------------------------------------------------------------------------------
struct infos_simple_mem_mgr * infos_smm_init(
    void * restrict buffer,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t total_len_buf_n_struct
    )
{
  assume( buffer );
  assume( _is_aligned_to_power_of_two((uintptr_t)buffer, ALIGN_FOR_AVX512) );
  // The size of the memory pool to be managed should be a multiple of
  //  alignof(infos_simple_mem_manager)
  assume( _is_aligned_to_power_of_two(
        total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr),
        alignof(struct infos_simple_mem_mgr)) );

  struct infos_simple_mem_mgr * new_manager;
  new_manager = buffer + total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
  new_manager->data_len = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
  new_manager->total_space_left = new_manager->data_len;
  new_manager->data = buffer;
  new_manager->nr_rents = 0;
  new_manager->nr_vacancy_infos = 1;
  new_manager->al_vacancies.active_block_cnt = 1;
  new_manager->al_rental_info.active_block_cnt = 1;

  constexpr size_t min_size_range_db = CACHELINE_LEN * 4;
  constexpr size_t rental_info_node_size = min_size_range_db * 2;
  constexpr size_t available_ranges_node_size = min_size_range_db;

  // Just in case we want to do AVX search on al_vacancies in the future
  static_assert( _is_aligned_to_power_of_two(rental_info_node_size, ALIGN_FOR_AVX512) );
  static_assert( _is_aligned_to_power_of_two(available_ranges_node_size, ALIGN_FOR_AVX512) );

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  infos_al_create_prealloc( &new_manager->al_rental_info,
      userdata, local_malloc, rental_info_node_size, sizeof(struct range_u32), false );
  const simde__m256i key_pack = simde_mm256_set1_epi64x( invalid_range32_union.u64 );
  constexpr uint64_t stride = ALIGN_FOR_AVX;
  void * restrict p_vectors = __builtin_assume_aligned(
      new_manager->al_rental_info.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t byte_idx = 0; byte_idx < rental_info_node_size; byte_idx += stride ) {
    simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
  }
  //struct range_u32 * const first_rental_info = infos_al_get_elem(
  //    &new_manager->al_rental_info, 0 );
  //*first_rental_info = empty_range;

  infos_al_create_prealloc( &new_manager->al_vacancies,
      userdata, local_malloc, available_ranges_node_size, sizeof(struct range_u32), false );
  p_vectors = __builtin_assume_aligned(
      new_manager->al_vacancies.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t byte_idx = 0; byte_idx < rental_info_node_size; byte_idx += stride ) {
    simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
  }

  struct range_u32 * const first_vacancy_info = infos_al_get_elem_unsafe(
      &new_manager->al_vacancies, 0 );
  first_vacancy_info->start = 0;
  first_vacancy_info->stop = new_manager->data_len - 1;

  return new_manager;
}

//-------------------------------------------------------------------------------------------------
struct infos_simple_mem_mgr * infos_smm_init_chunks( void * restrict buffer,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t total_len_buf_n_struct, uint8_t log2_chunk_len
    )
{
  assume( buffer );
  assume( _is_aligned_to_power_of_two((uintptr_t)buffer, ALIGN_FOR_AVX512) );
  // The size of the memory pool to be managed should be a multiple of
  //  alignof(infos_simple_mem_mgr)
  assume( _is_aligned_to_power_of_two(
        total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr),
        alignof(struct infos_simple_mem_mgr)) );

  if ( log2_chunk_len == 0 ) {
    constexpr size_t cacheline_len = CACHELINE_LEN;
    log2_chunk_len = get_log2(cacheline_len);
  }

  struct infos_simple_mem_mgr * new_manager;
  new_manager = buffer + total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
  new_manager->data_len = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
  new_manager->data_len >>= log2_chunk_len;
  new_manager->total_space_left = new_manager->data_len;
  new_manager->data = buffer;
  new_manager->nr_rents = 0;
  new_manager->nr_vacancy_infos = 1;
  new_manager->al_vacancies.active_block_cnt = 1;
  new_manager->al_rental_info.active_block_cnt = 1;

  constexpr size_t min_size_range_db = CACHELINE_LEN * 4;
  constexpr size_t rental_info_node_size = min_size_range_db * 2;
  constexpr size_t available_ranges_node_size = min_size_range_db;

  // Just in case we want to do AVX search on al_vacancies in the future
  static_assert( _is_aligned_to_power_of_two(rental_info_node_size, ALIGN_FOR_AVX512) );
  static_assert( _is_aligned_to_power_of_two(available_ranges_node_size, ALIGN_FOR_AVX512) );

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  infos_al_create_prealloc( &new_manager->al_rental_info,
      userdata, local_malloc, rental_info_node_size, sizeof(struct range_u32), false );
  const simde__m256i key_pack = simde_mm256_set1_epi64x( invalid_range32_union.u64 );
  constexpr uint64_t stride = ALIGN_FOR_AVX;
  void * restrict p_vectors = __builtin_assume_aligned(
      new_manager->al_rental_info.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t byte_idx = 0; byte_idx < rental_info_node_size; byte_idx += stride ) {
    simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
  }
  //struct range_u32 * const first_rental_info = infos_al_get_elem(&new_manager->al_rental_info, 0);
  //*first_rental_info = empty_range;

  infos_al_create_prealloc( &new_manager->al_vacancies,
      userdata, local_malloc, available_ranges_node_size, sizeof(struct range_u32), false );
  p_vectors = __builtin_assume_aligned(
      new_manager->al_vacancies.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t byte_idx = 0; byte_idx < rental_info_node_size; byte_idx += stride ) {
    simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
  }

  struct range_u32 * const first_vacancy_info = infos_al_get_elem_unsafe(
      &new_manager->al_vacancies, 0 );
  first_vacancy_info->start = 0;
  first_vacancy_info->stop = (new_manager->data_len - 1) >> log2_chunk_len;

  return new_manager;
}

//-------------------------------------------------------------------------------------------------
void infos_smm_init_chunks_prealloc(
    struct infos_simple_mem_mgr * restrict p_smm,
    uint32_t pool_size,
    uint8_t log2_chunk_len
    )
{
  assume( p_smm->data );
  assume( _is_aligned_to_power_of_two((uintptr_t)p_smm->data, ALIGN_FOR_AVX512) );

  if ( log2_chunk_len == 0 ) {
    constexpr size_t cacheline_len = CACHELINE_LEN;
    log2_chunk_len = get_log2(cacheline_len);
  }

  p_smm->data_len = pool_size >> log2_chunk_len;
  p_smm->total_space_left = p_smm->data_len;
  p_smm->nr_rents = 0;
  p_smm->nr_vacancy_infos = 1;
  p_smm->al_vacancies.active_block_cnt = 1;
  p_smm->al_rental_info.active_block_cnt = 1;

  constexpr size_t min_size_range_db = CACHELINE_LEN * 4;
  constexpr size_t rental_info_node_size = min_size_range_db * 2;
  constexpr size_t available_ranges_node_size = min_size_range_db;
  assume( pool_size > (rental_info_node_size + available_ranges_node_size) * 2 );

  // To just copy and paste I created one shared variable here.
  struct infos_arr_list * restrict al;

  al = &p_smm->al_rental_info;
  auto node_size = rental_info_node_size;
  typeof(al->list.head.arr_blocks[0]) p_data = p_smm->data;
  // Copy
  al->log2_block_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_elem_cnt = al->log2_block_size - al->log2_elem_size;
  al->list.head.arr_blocks[0] = p_data;
  al->list.head.next = nullptr;

  const simde__m256i key_pack = simde_mm256_set1_epi64x( invalid_range32_union.u64 );
  constexpr uint64_t stride = ALIGN_FOR_AVX;
  void * p_vectors = __builtin_assume_aligned(
      p_smm->al_rental_info.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t byte_idx = 0; byte_idx < rental_info_node_size; byte_idx += stride ) {
    simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
  }

  // Just in case we want to do AVX search on al_vacancies in the future
  static_assert( _is_aligned_to_power_of_two(rental_info_node_size, ALIGN_FOR_AVX512) );
  static_assert( _is_aligned_to_power_of_two(available_ranges_node_size, ALIGN_FOR_AVX512) );

  al = &p_smm->al_vacancies;
  node_size = available_ranges_node_size;
  p_data = p_smm->data + rental_info_node_size;
  // Paste
  al->log2_block_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_elem_cnt = al->log2_block_size - al->log2_elem_size;
  al->list.head.arr_blocks[0] = p_data;
  al->list.head.next = nullptr;
  p_vectors = __builtin_assume_aligned(
      p_smm->al_vacancies.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t byte_idx = 0; byte_idx < rental_info_node_size; byte_idx += stride ) {
    simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
  }

  // Take a few chunks from the first tip for the array lists
  struct range_u32 * const first_rental_info = infos_al_get_elem_unsafe(
      &p_smm->al_rental_info, 0 );
  first_rental_info->start = 0;
  first_rental_info->stop =
    ( (rental_info_node_size + available_ranges_node_size) >> log2_chunk_len ) - 1;
  p_smm->total_space_left -=
    (rental_info_node_size + available_ranges_node_size) >> log2_chunk_len;
  p_smm->nr_rents++;

  // Update availability information accordingly.
  // The entire pool except 'rental_info_node_size + available_ranges_node_size'
  struct range_u32 * const first_available_range = infos_al_get_elem_unsafe(
      &p_smm->al_vacancies, 0 );
  first_available_range->start =
    (rental_info_node_size + available_ranges_node_size) >> log2_chunk_len;
  first_available_range->stop = p_smm->data_len - 1;
}



//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_impl( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length, uint8_t take_from_left )
{
  if ( p_smm->total_space_left < length ) {
    errno = ENOBUFS;
    return 0;
  }

  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
  do {  // while ( base_vidx < base_vidx_cap )
    void * vblock = v_node->arr_blocks[block_idx];

    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }
      struct range_u32 * restrict p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);
      if ( get_range_len(*p_vacancy) >= length ) {
        // Register the rental_info
        void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
        local_malloc = custom_malloc ? custom_malloc : default_malloc;
        struct range_u32 * restrict p_rent_slot = infos_al_get_elem(
            &p_smm->al_rental_info, userdata, local_malloc,
            true, invalid_range32_union.u64, p_smm->nr_rents );
        if ( !p_rent_slot ) {
          errno = ENOMEM;
          return 0;
        }
        // Take the empty element; and return;
        if ( take_from_left ) {
          p_rent_slot->stop = p_vacancy->start + length - 1;
          p_rent_slot->start = p_vacancy->start;
        } else {
          p_rent_slot->start = p_vacancy->stop - length + 1;
          p_rent_slot->stop = p_vacancy->stop;
        }

        // Reduce the size of vacancy
        // From the right side? or the left side?
        if ( take_from_left )
          p_vacancy->start = p_vacancy->start + length;
        else
          p_vacancy->stop = p_vacancy->stop - length;

        if ( get_range_len(*p_vacancy) == 0 ) {
          typeof(p_vacancy) const p_last_vacancy = infos_al_get_elem_unsafe(
              &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
          *p_vacancy = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_smm->nr_vacancy_infos--;
        }

        p_smm->nr_rents++;
        p_smm->total_space_left -= get_range_len(*p_rent_slot);
        return p_rent_slot->start;
      }  // if ( get_range_len(*p_vacancy) >= length )

    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    block_idx++;
    base_vidx += nr_elems_in_vblock;
    if ( block_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  return 0;
}



//-------------------------------------------------------------------------------------------------
void * infos_smm_rent( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    bool take_from_left )
{
  errno = 0;
  uint32_t ret = infos_smm_rent_impl( p_smm, userdata, custom_malloc, length, take_from_left );
  if ( unlikely(errno) ) {
    return nullptr;
  }

  return p_smm->data + ret;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_rent_chunks( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_chunks,
    bool take_from_left,
    uint_fast8_t log2_chunk_len)
{
  errno = 0;
  uint32_t ret = infos_smm_rent_impl( p_smm, userdata, custom_malloc, nr_chunks, take_from_left );
  if ( unlikely(errno) ) {
    return nullptr;
  }

  return p_smm->data + (ret << log2_chunk_len);
}


constexpr size_t nr_tries_to_find_a_prealigned_space = 3;

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_impl( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length, uint32_t alignment )
{
  assume( alignment > 1 );
  assume( _is_power_of_two(alignment) );

  if ( p_smm->total_space_left < length ) {
    errno = ENOBUFS;
    return 0;
  }

  assume( _is_aligned_to_power_of_two((uintptr_t)p_smm->data, ALIGN_FOR_AVX512) );  // Hey, compiler
                                                        // Please do consider that we might never
                                                        //  have `alignment` > ALIGN_FOR_AVX512
                                                        //  and that the user will always have it
                                                        //  aligned to at least AVX512 vector size
                                                        //  when you do link-time optimization
  const uint32_t lsbs_head_addr = (uintptr_t)p_smm->data & (alignment - 1);
  const uint32_t nr_misaligned_bytes_at_head = (alignment - lsbs_head_addr) & (alignment - 1);
  const uint32_t lsbs_tail_addr_when_aligned =
    (nr_misaligned_bytes_at_head + length - 1) & (alignment - 1);
  uint_fast8_t nr_tries_to_find_prealigned = 0;
  bool take_from_right = true;  // Let us take the right-most part in most of the cases.
  struct range_u32 * spacious_range = nullptr;
  struct range_u32 * restrict p_vacancy;

  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
  do {  // while ( base_vidx < base_vidx_cap )
    void * vblock = v_node->arr_blocks[block_idx];

    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }

      p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);
      struct range_u32 new_vacancy = invalid_range;

      if ( get_aligned_range_len(*p_vacancy, alignment, lsbs_head_addr) >= length ) {
        nr_tries_to_find_prealigned++;
        if ( _is_aligned_to_power_of_two(p_vacancy->start - lsbs_head_addr, alignment) ) {
          // It's pre-aligned in the head
          take_from_right = false;  // The only case where we take the front part.
          goto register_new_rental_info;

        } else if ( ((alignment - 1) & p_vacancy->stop) == lsbs_tail_addr_when_aligned ) {
          // It's pre-aligned in the tail
          goto register_new_rental_info;

        } else {
          // Try other vacancies if it's not prealigned
          if ( nr_tries_to_find_prealigned < nr_tries_to_find_a_prealigned_space ) {
            spacious_range = p_vacancy;
            continue;
          }
        }

come_back_here_with_a_spacious_range:
        // Split vacancy in two pieces
        // NOTE It simply splits the range into 2 halves but there may be a better way.
        uint32_t nr_units_left_after_alloc = get_range_len(*p_vacancy) - length;
        uint32_t first_unit_idx = nr_units_left_after_alloc/2 - length/2;
        first_unit_idx =
          _align_to_power_of_two(p_vacancy->start + first_unit_idx + lsbs_head_addr, alignment)
          - lsbs_head_addr;

        while ( first_unit_idx + length - 1 > p_vacancy->stop ) {
          first_unit_idx -= alignment;
        }

        // Prepare to register a new vacancy
        new_vacancy.start = first_unit_idx + length;
        new_vacancy.stop = p_vacancy->stop;

        p_vacancy->stop = first_unit_idx - 1;


register_new_rental_info:
        // Register the rental_info
        void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
        local_malloc = custom_malloc ? custom_malloc : default_malloc;
        struct range_u32 * restrict p_rent_slot = infos_al_get_elem(
            &p_smm->al_rental_info, userdata, local_malloc,
            true, invalid_range32_union.u64, p_smm->nr_rents );
        if ( !p_rent_slot ) {
          errno = ENOMEM;
          return 0;
        }
        if ( !take_from_right ) {
          p_rent_slot->stop = p_vacancy->start + length - 1;
          p_rent_slot->start = p_vacancy->start;
        } else {
          p_rent_slot->start = p_vacancy->stop - length + 1;
          p_rent_slot->stop = p_vacancy->stop;
        }

        // Reduce the size of vacancy
        if ( !take_from_right )
          p_vacancy->start = p_vacancy->start + length;
        else
          p_vacancy->stop = p_vacancy->stop - length;

        // See if there is a new vacancy to register
        if ( is_range_u32_valid(new_vacancy) ) {
          typeof(p_vacancy) const p_new_slot = infos_al_get_elem(
            &p_smm->al_vacancies, userdata, local_malloc,
            true, invalid_range32_union.u64, p_smm->nr_vacancy_infos );
          if ( !p_new_slot ) {
            // Revert changes
            *p_rent_slot = invalid_range;
            if ( !take_from_right )
              p_vacancy->start = p_vacancy->start - length;
            else
              p_vacancy->stop = p_vacancy->stop + length;

            errno = ENOMEM;
            return 0;
          }
          p_new_slot->start = new_vacancy.start;
          p_new_slot->stop = new_vacancy.stop;
          p_smm->nr_vacancy_infos++;
        }

        if ( get_range_len(*p_vacancy) == 0 ) {
          typeof(p_vacancy) const p_last_vacancy = infos_al_get_elem_unsafe(
              &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
          *p_vacancy = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_smm->nr_vacancy_infos--;
        }

        // TODO The compiler should optimize this line and just use `length`
        p_smm->total_space_left -= get_range_len(*p_rent_slot);
        p_smm->nr_rents++;
        return p_rent_slot->start;

      }  // if ( get_aligned_range_len(*p_vacancy, alignment, lsbs_head_addr) >= length )

    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    block_idx++;
    base_vidx += nr_elems_in_vblock;
    if ( block_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );

  if ( spacious_range ) {
    p_vacancy = spacious_range;
    goto come_back_here_with_a_spacious_range;
  }

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  return 0;
}

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_chunks_impl(
    struct infos_simple_mem_mgr * restrict p_smm,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict userdata, uint32_t nr_chunks, uint8_t log2_chunk_len, uint32_t alignment )
{
  assume( alignment > 1 );
  assume( _is_power_of_two(alignment) );

  if ( p_smm->total_space_left < nr_chunks ) {
    errno = ENOBUFS;
    return 0;
  }

  assume( _is_aligned_to_power_of_two((uintptr_t)p_smm->data, ALIGN_FOR_AVX512) );  // Hey, compiler
                                                        // Please do consider that we might never
                                                        //  have `alignment` > ALIGN_FOR_AVX512
                                                        //  and that the user will always have it
                                                        //  aligned to at least AVX512 vector size
                                                        //  when you do link-time optimization
  const uint64_t lsbs_head_addr = (uintptr_t)p_smm->data & (alignment - 1);
  const uint64_t alignment_in_chunks = alignment >> log2_chunk_len;
  const uint64_t lsbs_headchunk_addr = lsbs_head_addr >> log2_chunk_len & (alignment_in_chunks - 1);
  const uint32_t nr_misaligned_chunks_at_head =
    ( (alignment_in_chunks - lsbs_headchunk_addr) & (alignment_in_chunks - 1) );
  const uint32_t lsbs_last_chunk_addr_when_aligned =
    ( nr_misaligned_chunks_at_head + (nr_chunks - 1) ) & ( alignment_in_chunks - 1 );

  uint_fast8_t nr_tries_to_find_prealigned = 0;
  bool take_from_right = true;  // Let us take the right-most part in most of the cases.
  struct range_u32 * spacious_range = nullptr;
  struct range_u32 * restrict p_vacancy;
  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
  do {  // while ( base_vidx < base_vidx_cap )
    void * vblock = v_node->arr_blocks[block_idx];

    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }

      p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);
      struct range_u32 new_vacancy = invalid_range;

      if ( get_aligned_range_len(*p_vacancy, alignment_in_chunks, lsbs_head_addr) >= nr_chunks ) {
        nr_tries_to_find_prealigned++;
        if ( _is_aligned_to_power_of_two(
              p_vacancy->start - lsbs_headchunk_addr, alignment_in_chunks) ) {
          // It's pre-aligned in the head
          take_from_right = false;  // The only case where we take the front part.
          goto register_new_rental_info;

        } else if ( ((alignment_in_chunks - 1) & p_vacancy->stop) ==
            lsbs_last_chunk_addr_when_aligned ) {
          // It's pre-aligned in the tail
          goto register_new_rental_info;

        } else {
          // Try other vacancies if it's not prealigned
          if ( nr_tries_to_find_prealigned < nr_tries_to_find_a_prealigned_space ) {
            spacious_range = p_vacancy;
            continue;
          }
        }

come_back_here_with_a_spacious_range:
        // Split vacancy in two pieces
        // NOTE It simply splits the range into 2 halves but there may be a better way.
        uint32_t nr_units_left_after_alloc = get_range_len(*p_vacancy) - nr_chunks;
        uint32_t first_unit_idx = nr_units_left_after_alloc/2 - nr_chunks/2;
        first_unit_idx =
          _align_to_power_of_two( p_vacancy->start + first_unit_idx + lsbs_headchunk_addr,
              alignment_in_chunks )
          - lsbs_headchunk_addr;

        while ( first_unit_idx + nr_chunks - 1 > p_vacancy->stop ) {
          first_unit_idx -= alignment_in_chunks;
        }

        // Prepare to register a new vacancy
        new_vacancy.start = first_unit_idx + nr_chunks;
        new_vacancy.stop = p_vacancy->stop;

        p_vacancy->stop = first_unit_idx - 1;


register_new_rental_info:
        // Register the rental_info
        void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
        local_malloc = custom_malloc ? custom_malloc : default_malloc;
        struct range_u32 * restrict p_rent_slot = infos_al_get_elem(
            &p_smm->al_rental_info, userdata, local_malloc,
            true, invalid_range32_union.u64, p_smm->nr_rents );
        if ( !p_rent_slot ) {
          errno = ENOMEM;
          return 0;
        }
        if ( !take_from_right ) {
          p_rent_slot->stop = p_vacancy->start + nr_chunks - 1;
          p_rent_slot->start = p_vacancy->start;
        } else {
          p_rent_slot->start = p_vacancy->stop - nr_chunks + 1;
          p_rent_slot->stop = p_vacancy->stop;
        }

        // Reduce the size of vacancy
        if ( !take_from_right )
          p_vacancy->start = p_vacancy->start + nr_chunks;
        else
          p_vacancy->stop = p_vacancy->stop - nr_chunks;

        // See if there is a new vacancy to register
        if ( is_range_u32_valid(new_vacancy) ) {
          typeof(p_vacancy) const p_new_slot = infos_al_get_elem(
            &p_smm->al_vacancies, userdata, local_malloc,
            true, invalid_range32_union.u64, p_smm->nr_vacancy_infos );
          if ( !p_new_slot ) {
            // Revert changes
            *p_rent_slot = invalid_range;
            if ( !take_from_right )
              p_vacancy->start = p_vacancy->start - nr_chunks;
            else
              p_vacancy->stop = p_vacancy->stop + nr_chunks;

            errno = ENOMEM;
            return 0;
          }
          p_new_slot->start = new_vacancy.start;
          p_new_slot->stop = new_vacancy.stop;
          p_smm->nr_vacancy_infos++;
        }

        if ( get_range_len(*p_vacancy) == 0 ) {
          typeof(p_vacancy) const p_last_vacancy = infos_al_get_elem_unsafe(
              &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
          *p_vacancy = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_smm->nr_vacancy_infos--;
        }

        // TODO The compiler should optimize this line and just use `length`
        p_smm->total_space_left -= get_range_len(*p_rent_slot);
        p_smm->nr_rents++;
        return p_rent_slot->start;

      }  // if ( get_aligned_range_len(*p_vacancy, alignment_in_chunks, lsbs_head_addr) >= length )
    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    block_idx++;
    base_vidx += nr_elems_in_vblock;
    if ( block_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );

  if ( spacious_range ) {
    p_vacancy = spacious_range;
    goto come_back_here_with_a_spacious_range;
  }

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  return 0;

}

//-------------------------------------------------------------------------------------------------
void * infos_smm_rent_aligned( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    uint32_t alignment )
{
  errno = 0;
  uint32_t ret = infos_smm_rent_aligned_impl(p_smm, userdata, custom_malloc, length, alignment);
  if ( errno ) {
    return nullptr;
  }

  return p_smm->data + ret;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_rent_aligned_chunks( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_chunks,
    uint32_t alignment,
    uint_fast8_t log2_chunk_len )
{
  errno = 0;
  uint32_t ret = infos_smm_rent_aligned_chunks_impl( p_smm, custom_malloc, userdata,
      nr_chunks, log2_chunk_len, alignment );
  if ( errno ) {
    return nullptr;
  }

  return p_smm->data + (ret << log2_chunk_len);
}



//-------------------------------------------------------------------------------------------------
static enum err_msg infos_smm_return_impl( struct infos_simple_mem_mgr * restrict p_smm,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * userdata, uint32_t * nr_units_released, uint32_t target )
{
  uint64_t last_elem_idx = p_smm->nr_rents - 1;

  struct range_u32 rent = { .start = target, .stop = 0 };
  //constexpr unsigned int stop_mask = 0xAAAAAAAAu;  // 0b101010..10
  constexpr unsigned int stop_mask = 0x55555555u;  // 0b010101..01

  const uint64_t nr_elems_in_rblock = UINT64_C(1) << p_smm->al_rental_info.log2_elem_cnt;
  uint64_t base_ridx = 0;  // The base idx of current vacancy block
  const uint64_t base_ridx_cap = nr_elems_in_rblock * p_smm->al_rental_info.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict r_node = &p_smm->al_rental_info.list.head;
  do {  // while ( base_ridx < base_ridx_cap )
    void * rblock = r_node->arr_blocks[block_idx];

    typeof(&rent) const restrict p_key = __builtin_assume_aligned( rblock, ALIGN_FOR_AVX512 );
    simde__m256i pack1, pack2, res1, res2;
    const union range_union { struct range_u32 range; uint64_t u64; } key_union = {
      .range = rent };
    const simde__m256i key_pack = simde_mm256_set1_epi64x( key_union.u64 );
    const uint64_t last_elem_idx_in_this_block = ( last_elem_idx < nr_elems_in_rblock ) ?
      last_elem_idx : nr_elems_in_rblock - 1;
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(rent);
    for ( uint64_t elem_idx = 0; elem_idx <= last_elem_idx_in_this_block; elem_idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + elem_idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + elem_idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      mask &= stop_mask;
      mask2 &= stop_mask;
      constexpr unsigned int whole_over_half = sizeof(rent) / sizeof(rent.start);
      if ( mask ) {
        elem_idx = elem_idx + __tzcnt_u32(mask) / whole_over_half;
        goto found_the_key_to_remove;
      }
      if ( mask2 ) {
        elem_idx = elem_idx + ALIGN_FOR_AVX / sizeof(rent) + __tzcnt_u32(mask2) / whole_over_half;
found_the_key_to_remove:
        void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
        local_malloc = custom_malloc ? custom_malloc : default_malloc;
        typeof(p_key[0].start) length = get_range_len(p_key[elem_idx]);
        enum err_msg err = remove_rent_and_update_vacancy( p_smm, userdata, local_malloc,
            p_key+elem_idx );
        if ( err ) {
          if ( nr_units_released ) {
            *nr_units_released = 0;
          }
          return err;
        }

        if ( nr_units_released ) {
          *nr_units_released = length;
        }
        //p_smm->nr_rents--;  done in remove_rent_and_update_vacancy
        return emsg_no_error;
      }  // if ( mask2 )

    }  // for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride )

    block_idx++;
    base_ridx += nr_elems_in_rblock;
    last_elem_idx -= nr_elems_in_rblock;
    if ( block_idx >= _arr_elem_cnt(r_node->arr_blocks) ) {
      r_node = r_node->next;
      assume( r_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_rental_info.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_rental_info.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_ridx < base_ridx_cap );

  if ( nr_units_released ) {
    *nr_units_released = 0;
  }
  return emsg_no_such_entry;
}

//-------------------------------------------------------------------------------------------------
static enum err_msg remove_rent_and_update_vacancy(
  struct infos_simple_mem_mgr * restrict p_smm,
  void * restrict userdata,
  void * (*custom_malloc_non_null)(void * userdata, size_t alignment, size_t size),
  struct range_u32 * p_rent
  )
{
  // Make a new vacancy information or merge with an existing one if it's a neighbor
  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
  struct range_u32 * restrict p_vacancy_to_merge_with = nullptr;
  bool has_been_merged = false;
  do {  // while ( base_vidx < base_vidx_cap )
    void * vblock = v_node->arr_blocks[block_idx];
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
        // Couldn't find a vacancy.
        goto out_of_nested_loop;
      }

      struct range_u32 * restrict p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);

      // Some messy code from a long time ago
      if ( !p_vacancy_to_merge_with ) {
        if ( p_vacancy->stop == p_rent->start - 1 ) {
          merge_range_into_lower(p_vacancy, p_rent);
          p_vacancy_to_merge_with = p_vacancy;
          has_been_merged = true;
        } else if ( p_vacancy->start == p_rent->stop + 1 ) {
          merge_range_into_higher(p_rent, p_vacancy);
          p_vacancy_to_merge_with = p_vacancy;
          has_been_merged = true;
        }
      } else {
        if ( p_vacancy->stop == p_vacancy_to_merge_with->start - 1 ) {
          merge_range_into_lower(p_vacancy, p_vacancy_to_merge_with);
          goto swap_positions_with_the_last_one;
        } else if ( p_vacancy->start == p_vacancy_to_merge_with->stop + 1 ) {
          merge_range_into_higher(p_vacancy_to_merge_with, p_vacancy);
swap_positions_with_the_last_one:
          typeof(p_vacancy) const p_last_vacancy = infos_al_get_elem_unsafe(
              &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
          *p_vacancy_to_merge_with = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_smm->nr_vacancy_infos--;
          p_vacancy_to_merge_with = nullptr;
        }

        if ( !p_vacancy_to_merge_with ) {
          p_smm->total_space_left += get_range_len(*p_rent);

          struct range_u32 * restrict p_rightmost_rent = infos_al_get_elem_unsafe(
              &p_smm->al_rental_info, p_smm->nr_rents - 1 );
          *p_rent = *p_rightmost_rent;
          *p_rightmost_rent = invalid_range;
          p_smm->nr_rents--;
          return emsg_no_error;
        }
      }  // if ( !p_vacancy_to_merge_with ) {} else

    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    block_idx++;
    base_vidx += UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
    if ( block_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );
out_of_nested_loop:
  if ( has_been_merged ) {
    p_smm->total_space_left += get_range_len(*p_rent);

    struct range_u32 * restrict p_rightmost_rent = infos_al_get_elem_unsafe(
        &p_smm->al_rental_info, p_smm->nr_rents - 1 );
    *p_rent = *p_rightmost_rent;
    *p_rightmost_rent = invalid_range;
    p_smm->nr_rents--;
    return emsg_no_error;
  }

  // Couldn't merge into any existing vacancy

  // Register a new one
  typeof(p_vacancy_to_merge_with) const p_new_slot = infos_al_get_elem(
    &p_smm->al_vacancies, userdata, custom_malloc_non_null,
    true, invalid_range32_union.u64, p_smm->nr_vacancy_infos );
  if ( !p_new_slot ) {
    return emsg_out_of_memory;
  }
  p_new_slot->start = p_rent->start;
  p_new_slot->stop = p_rent->stop;
  p_smm->nr_vacancy_infos++;
  p_smm->total_space_left += get_range_len(*p_rent);

  struct range_u32 * restrict p_rightmost_rent = infos_al_get_elem_unsafe(
      &p_smm->al_rental_info, p_smm->nr_rents - 1 );
  *p_rent = *p_rightmost_rent;
  *p_rightmost_rent = invalid_range;
  p_smm->nr_rents--;
  return emsg_no_error;
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_return( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr )
{
  uint32_t target = ptr - p_smm->data;
  return infos_smm_return_impl(p_smm, custom_malloc, userdata, 0, target);
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_return_chunks( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr,
    uint32_t * restrict nr_chunks_released,
    uint32_t log2_chunk_len )
{
  uint32_t target = (ptr - p_smm->data) >> log2_chunk_len;
  return infos_smm_return_impl(p_smm, custom_malloc, userdata, nr_chunks_released, target);
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_return_chunks_ofs( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t offset,
    uint32_t * restrict nr_chunks_released,
    uint32_t log2_chunk_len )
{
  uint32_t target = offset >> log2_chunk_len;
  return infos_smm_return_impl(p_smm, custom_malloc, userdata, nr_chunks_released, target);
}



//-------------------------------------------------------------------------------------------------
void * infos_smm_realloc_chunks( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length,
    uint32_t log2_chunk_len )
{
  if ( new_length == 0 ) {
    return 0;
  }

  uint64_t last_elem_idx = p_smm->nr_rents - 1;

  const uint64_t target = (orig_ptr - p_smm->data) >> log2_chunk_len;
  struct range_u32 rent = { .start = target, .stop = 0 };
  //constexpr unsigned int stop_mask = 0xAAAAAAAAu;  // 0b101010..10
  constexpr unsigned int stop_mask = 0x55555555u;  // 0b010101..01
  const uint32_t new_len_in_chunks = ( new_length + (UINT64_C(1) << log2_chunk_len) - 1 )
    >> log2_chunk_len;
  struct range_u32 * p_vacancy_front = nullptr;
  struct range_u32 * p_vacancy_back = nullptr;
  uint_fast8_t nr_neighbors_found = 0;

  const uint64_t nr_elems_in_rblock = UINT64_C(1) << p_smm->al_rental_info.log2_elem_cnt;
  uint64_t base_ridx = 0;  // The base idx of current vacancy block
  const uint64_t base_ridx_cap = nr_elems_in_rblock * p_smm->al_rental_info.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict r_node = &p_smm->al_rental_info.list.head;
  do {  // while ( base_ridx < base_ridx_cap )
    void * rblock = r_node->arr_blocks[block_idx];

    typeof(&rent) const restrict p_key = __builtin_assume_aligned( rblock, ALIGN_FOR_AVX512 );
    simde__m256i pack1, pack2, res1, res2;
    const union range_union { struct range_u32 range; uint64_t u64; } key_union = {
      .range = rent };
    const simde__m256i key_pack = simde_mm256_set1_epi64x( key_union.u64 );
    const uint64_t last_elem_idx_in_this_block = ( last_elem_idx < nr_elems_in_rblock ) ?
      last_elem_idx : nr_elems_in_rblock - 1;
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(rent);
    for ( uint64_t elem_idx = 0; elem_idx <= last_elem_idx_in_this_block; elem_idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + elem_idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + elem_idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      mask &= stop_mask;
      mask2 &= stop_mask;
      constexpr unsigned int whole_over_half = sizeof(rent) / sizeof(rent.start);
      if ( mask ) {
        elem_idx = elem_idx + __tzcnt_u32(mask) / whole_over_half;
        goto found_the_key_to_remove;
      }
      if ( mask2 ) {
        elem_idx = elem_idx + ALIGN_FOR_AVX / sizeof(rent) + __tzcnt_u32(mask2) / whole_over_half;
found_the_key_to_remove:
        int32_t len_diff_in_chunks = new_len_in_chunks - get_range_len(p_key[elem_idx]);
        if ( len_diff_in_chunks == 0 ) {
          p_smm->total_space_left -= 0;
          return orig_ptr;
        }

        const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
        uint64_t base_vidx = 0;  // The base idx of current vacancy block
        const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
        //uint64_t node_base_idx = 0;
        uint64_t vblock_idx = 0;
        struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
        do {  // while ( base_vidx < base_vidx_cap )
          void * vblock = v_node->arr_blocks[vblock_idx];

          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
              goto end_of_neighbor_search;
            }
            struct range_u32 * restrict p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);

            if ( p_vacancy->stop == target - 1 ) {
              //available_space_front = rental_info->start - p_vacancy->start;
              p_vacancy_front = p_vacancy;
              nr_neighbors_found++;
            } else if ( p_vacancy->start == p_key[elem_idx].stop + 1 ) {
              //available_space_back = p_vacancy->start - rental_info->start;
              p_vacancy_back = p_vacancy;
              nr_neighbors_found++;
            }

            if ( nr_neighbors_found >= 2 ) {
              goto end_of_neighbor_search;
            }

          }  // for ( uint_fast32_t vacancy_idx = 0;
             //     vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

          vblock_idx++;
          base_vidx += nr_elems_in_vblock;
          if ( vblock_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
            v_node = v_node->next;
            assume( v_node );
            //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
            //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
            vblock_idx = 0;
          }
        } while ( base_vidx < base_vidx_cap );
end_of_neighbor_search:

        if ( p_vacancy_back ) {
          if ( len_diff_in_chunks > 0 ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if ( get_range_len( *p_vacancy_back ) < len_diff_in_chunks ) {
#pragma GCC diagnostic pop
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the back
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            bool whole_merge = get_range_len(*p_vacancy_back) == len_diff_in_chunks ?
#pragma GCC diagnostic pop
              true : false;
            p_key[elem_idx].stop += len_diff_in_chunks;
            if ( !whole_merge ) {
              p_vacancy_back->start += len_diff_in_chunks;
            } else {  // whole_merge
              typeof(p_vacancy_back) const p_last_vacancy = infos_al_get_elem_unsafe(
                  &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
              *p_vacancy_back = *p_last_vacancy;
              *p_last_vacancy = invalid_range;
              p_smm->nr_vacancy_infos--;
              //p_vacancy_back = nullptr;
            }
            goto return_new_alloc;
          } else {  // len_diff_in_chunks < 0
            // Transfer space to the vacancy in the back
            p_key[elem_idx].stop += len_diff_in_chunks;
            p_vacancy_back->start += len_diff_in_chunks;
            goto return_new_alloc;
          }
        }  // if ( p_vacancy_back )

        if ( p_vacancy_front ) {
          if ( len_diff_in_chunks > 0 ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if ( get_range_len( *p_vacancy_front ) < len_diff_in_chunks ) {
#pragma GCC diagnostic pop
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the front
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            bool whole_merge = get_range_len(*p_vacancy_front) == len_diff_in_chunks ?
#pragma GCC diagnostic pop
              true : false;
            p_key[elem_idx].start -= len_diff_in_chunks;
            if ( !whole_merge ) {
              p_vacancy_front->stop -= len_diff_in_chunks;
            } else {  // whole_merge
              typeof(p_vacancy_front) const p_last_vacancy = infos_al_get_elem_unsafe(
                  &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
              *p_vacancy_front = *p_last_vacancy;
              *p_last_vacancy = invalid_range;
              p_smm->nr_vacancy_infos--;
              //p_vacancy_front = nullptr;
            }
            goto return_new_alloc;
          } else {
            // Transfer space to the vacancy in the front
            p_key[elem_idx].stop += len_diff_in_chunks;
            p_vacancy_front->start += len_diff_in_chunks;
            goto return_new_alloc;
          }  // if ( len_diff_in_chunks > 0 ) {} else
        }  // if ( p_vacancy_front )

        // Try to edit both vacancy_front and vacancy_back
        if ( p_vacancy_back && p_vacancy_front ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
          if ( len_diff_in_chunks > 0 &&
              (get_range_len(*p_vacancy_front) + get_range_len(*p_vacancy_back)
              < len_diff_in_chunks) ) {
#pragma GCC diagnostic pop
            goto no_surrounding_space_big_enough_to_take;
          }
          // Take space from the front first
          uint32_t added_len = get_range_len(*p_vacancy_front);
          p_key[elem_idx].start -= added_len;
          typeof(p_vacancy_front) p_last_vacancy = infos_al_get_elem_unsafe(
              &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
          *p_vacancy_front = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_smm->nr_vacancy_infos--;

          // Take space from the back
          uint32_t deficit = len_diff_in_chunks - added_len;
          p_key[elem_idx].stop += deficit;
          p_vacancy_back->start += deficit;
          if ( get_range_len(*p_vacancy_back) == 0 ) {
            p_last_vacancy = infos_al_get_elem_unsafe(
                &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
            *p_vacancy_back = *p_last_vacancy;
            *p_last_vacancy = invalid_range;
            p_smm->nr_vacancy_infos--;
          }
return_new_alloc:
          p_smm->total_space_left -= len_diff_in_chunks;
          return p_smm->data + (p_key[elem_idx].start << log2_chunk_len);
        }  // if ( p_vacancy_back && p_vacancy_front )

no_surrounding_space_big_enough_to_take:
        void * new_ptr = infos_smm_rent_chunks( p_smm, userdata, custom_malloc,
            new_len_in_chunks, true, log2_chunk_len );
        if ( !new_ptr ) {
          errno = ENOBUFS;
          return 0;
        }

        memcpy( new_ptr, orig_ptr, get_range_len(p_key[elem_idx]) > new_len_in_chunks ?
            new_len_in_chunks << log2_chunk_len :
            get_range_len(p_key[elem_idx]) << log2_chunk_len );

        struct range_u32 * restrict p_last_rent = infos_al_get_elem_unsafe(
            &p_smm->al_rental_info, p_smm->nr_rents - 1 );
        p_key[elem_idx] = *p_last_rent;
        *p_last_rent = invalid_range;
        p_smm->nr_rents--;
        //enum err_msg err = infos_smm_return_chunks( p_smm,
        //    userdata, custom_malloc, orig_ptr, nullptr, log2_chunk_len );
        //assume( err == emsg_no_error );
        //if ( err ) {
        //  fprintf( stderr, "[SMM] infos_smm_realloc_chunks failed. (%s)\n", arr_emsg_txt[err] );
        //}
        //assume( err == emsg_no_error );
        p_smm->total_space_left -= len_diff_in_chunks;
        return new_ptr;

      }  // if ( mask2 )

    }  // for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride )

    block_idx++;
    base_ridx += nr_elems_in_rblock;
    last_elem_idx -= nr_elems_in_rblock;
    if ( block_idx >= _arr_elem_cnt(r_node->arr_blocks) ) {
      r_node = r_node->next;
      assume( r_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_rental_info.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_rental_info.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_ridx < base_ridx_cap );

  // Couldn't find the rental information about ptr.
  errno = EBADSLT;
  return 0;
}



static void * try_to_merge_with_vacancy_right_next_for_an_aligned_alloc(
    struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc_non_null)(void * userdata, size_t alignment, size_t size),
    struct range_u32 ** pp_vacancy_back,
    struct range_u32 ** pp_vacancy_front,
    struct range_u32 * orig_rent,
    uint32_t alignment_in_chunks,
    uint32_t lsbs_headchunk_addr,
    bool has_tried_back_merge,
    bool has_tried_front_merge,
    int32_t len_diff_in_chunks,
    uint32_t log2_chunk_len
    )
{
  uint32_t new_len_in_chunks = get_range_len( *orig_rent ) + len_diff_in_chunks;
  uint32_t nr_misaligned_chunks_at_head =
    ( (alignment_in_chunks - lsbs_headchunk_addr) & (alignment_in_chunks - 1) );
  uint32_t lsbs_last_chunk_addr_when_aligned =
    ( nr_misaligned_chunks_at_head + (new_len_in_chunks - 1) ) & ( alignment_in_chunks - 1 );
  bool prealigned_head = _is_aligned_to_power_of_two(
      orig_rent->start - lsbs_headchunk_addr, alignment_in_chunks );
  bool prealigned_tail = ((alignment_in_chunks - 1) & orig_rent->stop) ==
    lsbs_last_chunk_addr_when_aligned;
  // Take allocation from the right-most part
  bool take_from_right = true;
  struct range_u32 merged_range;
  struct range_u32 new_vacancy;
  struct range_u32 * p_vacanco;
  new_vacancy = invalid_range;

  assume( pp_vacancy_back );
  assume( pp_vacancy_front );
  // vacancy_back first
  if ( *pp_vacancy_back && !has_tried_back_merge ) {
    merged_range.start = orig_rent->start;
    merged_range.stop = pp_vacancy_back[0]->stop;
    p_vacanco = *pp_vacancy_back;

    if ( get_aligned_range_len(merged_range, alignment_in_chunks, lsbs_headchunk_addr)
        >= new_len_in_chunks ) {
      if ( prealigned_head ) {
        take_from_right = false;
        goto skip_the_divider;
      } else {
dont_skip_the_divider:
        // Split vacancy in two pieces
        // NOTE It simply splits the range into 2 halves but there might be a better way.
        uint32_t nr_units_left_after_alloc = get_range_len(*&merged_range) - new_len_in_chunks;
        uint32_t first_unit_idx = nr_units_left_after_alloc/2 - new_len_in_chunks/2;
        first_unit_idx =
          _align_to_power_of_two( merged_range.start + first_unit_idx + lsbs_headchunk_addr,
              alignment_in_chunks )
          - lsbs_headchunk_addr;

        while ( first_unit_idx + new_len_in_chunks - 1 > merged_range.stop ) {
          first_unit_idx -= alignment_in_chunks;
        }

        if ( p_vacanco ) {
          // Prepare to register a new vacancy
          new_vacancy.start = first_unit_idx + new_len_in_chunks;
          new_vacancy.stop = merged_range.stop;
        } else {
          pp_vacancy_back[0]->start = first_unit_idx + new_len_in_chunks;
          pp_vacancy_back[0]->stop = merged_range.stop;
        }

        merged_range.stop = first_unit_idx - 1;

skip_the_divider:
        if ( !take_from_right ) {
          orig_rent->stop = merged_range.start + new_len_in_chunks - 1;
          orig_rent->start = merged_range.start;
        } else {
          orig_rent->start = merged_range.stop - new_len_in_chunks + 1;
          orig_rent->stop = merged_range.stop;
        }

        // Reduce the size of vacancy
        if ( !take_from_right ) {
          merged_range.start = merged_range.start + new_len_in_chunks;
        } else {
          merged_range.stop = merged_range.stop - new_len_in_chunks;
        }

        if ( p_vacanco ) {
          p_vacanco->start = merged_range.start;
          p_vacanco->stop = merged_range.stop;
        } else {
          pp_vacancy_front[0]->start = merged_range.start;
          pp_vacancy_front[0]->stop = merged_range.stop;
        }

        // See if there is a new vacancy to register
        if ( new_vacancy.start <= new_vacancy.stop ) {
          // Register new_vacancy
          typeof(p_vacanco) const p_new_slot = infos_al_get_elem(
            &p_smm->al_vacancies, userdata, custom_malloc_non_null,
            true, invalid_range32_union.u64, p_smm->nr_vacancy_infos );
          p_new_slot->start = new_vacancy.start;
          p_new_slot->stop = new_vacancy.stop;
          p_smm->nr_vacancy_infos++;
          if ( get_range_len(*p_vacanco) == 0 ) {
            typeof(p_vacanco) const p_last_vacancy = infos_al_get_elem_unsafe(
                &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
            *p_vacanco = *p_last_vacancy;
            if ( p_last_vacancy == *pp_vacancy_back ) {
              *pp_vacancy_back = nullptr;
            } else if ( p_last_vacancy == *pp_vacancy_front ) {
              *pp_vacancy_front = nullptr;
            }
            *p_last_vacancy = invalid_range;
            p_smm->nr_vacancy_infos--;
          }
          p_smm->total_space_left -= len_diff_in_chunks;
          return p_smm->data + (orig_rent->start << log2_chunk_len);
        }  // if ( new_vacancy.start <= new_vacancy.stop )

        if ( p_vacanco ) {
          if ( get_range_len(*p_vacanco) == 0 ) {
            typeof(p_vacanco) const p_last_vacancy = infos_al_get_elem_unsafe(
                &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
            *p_vacanco = *p_last_vacancy;
            if ( p_last_vacancy == *pp_vacancy_back ) {
              *pp_vacancy_back = p_vacanco;
            } else if ( p_last_vacancy == *pp_vacancy_front ) {
              *pp_vacancy_front = p_vacanco;
            }
            *p_last_vacancy = invalid_range;
            p_smm->nr_vacancy_infos--;
          }
        } else {
          if ( *pp_vacancy_back && get_range_len(**pp_vacancy_back) == 0 ) {
            typeof(p_vacanco) const p_last_vacancy = infos_al_get_elem_unsafe(
                &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
            **pp_vacancy_back = *p_last_vacancy;
            if ( p_last_vacancy == *pp_vacancy_front ) {
              *pp_vacancy_front = *pp_vacancy_back;
            }
            *p_last_vacancy = invalid_range;
            p_smm->nr_vacancy_infos--;
            //*pp_vacancy_back = nullptr;
          }
          if ( *pp_vacancy_front && get_range_len(**pp_vacancy_front) == 0 ) {
            typeof(p_vacanco) const p_last_vacancy = infos_al_get_elem_unsafe(
                &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
            **pp_vacancy_front = *p_last_vacancy;
            if ( p_last_vacancy == *pp_vacancy_back ) {
              *pp_vacancy_back = *pp_vacancy_front;
            }
            *p_last_vacancy = invalid_range;
            p_smm->nr_vacancy_infos--;
            //*pp_vacancy_front = nullptr;
          }
        }  // if ( p_vacanco ) {} else

        p_smm->total_space_left -= len_diff_in_chunks;
        return p_smm->data + (orig_rent->start << log2_chunk_len);
      }  // if ( prealigned_head ) {} else

    }  // if ( get_aligned_range_len(&merged_range, alignment_in_chunks, lsbs_headchunk_addr)
       //     >= new_len_in_chunks ) 

  }  // if ( pp_vacancy_back && !has_tried_back_merge )

  // Try to edit vacancy_front
  if ( *pp_vacancy_front && !has_tried_front_merge ) {
    merged_range.start = pp_vacancy_front[0]->start;
    merged_range.stop = orig_rent->stop;
    p_vacanco = pp_vacancy_front[0];

    if ( get_aligned_range_len(merged_range, alignment_in_chunks, lsbs_headchunk_addr)
        >= new_len_in_chunks ) {
      take_from_right = true;
      if ( prealigned_tail ) {
        goto skip_the_divider;
      } else {
        goto dont_skip_the_divider;
      }
    }
  }

  // Try to edit both vacancy_front and vacancy_back
  if ( *pp_vacancy_back && *pp_vacancy_front ) {
    merged_range.start = pp_vacancy_front[0]->start;
    merged_range.stop = pp_vacancy_back[0]->stop;
    p_vacanco = nullptr;

    if ( get_aligned_range_len(merged_range, alignment_in_chunks, lsbs_headchunk_addr)
        >= new_len_in_chunks ) {
      goto dont_skip_the_divider;
    }
  }

  return nullptr;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_realloc_aligned_chunks( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length,
    uint32_t alignment,
    uint32_t log2_chunk_len
    )
{
  if ( new_length == 0 ) {
    return 0;
  }
  uint64_t last_elem_idx = p_smm->nr_rents - 1;

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  assume( _is_power_of_two(alignment) );
  alignment = _align_to_power_of_two( alignment, 1 << log2_chunk_len );
  uint64_t lsbs_head_addr = (uintptr_t)p_smm->data & (alignment - 1);
  uint64_t alignment_in_chunks = alignment >> log2_chunk_len;
  assume( alignment_in_chunks >= 1 );
  uint64_t lsbs_headchunk_addr = lsbs_head_addr >> log2_chunk_len & (alignment_in_chunks - 1);

  const uint64_t target = (orig_ptr - p_smm->data) >> log2_chunk_len;
  struct range_u32 rent = { .start = target, .stop = 0 };
  //constexpr unsigned int stop_mask = 0xAAAAAAAAu;  // 0b101010..10
  constexpr unsigned int stop_mask = 0x55555555u;  // 0b010101..01
  const uint32_t new_len_in_chunks = ( new_length + (UINT64_C(1) << log2_chunk_len) - 1 )
    >> log2_chunk_len;
  struct range_u32 * p_vacancy_front = nullptr;
  struct range_u32 * p_vacancy_back = nullptr;
  uint_fast8_t nr_neighbors_found = 0;

  const uint64_t nr_elems_in_rblock = UINT64_C(1) << p_smm->al_rental_info.log2_elem_cnt;
  uint64_t base_ridx = 0;  // The base idx of current vacancy block
  const uint64_t base_ridx_cap = nr_elems_in_rblock * p_smm->al_rental_info.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict r_node = &p_smm->al_rental_info.list.head;
  do {  // while ( base_ridx < base_ridx_cap )
    void * rblock = r_node->arr_blocks[block_idx];

    typeof(&rent) const restrict p_key = __builtin_assume_aligned( rblock, ALIGN_FOR_AVX512 );
    simde__m256i pack1, pack2, res1, res2;
    const union range_union { struct range_u32 range; uint64_t u64; } key_union = {
      .range = rent };
    const simde__m256i key_pack = simde_mm256_set1_epi64x( key_union.u64 );
    const uint64_t last_elem_idx_in_this_block = ( last_elem_idx < nr_elems_in_rblock ) ?
      last_elem_idx : nr_elems_in_rblock - 1;
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(rent);
    for ( uint64_t elem_idx = 0; elem_idx <= last_elem_idx_in_this_block; elem_idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + elem_idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + elem_idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      mask &= stop_mask;
      mask2 &= stop_mask;
      constexpr unsigned int whole_over_half = sizeof(rent) / sizeof(rent.start);
      if ( mask ) {
        elem_idx = elem_idx + __tzcnt_u32(mask) / whole_over_half;
        goto found_the_key_to_remove;
      }
      if ( mask2 ) {
        elem_idx = elem_idx + ALIGN_FOR_AVX / sizeof(rent) + __tzcnt_u32(mask2) / whole_over_half;
found_the_key_to_remove:
        uint32_t nr_misaligned_chunks_at_head =
          ( (alignment_in_chunks - lsbs_headchunk_addr) & (alignment_in_chunks - 1) );
        uint32_t lsbs_last_chunk_addr_when_aligned =
          ( nr_misaligned_chunks_at_head + (new_len_in_chunks - 1) ) & ( alignment_in_chunks - 1 );
        // Is it pre-aligned in the head?
        bool prealigned_head = _is_aligned_to_power_of_two(
            p_key[elem_idx].start - lsbs_headchunk_addr, alignment_in_chunks);
        bool prealigned_tail = ((alignment_in_chunks - 1) & p_key[elem_idx].stop) ==
              lsbs_last_chunk_addr_when_aligned;

        int32_t len_diff_in_chunks = new_len_in_chunks - get_range_len(p_key[elem_idx]);
        if ( len_diff_in_chunks == 0 ) {
          p_smm->total_space_left -= 0;
          return orig_ptr;
        }
        //if ( get_aligned_range_len(p_key[elem_idx], alignment_in_chunks, lsbs_headchunk_addr) >=
        //    new_len_in_chunks ) {
        //  if ( prealigned_head ) {
        //    //if ( len_diff_in_chunks == 0 ) {
        //    //  return orig_ptr;
        //    //}
        //    // Try to find a vacancy right next to the original rent and merge
        //  }
        //  else if ( prealigned_tail ) {
        //    // Try to find a vacancy right in front of the original rent and merge
        //  }
        //}

        const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
        uint64_t base_vidx = 0;  // The base idx of current vacancy block
        const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
        //uint64_t node_base_idx = 0;
        uint64_t vblock_idx = 0;
        struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
        do {  // while ( base_vidx < base_vidx_cap )
          void * vblock = v_node->arr_blocks[vblock_idx];

          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
              goto end_of_neighbor_search;
            }
            struct range_u32 * restrict p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);

            if ( p_vacancy->stop == target - 1 ) {
              //available_space_front = rental_info->start - p_vacancy->start;
              p_vacancy_front = p_vacancy;
              nr_neighbors_found++;
            } else if ( p_vacancy->start == p_key[elem_idx].stop + 1 ) {
              //available_space_back = p_vacancy->start - rental_info->start;
              p_vacancy_back = p_vacancy;
              nr_neighbors_found++;
            }

            if ( nr_neighbors_found >= 2 ) {
              goto end_of_neighbor_search;
            }

          }  // for ( uint_fast32_t vacancy_idx = 0;
             //     vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

          vblock_idx++;
          base_vidx += nr_elems_in_vblock;
          if ( vblock_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
            v_node = v_node->next;
            assume( v_node );
            //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
            //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
            vblock_idx = 0;
          }
        } while ( base_vidx < base_vidx_cap );
end_of_neighbor_search:

        bool has_tried_front_merge = false;
        bool has_tried_back_merge = false;
        struct range_u32 * null = nullptr;
        if ( prealigned_head && p_vacancy_back ) {
          void * ret = try_to_merge_with_vacancy_right_next_for_an_aligned_alloc( p_smm,
              userdata, local_malloc,
              &p_vacancy_back, &null, p_key + elem_idx,
              alignment_in_chunks, lsbs_headchunk_addr,
              has_tried_back_merge, has_tried_front_merge, len_diff_in_chunks, log2_chunk_len );
          if ( ret ) {
            memmove( ret, orig_ptr, new_len_in_chunks << log2_chunk_len );
            return ret;
          }
          has_tried_back_merge = true;
        }
        if ( prealigned_tail && p_vacancy_front ) {
          void * ret = try_to_merge_with_vacancy_right_next_for_an_aligned_alloc( p_smm,
              userdata, local_malloc,
              &null, &p_vacancy_front, p_key + elem_idx,
              alignment_in_chunks, lsbs_headchunk_addr,
              has_tried_back_merge, has_tried_front_merge, len_diff_in_chunks, log2_chunk_len );
          if ( ret ) {
            memmove( ret, orig_ptr, new_len_in_chunks << log2_chunk_len );
            return ret;
          }
          has_tried_front_merge = true;
        }
        void * ret = try_to_merge_with_vacancy_right_next_for_an_aligned_alloc( p_smm,
            userdata, local_malloc,
            &p_vacancy_back, &p_vacancy_front, p_key + elem_idx,
            alignment_in_chunks, lsbs_headchunk_addr,
            has_tried_back_merge, has_tried_front_merge, len_diff_in_chunks, log2_chunk_len );
        if ( ret ) {
          memmove( ret, orig_ptr, new_len_in_chunks << log2_chunk_len );
          return ret;
        }

//no_surrounding_space_big_enough_to_take:
        void * new_ptr = infos_smm_rent_chunks( p_smm, userdata, local_malloc,
            new_len_in_chunks, true, log2_chunk_len );
        if ( !new_ptr ) {
          errno = ENOBUFS;
          return 0;
        }

        memcpy( new_ptr, orig_ptr, get_range_len(p_key[elem_idx]) > new_len_in_chunks ?
            new_len_in_chunks << log2_chunk_len :
            get_range_len(p_key[elem_idx]) << log2_chunk_len );

        struct range_u32 * restrict p_last_rent = infos_al_get_elem_unsafe(
            &p_smm->al_rental_info, p_smm->nr_rents - 1 );
        p_key[elem_idx] = *p_last_rent;
        *p_last_rent = invalid_range;
        p_smm->nr_rents--;
        //enum err_msg err = infos_smm_return_chunks( p_smm,
        //    userdata, local_malloc, orig_ptr, nullptr, log2_chunk_len );
        //if ( err ) {
        //  fprintf( stderr, "[SMM] infos_smm_realloc_aligned_chunks failed. "
        //      "(%s)\n", arr_emsg_txt[err] );
        //}
        //assume( err == emsg_no_error );
        p_smm->total_space_left -= len_diff_in_chunks;
        return new_ptr;

      }  // if ( mask2 )

    }  // for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride )

    block_idx++;
    base_ridx += nr_elems_in_rblock;
    last_elem_idx -= nr_elems_in_rblock;
    if ( block_idx >= _arr_elem_cnt(r_node->arr_blocks) ) {
      r_node = r_node->next;
      assume( r_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_rental_info.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_rental_info.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_ridx < base_ridx_cap );

  // Couldn't find the rental information about ptr.
  errno = EBADSLT;
  return 0;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_realloc( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length )
{
  if ( new_length == 0 ) {
    return 0;
  }

  uint64_t last_elem_idx = p_smm->nr_rents - 1;

  const uint64_t target = (orig_ptr - p_smm->data);
  struct range_u32 rent = { .start = target, .stop = 0 };
  //constexpr unsigned int stop_mask = 0xAAAAAAAAu;  // 0b101010..10
  constexpr unsigned int stop_mask = 0x55555555u;  // 0b010101..01
  struct range_u32 * p_vacancy_front = nullptr;
  struct range_u32 * p_vacancy_back = nullptr;
  uint_fast8_t nr_neighbors_found = 0;

  const uint64_t nr_elems_in_rblock = UINT64_C(1) << p_smm->al_rental_info.log2_elem_cnt;
  uint64_t base_ridx = 0;  // The base idx of current vacancy block
  const uint64_t base_ridx_cap = nr_elems_in_rblock * p_smm->al_rental_info.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict r_node = &p_smm->al_rental_info.list.head;
  do {  // while ( base_ridx < base_ridx_cap )
    void * rblock = r_node->arr_blocks[block_idx];

    typeof(&rent) const restrict p_key = __builtin_assume_aligned( rblock, ALIGN_FOR_AVX512 );
    simde__m256i pack1, pack2, res1, res2;
    const union range_union { struct range_u32 range; uint64_t u64; } key_union = {
      .range = rent };
    const simde__m256i key_pack = simde_mm256_set1_epi64x( key_union.u64 );
    const uint64_t last_elem_idx_in_this_block = ( last_elem_idx < nr_elems_in_rblock ) ?
      last_elem_idx : nr_elems_in_rblock - 1;
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(rent);
    for ( uint64_t elem_idx = 0; elem_idx <= last_elem_idx_in_this_block; elem_idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + elem_idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + elem_idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      mask &= stop_mask;
      mask2 &= stop_mask;
      constexpr unsigned int whole_over_half = sizeof(rent) / sizeof(rent.start);
      if ( mask ) {
        elem_idx = elem_idx + __tzcnt_u32(mask) / whole_over_half;
        goto found_the_key_to_remove;
      }
      if ( mask2 ) {
        elem_idx = elem_idx + ALIGN_FOR_AVX / sizeof(rent) + __tzcnt_u32(mask2) / whole_over_half;
found_the_key_to_remove:
        int32_t len_diff_in_chunks = new_length - get_range_len(p_key[elem_idx]);
        if ( len_diff_in_chunks == 0 ) {
          p_smm->total_space_left -= 0;
          return orig_ptr;
        }

        const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
        uint64_t base_vidx = 0;  // The base idx of current vacancy block
        const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
        //uint64_t node_base_idx = 0;
        uint64_t vblock_idx = 0;
        struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
        do {  // while ( base_vidx < base_vidx_cap )
          void * vblock = v_node->arr_blocks[vblock_idx];

          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
              goto end_of_neighbor_search;
            }
            struct range_u32 * restrict p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);

            if ( p_vacancy->stop == target - 1 ) {
              //available_space_front = rental_info->start - p_vacancy->start;
              p_vacancy_front = p_vacancy;
              nr_neighbors_found++;
            } else if ( p_vacancy->start == p_key[elem_idx].stop + 1 ) {
              //available_space_back = p_vacancy->start - rental_info->start;
              p_vacancy_back = p_vacancy;
              nr_neighbors_found++;
            }

            if ( nr_neighbors_found >= 2 ) {
              goto end_of_neighbor_search;
            }

          }  // for ( uint_fast32_t vacancy_idx = 0;
             //     vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

          vblock_idx++;
          base_vidx += nr_elems_in_vblock;
          if ( vblock_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
            v_node = v_node->next;
            assume( v_node );
            //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
            //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
            vblock_idx = 0;
          }
        } while ( base_vidx < base_vidx_cap );
end_of_neighbor_search:

        if ( p_vacancy_back ) {
          if ( len_diff_in_chunks > 0 ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if ( get_range_len( *p_vacancy_back ) < len_diff_in_chunks ) {
#pragma GCC diagnostic pop
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the back
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            bool whole_merge = get_range_len(*p_vacancy_back) == len_diff_in_chunks ?
#pragma GCC diagnostic pop
              true : false;
            p_key[elem_idx].stop += len_diff_in_chunks;
            if ( !whole_merge ) {
              p_vacancy_back->start += len_diff_in_chunks;
            } else {  // whole_merge
              typeof(p_vacancy_back) const p_last_vacancy = infos_al_get_elem_unsafe(
                  &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
              *p_vacancy_back = *p_last_vacancy;
              *p_last_vacancy = invalid_range;
              p_smm->nr_vacancy_infos--;
              //p_vacancy_back = nullptr;
            }
            goto return_new_alloc;
          } else {  // len_diff_in_chunks < 0
            // Transfer space to the vacancy in the back
            p_key[elem_idx].stop += len_diff_in_chunks;
            p_vacancy_back->start += len_diff_in_chunks;
            goto return_new_alloc;
          }
        }  // if ( p_vacancy_back )

        if ( p_vacancy_front ) {
          if ( len_diff_in_chunks > 0 ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if ( get_range_len( *p_vacancy_front ) < len_diff_in_chunks ) {
#pragma GCC diagnostic pop
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the front
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            bool whole_merge = get_range_len(*p_vacancy_front) == len_diff_in_chunks ?
#pragma GCC diagnostic pop
              true : false;
            p_key[elem_idx].start -= len_diff_in_chunks;
            if ( !whole_merge ) {
              p_vacancy_front->stop -= len_diff_in_chunks;
            } else {  // whole_merge
              typeof(p_vacancy_front) const p_last_vacancy = infos_al_get_elem_unsafe(
                  &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
              *p_vacancy_front = *p_last_vacancy;
              *p_last_vacancy = invalid_range;
              p_smm->nr_vacancy_infos--;
              //p_vacancy_front = nullptr;
            }
            goto return_new_alloc;
          } else {
            // Transfer space to the vacancy in the front
            p_key[elem_idx].stop += len_diff_in_chunks;
            p_vacancy_front->start += len_diff_in_chunks;
            goto return_new_alloc;
          }  // if ( len_diff_in_chunks > 0 ) {} else
        }  // if ( p_vacancy_front )

        // Try to edit both vacancy_front and vacancy_back
        if ( p_vacancy_back && p_vacancy_front ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
          if ( len_diff_in_chunks > 0 &&
              (get_range_len(*p_vacancy_front) + get_range_len(*p_vacancy_back)
              < len_diff_in_chunks) ) {
#pragma GCC diagnostic pop
            goto no_surrounding_space_big_enough_to_take;
          }
          // Take space from the front first
          uint32_t added_len = get_range_len(*p_vacancy_front);
          p_key[elem_idx].start -= added_len;
          typeof(p_vacancy_front) p_last_vacancy = infos_al_get_elem_unsafe(
              &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
          *p_vacancy_front = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_smm->nr_vacancy_infos--;

          // Take space from the back
          uint32_t deficit = len_diff_in_chunks - added_len;
          p_key[elem_idx].stop += deficit;
          p_vacancy_back->start += deficit;
          if ( get_range_len(*p_vacancy_back) == 0 ) {
            p_last_vacancy = infos_al_get_elem_unsafe(
                &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
            *p_vacancy_back = *p_last_vacancy;
            *p_last_vacancy = invalid_range;
            p_smm->nr_vacancy_infos--;
          }
return_new_alloc:
          p_smm->total_space_left -= len_diff_in_chunks;
          return p_smm->data + p_key[elem_idx].start;
        }  // if ( p_vacancy_back && p_vacancy_front )

no_surrounding_space_big_enough_to_take:
        void * new_ptr = infos_smm_rent( p_smm, userdata, custom_malloc, new_length, true );
        if ( !new_ptr ) {
          errno = ENOBUFS;
          return 0;
        }

        memcpy( new_ptr, orig_ptr, get_range_len(p_key[elem_idx]) > new_length ?
            new_length : get_range_len(p_key[elem_idx]) );

        struct range_u32 * restrict p_last_rent = infos_al_get_elem_unsafe(
            &p_smm->al_rental_info, p_smm->nr_rents - 1 );
        p_key[elem_idx] = *p_last_rent;
        *p_last_rent = invalid_range;
        p_smm->nr_rents--;
        //enum err_msg err = infos_smm_return( p_smm, userdata, custom_malloc, orig_ptr );
        //assume( err == emsg_no_error );
        //if ( err ) {
        //  fprintf( stderr, "[SMM] infos_smm_realloc failed. (%s)\n", arr_emsg_txt[err] );
        //}
        //assume( err == emsg_no_error );
        p_smm->total_space_left -= len_diff_in_chunks;
        return new_ptr;

      }  // if ( mask2 )

    }  // for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride )

    block_idx++;
    base_ridx += nr_elems_in_rblock;
    last_elem_idx -= nr_elems_in_rblock;
    if ( block_idx >= _arr_elem_cnt(r_node->arr_blocks) ) {
      r_node = r_node->next;
      assume( r_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_rental_info.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_rental_info.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_ridx < base_ridx_cap );

  // Couldn't find the rental information about ptr.
  errno = EBADSLT;
  return 0;
}


//-------------------------------------------------------------------------------------------------
size_t infos_smm_get_size(
    struct infos_simple_mem_mgr * restrict p_smm,
    uint32_t pos
    )
{
  uint64_t last_elem_idx = p_smm->nr_rents - 1;

  struct range_u32 rent = { .start = pos, .stop = 0 };
  //constexpr unsigned int stop_mask = 0xAAAAAAAAu;  // 0b101010..10
  constexpr unsigned int stop_mask = 0x55555555u;  // 0b010101..01

  const uint64_t nr_elems_in_rblock = UINT64_C(1) << p_smm->al_rental_info.log2_elem_cnt;
  uint64_t base_ridx = 0;  // The base idx of current vacancy block
  const uint64_t base_ridx_cap = nr_elems_in_rblock * p_smm->al_rental_info.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict r_node = &p_smm->al_rental_info.list.head;
  do {  // while ( base_ridx < base_ridx_cap )
    void * rblock = r_node->arr_blocks[block_idx];

    typeof(&rent) const restrict p_key = __builtin_assume_aligned( rblock, ALIGN_FOR_AVX512 );
    simde__m256i pack1, pack2, res1, res2;
    const union range_union { struct range_u32 range; uint64_t u64; } key_union = {
      .range = rent };
    const simde__m256i key_pack = simde_mm256_set1_epi64x( key_union.u64 );
    const uint64_t last_elem_idx_in_this_block = ( last_elem_idx < nr_elems_in_rblock ) ?
      last_elem_idx : nr_elems_in_rblock - 1;
    constexpr uint64_t stride = ALIGN_FOR_AVX * 2 / sizeof(rent);
    for ( uint64_t elem_idx = 0; elem_idx <= last_elem_idx_in_this_block; elem_idx += stride ) {
      pack1 = simde_mm256_load_si256( (void*)(p_key + elem_idx) );
      pack2 = simde_mm256_load_si256( (void*)(p_key + elem_idx) + ALIGN_FOR_AVX );
      res1 = simde_mm256_cmpeq_epi32( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi32( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_ps( (simde__m256) res1 );
      unsigned int mask2 = simde_mm256_movemask_ps( (simde__m256) res2 );
      mask &= stop_mask;
      mask2 &= stop_mask;
      constexpr unsigned int whole_over_half = sizeof(rent) / sizeof(rent.start);
      if ( mask ) {
        elem_idx = elem_idx + __tzcnt_u32(mask) / whole_over_half;
        goto found_the_key_to_remove;
      }
      if ( mask2 ) {
        elem_idx = elem_idx + ALIGN_FOR_AVX / sizeof(rent) + __tzcnt_u32(mask2) / whole_over_half;
found_the_key_to_remove:
        typeof(p_key[0].start) length = get_range_len(p_key[elem_idx]);
        return length;
      }  // if ( mask2 )

    }  // for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride )

    block_idx++;
    base_ridx += nr_elems_in_rblock;
    last_elem_idx -= nr_elems_in_rblock;
    if ( block_idx >= _arr_elem_cnt(r_node->arr_blocks) ) {
      r_node = r_node->next;
      assume( r_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_rental_info.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_rental_info.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_ridx < base_ridx_cap );

  return 0;
}


//-------------------------------------------------------------------------------------------------
void infos_smm_destroy( struct infos_simple_mem_mgr * p_smm,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  infos_al_destroy_except_preallocated_struct(&p_smm->al_rental_info, userdata, custom_free);
  infos_al_destroy_except_preallocated_struct(&p_smm->al_vacancies, userdata, custom_free);
}



//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_resize_pool( struct infos_simple_mem_mgr * restrict p_smm,
    void * restrict userdata,
    void * (*custom_malloc_non_null)(void * userdata, size_t alignment, size_t size),
    uint32_t total_len_buf_n_struct )
{
  assume( _is_aligned_to_power_of_two(
        total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr),
        alignof(struct infos_simple_mem_mgr)) );

  struct range_u32 new_vacancy;

  new_vacancy.start = p_smm->data_len;
  new_vacancy.stop = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr) - 1;

  // Make a vacancy and merge if possible
  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_smm->al_vacancies.log2_elem_cnt;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_smm->al_vacancies.active_block_cnt;
  //uint64_t node_base_idx = 0;
  uint64_t block_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_smm->al_vacancies.list.head;
  do {  // while ( base_vidx < nr_elems_in_vblock * p_smm->al_vacancies.active_blocks_cnt )
    void * vblock = v_node->arr_blocks[block_idx];

    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_smm->nr_vacancy_infos ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }
      struct range_u32 * restrict p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);

      if ( p_vacancy->stop == new_vacancy.start - 1 ) {
        merge_range_into_lower(p_vacancy, &new_vacancy);
        typeof(p_vacancy) const p_last_vacancy = infos_al_get_elem_unsafe(
            &p_smm->al_vacancies, p_smm->nr_vacancy_infos - 1 );
        *p_vacancy = *p_last_vacancy;
        *p_last_vacancy = invalid_range;
        p_smm->nr_vacancy_infos--;
      }

    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    block_idx++;
    base_vidx += nr_elems_in_vblock;
    if ( block_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_smm->al_vacancies.log2_elem_cnt +
      //    _arr_elem_cnt(p_smm->al_vacancies.list.head.arr_blocks) );
      block_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );

//no_existing_range_to_merge:
  typeof(&new_vacancy) const p_new_slot = infos_al_get_elem(
    &p_smm->al_vacancies, userdata, custom_malloc_non_null,
    true, invalid_range32_union.u64, p_smm->nr_vacancy_infos );
  p_new_slot->start = new_vacancy.start;
  p_new_slot->stop = new_vacancy.stop;
  p_smm->nr_vacancy_infos++;

  // Update data_len
  typeof(p_smm->data_len) new_data_len;
  new_data_len = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
  p_smm->total_space_left += p_smm->data_len - new_data_len;
  p_smm->data_len = new_data_len;
  return emsg_no_error;
}


