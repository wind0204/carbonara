#include <immintrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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
 */
static uint32_t infos_smm_rent_impl( struct infos_simple_mem_mgr * restrict manager,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict userdata, uint32_t length, uint8_t take_from_left );

//-------------------------------------------------------------------------------------------------
static enum err_msg infos_smm_return_impl( struct infos_simple_mem_mgr * restrict manager,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * userdata, uint32_t * nr_units_released, uint32_t target );

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_impl( struct infos_simple_mem_mgr * restrict manager,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict userdata, uint32_t length, uint32_t alignment );

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_chunks_impl(
    struct infos_simple_mem_mgr * restrict manager,
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
  new_manager->idx_last_rental_info = 0;
  new_manager->idx_last_vacancy = 0;

  infos_al_create_prealloc( &new_manager->al_rental_info,
      userdata, custom_malloc, CACHELINE_LEN * 8, sizeof(struct range_u32), 0 );
  infos_al_create_prealloc( &new_manager->al_vacancies,
      userdata, custom_malloc, CACHELINE_LEN * 4, sizeof(struct range_u32), 0 );

  struct range_u32 * first_one = infos_al_get_elem(&new_manager->al_rental_info, 0);
  first_one->start = -7;
  first_one->stop = 7;
  first_one = infos_al_get_elem(&new_manager->al_vacancies, 0);
  first_one->start = 0;
  first_one->stop = new_manager->data_len - 1;

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
  new_manager->idx_last_rental_info = 0;
  new_manager->idx_last_vacancy = 0;

  constexpr size_t min_size_ranges_database = CACHELINE_LEN * 4;
  infos_al_create_prealloc( &new_manager->al_rental_info,
      userdata, custom_malloc, min_size_ranges_database * 2, sizeof(struct range_u32), 0 );
  infos_al_create_prealloc( &new_manager->al_vacancies,
      userdata, custom_malloc, min_size_ranges_database, sizeof(struct range_u32), 0 );

  struct range_u32 * first_one = infos_al_get_elem(&new_manager->al_rental_info, 0);
  first_one->start = -7;
  first_one->stop = 7;
  assume( first_one->start > first_one->stop );
  first_one = infos_al_get_elem(&new_manager->al_vacancies, 0);
  first_one->start = 0;
  first_one->stop = (new_manager->data_len - 1) >> log2_chunk_len;

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

  if ( log2_chunk_len == 0 ) {
    constexpr size_t cacheline_len = CACHELINE_LEN;
    log2_chunk_len = get_log2(cacheline_len);
  }

  p_smm->data_len = pool_size >> log2_chunk_len;
  p_smm->total_space_left = p_smm->data_len;
  p_smm->idx_last_rental_info = 0;
  p_smm->idx_last_vacancy = 0;

  constexpr size_t min_size_range_db = CACHELINE_LEN * 4;
  constexpr size_t rental_info_node_size = min_size_range_db * 2;
  constexpr size_t available_ranges_node_size = min_size_range_db;
  assume( (rental_info_node_size + available_ranges_node_size) % CACHELINE_LEN == 0 );
  assume( pool_size > (rental_info_node_size + available_ranges_node_size) * 2 );

  // To just copy and paste I created one shared variable here.
  struct infos_arr_list * restrict al;

  al = &p_smm->al_rental_info;
  auto node_size = rental_info_node_size;
  // Copy
  al->log2_node_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_nr_elems = al->log2_node_size - al->log2_elem_size;
  al->list.head.data = p_smm->data;  // This line's not copied as-is!
  infos_take_care_of_linked_list_after_putting_head( &al->list );

  al = &p_smm->al_vacancies;
  node_size = available_ranges_node_size;
  // Paste
  al->log2_node_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_nr_elems = al->log2_node_size - al->log2_elem_size;
  al->list.head.data = p_smm->data + rental_info_node_size;  // This line's different!
  infos_take_care_of_linked_list_after_putting_head( &al->list );

  // Take a few chunks from the first tip for the array lists
  struct range_u32 * first_rental_info;
  first_rental_info = infos_al_get_elem(&p_smm->al_rental_info, 0);
  first_rental_info->start = 0;
  first_rental_info->stop =
    ( (rental_info_node_size + available_ranges_node_size) >> log2_chunk_len ) - 1;
  p_smm->total_space_left -=
    (rental_info_node_size + available_ranges_node_size) >> log2_chunk_len;

  // Update availability information accordingly.
  // The entire pool except 'rental_info_node_size + available_ranges_node_size'
  struct range_u32 * first_available_range;
  first_available_range = infos_al_get_elem(&p_smm->al_vacancies, 0);
  first_available_range->start =
    (rental_info_node_size + available_ranges_node_size) >> log2_chunk_len;
  first_available_range->stop = p_smm->data_len - 1;
}



//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_impl( struct infos_simple_mem_mgr * restrict p_mgr,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict userdata, uint32_t length, uint8_t take_from_left )
{
  uint64_t nr_elems;
  uint64_t base_vidx;

  if ( p_mgr->total_space_left < length ) {
    errno = ENOBUFS;
    return 0;
  }

  nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  struct infos_singly_linked_list_node * restrict v_node = &p_mgr->al_vacancies.list.head;
  base_vidx = 0;
  do {  // while ( v_node );
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }
      struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
      // Is it an invalid one?
      if ( vacancy->start > vacancy->stop ) {
        continue;
      }

      if ( get_range_len(*vacancy) >= length ) {
        // Register the rental_info
        nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict r_node;
        r_node = &p_mgr->al_rental_info.list.head;
        uint64_t base_ridx = 0;
        do {  // while ( r_node );
          for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
            struct range_u32 * restrict rental;
            rental = r_node->data + rent_idx * sizeof(*rental);
            if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
              p_mgr->idx_last_rental_info = base_ridx + rent_idx;
              goto take_and_return;
            }
            if ( rental->stop < rental->start ) {
              // Take the empty element; and return;
take_and_return:
              if ( take_from_left ) {
                rental->stop = vacancy->start + length - 1;
                rental->start = vacancy->start;
              } else {
                rental->start = vacancy->stop - length + 1;
                rental->stop = vacancy->stop;
              }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

              // Reduce the size of vacancy
              // From the right side? or the left side?
              if ( take_from_left )
                vacancy->start = vacancy->start + length;
              else
                vacancy->stop = vacancy->stop - length;

              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                  if ( p_mgr->idx_last_vacancy > 0 ) {
                    p_mgr->idx_last_vacancy--;
                  }
                }
              }

              p_mgr->total_space_left -= get_range_len(*rental);
              return rental->start;
            }
          }

          r_node = r_node->next;
          base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
        } while ( r_node );

        // Grow the infos_arr_list and register one
        uint_fast8_t err = infos_al_grow( &p_mgr->al_rental_info, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          if ( err == emsg_out_of_memory ) {
            errno = ENOMEM;
          }
          return 0;
        }
        r_node = infos_ll_get_last_node(&p_mgr->al_rental_info.list.head);
        p_mgr->idx_last_rental_info = base_ridx;
        struct range_u32 * restrict rental = r_node->data;
        if ( take_from_left ) {
          rental->stop = vacancy->start + length - 1;
          rental->start = vacancy->start;
        } else {
          rental->start = vacancy->stop - length + 1;
          rental->stop = vacancy->stop;
        }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

        // Reduce the size of vacancy
        // From the right side? or the left side?
        if ( take_from_left )
          vacancy->start = vacancy->start + length;
        else
          vacancy->stop = vacancy->stop - length;

        if ( get_range_len(*vacancy) == 0 ) {
          vacancy->start = -7;
          vacancy->stop = 7;
          if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
            if ( p_mgr->idx_last_vacancy > 0 ) {
              p_mgr->idx_last_vacancy--;
            }
          }
        }

        p_mgr->total_space_left -= get_range_len(*rental);
        return rental->start;
      }
    }

    v_node = v_node->next;
    base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  } while ( v_node );

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  return 0;
}



//-------------------------------------------------------------------------------------------------
void * infos_smm_rent( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    bool take_from_left )
{
  errno = 0;
  uint32_t ret = infos_smm_rent_impl( p_mgr, custom_malloc, userdata, length, take_from_left );
  if ( errno ) {
    return nullptr;
  }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent %lx + %u-%u\n", ptr2ui(p_mgr->data), ret, ret+length-1);
//#endif

  return p_mgr->data + ret;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_rent_chunks( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_chunks,
    bool take_from_left,
    uint_fast8_t log2_chunk_len)
{
  if ( p_mgr->total_space_left < nr_chunks ) {
    errno = ENOBUFS;
    return 0;
  }

  uint64_t nr_elems;
  uint64_t base_vidx;
  nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  struct infos_singly_linked_list_node * restrict v_node = &p_mgr->al_vacancies.list.head;
  base_vidx = 0;
  do {
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }
      struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
      // Is it an invalid one?
      if ( vacancy->start > vacancy->stop ) {
        continue;
      }

      if ( get_range_len(*vacancy) >= nr_chunks ) {
        // Register the rental_info
        nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict r_node;
        r_node = &p_mgr->al_rental_info.list.head;
        uint64_t base_ridx = 0;
        do {
          for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
            struct range_u32 * restrict rental;
            rental = r_node->data + rent_idx * sizeof(*rental);
            if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
              p_mgr->idx_last_rental_info = base_ridx + rent_idx;
              goto take_and_return;
            }
            if ( rental->stop < rental->start ) {
              // Take the empty element; and return;
take_and_return:
              if ( take_from_left ) {
                rental->stop = vacancy->start + nr_chunks - 1;
                rental->start = vacancy->start;
              } else {
                rental->start = vacancy->stop - nr_chunks + 1;
                rental->stop = vacancy->stop;
              }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_chunks_ %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

              // Reduce the size of vacancy
              // From the right side? or the left side?
              if ( take_from_left )
                vacancy->start = vacancy->start + nr_chunks;
              else
                vacancy->stop = vacancy->stop - nr_chunks;

              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                  if ( p_mgr->idx_last_vacancy > 0 ) {
                    p_mgr->idx_last_vacancy--;
                  }
                }
              }

              p_mgr->total_space_left -= get_range_len(*rental);
              return p_mgr->data + (rental->start << log2_chunk_len);
            }
          }

          r_node = r_node->next;
          base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
        } while ( r_node );

        // Grow the infos_arr_list and register one
        uint_fast8_t err = infos_al_grow( &p_mgr->al_rental_info, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          if ( err == emsg_out_of_memory ) {
            errno = ENOMEM;
          }
          return 0;
        }
        r_node = infos_ll_get_last_node(&p_mgr->al_rental_info.list.head);
        p_mgr->idx_last_rental_info = base_ridx;
        struct range_u32 * restrict rental = r_node->data;
        if ( take_from_left ) {
          rental->stop = vacancy->start + nr_chunks - 1;
          rental->start = vacancy->start;
        } else {
          rental->start = vacancy->stop - nr_chunks + 1;
          rental->stop = vacancy->stop;
        }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_chunks_ %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

        // Reduce the size of vacancy
        // From the right side? or the left side?
        if ( take_from_left )
          vacancy->start = vacancy->start + nr_chunks;
        else
          vacancy->stop = vacancy->stop - nr_chunks;

        if ( get_range_len(*vacancy) == 0 ) {
          vacancy->start = -7;
          vacancy->stop = 7;
          if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
            if ( p_mgr->idx_last_vacancy > 0 ) {
              p_mgr->idx_last_vacancy--;
            }
          }
        }

        p_mgr->total_space_left -= get_range_len(*rental);
        return p_mgr->data + (rental->start << log2_chunk_len);
      }
    }

    v_node = v_node->next;  // NOLINT(clang-analyzer-core.NullDereference)
    base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  } while ( v_node );

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  return 0;
}


constexpr size_t NR_TRIES_TO_FIND_A_PREALIGNED_SPACE = 3;

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_impl( struct infos_simple_mem_mgr * restrict p_mgr,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict userdata, uint32_t length, uint32_t alignment )
{
  assume( alignment > 1 );
  assume( _is_power_of_two(alignment) );
  //assume( _is_aligned_to_power_of_two(length, alignment) );

  if ( p_mgr->total_space_left < length ) {
    errno = ENOBUFS;
    return 0;
  }

  uint_fast8_t nr_tries_to_find_prealigned = 0;
  struct range_u32 * spacious_range;
  struct range_u32 * restrict vacancy;
  uint64_t nr_elems;
  uint64_t base_vidx;
  nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  struct infos_singly_linked_list_node * restrict v_node = &p_mgr->al_vacancies.list.head;
  base_vidx = 0;
  do {  // while ( v_node );
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {

      if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }
      vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
      // Is it an invalid one?
      if ( vacancy->start > vacancy->stop ) {
        continue;
      }

      uint32_t lsbs_head_addr = ptr2ui(p_mgr->data) & (alignment - 1);
      uint32_t nr_misaligned_bytes_at_head = (alignment - lsbs_head_addr) & (alignment - 1);
      uint32_t lsbs_tail_addr_when_aligned =
        (nr_misaligned_bytes_at_head + length - 1) & (alignment - 1);

      struct range_u32 new_vacancy;
      new_vacancy.start = -7;
      new_vacancy.stop = 7;
      if ( get_aligned_range_len(*vacancy, alignment, lsbs_head_addr)
          >= length ) {
        bool take_from_right = true;  // Let us take the right-most part in most of the cases.
        nr_tries_to_find_prealigned++;
        if ( _is_aligned_to_power_of_two(vacancy->start - lsbs_head_addr, alignment) ) {
          // It's pre-aligned in the head

          take_from_right = false;  // The only case where we take the front part.
          goto register_new_rental_info;

        } else if ( ((alignment - 1) & vacancy->stop)
            == lsbs_tail_addr_when_aligned ) {
          // It's pre-aligned in the tail

          goto register_new_rental_info;


        } else {
          // Try other vacancies if it's not prealigned
          if ( nr_tries_to_find_prealigned < NR_TRIES_TO_FIND_A_PREALIGNED_SPACE ) {
            spacious_range = vacancy;
            continue;
          }

        }

come_back_here_with_a_spacious_range:
        // Split vacancy in two pieces
        // NOTE It simply splits the range into 2 halves but there may be a better way.
        uint32_t nr_units_left_after_alloc = get_range_len(*vacancy) - length;
        uint32_t first_unit_idx = nr_units_left_after_alloc/2 - length/2;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
        first_unit_idx =
          _align_to_power_of_two(vacancy->start + first_unit_idx
              + lsbs_head_addr, alignment)
          - lsbs_head_addr;
#pragma GCC diagnostic pop

        while ( first_unit_idx + length - 1 > vacancy->stop ) {
          first_unit_idx -= alignment;
        }

        // Prepare to register a new vacancy
        new_vacancy.start = first_unit_idx + length;
        new_vacancy.stop = vacancy->stop;

        vacancy->stop = first_unit_idx - 1;


register_new_rental_info:
        // Register the rental_info
        nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict r_node;
        r_node = &p_mgr->al_rental_info.list.head;
        uint64_t base_ridx = 0;
        do {
          struct range_u32 * restrict rental;
          for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
            rental = r_node->data + rent_idx * sizeof(*rental);
            if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
              p_mgr->idx_last_rental_info = base_ridx + rent_idx;
              goto take_it_and_return;
            }
            if ( rental->stop < rental->start ) {
              // Take the empty element; and return;
take_it_and_return:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
              if ( !take_from_right ) {
                rental->stop = vacancy->start + length - 1;
                rental->start = vacancy->start;
              } else {
                rental->start = vacancy->stop - length + 1;
                rental->stop = vacancy->stop;
              }
#pragma GCC diagnostic pop
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif
              // Reduce the size of vacancy
              // From the right side? or the left side?
              if ( !take_from_right )
                vacancy->start = vacancy->start + length;
              else
                vacancy->stop = vacancy->stop - length;
              // See if there is a new vacancy to register
              if ( new_vacancy.start <= new_vacancy.stop ) {
                goto register_new_vacancy;
              }

              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                  if ( p_mgr->idx_last_vacancy > 0 ) {
                    p_mgr->idx_last_vacancy--;
                  }
                }
              }
              p_mgr->total_space_left -= get_range_len(*rental);
              return rental->start;
            }
          }

          r_node = r_node->next;
          base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
          if ( r_node && base_ridx >= p_mgr->idx_last_rental_info ) {
            rental = r_node->data;
            if ( base_ridx == p_mgr->idx_last_rental_info ) {
              rental += 1;
            }
            p_mgr->idx_last_rental_info++;

            // Take the next element; and return;
            if ( !take_from_right ) {
              rental->stop = vacancy->start + length - 1;
              rental->start = vacancy->start;
            } else {
              rental->start = vacancy->stop - length + 1;
              rental->stop = vacancy->stop;
            }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

            // Reduce the size of vacancy
            // From the right side? or the left side?
            if ( !take_from_right )
              vacancy->start = vacancy->start + length;
            else
              vacancy->stop = vacancy->stop - length;

            // See if there is a new vacancy to register
            if ( new_vacancy.start <= new_vacancy.stop ) {
                goto register_new_vacancy;
            }

            if ( get_range_len(*vacancy) == 0 ) {
              vacancy->start = -7;
              vacancy->stop = 7;
              if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                if ( p_mgr->idx_last_vacancy > 0 ) {
                  p_mgr->idx_last_vacancy--;
                }
              }
            }
            p_mgr->total_space_left -= get_range_len(*rental);
            return rental->start;
          }
        } while ( r_node );

        // Grow the infos_arr_list and register one
        uint_fast8_t err = infos_al_grow( &p_mgr->al_rental_info, userdata, custom_malloc,
            1, 0 );
        if ( err ) {
          switch ( err ) {
            case emsg_out_of_memory:
              errno = ENOMEM;
              break;
            default:
              unreachable();
          }
          return 0;
        }
        r_node = infos_ll_get_last_node(&p_mgr->al_rental_info.list.head);
        p_mgr->idx_last_rental_info++;
        struct range_u32 * restrict rental = r_node->data;
        if ( !take_from_right ) {
          rental->stop = vacancy->start + length - 1;
          rental->start = vacancy->start;
        } else {
          rental->start = vacancy->stop - length + 1;
          rental->stop = vacancy->stop;
        }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

        if ( new_vacancy.start > new_vacancy.stop ) {
          // Reduce the size of vacancy
          // From the right side? or the left side?
          if ( !take_from_right )
            vacancy->start = vacancy->start + length;
          else
            vacancy->stop = vacancy->stop - length;

          if ( get_range_len(*vacancy) == 0 ) {
            vacancy->start = -7;
            vacancy->stop = 7;
            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              if ( p_mgr->idx_last_vacancy > 0 ) {
                p_mgr->idx_last_vacancy--;
              }
            }
          }
          p_mgr->total_space_left -= get_range_len(*rental);
          return rental->start;
        }

register_new_vacancy:
        rental = r_node->data;
        // Register new_vacancy
        nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        r_node = &p_mgr->al_vacancies.list.head;
        struct range_u32 * restrict new_slot;
        base_vidx = 0;
        do {
          for ( uint_fast32_t il_vidx = 0; il_vidx < nr_elems; il_vidx++ ) {
            if ( base_vidx + il_vidx == p_mgr->idx_last_vacancy ) {
              if ( il_vidx + 1 == nr_elems ) {
                break;
              } else {
                il_vidx++;
                new_slot = r_node->data + il_vidx * sizeof(*new_slot);
                goto populate_new_vacancy;
              }
            }

            new_slot = r_node->data + il_vidx * sizeof(*new_slot);
            if ( new_slot->stop < new_slot->start ) {
//#ifdef DEBUG_MODE
//fprintf(stdout,
//    "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u (register_new_vacancy)\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

              new_slot->start = new_vacancy.start;
              new_slot->stop = new_vacancy.stop;
              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + il_vidx == p_mgr->idx_last_vacancy ) {
                  if ( p_mgr->idx_last_vacancy > 0 ) {
                    p_mgr->idx_last_vacancy--;
                  }
                }
              }
              p_mgr->total_space_left -= get_range_len(*rental);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
              return rental->start;
#pragma GCC diagnostic pop
            }
          }

          r_node = r_node->next;
          base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_elem_size;
          if ( r_node && base_vidx >= p_mgr->idx_last_vacancy ) {
            new_slot = r_node->data;
            if ( base_vidx == p_mgr->idx_last_vacancy ) {
              new_slot++;
            }
populate_new_vacancy:
//#ifdef DEBUG_MODE
//fprintf(stdout,
//    "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u (populate_new_vacancy)\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

            new_slot->start = new_vacancy.start;
            new_slot->stop = new_vacancy.stop;
            p_mgr->idx_last_vacancy++;
            if ( get_range_len(*vacancy) == 0 ) {
              vacancy->start = -7;
              vacancy->stop = 7;
              if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                if ( p_mgr->idx_last_vacancy > 0 ) {
                  p_mgr->idx_last_vacancy--;
                }
              }
            }
            p_mgr->total_space_left -= get_range_len(*rental);
            return rental->start;
          }
        } while ( r_node );

        // Grow the linked array
        err = infos_al_grow( &p_mgr->al_vacancies, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          switch ( err ) {
            case emsg_out_of_memory:
              errno = ENOMEM;
              break;
            default:
              unreachable();
          }
          // Revert the removal
          if ( !take_from_right )
            vacancy->start = vacancy->start - length;
          else
            vacancy->stop = vacancy->stop + length;

          return 0;
        }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u (Grow the linked array)\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

        r_node = infos_ll_get_last_node(&p_mgr->al_vacancies.list.head);
        p_mgr->idx_last_vacancy++;
        new_slot = r_node->data;
        new_slot->start = new_vacancy.start;
        new_slot->stop = new_vacancy.stop;

        if ( get_range_len(*vacancy) == 0 ) {
          vacancy->start = -7;
          vacancy->stop = 7;
          if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
            if ( p_mgr->idx_last_vacancy > 0 ) {
              p_mgr->idx_last_vacancy--;
            }
          }
        }
        p_mgr->total_space_left -= get_range_len(*rental);
        return rental->start;
      }
    }

    v_node = v_node->next;  // NOLINT(clang-analyzer-core.NullDereference)
    base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  } while ( v_node );

  if ( nr_tries_to_find_prealigned ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
    vacancy = spacious_range;
#pragma GCC diagnostic pop
    goto come_back_here_with_a_spacious_range;
  }

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  return 0;
}

//-------------------------------------------------------------------------------------------------
static uint32_t infos_smm_rent_aligned_chunks_impl(
    struct infos_simple_mem_mgr * restrict p_mgr,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict userdata, uint32_t nr_chunks, uint8_t log2_chunk_len, uint32_t alignment )
{
  assume( alignment > 1 );
  assume( _is_power_of_two(alignment) );

  if ( p_mgr->total_space_left < nr_chunks ) {
    errno = ENOBUFS;
    return 0;
  }

  uint64_t lsbs_head_addr = ptr2ui(p_mgr->data) & (alignment - 1);
  uint64_t alignment_in_chunks = alignment >> log2_chunk_len;
  uint64_t lsbs_headchunk_addr = lsbs_head_addr >> log2_chunk_len & (alignment_in_chunks - 1);

  uint_fast8_t nr_tries_to_find_prealigned = 0;
  struct range_u32 * spacious_range;
  struct range_u32 * restrict vacancy;
  uint64_t nr_elems;
  uint64_t base_vidx;
  nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  struct infos_singly_linked_list_node * restrict v_node = &p_mgr->al_vacancies.list.head;
  base_vidx = 0;
  do {  // while ( v_node );
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {

      if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        return 0;
      }
      vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
      // Is it an invalid one?
      if ( vacancy->start > vacancy->stop ) {
        continue;
      }

      uint32_t nr_misaligned_chunks_at_head =
        ( (alignment_in_chunks - lsbs_headchunk_addr) & (alignment_in_chunks - 1) );
      uint32_t lsbs_last_chunk_addr_when_aligned =
        ( nr_misaligned_chunks_at_head + (nr_chunks - 1) ) & ( alignment_in_chunks - 1 );

      struct range_u32 new_vacancy;
      new_vacancy.start = -7;
      new_vacancy.stop = 7;
      if ( get_aligned_range_len(*vacancy, alignment_in_chunks, lsbs_headchunk_addr)
          >= nr_chunks ) {
        bool take_from_right = true;  // Let us take the back part in most of the cases.
        nr_tries_to_find_prealigned++;
        if ( _is_aligned_to_power_of_two(
              vacancy->start - lsbs_headchunk_addr, alignment_in_chunks) ) {
          // It's pre-aligned in the head

          take_from_right = false;  // The only case where we take the front part.
          goto register_new_rental_info;

        } else if ( ((alignment_in_chunks - 1) & vacancy->stop)
            == lsbs_last_chunk_addr_when_aligned ) {
          // It's pre-aligned in the tail

          goto register_new_rental_info;


        } else {
          // Try more if it's not prealigned
          if ( nr_tries_to_find_prealigned < NR_TRIES_TO_FIND_A_PREALIGNED_SPACE ) {
            spacious_range = vacancy;
            continue;
          }

        }

come_back_here_with_a_spacious_range:
        // Split vacancy in two pieces
        // NOTE It simply splits the range into 2 halves but there might be a better way.
        uint32_t nr_units_left_after_alloc = get_range_len(*vacancy) - nr_chunks;
        uint32_t first_unit_idx = nr_units_left_after_alloc/2 - nr_chunks/2;
        first_unit_idx =
          _align_to_power_of_two( vacancy->start + first_unit_idx + lsbs_headchunk_addr,
              alignment_in_chunks )
          - lsbs_headchunk_addr;

        while ( first_unit_idx + nr_chunks - 1 > vacancy->stop ) {
          first_unit_idx -= alignment_in_chunks;
        }

        // Prepare to register a new vacancy
        new_vacancy.start = first_unit_idx + nr_chunks;
        new_vacancy.stop = vacancy->stop;

        vacancy->stop = first_unit_idx - 1;


register_new_rental_info:
        // Register the rental_info
        nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict r_node;
        r_node = &p_mgr->al_rental_info.list.head;
        uint64_t base_ridx = 0;
        do {
          struct range_u32 * restrict rental;
          for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
            rental = r_node->data + rent_idx * sizeof(*rental);
            if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
              p_mgr->idx_last_rental_info = base_ridx + rent_idx;
              goto take_it_and_return;
            }
            if ( rental->stop < rental->start ) {
              // Take the empty element; and return;
take_it_and_return:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
              if ( !take_from_right ) {
                rental->stop = vacancy->start + nr_chunks - 1;
                rental->start = vacancy->start;
              } else {
                rental->start = vacancy->stop - nr_chunks + 1;
                rental->stop = vacancy->stop;
              }
#pragma GCC diagnostic pop
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_chunks_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

              // Reduce the size of vacancy
              // From the right side? or the left side?
              if ( !take_from_right )
                vacancy->start = vacancy->start + nr_chunks;
              else
                vacancy->stop = vacancy->stop - nr_chunks;
              // See if there is a new vacancy to register
              if ( new_vacancy.start <= new_vacancy.stop ) {
                goto register_new_vacancy;
              }

              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                  if ( p_mgr->idx_last_vacancy > 0 ) {
                    p_mgr->idx_last_vacancy--;
                  }
                }
              }
              p_mgr->total_space_left -= get_range_len(*rental);
              return rental->start;
            }
          }

          r_node = r_node->next;
          base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
          if ( r_node && base_ridx >= p_mgr->idx_last_rental_info ) {
            rental = r_node->data;
            if ( base_ridx == p_mgr->idx_last_rental_info ) {
              rental += 1;
            }
            p_mgr->idx_last_rental_info++;

            // Take the next element; and return;
            if ( !take_from_right ) {
              rental->stop = vacancy->start + nr_chunks - 1;
              rental->start = vacancy->start;
            } else {
              rental->start = vacancy->stop - nr_chunks + 1;
              rental->stop = vacancy->stop;
            }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_chunks_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

            // Reduce the size of vacancy
            // From the right side? or the left side?
            if ( !take_from_right )
              vacancy->start = vacancy->start + nr_chunks;
            else
              vacancy->stop = vacancy->stop - nr_chunks;

            // See if there is a new vacancy to register
            if ( new_vacancy.start <= new_vacancy.stop ) {
                goto register_new_vacancy;
            }

            if ( get_range_len(*vacancy) == 0 ) {
              vacancy->start = -7;
              vacancy->stop = 7;
              if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                if ( p_mgr->idx_last_vacancy > 0 ) {
                  p_mgr->idx_last_vacancy--;
                }
              }
            }
            p_mgr->total_space_left -= get_range_len(*rental);
            return rental->start;
          }
        } while ( r_node );

        // Grow the infos_arr_list and register one
        uint_fast8_t err = infos_al_grow( &p_mgr->al_rental_info, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          switch ( err ) {
            case emsg_out_of_memory:
              errno = ENOMEM;
              break;
            default:
              unreachable();
          }
          return 0;
        }
        r_node = infos_ll_get_last_node(&p_mgr->al_rental_info.list.head);
        p_mgr->idx_last_rental_info++;
        struct range_u32 * restrict rental = r_node->data;
        if ( !take_from_right ) {
          rental->stop = vacancy->start + nr_chunks - 1;
          rental->start = vacancy->start;
        } else {
          rental->start = vacancy->stop - nr_chunks + 1;
          rental->stop = vacancy->stop;
        }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_chunks_impl %lx + %u-%u\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

        if ( new_vacancy.start > new_vacancy.stop ) {
          // Reduce the size of vacancy
          // From the right side? or the left side?
          if ( !take_from_right )
            vacancy->start = vacancy->start + nr_chunks;
          else
            vacancy->stop = vacancy->stop - nr_chunks;

          if ( get_range_len(*vacancy) == 0 ) {
            vacancy->start = -7;
            vacancy->stop = 7;
            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              if ( p_mgr->idx_last_vacancy > 0 ) {
                p_mgr->idx_last_vacancy--;
              }
            }
          }
          p_mgr->total_space_left -= get_range_len(*rental);
          return rental->start;
        }

register_new_vacancy:
        rental = r_node->data;
        // Register new_vacancy
        nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict il_v_node;
        il_v_node = &p_mgr->al_vacancies.list.head;
        struct range_u32 * restrict new_slot;
        base_vidx = 0;
        do {
          for ( uint_fast32_t il_vidx = 0; il_vidx < nr_elems; il_vidx++ ) {
            if ( base_vidx + il_vidx == p_mgr->idx_last_vacancy ) {
              if ( il_vidx + 1 == nr_elems ) {
                break;
              } else {
                il_vidx++;
                new_slot = il_v_node->data + il_vidx * sizeof(*new_slot);
                goto populate_new_vacancy;
              }
            }

            new_slot = il_v_node->data + il_vidx * sizeof(*new_slot);
            if ( new_slot->stop < new_slot->start ) {
//#ifdef DEBUG_MODE
//fprintf(stdout,
//    "[SMM] infos_smm_rent_aligned_chunks_impl %lx + %u-%u (register_new_vacancy)\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop);
//#endif

              new_slot->start = new_vacancy.start;
              new_slot->stop = new_vacancy.stop;
              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + il_vidx == p_mgr->idx_last_vacancy ) {
                  if ( p_mgr->idx_last_vacancy > 0 ) {
                    p_mgr->idx_last_vacancy--;
                  }
                }
              }
              p_mgr->total_space_left -= get_range_len(*rental);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
              return rental->start;
#pragma GCC diagnostic pop
            }
          }

          il_v_node = il_v_node->next;
          base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
          if ( il_v_node && base_vidx >= p_mgr->idx_last_vacancy ) {
            new_slot = il_v_node->data;
            if ( base_vidx == p_mgr->idx_last_vacancy ) {
              new_slot++;
            }
populate_new_vacancy:
//#ifdef DEBUG_MODE
//fprintf( stdout,
//    "[SMM] infos_smm_rent_aligned_chunks_impl %lx + %u-%u (populate_new_vacancy)\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop );
//#endif

            new_slot->start = new_vacancy.start;
            new_slot->stop = new_vacancy.stop;
            p_mgr->idx_last_vacancy++;
            if ( get_range_len(*vacancy) == 0 ) {
              vacancy->start = -7;
              vacancy->stop = 7;
              if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                if ( p_mgr->idx_last_vacancy > 0 ) {
                  p_mgr->idx_last_vacancy--;
                }
              }
            }
            p_mgr->total_space_left -= get_range_len(*rental);
            return rental->start;
          }
        } while ( il_v_node );

        // Grow the linked array
        err = infos_al_grow( &p_mgr->al_vacancies, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          switch ( err ) {
            case emsg_out_of_memory:
              errno = ENOMEM;
              break;
            default:
              unreachable();
          }
          // Revert the removal
          if ( !take_from_right )
            vacancy->start = vacancy->start - nr_chunks;
          else
            vacancy->stop = vacancy->stop + nr_chunks;

          return 0;
        }
//#ifdef DEBUG_MODE
//fprintf( stdout,
//    "[SMM] infos_smm_rent_aligned_chunks_impl %lx + %u-%u (Grow the linked array)\n",
//    ptr2ui(p_mgr->data), rental->start, rental->stop );
//#endif

        il_v_node = infos_ll_get_last_node(&p_mgr->al_vacancies.list.head);
        p_mgr->idx_last_vacancy++;
        new_slot = il_v_node->data;
        new_slot->start = new_vacancy.start;
        new_slot->stop = new_vacancy.stop;

        if ( get_range_len(*vacancy) == 0 ) {
          vacancy->start = -7;
          vacancy->stop = 7;
          if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
            if ( p_mgr->idx_last_vacancy > 0 ) {
              p_mgr->idx_last_vacancy--;
            }
          }
        }
        p_mgr->total_space_left -= get_range_len(*rental);
        return rental->start;
      }
    }

    v_node = v_node->next;  // NOLINT(clang-analyzer-core.NullDereference)
    base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  } while ( v_node );

  if ( nr_tries_to_find_prealigned ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
    vacancy = spacious_range;
#pragma GCC diagnostic pop
    goto come_back_here_with_a_spacious_range;
  }

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  return 0;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_rent_aligned( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    uint32_t alignment )
{
  errno = 0;
  uint32_t ret = infos_smm_rent_aligned_impl(p_mgr, custom_malloc, userdata,
      length, alignment);
  if ( errno ) {
    return nullptr;
  }

  return p_mgr->data + ret;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_rent_aligned_chunks( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_chunks,
    uint32_t alignment,
    uint_fast8_t log2_chunk_len )
{
  errno = 0;
  uint32_t ret = infos_smm_rent_aligned_chunks_impl( p_mgr, custom_malloc, userdata,
      nr_chunks, log2_chunk_len, alignment );
  if ( errno ) {
    return nullptr;
  }

  return p_mgr->data + (ret << log2_chunk_len);
}



//-------------------------------------------------------------------------------------------------
static enum err_msg infos_smm_return_impl( struct infos_simple_mem_mgr * restrict p_mgr,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * userdata, uint32_t * nr_units_released, uint32_t target )
{
  struct infos_singly_linked_list_node * restrict r_node = &p_mgr->al_rental_info.list.head;
  uint64_t nr_elems;
  nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
  uint64_t base_ridx = 0;
  do {  // while ( r_node );

    for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
      struct range_u32 * restrict rental_info;
      rental_info = r_node->data + rent_idx * sizeof(*rental_info);
      if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
        if ( nr_units_released ) {
          *nr_units_released = 0;
        }
        return emsg_no_such_entry;
      }

      // Skip if it's empty.
      if ( rental_info->start > rental_info->stop ) {
        continue;
      }

      // We found a match?
      if ( rental_info->start == target ) {
        // Make a vacancy and merge if possible
        nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict v_node;
        v_node = &p_mgr->al_vacancies.list.head;
        uint64_t base_vidx = 0;
        struct range_u32 * restrict vacancy_to_merge_with = nullptr;
        do {
          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
              goto no_existing_range_to_merge;
            }

            struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);

            // Skip if it's empty.
            if ( vacancy->start > vacancy->stop ) {
              continue;
            }

            // Some messy code from a long time ago
            if ( !vacancy_to_merge_with ) {
              if ( vacancy->stop == target - 1 ) {
                merge_range_into_lower(vacancy, rental_info);
                vacancy_to_merge_with = vacancy;
              } else if ( vacancy->start == rental_info->stop + 1 ) {
                merge_range_into_higher(rental_info, vacancy);
                vacancy_to_merge_with = vacancy;
              }
            } else {
              if ( vacancy->stop == vacancy_to_merge_with->start - 1 ) {
                merge_range_into_lower(vacancy, vacancy_to_merge_with);
                vacancy_to_merge_with = 0;
              } else if ( vacancy->start == vacancy_to_merge_with->stop + 1 ) {
                merge_range_into_higher(vacancy_to_merge_with, vacancy);
                vacancy_to_merge_with = 0;
              }

              if ( vacancy_to_merge_with == 0 ) {
mark_empty_and_return:
                if ( nr_units_released ) {
                  *nr_units_released = rental_info->stop - rental_info->start + 1;
                }
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_return_impl released %u-%u\n",
//    rental_info->start, rental_info->stop );
//#endif
                p_mgr->total_space_left += get_range_len(*rental_info);

                // Mark the element empty.
                rental_info->start = -7;
                rental_info->stop = 7;
                if ( base_ridx + rent_idx == p_mgr->idx_last_rental_info &&
                    p_mgr->idx_last_rental_info > 0 ) {
                  p_mgr->idx_last_rental_info--;
                }
                return 0;
              }
            }

            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              goto mark_empty_and_return;
            }
          }
          v_node = v_node->next;
          base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        } while ( v_node );

no_existing_range_to_merge:
        // Couldn't merge into any existing vacancy

        // Register a new one
        nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        v_node = &p_mgr->al_vacancies.list.head;
        struct range_u32 * restrict vacancy;
        base_vidx = 0;
        do {
          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              if ( vacancy_idx + 1 == nr_elems ) {
                break;
              } else {
                vacancy_idx++;
                vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
                goto populate_new_vacancy;
              }
            }

            vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
            if ( vacancy->stop < vacancy->start ) {
              vacancy->start = rental_info->start;
              vacancy->stop = rental_info->stop;
              if ( nr_units_released ) {
                *nr_units_released = rental_info->stop - rental_info->start + 1;
              }
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_return_impl released %u-%u\n",
//    rental_info->start, rental_info->stop );
//#endif
              p_mgr->total_space_left += get_range_len(*rental_info);

              // Mark the element empty.
              rental_info->start = -7;
              rental_info->stop = 7;
              if ( base_ridx + rent_idx == p_mgr->idx_last_rental_info &&
                  p_mgr->idx_last_rental_info > 0 ) {
                p_mgr->idx_last_rental_info--;
              }
              return 0;
            }
          }

          v_node = v_node->next;
          base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
          if ( v_node && base_vidx >= p_mgr->idx_last_vacancy ) {
            vacancy = v_node->data;
            if ( base_vidx == p_mgr->idx_last_vacancy ) {
              vacancy++;
            }
populate_new_vacancy:
            vacancy->start = rental_info->start;
            vacancy->stop = rental_info->stop;
            p_mgr->idx_last_vacancy++;
            if ( nr_units_released ) {
              *nr_units_released = rental_info->stop - rental_info->start + 1;
            }
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_return_impl released %u-%u\n",
//    rental_info->start, rental_info->stop );
//#endif
            p_mgr->total_space_left += get_range_len(*rental_info);

            // Mark the element empty.
            rental_info->start = -7;
            rental_info->stop = 7;
            if ( base_ridx + rent_idx == p_mgr->idx_last_rental_info &&
                p_mgr->idx_last_rental_info > 0 ) {
              p_mgr->idx_last_rental_info--;
            }
            return 0;
          }
        } while ( v_node );

        // Grow the linked array
        uint_fast8_t err = infos_al_grow( &p_mgr->al_vacancies, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          if ( nr_units_released ) {
            *nr_units_released = 0;
          }
          return emsg_out_of_memory;
        }
        v_node = infos_ll_get_last_node(&p_mgr->al_vacancies.list.head);
        p_mgr->idx_last_vacancy++;
        vacancy = v_node->data;
        vacancy->start = rental_info->start;
        vacancy->stop = rental_info->stop;

        if ( nr_units_released ) {
          *nr_units_released = rental_info->stop - rental_info->start + 1;
        }
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_return_impl released %u-%u\n",
//    rental_info->start, rental_info->stop );
//#endif
        p_mgr->total_space_left += get_range_len(*rental_info);

        // Mark the element empty.
        rental_info->start = -7;
        rental_info->stop = 7;
        if ( base_ridx + rent_idx == p_mgr->idx_last_rental_info &&
            p_mgr->idx_last_rental_info > 0 ) {
          p_mgr->idx_last_rental_info--;
        }
        return 0;
      }

      // Did not find a match, check the next array
    }
    r_node = r_node->next;
    base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;

  } while ( r_node );

  if ( nr_units_released ) {
    *nr_units_released = 0;
  }
  return emsg_no_such_entry;
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_return( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr )
{
  uint32_t target = ptr - p_mgr->data;
  return infos_smm_return_impl(p_mgr, custom_malloc, userdata, 0, target);
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_return_chunks( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr,
    uint32_t * restrict nr_chunks_released,
    uint32_t log2_chunk_len )
{
  uint32_t target = (ptr - p_mgr->data) >> log2_chunk_len;
  return infos_smm_return_impl(p_mgr, custom_malloc, userdata, nr_chunks_released, target);
}

//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_return_chunks_ofs( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t offset,
    uint32_t * restrict nr_chunks_released,
    uint32_t log2_chunk_len )
{
  uint32_t target = offset >> log2_chunk_len;
  return infos_smm_return_impl(p_mgr, custom_malloc, userdata, nr_chunks_released, target);
}



//-------------------------------------------------------------------------------------------------
void * infos_smm_realloc_chunks( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length,
    uint32_t log2_chunk_len )
{
  if ( new_length == 0 ) {
    return 0;
  }

  struct infos_singly_linked_list_node * restrict r_node = &p_mgr->al_rental_info.list.head;
  uint64_t nr_elems;
  nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
  uint64_t base_ridx = 0;
  uint64_t target = (orig_ptr - p_mgr->data) >> log2_chunk_len;
  do {  // while ( r_node );
    for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
      struct range_u32 * restrict rental_info;
      rental_info = r_node->data + rent_idx * sizeof(*rental_info);
      if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
        // Couldn't find it
        errno = EBADSLT;
        return 0;
      }

      // Skip if it's empty.
      if ( rental_info->start > rental_info->stop ) {
        continue;
      }
      if ( rental_info->start == target ) {
        //int32_t len_diff = new_length - (_length_of_range(rental_info) << log2_chunk_len);
        uint32_t new_len_in_chunks = ( new_length + (UINT64_C(1) << log2_chunk_len) - 1 )
          >> log2_chunk_len;
        int32_t len_diff_in_chunks = new_len_in_chunks - get_range_len(*rental_info);
        if ( len_diff_in_chunks == 0 ) {
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc_chunks %lx(%lu) -> %lx(%u) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(orig_ptr),
//    rental_info->start,
//    0,
//    new_len_in_chunks );
//#endif
          p_mgr->total_space_left -= 0;
          return orig_ptr;
        }

        nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict v_node;
        v_node = &p_mgr->al_vacancies.list.head;
        uint64_t base_vidx = 0;
        // Attempt to take vacancies right next to it
        struct range_u32 * vacancy_front = nullptr;
        struct range_u32 * vacancy_back = nullptr;
        struct range_u32 * vacancy_at_tip = (void*)-1;
        uint_fast8_t nr_neighbors_found = 0;
        do {
          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
              goto end_of_neighbor_search;
            }
            struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              vacancy_at_tip = vacancy;
            }

            // Skip if it's empty.
            if ( vacancy->start > vacancy->stop ) {
              continue;
            }

            if ( vacancy->stop == target - 1 ) {
              //available_space_front = rental_info->start - vacancy->start;
              vacancy_front = vacancy;
              nr_neighbors_found++;
            } else if ( vacancy->start == rental_info->stop + 1 ) {
              //available_space_back = vacancy->start - rental_info->start;
              vacancy_back = vacancy;
              nr_neighbors_found++;
            }

            if ( nr_neighbors_found >= 2 ) {
              goto end_of_neighbor_search;
            }
          }
          v_node = v_node->next;
          base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        } while ( v_node );

end_of_neighbor_search:
        // Try to edit vacancy_back
        if ( vacancy_back ) {
          if ( len_diff_in_chunks > 0 ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if ( get_range_len( *vacancy_back ) < len_diff_in_chunks ) {
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the back
            int whole_merge = get_range_len(*vacancy_back) == len_diff_in_chunks ?
              -7 : 0;
            rental_info->stop += len_diff_in_chunks;
            if ( !whole_merge ) {
              vacancy_back->start += len_diff_in_chunks;
            } else {  // whole_merge
              vacancy_back->start = -7;
              vacancy_back->stop = 7;
              if ( vacancy_back == vacancy_at_tip && p_mgr->idx_last_vacancy > 0 ) {
                p_mgr->idx_last_vacancy--;
              }
            }
            goto return_new_alloc;
          } else {  // len_diff_in_chunks < 0
            // Make space for the range in the back
            rental_info->stop += len_diff_in_chunks;
            vacancy_back->start += len_diff_in_chunks;
            goto return_new_alloc;
          }
        }
        // Try to edit vacancy_front
        if ( vacancy_front ) {
          if ( len_diff_in_chunks > 0 ) {
            if ( get_range_len( *vacancy_front ) < len_diff_in_chunks ) {
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the front
            int whole_merge = get_range_len(*vacancy_front) == len_diff_in_chunks ?
              -7 : 0;
            rental_info->start -= len_diff_in_chunks;
            if ( !whole_merge ) {
              vacancy_front->stop -= len_diff_in_chunks;
            } else {  // whole_merge
              vacancy_front->start = -7;
              vacancy_front->stop = 7;
              if ( vacancy_front == vacancy_at_tip && p_mgr->idx_last_vacancy > 0 ) {
                p_mgr->idx_last_vacancy--;
              }
            }
            goto return_new_alloc;
          } else {  // len_diff_in_chunks < 0
            // Make space for the range in the front
            rental_info->stop += len_diff_in_chunks;
            vacancy_front->start += len_diff_in_chunks;
            goto return_new_alloc;
          }
        }

        // Try to edit both vacancy_front and vacancy_back
        if ( vacancy_back && vacancy_front ) {
          if ( len_diff_in_chunks > 0 &&
              (get_range_len(*vacancy_front) + get_range_len(*vacancy_back)
              < len_diff_in_chunks) ) {
#pragma GCC diagnostic pop
            goto no_surrounding_space_big_enough_to_take;
          }
          // Take space from the front first
          uint32_t added_len = get_range_len(*vacancy_front);
          rental_info->start -= added_len;
          vacancy_front->start = -7;
          vacancy_front->stop = 7;
          if ( vacancy_front == vacancy_at_tip && p_mgr->idx_last_vacancy > 0 ) {
            p_mgr->idx_last_vacancy--;
          }

          // Take space from the back
          uint32_t deficit = len_diff_in_chunks - added_len;
          rental_info->stop += deficit;
          vacancy_back->start += deficit;
          if ( get_range_len(*vacancy_back) == 0 ) {
            vacancy_back->start = -7;
            vacancy_back->stop = 7;
            if ( vacancy_back == vacancy_at_tip && p_mgr->idx_last_vacancy > 0 ) {
              p_mgr->idx_last_vacancy--;
            }
          }
return_new_alloc:
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc_chunks %lx(%lu) -> %lx(%u) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(p_mgr->data) + (rental_info->start << log2_chunk_len),
//    rental_info->start,
//    len_diff_in_chunks,
//    new_len_in_chunks );
//#endif
          p_mgr->total_space_left -= len_diff_in_chunks;
          return p_mgr->data + (rental_info->start << log2_chunk_len);
        }


no_surrounding_space_big_enough_to_take:
        void * new_ptr = infos_smm_rent_chunks( p_mgr, userdata, custom_malloc,
            new_len_in_chunks, -7, log2_chunk_len );
        if ( !new_ptr ) {
          errno = ENOBUFS;
          return 0;
        }

        memcpy( new_ptr, orig_ptr, get_range_len(*rental_info) > new_len_in_chunks ?
            new_len_in_chunks << log2_chunk_len : get_range_len(*rental_info) << log2_chunk_len );
        if ( base_ridx + rent_idx == p_mgr->idx_last_rental_info  &&
            p_mgr->idx_last_rental_info > 0) {
          p_mgr->idx_last_rental_info--;
        }

//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc_chunks %lx(%lu) -> %lx(%lu) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(new_ptr),
//    ptr2ui(new_ptr - p_mgr->data) >> log2_chunk_len,
//    len_diff_in_chunks,
//    new_len_in_chunks );
//#endif


        enum err_msg err = infos_smm_return_chunks( p_mgr,
            userdata, custom_malloc, orig_ptr, NULL, log2_chunk_len );
        assume( err == emsg_no_error );
        if ( err ) {
          fprintf( stderr, "[SMM] infos_smm_realloc_chunks failed. (%s)\n", arr_emsg_txt[err] );
        }
        p_mgr->total_space_left -= len_diff_in_chunks;
        return new_ptr;
      }
    }

    r_node = r_node->next;
    base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
  } while ( r_node );

  // Couldn't find the rental information about ptr.
  errno = EBADSLT;
  return 0;
}

static void * try_to_merge_with_vacancy_right_next_for_an_aligned_alloc(
    struct infos_simple_mem_mgr * restrict p_mgr,
    struct range_u32 * vacancy_back,
    struct range_u32 * vacancy_front,
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
  struct range_u32 * the_vacancy;
  new_vacancy.start = -7;
  new_vacancy.stop = 7;
  assume( new_vacancy.start > new_vacancy.stop );
  // Try to edit vacancy_back
  if ( vacancy_back && !has_tried_back_merge ) {
    merged_range.start = orig_rent->start;
    merged_range.stop = vacancy_back->stop;
    the_vacancy = vacancy_back;

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

        if ( the_vacancy ) {
          // Prepare to register a new vacancy
          new_vacancy.start = first_unit_idx + new_len_in_chunks;
          new_vacancy.stop = merged_range.stop;
        } else {
          vacancy_back->start = first_unit_idx + new_len_in_chunks;
          vacancy_back->stop = merged_range.stop;
        }

        merged_range.stop = first_unit_idx - 1;

skip_the_divider:
        if ( !take_from_right ) {
          orig_rent->stop = merged_range.start + new_len_in_chunks - 1;
          orig_rent->start = merged_range.start;
        } else {
          orig_rent->start = merged_range.stop - new_len_in_chunks + 1;
          orig_rent->stop = merged_range.stop;;
        }

        // Reduce the size of vacancy
        if ( !take_from_right ) {
          merged_range.start = merged_range.start + new_len_in_chunks;
        } else {
          merged_range.stop = merged_range.stop - new_len_in_chunks;
        }

        if ( the_vacancy ) {
          the_vacancy->start = merged_range.start;
          the_vacancy->stop = merged_range.stop;
        } else {
          vacancy_front->start = merged_range.start;
          vacancy_front->stop = merged_range.stop;
        }

        // See if there is a new vacancy to register
        if ( new_vacancy.start <= new_vacancy.stop ) {
          // Register new_vacancy
          uint64_t nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
          struct infos_singly_linked_list_node * restrict il_v_node;
          il_v_node = &p_mgr->al_vacancies.list.head;
          struct range_u32 * restrict new_slot;
          uint64_t base_vidx = 0;
          do {
            for ( uint_fast32_t il_vidx = 0; il_vidx < nr_elems; il_vidx++ ) {
              if ( base_vidx + il_vidx == p_mgr->idx_last_vacancy ) {
                if ( il_vidx + 1 == nr_elems ) {
                  break;
                } else {
                  il_vidx++;
                  new_slot = il_v_node->data + il_vidx * sizeof(*new_slot);
                  goto populate_new_vacancy;
                }
              }

              new_slot = il_v_node->data + il_vidx * sizeof(*new_slot);
              if ( new_slot->stop < new_slot->start ) {
                new_slot->start = new_vacancy.start;
                new_slot->stop = new_vacancy.stop;
                if ( get_range_len(*the_vacancy) == 0 ) {
                  the_vacancy->start = -7;
                  the_vacancy->stop = 7;
                  if ( base_vidx + il_vidx == p_mgr->idx_last_vacancy ) {
                    if ( p_mgr->idx_last_vacancy > 0 ) {
                      p_mgr->idx_last_vacancy--;
                    }
                  }
                }
                p_mgr->total_space_left -= len_diff_in_chunks;
                return p_mgr->data + (orig_rent->start << log2_chunk_len);
              }
            }

            il_v_node = il_v_node->next;
            base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
            if ( il_v_node && base_vidx >= p_mgr->idx_last_vacancy ) {
              new_slot = il_v_node->data;
              if ( base_vidx == p_mgr->idx_last_vacancy ) {
                new_slot++;
              }
populate_new_vacancy:
              new_slot->start = new_vacancy.start;
              new_slot->stop = new_vacancy.stop;
              p_mgr->idx_last_vacancy++;
              if ( get_range_len(*the_vacancy) == 0 ) {
                the_vacancy->start = -7;
                the_vacancy->stop = 7;
                const uint64_t vacancy_idx = ( (void*)the_vacancy - il_v_node->data ) /
                  sizeof(*the_vacancy);
                if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                  if ( p_mgr->idx_last_vacancy > 0 ) {
                    p_mgr->idx_last_vacancy--;
                  }
                }
              }
              p_mgr->total_space_left -= len_diff_in_chunks;
              return p_mgr->data + (orig_rent->start << log2_chunk_len);
            }
          } while ( il_v_node );
        }  // if ( new_vacancy.start <= new_vacancy.stop ) 

        if ( the_vacancy ) {
          if ( get_range_len(*the_vacancy) == 0 ) {
            the_vacancy->start = -7;
            the_vacancy->stop = 7;

            struct infos_singly_linked_list_node * restrict v_node;
            v_node = &p_mgr->al_vacancies.list.head;
            const uint64_t nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
            uint64_t base_vidx = 0;
            while ( ptr2ui((void*)the_vacancy - v_node->data) >=
                sizeof(*the_vacancy) << p_mgr->al_vacancies.log2_nr_elems ) {
              v_node = v_node->next;
              base_vidx += nr_elems;;
            }
            const uint64_t vacancy_idx = ( (void*)the_vacancy - v_node->data ) /
              sizeof(*the_vacancy);
            
            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              if ( p_mgr->idx_last_vacancy > 0 ) {
                p_mgr->idx_last_vacancy--;
              }
            }
          }
        } else {
          struct range_u32 * vacancies_near[2] = { vacancy_back, vacancy_front };
          for ( int ptr_idx = 0; ptr_idx < 2; ptr_idx++ ) {
            typeof(vacancies_near[0]) il_vacancy = vacancies_near[ptr_idx];
            if ( get_range_len(*il_vacancy) == 0 ) {
              il_vacancy->start = -7;
              il_vacancy->stop = 7;

              struct infos_singly_linked_list_node * restrict v_node;
              v_node = &p_mgr->al_vacancies.list.head;
              const uint64_t nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
              uint64_t base_vidx = 0;
              while ( ptr2ui((void*)il_vacancy - v_node->data) >=
                  sizeof(*il_vacancy) << p_mgr->al_vacancies.log2_nr_elems ) {
                v_node = v_node->next;
                base_vidx += nr_elems;;
              }
              const uint64_t vacancy_idx = ( (void*)il_vacancy - v_node->data ) /
                sizeof(*il_vacancy);
              
              if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
                if ( p_mgr->idx_last_vacancy > 0 ) {
                  p_mgr->idx_last_vacancy--;
                }
              }
            }
          }
        }

        p_mgr->total_space_left -= len_diff_in_chunks;
        return p_mgr->data + (orig_rent->start << log2_chunk_len);
      }
    }  // if ( get_aligned_range_len(&merged_range, alignment_in_chunks, lsbs_headchunk_addr)
       //     >= new_len_in_chunks ) 
  }


  // Try to edit vacancy_front
  if ( vacancy_front && !has_tried_front_merge ) {
    merged_range.start = vacancy_front->start;
    merged_range.stop = orig_rent->stop;
    the_vacancy = vacancy_front;

    if ( get_aligned_range_len(merged_range, alignment_in_chunks, lsbs_headchunk_addr)
        >= new_len_in_chunks ) {
      if ( prealigned_tail ) {
        goto skip_the_divider;
      } else {
        goto dont_skip_the_divider;
      }
    }
  }


  // Try to edit both vacancy_front and vacancy_back
  if ( vacancy_back && vacancy_front ) {
    merged_range.start = vacancy_front->start;
    merged_range.stop = vacancy_back->stop;
    the_vacancy = nullptr;

    if ( get_aligned_range_len(merged_range, alignment_in_chunks, lsbs_headchunk_addr)
        >= new_len_in_chunks ) {
      goto dont_skip_the_divider;
    }
  }

  return nullptr;
}
//-------------------------------------------------------------------------------------------------
void * infos_smm_realloc_aligned_chunks( struct infos_simple_mem_mgr * restrict p_mgr,
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
  assume( _is_power_of_two(alignment) );
  alignment = _align_to_power_of_two( alignment, 1 << log2_chunk_len );
  uint64_t lsbs_head_addr = ptr2ui(p_mgr->data) & (alignment - 1);
  uint64_t alignment_in_chunks = alignment >> log2_chunk_len;
  assume( alignment_in_chunks >= 1 );
  uint64_t lsbs_headchunk_addr = lsbs_head_addr >> log2_chunk_len & (alignment_in_chunks - 1);

  struct infos_singly_linked_list_node * restrict r_node = &p_mgr->al_rental_info.list.head;
  uint64_t nr_elems;
  nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
  uint64_t base_ridx = 0;
  uint64_t target = (orig_ptr - p_mgr->data) >> log2_chunk_len;
  do {  // while ( r_node );
    for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
      struct range_u32 * restrict rental_info;
      rental_info = r_node->data + rent_idx * sizeof(*rental_info);
      if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
        // Couldn't find it
        errno = EBADSLT;
        return 0;
      }

      // Skip if it's empty.
      if ( rental_info->start > rental_info->stop ) {
        continue;
      }
      if ( rental_info->start == target ) {
        //int32_t len_diff = new_length - (_length_of_range(rental_info) << log2_chunk_len);
        uint32_t new_len_in_chunks = ( new_length + (UINT64_C(1) << log2_chunk_len) - 1 )
          >> log2_chunk_len;
        int32_t len_diff_in_chunks = new_len_in_chunks - get_range_len(*rental_info);
        uint32_t nr_misaligned_chunks_at_head =
          ( (alignment_in_chunks - lsbs_headchunk_addr) & (alignment_in_chunks - 1) );
        uint32_t lsbs_last_chunk_addr_when_aligned =
          ( nr_misaligned_chunks_at_head + (new_len_in_chunks - 1) ) & ( alignment_in_chunks - 1 );

        // It's pre-aligned in the head
        bool prealigned_head = _is_aligned_to_power_of_two(
            rental_info->start - lsbs_headchunk_addr, alignment_in_chunks);
        bool prealigned_tail = ((alignment_in_chunks - 1) & rental_info->stop) ==
              lsbs_last_chunk_addr_when_aligned;

        //if ( len_diff_in_chunks <= 0 ) 
        if ( get_aligned_range_len(*rental_info, alignment_in_chunks, lsbs_headchunk_addr) >=
            new_len_in_chunks ) {
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc_chunks %lx(%lu) -> %lx(%u) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(orig_ptr),
//    rental_info->start,
//    0,
//    new_len_in_chunks );
//#endif
          if ( prealigned_head ) {
            if ( len_diff_in_chunks == 0 ) {
              return orig_ptr;
            }
            // Try to find a vacancy right behind the original rent and merge
          }
          //else if ( prealigned_tail ) {
          //  // Try to find a vacancy right in front of the original rent and merge
          //}
        }

        nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict v_node;
        v_node = &p_mgr->al_vacancies.list.head;
        uint64_t base_vidx = 0;
        // Attempt to take vacancies right next to it
        struct range_u32 * vacancy_front = nullptr;
        struct range_u32 * vacancy_back = nullptr;
        //struct range_u32 * vacancy_at_tip = (void*)~UINT64_C(0);
        uint_fast8_t nr_neighbors_found = 0;
        do {
          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
              goto end_of_neighbor_search;
            }
            struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              //vacancy_at_tip = vacancy;
            }

            // Skip if it's empty.
            if ( vacancy->start > vacancy->stop ) {
              continue;
            }

            if ( vacancy->stop == target - 1 ) {
              //available_space_front = rental_info->start - vacancy->start;
              vacancy_front = vacancy;
              nr_neighbors_found++;
            } else if ( vacancy->start == rental_info->stop + 1 ) {
              //available_space_back = vacancy->start - rental_info->start;
              vacancy_back = vacancy;
              nr_neighbors_found++;
            }

            if ( nr_neighbors_found >= 2 ) {
              goto end_of_neighbor_search;
            }
          }
          v_node = v_node->next;
          base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        } while ( v_node );

end_of_neighbor_search:

        bool has_tried_front_merge = false;
        bool has_tried_back_merge = false;
        if ( prealigned_head && vacancy_back ) {
          void * ret = try_to_merge_with_vacancy_right_next_for_an_aligned_alloc( p_mgr,
              vacancy_back, nullptr, rental_info,
              alignment_in_chunks, lsbs_headchunk_addr,
              has_tried_back_merge, has_tried_front_merge, len_diff_in_chunks, log2_chunk_len );
          if ( ret ) {
            memmove( ret, orig_ptr, new_len_in_chunks << log2_chunk_len );
            return ret;
          }
          has_tried_back_merge = true;
        }
        if ( prealigned_tail && vacancy_front ) {
          void * ret = try_to_merge_with_vacancy_right_next_for_an_aligned_alloc( p_mgr,
              nullptr, vacancy_front, rental_info,
              alignment_in_chunks, lsbs_headchunk_addr,
              has_tried_back_merge, has_tried_front_merge, len_diff_in_chunks, log2_chunk_len );
          if ( ret ) {
            memmove( ret, orig_ptr, new_len_in_chunks << log2_chunk_len );
            return ret;
          }
          has_tried_front_merge = true;
        }
        void * ret = try_to_merge_with_vacancy_right_next_for_an_aligned_alloc( p_mgr,
            vacancy_back, vacancy_front, rental_info,
            alignment_in_chunks, lsbs_headchunk_addr,
            has_tried_back_merge, has_tried_front_merge, len_diff_in_chunks, log2_chunk_len );
        if ( ret ) {
          memmove( ret, orig_ptr, new_len_in_chunks << log2_chunk_len );
          return ret;
        }


//no_surrounding_space_big_enough_to_take:
        void * new_ptr = infos_smm_rent_chunks( p_mgr, userdata, custom_malloc,
            new_len_in_chunks, -7, log2_chunk_len );
        if ( !new_ptr ) {
          errno = ENOBUFS;
          return 0;
        }

        memcpy( new_ptr, orig_ptr, get_range_len(*rental_info) > new_len_in_chunks ?
            new_len_in_chunks << log2_chunk_len : get_range_len(*rental_info) << log2_chunk_len );
        if ( base_ridx + rent_idx == p_mgr->idx_last_rental_info  &&
            p_mgr->idx_last_rental_info > 0) {
          p_mgr->idx_last_rental_info--;
        }

//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc_chunks %lx(%lu) -> %lx(%lu) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(new_ptr),
//    ptr2ui(new_ptr - p_mgr->data) >> log2_chunk_len,
//    len_diff_in_chunks,
//    new_len_in_chunks );
//#endif


        enum err_msg err = infos_smm_return_chunks( p_mgr,
            userdata, custom_malloc, orig_ptr, NULL, log2_chunk_len );
        if ( err ) {
          fprintf( stderr, "[SMM] infos_smm_realloc_chunks failed. (%s)\n", arr_emsg_txt[err] );
        }
        assume( err == emsg_no_error );
        p_mgr->total_space_left -= len_diff_in_chunks;
        return new_ptr;
      }
    }

    r_node = r_node->next;
    base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
  } while ( r_node );

  // Couldn't find the rental information about ptr.
  errno = EBADSLT;
  return 0;
}

//-------------------------------------------------------------------------------------------------
void * infos_smm_realloc( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length )
{
  if ( new_length == 0 ) {
    return 0;
  }

  struct infos_singly_linked_list_node * restrict r_node = &p_mgr->al_rental_info.list.head;
  uint64_t nr_elems;
  nr_elems = UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
  uint64_t base_ridx = 0;
  uint64_t target = orig_ptr - p_mgr->data;
  do {  // while ( r_node );
    for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
      struct range_u32 * restrict rental_info;
      rental_info = r_node->data + rent_idx * sizeof(*rental_info);
      if ( base_ridx + rent_idx > p_mgr->idx_last_rental_info ) {
        // Couldn't find it
        errno = EBADSLT;
        return 0;
      }

      // Skip if it's empty.
      if ( rental_info->start > rental_info->stop ) {
        continue;
      }
      if ( rental_info->start == target ) {
        int32_t len_diff = new_length - get_range_len(*rental_info);
        if ( len_diff == 0 ) {
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc %lx(%lu) -> %lx(%u) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(orig_ptr),
//    rental_info->start,
//    0,
//    new_length );
//#endif
          p_mgr->total_space_left -= 0;
          return orig_ptr;
        }

        nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict v_node;
        v_node = &p_mgr->al_vacancies.list.head;
        uint64_t base_vidx = 0;
        // Attempt to take vacancies right next to it
        struct range_u32 * vacancy_front = nullptr;
        struct range_u32 * vacancy_back = nullptr;
        struct range_u32 * vacancy_at_tip = (void*)-1;
        uint_fast8_t nr_neighbors_found = 0;
        do {
          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
              goto end_of_neighbor_search;
            }
            struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
            if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
              vacancy_at_tip = vacancy;
            }

            // Skip if it's empty.
            if ( vacancy->start > vacancy->stop ) {
              continue;
            }

            if ( vacancy->stop == target - 1 ) {
              //vacancy_front = rental_info->start - vacancy->start;
              vacancy_front = vacancy;
              nr_neighbors_found++;
            } else if ( vacancy->start == rental_info->stop + 1 ) {
              //vacancy_back = vacancy->start - rental_info->start;
              vacancy_back = vacancy;
              nr_neighbors_found++;
            }

            if ( nr_neighbors_found == 2 ) {
              goto end_of_neighbor_search;
            }
          }
          v_node = v_node->next;
          base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
        } while ( v_node );

end_of_neighbor_search:
        // Try to edit vacancy_back
        if ( vacancy_back ) {
          if ( len_diff > 0 ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if ( get_range_len( *vacancy_back ) < len_diff ) {
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the back
            int whole_merge = get_range_len(*vacancy_back) == len_diff ?
              -7 : 0;
            rental_info->stop += len_diff;
            if ( !whole_merge ) {
              vacancy_back->start += len_diff;
            } else {  // whole_merge
              vacancy_back->start = -7;
              vacancy_back->stop = 7;
              if ( p_mgr->idx_last_vacancy > 0 &&
                  vacancy_back == vacancy_at_tip ) {
                p_mgr->idx_last_vacancy--;
              }
            }
            goto return_new_alloc;
          } else {  // len_diff < 0
            // Make space for the range in the back
            rental_info->stop += len_diff;
            vacancy_back->start += len_diff;
            goto return_new_alloc;
          }
        }
        // Try to edit vacancy_front
        if ( vacancy_front ) {
          if ( len_diff > 0 ) {
            if ( get_range_len( *vacancy_front ) < len_diff ) {
              goto no_surrounding_space_big_enough_to_take;
            }
            // Merge with the range in the front
            int whole_merge = get_range_len(*vacancy_front) == len_diff ?
              -7 : 0;
            rental_info->start -= len_diff;
            if ( !whole_merge ) {
              vacancy_front->stop -= len_diff;
            } else {  // whole_merge
              vacancy_front->start = -7;
              vacancy_front->stop = 7;
              if ( p_mgr->idx_last_vacancy > 0 &&
                  vacancy_front == vacancy_at_tip ) {
                p_mgr->idx_last_vacancy--;
              }
            }
            goto return_new_alloc;
          } else {  // len_diff < 0
            // Make space for the range in the front
            rental_info->stop += len_diff;
            vacancy_front->start += len_diff;
            goto return_new_alloc;
          }
        }

        // Try to edit both vacancy_front and vacancy_back
        if ( vacancy_back && vacancy_front ) {
          if ( len_diff > 0 &&
              get_range_len(*vacancy_front) + get_range_len(*vacancy_back) < len_diff ) {
#pragma GCC diagnostic pop
            goto no_surrounding_space_big_enough_to_take;
          }
          // Take space from the front first
          uint32_t added_len = get_range_len(*vacancy_front);
          rental_info->start -= added_len;
          vacancy_front->start = -7;
          vacancy_front->stop = 7;
          if ( p_mgr->idx_last_vacancy > 0 &&
              vacancy_front == vacancy_at_tip ) {
            p_mgr->idx_last_vacancy--;
          }

          // Take space from the back
          uint32_t deficit = len_diff - added_len;
          rental_info->stop += deficit;
          vacancy_back->start += deficit;
          if ( get_range_len(*vacancy_back) == 0 ) {
            vacancy_back->start = -7;
            vacancy_back->stop = 7;
            if ( p_mgr->idx_last_vacancy > 0 &&
                vacancy_back == vacancy_at_tip ) {
              p_mgr->idx_last_vacancy--;
            }
          }
return_new_alloc:
//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc %lx(%lu) -> %lx(%u) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(p_mgr->data) + (rental_info->start),
//    rental_info->start,
//    len_diff,
//    new_length );
//#endif
          p_mgr->total_space_left -= len_diff;
          return p_mgr->data + (rental_info->start);
        }

no_surrounding_space_big_enough_to_take:
        void * new_ptr = infos_smm_rent( p_mgr, userdata, custom_malloc,
            new_length, -1 );
        if ( !new_ptr ) {
          errno = ENOBUFS;
          return 0;
        }

        memcpy( new_ptr, orig_ptr, get_range_len(*rental_info) > new_length ?
            new_length : get_range_len(*rental_info) );
        rental_info->stop = -7;
        rental_info->start = 7;
        if ( base_ridx + rent_idx == p_mgr->idx_last_rental_info  &&
            p_mgr->idx_last_rental_info > 0) {
          p_mgr->idx_last_rental_info--;
        }

//#ifdef DEBUG_MODE
//fprintf( stdout, "[SMM] smm_realloc %lx(%lu) -> %lx(%lu) (%+d=%u)\n",
//    ptr2ui(orig_ptr),
//    target,
//    ptr2ui(new_ptr),
//    ptr2ui(new_ptr - p_mgr->data),
//    len_diff,
//    new_length );
//#endif
        enum err_msg err = infos_smm_return( p_mgr, userdata, custom_malloc, orig_ptr );
        assume( err == emsg_no_error );
        if ( err ) {
          fprintf( stderr, "[SMM] infos_smm_realloc failed. (%s)\n", arr_emsg_txt[err] );
        }
        p_mgr->total_space_left -= len_diff;
        return new_ptr;
      }
    }

    r_node = r_node->next;
    base_ridx += UINT64_C(1) << p_mgr->al_rental_info.log2_nr_elems;
  } while ( r_node );

  // Couldn't find the rental information about ptr.
  errno = EBADSLT;
  return 0;
}


//-------------------------------------------------------------------------------------------------
void infos_smm_destroy( struct infos_simple_mem_mgr * p_mgr,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  infos_al_destroy_except_preallocated_struct(&p_mgr->al_rental_info, userdata, custom_free);
  infos_al_destroy_except_preallocated_struct(&p_mgr->al_vacancies, userdata, custom_free);
}



//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_resize_pool( struct infos_simple_mem_mgr * restrict p_mgr,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t total_len_buf_n_struct )
{
  assume( _is_aligned_to_power_of_two(
        total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr),
        alignof(struct infos_simple_mem_mgr)) );

  struct range_u32 new_vacancy;

  new_vacancy.start = p_mgr->data_len;
  new_vacancy.stop = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr) - 1;

  // Make a vacancy and merge if possible
  uint64_t nr_elems;
  nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  struct infos_singly_linked_list_node * restrict v_node = &p_mgr->al_vacancies.list.head;
  uint64_t base_vidx = 0;
  //struct range_u32 * restrict vacancy_to_merge = nullptr;
  do {
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx > p_mgr->idx_last_vacancy ) {
        goto no_existing_range_to_merge;
      }
      struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);

      // Skip if it's empty.
      if ( vacancy->start > vacancy->stop ) {
        continue;
      }
      if ( vacancy->stop == new_vacancy.start - 1 ) {
        merge_range_into_lower(vacancy, &new_vacancy);
        p_mgr->idx_last_vacancy--;
      }
    }
    v_node = v_node->next;
    base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  } while ( v_node );

no_existing_range_to_merge:
  // Couldn't merge into any existing vacancy
  // Register a new one
  nr_elems = UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
  v_node = &p_mgr->al_vacancies.list.head;
  struct range_u32 * restrict vacancy;
  base_vidx = 0;
  do {
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx == p_mgr->idx_last_vacancy ) {
        if ( vacancy_idx + 1 == nr_elems ) {
          break;
        } else {
          vacancy_idx++;
          vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
          goto populate_new_vacancy;
        }
      }

      vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
      if ( vacancy->stop < vacancy->start ) {
        vacancy->start = new_vacancy.start;
        vacancy->stop = new_vacancy.stop;

        // Update data_len
        typeof(p_mgr->data_len) new_data_len;
        new_data_len = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
        p_mgr->total_space_left += p_mgr->data_len - new_data_len;
        p_mgr->data_len = new_data_len;
        return 0;
      }
    }

    v_node = v_node->next;
    base_vidx += UINT64_C(1) << p_mgr->al_vacancies.log2_nr_elems;
    if ( v_node && base_vidx >= p_mgr->idx_last_vacancy ) {
      vacancy = v_node->data;
      if ( base_vidx == p_mgr->idx_last_vacancy ) {
        vacancy++;
      }
populate_new_vacancy:
      vacancy->start = new_vacancy.start;
      vacancy->stop = new_vacancy.stop;
      p_mgr->idx_last_vacancy++;

      // Update data_len
      typeof(p_mgr->data_len) new_data_len;
      new_data_len = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
      p_mgr->total_space_left += p_mgr->data_len - new_data_len;
      p_mgr->data_len = new_data_len;
      return 0;
    }
  } while ( v_node );

  // Grow bigger if there is no space
  uint_fast8_t err = infos_al_grow( &p_mgr->al_vacancies, userdata, custom_malloc, 1, 0 );
  if ( err ) {
    return emsg_out_of_memory;
  }
  struct infos_singly_linked_list_node * tail =
    infos_ll_get_last_node(&p_mgr->al_vacancies.list.head);
  p_mgr->idx_last_vacancy++;
  vacancy = tail->data;
  vacancy->start = new_vacancy.start;
  vacancy->stop = new_vacancy.stop;

  // Update data_len
  typeof(p_mgr->data_len) new_data_len;
  new_data_len = total_len_buf_n_struct - sizeof(struct infos_simple_mem_mgr);
  p_mgr->total_space_left += p_mgr->data_len - new_data_len;
  p_mgr->data_len = new_data_len;
  return 0;
}


