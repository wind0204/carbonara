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


//-------------------------------------------------------------------------------------------------
struct range_u32 * infos_pm_rent_aligned( struct infos_page_mgr * restrict p_pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_seq_pages_needed,
    uint32_t alignment,
    bool take_from_left
    )
{
  SPALL_BEGIN("pm_rent_aligned");
  assume( alignment > 1 );
  assume( _is_power_of_two(alignment) );

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
                p_pm->nr_unused_pages -= get_range_len(*rental_info);

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
              p_pm->nr_unused_pages -= get_range_len(*rental_info);

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
            p_pm->nr_unused_pages -= get_range_len(*rental_info);

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

        p_pm->nr_unused_pages -= get_range_len(*rental_info);

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
            p_pm->nr_unused_pages -= get_range_len(*p_range);

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
          p_pm->nr_unused_pages -= get_range_len(*p_range);

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
        p_pm->nr_unused_pages -= get_range_len(*p_range);

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

    p_pm->nr_unused_pages -= get_range_len(*p_range);

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
