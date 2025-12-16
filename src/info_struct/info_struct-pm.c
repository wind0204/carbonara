#include <stdint.h>
#include <errno.h>

#include "err_msg.h"
#include "info_struct.h"
#include "util-spall.h"


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
  p_pm->idx_last_rental_info = 0;
  p_pm->idx_last_vacancy = 0;

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
    return err_msg_out_of_memory;
  }

  // To just copy and paste I created one shared variable here.
  struct infos_arr_list * restrict al;

  al = &p_pm->al_rental_info;
  auto node_size = rental_info_node_size;
  // Copy
  al->log2_node_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_nr_elems = al->log2_node_size - al->log2_elem_size;
  al->list.head.data = alloc;
  infos_take_care_of_linked_list_after_putting_head( &al->list );

  alloc = alloc + rental_info_node_size;
  al = &p_pm->al_vacancies;
  node_size = available_ranges_node_size;
  // Paste
  al->log2_node_size = get_log2( node_size );
  al->log2_elem_size = 31 - _lzcnt_u32( sizeof(struct range_u32) );
  al->log2_nr_elems = al->log2_node_size - al->log2_elem_size;
  al->list.head.data = alloc;
  infos_take_care_of_linked_list_after_putting_head( &al->list );

  struct range_u32 * first_rental_info;
  first_rental_info = infos_al_get_elem(&p_pm->al_rental_info, 0);
  first_rental_info->start = -7;
  first_rental_info->stop = 7;
  assume( first_rental_info->start > first_rental_info->stop );

  struct range_u32 * first_available_range;
  first_available_range = infos_al_get_elem(&p_pm->al_vacancies, 0);
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
struct range_u32 * infos_pm_rent( struct infos_page_mgr * restrict p_pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_seq_pages_needed,
    bool take_from_left
    )
{
  SPALL_BEGIN("pm_rent");

  if ( p_pm->nr_unused_pages < nr_seq_pages_needed ) {
    errno = ENOBUFS;
    return 0;
  }

  uint64_t nr_elems = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
  uint64_t base_vidx = 0;
  struct infos_singly_linked_list_node * restrict v_node = &p_pm->al_vacancies.list.head;
  do {  // while ( v_node );
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
      if ( base_vidx + vacancy_idx > p_pm->idx_last_vacancy ) {
        // Couldn't find a vacancy
        errno = ENOBUFS;
        SPALL_END();
        return nullptr;
      }

      struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
      // Is it an invalid one?
      if ( vacancy->start > vacancy->stop ) {
        continue;
      }

      if ( get_range_len(*vacancy) >= nr_seq_pages_needed ) {
        // Register the rental_info
        nr_elems = UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict r_node;
        r_node = &p_pm->al_rental_info.list.head;
        uint64_t base_ridx = 0;
        do {  // while ( r_node );
          for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
            struct range_u32 * restrict rental;
            rental = r_node->data + rent_idx * sizeof(*rental);
            if ( base_ridx + rent_idx > p_pm->idx_last_rental_info ) {
              p_pm->idx_last_rental_info = base_ridx + rent_idx;
              goto take_and_return;
            }
            if ( rental->stop < rental->start ) {
              // Take the empty element; and return;
take_and_return:
              if ( take_from_left ) {
                rental->stop = vacancy->start + nr_seq_pages_needed - 1;
                rental->start = vacancy->start;
              } else {
                rental->start = vacancy->stop - nr_seq_pages_needed + 1;
                rental->stop = vacancy->stop;
              }

              // Reduce the size of vacancy
              if ( take_from_left )
                vacancy->start = vacancy->start + nr_seq_pages_needed;
              else
                vacancy->stop = vacancy->stop - nr_seq_pages_needed;

              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
                  if ( p_pm->idx_last_vacancy > 0 ) {
                    p_pm->idx_last_vacancy--;
                  }
                }
              }

              // XXX Compiler should optimize this line and just use nr_seq_pages_needed
              p_pm->nr_unused_pages -= get_range_len(*rental);
              SPALL_END();
              return rental;
            }
          }

          r_node = r_node->next;
          base_ridx += UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;
        } while ( r_node );

        // Grow the infos_arr_list and register one
        uint_fast8_t err = infos_al_grow( &p_pm->al_rental_info, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          switch ( err ) {
            case err_msg_out_of_memory:
              errno = ENOMEM;
              break;
            default:
          }
          SPALL_END();
          return nullptr;
        }
        r_node = infos_ll_get_last_node(&p_pm->al_rental_info.list.head);
        p_pm->idx_last_rental_info = base_ridx;
        struct range_u32 * restrict rental = r_node->data;
        if ( take_from_left ) {
          rental->stop = vacancy->start + nr_seq_pages_needed - 1;
          rental->start = vacancy->start;
        } else {
          rental->start = vacancy->stop - nr_seq_pages_needed + 1;
          rental->stop = vacancy->stop;
        }
        // XXX Compiler should optimize this line and just use nr_seq_pages_needed
        p_pm->nr_unused_pages -= get_range_len(*rental);

        // Reduce the size of vacancy
        if ( take_from_left )
          vacancy->start = vacancy->start + nr_seq_pages_needed;
        else
          vacancy->stop = vacancy->stop - nr_seq_pages_needed;

        if ( get_range_len(*vacancy) == 0 ) {
          vacancy->start = -7;
          vacancy->stop = 7;
          if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
            if ( p_pm->idx_last_vacancy > 0 ) {
              p_pm->idx_last_vacancy--;
            }
          }
        }

        p_pm->nr_unused_pages -= get_range_len(*rental);
        SPALL_END();
        return rental;
      }
    }

    v_node = v_node->next;
    base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
  } while (v_node);

  // Couldn't find a vacancy.
  errno = ENOBUFS;
  SPALL_END();
  return nullptr;
}


constexpr size_t NR_TRIES_TO_FIND_A_PREALIGNED_SPACE = 3;

//-------------------------------------------------------------------------------------------------
struct range_u32 * infos_pm_rent_aligned( struct infos_page_mgr * restrict p_pm,
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
    return nullptr;
  }

  uint_fast8_t nr_tries_to_find_prealigned = 0;
  struct range_u32 * spacious_range;
  struct range_u32 * restrict vacancy;
  uint64_t nr_elems;
  uint64_t base_vidx;
  nr_elems = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
  struct infos_singly_linked_list_node * restrict v_node = &p_pm->al_vacancies.list.head;
  base_vidx = 0;
  do {  // while ( v_node );
    for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {

      if ( base_vidx + vacancy_idx > p_pm->idx_last_vacancy ) {
        // Couldn't find a vacancy.
        errno = ENOBUFS;
        SPALL_END();
        return nullptr;
      }
      vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);
      // Is it an invalid one?
      if ( vacancy->start > vacancy->stop ) {
        continue;
      }

      //uint32_t lsbs_head_addr = ptr2ui(p_pm->data) & (alignment - 1);
      constexpr uint32_t lsbs_head_addr = 0;
      //uint32_t nr_misaligned_bytes_at_head = (alignment - lsbs_head_addr) & (alignment - 1);
      constexpr uint32_t nr_misaligned_bytes_at_head = 0;
      uint32_t lsbs_tail_addr_when_aligned =
        (nr_misaligned_bytes_at_head + nr_seq_pages_needed - 1) & (alignment - 1);

      struct range_u32 new_vacancy;
      new_vacancy.start = -7;
      new_vacancy.stop = 7;
      if ( get_aligned_range_len(*vacancy, alignment, lsbs_head_addr)
          >= nr_seq_pages_needed ) {
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
        uint32_t nr_units_left_after_alloc = get_range_len(*vacancy) - nr_seq_pages_needed;
        uint32_t first_unit_idx = nr_units_left_after_alloc/2 - nr_seq_pages_needed/2;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
        first_unit_idx =
          _align_to_power_of_two(vacancy->start + first_unit_idx
              + lsbs_head_addr, alignment)
          - lsbs_head_addr;
#pragma GCC diagnostic pop

        while ( first_unit_idx + nr_seq_pages_needed - 1 > vacancy->stop ) {
          first_unit_idx -= alignment;
        }

        // Prepare to register a new vacancy
        new_vacancy.start = first_unit_idx + nr_seq_pages_needed;
        new_vacancy.stop = vacancy->stop;

        vacancy->stop = first_unit_idx - 1;


register_new_rental_info:
        // Register the rental_info
        nr_elems = UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict r_node;
        r_node = &p_pm->al_rental_info.list.head;
        uint64_t base_ridx = 0;
        do {
          struct range_u32 * restrict rental;
          for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
            rental = r_node->data + rent_idx * sizeof(*rental);
            if ( base_ridx + rent_idx > p_pm->idx_last_rental_info ) {
              p_pm->idx_last_rental_info = base_ridx + rent_idx;
              goto take_it_and_return;
            }
            if ( rental->stop < rental->start ) {
              // Take the empty element; and return;
take_it_and_return:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
              if ( !take_from_right ) {
                rental->stop = vacancy->start + nr_seq_pages_needed - 1;
                rental->start = vacancy->start;
              } else {
                rental->start = vacancy->stop - nr_seq_pages_needed + 1;
                rental->stop = vacancy->stop;
              }
#pragma GCC diagnostic pop
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u\n",
//    ptr2ui(p_pm->data), rental->start, rental->stop);
//#endif
              // Reduce the size of vacancy
              // From the right side? or the left side?
              if ( !take_from_right )
                vacancy->start = vacancy->start + nr_seq_pages_needed;
              else
                vacancy->stop = vacancy->stop - nr_seq_pages_needed;
              // See if there is a new vacancy to register
              if ( new_vacancy.start <= new_vacancy.stop ) {
                goto register_new_vacancy;
              }

              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
                  if ( p_pm->idx_last_vacancy > 0 ) {
                    p_pm->idx_last_vacancy--;
                  }
                }
              }
              p_pm->nr_unused_pages -= get_range_len(*rental);
              SPALL_END();
              return (struct range_u32*)rental;
            }
          }

          r_node = r_node->next;
          base_ridx += UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;
          if ( r_node && base_ridx >= p_pm->idx_last_rental_info ) {
            rental = r_node->data;
            if ( base_ridx == p_pm->idx_last_rental_info ) {
              rental += 1;
            }
            p_pm->idx_last_rental_info++;

            // Take the next element; and return;
            if ( !take_from_right ) {
              rental->stop = vacancy->start + nr_seq_pages_needed - 1;
              rental->start = vacancy->start;
            } else {
              rental->start = vacancy->stop - nr_seq_pages_needed + 1;
              rental->stop = vacancy->stop;
            }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u\n",
//    ptr2ui(p_pm->data), rental->start, rental->stop);
//#endif

            // Reduce the size of vacancy
            // From the right side? or the left side?
            if ( !take_from_right )
              vacancy->start = vacancy->start + nr_seq_pages_needed;
            else
              vacancy->stop = vacancy->stop - nr_seq_pages_needed;

            // See if there is a new vacancy to register
            if ( new_vacancy.start <= new_vacancy.stop ) {
                goto register_new_vacancy;
            }

            if ( get_range_len(*vacancy) == 0 ) {
              vacancy->start = -7;
              vacancy->stop = 7;
              if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
                if ( p_pm->idx_last_vacancy > 0 ) {
                  p_pm->idx_last_vacancy--;
                }
              }
            }
            p_pm->nr_unused_pages -= get_range_len(*rental);
            SPALL_END();
            return (struct range_u32*)rental;
          }
        } while ( r_node );

        // Grow the infos_arr_list and register one
        uint_fast8_t err = infos_al_grow( &p_pm->al_rental_info, userdata, custom_malloc,
            1, 0 );
        if ( err ) {
          switch ( err ) {
            case err_msg_out_of_memory:
              errno = ENOMEM;
              break;
            default:
              unreachable();
          }
          SPALL_END();
          return nullptr;
        }
        r_node = infos_ll_get_last_node(&p_pm->al_rental_info.list.head);
        p_pm->idx_last_rental_info++;
        struct range_u32 * restrict rental = r_node->data;
        if ( !take_from_right ) {
          rental->stop = vacancy->start + nr_seq_pages_needed - 1;
          rental->start = vacancy->start;
        } else {
          rental->start = vacancy->stop - nr_seq_pages_needed + 1;
          rental->stop = vacancy->stop;
        }
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u\n",
//    ptr2ui(p_pm->data), rental->start, rental->stop);
//#endif

        if ( new_vacancy.start > new_vacancy.stop ) {
          // Reduce the size of vacancy
          // From the right side? or the left side?
          if ( !take_from_right )
            vacancy->start = vacancy->start + nr_seq_pages_needed;
          else
            vacancy->stop = vacancy->stop - nr_seq_pages_needed;

          if ( get_range_len(*vacancy) == 0 ) {
            vacancy->start = -7;
            vacancy->stop = 7;
            if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
              if ( p_pm->idx_last_vacancy > 0 ) {
                p_pm->idx_last_vacancy--;
              }
            }
          }
          p_pm->nr_unused_pages -= get_range_len(*rental);
          SPALL_END();
          return (struct range_u32*)rental;
        }

register_new_vacancy:
        rental = r_node->data;
        // Register new_vacancy
        nr_elems = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
        r_node = &p_pm->al_vacancies.list.head;
        struct range_u32 * restrict new_slot;
        base_vidx = 0;
        do {
          for ( uint_fast32_t il_vidx = 0; il_vidx < nr_elems; il_vidx++ ) {
            if ( base_vidx + il_vidx == p_pm->idx_last_vacancy ) {
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
//    ptr2ui(p_pm->data), rental->start, rental->stop);
//#endif

              new_slot->start = new_vacancy.start;
              new_slot->stop = new_vacancy.stop;
              if ( get_range_len(*vacancy) == 0 ) {
                vacancy->start = -7;
                vacancy->stop = 7;
                if ( base_vidx + il_vidx == p_pm->idx_last_vacancy ) {
                  if ( p_pm->idx_last_vacancy > 0 ) {
                    p_pm->idx_last_vacancy--;
                  }
                }
              }
              p_pm->nr_unused_pages -= get_range_len(*rental);
              SPALL_END();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
              return (struct range_u32*)rental;
#pragma GCC diagnostic pop
            }
          }

          r_node = r_node->next;
          base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_elem_size;
          if ( r_node && base_vidx >= p_pm->idx_last_vacancy ) {
            new_slot = r_node->data;
            if ( base_vidx == p_pm->idx_last_vacancy ) {
              new_slot++;
            }
populate_new_vacancy:
//#ifdef DEBUG_MODE
//fprintf(stdout,
//    "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u (populate_new_vacancy)\n",
//    ptr2ui(p_pm->data), rental->start, rental->stop);
//#endif

            new_slot->start = new_vacancy.start;
            new_slot->stop = new_vacancy.stop;
            p_pm->idx_last_vacancy++;
            if ( get_range_len(*vacancy) == 0 ) {
              vacancy->start = -7;
              vacancy->stop = 7;
              if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
                if ( p_pm->idx_last_vacancy > 0 ) {
                  p_pm->idx_last_vacancy--;
                }
              }
            }
            p_pm->nr_unused_pages -= get_range_len(*rental);
            SPALL_END();
            return (struct range_u32*)rental;
          }
        } while ( r_node );

        // Grow the linked array
        err = infos_al_grow( &p_pm->al_vacancies, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          switch ( err ) {
            case err_msg_out_of_memory:
              errno = ENOMEM;
              break;
            default:
              unreachable();
          }
          // Revert the removal
          if ( !take_from_right )
            vacancy->start = vacancy->start - nr_seq_pages_needed;
          else
            vacancy->stop = vacancy->stop + nr_seq_pages_needed;

          SPALL_END();
          return nullptr;
        }
        //p_pm->nr_unused_pages -= UINT64_C(1) << p_pm->al_vacancies.log2_node_size;
//#ifdef DEBUG_MODE
//fprintf(stdout, "[SMM] infos_smm_rent_aligned_impl %lx + %u-%u (Grow the linked array)\n",
//    ptr2ui(p_pm->data), rental->start, rental->stop);
//#endif

        r_node = infos_ll_get_last_node(&p_pm->al_vacancies.list.head);
        p_pm->idx_last_vacancy++;
        new_slot = r_node->data;
        new_slot->start = new_vacancy.start;
        new_slot->stop = new_vacancy.stop;

        if ( get_range_len(*vacancy) == 0 ) {
          vacancy->start = -7;
          vacancy->stop = 7;
          if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
            if ( p_pm->idx_last_vacancy > 0 ) {
              p_pm->idx_last_vacancy--;
            }
          }
        }
        p_pm->nr_unused_pages -= get_range_len(*rental);
        SPALL_END();
        return (struct range_u32*)rental;
      }
    }

    v_node = v_node->next;  // NOLINT(clang-analyzer-core.NullDereference)
    base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
  } while ( v_node );

  if ( nr_tries_to_find_prealigned ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
    vacancy = spacious_range;
#pragma GCC diagnostic pop
    goto come_back_here_with_a_spacious_range;
  }
  SPALL_END();
  return nullptr;
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_pm_return( struct infos_page_mgr * restrict p_pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t head_idx
    )
{
  SPALL_BEGIN("pm_return");
  struct infos_singly_linked_list_node * restrict r_node = &p_pm->al_rental_info.list.head;
  uint64_t nr_elems = UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;
  uint64_t base_ridx = 0;
  do {  // while ( r_node );
    for ( uint_fast32_t rent_idx = 0; rent_idx < nr_elems; rent_idx++ ) {
      struct range_u32 * restrict rental_info;
      rental_info = r_node->data + rent_idx * sizeof(*rental_info);
      if ( base_ridx + rent_idx > p_pm->idx_last_rental_info ) {
        SPALL_END();
        return err_msg_no_such_entry;
      }

      // Skip if it's empty.
      if ( rental_info->start > rental_info->stop ) {
        continue;
      }

      // We found a match?
      if ( rental_info->start == head_idx ) {
        // Make a vacancy and merge if possible
        nr_elems = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
        struct infos_singly_linked_list_node * restrict v_node;
        v_node = &p_pm->al_vacancies.list.head;
        struct range_u32 * restrict vacancy_to_merge_with = nullptr;
        uint64_t base_vidx = 0;
        do {
          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx > p_pm->idx_last_vacancy ) {
              goto no_existing_range_to_merge;
            }

            struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);

            // Skip if it's empty.
            if ( vacancy->start > vacancy->stop ) {
              continue;
            }

            // Some messy code from a long time ago
            if ( !vacancy_to_merge_with ) {
              if ( vacancy->stop == head_idx - 1 ) {
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
                p_pm->nr_unused_pages += get_range_len(*rental_info);

                // Mark the element empty.
                rental_info->start = -7;
                rental_info->stop = 7;
                if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
                    p_pm->idx_last_rental_info > 0 ) {
                  p_pm->idx_last_rental_info--;
                }
                SPALL_END();
                return 0;
              }
            }

            if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
              goto mark_empty_and_return;
            }
          }
          v_node = v_node->next;
          base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
        } while ( v_node );

no_existing_range_to_merge:
        // Couldn't merge into any existing vacancy

        // Register a new one
        nr_elems = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
        v_node = &p_pm->al_vacancies.list.head;
        struct range_u32 * restrict vacancy;
        base_vidx = 0;
        do {
          for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
            if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
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
              p_pm->nr_unused_pages += get_range_len(*rental_info);

              // Mark the element empty.
              rental_info->start = -7;
              rental_info->stop = 7;
              if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
                  p_pm->idx_last_rental_info > 0 ) {
                p_pm->idx_last_rental_info--;
              }
              SPALL_END();
              return 0;
            }
          }

          v_node = v_node->next;
          base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
          if ( v_node && base_vidx >= p_pm->idx_last_vacancy ) {
            vacancy = v_node->data;
            if ( base_vidx == p_pm->idx_last_vacancy ) {
              vacancy++;
            }
populate_new_vacancy:
            vacancy->start = rental_info->start;
            vacancy->stop = rental_info->stop;
            p_pm->idx_last_vacancy++;
            p_pm->nr_unused_pages += get_range_len(*rental_info);

            // Mark the element empty.
            rental_info->start = -7;
            rental_info->stop = 7;
            if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
                p_pm->idx_last_rental_info > 0 ) {
              p_pm->idx_last_rental_info--;
            }
            SPALL_END();
            return 0;
          }
        } while ( v_node );

        // Grow the linked array
        uint_fast8_t err = infos_al_grow( &p_pm->al_vacancies, userdata, custom_malloc, 1, 0 );
        if ( err ) {
          SPALL_END();
          return err_msg_out_of_memory;
        }
        p_pm->nr_unused_pages -= UINT64_C(1) <<  p_pm->al_vacancies.log2_node_size;
        v_node = infos_ll_get_last_node(&p_pm->al_vacancies.list.head);
        p_pm->idx_last_vacancy++;
        vacancy = v_node->data;
        vacancy->start = rental_info->start;
        vacancy->stop = rental_info->stop;

        p_pm->nr_unused_pages += get_range_len(*rental_info);

        // Mark the element empty.
        rental_info->start = -7;
        rental_info->stop = 7;
        if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
            p_pm->idx_last_rental_info > 0 ) {
          p_pm->idx_last_rental_info--;
        }
        SPALL_END();
        return 0;
      }

      // Did not find a match, check the next array
    }
    r_node = r_node->next;
    base_ridx += UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;

  } while ( r_node );

  SPALL_END();
  return err_msg_no_such_entry;
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_pm_return_via_ptr( struct infos_page_mgr * restrict p_pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    struct range_u32 * p_range
    )
{
  SPALL_BEGIN("pm_return_ptr");
  struct infos_singly_linked_list_node * restrict r_node = &p_pm->al_rental_info.list.head;
  uint64_t base_ridx = 0;

  do {  // while ( r_node );
    if ( (void*)p_range < r_node->data ||
        (void*)p_range >= r_node->data + (UINT64_C(1) << p_pm->al_rental_info.log2_node_size) ) {
      r_node = r_node->next;
      base_ridx += UINT64_C(1) << p_pm->al_rental_info.log2_nr_elems;
      continue;
    }

    if ( p_range->start > p_range->stop ) {
      SPALL_END();
      return err_msg_already_done;
    }

    uint64_t rent_idx = ptr2ui((void*)p_range - r_node->data) >>
      p_pm->al_rental_info.log2_elem_size;
    // Make a vacancy and merge if possible
    uint64_t nr_elems = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
    struct infos_singly_linked_list_node * restrict v_node;
    v_node = &p_pm->al_vacancies.list.head;
    struct range_u32 * restrict vacancy_to_merge_with = nullptr;
    uint64_t base_vidx = 0;
    do {
      for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
        if ( base_vidx + vacancy_idx > p_pm->idx_last_vacancy ) {
          goto no_existing_range_to_merge;
        }

        struct range_u32 * restrict vacancy = v_node->data + vacancy_idx * sizeof(*vacancy);

        // Skip if it's empty.
        if ( vacancy->start > vacancy->stop ) {
          continue;
        }

        // Some messy code from a long time ago
        if ( !vacancy_to_merge_with ) {
          if ( vacancy->stop == p_range->start - 1 ) {
            merge_range_into_lower(vacancy, p_range);
            vacancy_to_merge_with = vacancy;
          } else if ( vacancy->start == p_range->stop + 1 ) {
            merge_range_into_higher(p_range, vacancy);
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
            p_pm->nr_unused_pages += get_range_len(*p_range);

            // Mark the element empty.
            p_range->start = -7;
            p_range->stop = 7;
            if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
                p_pm->idx_last_rental_info > 0 ) {
              p_pm->idx_last_rental_info--;
            }
            SPALL_END();
            return 0;
          }
        }

        if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
          goto mark_empty_and_return;
        }
      }
      v_node = v_node->next;
      base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
    } while ( v_node );

no_existing_range_to_merge:
    // Couldn't merge into any existing vacancy

    // Register a new one
    nr_elems = UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
    v_node = &p_pm->al_vacancies.list.head;
    struct range_u32 * restrict vacancy;
    base_vidx = 0;
    do {
      for ( uint_fast32_t vacancy_idx = 0; vacancy_idx < nr_elems; vacancy_idx++ ) {
        if ( base_vidx + vacancy_idx == p_pm->idx_last_vacancy ) {
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
          vacancy->start = p_range->start;
          vacancy->stop = p_range->stop;
          p_pm->nr_unused_pages += get_range_len(*p_range);

          // Mark the element empty.
          p_range->start = -7;
          p_range->stop = 7;
          if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
              p_pm->idx_last_rental_info > 0 ) {
            p_pm->idx_last_rental_info--;
          }
          SPALL_END();
          return 0;
        }
      }

      v_node = v_node->next;
      base_vidx += UINT64_C(1) << p_pm->al_vacancies.log2_nr_elems;
      if ( v_node && base_vidx >= p_pm->idx_last_vacancy ) {
        vacancy = v_node->data;
        if ( base_vidx == p_pm->idx_last_vacancy ) {
          vacancy++;
        }
populate_new_vacancy:
        vacancy->start = p_range->start;
        vacancy->stop = p_range->stop;
        p_pm->idx_last_vacancy++;
        p_pm->nr_unused_pages += get_range_len(*p_range);

        // Mark the element empty.
        p_range->start = -7;
        p_range->stop = 7;
        if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
            p_pm->idx_last_rental_info > 0 ) {
          p_pm->idx_last_rental_info--;
        }
        SPALL_END();
        return 0;
      }
    } while ( v_node );

    // Grow the linked array
    uint_fast8_t err = infos_al_grow( &p_pm->al_vacancies, userdata, custom_malloc, 1, 0 );
    if ( err ) {
      SPALL_END();
      return err_msg_out_of_memory;
    }
    p_pm->nr_unused_pages -= UINT64_C(1) <<  p_pm->al_vacancies.log2_node_size;
    v_node = infos_ll_get_last_node(&p_pm->al_vacancies.list.head);
    p_pm->idx_last_vacancy++;
    vacancy = v_node->data;
    vacancy->start = p_range->start;
    vacancy->stop = p_range->stop;

    p_pm->nr_unused_pages += get_range_len(*p_range);

    // Mark the element empty.
    p_range->start = -7;
    p_range->stop = 7;
    if ( base_ridx + rent_idx == p_pm->idx_last_rental_info &&
        p_pm->idx_last_rental_info > 0 ) {
      p_pm->idx_last_rental_info--;
    }
    SPALL_END();
    return 0;

  } while ( r_node );

  SPALL_END();
  return err_msg_no_such_entry;
}
