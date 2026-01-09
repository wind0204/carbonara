#include <asm-generic/errno.h>
#include <immintrin.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>

#include "err_msg.h"
#include "util-c_lang-basic.h"
#include "memorable_arena.h"

#ifdef DEBUG_MODE
//#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
/*
 * \warning call it only when you have locked the sub_arena
 */
static void remember_to_free_it_later( struct ma_sub_arena * restrict sub_arena,
    uint8_t idx_proc, uint32_t offset );

//-------------------------------------------------------------------------------------------------
/*
 * \warning call it only when you have locked the sub_arena
 */
static void release_all( struct ma_sub_arena * restrict sub_arena, uint8_t idx_proc );



//-------------------------------------------------------------------------------------------------
static void * ma_internal_malloc( void * sub_arena, size_t alignment, size_t size )
{
  assume( _is_power_of_two(alignment) );
  assume( _is_aligned_to_power_of_two(size, alignment) );

  void * ret;
  if ( alignment <= CACHELINE_LEN ) {
    ret = ma_malloc( sub_arena, _get_nr_units_to_cover(size, CACHELINE_LEN),
        0, default_malloc );
  } else {
    ret = ma_aligned_alloc( sub_arena, alignment, size, 0, default_malloc );
  }
  return ret;
}

//-------------------------------------------------------------------------------------------------
void * ma_malloc( struct ma_sub_arena * sub_arena,
    uint32_t nr_mem_chunks,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  void * ret = infos_smm_rent_chunks( &sub_arena->smm, userdata, custom_malloc,
      nr_mem_chunks, -7, LOG2_CACHELINE_LEN ); 
  //if ( ret ) {
  //  sub_arena->nr_free_chunks -= nr_mem_chunks;
  //}

  return ret;
}

//-------------------------------------------------------------------------------------------------
void * ma_malloc_bytes( struct ma_sub_arena * sub_arena,
    uint32_t nr_bytes,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  uint32_t nr_mem_chunks = _align_to_power_of_two( nr_bytes, CACHELINE_LEN );
  void * ret = infos_smm_rent_chunks( &sub_arena->smm, userdata, custom_malloc,
      nr_mem_chunks, -7, LOG2_CACHELINE_LEN ); 
  //if ( ret ) {
  //  sub_arena->nr_free_chunks -= nr_mem_chunks;
  //}

  return ret;
}


//-------------------------------------------------------------------------------------------------
void * ma_aligned_alloc(
    struct ma_sub_arena * sub_arena, uint32_t alignment, uint64_t nr_bytes,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  if ( alignment < CACHELINE_LEN ) {
    alignment = CACHELINE_LEN;
  }

  assume( _is_aligned_to_power_of_two(alignment, CACHELINE_LEN) );

  uint32_t nr_mem_chunks = (nr_bytes + CACHELINE_LEN - 1) >> LOG2_CACHELINE_LEN;

  void * ret = nullptr;
  if ( sub_arena->is_allocating ) {
    ret = default_malloc( nullptr, alignment, nr_mem_chunks << LOG2_CACHELINE_LEN );
  } else {
    sub_arena->is_allocating = 7;
    ret = infos_smm_rent_aligned_chunks( &sub_arena->smm, userdata, custom_malloc,
        nr_mem_chunks, alignment, LOG2_CACHELINE_LEN );
  }
  sub_arena->is_allocating = 0;

  //if ( ret ) {
  //  sub_arena->nr_free_chunks -= nr_mem_chunks;
  //}

  return ret;
}


//-------------------------------------------------------------------------------------------------
void ma_free_simple_ptr( memorable_arena_t * restrict arena, uint8_t idx_user, void * restrict ptr )
{
  if ( !ptr ) return;

  // Look for the right sub_arena
  struct ma_sub_arena * restrict sub_arena;
  for ( sub_arena = arena->head; sub_arena; sub_arena = sub_arena->next ) {
    if ( sub_arena->smm.data <= ptr &&
        ptr <= (sub_arena->smm.data + sub_arena->smm.data_len * CACHELINE_LEN) ) {
      uint8_t old_value = atomic_load_explicit( &sub_arena->lock, memory_order_acquire );

      if ( old_value == 0 ) {
        // Try to lock the sub_arena
        if ( !atomic_compare_exchange_strong_explicit(&sub_arena->lock,
              &old_value, idx_user+1, memory_order_acq_rel, memory_order_acquire) ) {
          // When failed to lock the sub_arena

          // Remember to free it later.
          uint32_t offset = (ptr - sub_arena->smm.data) / CACHELINE_LEN;
          remember_to_free_it_later(sub_arena, idx_user, offset);

        } else {
          // Release the string
          //uint32_t nr_chunks_released;
          enum err_msg err = infos_smm_return_chunks( &sub_arena->smm,
              sub_arena, ma_internal_malloc, ptr, nullptr, LOG2_CACHELINE_LEN );
          //    sub_arena, ma_internal_malloc, ptr, &nr_chunks_released, LOG2_CACHELINE_LEN );
          //sub_arena->nr_free_chunks += nr_chunks_released;

          switch ( err ) {
            case emsg_no_such_entry:
              fprintf( stdout, "[MA-Debug] ptr = %#018lx\n", ptr2ui(ptr) );
              fprintf( stdout, "[MA-Debug] sub_arena->smm.data = %#018lx\n",
                  ptr2ui(sub_arena->smm.data) );
              fprintf( stdout, "[MA-Debug] sub_arena->smm.data_len = %u\n",
                  sub_arena->smm.data_len*CACHELINE_LEN );
              fprintf( stdout, "[MA-Debug] data+data_len-ptr = %lu\n", ptr2ui(sub_arena->smm.data) + sub_arena->smm.data_len*CACHELINE_LEN - ptr2ui(ptr) );
              default_free( nullptr, ptr );
              break;
            case emsg_no_error:
              break;
            default:
              fprintf( stderr, "[MA] ma_free_simple_ptr failed. (%s)\n", arr_emsg_txt[err] );
              // Unlock the sub_arena
              atomic_store_explicit(&sub_arena->lock, 0, memory_order_release);
              return;
          }

          // Release more strings
          release_all(sub_arena, idx_user);

          // Unlock the sub_arena
          atomic_store_explicit(&sub_arena->lock, 0, memory_order_release);
        } 
      } else if ( old_value == idx_user + 1 ) {
        // We already own the lock

        // Release the string
        //uint32_t nr_chunks_released;
        enum err_msg err = infos_smm_return_chunks( &sub_arena->smm,
            sub_arena, ma_internal_malloc, ptr, nullptr, LOG2_CACHELINE_LEN );
        //    sub_arena, ma_internal_malloc, ptr, &nr_chunks_released, LOG2_CACHELINE_LEN );
        //sub_arena->nr_free_chunks += nr_chunks_released;

        switch ( err ) {
          case emsg_no_such_entry:
            fprintf( stdout, "[MA-Debug] ptr = %#018lx\n", ptr2ui(ptr) );
            fprintf( stdout, "[MA-Debug] sub_arena->smm.data = %#018lx\n",
                ptr2ui(sub_arena->smm.data) );
            fprintf( stdout, "[MA-Debug] sub_arena->smm.data_len = %u\n",
                sub_arena->smm.data_len*CACHELINE_LEN );
            fprintf( stdout, "[MA-Debug] data+data_len-ptr = %lu\n", ptr2ui(sub_arena->smm.data) + sub_arena->smm.data_len*CACHELINE_LEN - ptr2ui(ptr) );
            default_free( nullptr, ptr );
            break;
          case emsg_no_error:
            break;
          default:
            fprintf( stderr, "[MA] ma_free_simple_ptr failed. (%s)\n", arr_emsg_txt[err] );
            return;
        }

        // Release more strings
        release_all(sub_arena, idx_user);
      }

      return;
    }
  }

}


//-------------------------------------------------------------------------------------------------
void ma_free_assuming_lock_has_been_obtained( struct ma_sub_arena * restrict sub_arena,
    uint8_t idx_user, void * restrict ptr )
{
  if (!ptr) return;

  // Release the string
  //uint32_t nr_chunks_released;
  enum err_msg err = infos_smm_return_chunks( &sub_arena->smm,
      sub_arena, ma_internal_malloc, ptr, nullptr, LOG2_CACHELINE_LEN );
  //    sub_arena, ma_internal_malloc, ptr, &nr_chunks_released, LOG2_CACHELINE_LEN );
  switch ( err ) {
    case emsg_no_such_entry:
      fprintf( stdout, "[MA-Debug] ptr = %#018lx\n", ptr2ui(ptr) );
      fprintf( stdout, "[MA-Debug] sub_arena->smm.data = %#018lx\n",
          ptr2ui(sub_arena->smm.data) );
      fprintf( stdout, "[MA-Debug] sub_arena->smm.data_len = %u\n",
          sub_arena->smm.data_len*CACHELINE_LEN );
      fprintf( stdout, "[MA-Debug] data+data_len-ptr = %lu\n", ptr2ui(sub_arena->smm.data) + sub_arena->smm.data_len*CACHELINE_LEN - ptr2ui(ptr) );
      default_free( nullptr, ptr );
      break;
    case emsg_no_error:
      break;
    default:
      fprintf( stderr, "[MA] ma_free_assuming_lock_has_been_obtained failed. (%s)\n",
          arr_emsg_txt[err] );
      return;
  }
  //sub_arena->nr_free_chunks += nr_chunks_released;

  // Release more strings
  release_all( sub_arena, idx_user );
}


//-------------------------------------------------------------------------------------------------
static void remember_to_free_it_later( struct ma_sub_arena * restrict sub_arena,
    uint8_t idx_user, uint32_t offset )
{
  struct ma_mem_release_recorder * restrict mrr = &sub_arena->arr_lrm[idx_user];

  constexpr size_t ofs_aligned_arr = ( sizeof(mrr->first_few) - ALIGN_FOR_AVX ) /
    sizeof( mrr->first_few[0] );
  //for ( size_t i = 0; i < _arr_elem_cnt(mrr->first_few); i++ ) {
  for ( size_t i = 0; i < ofs_aligned_arr; i++ ) {
    if ( !mrr->first_few[i] ) {
      mrr->first_few[i] = offset;
      return;
    }
  }
  simde__m256i m256i = simde_mm256_load_si256( (void*)(mrr->first_few + ofs_aligned_arr) );
  simde__m256i zero = simde_mm256_set1_epi32( 0 );
  m256i = simde_mm256_cmpeq_epi32( m256i, zero );
  unsigned int mask = simde_mm256_movemask_ps( (simde__m256) m256i );
  if ( mask ) {
    mrr->first_few[ofs_aligned_arr + __tzcnt_u32(mask)] = offset;
    return;
  }

  // So, the `first_few` slots are all occupied.
  {
    // Lock the ma_linked_released_memories.
    uint8_t old_value = 0;
    while ( !atomic_compare_exchange_weak_explicit(&mrr->lock,
          &old_value, -7, memory_order_acq_rel, memory_order_acquire) ) {
      old_value = 0;
    }
  }

  struct ma_linked_released_mem * restrict lrm;
  if ( !mrr->ofs_head ) {
    mrr->ofs_head = offset;
    lrm = sub_arena->smm.data + offset;
    lrm->ofs_next = 0;
    for ( size_t i = 0; i < _arr_elem_cnt(lrm->released_memories); i++ ) {
      lrm->released_memories[i] = 0;
    }
    // Unlock the ma_linked_released_memories
    atomic_store_explicit(&mrr->lock, 0, memory_order_release);
    return;
  }

  lrm = sub_arena->smm.data + mrr->ofs_head;
  // Unlock the ma_linked_released_memories
  atomic_store_explicit(&mrr->lock, 0, memory_order_release);
  while ( -7 ) {
    for ( size_t i = 0; i < _arr_elem_cnt(lrm->released_memories); i++ ) {
      if ( !lrm->released_memories[i] ) {
        lrm->released_memories[i] = offset;
        return;
      }
    }

    if ( !lrm->ofs_next ) {
      lrm->ofs_next = offset;
      lrm = sub_arena->smm.data + lrm->ofs_next;
      lrm->ofs_next = 0;
      for ( size_t i = 0; i < _arr_elem_cnt(lrm->released_memories); i++ ) {
        lrm->released_memories[i] = 0;
      }
      return;
    }

    lrm = sub_arena->smm.data + lrm->ofs_next;
  }

}


//-------------------------------------------------------------------------------------------------
static void release_all( struct ma_sub_arena * restrict sub_arena, uint8_t idx_user )
{
  //uint32_t total_nr_chunks = 0;
  uint32_t nr_chunks_released;

  struct ma_mem_release_recorder * restrict mrr = &sub_arena->arr_lrm[idx_user];

  // I hope the compiler is smart enough to optimize this brute force method
  for ( size_t i = 0; i < _arr_elem_cnt(mrr->first_few); i++ ) {
    if ( mrr->first_few[i] ) {
      enum err_msg err = infos_smm_return_chunks_ofs( &sub_arena->smm, sub_arena,
          ma_internal_malloc, mrr->first_few[i], &nr_chunks_released, LOG2_CACHELINE_LEN );
      switch ( err ) {
        case emsg_no_error:
          break;
        case emsg_no_such_entry:
          fprintf( stderr, "[MA-Debug] smm_return_chunks failed. in release_all (%s)\n",
              arr_emsg_txt[err] );
          default_free( nullptr, sub_arena->smm.data + mrr->first_few[i] );
          break;
        default:
          fprintf( stderr, "[MA-Debug] smm_return_chunks failed. in release_all (%s)\n",
              arr_emsg_txt[err] );
          break;
      }
      //total_nr_chunks += nr_chunks_released;
      mrr->first_few[i] = 0;
    }
  }

  {
    // Lock the ma_linked_released_memories.
    uint8_t zero = 0;
    while ( !atomic_compare_exchange_weak_explicit(&mrr->lock,
          &zero, -7, memory_order_acq_rel, memory_order_acquire) ) {
      zero = 0;
    }
  }

  if ( !mrr->ofs_head ) {
    // Unlock the ma_linked_released_memories
    atomic_store_explicit(&mrr->lock, 0, memory_order_release);
    return;
  }

  struct ma_linked_released_mem * restrict lrm;
  lrm = sub_arena->smm.data + mrr->ofs_head;
  do {
    enum err_msg err;
    for ( size_t i = 0; i < _arr_elem_cnt(lrm->released_memories); i++ ) {
      if ( lrm->released_memories[i] ) {
        err = infos_smm_return_chunks_ofs( &sub_arena->smm, sub_arena, ma_internal_malloc,
            lrm->released_memories[i], &nr_chunks_released, LOG2_CACHELINE_LEN );
        switch ( err ) {
          case emsg_no_error:
            break;
          case emsg_no_such_entry:
            fprintf( stderr, "[MA-Debug] smm_return_chunks failed. in release_all (%s)\n",
                arr_emsg_txt[err] );
            default_free( nullptr, sub_arena->smm.data + lrm->released_memories[i] );
            break;
          default:
            fprintf( stderr, "[MA-Debug] smm_return_chunks failed. in release_all (%s)\n",
                arr_emsg_txt[err] );
            break;
        }
        //total_nr_chunks += nr_chunks_released;
        lrm->released_memories[i] = 0;
      }
    }

    //uint32_t ofs_next = lrm->ofs_next;
    err = infos_smm_return_chunks_ofs( &sub_arena->smm,
        sub_arena, ma_internal_malloc, lrm->ofs_next, &nr_chunks_released, LOG2_CACHELINE_LEN );
    switch ( err ) {
      case emsg_no_error:
        break;
      case emsg_no_such_entry:
        fprintf( stderr, "[MA-Debug] smm_return_chunks(ofs_next) failed. in release_all (%s)\n",
            arr_emsg_txt[err] );
        default_free( nullptr, sub_arena->smm.data + lrm->ofs_next );
        break;
      default:
        fprintf( stderr, "[MA-Debug] smm_return_chunks(ofs_next) failed. in release_all (%s)\n",
            arr_emsg_txt[err] );
        break;
    }
    //total_nr_chunks += nr_chunks_released;

    //if ( !ofs_next ) {
    if ( !lrm->ofs_next ) {
      break;
    }
    lrm = sub_arena->smm.data + lrm->ofs_next;
  } while ( -7 );
  mrr->ofs_head = 0;

  // Unlock the ma_linked_released_memories
  atomic_store_explicit(&mrr->lock, 0, memory_order_release);
}


//-------------------------------------------------------------------------------------------------
void * ma_realloc( struct ma_sub_arena * sub_arena,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * orig_ptr,
    uint64_t new_length )
{
  void * new_alloc = infos_smm_realloc_chunks( &sub_arena->smm, userdata, custom_malloc,
      orig_ptr, new_length, LOG2_CACHELINE_LEN );
  return new_alloc;
}

//-------------------------------------------------------------------------------------------------
void * ma_realloc_aligned( struct ma_sub_arena * sub_arena,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint64_t new_length,
    uint32_t alignment
    )
{
  void * new_alloc = infos_smm_realloc_aligned_chunks( &sub_arena->smm, userdata, custom_malloc,
      orig_ptr, new_length, max( alignment, CACHELINE_LEN ), LOG2_CACHELINE_LEN );
  return new_alloc;
}


//-------------------------------------------------------------------------------------------------
size_t ma_get_size( memorable_arena_t * restrict arena, void * restrict ptr )
{
  assume( ptr );

  // Look for the right sub_arena
  struct ma_sub_arena * restrict sub_arena;
  for ( sub_arena = arena->head; sub_arena; sub_arena = sub_arena->next ) {
    if ( sub_arena->smm.data <= ptr &&
        ptr <= (sub_arena->smm.data + sub_arena->smm.data_len * CACHELINE_LEN) ) {
      auto offset = ptr2ui((ptr) - sub_arena->smm.data) / CACHELINE_LEN;
      return infos_smm_get_size( &sub_arena->smm, offset );
    }
  }

  return 0;
}


