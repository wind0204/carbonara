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
  p_vmm->len_non_coherent_atom =  // Align to CACHELINE_LEN for multi-threaded loading
    _align_to_power_of_two( p_limits->nonCoherentAtomSize, CACHELINE_LEN );
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
    return emsg_out_of_memory;
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
  return emsg_no_error;
}

//-------------------------------------------------------------------------------------------------
void ma_destroy_vk_mem_mgr(
    struct ma_vk_mem_mgr * restrict p_vmm,
    VkDevice logical_dev,
    void * restrict userdata_free,
    void (*custom_free)(void * restrict userdata, void * restrict ptr),
    void * restrict userdata_malloc,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size)
    )
{
  void (*local_free)(void * restrict userdata, void * restrict ptr);
  local_free = custom_free ? custom_free : default_free;
  local_free( userdata_free, p_vmm->arr_usages );
  void * (*local_malloc)(void * userdata_malloc, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  // Release every memory allocation
  for ( unsigned int type_idx = 0; type_idx < p_vmm->type_cnt; type_idx++ ) {
    enum err_msg err;

    uint32_t heap_idx = p_vmm->arr_heap_indices[type_idx];
    struct infos_singly_linked_list_node * m_node = &p_vmm->mallocs.lists[type_idx].head;
    struct infos_singly_linked_list_node * p_node = &p_vmm->page_mgrs.lists[type_idx].head;
    const uint32_t last_idx = 1 << p_vmm->mallocs.log2_nr_elems;
    uint32_t node_idx = 0;
    while(7) {
      for ( uint32_t malloc_idx = 0; malloc_idx < last_idx; malloc_idx++ ) {
        struct infos_page_mgr_p2 * p_pm = p_node->data + malloc_idx * sizeof(*p_pm);
        if ( !p_pm->al_vacancies.list.nr_nodes ) {
          break;
        }

        VkDeviceMemory * p_malloc = m_node->data + malloc_idx * sizeof(VkDeviceMemory);
        if ( *p_malloc == VK_NULL_HANDLE ) {
          fprintf( stderr, "[MA] *p_malloc is already VK_NULL_HANDLE "
              "type_idx=%u node_idx=%u malloc_idx=%u\n",
              type_idx, node_idx, malloc_idx );
          continue;
        }

        vkFreeMemory( logical_dev, *p_malloc, &vk_alloc_callbacks );
        *p_malloc = VK_NULL_HANDLE;
#if !defined( ARE_WE_SHIPPING_IT )
        struct infos_singly_linked_list_node * r_node = &p_pm->al_rental_info.list.head;
        while( r_node ) {
          struct range_u32 * p_malloc_range = r_node->data; p_malloc_range--;
          for ( uint32_t bind_idx = 0; bind_idx < p_pm->idx_last_rental_info; bind_idx++ ) {
            p_malloc_range++;
            if ( (*p_malloc_range).start > (*p_malloc_range).stop ) {
              continue;
            }

            err = infos_pm_return_via_ptr( (struct infos_page_mgr*)p_pm,
                userdata_malloc, local_malloc,
                p_malloc_range );
            switch ( err ) {
              case 0:
                p_vmm->arr_usages[heap_idx] -= get_range_len(*p_malloc_range);
                break;
              case emsg_no_such_entry:
                [[fallthrough]];
              case emsg_already_done:
                [[fallthrough]];
              case emsg_out_of_memory:
                [[fallthrough]];
              default:
                fprintf( stderr, "[MA] Error occured in infos_pm_return_via_ptr: %s\n",
                    arr_emsg_txt[err] );
                fprintf( stderr, "[MA] context: "
                    "type_idx=%u node_idx=%u malloc_idx=%u rnode_idx=? bind_idx=%u\n",
                    type_idx, node_idx, malloc_idx, bind_idx );
                break;
            }
          }
          r_node = r_node->next;
        }  // while( r_node )

        if ( p_vmm->arr_usages[heap_idx] > 0 ) {
          fprintf( stdout, "[MA-Debug] leftover in heap_idx %u = %lu\n",
              heap_idx, p_vmm->arr_usages[heap_idx] );
        }
#endif
      }  // for ( uint32_t malloc_idx = 0; malloc_idx < last_idx; malloc_idx++ )

      if ( !p_node->next ) {
        infos_multi_al_destroy_except_preallocated_struct( &p_vmm->page_mgrs,
            userdata_free, local_free );
        infos_multi_al_destroy_except_preallocated_struct( &p_vmm->mallocs,
            userdata_free, local_free );
        break;
      }

      m_node = m_node->next;
      p_node = p_node->next;
      node_idx++;
    }
  }

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
    return emsg_unknown;
  }


  // Obtain the requirements
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements( logical_dev, p_vmm->staging_buf, &req );

  // Choose the best memory type
  VkMemoryPropertyFlags preferred_props, required_props, tolerable_props;
  preferred_props = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  required_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  tolerable_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  const uint32_t type_idx = ma_pick_best_mtype(
      p_vmm, &req, required_props, preferred_props, tolerable_props );
  //p_vmm->arr_mtype_used[vk_buf_type_transfer] = type_idx;


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
    return emsg_out_of_memory;
  }


  // Bind the buffer to the memory allocation
  result = vkBindBufferMemory( logical_dev, p_vmm->staging_buf, p_vmm->staging_buf_malloc, 0 );
  if ( result != VK_SUCCESS ) {
    vkFreeMemory( logical_dev, p_vmm->staging_buf_malloc, &vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, p_vmm->staging_buf, &vk_alloc_callbacks );
    p_vmm->staging_buf = 0;
    p_vmm->staging_buf_malloc = 0;
    return emsg_unknown;
  }


  // Map the memory region
  result = vkMapMemory( logical_dev, p_vmm->staging_buf_malloc, 0, buf_len, 0, &p_vmm->p_staging_buf );
  if ( result != VK_SUCCESS ) {
    vkFreeMemory( logical_dev, p_vmm->staging_buf_malloc, &vk_alloc_callbacks );
    vkDestroyBuffer( logical_dev, p_vmm->staging_buf, &vk_alloc_callbacks );
    p_vmm->staging_buf = 0;
    p_vmm->staging_buf_malloc = 0;
    p_vmm->p_staging_buf = nullptr;
    return emsg_unknown;
  }

  // Have the OS actually prepare pages for the mapped memory range
  memset( p_vmm->p_staging_buf, 0, buf_len );

  p_vmm->len_staging_buf = buf_len;

  const uint32_t heap_idx = p_vmm->arr_heap_indices[type_idx];
  p_vmm->arr_usages[heap_idx] += req.size;

  return 0;
}


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
    VkDeviceSize req_size,  // Required allocation size in bytes
    VkDeviceSize req_alignment,  // Required allocation alignment in bytes
    struct range_u32 ** pp_malloc_range
    )
{
  SPALL_BEGIN("bind_img_to_m");
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
    for ( uint32_t malloc_idx = 0; malloc_idx < last_idx; malloc_idx++ ) {
      VkDeviceMemory * p_malloc = m_node->data + malloc_idx * sizeof(VkDeviceMemory);
      struct infos_page_mgr_p2 * p_pm = p_node->data + malloc_idx * sizeof(*p_pm);
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
              SPALL_END();
              return emsg_out_of_memory;

            case VK_ERROR_INVALID_EXTERNAL_HANDLE:
              [[fallthrough]];
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR:
              [[fallthrough]];
            case VK_ERROR_VALIDATION_FAILED:
              SPALL_END();
              return emsg_invalid_request;

            default:
              SPALL_END();
              return emsg_unknown;
          }
        }

        // Init struct infos_page_mgr_p2
        err = infos_pm_init_prealloc( (struct infos_page_mgr*)p_pm, len_chunk_of_pages,
            userdata, local_malloc );
        if ( err ) {
          fprintf( stderr, "Error occured in infos_pm_init_prealloc. (%s)\n", arr_emsg_txt[err] );
          SPALL_END();
          return err;
        }
      }

      // We must've failed to init struct infos_page_mgr_p2 before this call
      if ( !p_pm->al_vacancies.list.nr_nodes ) {
        err = infos_pm_init_prealloc( (struct infos_page_mgr*)p_pm, len_chunk_of_pages,
            userdata, local_malloc );
        if ( err ) {
          fprintf( stderr, "Error occured in infos_pm_init_prealloc. (%s)\n", arr_emsg_txt[err] );
          SPALL_END();
          return err;
        }
      }

      // Check if there is enough space
      struct range_u32 * p_rent;
      p_rent = infos_pm_rent_aligned( (struct infos_page_mgr*)p_pm, userdata, local_malloc,
          _align_to_power_of_two(size_to_req, req_alignment),
          req_alignment );
      if ( !p_rent ) {
        continue;
      }
      p_vmm->arr_usages[heap_idx] += _align_to_power_of_two( size_to_req, req_alignment );

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
              "'%s'\n", arr_emsg_txt[err] );
        } else {
          p_vmm->arr_usages[heap_idx] -= _align_to_power_of_two( size_to_req, req_alignment );
        }
        switch ( result ) {
          case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            [[fallthrough]];
          case VK_ERROR_OUT_OF_HOST_MEMORY:
            SPALL_END();
            return emsg_out_of_memory;

          case VK_ERROR_VALIDATION_FAILED:
            SPALL_END();
            return emsg_invalid_request;

          default:
            SPALL_END();
            return emsg_unknown;
        }
      }

      *pp_malloc_range = p_rent;
      SPALL_END();
      return 0;
    }

    if ( !m_node->next ) {
      // We need to create one more node
      err = infos_multi_al_grow( &p_vmm->mallocs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", arr_emsg_txt[err] );
        SPALL_END();
        return err;
      }
      err = infos_multi_al_grow( &p_vmm->page_mgrs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", arr_emsg_txt[err] );
        SPALL_END();
        return err;
      }
    } else if ( !p_node->next ) {
      // We must've failed to grow page_mgrs
      err = infos_multi_al_grow( &p_vmm->page_mgrs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", arr_emsg_txt[err] );
        SPALL_END();
        return err;
      }
    }

    m_node = m_node->next;
    p_node = p_node->next;
  }

}

//-------------------------------------------------------------------------------------------------
enum err_msg ma_bind_buf_to_malloc(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    VkDevice log_dev,
    VkBuffer vk_buf,
    uint32_t type_idx,
    VkDeviceSize req_size,  // Required allocation size in bytes
    VkDeviceSize req_alignment,  // Required allocation alignment in bytes
    struct range_u32 ** pp_malloc_range
    )
{
  SPALL_BEGIN("bind_buf_to_m");
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
    for ( uint32_t malloc_idx = 0; malloc_idx < last_idx; malloc_idx++ ) {
      VkDeviceMemory * p_malloc = m_node->data + malloc_idx * sizeof(VkDeviceMemory);
      struct infos_page_mgr_p2 * p_pm = p_node->data + malloc_idx * sizeof(*p_pm);
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
              SPALL_END();
              return emsg_out_of_memory;

            case VK_ERROR_INVALID_EXTERNAL_HANDLE:
              [[fallthrough]];
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR:
              [[fallthrough]];
            case VK_ERROR_VALIDATION_FAILED:
              SPALL_END();
              return emsg_invalid_request;

            default:
              SPALL_END();
              return emsg_unknown;
          }
        }

        // Init struct infos_page_mgr_p2
        err = infos_pm_init_prealloc( (struct infos_page_mgr*)p_pm, len_chunk_of_pages,
            userdata, local_malloc );
        if ( err ) {
          fprintf( stderr, "Error occured in infos_pm_init_prealloc. (%s)\n", arr_emsg_txt[err] );
          SPALL_END();
          return err;
        }
      }

      // We must've failed to init struct infos_page_mgr_p2 before this call
      if ( !p_pm->al_vacancies.list.nr_nodes ) {
        err = infos_pm_init_prealloc( (struct infos_page_mgr*)p_pm, len_chunk_of_pages,
            userdata, local_malloc );
        if ( err ) {
          fprintf( stderr, "Error occured in infos_pm_init_prealloc. (%s)\n", arr_emsg_txt[err] );
          SPALL_END();
          return err;
        }
      }

      // Check if there is enough space
      struct range_u32 * p_rent;
      p_rent = infos_pm_rent_aligned( (struct infos_page_mgr*)p_pm, userdata, local_malloc,
          _align_to_power_of_two(size_to_req, req_alignment),
          req_alignment );
      if ( !p_rent ) {
        continue;
      }
      p_vmm->arr_usages[heap_idx] += _align_to_power_of_two( size_to_req, req_alignment );

      // Bind the buffer with *p_malloc
      SPALL_BEGIN("bind_buf_mem");
      result = vkBindBufferMemory( log_dev, vk_buf, *p_malloc, p_rent->start );
      SPALL_END();
      if( result != VK_SUCCESS ) {
        fprintf( stderr, "[Carbonara] Could not bind buffer to malloc. "
            "'%s'\n", string_VkResult(result) );
        err = infos_pm_return( (struct infos_page_mgr*)p_pm, userdata, local_malloc,
            p_rent->start );
        if ( err ) {
          fprintf( stderr, "[Carbonara] Could not return the rented pages!! "
              "'%s'\n", arr_emsg_txt[err] );
        } else {
          p_vmm->arr_usages[heap_idx] -= _align_to_power_of_two( size_to_req, req_alignment );
        }
        switch ( result ) {
          case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            [[fallthrough]];
          case VK_ERROR_OUT_OF_HOST_MEMORY:
            SPALL_END();
            return emsg_out_of_memory;

          case VK_ERROR_VALIDATION_FAILED:
            SPALL_END();
            return emsg_invalid_request;

          default:
            SPALL_END();
            return emsg_unknown;
        }
      }

      *pp_malloc_range = p_rent;
      SPALL_END();
      return 0;
    }

    if ( !m_node->next ) {
      // We need to create one more node
      err = infos_multi_al_grow( &p_vmm->mallocs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", arr_emsg_txt[err] );
        SPALL_END();
        return err;
      }
      err = infos_multi_al_grow( &p_vmm->page_mgrs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", arr_emsg_txt[err] );
        SPALL_END();
        return err;
      }
    } else if ( !p_node->next ) {
      // We must've failed to grow page_mgrs
      err = infos_multi_al_grow( &p_vmm->page_mgrs, userdata, local_malloc,
          1, &type_idx,
          true );
      if ( err ) {
        fprintf( stderr, "Error occured in infos_multi_al_grow. (%s)\n", arr_emsg_txt[err] );
        SPALL_END();
        return err;
      }
    }

    m_node = m_node->next;
    p_node = p_node->next;
  }

}


//-------------------------------------------------------------------------------------------------
enum err_msg ma_report_vk_malloc_rel(
    struct ma_vk_mem_mgr * restrict p_vmm,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    uint32_t type_idx,
    struct range_u32 * p_malloc_range
    )
{
  assume( p_malloc_range );

  void * (*local_malloc)(void * userdata, size_t alignment, size_t size);
  local_malloc = custom_malloc ? custom_malloc : default_malloc;

  enum err_msg err;

  uint32_t heap_idx = p_vmm->arr_heap_indices[type_idx];
#if !defined( ARE_WE_SHIPPING_IT )
  struct infos_singly_linked_list_node * m_node = &p_vmm->mallocs.lists[type_idx].head;
#endif
  struct infos_singly_linked_list_node * p_node = &p_vmm->page_mgrs.lists[type_idx].head;
  const uint32_t last_idx = 1 << p_vmm->mallocs.log2_nr_elems;
  while(7) {
    for ( uint32_t idx = 0; idx < last_idx; idx++ ) {
      struct infos_page_mgr_p2 * p_pm = p_node->data + idx * sizeof(*p_pm);
      if ( !p_pm->al_vacancies.list.nr_nodes ) {
        return emsg_no_such_entry;
      }

      err = infos_pm_return_via_ptr( (struct infos_page_mgr*)p_pm, userdata, local_malloc,
          p_malloc_range );

      switch ( err ) {
        case emsg_no_such_entry:
          continue;
        case 0:
          p_vmm->arr_usages[heap_idx] -= get_range_len(*p_malloc_range);
#if !defined( ARE_WE_SHIPPING_IT )
          VkDeviceMemory * p_malloc = m_node->data + idx * sizeof(VkDeviceMemory);
          assume( *p_malloc );
#endif
          [[fallthrough]];
        case emsg_already_done:
          [[fallthrough]];
        case emsg_out_of_memory:
          [[fallthrough]];
        default:
          return err;
      }
    }

    if ( !p_node->next ) {
      return emsg_no_such_entry;
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
  SPALL_BEGIN("pick_best_type");
  uint_fast32_t type_idx;
  // preferred_props + required_props + !tolerable_props
  for ( type_idx = 0; type_idx < p_vmm->type_cnt; type_idx++ ) {
    if ( req->memoryTypeBits & (1 << type_idx) ) {
      if ( (p_vmm->arr_mem_property_flags[type_idx] & preferred_props) &&
          (p_vmm->arr_mem_property_flags[type_idx] & required_props) &&
          !(p_vmm->arr_mem_property_flags[type_idx] & tolerable_props) ) {
        if ( p_vmm->arr_usages[p_vmm->arr_heap_indices[type_idx]] + req->size <=
            p_vmm->arr_sizes[p_vmm->arr_heap_indices[type_idx]] ) {
          SPALL_END();
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
          SPALL_END();
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
          SPALL_END();
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
          SPALL_END();
          return type_idx;
        }
      }
    }
  }

  SPALL_END();
  return 0;
}
