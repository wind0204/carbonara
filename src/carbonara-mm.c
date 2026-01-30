#include <stdatomic.h>
#include <errno.h>

//#ifdef DEBUG_MODE
  #include <stdio.h>
//#endif

#include "carbonara-mm.h"
#include "carbonara-singleton.h"
#include "memorable_arena.h"


//-------------------------------------------------------------------------------------------------
void * cara_malloc( void * cara_malloc_helper, size_t alignment, size_t size )
{
  struct cara_malloc_helper * malloc_helper = cara_malloc_helper;

  // Iterate through the first one
  struct ma_sub_arena * sub_arena;
  sub_arena = malloc_helper->node_recently_used;
  uint_fast16_t idx_pool = sub_arena->idx_sub_arena;
  for ( ; sub_arena != 0; sub_arena = sub_arena->prev, idx_pool-- ) {
    uint8_t old_value = atomic_load_explicit( &sub_arena->lock, memory_order_acquire );
    if ( old_value == malloc_helper->worker_idx + 1 ) {
      // We already own the lock
      struct mm_string * s = ma_aligned_alloc( sub_arena, alignment, size,
          malloc_helper, cara_malloc );
      if ( s ) {
        malloc_helper->node_recently_used = sub_arena;
        return s;
      }
    } else {
      old_value = 0;
      if ( atomic_compare_exchange_strong_explicit(&sub_arena->lock,
            &old_value, malloc_helper->worker_idx + 1,
            memory_order_acq_rel, memory_order_acquire) ) {
        struct mm_string * string = ma_aligned_alloc(
            sub_arena, alignment, size, malloc_helper, cara_malloc );

        atomic_store_explicit(&sub_arena->lock, 0, memory_order_release);

        if ( string ) {
          malloc_helper->node_recently_used = sub_arena;
          return string;
        }
      }
    }
  }

  // Iterate through the last one
  sub_arena = malloc_helper->node_recently_used;
  idx_pool = sub_arena->idx_sub_arena;
  while ( sub_arena->next != 0 ) {
    idx_pool++;
    sub_arena = sub_arena->next;
iterate_toward_tail:
    uint8_t old_value = atomic_load_explicit( &sub_arena->lock, memory_order_acquire );
    if ( old_value == malloc_helper->worker_idx + 1 ) {
      // We already own the lock
      struct mm_string * s = ma_aligned_alloc( sub_arena, alignment, size,
          malloc_helper, cara_malloc );
      if ( s ) {
        malloc_helper->node_recently_used = sub_arena;
        return s;
      }
    } else {
      if ( atomic_compare_exchange_strong_explicit(&sub_arena->lock,
            &old_value, malloc_helper->worker_idx + 1,
            memory_order_acq_rel, memory_order_acquire) ) {
        struct mm_string * s = ma_aligned_alloc( sub_arena, alignment, size,
            malloc_helper, cara_malloc );
        atomic_store_explicit(&sub_arena->lock, 0, memory_order_release);
        if ( s ) {
          malloc_helper->node_recently_used = sub_arena;
          return s;
        }
      }
    }
  }

  // Make a new sub_arena
  uint16_t old_value = idx_pool;
  //typeof(idx_pool) last_idx = idx_pool;
  if ( !atomic_compare_exchange_strong_explicit(&cara_single.mem_arena->nr_nodes,
        &old_value, old_value + 1, memory_order_acq_rel, memory_order_acquire) ) {
    // Someone else has already added a new memo map.
    sub_arena = cara_single.mem_arena->tail;
    goto iterate_toward_tail;
  }

#ifdef DEBUG_MODE
  fprintf(stdout, "Trying to add another memo map to the pool...\n");
#endif

  unsigned int nr_added_pools = ma_add_sub_arena(
      cara_single.proc_info,
      cara_single.mem_arena,
      cara_single.len_each_arena,
      old_value + 1 );
  if ( !nr_added_pools ) {
    atomic_store_explicit(
        &cara_single.mem_arena->nr_nodes,
        old_value,
        memory_order_release );
    errno = errno;
    return nullptr;
  }

  // Allocate in the new pool
  struct mm_string * s = ma_aligned_alloc( cara_single.mem_arena->tail,
      alignment, size, malloc_helper, cara_malloc );

  // Unlock the new pool
  atomic_store_explicit(&cara_single.mem_arena->tail->lock, 0, memory_order_release);

  if ( s ) {
    malloc_helper->node_recently_used = cara_single.mem_arena->tail;
    return s;
  }

  sub_arena = cara_single.mem_arena->tail;
  goto iterate_toward_tail;
}

//-------------------------------------------------------------------------------------------------
void * cara_malloc_vk( void * cara_malloc_helper, size_t size, size_t alignment,
    [[maybe_unused]] VkSystemAllocationScope alloc_scope )
{
  void * ret = cara_malloc( cara_malloc_helper, alignment, size );
  //fprintf( stdout, "Debug.. cara_malloc_vk returns %#018lx\n", (uintptr_t)ret );
  return ret;
}

//-------------------------------------------------------------------------------------------------
void cara_free( void * idx_proc, void * ptr )
{
  ma_free( cara_single.mem_arena, (uintptr_t)idx_proc, ptr );
}

//-------------------------------------------------------------------------------------------------
void cara_free_vk( void * cara_malloc_helper, void * ptr )
{
  //fprintf( stdout, "Debug.. cara_free_vk( %#018lx )\n", (uintptr_t)ptr );
  struct cara_malloc_helper * helper = cara_malloc_helper;
  ma_free( cara_single.mem_arena, helper->worker_idx, ptr );
}

//-------------------------------------------------------------------------------------------------
void * cara_realloc( void * cara_malloc_helper, void * orig_ptr,
    size_t size, size_t alignment, size_t orig_size )
{
  struct cara_malloc_helper * malloc_helper = cara_malloc_helper;
  struct ma_sub_arena * sub_arena;

  if ( !orig_ptr ) {
    return cara_malloc(cara_malloc_helper, alignment, size);
  }

  // Iterate through the first one
  sub_arena = malloc_helper->node_recently_used;
  uint_fast16_t idx_pool = sub_arena->idx_sub_arena;
  for ( ; sub_arena != 0; sub_arena = sub_arena->prev, idx_pool-- ) {
    if ( sub_arena->smm.data <= orig_ptr &&
        orig_ptr <= (sub_arena->smm.data + sub_arena->smm.data_len * CACHELINE_LEN) ) {
      goto obtain_lock;
    }
  }

  // Iterate through the last one
  sub_arena = malloc_helper->node_recently_used;
  idx_pool = sub_arena->idx_sub_arena;
  while ( sub_arena->next != 0 ) {
    idx_pool++;
    sub_arena = sub_arena->next;
//iterate_toward_tail:;

    if ( sub_arena->smm.data <= orig_ptr &&
        orig_ptr <= (sub_arena->smm.data + sub_arena->smm.data_len * CACHELINE_LEN) ) {
      goto obtain_lock;
    }
  }

obtain_lock:
  // Obtain the lock of sub_arena
  {
    uint8_t old_value = atomic_load_explicit( &sub_arena->lock, memory_order_acquire );
    if ( old_value == malloc_helper->worker_idx + 1 ) {
      // We already own the lock
      void * new_ptr = ma_realloc_aligned( sub_arena, cara_malloc_helper, cara_malloc,
          orig_ptr, size, alignment );
      if ( new_ptr ) {
        return new_ptr;
      }
    } else {
      // Obtain the lock
      old_value = 0;
      while ( !atomic_compare_exchange_weak_explicit(&sub_arena->lock,
            &old_value, malloc_helper->worker_idx + 1,
            memory_order_acq_rel, memory_order_acquire) ) {
        old_value = 0;
      }

      void * new_ptr = ma_realloc_aligned( sub_arena, cara_malloc_helper, cara_malloc,
          orig_ptr, size, alignment );
      if ( new_ptr ) {
        // Release the lock and return
        atomic_store_explicit( &sub_arena->lock, 0, memory_order_release );
        return new_ptr;
      }
      atomic_store_explicit( &sub_arena->lock, 0, memory_order_release );
    }
  }


  void * new_ptr = cara_malloc( malloc_helper, alignment, size );
  if ( new_ptr ) {
    memcpy( new_ptr, orig_ptr, orig_size );
    ma_free_simple_ptr( cara_single.mem_arena, malloc_helper->worker_idx, orig_ptr );
    //ma_free_assuming_lock_has_been_obtained( sub_arena, malloc_helper->worker_idx, orig_ptr );
  }
  return new_ptr;
}

//-------------------------------------------------------------------------------------------------
size_t cara_get_size( memorable_arena_t * arena, void * ptr )
{
  return ma_get_size( arena, ptr );
}

//-------------------------------------------------------------------------------------------------
void * cara_realloc_vk( void * cara_malloc_helper,
    void * orig_ptr, size_t size, size_t alignment,
    [[maybe_unused]] VkSystemAllocationScope alloc_scope )
{
  //fprintf( stdout, "Debug.. cara_realloc_vk( %#018lx )\n", (uintptr_t)orig_ptr );
  size_t orig_size = cara_get_size( cara_malloc_helper, orig_ptr );
  void * ret = cara_realloc( cara_malloc_helper, orig_ptr, size, alignment, orig_size );
  //fprintf( stdout, "Debug.. cara_realloc_vk returns %#018lx\n", (uintptr_t)ret );
  return ret;
}


