#if !defined SALAMI_MM_H_
#define SALAMI_MM_H_


#include <stdint.h>
#include <stddef.h>
#include "vulkan/vulkan_core.h"
//#include "memorable_arena.h"


//-------------------------------------------------------------------------------------------------
//struct cara_grower_manager {
//  struct mm_grower * grower;
//  //struct infos_arr_list * al_available_ranges;
//  atomic_uint_least32_t cur_write_index;
//};
//struct cara_malloc_helper_grower {
//  struct infos_linked_list_node * node_recently_used;
//};
//struct cara_malloc_helper_dynamic_bound {
//  struct mm_linked_memo_map * node_recently_used;
//  struct ms_worker_info * worker_info;
//};
struct cara_malloc_helper {
  struct ma_sub_arena * node_recently_used;
  uint16_t worker_idx;
  //uint8_t alignment;
};


//-------------------------------------------------------------------------------------------------
/**
 * \brief It calls ma_malloc function internally.
 *
 * \param cara_malloc_helper A pointer to a struct cara_malloc_helper
 * \return The pointer to the newly allocated string which is determined by
 *     the offset_to_locate field of the struct colored_string returned by
 *     mm_malloc, 0 if it was not successful.
 * \see mm_add_pool
 *
 * errno It's forwarded from an internal call of mm_add_pool
 */
void * cara_malloc( void * cara_malloc_helper, size_t alignment, size_t size );

//-------------------------------------------------------------------------------------------------
#define _carbonara_malloc_array( ptr_to_carbonara_malloc_helper,                            \
    ptr_to_the_ptr_to_populate, element_count )                                             \
_Pragma ( "GCC diagnostic push" )                                                           \
_Pragma ( "GCC diagnostic ignored \"-Wgnu-alignof-expression\"" )                           \
  *(ptr_to_the_ptr_to_populate) = (typeof(*(ptr_to_the_ptr_to_populate))) cara_malloc( \
      (ptr_to_carbonara_malloc_helper),                                                     \
      alignof(*(*(ptr_to_the_ptr_to_populate))),                                            \
      (element_count) * sizeof(*(*(ptr_to_the_ptr_to_populate))) )                          \
_Pragma ( "GCC diagnostic pop" )

/*
_Pragma ( "GCC diagnostic push" ) \
_Pragma ( "GCC diagnostic ignored \"-Wgnu-alignof-expression\"" ) \
_Pragma ( "GCC diagnostic ignored \"-Wbugprone-sizeof-expression\"" ) \
  *(ptr_to_the_ptr_to_populate) = (typeof(*(ptr_to_the_ptr_to_populate))) cara_malloc( (ptr_to_carbonara_malloc_helper), \
      alignof(*(*(ptr_to_the_ptr_to_populate))), (element_count) * sizeof(*(*(ptr_to_the_ptr_to_populate))) ) \
_Pragma ( "GCC diagnostic pop" )
*/

//-------------------------------------------------------------------------------------------------
/**
 * \brief Release allocation back to the memorable_arena
 * \a idx_proc Worker thread index
 */
void cara_free( void * idx_proc, void * ptr );

//-------------------------------------------------------------------------------------------------
void * cara_realloc( void * cara_malloc_helper, void * orig_ptr, size_t size, size_t alignment );

//-------------------------------------------------------------------------------------------------
void * cara_malloc_vk( void * cara_malloc_helper,
    size_t size, size_t alignment,
    VkSystemAllocationScope alloc_scope );

//-------------------------------------------------------------------------------------------------
void cara_free_vk( void * cara_malloc_helper, void * ptr );

//-------------------------------------------------------------------------------------------------
void * cara_realloc_vk( void * cara_malloc_helper,
    void * orig_ptr, size_t size, size_t alignment,
    VkSystemAllocationScope alloc_scope );





#endif  // #if !defined SALAMI_MM_H_
