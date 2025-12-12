#include <stdint.h>
#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include "carbonara-graphics.h"
#include "info_struct.h"
#include "memorable_arena.h"
#include "err_msg.h"
#include "util-c_lang-basic.h"
#include "util-spall.h"
//#include "vulkan_functions.h"


//-------------------------------------------------------------------------------------------------
enum err_msg ma_init_vk_mem_mgr(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkPhysicalDeviceMemoryProperties2 * restrict p_mem_properties,
    VkPhysicalDeviceLimits * restrict p_limits,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  // Save numbers.
  p_vmm->heap_cnt = p_mem_properties->memoryProperties.memoryHeapCount;
  p_vmm->type_cnt = p_mem_properties->memoryProperties.memoryTypeCount;
  p_vmm->len_non_coherent_atom = p_limits->nonCoherentAtomSize;
  p_vmm->buf_granularity = p_limits->bufferImageGranularity;
  assume( _is_power_of_two( p_limits->bufferImageGranularity) );
  assume( _is_power_of_two( p_limits->nonCoherentAtomSize) );

  // Get memory allocation for the arrays
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  assume( alignof(VkDeviceSize) > alignof(VkMemoryPropertyFlags) );
  assume( alignof(uint32_t) >= alignof(VkMemoryPropertyFlags) );
  p_vmm->arr_usages = local_malloc(
      userdata,
      alignof(VkDeviceSize),  // alignof(VkDeviceSize) = 8
      p_vmm->heap_cnt * (
        sizeof(p_vmm->arr_usages[0]) +
        sizeof(p_vmm->arr_sizes[0]) +
        0 ) +
      p_vmm->type_cnt * (
        sizeof(p_vmm->arr_heap_indices[0]) +
        sizeof(p_vmm->arr_mem_property_flags[0]) +
        0 )
      );
  if ( !p_vmm->arr_usages ) {
    return err_msg_out_of_memory;
  }
  p_vmm->arr_sizes = p_vmm->arr_usages + p_vmm->heap_cnt;
  p_vmm->arr_heap_indices = (void*)( p_vmm->arr_sizes + p_vmm->heap_cnt );
  p_vmm->arr_mem_property_flags = p_vmm->arr_heap_indices + p_vmm->type_cnt;

  // Cache some properties.
  for ( unsigned int heap_idx = 0; heap_idx < p_vmm->heap_cnt; heap_idx++ ) {
    p_vmm->arr_sizes[heap_idx] =
      p_mem_properties->memoryProperties.memoryHeaps[heap_idx].size;
  }
  for ( unsigned int type_idx = 0; type_idx < p_vmm->type_cnt; type_idx++ ) {
    p_vmm->arr_heap_indices[type_idx] =
      p_mem_properties->memoryProperties.memoryTypes[type_idx].heapIndex;
    p_vmm->arr_mem_property_flags[type_idx] =
      p_mem_properties->memoryProperties.memoryTypes[type_idx].propertyFlags;
  }

  // Assume 0 bytes of heap memory was used by our application.
  for ( unsigned int heap_idx = 0; heap_idx < p_vmm->heap_cnt; heap_idx++ ) {
    p_vmm->arr_usages[heap_idx] = 0;
  }


  //
  // Initialize multi_arr_list which will contain VkDeviceMemory objects and paging information
  //
  assume( _is_power_of_two(mallocs_node_size) );
  infos_multi_al_init_prealloc( &p_vmm->mallocs, userdata, local_malloc,
      get_log2(mallocs_node_size),
      31 - _lzcnt_u32(sizeof(VkDeviceMemory)),
      get_log2( _align_to_the_close_power_of_two(p_vmm->type_cnt) ),
      true );  // Fill with NULL
  assume( _is_power_of_two(page_mgrs_node_size) );
  infos_multi_al_init_prealloc( &p_vmm->page_mgrs, userdata, local_malloc,
      get_log2(page_mgrs_node_size),
      31 - _lzcnt_u32(sizeof(struct infos_page_mgr_p2)),
      get_log2( _align_to_the_close_power_of_two(p_vmm->type_cnt) ),
      true );  // Fill with NULL

  //vmm->uploads.al_rental_info.log2_node_size = 0;
  //vmm->uploads.al_vacancies.log2_node_size = 0;
  p_vmm->p_staging_buf = nullptr;
  p_vmm->staging_buf = 0;
  p_vmm->staging_buf_malloc = 0;
  p_vmm->len_staging_buf = 0;


  // Return 0
  return err_msg_no_error;
}

//-------------------------------------------------------------------------------------------------
void ma_destroy_vk_mem_mgr(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkDevice logical_dev,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;
  local_free( userdata, p_vmm->arr_usages );

  // TODO vkFreeMemory per malloc in vmm->mallocs

  if ( p_vmm->p_staging_buf ) {
    vkUnmapMemory( logical_dev, p_vmm->staging_buf_malloc );
    vkFreeMemory( logical_dev, p_vmm->staging_buf_malloc, &vk_alloc_callbacks ); 
    //vkFreeMemory( logical_dev, vmm->staging_buf_malloc, nullptr ); 
    vkDestroyBuffer( logical_dev, p_vmm->staging_buf, &vk_alloc_callbacks );
    //vkDestroyBuffer( logical_dev, vmm->staging_buf, nullptr );
    p_vmm->p_staging_buf = nullptr;
    p_vmm->staging_buf = 0;
    p_vmm->staging_buf_malloc = 0;
  }

  //if ( vmm->uploads.al_vacancies.log2_node_size ) {
  //  infos_pm_destroy( &vmm->uploads, userdata, custom_free );
  //  vmm->uploads.al_vacancies.log2_node_size = 0;
  //  vmm->uploads.al_rental_info.log2_node_size = 0;
  //}

  //if ( vmm->arr_trans_bufs ) {
  //  // Destroy the transfer buffers and free the memory allocations
  //  const uint32_t heap_idx = vmm->arr_heap_indices[ vmm->arr_best_mtype_idx[vk_buf_type_transfer] ];
  //  auto idx_first_empty_element = vmm->arr_idx_next_unoccupied_elem[heap_idx];
  //  struct infos_singly_linked_list_node * m_node = &vmm->mallocs.lists[heap_idx].head;
  //  for ( typeof(vmm->tb_cnt) malloc_idx = 0; malloc_idx < vmm->tb_cnt; malloc_idx++ ) {
  //    while ( idx_first_empty_element >= UINT64_C(1) << vmm->mallocs.log2_nr_elems ) {
  //      idx_first_empty_element -= UINT64_C(1) << vmm->mallocs.log2_nr_elems;
  //      m_node = m_node->next;
  //    }
  //    VkDeviceMemory * p_alloc = m_node->data + idx_first_empty_element * sizeof(VkDeviceMemory);
  //    vkDestroyBuffer( logical_dev, vmm->arr_trans_bufs[malloc_idx], &cara_vk.vk_alloc_callbacks );
  //    vkFreeMemory( logical_dev, *p_alloc, &cara_vk.vk_alloc_callbacks );
  //  }
  //  local_free( userdata, vmm->arr_trans_bufs );
  //  vmm->arr_trans_bufs = nullptr;
  //}

  p_vmm->arr_usages = nullptr;
  p_vmm->arr_sizes = nullptr;
  p_vmm->arr_heap_indices = nullptr;
  p_vmm->arr_mem_property_flags = nullptr;
}


//-------------------------------------------------------------------------------------------------
enum err_msg ma_create_staging_buf(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkDevice logical_dev,
    unsigned int buf_len
    )
{
  constexpr unsigned int default_buf_len = 1024*1024*64;
  buf_len = buf_len ? buf_len : default_buf_len;

  // Create the buffer
  VkBufferCreateInfo buf_create_info;
  buf_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_create_info.pNext = nullptr;
  buf_create_info.flags = 0;
  buf_create_info.size = buf_len;
  buf_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buf_create_info.queueFamilyIndexCount = 0;
  buf_create_info.pQueueFamilyIndices = nullptr;
  VkResult result = vkCreateBuffer(
      logical_dev, &buf_create_info, &vk_alloc_callbacks, &p_vmm->staging_buf
      );
  if ( result != VK_SUCCESS ) {
    return err_msg_unknown;
  }


  // Obtain the requirements
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements( logical_dev, p_vmm->staging_buf, &req );

  // Choose the best memory type
  VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
  preferred_props = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  required_props =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
    0;
  tolerable_props =
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    0;
  const uint32_t type_idx = ma_pick_best_mtype(
      p_vmm, &req, required_props, preferred_props, tolerable_props );
  p_vmm->arr_best_mtype_idx[vk_buf_type_transfer] = type_idx;


  // Allocate memory for the buffer
  VkMemoryAllocateInfo buf_malloc_info;
  buf_malloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  buf_malloc_info.pNext = nullptr;
  buf_malloc_info.allocationSize = req.size;
  buf_malloc_info.memoryTypeIndex = type_idx;

  result = vkAllocateMemory( logical_dev,
      &buf_malloc_info, &vk_alloc_callbacks, &p_vmm->staging_buf_malloc );
  if ( result != VK_SUCCESS ) {
    vkDestroyBuffer( logical_dev, p_vmm->staging_buf, &vk_alloc_callbacks );
    p_vmm->staging_buf = 0;
    return err_msg_out_of_memory;
  }


  // Bind the buffer to the memory allocation
  result = vkBindBufferMemory( logical_dev, p_vmm->staging_buf, p_vmm->staging_buf_malloc, 0 );
  if ( result != VK_SUCCESS ) {
    vkFreeMemory( logical_dev, p_vmm->staging_buf_malloc, &vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, p_vmm->staging_buf, &vk_alloc_callbacks );
    p_vmm->staging_buf = 0;
    p_vmm->staging_buf_malloc = 0;
    return err_msg_unknown;
  }


  // Map the memory region
  result = vkMapMemory( logical_dev, p_vmm->staging_buf_malloc, 0, buf_len, 0, &p_vmm->p_staging_buf );
  if ( result != VK_SUCCESS ) {
    vkFreeMemory( logical_dev, p_vmm->staging_buf_malloc, &vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, p_vmm->staging_buf, &vk_alloc_callbacks );
    p_vmm->staging_buf = 0;
    p_vmm->staging_buf_malloc = 0;
    p_vmm->p_staging_buf = nullptr;
    return err_msg_unknown;
  }

  // Have the OS actually prepare pages for the mapped memory range
  memset( p_vmm->p_staging_buf, 0, buf_len );

  p_vmm->len_staging_buf = buf_len;

  const uint32_t heap_idx = p_vmm->arr_heap_indices[type_idx];
  p_vmm->arr_usages[heap_idx] += req.size;

  return 0;
}


//-------------------------------------------------------------------------------------------------
/*
enum err_msg prototype_create_vk_trans_bufs(
    struct ma_vk_mem_mgr * vmm,
    VkDevice logical_dev,
    unsigned int max_nr_pages,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    //void * (*custom_realloc)(void * userdata, void * orig_ptr, size_t size, size_t alignment)
    )
{
  constexpr unsigned int arr_elem_cnt = 1024*1024*1024 / sizeof(union vk_page);
  VkBuffer arr_bufs[arr_elem_cnt];
  max_nr_pages = max_nr_pages ? min( max_nr_pages, arr_elem_cnt ) : arr_elem_cnt;
  typeof(vmm->tb_cnt) malloc_cnt = 0;

  // Create the buffer for the first page
  VkBufferCreateInfo buf_create_info;
  buf_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_create_info.pNext = nullptr;
  buf_create_info.flags = 0;
  buf_create_info.size = sizeof(union vk_page);
  buf_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buf_create_info.queueFamilyIndexCount = 0;
  buf_create_info.pQueueFamilyIndices = nullptr;
  VkResult result = vkCreateBuffer(
      logical_dev, &buf_create_info, &cara_vk.vk_alloc_callbacks, &arr_bufs[0] );
  if ( result != VK_SUCCESS ) {
    return err_msg_out_of_resource;
  }

  // Obtain the requirements
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements( logical_dev, arr_bufs[0], &req );
  fprintf( stdout, "[MA-Debug] VkMemoryRequirements .foo=%lu .bar=%u, .size=%lu\n", req.alignment, req.memoryTypeBits, req.size );

  // Choose the best memory type
  VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
  preferred_props = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  required_props =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
    0;
  tolerable_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  const uint32_t type_idx = ma_pick_best_mtype(
      vmm, &req, required_props, preferred_props, tolerable_props );


  // Allocate a memory page for the first memory object
  VkMemoryAllocateInfo buf_malloc_info;
  buf_malloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  buf_malloc_info.pNext = nullptr;
  buf_malloc_info.allocationSize = req.size;
  buf_malloc_info.memoryTypeIndex = type_idx;

  const uint32_t heap_idx = vmm->arr_heap_indices[type_idx];
  auto idx_first_empty_element = vmm->arr_idx_next_unoccupied_elem[heap_idx];
  struct infos_singly_linked_list_node * m_node = &vmm->mallocs.lists[heap_idx].head;
  while ( idx_first_empty_element >= UINT64_C(1) << vmm->mallocs.log2_nr_elems ) {
    idx_first_empty_element -= UINT64_C(1) << vmm->mallocs.log2_nr_elems;
    if ( !m_node->next ) {
      // Make another node.
      uint16_t indices_buckets[] = { heap_idx };
      if ( infos_multi_al_grow(&vmm->mallocs, 0, 0, 1, indices_buckets, 0) ) {
        vkDestroyBuffer( logical_dev, arr_bufs[0], &cara_vk.vk_alloc_callbacks );
        return err_msg_out_of_memory;
      }
    }
    m_node = m_node->next;
  }
  VkDeviceMemory * p_alloc = m_node->data + idx_first_empty_element * sizeof(VkDeviceMemory);

  result = vkAllocateMemory( logical_dev,
      &buf_malloc_info, &cara_vk.vk_alloc_callbacks, p_alloc );
  if ( result != VK_SUCCESS ) {
    vkDestroyBuffer( logical_dev, arr_bufs[0], &cara_vk.vk_alloc_callbacks );
    return err_msg_out_of_memory;
  }


  // Bind the first buffer to the first allocation
  result = vkBindBufferMemory( logical_dev, arr_bufs[0], *p_alloc, 0 );
  if ( result != VK_SUCCESS ) {
    vkDestroyBuffer( logical_dev, arr_bufs[0], &cara_vk.vk_alloc_callbacks );
    vkFreeMemory( logical_dev, *p_alloc, &cara_vk.vk_alloc_callbacks );
    return err_msg_unknown;
  }
  malloc_cnt++;


  // Create as many buffers and allocations as possible and bind the buffers to the allocations
  while( malloc_cnt < max_nr_pages ) {
    // Create the buffer for the next page
    result = vkCreateBuffer(
        logical_dev, &buf_create_info, &cara_vk.vk_alloc_callbacks, &arr_bufs[malloc_cnt] );
    if ( result != VK_SUCCESS ) {
      break;
    }

    idx_first_empty_element++;
    while ( idx_first_empty_element >= UINT64_C(1) << vmm->mallocs.log2_nr_elems ) {
      idx_first_empty_element -= UINT64_C(1) << vmm->mallocs.log2_nr_elems;
      if ( !m_node->next ) {
        // Make another node.
        uint16_t indices_buckets[] = { heap_idx };
        if ( infos_multi_al_grow(&vmm->mallocs, 0, 0, 1, indices_buckets, 0) ) {
          vkDestroyBuffer( logical_dev, arr_bufs[malloc_cnt], &cara_vk.vk_alloc_callbacks );
          // Release every resource
          idx_first_empty_element = vmm->arr_idx_next_unoccupied_elem[heap_idx];
          m_node = &vmm->mallocs.lists[heap_idx].head;
          for ( typeof(malloc_cnt) malloc_idx = 0; malloc_idx < malloc_cnt; malloc_idx++ ) {
            while ( idx_first_empty_element >= UINT64_C(1) << vmm->mallocs.log2_nr_elems ) {
              idx_first_empty_element -= UINT64_C(1) << vmm->mallocs.log2_nr_elems;
              m_node = m_node->next;
            }
            p_alloc = m_node->data + idx_first_empty_element * sizeof(VkDeviceMemory);
            vkDestroyBuffer( logical_dev, arr_bufs[malloc_idx], &cara_vk.vk_alloc_callbacks );
            vkFreeMemory( logical_dev, *p_alloc, &cara_vk.vk_alloc_callbacks );
          }
          return err_msg_out_of_memory;
        }
      }
      m_node = m_node->next;
    }
    p_alloc = m_node->data + idx_first_empty_element * sizeof(VkDeviceMemory);
    result = vkAllocateMemory( logical_dev,
        &buf_malloc_info, &cara_vk.vk_alloc_callbacks, p_alloc );
    if ( result != VK_SUCCESS ) {
      // Destroy the buffer
      vkDestroyBuffer( logical_dev, arr_bufs[malloc_cnt], &cara_vk.vk_alloc_callbacks );
      break;
    }

    // Bind the buffer to the new allocation
    result = vkBindBufferMemory( logical_dev, arr_bufs[malloc_cnt], *p_alloc, 0 );
    if ( result != VK_SUCCESS ) {
      vkDestroyBuffer( logical_dev, arr_bufs[malloc_cnt], &cara_vk.vk_alloc_callbacks );
      vkFreeMemory( logical_dev, *p_alloc, &cara_vk.vk_alloc_callbacks );
      break;
    }
    malloc_cnt++;
  }


  // Get memory allocation for the arrays
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  //void * (*local_realloc)(void * userdata, void * orig_ptr, size_t size, size_t alignment);
  //local_realloc = custom_realloc ? custom_realloc : default_realloc;
  VkBuffer * new_arr = local_malloc( userdata, alignof(*new_arr), malloc_cnt * sizeof(*new_arr) );
  if ( !new_arr ) {
    // Release every resource
    idx_first_empty_element = vmm->arr_idx_next_unoccupied_elem[heap_idx];
    m_node = &vmm->mallocs.lists[heap_idx].head;
    for ( typeof(malloc_cnt) malloc_idx = 0; malloc_idx < malloc_cnt; malloc_idx++ ) {
      while ( idx_first_empty_element >= UINT64_C(1) << vmm->mallocs.log2_nr_elems ) {
        idx_first_empty_element -= UINT64_C(1) << vmm->mallocs.log2_nr_elems;
        m_node = m_node->next;
      }
      p_alloc = m_node->data + idx_first_empty_element * sizeof(VkDeviceMemory);
      vkDestroyBuffer( logical_dev, arr_bufs[malloc_idx], &cara_vk.vk_alloc_callbacks );
      vkFreeMemory( logical_dev, *p_alloc, &cara_vk.vk_alloc_callbacks );
    }
    return err_msg_out_of_memory;
  }

  memcpy( new_arr, arr_bufs, sizeof(VkBuffer) * malloc_cnt );
  vmm->arr_trans_bufs = new_arr;
  vmm->tb_cnt = malloc_cnt;
  vmm->arr_idx_next_unoccupied_elem[heap_idx] = idx_first_empty_element;
  vmm->arr_best_mtype_idx[vk_buf_type_transfer] = type_idx;


  return 0;
}
*/


//-------------------------------------------------------------------------------------------------

enum err_msg ma_prepare_upload_mgr(
    struct ma_vk_mem_mgr * restrict vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  //constexpr size_t max_nr_pages = 1024*1024*1024;
  //constexpr size_t default_nr_pages = 1024*1024*128;
  //nr_pages = nr_pages ? min( max_nr_pages, nr_pages ) : default_nr_pages;
  uint32_t nr_pages = vmm->len_staging_buf;
  enum err_msg err = infos_pm_init_prealloc( &vmm->uploads, nr_pages, userdata, custom_malloc );
  if ( err ) {
    return err;
  }

  return 0;
}


//-------------------------------------------------------------------------------------------------
enum err_msg ma_bind_img_to_malloc(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    VkDevice log_dev,
    VkImage vk_img,
    uint32_t type_idx,
    VkDeviceSize req_size
    )
{
  VkDeviceSize size_to_req = _align_to_power_of_two( req_size, p_vmm->buf_granularity );
  VkResult result;
  enum err_msg err;
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  uint32_t heap_idx = p_vmm->arr_heap_indices[type_idx];
  struct infos_singly_linked_list_node * m_node = &p_vmm->mallocs.lists[type_idx].head;
  struct infos_singly_linked_list_node * p_node = &p_vmm->page_mgrs.lists[type_idx].head;
  const uint32_t last_idx = 1 << p_vmm->mallocs.log2_nr_elems;
  while(7) {
    for ( uint32_t idx = 0; idx < last_idx; idx++ ) {
      VkDeviceMemory * p_malloc = m_node->data + idx * sizeof(VkDeviceMemory);
      struct infos_page_mgr_p2 * p_pm = p_node->data + idx * sizeof(*p_pm);
      if ( *p_malloc == 0 ) {
        // Create a new VkDeviceMemory
        VkMemoryAllocateInfo buf_malloc_info;
        buf_malloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        buf_malloc_info.pNext = nullptr;
        buf_malloc_info.allocationSize = len_chunk_of_pages;
        buf_malloc_info.memoryTypeIndex = type_idx;
        SPALL_BEGIN("alloc_mem");
        result = vkAllocateMemory( log_dev,
            &buf_malloc_info, &vk_alloc_callbacks, p_malloc );
        SPALL_END();
        if ( result != VK_SUCCESS ) {
          fprintf( stderr, "[Carbonara] vkAllocateMemory failed. "
              "'%s'\n", string_VkResult(result) );
          switch ( result ) {
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
              [[fallthrough]];
            case VK_ERROR_OUT_OF_HOST_MEMORY:
              return err_msg_out_of_memory;

            case VK_ERROR_INVALID_EXTERNAL_HANDLE:
              [[fallthrough]];
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR:
              [[fallthrough]];
            case VK_ERROR_VALIDATION_FAILED:
              return err_msg_invalid_request;

            default:
              return err_msg_unknown;
          }
        }

        // Init struct infos_page_mgr_p2
        err = infos_pm_init_prealloc( (struct infos_page_mgr*)p_pm, len_chunk_of_pages,
            userdata, local_malloc );
        if ( err ) {
          fprintf( stderr, "Error occured in infos_pm_init_prealloc. (%s)\n", err_msg_text[err] );
          return err;
        }
      }

      // We must've failed to init struct infos_page_mgr_p2 before this call
      if ( !p_pm->al_vacancies.list.nr_nodes ) {
        err = infos_pm_init_prealloc( (struct infos_page_mgr*)p_pm, len_chunk_of_pages,
            userdata, local_malloc );
        if ( err ) {
          fprintf( stderr, "Error occured in infos_pm_init_prealloc. (%s)\n", err_msg_text[err] );
          return err;
        }
      }

      // Check if there is enough space
      struct range_u32 * p_rent;
      p_rent = infos_pm_rent( (struct infos_page_mgr*)p_pm, userdata, local_malloc,
          _align_to_power_of_two(size_to_req, p_vmm->len_non_coherent_atom), true );
      if ( !p_rent ) {
        continue;
      }
      p_vmm->arr_usages[heap_idx] += _align_to_power_of_two(
          size_to_req, p_vmm->len_non_coherent_atom );

      // Bind the image with *p_malloc
      SPALL_BEGIN("bind_img_mem");
      result = vkBindImageMemory( log_dev, vk_img, *p_malloc, p_rent->start );
      SPALL_END();
      if( result != VK_SUCCESS ) {
        fprintf( stderr, "[Carbonara] Could not bind image to malloc. "
            "'%s'\n", string_VkResult(result) );
        err = infos_pm_return( (struct infos_page_mgr*)p_pm, userdata, local_malloc,
            p_rent->start );
        if ( err ) {
          fprintf( stderr, "[Carbonara] Could not return the rented pages!! "
              "'%s'\n", err_msg_text[err] );
        } else {
          p_vmm->arr_usages[heap_idx] -= _align_to_power_of_two(
              size_to_req, p_vmm->len_non_coherent_atom );
        }
        switch ( result ) {
          case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            [[fallthrough]];
          case VK_ERROR_OUT_OF_HOST_MEMORY:
            return err_msg_out_of_memory;

          case VK_ERROR_VALIDATION_FAILED:
            return err_msg_invalid_request;

          default:
            return err_msg_unknown;
        }
      }

      return 0;
    }

    if ( !m_node->next ) {
      // We need to create one more node
      err = infos_multi_al_grow( &p_vmm->mallocs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", err_msg_text[err] );
        return err;
      }
      err = infos_multi_al_grow( &p_vmm->page_mgrs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", err_msg_text[err] );
        return err;
      }
    } else if ( !p_node->next ) {
      // We must've failed to grow page_mgrs
      err = infos_multi_al_grow( &p_vmm->page_mgrs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", err_msg_text[err] );
        return err;
      }
    }

    m_node = m_node->next;
    p_node = p_node->next;
  }

}


//-------------------------------------------------------------------------------------------------
enum err_msg ma_report_malloc_rel(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t type_idx,
    struct range_u32 * p_malloc_range
    )
{
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  enum err_msg err;

  uint32_t heap_idx = p_vmm->arr_heap_indices[type_idx];
  struct infos_singly_linked_list_node * m_node = &p_vmm->mallocs.lists[type_idx].head;
  struct infos_singly_linked_list_node * p_node = &p_vmm->page_mgrs.lists[type_idx].head;
  const uint32_t last_idx = 1 << p_vmm->mallocs.log2_nr_elems;
  while(7) {
    for ( uint32_t idx = 0; idx < last_idx; idx++ ) {
      struct infos_page_mgr_p2 * p_pm = p_node->data + idx * sizeof(*p_pm);
      if ( !p_pm->al_vacancies.list.nr_nodes ) {
        return err_msg_no_such_entry;
      }

      err = infos_pm_return_via_ptr( (struct infos_page_mgr*)p_pm, userdata, local_malloc,
          p_malloc_range );

      switch ( err ) {
        case err_msg_no_such_entry:
          continue;
        case 0:
          p_vmm->arr_usages[heap_idx] -= get_range_len(*p_malloc_range);
#if !defined( ARE_WE_SHIPPING_IT )
          VkDeviceMemory * p_malloc = m_node->data + idx * sizeof(VkDeviceMemory);
          if ( *p_malloc == VK_NULL_HANDLE ) {
            return err_msg_unknown;
          }
#endif
          [[fallthrough]];
        case err_msg_already_done:
          [[fallthrough]];
        case err_msg_out_of_memory:
          [[fallthrough]];
        default:
          return err;
      }
    }

    if ( !p_node->next ) {
      return err_msg_no_such_entry;
    }

#if !defined( ARE_WE_SHIPPING_IT )
    m_node = m_node->next;
#endif
    p_node = p_node->next;
  }
}


//-------------------------------------------------------------------------------------------------
uint32_t ma_pick_best_mtype(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkMemoryRequirements * restrict req,
    VkMemoryPropertyFlags required_props,
    VkMemoryPropertyFlags preferred_props,
    VkMemoryPropertyFlags tolerable_props
    )
{
  uint_fast32_t type_idx;
  // preferred_props + required_props + !tolerable_props
  for ( type_idx = 0; type_idx < p_vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( (p_vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (p_vmm->arr_mem_property_flags[type_idx] & required_props) &&
          !(p_vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( p_vmm->arr_usages[p_vmm->arr_heap_indices[type_idx]] + req->size <=
            p_vmm->arr_sizes[p_vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  // !preferred_props + required_props + !tolerable_props
  for ( type_idx = 0; type_idx < p_vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( !(p_vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (p_vmm->arr_mem_property_flags[type_idx] & required_props) &&
          !(p_vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( p_vmm->arr_usages[p_vmm->arr_heap_indices[type_idx]] + req->size <=
            p_vmm->arr_sizes[p_vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  // preferred_props + required_props + tolerable_props
  for ( type_idx = 0; type_idx < p_vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( (p_vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (p_vmm->arr_mem_property_flags[type_idx] & required_props) &&
          (p_vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( p_vmm->arr_usages[p_vmm->arr_heap_indices[type_idx]] + req->size <=
            p_vmm->arr_sizes[p_vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  // !preferred_props + required_props + tolerable_props
  for ( type_idx = 0; type_idx < p_vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( !(p_vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (p_vmm->arr_mem_property_flags[type_idx] & required_props) &&
          (p_vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( p_vmm->arr_usages[p_vmm->arr_heap_indices[type_idx]] + req->size <=
            p_vmm->arr_sizes[p_vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  return 0;
}
