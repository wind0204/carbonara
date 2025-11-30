#if !defined INFO_STRUCT_H
#define INFO_STRUCT_H


#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "func_overloading_and_def_args_and_named_args.h"
#include "util-c_lang-bits.h"
#include "err_msg.h"

#ifdef DEBUG_MODE
//#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
void init_info_struct(uint64_t proc_info);


//-------------------------------------------------------------------------------------------------
struct infos_singly_linked_list {
  struct infos_singly_linked_list_node * head;
  static_assert( alignof(void*) >= alignof(uint64_t), "Sort the struct members!" );
  uint64_t nr_nodes;
};
struct infos_singly_linked_list_with_tail {
  struct infos_singly_linked_list_node * tail;
  static_assert( alignof(void*) >= alignof(uint64_t), "Sort the struct members!" );
  uint64_t nr_nodes;
};
struct infos_singly_linked_list_node {
  struct infos_singly_linked_list_node * next;
  void * data;
};
// infos_singly_linked_list with a fixed head
struct infos_singly_linked_list_fh {
  struct infos_singly_linked_list_node head;
  static_assert( alignof(struct infos_singly_linked_list_node) >= alignof(uint64_t),
      "Sort the struct members!" );
  uint64_t nr_nodes;
};

struct infos_linked_list {
  struct infos_linked_list_node * head;
  struct infos_linked_list_node * tail;
  static_assert( alignof(void*) >= alignof(uint64_t), "Sort the struct members!" );
  uint64_t nr_nodes;
};
struct infos_linked_list_node {
  struct infos_linked_list_node * prev;
  struct infos_linked_list_node * next;
  void * data;
};
// infos_linked_list with fixed head
struct infos_linked_list_fh {
  struct infos_linked_list_node * tail;
  static_assert( alignof(void*) >= alignof(struct infos_linked_list_node),
      "Sort the struct members!" );
  struct infos_linked_list_node head;
  uint64_t nr_nodes;
};
/**
 * \brief Same with struct infos_linked_list_node except that next is _Atomic.
 */
struct infos_linked_list_node_atomic {
  struct infos_linked_list_node_atomic * prev;
  struct infos_linked_list_node_atomic * _Atomic next;
  void * data;
};

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline void infos_take_care_of_tail_next_prev_and_nr_nodes_after_putting_head_uh(
    struct infos_linked_list * ll )
{
  ll->head->next = ll->head->prev = 0;
  ll->tail = ll->head;
  ll->nr_nodes = 1;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline void infos_take_care_of_tail_next_prev_and_nr_nodes_after_putting_head_fh(
    struct infos_linked_list_fh * ll )
{
  ll->head.next = ll->head.prev = 0;
  ll->tail = &ll->head;
  ll->nr_nodes = 1;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline void infos_take_care_of_next_and_nr_nodes_after_putting_head_uh(
    struct infos_singly_linked_list * ll )
{
  ll->head->next = 0;
  ll->nr_nodes = 1;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline void infos_take_care_of_next_and_nr_nodes_after_putting_head_fh(
    struct infos_singly_linked_list_fh * ll )
{
  ll->head.next = 0;
  ll->nr_nodes = 1;
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief no docstring for macro :(
 */
#define infos_take_care_of_linked_list_after_putting_head(ptr_ll) _Generic((ptr_ll),  \
    struct infos_linked_list * :                                                      \
        infos_take_care_of_tail_next_prev_and_nr_nodes_after_putting_head_uh,         \
    struct infos_linked_list_fh * :                                                   \
        infos_take_care_of_tail_next_prev_and_nr_nodes_after_putting_head_fh,         \
    struct infos_singly_linked_list * :                                               \
        infos_take_care_of_next_and_nr_nodes_after_putting_head_uh,                   \
    struct infos_singly_linked_list_fh * :                                            \
        infos_take_care_of_next_and_nr_nodes_after_putting_head_fh                    \
    )(ptr_ll)

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline struct infos_linked_list_node * infos_ll_get_node_uh(
    const struct infos_linked_list * list, uint32_t idx_node )
{
  struct infos_linked_list_node * restrict cur = list->head;
  typeof(idx_node) i = 0;
  while(-7) {
    if ( i == idx_node ) {
      return cur;
    }
    i++;
    cur = cur->next;
    if ( !cur ) {
      return 0;
    }
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline struct infos_linked_list_node * infos_ll_get_node_fh(
    const struct infos_linked_list_fh * list, uint32_t idx_node )
{
  const struct infos_linked_list_node * restrict cur = &list->head;
  typeof(idx_node) i = 0;
  while(-7) {
    if ( i == idx_node ) {
      return (struct infos_linked_list_node *) cur;
    }
    i++;
    cur = cur->next;
    if ( !cur ) {
      return 0;
    }
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline struct infos_singly_linked_list_node * infos_sll_get_node_uh(
    const struct infos_singly_linked_list * list, uint32_t idx_node )
{
  struct infos_singly_linked_list_node * restrict cur = list->head;
  typeof(idx_node) i = 0;
  while(-7) {
    if ( i == idx_node ) {
      return cur;
    }
    i++;
    cur = cur->next;
    if ( !cur ) {
      return 0;
    }
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline struct infos_singly_linked_list_node * infos_sll_get_node_fh(
    const struct infos_singly_linked_list_fh * list, uint32_t idx_node )
{
  const struct infos_singly_linked_list_node * restrict cur = &list->head;
  typeof(idx_node) i = 0;
  while(-7) {
    if ( i == idx_node ) {
      return (struct infos_singly_linked_list_node *) cur;
    }
    i++;
    cur = cur->next;
    if ( !cur ) {
      return 0;
    }
  }
}

//-------------------------------------------------------------------------------------------------
/**
 * \warning I've never tried using these functions but wrote the function-like macro just in case.
 */
#define infos_ll_get_node(ll, idx_node) _Generic((ll),            \
    struct infos_linked_list *  : infos_ll_get_node_uh,           \
    struct infos_linked_list_fh *  : infos_ll_get_node_fh,        \
    struct infos_singly_linked_list *  : infos_sll_get_node_uh,   \
    struct infos_singly_linked_list_fh *  : infos_sll_get_node_fh \
    )(ll, idx_node)

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline struct infos_singly_linked_list_node * infos_ll_get_last_node(
    const struct infos_singly_linked_list_node * head )
{
  const struct infos_singly_linked_list_node * restrict cur = head;
  while(-7) {
    if ( cur->next == 0 ) {
      return (struct infos_singly_linked_list_node *) cur;
    }
    cur = cur->next;
  }
}



//-------------------------------------------------------------------------------------------------
struct range_u32 {
  uint32_t start;
  uint32_t stop;
};

//-------------------------------------------------------------------------------------------------
/**
 * no docstring for macro :(
 */
#define _length_of_range(ptr_range) ( (ptr_range)->stop - (ptr_range)->start + 1 )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Merge two ranges and the range between them as well if exists.
 * Keep the value at \a ptr_lower as is. Use the result at \a ptr_higher.
 */
#define _merge_range_into_higher(ptr_lower, ptr_higher) \
    ( (ptr_higher)->start = (ptr_lower)->start )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Merge two ranges and the range between them as well if exists.
 * Keep the value at \a ptr_higher as is. Use the result at \a ptr_lower.
 */
#define _merge_range_into_lower(ptr_lower, ptr_higher) ( (ptr_lower)->stop = (ptr_higher)->stop )

//-------------------------------------------------------------------------------------------------
/**
 * \brief The number of units available in the range when the head is aligned
 */
#define _aligned_length_of_range(ptr_range, nr_units_to_align_to, lsbs_head_address) \
  ( 1 + (ptr_range)->stop - \
    ( _align_to_power_of_two( (ptr_range)->start + (lsbs_head_address), (nr_units_to_align_to) )\
      - (lsbs_head_address) ) \
  )



//-------------------------------------------------------------------------------------------------

extern void * default_malloc( void * userdata, size_t alignment, size_t size );

/**
 * \warning \a alignment is ignored
 */
extern void * default_realloc( void * userdata, void * orig_ptr, size_t size, size_t alignment );

extern void default_free( void * restrict userdata, void * restrict ptr );



//-------------------------------------------------------------------------------------------------
/**
 * \brief A simple linked list of arrays.
 *    Use functions with names starting with infos_al_.
 */
struct infos_arr_list {
  struct infos_singly_linked_list_fh list;
  uint8_t log2_node_size;
  uint8_t log2_nr_elems;
  uint8_t log2_elem_size;
};
struct infos_multi_arr_list {
  struct infos_singly_linked_list_fh * lists;
  uint8_t log2_node_size;
  uint8_t log2_nr_lists;
  uint8_t log2_nr_elems;
  uint8_t log2_elem_size;
};

//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a new infos_arr_list
 * \return a pointer to struct infos_arr_list which will be in the middle of new memory allocation
 * \param userdata \a custom_malloc is called with this as an argument
 */
struct infos_arr_list * infos_al_create(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t node_size, uint_fast16_t elem_size, bool zero_all
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_al_create_(...) CWDV(infos_al_create, V(0, 0, 4096, sizeof(int), 0), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a new infos_arr_list
 * \return a pointer to struct infos_arr_list which will be in the middle of new memory allocation
 * \param userdata \a custom_malloc is called with this as an argument
 */
struct infos_arr_list * infos_al_create_prealloc(
    struct infos_arr_list * restrict al,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t node_size, uint_fast16_t elem_size, bool zero_all
    );
#define infos_al_create_prealloc_(...) \
  CWDV(infos_al_create_prealloc, V(0, 0, 0, 4096, sizeof(int), 0), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a new infos_multi_arr_list
 * \return a pointer to struct infos_multi_arr_list which will be in the middle of new memory all-
 * ocation.
 * \param userdata \a custom_malloc is called with this as an argument
 */
struct infos_multi_arr_list * infos_multi_al_create(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast8_t log2_node_size, uint_fast8_t log2_elem_size, uint_fast8_t log2_nr_arr_lists,
    bool zero_all
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_multi_al_create_(...) \
  CWDV(infos_multi_al_create, V(0, 0, 4096, sizeof(int), 32, 0), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize a new infos_multi_arr_list using a preallocated struct infos_multi_arr_list
 * \return 0 if it did succeed non-zero otherwise.
 * \param mal a pointer to struct infos_multi_arr_list which will be populated with data
 * \param userdata \a custom_malloc is called with this as an argument
 */
uint_fast8_t infos_multi_al_init_prealloc(
    struct infos_multi_arr_list * restrict mal,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast8_t log2_node_size, uint_fast8_t log2_elem_size, uint_fast8_t log2_nr_arr_lists,
    bool zero_all
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_multi_al_init_prealloc_(...) \
  CWDV(infos_multi_al_init_prealloc, V(0, 0, 0, 4096, sizeof(int), 32, 0), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
void * infos_al_get_elem( const struct infos_arr_list * al, uint32_t idx );

//-------------------------------------------------------------------------------------------------
void * infos_multi_al_get_elem( const struct infos_multi_arr_list * al,
    uint_fast16_t idx_al, uint32_t idx_elem );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Add new nodes
 * \return Non-zero if it couldn't allocate memory, 0 otherwise
 * \param nr_nodes The number of nodes to add
 */
enum err_msg infos_al_grow(
    struct infos_arr_list * restrict al,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast16_t nr_nodes,
    bool zero_all
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_al_grow_(...) CWDV(infos_al_grow, V(0, 0, 0, 1, 0), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
void infos_al_destroy( struct infos_arr_list * restrict al,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );

//-------------------------------------------------------------------------------------------------
void infos_al_destroy_except_preallocated_struct( struct infos_arr_list * restrict al,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Add 1 new node in each array list indexed by arr_len[idx]
 * \return Non-zero if it couldn't allocate memory, 0 otherwise
 */
enum err_msg infos_multi_al_grow( struct infos_multi_arr_list * restrict mal,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint_fast16_t arr_len,
    const uint16_t idx_al[arr_len],
    bool zero_all
    );

//-------------------------------------------------------------------------------------------------
void infos_multi_al_destroy( struct infos_multi_arr_list * restrict mal,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );

//-------------------------------------------------------------------------------------------------
void infos_multi_al_destroy_except_preallocated_struct(
    struct infos_multi_arr_list * restrict mal,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );



//-------------------------------------------------------------------------------------------------
/**
 * \brief A simple memory manager
 */
struct infos_simple_mem_mgr {
  void * data;
  static_assert( sizeof(void*) >= alignof(struct infos_arr_list), "Sort the struct members!" );
  struct infos_arr_list al_rental_info;
  struct infos_arr_list al_vacancies;
  static_assert( sizeof(struct infos_arr_list) >= alignof(uint32_t), "Sort the struct members!" );
  uint32_t data_len;  ///< The length of the buffer pointed by data
                      ///< The value assigned at this field is written in bytes or,
                      ///< you can manually write it in any other units.
                      ///< There are functions with a name that ends with "chunks.*"
                      ///< e.g. infos_smm_rent_chunks, infos_smm_return_chunks,
                      ///<  infos_smm_init_chunks_prealloc
  uint32_t total_space_left;  ///< Like \a data_len, the value assigned at this field is written
                              ///< in bytes but you can manually write it in any units.
  uint32_t idx_last_rental_info;
  uint32_t idx_last_vacancy;
};

//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize using the provided buffer, it calls malloc or custom_malloc for memory
 *    allocation and the memory pool's initial size is
 *    (\a total_len_buf_n_struct - sizeof(struct infos_simple_mem_manager))
 *    not including the size of struct infos_simple_mem_manager
 * \param buffer The pointer to preallocated memory.
 * \param total_len_buf_n_struct The total sum of the lengths of \a buffer and one struct
 *    infos_simple_mem_manager. the length of \a buffer should be a multiple of
 *    alignof(struct infos_simple_mem_manager).
 * \param userdata \a custom_malloc is called with these arguments:
 *    custom_malloc( \a userdata, alignment, \a total_len_buf_n_struct ).
 * \param custom_malloc A malloc function that can allocate large-page aligned memory area.
 * \return The pointer to struct infos_simple_mem_manager
 */
struct infos_simple_mem_mgr * infos_smm_init( void * restrict buffer,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t total_len_buf_n_struct
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize using the provided buffer, it calls malloc or custom_malloc for memory
 *    allocation and the memory pool's initial size is
 *    (\a total_len_buf_n_struct - sizeof(struct infos_simple_mem_manager))
 *    not including the size of struct infos_simple_mem_manager
 * \param buffer The pointer to preallocated memory.
 * \param total_len_buf_n_struct The total sum of the lengths of \a buffer and one struct
 *    infos_simple_mem_manager. the length of \a buffer should be a multiple of
 *    alignof(struct infos_simple_mem_manager).
 * \param userdata \a custom_malloc is called with these arguments:
 *    custom_malloc( \a userdata, alignment, \a total_len_buf_n_struct ).
 * \param custom_malloc A malloc function that can allocate large-page aligned memory area.
 * \return The pointer to struct infos_simple_mem_manager
 */
struct infos_simple_mem_mgr * infos_smm_init_chunks( void * restrict buffer,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t total_len_buf_n_struct, uint8_t log2_chunk_len
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize preallocated struct infos_simple_mem_manager with its `data` points to the pool
 * \param ptr_smm A pointer to the struct, its member `data` should contain the address to the pool.
 * \param pool_size The size of the allocation i.e. ptr_smm->data
 */
void infos_smm_init_chunks_prealloc(
    struct infos_simple_mem_mgr * restrict ptr_smm,
    uint32_t pool_size,
    uint8_t log2_chunk_len
    );

//struct infos_simple_mem_manager * infos_smm_chunkmode_init( void * restrict buffer,
//    void * (*custom_malloc)(size_t alignment, size_t size, void * userdata),
//    void * restrict userdata, uint32_t total_len_buf_n_struct, uint_fast8_t log2_chunk_len
//    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate memory
 * \param length The length of the string which is being allocated in bytes.
 * \return 0 if failed, A pointer if the operation was successful. you
 *    can take a look at the errno value after the failure.
 *
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_smm_rent( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    bool take_from_left
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate memory assuming manager->len_data, availability and rental
 *    information is all written in units of chunks.
 * \param nr_chunks The number of chunks to have allocated, each chunk is
 *    \a 2**len_chunk bytes long.
 * \param log2_chunk_len The length of each chunk expressed in a way where
 *    the length of a chunk is 2**len_chunk_exp.
 * \return 0 if failed, A pointer if the operation was successful. you
 *    can take a look at the errno value after the failure.
 *
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_smm_rent_chunks( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_chunks,
    bool take_from_left,
    uint_fast8_t log2_chunk_len
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_smm_rent_chunks_(...) CWDV(infos_smm_rent_chunks, \
    V(0, 0, 0, 0, 0, LOG2_CACHELINE_LEN), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate aligned memory
 * \param length The length of the string which is being allocated in bytes.
 * \param alignment The number of bytes to align the starting address to.
 *    It must be a power of two and greater than 1.
 * \return 0 if failed, A pointer if the operation was successful. you
 *    can take a look at the errno value after the failure.
 *
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_smm_rent_aligned( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    uint32_t alignment
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_smm_rent_aligned_(...) CWDV(infos_smm_rent_aligned, \
    V(0, 0, 0, 0, alignof(max_align_t)), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate aligned memory
 * \param nr_chunks The number of chunks of memory to allocate
 * \param alignment The number of bytes to align the starting address to.
 *    It must be a power of two and greater than 1.
 * \return 0 if failed, A pointer if the operation was successful. you
 *    can take a look at the errno value after the failure.
 *
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_smm_rent_aligned_chunks( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_chunks,
    uint32_t alignment,
    uint_fast8_t log2_chunk_len
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_smm_rent_aligned_chunks_(...) CWDV(infos_smm_rent_aligned_chunks, \
    V(0, 0, 0, 0, max(alignof(max_align_t), CACHELINE_LEN), 0), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Call .*smm_return when you wanna stop renting a portion of memory.
 * \return err_msg_no_such_entry or err_msg_out_of_memory
 */
enum err_msg infos_smm_return( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr
    );

//-------------------------------------------------------------------------------------------------
enum err_msg infos_smm_return_chunks( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr,
    uint32_t * restrict nr_chunks_released,
    uint32_t log2_chunk_len);
/**
 * \brief no docstring for macro :(
 */
#define infos_smm_return_chunks_(...) CWDV(infos_smm_return_chunks, \
    V(0, 0, 0, 0, 0, LOG2_CACHELINE_LEN), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate aligned memory
 * \param offset = ptr2ui(ptr) - ptr2ui(smm->data)
 */
enum err_msg infos_smm_return_chunks_ofs( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t offset,
    uint32_t * restrict nr_chunks_released,
    uint32_t log2_chunk_len);
/**
 * \brief no docstring for macro :(
 */
#define infos_smm_return_chunks_ofs_(...) CWDV(infos_smm_return_chunks_ofs, \
    V(0, 0, 0, 0, 0, LOG2_CACHELINE_LEN), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Attempt to resize the rented string of memory
 * \return 0 if it was not successful; A pointer if the operation was successful.
 *    you can check the errno value after when it failed.
 * \param orig_ptr The pointer to the rented memory area
 * \param new_length The length in bytes you want the new allocation to have.
 *
 * errno EBADSLT It failed to find the rental information about \a orig_ptr
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_smm_realloc( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Attempt to resize the rented string of memory
 * \return 0 if it was not successful; A pointer if the operation was successful.
 *    you can check the errno value after when it failed.
 * \param orig_ptr The pointer to the rented memory area
 * \param new_length The length in bytes you want the new allocation to have.
 *
 * errno EBADSLT It failed to find the rental information about \a orig_ptr
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_smm_realloc_chunks( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length,
    uint32_t log2_chunk_len
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Attempt to resize the rented string of memory
 * \return 0 if it was not successful; A pointer if the operation was successful.
 *    you can check the errno value after when it failed.
 * \param orig_ptr The pointer to the rented memory area
 * \param new_length The length in bytes you want the new allocation to have.
 *
 * errno EBADSLT It failed to find the rental information about \a orig_ptr
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_smm_realloc_aligned_chunks( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint32_t new_length,
    uint32_t alignment,
    uint32_t log2_chunk_len
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief free al_rental_info and al_available_ranges.
 */
void infos_smm_destroy(struct infos_simple_mem_mgr * smm,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allow smm to touch the part beyond the current length of data
 * \return 0 if it was successful, an enum err_msg otherwise
 * \param total_len_buf_n_struct The total length sum of the buffer and one struct
 * smm
 */
enum err_msg infos_smm_resize_pool( struct infos_simple_mem_mgr * restrict smm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t total_len_buf_n_struct
    );



//-------------------------------------------------------------------------------------------------
/**
 * A simple stack
 */
struct infos_stack {
  void * data;
  uint32_t total_len;
  uint32_t idx_last_elem;  ///< The index of the element that will be popped first,
                           ///  0 if there is no element in stack.
};

//-------------------------------------------------------------------------------------------------
/**
 * \brief Create struct infos_stack using the buffer. \a buf_len will be
 *  max_nr_stack_elements * sizeof(struct func_call_stack_element) + sizeof(struct infos_stack).
 * \warning If \a buf_len is equal to or smaller than sizeof(struct infos_stack) you will see an
 *  undefined behavior.
 */
struct infos_stack * infos_stack_create(void * buffer, uint32_t buf_len);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Push elements into the stack by calling memcpy
 * \return UINT_FAST8_MAX if succeeded, 0 if failed.
 */
uint_fast8_t infos_stack_push(struct infos_stack * restrict stack,
    uint_fast16_t nr_elems, const void * elements, uint_fast8_t log2_elem_len);

//-------------------------------------------------------------------------------------------------
/**
 * \return The element that was pushed into the stack last time
 */
void * infos_stack_pop(struct infos_stack * stack, uint_fast16_t elem_len);


//-------------------------------------------------------------------------------------------------
/**
 * \brief no docstring for macro :(
 */
#define _are_keys_in_front(alignof_key, alignof_value) \
  ( ((alignof_key) >= (alignof_value)) ? -7 : 0 )



//-------------------------------------------------------------------------------------------------
/**
 * \brief A simple growing memory manager which utilizes struct infos_linked_list
 *    and struct simple_mem_mgr.
 */
struct infos_growing_mem_manager {
  struct infos_singly_linked_list_fh llf_smm;
};

//-------------------------------------------------------------------------------------------------
/**
 * \brief Make a struct growing_mem_manager
 * \param initial_length The length of the initial memory pool
 *    It should be a multiple of alignof(struct infos_simple_mem_mgr)
 */
struct infos_growing_mem_manager * infos_gmm_init(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t initial_length );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate a portion of memory for the caller
 * \param growth If \a growth is bigger than 0, try to make another memory pool node if needed.
 *    It'll call malloc passing an argument with the value of (\a growth +
 *    sizeof(struct infos_linked_list_node) + sizeof(struct infos_simple_mem_mgr))
 * \param length The length of the memory to rent in bytes.
 * \return 0 if failed, A pointer to the newly allocated memory if succeeded.
 *
 * errno EBADR The value of \a growth should've been bigger than 0
 *    when it had to grow the memory pool.
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS \a growth was too small in comparison with length;
 */
void * infos_gmm_rent( struct infos_growing_mem_manager * restrict gmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    uint32_t growth
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_gmm_rent_(...) CWDV(infos_gmm_rent, V(0, 0, 0, 4096, 65536), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
void * infos_gmm_rent_aligned(struct infos_growing_mem_manager * restrict gmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t length,
    uint32_t growth,
    uint_fast8_t alignment
    );
/**
 * \brief no docstring for macro :(
 */
#define infos_gmm_rent_aligned_(...) \
  CWDV(infos_gmm_rent_aligned, V(0, 0, 0, 4096, 65536, sizeof(void*)), __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
enum err_msg infos_gmm_return( struct infos_growing_mem_manager * restrict gmm,
    void * restrict ptr );

//-------------------------------------------------------------------------------------------------
uint_fast8_t infos_gmm_grow( struct infos_growing_mem_manager * restrict gmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t growth
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Attempt to resize the rented string of memory
 * \return 0 if it was not successful; A pointer if the operation was successful.
 *   you can check errno value after it failed.
 * \param ptr The pointer to the rented memory area.
 * \param new_length The length you want to change to.
 * \param growth If \a growth is bigger than 0, try to make another memory pool node if needed.
 *                  It'll allocate passing an argument of ( \a growth +
 *                  sizeof(struct infos_linked_list_node) +
 *                  sizeof(struct infos_simple_mem_mgr) )
 *
 * errno EBADSLT It failed to find the rental information about \a ptr
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
void * infos_gmm_realloc( struct infos_growing_mem_manager * restrict gmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict ptr,
    uint32_t new_length,
    uint32_t growth
    );

//-------------------------------------------------------------------------------------------------
void infos_gmm_destroy( struct infos_growing_mem_manager * restrict gmm,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );




//-------------------------------------------------------------------------------------------------
/**
 * \brief Copy a portion of string ahead or behind, without cleaning up
 *    the remnant data in the original position.
 * \param ptr_src The pointer to the portion of data to copy.
 * \param offset The distance between \a ptr_src and the destination. It can be negative.
 *   It shouldn't be 0.
 * \param data_len The length of data to be copied \a offset bytes ahead
 */
__attribute__((always_inline)) [[maybe_unused]]
static void push_string( void * ptr_src, uint32_t data_len, int64_t offset )
{
  assume( offset != 0 );

  const typeof(data_len) move_dist_abs = offset < 0 ? -offset : offset;
  if ( data_len < move_dist_abs ) {
    memcpy(ptr_src + offset, ptr_src, data_len);
  } else {
    memmove(ptr_src + offset, ptr_src, data_len);
  }

  //if ( offset < 0 ) {
  //  if ( data_len < (typeof(data_len)) -offset ) {
  //    memcpy(ptr_src + offset, ptr_src, data_len);
  //  } else {
  //    memmove(ptr_src + offset, ptr_src, data_len);
  //    //uint32_t len_first_batch = -offset * 2;
  //    //memcpy(ptr_src + offset, ptr_src, len_first_batch);
  //    //memcpy(ptr_src + offset + len_first_batch, ptr_src - offset, data_len - len_first_batch);
  //  }
  //} else {
  //  if ( data_len < (typeof(data_len)) offset ) {
  //    memcpy(ptr_src + offset, ptr_src, data_len);
  //  } else {
  //    memmove(ptr_src + offset, ptr_src, data_len);
  //    //uint32_t len_first_batch = data_len - offset;
  //    //memcpy(ptr_src + offset + data_len - len_first_batch,
  //    //    ptr_src + data_len - len_first_batch, len_first_batch);
  //    //memcpy(ptr_src + offset, ptr_src, offset);
  //  }
  //}
}


//-------------------------------------------------------------------------------------------------
/**
 * \brief Copy a string and also copy the separate string of corresponding bitfields aka sepd.
 * \param move_dist The move distance in number of fields (key_slots/bitfields)
 *   It shouldn't be 0.
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds access on the super-poor-64(sp64) array of bit-fields
 */
__attribute__((always_inline)) [[maybe_unused]]
static void push_keys_and_sepd_sp( void * arr_keys, size_t head_key_idx,
    size_t nr_keys_to_move, int64_t move_dist, void * arr_sepd,
    int64_t log2_key_len, uint_fast8_t sepd_len_in_bits )
{
  assume( move_dist != 0 );

  const int64_t key_len_in_bytes = 1 << log2_key_len;
  const int64_t key_move_dist_in_bytes = move_dist << log2_key_len;
  //const size_t move_dist_abs = move_dist < 0 ? -move_dist : move_dist;
  const size_t nr_bytes_moved = nr_keys_to_move << log2_key_len;

  const typeof(nr_bytes_moved) key_move_dist_abs =
    key_move_dist_in_bytes < 0 ? -key_move_dist_in_bytes : key_move_dist_in_bytes;
  //const int64_t bitfield_move_dist = move_dist * sepd_len_in_bits;
  if ( nr_bytes_moved < (typeof(nr_bytes_moved)) key_move_dist_abs ) {
    memcpy(arr_keys + head_key_idx*key_len_in_bytes + key_move_dist_in_bytes,
        arr_keys + head_key_idx*key_len_in_bytes, nr_bytes_moved);
  } else {
    memmove(arr_keys + head_key_idx*key_len_in_bytes + key_move_dist_in_bytes,
        arr_keys + head_key_idx*key_len_in_bytes, nr_bytes_moved);
  }

  if ( 0 ) {  // An opportunity of optimization
  //if ( bitfield_move_dist % 8 == 0 )

  } else {  // if ( bitfield_move_dist % 8 == 0 )

    uint64_t head_bit_idx = (head_key_idx + move_dist) * sepd_len_in_bits;
    uint32_t byte_idx = head_bit_idx / 8;
    if ( sepd_len_in_bits > 32 ) {
      // Use 64-bit masks
      uint64_t cur_idx = head_key_idx;
      uint32_t qword_idx = byte_idx / (64/8);
      const uint32_t threshold = 64 - sepd_len_in_bits;
      uint64_t first_qword = *((uint64_t*)arr_sepd + qword_idx);
      while (-7) {
        //head_bit_idx = (cur_idx + move_dist) * sepd_len_in_bits;
        byte_idx = head_bit_idx / 8;
        const uint32_t pos_msb_in_qword = head_bit_idx % 64;
        uint64_t src_sepd_val = get_bits_sp_u64(arr_sepd, cur_idx, sepd_len_in_bits);
        uint64_t mask = ~0ul;
        mask = mask << (sizeof(mask)*8 - sepd_len_in_bits);
        mask = mask >> pos_msb_in_qword;
        mask = ~mask;
        uint64_t stamp = src_sepd_val;
        stamp = stamp << (sizeof(stamp)*8 - sepd_len_in_bits);
        stamp = stamp >> pos_msb_in_qword;
        first_qword &= mask;
        first_qword |= stamp;

        // Commit to memory when jumping to the next qword.
        if ( pos_msb_in_qword >= threshold ) {
          //to_another_byte_order(0, util_endian_BIG_ENDIAN, &first_qword, sizeof(first_qword));
          *((uint64_t*)arr_sepd + qword_idx) = first_qword;
          qword_idx++;
          uint_fast8_t nr_bits_in_first_qword =  // Avoid 64 because SH[LR] 64 is a no-op.
            sizeof(stamp)*8 - (pos_msb_in_qword?pos_msb_in_qword:1);
          uint64_t stamp_next_qword = src_sepd_val;
          stamp_next_qword = stamp_next_qword << ( sizeof(stamp)*8 - sepd_len_in_bits );
          stamp_next_qword = stamp_next_qword << nr_bits_in_first_qword;
          first_qword = *((uint64_t*)arr_sepd + qword_idx);
          first_qword = first_qword << (sepd_len_in_bits - nr_bits_in_first_qword);
          first_qword = first_qword >> (sepd_len_in_bits - nr_bits_in_first_qword);
          first_qword |= stamp_next_qword;
        }

        cur_idx++;
        head_bit_idx += sepd_len_in_bits;
        if ( cur_idx >= nr_keys_to_move + head_key_idx ) {
          break;
        }
      }

    } else {  // if ( sepd_len_in_bits > 32 )
      // Use 32-bit masks

      uint32_t cur_idx = head_key_idx;
      uint32_t dword_idx = byte_idx / (32/8);
      const uint32_t threshold = 32 - sepd_len_in_bits;
      uint32_t first_dword = *((uint32_t*)arr_sepd + dword_idx);
      while (-7) {
        //head_bit_idx = (cur_idx + move_dist) * sepd_len_in_bits;
        byte_idx = head_bit_idx / 8;
        const uint32_t pos_msb_in_dword = head_bit_idx % 32;
        uint32_t src_sepd_val = get_bits_sp_u32(arr_sepd, cur_idx, sepd_len_in_bits);
        uint32_t mask = ~0;
        mask = mask << (sizeof(mask)*8 - sepd_len_in_bits);
        mask = mask >> pos_msb_in_dword;
        mask = ~mask;
        uint32_t stamp = src_sepd_val;
        stamp = stamp << (sizeof(stamp)*8 - sepd_len_in_bits);
        stamp = stamp >> pos_msb_in_dword;
        first_dword &= mask;
        first_dword |= stamp;

        // Commit to memory when jumping to the next dword.
        if ( pos_msb_in_dword >= threshold ) {
          //to_another_byte_order(0, util_endian_BIG_ENDIAN, &first_dword, sizeof(first_dword));
          *((uint32_t*)arr_sepd + dword_idx) = first_dword;
          dword_idx++;
          uint_fast8_t nr_bits_in_first_dword =  // Avoid 32 because SH[LR] 32 is a no-op.
            sizeof(stamp)*8 - (pos_msb_in_dword?pos_msb_in_dword:1);
          uint32_t stamp_next_dword = src_sepd_val;
          stamp_next_dword = stamp_next_dword << ( sizeof(stamp)*8 - sepd_len_in_bits );
          stamp_next_dword = stamp_next_dword << nr_bits_in_first_dword;
          first_dword = *((uint32_t*)arr_sepd + dword_idx);
          first_dword = first_dword << (sepd_len_in_bits - nr_bits_in_first_dword);
          first_dword = first_dword >> (sepd_len_in_bits - nr_bits_in_first_dword);
          first_dword |= stamp_next_dword;
        }

        cur_idx++;
        head_bit_idx += sepd_len_in_bits;
        if ( cur_idx >= nr_keys_to_move + head_key_idx ) {
          break;
        }
      }
    }  // if ( sepd_len_in_bits > 32 ) {} else ..

  }  // if ( bitfield_move_dist % 8 == 0 ) {} else ..

}



//-------------------------------------------------------------------------------------------------
/**
 * \brief A sorted array that provides a binary search tree. it's located behind its own member
 *   `keys`, and there is also an array of compact bitfields hiding behind struct infos_bst which
 *   represents the handles/indices of the keys.--Each index shouldn't be ~0 which is effectively
 *   represented by `2 << height_cap - 1`. A value of ~0 wouldn't cause any error by itself but
 *   there will be confusion due to the fact that the search function returns ~0 when the tree
 *   doesn't contain the target key value.
 */
struct infos_bst {
  void * keys;  ///< Each key must not be 0 because 0 means it's empty.
  //void * data;  ///< A pointer to user's array of data structs
  uint32_t nr_active_nodes;  ///< The number of non-null elements in keys
  uint8_t key_len;      ///< The length of each element in keys in units of byte
  uint8_t height_cap;   ///< How tall the tree can get, height is 0 when there is no child.
  //uint8_t min_depth;    ///< The depth of the shallowest leaf node, 0 if there's no leaf node.
  uint8_t max_active_depth;  ///< The depth of the deepest leaf node i.e. The height of the tree.
                      /// \warning The internal code keeps the length of the array 3 or above as
                      /// though the height of the tree never goes below 1--So be careful not to
                      /// pass 0 as the parameter to infos_bst_locate_(root|bottom_right_corner).
  uint8_t ofs_idx_buf;  ///< The distance between the array of indices and the struct itself
};
typedef _typeof_member(struct infos_bst, nr_active_nodes) bst_node_idx_t;

//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a binary search tree
 */
struct infos_bst * infos_bst_create( //void * restrict data,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    //void (*custom_free)( void * restrict userdata, void * restrict ptr ),
    const void * restrict keys,
    bst_node_idx_t nr_active_nodes,
    bst_node_idx_t max_nr_nodes,
    uint8_t key_len  ///< The length of each key in bytes
    );
/**
 * \brief no docstring for macros :(
 */
#define infos_bst_create_(...) \
  _Pragma ( "GCC diagnostic push" ) \
  _Pragma ( "GCC diagnostic ignored \"-Wnon-literal-null-conversion\"" ) \
  CWDV( infos_bst_create, V(0, 0, 0, 0, 512-1, sizeof(void*)), __VA_ARGS__ ) \
  _Pragma ( "GCC diagnostic pop" )

//-------------------------------------------------------------------------------------------------
void infos_bst_destroy(struct infos_bst * restrict tree,
    void * restrict userdata,
    void (*custom_free)( void * restrict userdata, void * restrict ptr )
    );

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_calc_capacity( uint_fast8_t height_cap )
{
  return (2 << height_cap) - 1;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_tr_down_left( bst_node_idx_t cur_node_idx,
    uint_fast8_t cur_node_height )
{
  return cur_node_idx - (1 << (cur_node_height - 1));
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_tr_down_right( bst_node_idx_t cur_node_idx,
    uint_fast8_t cur_node_height )
{
  return cur_node_idx + (1 << (cur_node_height - 1));
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_tr_up_from_left_child( bst_node_idx_t cur_node_idx,
    uint_fast8_t cur_node_height )
{
  return cur_node_idx + (1 << cur_node_height);
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_tr_up_from_right_child( bst_node_idx_t cur_node_idx,
    uint_fast8_t cur_node_height )
{
  return cur_node_idx - (1 << cur_node_height);
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_tr_up( bst_node_idx_t cur_node_idx,
    uint_fast8_t cur_node_height )
{
  bst_node_idx_t ret;
  bst_node_idx_t mask = 2 << cur_node_height;
  int is_on_right_of_parent = cur_node_idx & mask;
  if ( is_on_right_of_parent ) {
    ret = cur_node_idx - (1 << cur_node_height);
  } else {
    ret = cur_node_idx + (1 << cur_node_height);
  }
  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_tr_next( bst_node_idx_t cur_node_idx,
    uint_fast8_t cur_node_height )
{
  return cur_node_idx + (2 << cur_node_height);
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_tr_prev( bst_node_idx_t cur_node_idx,
    uint_fast8_t cur_node_height )
{
  return cur_node_idx - (2 << cur_node_height);
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_locate_first_node_in_row( uint_fast8_t row_height )
{
  return (1 << row_height) - 1;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_locate_node( uint_fast8_t node_height,
    bst_node_idx_t node_idx_in_row )
{
  return (2 << node_height) * node_idx_in_row + infos_bst_locate_first_node_in_row(node_height);
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Locate the index of the root/roof
 */
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_locate_root( uint_fast8_t tree_height )
{
  return infos_bst_locate_first_node_in_row(tree_height);
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_locate_bottom_right_corner( uint_fast8_t tree_height )
{
  return (2 << tree_height) - 2;
  //return infos_bst_locate_node(0, (1 << tree_height) - 1);
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint_fast8_t infos_bst_get_height( bst_node_idx_t node_idx )
{
  return _tzcnt_u32(~node_idx);
}

//-------------------------------------------------------------------------------------------------
void infos_bst_print( const struct infos_bst * tree, uint_fast8_t nr_digits );

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_insert_u64( struct infos_bst * tree,
    uint64_t new_key, bst_node_idx_t val );
bst_node_idx_t infos_bst_insert_u32( struct infos_bst * tree,
    uint32_t new_key, bst_node_idx_t val );
bst_node_idx_t infos_bst_insert_u16( struct infos_bst * tree,
    uint16_t new_key, bst_node_idx_t val );
bst_node_idx_t infos_bst_insert_u8( struct infos_bst * tree,
    uint8_t new_key, bst_node_idx_t val );

__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_insert_i64( struct infos_bst * tree,
    int64_t new_key, bst_node_idx_t val )
{
  return infos_bst_insert_u64( tree, new_key, val );
}

__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_insert_i32( struct infos_bst * tree,
    int32_t new_key, bst_node_idx_t val )
{
  return infos_bst_insert_u32( tree, new_key, val );
}

__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_insert_i16( struct infos_bst * tree,
    int16_t new_key, bst_node_idx_t val )
{
  return infos_bst_insert_u32( tree, new_key, val );
}

__attribute__((always_inline))
static inline bst_node_idx_t infos_bst_insert_i8( struct infos_bst * tree,
    int8_t new_key, bst_node_idx_t val )
{
  return infos_bst_insert_u32( tree, new_key, val );
}

/**
 * \brief no docstring for macros :(
 */
#define infos_bst_insert(tree, new_key, idx_val) _Generic((new_key),  \
    uint64_t  : infos_bst_insert_u64,                                 \
    uint32_t  : infos_bst_insert_u32,                                 \
    uint16_t  : infos_bst_insert_u16,                                 \
    uint8_t  : infos_bst_insert_u8,                                   \
    int64_t  : infos_bst_insert_i64,                                  \
    int32_t  : infos_bst_insert_i32,                                  \
    int16_t  : infos_bst_insert_i16,                                  \
    int8_t  : infos_bst_insert_i8                                     \
    )(tree, new_key, idx_val)


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline)) [[maybe_unused]]
static uint64_t infos_bst_fetch_sepd_u64(
    const struct infos_bst * tree, bst_node_idx_t node_idx )
{
  void * restrict indices = (void*)(tree + 1);
  uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
  return get_bits_sp_u64(indices, node_idx, idx_len_in_bits);
}

__attribute__((always_inline)) [[maybe_unused]]
static uint32_t infos_bst_fetch_sepd_u32(
    const struct infos_bst * tree, bst_node_idx_t node_idx )
{
  void * restrict indices = (void*)(tree + 1);
  uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
  return get_bits_sp_u32(indices, node_idx, idx_len_in_bits);
}

__attribute__((always_inline)) [[maybe_unused]]
static uint16_t infos_bst_fetch_sepd_u16(
    const struct infos_bst * tree, bst_node_idx_t node_idx )
{
  void * restrict indices = (void*)(tree + 1);
  uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
  return get_bits_sp_u16(indices, node_idx, idx_len_in_bits);
}

__attribute__((always_inline)) [[maybe_unused]]
static uint8_t infos_bst_fetch_sepd_u8(
    const struct infos_bst * tree, bst_node_idx_t node_idx )
{
  void * restrict indices = (void*)(tree + 1);
  uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
  return get_bits_sp_u8(indices, node_idx, idx_len_in_bits);
}

/**
 * \brief no docstring for macros :(
 */
#define infos_bst_fetch_sepd_auto( ptr_tree, node_idx ) \
  ( get_bits_sp( (void*)((ptr_tree)+1), (node_idx), (ptr_tree)->height_cap+1 ) )


//-------------------------------------------------------------------------------------------------
void infos_bst_remove_3( struct infos_bst * tree,
    bst_node_idx_t node_idx,
    uint_fast8_t node_height
    );

__attribute__((always_inline))
static inline void infos_bst_remove_2( struct infos_bst * tree, bst_node_idx_t node_idx )
{
  uint_fast8_t node_height = infos_bst_get_height( node_idx );
  infos_bst_remove_3( tree, node_idx, node_height );
}
/**
 * \brief no docstring for macros :(
 */
#define infos_bst_remove(...) VAFUNC(infos_bst_remove, __VA_ARGS__)


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline void infos_bst_change_key_u64(
    struct infos_bst * tree, bst_node_idx_t node_idx, uint64_t new_key )
{
  auto node_height = infos_bst_get_height( node_idx );
  typeof(new_key) sepd = infos_bst_fetch_sepd_auto( tree, node_idx );
  infos_bst_remove_3( tree, node_idx, node_height );
  infos_bst_insert( tree, new_key, sepd );
}

__attribute__((always_inline))
static inline void infos_bst_change_key_u32(
    struct infos_bst * tree, bst_node_idx_t node_idx, uint32_t new_key )
{
#define change_key_template                                           \
  auto node_height = infos_bst_get_height( node_idx );                \
  typeof(new_key) sepd = infos_bst_fetch_sepd_auto( tree, node_idx ); \
  infos_bst_remove_3( tree, node_idx, node_height );                  \
  infos_bst_insert( tree, new_key, sepd );
  change_key_template
}

__attribute__((always_inline))
static inline void infos_bst_change_key_u16(
    struct infos_bst * tree, bst_node_idx_t node_idx, uint16_t new_key )
{
  change_key_template
}

__attribute__((always_inline))
static inline void infos_bst_change_key_u8(
    struct infos_bst * tree, bst_node_idx_t node_idx, uint8_t new_key )
{
  change_key_template
}

__attribute__((always_inline))
static inline void infos_bst_change_key_i64(
    struct infos_bst * tree, bst_node_idx_t node_idx, int64_t new_key )
{
  change_key_template
}

__attribute__((always_inline))
static inline void infos_bst_change_key_i32(
    struct infos_bst * tree, bst_node_idx_t node_idx, int32_t new_key )
{
  change_key_template
}

__attribute__((always_inline))
static inline void infos_bst_change_key_i16(
    struct infos_bst * tree, bst_node_idx_t node_idx, int16_t new_key )
{
  change_key_template
}

__attribute__((always_inline))
static inline void infos_bst_change_key_i8(
    struct infos_bst * tree, bst_node_idx_t node_idx, int8_t new_key )
{
  change_key_template
}
#undef change_key_template

/**
 * \brief no docstring for macros :(
 */
#define infos_bst_change_key( ptr_tree, node_idx, new_key ) _Generic((new_key), \
    uint64_t : infos_bst_change_key_u64,                                        \
    uint32_t : infos_bst_change_key_u32,                                        \
    uint16_t : infos_bst_change_key_u16,                                        \
    uint8_t : infos_bst_change_key_u8,                                          \
    int64_t : infos_bst_change_key_i64,                                         \
    int32_t : infos_bst_change_key_i32,                                         \
    int16_t : infos_bst_change_key_i16,                                         \
    int8_t : infos_bst_change_key_i8                                            \
    )( (ptr_tree), (node_idx), (new_key) )


//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_u64( const struct infos_bst * tree, uint64_t key );
bst_node_idx_t infos_bst_search_u32( const struct infos_bst * tree, uint32_t key );
bst_node_idx_t infos_bst_search_u16( const struct infos_bst * tree, uint16_t key );
bst_node_idx_t infos_bst_search_u8( const struct infos_bst * tree, uint8_t key );
bst_node_idx_t infos_bst_search_i64( const struct infos_bst * tree, int64_t key );
bst_node_idx_t infos_bst_search_i32( const struct infos_bst * tree, int32_t key );
bst_node_idx_t infos_bst_search_i16( const struct infos_bst * tree, int16_t key );
bst_node_idx_t infos_bst_search_i8( const struct infos_bst * tree, int8_t key );
/*
 * \brief Search for the key and find out the sepd/data/idx associated to the key, return UINT_MAX_(bst_node_idx_Type) when the key is not found
 */
#define infos_bst_search(tree, key) _Generic((key), \
    uint64_t  : infos_bst_search_u64,               \
    uint32_t  : infos_bst_search_u32,               \
    uint16_t  : infos_bst_search_u16,               \
    uint8_t  : infos_bst_search_u8,                 \
    int64_t  : infos_bst_search_i64,                \
    int32_t  : infos_bst_search_i32,                \
    int16_t  : infos_bst_search_i16,                \
    int8_t  : infos_bst_search_i8                   \
    )(tree, key)

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_u64(
    const struct infos_bst * tree, uint64_t key );
bst_node_idx_t infos_bst_search_for_node_idx_u32(
    const struct infos_bst * tree, uint32_t key );
bst_node_idx_t infos_bst_search_for_node_idx_u16(
    const struct infos_bst * tree, uint16_t key );
bst_node_idx_t infos_bst_search_for_node_idx_u8(
    const struct infos_bst * tree, uint8_t key );
bst_node_idx_t infos_bst_search_for_node_idx_i64(
    const struct infos_bst * tree, int64_t key );
bst_node_idx_t infos_bst_search_for_node_idx_i32(
    const struct infos_bst * tree, int32_t key );
bst_node_idx_t infos_bst_search_for_node_idx_i16(
    const struct infos_bst * tree, int16_t key );
bst_node_idx_t infos_bst_search_for_node_idx_i8(
    const struct infos_bst * tree, int8_t key );
/**
 * \brief no docstring for macros :(
 */
#define infos_bst_search_for_node_idx(tree, key) _Generic((key),  \
    uint64_t  : infos_bst_search_for_node_idx_u64,                \
    uint32_t  : infos_bst_search_for_node_idx_u32,                \
    uint16_t  : infos_bst_search_for_node_idx_u16,                \
    uint8_t  : infos_bst_search_for_node_idx_u8,                  \
    int64_t  : infos_bst_search_for_node_idx_i64,                 \
    int32_t  : infos_bst_search_for_node_idx_i32,                 \
    int16_t  : infos_bst_search_for_node_idx_i16,                 \
    int8_t  : infos_bst_search_for_node_idx_i8                    \
    )(tree, key)

//-------------------------------------------------------------------------------------------------
void infos_insert_sort_bst( struct infos_bst * tree );



//-------------------------------------------------------------------------------------------------
/**
 *  Hashtable
 */
struct infos_hashtable {
  struct infos_multi_arr_list keys;
  struct infos_multi_arr_list values;
  uint32_t * arr_idx_next_unoccupied_elem;
  uint8_t log2_nr_keys;
};

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline uint32_t hash_func_cormen_mult( uint32_t key, uint_fast8_t log2_nr_buckets )
{
  const double seed = ( sqrt(5.0) - 1.0 ) / 2.0;
  double hash = seed * (double)key;
  uint64_t ret = (uint64_t)( hash - floor(hash) );
  return ret >> (32 - log2_nr_buckets);
}

__attribute__((always_inline))
inline uint32_t hash_func_pi( uint32_t key, uint_fast8_t log2_nr_buckets )
{
  const double seed = floor( M_PI * ((uint64_t)1 << 32) );
  union union64 uni64; uni64.f = seed;
  uint32_t ret = key * (uint32_t)(uni64.u & UINT64_C(0xffffffff));
  return ret >> (32 - log2_nr_buckets);
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Create a new infos_hashtable
 * \return a pointer to struct infos_hashtable which will be in the middle of new
 *  memory allocation
 * \param userdata \a custom_malloc is called with these arguments:
 *  custom_malloc( \a userdata, \a key_arr_len + structs_size )
 * \param log2_nr_buckets It's written in this way: log_2(the number of possible hash values)
 *  i.e. The number of buckets
 *  e.g. 5 for 32 keys, 6 for 64 keys. floor(log2(UINT16_MAX))=15 at maximum
 * \param key_arr_len The size of each key bucket in bytes.
 * \param item_size The size of each value in bytes
 */
struct infos_hashtable * infos_hashtable_create(
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void (*custom_free)(void * restrict userdata, void * restrict ptr),
    uint32_t key_arr_len,
    uint_fast16_t item_size,
    uint8_t log2_nr_buckets
    );
constexpr uint_fast8_t default_log2_nr_buckets = 7;
/**
 * \brief no docstring for macros :(
 */
#define infos_hashtable_create_(...) \
  _Pragma ( "GCC diagnostic push" ) \
  _Pragma ( "GCC diagnostic ignored \"-Wnon-literal-null-conversion\"" ) \
  CWDV( infos_hashtable_create, \
      V(0, 0, 0, 64*sizeof(void*), sizeof(void*), 7), __VA_ARGS__ ) \
  _Pragma ( "GCC diagnostic pop" )

//-------------------------------------------------------------------------------------------------
void infos_hashtable_destroy(
    struct infos_hashtable * restrict table,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );

//-------------------------------------------------------------------------------------------------
/**
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
enum err_msg infos_hashtable_insert_4(
    struct infos_hashtable * restrict table,
    uint32_t key,
    uint32_t bucket_idx,
    const void * restrict data
    );

//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert_u32_4(
    struct infos_hashtable * restrict table,
    uint32_t key,
    uint32_t bucket_idx,
    const uint32_t data
    );

//-------------------------------------------------------------------------------------------------
enum err_msg infos_hashtable_insert_u64_4(
    struct infos_hashtable * restrict table,
    uint32_t key,
    uint32_t bucket_idx,
    const uint64_t data
    );

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline enum err_msg infos_hashtable_insert_3(struct infos_hashtable * table,
    uint32_t key, const void * restrict data )
{
  assume( key != 0 );
  uint32_t hash_value;
  hash_value = hash_func_pi( key, table->keys.log2_nr_lists );

  return infos_hashtable_insert_4( table, key,
      hash_value -
      (1 << table->keys.log2_nr_lists) * (hash_value >> table->keys.log2_nr_lists),
      data );
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline enum err_msg infos_hashtable_insert_u32_3(struct infos_hashtable * table,
    uint32_t key, uint32_t data )
{
  assume( key != 0 );
  uint32_t hash_value;
  //hash_value = hash_func_pi( key, _tzcnt_u16(table->buckets.nr_arr_list_packs * INFOS_AL_PACK_SIZE) );
  hash_value = hash_func_pi( key, table->keys.log2_nr_lists );

  return infos_hashtable_insert_u32_4( table, key,
      hash_value -
      (1 << table->keys.log2_nr_lists) * (hash_value >> table->keys.log2_nr_lists),
      data );
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline enum err_msg infos_hashtable_insert_u64_3(struct infos_hashtable * table,
    uint32_t key, uint64_t data )
{
  assume( key != 0 );
  uint32_t hash_value;
  hash_value = hash_func_pi( key, table->keys.log2_nr_lists );

  return infos_hashtable_insert_u64_4( table, key,
      hash_value -
      (1 << table->keys.log2_nr_lists) * (hash_value >> table->keys.log2_nr_lists),
      data );
}

/**
 * \brief no docstring for macros :(
 */
#define infos_hashtable_insert(...) \
  VAFUNC(infos_hashtable_insert, __VA_ARGS__)
/**
 * \brief no docstring for macros :(
 */
#define infos_hashtable_insert_u32(...) \
  VAFUNC(infos_hashtable_insert_u32, __VA_ARGS__)
/**
 * \brief no docstring for macros :(
 */
#define infos_hashtable_insert_u64(...) \
  VAFUNC(infos_hashtable_insert_u64, __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
enum err_msg infos_hashtable_remove_3( struct infos_hashtable * table,
    uint32_t key,
    uint32_t bucket_idx );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Use the simple Pi-based hasher.
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
__attribute__((always_inline))
static inline enum err_msg infos_hashtable_remove_2(struct infos_hashtable * table,
    uint32_t key )
{
  uint32_t hash_value;
  hash_value = hash_func_pi( key, table->keys.log2_nr_lists );

  return infos_hashtable_remove_3( table, key,
      hash_value -
      (1 << table->keys.log2_nr_lists) * (hash_value >> table->keys.log2_nr_lists) );
}
/**
 * \brief no docstring for macros :(
 */
#define infos_hashtable_remove(...) \
  VAFUNC(infos_hashtable_remove, __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \return A pointer to the value assigned to \a key, or 0 if no matching key-value pair was found
 */
void * infos_hashtable_search_3( const struct infos_hashtable * table,
    uint32_t key,
    uint32_t bucket_idx );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Use the simple Pi-based hasher.
 * \return A pointer to the value assigned to \a key, or 0 if no matching key-value pair was found
 */
__attribute__((always_inline))
static inline void * infos_hashtable_search_2( const struct infos_hashtable * table,
    uint32_t key )
{
  assume( key != 0 );
  uint32_t hash_value;
  hash_value = hash_func_pi( key, table->keys.log2_nr_lists );

  return infos_hashtable_search_3( table, key,
      hash_value -
      (1 << table->keys.log2_nr_lists) * (hash_value >> table->keys.log2_nr_lists) );
}
/**
 * \brief no docstring for macros :(
 */
#define infos_hashtable_search(...) \
  VAFUNC(infos_hashtable_search, __VA_ARGS__)



//-------------------------------------------------------------------------------------------------
/**
 * \brief A page manager, rent some pages, and return them after use.
 */
struct infos_page_mgr {
  struct infos_arr_list al_rental_info;
  struct infos_arr_list al_vacancies;
  static_assert( sizeof(struct infos_arr_list) >= alignof(uint32_t), "Sort the struct members!" );
  uint32_t nr_unused_pages;
  uint32_t idx_last_rental_info;
  uint32_t idx_last_vacancy;
};

//-------------------------------------------------------------------------------------------------
enum err_msg infos_pm_init_prealloc(
    struct infos_page_mgr * restrict ptr_pm,
    uint32_t nr_pages,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    );

//-------------------------------------------------------------------------------------------------
void infos_pm_destroy( struct infos_page_mgr * manager,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Rent a page or a number of pages.
 * \return The pointer to the range_u32
 *
 * bugprone-sizeof-expression
 * errno ENOMEM There was a failure of a memory allocation;
 * errno ENOBUFS There was not enough space left;
 */
struct range_u32 * infos_pm_rent( struct infos_page_mgr * restrict pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t nr_seq_pages_needed,
    bool take_from_left
    );

//-------------------------------------------------------------------------------------------------
enum err_msg infos_pm_return( struct infos_page_mgr * restrict pm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t head_idx
    );





#endif  // INFO_STRUCT_H
