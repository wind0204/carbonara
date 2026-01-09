#include <malloc.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "memorable_arena.h"
#include "err_msg.h"
#include "util-c_lang-basic.h"
#include "util-c_lang-x86.h"

#ifdef DEBUG_MODE
#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
//#define ID_HEAD_OF_THE_POOL ( 'M' << (4-1)*8 | 'A' << (4-2)*8 | 'n' << (4-3)*8 | 'a' << (4-4)*8 )
constexpr _typeof_member(memorable_arena_t, id) ID_HEAD_OF_THE_POOL =
  ( 'M' << (4-1)*8 | 'A' << (4-2)*8 | 'n' << (4-3)*8 | 'a' << (4-4)*8 );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a new struct ma_sub_arena
 * \return A pointer to a new struct ma_sub_arena or nullptr; `errno` is set to an enum err_msg when
 *          the return value is nullptr
 */
static struct ma_sub_arena * create_new_node(
    uint32_t sub_arena_len,
    uint16_t idx_sub_arena,
    uint64_t proc_info,
    typeof(((memorable_arena_t*)0)->nr_users) nr_users );

//-------------------------------------------------------------------------------------------------
enum err_msg ma_init_arena(
    uint64_t proc_info,
    memorable_arena_t * p_arena,
    uint32_t sub_arena_len,
    typeof(((memorable_arena_t*)0)->nr_users) nr_users
    )
{
  if ( p_arena != 0 ) {
    uint16_t nr_nodes = atomic_load_explicit( &p_arena->nr_nodes, memory_order_acquire );
    if ( p_arena->id == ID_HEAD_OF_THE_POOL && nr_nodes > 0 &&
         p_arena->head->prev == 0 ) {
      uint_fast8_t err = ma_end_arena(p_arena);
      if ( err ) {
        return emsg_get_rid_of_the_old_data_first;
      }
    }
  }

  p_arena->id = ID_HEAD_OF_THE_POOL;
  p_arena->nr_users = nr_users;

  p_arena->head = 0;
  p_arena->tail = 0;

  mallopt( M_MMAP_THRESHOLD, 1024 * 1024 * 32 );
#if !defined( ARE_WE_SHIPPING_IT )
  mallopt( M_CHECK_ACTION, 0b11 );
#else
  mallopt( M_CHECK_ACTION, 0b00 );
#endif

  uint_fast16_t nr_added_pools = ma_add_sub_arena( proc_info, p_arena, sub_arena_len, 1 );
  if ( !nr_added_pools ) {
    return errno;
  }
  // A variable used for communication between threads, 0 is assigned when unlocked.
  atomic_store_explicit( &p_arena->head->lock, 0, memory_order_release );

  atomic_init(&p_arena->nr_nodes, 1);

  return 0;
}


//-------------------------------------------------------------------------------------------------
unsigned int ma_end_arena( memorable_arena_t * arena )
{
  if ( arena->id != ID_HEAD_OF_THE_POOL ) {
    return emsg_corrupted_data_detected;
  }

  struct ma_sub_arena * cur = arena->tail;
  struct ma_sub_arena * prev;
  while(-7) {
    /*
    if ( cur->data->nr_unallocated != cur->data->capacity / (LEN_CACHE_LINE) ) {
      return err_msg_get_rid_of_the_old_data_first;
    }
    */
    prev = cur->prev;
#ifdef DEBUG_MODE
    fprintf(stdout, "[MA] Removing a sub_arena.. (%lx)\n", ptr2ui(cur));
#endif
    ma_free_simple_ptr(arena, 0, cur->arr_lrm);
    free(cur->smm.data);
    atomic_fetch_sub_explicit( &arena->nr_nodes, 1, memory_order_release );

    if ( prev == 0 ) {
      break;
    }

    prev->next = 0;
    cur = prev;
  }

  return 0;
}


//-------------------------------------------------------------------------------------------------
static struct ma_sub_arena * create_new_node(
    uint32_t sub_arena_len,
    uint16_t idx_sub_arena,
    uint64_t proc_info,
    typeof(((memorable_arena_t*)0)->nr_users) nr_users )
{
  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = proc_info;

  size_t large_page_size = page_sizes[x86_proc_info.large_page_size_class];
  assume( _is_aligned_to_power_of_two(sub_arena_len, large_page_size) );

  void * p_alloc = nullptr;
  uint32_t cur_len_sub_arena = sub_arena_len;
  while ( 1 ) {
    p_alloc = aligned_alloc(large_page_size, cur_len_sub_arena);
    if ( p_alloc != nullptr ) {
      break;
    }

    cur_len_sub_arena = cur_len_sub_arena - page_sizes[x86_proc_info.large_page_size_class];
    if ( cur_len_sub_arena < sub_arena_len - sub_arena_len/4 ) {
      errno = emsg_out_of_memory;
      return nullptr;
    }
  }
  madvise( p_alloc, cur_len_sub_arena, MADV_HUGEPAGE );

#ifdef DEBUG_MODE
  if ( sub_arena_len != cur_len_sub_arena ) {
    fprintf(stdout, "[MA] Allocated %u bytes of memory instead of %u bytes\n",
        cur_len_sub_arena, sub_arena_len);
  } else {
    fprintf(stdout, "[MA] Allocated %u bytes of memory\n", cur_len_sub_arena);
  }
#endif

  struct ma_sub_arena * sub_arena = p_alloc + cur_len_sub_arena - sizeof(struct ma_sub_arena);

#ifdef DEBUG_MODE
  fprintf(stdout, "[MA] new 0x%02x node: [%#016lx]\n", idx_sub_arena, ptr2ui(sub_arena));
#endif

  // The length of the memory pool, not including the struct.
  cur_len_sub_arena = cur_len_sub_arena - sizeof(struct ma_sub_arena);

  struct infos_simple_mem_mgr * restrict p_smm = &sub_arena->smm;
  p_smm->data = p_alloc;

  infos_smm_init_chunks_prealloc( p_smm, cur_len_sub_arena, 0 );
#ifdef DEBUG_MODE
  fprintf( stdout, "[MA] ma create_new_node %lx\n", ptr2ui(sub_arena->smm.data) );
#endif

  // Theoretical maximum capacity and the number of free chunks
  //sub_arena->capacity = cur_len_sub_arena - p_smm->total_space_left * CACHELINE_LEN;
  //sub_arena->nr_free_chunks = sub_arena->capacity >> LOG2_CACHELINE_LEN;
  //sub_arena->capacity = p_smm->total_space_left * CACHELINE_LEN;
  sub_arena->capacity = p_smm->total_space_left;

  sub_arena->idx_sub_arena = idx_sub_arena;

  // Prepare the linked queues
  sub_arena->arr_lrm = (void *) ma_malloc(
      sub_arena,
      _get_nr_units_to_cover(sizeof(struct ma_mem_release_recorder) * nr_users, CACHELINE_LEN),
      0,
      0
      );
  if ( !sub_arena->arr_lrm ) {
    fprintf( stderr, "[MA] Out of memory\n" );
    errno = emsg_out_of_memory;
    return nullptr;
  }
  for ( int i = 0; i < nr_users; i++ ) {
    struct ma_mem_release_recorder * mrr = &sub_arena->arr_lrm[i];
    atomic_init( &mrr->lock, 0 );
    mrr->ofs_head = 0;
    mrr->nr_released_memories = 0;
    mrr->idx_sub_arena = idx_sub_arena;
    memset( mrr->first_few, 0, sizeof(mrr->first_few) );
  }


  // A variable used for communication between threads, 0 is assigned when unlocked.
  //atomic_init( &sub_arena->lock, 0 );
  atomic_init( &sub_arena->lock, UINT_MAX_(sub_arena->lock) );

#ifdef DEBUG_MODE
  fprintf(stdout, "[MA] sub_arena->smm.data: [%#016lx]\n", ptr2ui(sub_arena->smm.data));
  fprintf(stdout, "[MA] The theoretical maximum capacity is: %u chunks. (%lu KiB)\n",
      sub_arena->capacity, sub_arena->capacity * (uint64_t)CACHELINE_LEN / 1024);
#endif

  sub_arena->is_allocating = 0;

  return sub_arena;
}


//-------------------------------------------------------------------------------------------------
unsigned int ma_add_sub_arena(
    uint64_t proc_info,
    memorable_arena_t * p_arena,
    uint32_t sub_arena_len,
    unsigned int desired_last_idx )
{
  assume( p_arena->id == ID_HEAD_OF_THE_POOL );
    //errno = err_msg_corrupted_data_detected;
    //return 0;
  //}

  struct ma_sub_arena * prev_sub_arena = nullptr;
  struct ma_sub_arena * cur_sub_arena = p_arena->head;
  uint_fast16_t idx_node = 1;
  uint_fast8_t nr_new_nodes = 0;

  do {
    if ( cur_sub_arena == 0 ) {
      struct ma_sub_arena * new_node = create_new_node(
          sub_arena_len, idx_node, proc_info, p_arena->nr_users);
      if ( !new_node ) {
        errno = errno;
        return nr_new_nodes;;
      }

      nr_new_nodes++;
      new_node->next = 0;

      // When there is no head
      if ( idx_node == 1 ) {
        new_node->prev = 0;
        p_arena->head = new_node;
      } else {
        new_node->prev = prev_sub_arena;
        prev_sub_arena->next = new_node;
      }

      p_arena->tail = new_node;
      cur_sub_arena = new_node;
    }

    prev_sub_arena = cur_sub_arena;
    cur_sub_arena = prev_sub_arena->next;

    idx_node++;

  } while ( idx_node <= desired_last_idx );

  return nr_new_nodes;
}


//-------------------------------------------------------------------------------------------------
enum err_msg ma_remove_sub_arena( memorable_arena_t * arena, uint8_t idx_sub_arena )
{
  if ( arena->id != ID_HEAD_OF_THE_POOL ) {
    return emsg_corrupted_data_detected;
  }

  uint_fast8_t idx = 1;

  struct ma_sub_arena * cur = arena->head;
  struct ma_sub_arena * next;
  while(-7) {
    next = cur->next;

    if ( cur->smm.total_space_left != cur->capacity ) {
      return emsg_get_rid_of_the_old_data_first;
    }

    if ( idx == idx_sub_arena ) {
      if ( cur == arena->head ) {
        arena->head = next;
        if ( next ) {
          next->prev = 0;
        }
      } else {
        if ( next ) {
          next->prev = cur->prev;
        }
        cur->prev->next = next;
      }

      free(cur);
      return emsg_no_error;
    }

    if ( next == 0 ) {
      return emsg_out_of_index;
    }

    if ( !next ) {
      return emsg_not_found;
    }
    cur = next;
  }

}


//-------------------------------------------------------------------------------------------------
struct ma_sub_arena * ma_get_sub_arena( memorable_arena_t * arena, uint8_t idx_sub_arena )
{
  struct ma_sub_arena * cur = arena->head;
  uint_fast8_t idx = 1;
  while(-7) {
    if ( idx == idx_sub_arena ) {
      return cur;
    }

    idx++;
    cur = cur->next;

    if ( !cur ) {
      errno = emsg_out_of_index;
      return 0;
    }
  }
}


