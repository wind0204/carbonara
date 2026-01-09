#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "err_msg.h"
#include "info_struct.h"
#include "util-c_lang-basic.h"



//-------------------------------------------------------------------------------------------------
struct infos_growing_mem_manager * infos_gmm_init(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t initial_length )
{
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  assume( initial_length );

  // Just in case custom_malloc allocates a 128-bit aligned string for SIMD or
  // a 64-byte aligned string for cache line separation, put data(infos_simple_mem_manager)
  // first up front.

  static_assert( alignof(struct infos_simple_mem_mgr)
      >= alignof(struct infos_growing_mem_manager),
      "I thought both structs were all aligned to pointer size." );
  struct infos_simple_mem_mgr * smm = local_malloc(
      userdata,
      ALIGN_FOR_AVX,
      initial_length
      + sizeof(struct infos_simple_mem_mgr) + sizeof(struct infos_growing_mem_manager)
      );
  smm = infos_smm_init( smm, userdata, custom_malloc,
      initial_length + sizeof(struct infos_simple_mem_mgr) );
  struct infos_growing_mem_manager * new_manager = (void*) (smm + 1);

  struct infos_singly_linked_list_fh * llfh = &new_manager->llf_smm;
  llfh->head.data = smm;
  infos_take_care_of_linked_list_after_putting_head( llfh );

  return new_manager;
}


//-------------------------------------------------------------------------------------------------
void * infos_gmm_rent( struct infos_growing_mem_manager * restrict manager,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    uint32_t growth )
{
  struct infos_singly_linked_list_node * node = &manager->llf_smm.head;
  struct infos_simple_mem_mgr * smm;
  while(-7) {
    smm = node->data;

    void * ptr1 = infos_smm_rent( smm, userdata, custom_malloc, length, 0);
    if ( ptr1 ) {
      return ptr1;
    }

    if ( !node->next ) {
      uint8_t err = infos_gmm_grow(manager, 0, 0, growth);
      if ( !err ) {
        void * ptr2 = infos_smm_rent( smm, userdata, custom_malloc, length, 0 );
        if ( ptr2 ) {
          return ptr2;
        }
      }

      switch ( err ) {
        case emsg_out_of_memory:
          errno = ENOMEM;
          break;
        case emsg_wrong_name_given:
          errno = EBADR;
          break;
        default:
          errno = ENOBUFS;
      }
      return 0;
    }

    node = node->next;
  }
}

//-------------------------------------------------------------------------------------------------
void * infos_gmm_rent_aligned(struct infos_growing_mem_manager * restrict manager,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    uint32_t growth,
    uint_fast8_t alignment )
{
  struct infos_singly_linked_list_node * node = &manager->llf_smm.head;
  struct infos_simple_mem_mgr * smm;
  while(-7) {
    smm = node->data;

    void * ptr1 = infos_smm_rent_aligned(smm, userdata, custom_malloc, length, alignment);
    if ( ptr1 ) {
      return ptr1;
    }

    if ( !node->next ) {
      uint8_t err = infos_gmm_grow(manager, 0, 0, growth);
      if ( !err ) {
        void * ptr2 =
          infos_smm_rent_aligned(node->next->data, userdata, custom_malloc, length, alignment);
        if ( ptr2 ) {
          return ptr2;
        }
      }

      switch ( err ) {
        case emsg_out_of_memory:
          errno = ENOMEM;
          break;
        case emsg_wrong_name_given:
          errno = EBADR;
          break;
        default:
          errno = ENOBUFS;
      }
      return 0;
    }

    node = node->next;
  }
}


//-------------------------------------------------------------------------------------------------
enum err_msg infos_gmm_return( struct infos_growing_mem_manager * restrict manager,
    void * restrict ptr )
{
  struct infos_singly_linked_list_node * node = &manager->llf_smm.head;
  struct infos_simple_mem_mgr * smm;
  while(-7) {
    smm = node->data;

    uint_fast8_t err = infos_smm_return(smm, 0, 0, ptr);
    if ( !err ) {
      return 0;
    }

    node = node->next;
    if ( !node ) {
      return emsg_wrong_name_given;
    }
  }
}


//-------------------------------------------------------------------------------------------------
uint_fast8_t infos_gmm_grow( struct infos_growing_mem_manager * restrict manager,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t growth )
{
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  if ( growth ) {
    struct infos_singly_linked_list_node * tail = &manager->llf_smm.head;
    while ( tail->next ) {
      tail = tail->next;
    }
    tail->next = local_malloc( userdata, alignof(max_align_t),
        growth + sizeof(struct infos_simple_mem_mgr) +
        sizeof(struct infos_singly_linked_list_node) );
    if ( tail->next )
      return emsg_out_of_memory;

    if ( alignof(struct infos_simple_mem_mgr)
        > alignof(struct infos_singly_linked_list_node) ) {
      tail->next->data = tail->next;
      tail->next = tail->next->data + sizeof(struct infos_simple_mem_mgr) + growth;
      assume( growth % alignof(struct infos_singly_linked_list_node) == 0 );
    } else {
      tail->next->data = (void*)(tail->next + 1);
    }
    //tail->next->prev = tail;
    tail->next->next = 0;
    //manager->llf_smm.tail = tail->next;
    manager->llf_smm.nr_nodes++;

    tail = tail->next;
    tail->data = infos_smm_init( tail->data, userdata, custom_malloc,
        growth + sizeof(struct infos_simple_mem_mgr) );
    return 0;

  }

  return emsg_wrong_name_given;
}


//-------------------------------------------------------------------------------------------------
// void * infos_gmm_realloc(struct infos_growing_mem_manager * manager, void * ptr, uint32_t new_length, uint32_t growth) {
void * infos_gmm_realloc( struct infos_growing_mem_manager * restrict manager,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr,
    uint32_t new_length,
    uint32_t growth )
{
  struct infos_singly_linked_list_node * node = &manager->llf_smm.head;
  struct infos_simple_mem_mgr * smm;
  while(-7) {
    smm = node->data;

    void * new_ptr = infos_smm_realloc( smm, userdata, custom_malloc, ptr, new_length );
    if ( new_ptr ) {
      return new_ptr;
    }

    if ( !node->next ) {
      if ( growth ) {
        uint8_t err = infos_gmm_grow(manager, 0, 0, growth);
        switch ( err ) {
          case emsg_out_of_memory:
            errno = ENOMEM;
            return 0;
          case emsg_wrong_name_given:
            errno = EBADSLT;
            return 0;
          case 0:
            break;
          default:
            unreachable();
        }
      } else {
        errno = ENOBUFS;
        return 0;
      }
    }

    node = node->next;
  }
}


//-------------------------------------------------------------------------------------------------
void infos_gmm_destroy( struct infos_growing_mem_manager * restrict manager,
    void * restrict userdata,
    void (custom_free)(void * restrict userdata, void * restrict ptr) )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;

  struct infos_singly_linked_list_node * restrict node = manager->llf_smm.head.next;
  while ( node ) {
    struct infos_singly_linked_list_node * next_node = node->next;
    //uint32_t buflen = ((struct infos_simple_mem_manager*)node->data)->data_len;
    infos_smm_destroy(node->data, userdata, custom_free);
    if ( alignof(struct infos_simple_mem_mgr)
        > alignof(struct infos_singly_linked_list_node) ) {
      local_free( userdata, node->data );
    } else {
      local_free( userdata, node );
    }
    node = next_node;
  }

  //uint32_t buflen = ((struct infos_simple_mem_manager*)manager->llf_smm.head.data)->data_len;
  infos_smm_destroy(manager->llf_smm.head.data, userdata, custom_free);
  local_free( userdata, manager->llf_smm.head.data );
}


