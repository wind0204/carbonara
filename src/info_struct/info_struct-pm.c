#include <stdint.h>
#include <errno.h>
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>

#include "err_msg.h"
#include "info_struct.h"
#include "util-c_lang-basic.h"
#include "util-spall.h"



//-------------------------------------------------------------------------------------------------
static enum err_msg remove_rent_and_update_vacancy(
  struct infos_page_mgr * restrict p_pm,
  void * restrict userdata,
  void * (*custom_malloc_non_null)(void * userdata, size_t alignment, size_t size),
  struct range_u32 * p_rent,
  struct range_u32 * p_rightmost_rent );

//-------------------------------------------------------------------------------------------------
enum err_msg infos_pm_init_prealloc(
    struct infos_page_mgr * restrict p_pm,
    uint32_t nr_pages,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  SPALL_BEGIN("pm_init");
  assume( nr_pages >= 2 );

  p_pm->nr_unused_pages = nr_pages;
  p_pm->nr_rents = 0;
  p_pm->nr_vacancy_infos = 1;

  constexpr size_t min_size_range_db = CACHELINE_LEN * 8;
  constexpr size_t rental_info_node_size = min_size_range_db * 2;
  constexpr size_t available_ranges_node_size = min_size_range_db;
  assume( (rental_info_node_size + available_ranges_node_size) % CACHELINE_LEN == 0 );

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  void * alloc = local_malloc( userdata, ALIGN_FOR_AVX,
      rental_info_node_size + available_ranges_node_size );
  if ( !alloc ) {
    SPALL_END();
    return emsg_out_of_memory;
  }

  // To just copy and paste I created one shared variable here.
  struct infos_arr_list * restrict al;

  al = &p_pm->al_rental_info;
  auto node_size = rental_info_node_size;
  // Copy
  al->log2_block_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_nr_elems = al->log2_block_size - al->log2_elem_size;
  al->list.head.arr_blocks[0] = alloc;
  al->list.head.next = nullptr;
  al->nr_active_blocks = 1;

  // Just in case we want to do AVX search on al_vacancies in the future
  static_assert( _is_aligned_to_power_of_two(rental_info_node_size, ALIGN_FOR_AVX512) );
  static_assert( _is_aligned_to_power_of_two(available_ranges_node_size, ALIGN_FOR_AVX512) );

  const simde__m256i key_pack = simde_mm256_set1_epi64x( invalid_range32_union.u64 );
  constexpr uint64_t stride = ALIGN_FOR_AVX;
  void * restrict p_vectors = __builtin_assume_aligned(
      p_pm->al_rental_info.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t idx = 0; idx < rental_info_node_size >> al->log2_elem_size; idx += stride ) {
    simde_mm256_store_si256( p_vectors + idx, key_pack );
  }

  alloc = alloc + rental_info_node_size;
  al = &p_pm->al_vacancies;
  node_size = available_ranges_node_size;
  // Paste
  al->log2_block_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_nr_elems = al->log2_block_size - al->log2_elem_size;
  al->list.head.arr_blocks[0] = alloc;
  al->list.head.next = nullptr;
  al->nr_active_blocks = 1;

  p_vectors = __builtin_assume_aligned(
      p_pm->al_vacancies.list.head.arr_blocks[0], ALIGN_FOR_AVX512 );
  for ( uint64_t byte_idx = 0; byte_idx < rental_info_node_size; byte_idx += stride ) {
    simde_mm256_store_si256( p_vectors + byte_idx, key_pack );
  }

  //struct range_u32 * const first_rental_info = infos_al_get_elem(&p_pm->al_rental_info, 0);
  //*first_rental_info = invalid_range;

  struct range_u32 * const first_available_range = infos_al_get_elem_unsafe(
      &p_pm->al_vacancies, 0);
  first_available_range->start = 0;
  first_available_range->stop = nr_pages - 1;

  SPALL_END();
  return 0;
}


//-------------------------------------------------------------------------------------------------
void infos_pm_destroy( struct infos_page_mgr * p_pm,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  infos_al_destroy_except_preallocated_struct(&p_pm->al_rental_info, userdata, custom_free);
  infos_al_destroy_except_preallocated_struct(&p_pm->al_vacancies, userdata, custom_free);
}


//-------------------------------------------------------------------------------------------------
struct range_u32 infos_pm_rent( struct infos_page_mgr * restrict p_pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_seq_pages_needed,
    bool take_from_left
    )
{
  SPALL_BEGIN("pm_rent");

  if ( p_pm->nr_unused_pages < nr_seq_pages_needed ) {
    errno = ENOBUFS;
    return invalid_range;
  }

  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_pm->al_vacancies.nr_active_blocks;
  //uint64_t node_base_idx = 0;
  uint64_t vblock_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_pm->al_vacancies.list.head;
  do {  // while ( base_vidx < base_vidx_cap )
    void * vblock = v_node->arr_blocks[vblock_idx];

    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_pm->nr_vacancy_infos ) {
        // Couldn't find a vacancy
        errno = ENOBUFS;
        SPALL_END();
        return invalid_range;
      }

      struct range_u32 * restrict p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);
      //if ( is_range_u32_invalid(*p_vacancy) ) {
      //  continue;
      //}

      if ( get_range_len(*p_vacancy) >= nr_seq_pages_needed ) {
        // Register the rental_info
        void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
        local_malloc = custom_malloc ? custom_malloc : default_malloc;
        struct range_u32 * restrict p_rent_slot = infos_al_get_elem(
            &p_pm->al_rental_info, userdata, local_malloc,
            true, invalid_range32_union.u64, p_pm->nr_rents );
        if ( !p_rent_slot ) {
          errno = ENOMEM;
          SPALL_END();
          return invalid_range;
        }
        if ( take_from_left ) {
          p_rent_slot->stop = p_vacancy->start + nr_seq_pages_needed - 1;
          p_rent_slot->start = p_vacancy->start;
        } else {
          p_rent_slot->start = p_vacancy->stop - nr_seq_pages_needed + 1;
          p_rent_slot->stop = p_vacancy->stop;
        }

        // Reduce the size of vacancy
        if ( take_from_left )
          p_vacancy->start = p_vacancy->start + nr_seq_pages_needed;
        else
          p_vacancy->stop = p_vacancy->stop - nr_seq_pages_needed;

        if ( get_range_len(*p_vacancy) == 0 ) {
          typeof(p_vacancy) const p_last_vacancy = infos_al_get_elem_unsafe(
              &p_pm->al_vacancies, p_pm->nr_vacancy_infos - 1 );
          *p_vacancy = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_pm->nr_vacancy_infos--;
        }

        // TODO The compiler should optimize this line and just use nr_seq_pages_needed
        p_pm->nr_unused_pages -= get_range_len(*p_rent_slot);
        p_pm->nr_rents++;
        SPALL_END();
        return *p_rent_slot;
      }  // if ( get_range_len(*p_vacancy) >= nr_seq_pages_needed )
    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    vblock_idx++;
    base_vidx += nr_elems_in_vblock;
    if ( vblock_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_pm->al_vacancies.log2_nr_elems +
      //    _arr_elem_cnt(p_pm->al_vacancies.list.head.arr_blocks) );
      vblock_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  SPALL_END();
  return invalid_range;
}


constexpr size_t nr_tries_to_find_a_prealigned_space = 3;

//-------------------------------------------------------------------------------------------------
struct range_u32 infos_pm_rent_aligned( struct infos_page_mgr * restrict p_pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_seq_pages_needed,
    uint32_t alignment
    )
{
  SPALL_BEGIN("pm_rent_aligned");
  assume( alignment > 1 );
  assume( _is_power_of_two(alignment) );
  //assume( _is_aligned_to_power_of_two(length, alignment) );

  if ( p_pm->nr_unused_pages < nr_seq_pages_needed ) {
    errno = ENOBUFS;
    SPALL_END();
    return invalid_range;
  }

  //uint32_t lsbs_head_addr = (uintptr_t)p_pm->data & (alignment - 1);
  constexpr uint32_t lsbs_head_addr = 0;
  //uint32_t nr_misaligned_bytes_at_head = (alignment - lsbs_head_addr) & (alignment - 1);
  constexpr uint32_t nr_misaligned_bytes_at_head = 0;
  uint32_t lsbs_tail_addr_when_aligned =
    (nr_misaligned_bytes_at_head + nr_seq_pages_needed - 1) & (alignment - 1);

  bool take_from_right = true;  // Let us take the right-most part in most of the cases.
  uint_fast8_t nr_tries_to_find_prealigned = 0;
  struct range_u32 * spacious_range = nullptr;
  struct range_u32 * restrict p_vacancy;

  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_pm->al_vacancies.nr_active_blocks;
  //uint64_t node_base_idx = 0;
  uint64_t vblock_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_pm->al_vacancies.list.head;
  do {  // while ( base_vidx < base_vidx_cap )
    void * vblock = v_node->arr_blocks[vblock_idx];
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_pm->nr_vacancy_infos ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        SPALL_END();
        return invalid_range;
      }

      p_vacancy = vblock + vacancy_idx * sizeof(*p_vacancy);
      struct range_u32 new_vacancy = invalid_range;

      if ( get_aligned_range_len(*p_vacancy, alignment, lsbs_head_addr) >= nr_seq_pages_needed ) {
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
        uint32_t nr_units_left_after_alloc = get_range_len(*p_vacancy) - nr_seq_pages_needed;
        uint32_t first_unit_idx = nr_units_left_after_alloc/2 - nr_seq_pages_needed/2;
        first_unit_idx =
          _align_to_power_of_two(p_vacancy->start + first_unit_idx
              + lsbs_head_addr, alignment)
          - lsbs_head_addr;

        while ( first_unit_idx + nr_seq_pages_needed - 1 > p_vacancy->stop ) {
          first_unit_idx -= alignment;
        }

        // Prepare to register a new vacancy
        new_vacancy.start = first_unit_idx + nr_seq_pages_needed;
        new_vacancy.stop = p_vacancy->stop;

        p_vacancy->stop = first_unit_idx - 1;


register_new_rental_info:
        // Register the rental_info
        void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
        local_malloc = custom_malloc ? custom_malloc : default_malloc;
        struct range_u32 * restrict p_rent_slot = infos_al_get_elem(
            &p_pm->al_rental_info, userdata, local_malloc,
            true, invalid_range32_union.u64, p_pm->nr_rents );
        if ( !p_rent_slot ) {
          errno = ENOMEM;
          SPALL_END();
          return invalid_range;
        }
        if ( !take_from_right ) {
          p_rent_slot->stop = p_vacancy->start + nr_seq_pages_needed - 1;
          p_rent_slot->start = p_vacancy->start;
        } else {
          p_rent_slot->start = p_vacancy->stop - nr_seq_pages_needed + 1;
          p_rent_slot->stop = p_vacancy->stop;
        }

        // Reduce the size of vacancy
        if ( !take_from_right )
          p_vacancy->start = p_vacancy->start + nr_seq_pages_needed;
        else
          p_vacancy->stop = p_vacancy->stop - nr_seq_pages_needed;

        // See if there is a new vacancy to register
        if ( is_range_u32_valid(new_vacancy) ) {
          typeof(p_vacancy) const p_new_slot = infos_al_get_elem(
            &p_pm->al_vacancies, userdata, local_malloc,
            true, invalid_range32_union.u64, p_pm->nr_vacancy_infos );
          if ( !p_new_slot ) {
            // Revert changes
            *p_rent_slot = invalid_range;
            if ( !take_from_right )
              p_vacancy->start = p_vacancy->start - nr_seq_pages_needed;
            else
              p_vacancy->stop = p_vacancy->stop + nr_seq_pages_needed;

            errno = ENOMEM;
            SPALL_END();
            return invalid_range;
          }
          p_new_slot->start = new_vacancy.start;
          p_new_slot->stop = new_vacancy.stop;
          p_pm->nr_vacancy_infos++;
        }

        if ( get_range_len(*p_vacancy) == 0 ) {
          typeof(p_vacancy) const p_last_vacancy = infos_al_get_elem_unsafe(
              &p_pm->al_vacancies, p_pm->nr_vacancy_infos - 1 );
          *p_vacancy = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_pm->nr_vacancy_infos--;
        }

        // TODO The compiler should optimize this line and just use nr_seq_pages_needed
        p_pm->nr_unused_pages -= get_range_len(*p_rent_slot);
        p_pm->nr_rents++;
        SPALL_END();
        return *p_rent_slot;

      }  // if ( get_aligned_range_len(*p_vacancy, alignment, lsbs_head_addr) >=
         //     nr_seq_pages_needed )

    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    vblock_idx++;
    base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
    if ( vblock_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_pm->al_vacancies.log2_nr_elems +
      //    _arr_elem_cnt(p_pm->al_vacancies.list.head.arr_blocks) );
      vblock_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );

  if ( spacious_range ) {
    p_vacancy = spacious_range;
    goto come_back_here_with_a_spacious_range;
  }
  SPALL_END();
  return invalid_range;
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_pm_return( struct infos_page_mgr * restrict p_pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    struct range_u32 rent
    )
{
  SPALL_BEGIN("pm_return");

  uint64_t last_elem_idx = p_pm->nr_rents - 1;

  const uint64_t nr_elems_in_rblock = UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;
  uint64_t base_ridx = 0;  // The base idx of current vacancy block
  const uint64_t base_ridx_cap = nr_elems_in_rblock * p_pm->al_rental_info.nr_active_blocks;
  //uint64_t node_base_idx = 0;
  uint64_t rblock_idx = 0;
  struct linked_list_of_lists_node * restrict r_node = &p_pm->al_rental_info.list.head;
  do {  // while ( base_ridx < base_ridx_cap )
    void * rblock = r_node->arr_blocks[rblock_idx];

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
      res1 = simde_mm256_cmpeq_epi64( pack1, key_pack );
      res2 = simde_mm256_cmpeq_epi64( pack2, key_pack );
      unsigned int mask = simde_mm256_movemask_pd( (simde__m256d) res1 );
      unsigned int mask2 = simde_mm256_movemask_pd( (simde__m256d) res2 );
      if ( mask ) {
        elem_idx = elem_idx + __tzcnt_u32(mask);
        goto found_the_key_to_remove;
      }
      if ( mask2 ) {
        elem_idx = elem_idx + ALIGN_FOR_AVX / sizeof(rent) + __tzcnt_u32(mask2);
found_the_key_to_remove:
        struct range_u32 * restrict p_last_rent = infos_al_get_elem_unsafe(
            &p_pm->al_rental_info, p_pm->nr_rents - 1 );
        void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
        local_malloc = custom_malloc ? custom_malloc : default_malloc;
        enum err_msg err = remove_rent_and_update_vacancy( p_pm, userdata, local_malloc,
            p_key+elem_idx, p_last_rent );
        if ( err ) {
          SPALL_END();
          return err;
        }
        SPALL_END();
        return emsg_no_error;
      }  // if ( mask2 )

    }  // for ( uint64_t idx = 0; idx < last_idx_plus1; idx += stride )

    rblock_idx++;
    base_ridx += nr_elems_in_rblock;
    last_elem_idx -= nr_elems_in_rblock;
    if ( rblock_idx >= _arr_elem_cnt(r_node->arr_blocks) ) {
      r_node = r_node->next;
      assume( r_node );
      //node_base_idx += UINT64_C(1) << ( p_pm->al_rental_info.log2_nr_elems +
      //    _arr_elem_cnt(p_pm->al_rental_info.list.head.arr_blocks) );
      rblock_idx = 0;
    }
  } while ( base_ridx < base_ridx_cap );

  SPALL_END();
  return emsg_no_such_entry;
}

//-------------------------------------------------------------------------------------------------
static enum err_msg remove_rent_and_update_vacancy(
  struct infos_page_mgr * restrict p_pm,
  void * restrict userdata,
  void * (*custom_malloc_non_null)(void * userdata, size_t alignment, size_t size),
  struct range_u32 * p_rent,
  struct range_u32 * p_rightmost_rent )
{
  // Make a new vacancy information or merge with an existing one if it's a neighbor
  const uint64_t nr_elems_in_vblock = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
  uint64_t base_vidx = 0;  // The base idx of current vacancy block
  const uint64_t base_vidx_cap = nr_elems_in_vblock * p_pm->al_vacancies.nr_active_blocks;
  //uint64_t node_base_idx = 0;
  uint64_t vblock_idx = 0;
  struct linked_list_of_lists_node * restrict v_node = &p_pm->al_vacancies.list.head;
  struct range_u32 * restrict p_vacancy_to_merge_with = nullptr;
  bool has_been_merged = false;
  do {  // while ( base_vidx < base_vidx_cap )
    void * vblock = v_node->arr_blocks[vblock_idx];
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_pm->nr_vacancy_infos ) {
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
              &p_pm->al_vacancies, p_pm->nr_vacancy_infos - 1 );
          *p_vacancy_to_merge_with = *p_last_vacancy;
          *p_last_vacancy = invalid_range;
          p_pm->nr_vacancy_infos--;
          p_vacancy_to_merge_with = nullptr;
        }

        if ( !p_vacancy_to_merge_with ) {
          p_pm->nr_unused_pages += get_range_len(*p_rent);

          *p_rent = *p_rightmost_rent;
          *p_rightmost_rent = invalid_range;
          p_pm->nr_rents--;
          return emsg_no_error;
        }
      }  // if ( !p_vacancy_to_merge_with ) {} else

    }  // for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems_in_vblock; vacancy_idx++ )

    vblock_idx++;
    base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
    if ( vblock_idx >= _arr_elem_cnt(v_node->arr_blocks) ) {
      v_node = v_node->next;
      assume( v_node );
      //node_base_idx += UINT64_C(1) << ( p_pm->al_vacancies.log2_nr_elems +
      //    _arr_elem_cnt(p_pm->al_vacancies.list.head.arr_blocks) );
      vblock_idx = 0;
    }
  } while ( base_vidx < base_vidx_cap );
out_of_nested_loop:
  if ( has_been_merged ) {
    p_pm->nr_unused_pages += get_range_len(*p_rent);

    *p_rent = *p_rightmost_rent;
    *p_rightmost_rent = invalid_range;
    p_pm->nr_rents--;
    return emsg_no_error;
  }

  // Couldn't merge into any existing vacancy

  // Register a new one
  typeof(p_vacancy_to_merge_with) const p_new_slot = infos_al_get_elem(
    &p_pm->al_vacancies, userdata, custom_malloc_non_null,
    true, invalid_range32_union.u64, p_pm->nr_vacancy_infos );
  if ( !p_new_slot ) {
    return emsg_out_of_memory;
  }
  p_new_slot->start = p_rent->start;
  p_new_slot->stop = p_rent->stop;
  p_pm->nr_vacancy_infos++;
  p_pm->nr_unused_pages += get_range_len(*p_rent);

  *p_rent = *p_rightmost_rent;
  *p_rightmost_rent = invalid_range;
  p_pm->nr_rents--;
  return emsg_no_error;
}

