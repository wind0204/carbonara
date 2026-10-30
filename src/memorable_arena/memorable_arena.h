#if !defined MEM_ARENA_H_
#define MEM_ARENA_H_


#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <vulkan/vulkan_core.h>
#include <SDL3/SDL_asyncio.h>

#include "util-c_lang-basic.h"
#include "info_struct.h"


//-------------------------------------------------------------------------------------------------
#define MA_MIN_LEN_CACHE_LINE 64
#if CACHELINE_LEN < MA_MIN_LEN_CACHE_LINE
  #error CACHELINE_LEN is below MA_MIN_LEN_CACHE_LINE
#endif

/**
 * \brief Arena of heap memory
 */
typedef struct {
  struct ma_sub_arena * head;
  uint32_t id;
  uint8_t nr_users;   ///< The number of users/threads
  //char padding[ _get_nr_bytes_for_struct_padding(CACHELINE_LEN,
  //    sizeof(void*) + sizeof(uint32_t) + sizeof(uint8_t)) ];
  alignas(CACHELINE_LEN)
  struct ma_sub_arena * tail;  ///< Obviously the last ma_sub_arena in the linked list
  static_assert( alignof(void*) >= alignof(atomic_uint_least16_t), "Sort the struct members!" );
  atomic_uint_least16_t nr_nodes;  ///< The user should update this value themself whenever they are
                                   ///<  about to call ma_add_sub_arena or ma_remove_sub_arena.
} memorable_arena_t;

/**
 * \brief One of the linked list of memory pools for memorable_arena_t
 */
struct ma_sub_arena {
  struct ma_mem_release_recorder * arr_lrm;  ///< an array of [nr_users] structs
                                                 ///<  ma_linked_released_memory.
  struct ma_sub_arena * restrict next;
  struct ma_sub_arena * restrict prev;
  uint32_t capacity;  ///< The maximum capacity in units of cache-line sized chunks.
  uint16_t idx_sub_arena;  ///< The index number of the sub_arena itself, starts from 1.
  uint16_t userdata;  // This field is not used/touched by memorable_arena
  void * padding[4];  // This field is not used/touched by memorable_arena

  // 👆 Static

  // Not static
  struct infos_simple_mem_mgr smm;  ///< We utilize the code from info_struct-smm.c
  //uint32_t nr_free_chunks;  ///< The number of unallocated cache-line sized chunks left.
  static_assert( alignof(struct infos_simple_mem_mgr) >= alignof(atomic_uint_least8_t),
      "Sort the struct members!" );
  atomic_uint_least8_t lock; ///< A simple lock; 0 is assigned when unlocked, `idx_user+1` is
                              ///<  assigned otherwise.
  uint8_t is_allocating;
};
static_assert( _is_aligned_to_power_of_two(
      sizeof(struct ma_sub_arena), CACHELINE_LEN ), "Align the struct to cacheline?" );

//struct ma_linked_sub_arena {
//  struct ma_linked_sub_arena * prev;
//  struct ma_linked_sub_arena * next;
//  struct ma_sub_arena sub_arena;
//};

struct ma_linked_released_mem {
  uint32_t ofs_next;
  uint32_t released_memories[ (CACHELINE_LEN - sizeof(uint32_t)) / sizeof(uint32_t) ];  ///< Each
                                                      ///< field contains the offset  i.e.
                                                      ///< (uintptr_t)(ptr) - ma_sub_arena.smm.data
};

struct ma_mem_release_recorder {
  uint32_t ofs_head;  ///< The first ma_linked_released_memory
                      ///< ( (uintptr_t)ptr2ma_linked_released_memory - ma_sub_arena.smm.data )
  uint32_t nr_released_memories;
  uint16_t idx_sub_arena;
  atomic_uint_least8_t lock;  ///< A simple lock for the data corresponds to ofs_head.
                              ///<  0 is assigned when unlocked

  // Just an unnecessary explicit struct padding
  char manual_padding[ _get_nr_bytes_for_struct_padding(alignof(uint32_t),
      2*sizeof(uint32_t) + sizeof(uint16_t) + sizeof(atomic_uint_least8_t)) ];

  // The first few released strings' offsets which won't be taken care of by other users unlike the
  // strings' offsets written in the struct pointed by ofs_head
  uint32_t first_few[ _get_nr_bytes_for_struct_padding( CACHELINE_LEN,
      2*sizeof(uint32_t) + sizeof(uint16_t) + sizeof(atomic_uint_least8_t)
      + _get_nr_bytes_for_struct_padding(
        alignof(uint32_t), 2*sizeof(uint32_t)
        + sizeof(uint16_t) + sizeof(atomic_uint_least8_t)) ) / sizeof(uint32_t) ];
};
static_assert( _is_aligned_to_power_of_two(
      sizeof(struct ma_mem_release_recorder), CACHELINE_LEN ), "Align structs to cachelines?" );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize the memory management by creating the first sub_arena.
 * If there is any existing memory pool, the data in the pool will be lost.
 *
 * \param sub_arena_len The size of the first sub_arena in bytes
 * \param nr_users Worker thread count. It affects the number of ma_group_of_strings_to_release
 *    in free_strings field of struct ma_sub_arena
 * \param proc_info a union ucl_x86_proc_info
 * \return 0 if it was successful, an enum err_msg if there was a failure.
 */
enum err_msg ma_init_arena(
    uint64_t proc_info,
    memorable_arena_t * ptr_arena,
    uint32_t sub_arena_len,
    typeof(((memorable_arena_t*)0)->nr_users) nr_users
    );
//#define ma_init_arena(...) _cwps_with_def_val_wo_header( ma_init_arena, V(.sub_arena_len=64*1024*1024), __VA_ARGS__ )
//uint8_t ma_init_arena(uint32_t sub_arena_len, uint16_t nr_users, uint8_t len_cacheline, uint64_t proc_info);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Add new structs of ma_sub_arena to the list until the index of the tail is
 * \warning The new sub-arena pools will be locked with a UINT_MAX.+ value when they've been created;
 *            You should unlock it.
 * \a desired_last_idx The serial no. of the last sub arena pointed by ptr_arena after the operation
 * \return The number of the new pools added. Even when the return value is greater than 0 `errno`
 *          will be set to an enum err_msg when it has failed to reach \a desired_last_idx
 *
 * errno enum err_msg
 *     err_msg_corrupted_data_detected
 *     err_msg_out_of_memory
 */
unsigned int ma_add_sub_arena(
    uint64_t proc_info,
    memorable_arena_t * ptr_arena,
    uint32_t sub_arena_len,
    unsigned int desired_last_idx
    );

//-------------------------------------------------------------------------------------------------
enum err_msg ma_remove_sub_arena( memorable_arena_t * arena, uint8_t idx_sub_arena );

//-------------------------------------------------------------------------------------------------
/**
 * \brief remove every sub_arena from the arena.
 */
unsigned int ma_end_arena( memorable_arena_t * arena );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Get the pointer to a sub_arena from the list.
 *
 * \param idx_sub_arena The index number of the desired sub_arena, it depends on where
 *     the sub_arena is listed in the linked list and starts from 1.
 * \return 0 if it was not successful, or the pointer to the requested struct
 *     ma_sub_arena otherwise.
 *
 * errno an enum err_msg will be set in errno
 */
struct ma_sub_arena * ma_get_sub_arena( memorable_arena_t * arena, uint8_t idx_sub_arena );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate a contiguous string of memory. ( unit: CACHELINE_LEN )
 * \warning You must lock \a sub_arena before requesting a memory allocation.
 *
 * errno ENOMEM if there is no big enough space left for the request
 */
void * ma_malloc( struct ma_sub_arena * sub_arena, uint32_t nr_mem_chunks,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate a contiguous string of memory. ( unit: bytes )
 * \warning You must lock \a sub_arena before requesting a memory allocation.
 *
 * errno ENOMEM if there is no big enough space left for the request
 */
void * ma_malloc_bytes( struct ma_sub_arena * sub_arena, uint32_t nr_bytes,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Try to get an allocation with new size, and if succeded copy the original content to the new allocation.
 * \param new_length The length in bytes you want the new allocation to have
 * \warning You must lock \a sub_arena before calling the function.
 * \return 0 if failed to get a new allocation within \a sub_arena, otherwise the pointer to the new allocation.
 */
void * ma_realloc( struct ma_sub_arena * sub_arena,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * orig_ptr,
    uint64_t new_length
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Get the size of the memory allocation in bytes
 * \return 0 if failed to find the allocation associated with \a orig_ptr
 */
size_t ma_get_size( memorable_arena_t * restrict arena, void * restrict ptr );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Try to get an allocation with new size, and if succeded copy the original content to the new allocation.
 * \param new_length The length in bytes you want the new allocation to have
 * \warning You must lock \a sub_arena before calling the function.
 * \return 0 if failed to get a new allocation within \a sub_arena, otherwise the pointer to the new allocation.
 */
void * ma_realloc_aligned( struct ma_sub_arena * sub_arena,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    void * restrict orig_ptr,
    uint64_t new_length,
    uint32_t alignment
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Allocate a contiguous string of memory with an aligned position.
 * You don't need to call this if you want an n-byte aligned position whre n is
 * less than or equal to the size of a cacheline.
 * You must lock \a sub_arena before requesting a memory allocation.
 *
 * \param alignment The number of bytes to align the position right after the returned
 *     colored_string, it MUST be a power of two.
 *     It should also not be greater than UINT8_MAX since the type of the offset field
 *     is uint8_t
 *
 * errno ENOMEM if there is no big enough space left for the request
 */
void * ma_aligned_alloc(
    struct ma_sub_arena * sub_arena, uint32_t alignment, uint64_t nr_bytes,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    );

//-------------------------------------------------------------------------------------------------
/*
 * \brief Not implemented yet.
 */
void ma_free_simple_ofs_a( memorable_arena_t * restrict arena, uint8_t idx_user,
      uint32_t offset, struct ma_sub_arena * restrict sub_arena );
/*
 * \brief Not implemented yet.
 */
void ma_free_simple_ofs_b( memorable_arena_t * restrict arena, uint8_t idx_user,
    uint32_t offset, _typeof_member(struct ma_sub_arena, idx_sub_arena) idx_sub_arena );

//-------------------------------------------------------------------------------------------------
void ma_free_simple_ptr( memorable_arena_t * restrict arena, uint8_t idx_user, void * restrict ptr );

//-------------------------------------------------------------------------------------------------
void ma_free_assuming_lock_has_been_obtained( struct ma_sub_arena * restrict sub_arena,
    uint8_t idx_user, void * restrict ptr );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Release as many chunks as possible; Release allocated chunks in a struct ma_sub_arena
 *    possibly together with the other chunks queued by others.
 *    You must call it without unlocking the spinlock of the sub_arena \a ptr belongs to.
 * \warning Not implemented yet. WARNING
 */
void ma_free_amap( memorable_arena_t * arena, uint8_t idx_user );

//-------------------------------------------------------------------------------------------------
#define ma_free_3(arena, idx_user, third_arg) _Generic((third_arg), \
    memorable_arena_t * : ma_free_amap, \
    default           : ma_free_simple_ptr \
    )(arena, idx_user, third_arg)
#define ma_free_4(arena, idx_user, offset, sub_arena) _Generic((sub_arena), \
    _typeof_member(struct ma_sub_arena, idx_sub_arena) : ma_free_simple_ofs_b, \
    struct ma_sub_arena * : ma_free_simple_ofs_a, \
    )(arena, idx_user, offset, sub_arena)
#define ma_free(...) VAFUNC(ma_free, __VA_ARGS__)





//
// Why do we have graphics stuff here?
//

//-------------------------------------------------------------------------------------------------

enum vk_buf_types : unsigned int {
  vk_buf_type_unknown = 0,
  vk_buf_type_texarr_2048,
  //vk_buf_type_transfer,
  //vk_buf_type_vert_buf,
  //vk_buf_type_ind_buf,
  vk_buf_type_cnt,
};
struct vk_upload {
  uint32_t real_size;
  uint16_t head_page_idx;
  uint16_t nr_pages;
};
/**
 * \brief Rent an area from uploads, return it when the upload is done.
 */
struct ma_vk_mem_mgr {
  void * p_staging_buf;         // ptr to a mapped memory region for the staging buffer
  VkDeviceSize * arr_usages;    // array of heap_cnt VkDeviceSize
  VkDeviceSize * arr_sizes;     // array of heap_cnt VkDeviceSize
  //uint32_t * arr_idx_next_unoccupied_elem;        // array of heap_cnt uint32_t for managing mal
  uint32_t * arr_heap_indices;  // array of type_cnt uint32_t
  VkMemoryPropertyFlags * arr_mem_property_flags;   // array of type_cnt VkMemoryPropertyFlags
  static_assert( alignof(void*) >= alignof(VkDeviceMemory) );
  VkDeviceMemory staging_buf_malloc;
  static_assert( alignof(VkDeviceMemory) >= alignof(VkDeviceSize) );
  VkDeviceSize buf_granularity;
  static_assert( alignof(VkDeviceSize) >= alignof(VkBuffer) );
  VkBuffer staging_buf;
  static_assert( alignof(VkBuffer) >= alignof(struct infos_multi_arr_list) );
  struct infos_multi_arr_list page_mgrs;  ///< One array list per memory type. It stores
                                          ///< infos_page_mgr_p2 objects for each "chunk of pages"
  struct infos_multi_arr_list mallocs;  ///< One array list per memory type. It stores
                                        ///< VkDeviceMemory objects for paging, each VkDeviceMemory
                                        ///< object is one single "chunk of pages"
  struct infos_page_mgr uploads;  ///< Bookkeeper of staging buffer allocations.
                                  ///< Treat each byte of staging_buf as one page.
  uint32_t heap_cnt;
  uint32_t type_cnt;
  uint32_t len_staging_buf;  ///< The length of the staging buffer in bytes
  uint16_t len_non_coherent_atom;  ///< The size and alignment in bytes that bounds concurrent
                                   ///<  access to host-mapped device memory. The value must be a
                                   ///<  power of two.
  uint8_t arr_mtype_used[vk_buf_type_cnt];
};
constexpr size_t len_chunk_of_pages = 1024ul * 1024 * 64;
constexpr size_t mallocs_node_size = 1024ul * 1024 * 1024 * 8 / len_chunk_of_pages *
  sizeof(VkDeviceMemory);
constexpr size_t page_mgrs_node_size = mallocs_node_size / sizeof(VkDeviceMemory) *
  sizeof(struct infos_page_mgr_p2);

struct vk_mbind_info {
  VkDeviceMemory malloc;
  struct range_u32 range;
};

//-------------------------------------------------------------------------------------------------
/**
 * \brief Start managing VkDeviceMemory objects
 */
enum err_msg ma_init_vk_mem_mgr(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkPhysicalDeviceMemoryProperties2 * restrict p_mem_properties,
    VkPhysicalDeviceLimits * restrict p_limits,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    );

//-------------------------------------------------------------------------------------------------
void ma_destroy_vk_mem_mgr(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkDevice logical_dev,
    void * restrict userdata_free,
    void (*custom_free)(void * restrict userdata, void * restrict ptr),
    void * restrict userdata_malloc,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    );

//-------------------------------------------------------------------------------------------------
enum err_msg ma_create_staging_buf(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkDevice logical_dev,
    unsigned int buf_len
    );

//-------------------------------------------------------------------------------------------------
enum err_msg prototype_create_vk_trans_bufs(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkDevice logical_device,
    unsigned int max_nr_pages,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    //void * (*custom_realloc)(void * userdata, void * orig_ptr, size_t size, size_t alignment)
    );

//-------------------------------------------------------------------------------------------------
enum err_msg ma_prepare_upload_mgr(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    );

//-------------------------------------------------------------------------------------------------
enum err_msg ma_bind_img_to_malloc(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    VkDevice log_dev,
    VkImage vk_img,
    uint32_t type_idx,
    VkDeviceSize req_size,  // Required allocation size in bytes
    VkDeviceSize req_alignment,  // Required allocation alignment in bytes
    struct vk_mbind_info * p_mbind_info
    );
enum err_msg ma_bind_buf_to_malloc(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    VkDevice log_dev,
    VkBuffer vk_buf,
    uint32_t type_idx,
    VkDeviceSize req_size,  // Required allocation size in bytes
    VkDeviceSize req_alignment,  // Required allocation alignment in bytes
    struct vk_mbind_info * p_mbind_info
    );

//-------------------------------------------------------------------------------------------------
enum err_msg ma_report_vk_malloc_rel(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t type_idx,
    struct vk_mbind_info malloc_range
    );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Should I make it a static function? FIXME
 */
uint32_t ma_pick_best_mtype(
    struct ma_vk_mem_mgr * restrict vmm,
    VkMemoryRequirements * restrict req,
    VkMemoryPropertyFlags required_props,
    VkMemoryPropertyFlags preferred_props,
    VkMemoryPropertyFlags tolerable_props
    );



#endif  // MEM_ARENA_H_
