#include <stdint.h>
#include <volk/volk.h>

#include "info_struct.h"
#include "memorable_arena.h"
#include "err_msg.h"
#include "util-c_lang-basic.h"
//#include "vulkan_functions.h"


//-------------------------------------------------------------------------------------------------
enum err_msg ma_init_vk_mem_mgr(
    struct ma_vk_mem_mgr * vmm,
    VkPhysicalDeviceMemoryProperties2 * ptr_mem_properties,
    VkPhysicalDeviceLimits * limits,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  // Save two numbers.
  vmm->heap_cnt = ptr_mem_properties->memoryProperties.memoryHeapCount;
  vmm->type_cnt = ptr_mem_properties->memoryProperties.memoryTypeCount;
  vmm->len_non_coherent_atom = limits->nonCoherentAtomSize;
  vmm->buf_granularity = limits->bufferImageGranularity;
  assume( _is_power_of_two( limits->bufferImageGranularity) );
  assume( _is_power_of_two( limits->nonCoherentAtomSize) );

  // Get memory allocation for the arrays
  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;
  assume( alignof(VkDeviceSize) > alignof(VkMemoryPropertyFlags) );
  assume( alignof(uint32_t) >= alignof(VkMemoryPropertyFlags) );
  vmm->arr_usages = local_malloc(
      userdata,
      alignof(VkDeviceSize),  // alignof(VkDeviceSize) = 8
      vmm->heap_cnt * (
        sizeof(vmm->arr_usages[0]) +
        sizeof(vmm->arr_sizes[0]) +
        sizeof(vmm->arr_idx_next_unoccupied_elem[0]) +
        0 ) +
      vmm->type_cnt * (
        sizeof(vmm->arr_heap_indices[0]) +
        sizeof(vmm->arr_mem_property_flags[0]) +
        0 )
      );
  if ( !vmm->arr_usages ) {
    return err_msg_out_of_memory;
  }
  vmm->arr_sizes = vmm->arr_usages + vmm->heap_cnt;
  vmm->arr_heap_indices = (void*)( vmm->arr_sizes + vmm->heap_cnt );
  vmm->arr_idx_next_unoccupied_elem = vmm->arr_heap_indices + vmm->type_cnt;
  vmm->arr_mem_property_flags = vmm->arr_idx_next_unoccupied_elem + vmm->heap_cnt;

  // Cache some properties.
  for ( unsigned int heap_idx = 0; heap_idx < vmm->heap_cnt; heap_idx++ ) {
    vmm->arr_sizes[heap_idx] =
      ptr_mem_properties->memoryProperties.memoryHeaps[heap_idx].size;
    vmm->arr_idx_next_unoccupied_elem[heap_idx] = 0;
  }
  for ( unsigned int type_idx = 0; type_idx < vmm->type_cnt; type_idx++ ) {
    vmm->arr_heap_indices[type_idx] =
      ptr_mem_properties->memoryProperties.memoryTypes[type_idx].heapIndex;
    vmm->arr_mem_property_flags[type_idx] =
      ptr_mem_properties->memoryProperties.memoryTypes[type_idx].propertyFlags;
  }

  // Assume 0 bytes were used by our application.
  for ( unsigned int heap_idx = 0; heap_idx < vmm->heap_cnt; heap_idx++ ) {
    vmm->arr_usages[heap_idx] = 0;
  }


  //
  // Initialize multi_arr_list which will contain VkDeviceMemory objects
  //

  // FIXME 4 KiB is probably too small (just enough for 512 VkDeviceMemory objects)
  constexpr size_t node_size = 4 * 1024;
  assume( _is_power_of_two(node_size) );
  infos_multi_al_init_prealloc( &vmm->mallocs, userdata, local_malloc,
      get_log2(node_size),
      31 - _lzcnt_u32(sizeof(VkDeviceMemory)),
      _align_to_the_close_power_of_two(vmm->heap_cnt),
      0 );

  //vmm->uploads.al_rental_info.log2_node_size = 0;
  //vmm->uploads.al_vacancies.log2_node_size = 0;
  vmm->p_staging_buf = nullptr;
  vmm->staging_buf = 0;
  vmm->staging_buf_malloc = 0;


  // Return 0
  return err_msg_no_error;
}

//-------------------------------------------------------------------------------------------------
void ma_destroy_vk_mem_mgr(
    struct ma_vk_mem_mgr * vmm,
    VkDevice logical_dev,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr)
    )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;
  local_free( userdata, vmm->arr_usages );

  // TODO vkFreeMemory per malloc in vmm->mallocs

  if ( vmm->p_staging_buf ) {
    vkUnmapMemory( logical_dev, vmm->staging_buf_malloc );
    //vkFreeMemory( logical_dev, vmm->staging_buf_malloc, &cara_vk.vk_alloc_callbacks ); 
    vkFreeMemory( logical_dev, vmm->staging_buf_malloc, nullptr ); 
    //vkDestroyBuffer( logical_dev, vmm->staging_buf, &cara_vk.vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, vmm->staging_buf, nullptr );
    vmm->p_staging_buf = nullptr;
    vmm->staging_buf = 0;
    vmm->staging_buf_malloc = 0;
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

  vmm->arr_sizes = nullptr;
  vmm->arr_usages = nullptr;
  vmm->arr_heap_indices = nullptr;
  vmm->arr_mem_property_flags = nullptr;
  vmm->arr_idx_next_unoccupied_elem = nullptr;
}


//-------------------------------------------------------------------------------------------------
enum err_msg ma_create_staging_buf(
    struct ma_vk_mem_mgr * vmm,
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
      //logical_dev, &buf_create_info, &cara_vk.vk_alloc_callbacks, &vmm->staging_buf );
      logical_dev, &buf_create_info, nullptr, &vmm->staging_buf );
  if( result != VK_SUCCESS ) {
    return err_msg_unknown;
  }


  // Obtain the requirements
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements( logical_dev, vmm->staging_buf, &req );

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
      vmm, &req, required_props, preferred_props, tolerable_props );
  vmm->arr_best_mtype_idx[vk_buf_type_transfer] = type_idx;


  // Allocate memory for the buffer
  VkMemoryAllocateInfo buf_malloc_info;
  buf_malloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  buf_malloc_info.pNext = nullptr;
  buf_malloc_info.allocationSize = req.size;
  buf_malloc_info.memoryTypeIndex = type_idx;

  result = vkAllocateMemory( logical_dev,
      //&buf_malloc_info, &cara_vk.vk_alloc_callbacks, &vmm->staging_buf_malloc );
      &buf_malloc_info, nullptr, &vmm->staging_buf_malloc );
  if( result != VK_SUCCESS ) {
    //vkDestroyBuffer( logical_dev, vmm->staging_buf, &cara_vk.vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, vmm->staging_buf, nullptr );
    vmm->staging_buf = 0;
    return err_msg_out_of_memory;
  }


  // Bind the buffer to the memory allocation
  result = vkBindBufferMemory( logical_dev, vmm->staging_buf, vmm->staging_buf_malloc, 0 );
  if( result != VK_SUCCESS ) {
    //vkFreeMemory( logical_dev, vmm->staging_buf_malloc, &cara_vk.vk_alloc_callbacks );
    vkFreeMemory( logical_dev, vmm->staging_buf_malloc, nullptr );
    //vkDestroyBuffer( logical_dev, vmm->staging_buf, &cara_vk.vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, vmm->staging_buf, nullptr );
    vmm->staging_buf = 0;
    vmm->staging_buf_malloc = 0;
    return err_msg_unknown;
  }


  // Map the memory region
  result = vkMapMemory( logical_dev, vmm->staging_buf_malloc, 0, buf_len, 0, &vmm->p_staging_buf );
  if( result != VK_SUCCESS ) {
    //vkFreeMemory( logical_dev, vmm->staging_buf_malloc, &cara_vk.vk_alloc_callbacks );
    vkFreeMemory( logical_dev, vmm->staging_buf_malloc, nullptr );
    //vkDestroyBuffer( logical_dev, vmm->staging_buf, &cara_vk.vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, vmm->staging_buf, nullptr );
    vmm->staging_buf = 0;
    vmm->staging_buf_malloc = 0;
    vmm->p_staging_buf = nullptr;
    return err_msg_unknown;
  }

  // Have the OS actually prepare pages for the mapped memory range
  memset( vmm->p_staging_buf, 0, buf_len );

  vmm->len_staging_buf = buf_len;

  const uint32_t heap_idx = vmm->arr_heap_indices[type_idx];
  vmm->arr_usages[heap_idx] += req.size;

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
  if( result != VK_SUCCESS ) {
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
  if( result != VK_SUCCESS ) {
    vkDestroyBuffer( logical_dev, arr_bufs[0], &cara_vk.vk_alloc_callbacks );
    return err_msg_out_of_memory;
  }


  // Bind the first buffer to the first allocation
  result = vkBindBufferMemory( logical_dev, arr_bufs[0], *p_alloc, 0 );
  if( result != VK_SUCCESS ) {
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
    if( result != VK_SUCCESS ) {
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
    if( result != VK_SUCCESS ) {
      // Destroy the buffer
      vkDestroyBuffer( logical_dev, arr_bufs[malloc_cnt], &cara_vk.vk_alloc_callbacks );
      break;
    }

    // Bind the buffer to the new allocation
    result = vkBindBufferMemory( logical_dev, arr_bufs[malloc_cnt], *p_alloc, 0 );
    if( result != VK_SUCCESS ) {
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
uint32_t ma_pick_best_mtype(
    struct ma_vk_mem_mgr * vmm,
    VkMemoryRequirements * req,
    VkMemoryPropertyFlags required_props,
    VkMemoryPropertyFlags preferred_props,
    VkMemoryPropertyFlags tolerable_props
    )
{
  uint_fast32_t type_idx;
  // preferred_props + required_props + !tolerable_props
  for ( type_idx = 0; type_idx < vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( (vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (vmm->arr_mem_property_flags[type_idx] & required_props) &&
          !(vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( vmm->arr_usages[vmm->arr_heap_indices[type_idx]] + req->size <=
            vmm->arr_sizes[vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  // !preferred_props + required_props + !tolerable_props
  for ( type_idx = 0; type_idx < vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( !(vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (vmm->arr_mem_property_flags[type_idx] & required_props) &&
          !(vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( vmm->arr_usages[vmm->arr_heap_indices[type_idx]] + req->size <=
            vmm->arr_sizes[vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  // preferred_props + required_props + tolerable_props
  for ( type_idx = 0; type_idx < vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( (vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (vmm->arr_mem_property_flags[type_idx] & required_props) &&
          (vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( vmm->arr_usages[vmm->arr_heap_indices[type_idx]] + req->size <=
            vmm->arr_sizes[vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  // !preferred_props + required_props + tolerable_props
  for ( type_idx = 0; type_idx < vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( !(vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (vmm->arr_mem_property_flags[type_idx] & required_props) &&
          (vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( vmm->arr_usages[vmm->arr_heap_indices[type_idx]] + req->size <=
            vmm->arr_sizes[vmm->arr_heap_indices[type_idx]] ) {
          return type_idx;
        }
      }
    }
  }

  return 0;
}


//-------------------------------------------------------------------------------------------------

enum err_msg ma_prepare_upload_mgr(
    struct ma_vk_mem_mgr * vmm,
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



