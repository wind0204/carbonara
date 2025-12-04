#if !defined SALAMI_GRAPHICS_H_
#define SALAMI_GRAPHICS_H_


#include <stdint.h>
#include <vulkan/vulkan.h>

#include <SDL3/SDL_video.h>

#include "carbonara.h"
#include "appdev_support-shared_library.h"
#include "memorable_arena.h"


//-------------------------------------------------------------------------------------------------

/**
 * \brief 1 meter
 */
constexpr float cara_1m = 1.0 / 2;

// I guess two sets of command pools is okay?
constexpr unsigned int cara_nr_cmd_centers = 2;

enum cara_signs_born : uint64_t {
  csb_origin = 0,
  csb_data_transfer_done,
  csb_acquire_swapchain_img_done,
  csb_rast_done,
  csb_present_swapchain_img,
};


constexpr size_t cara_vk_page_len = 512 * 1024;
struct vk_buf_512kb {
  alignas(cara_vk_page_len) unsigned char bytes[cara_vk_page_len];
};
union vk_page {  // TODO Do we really need a union here?
  struct vk_buf_512kb page;  // cara_vk_page_len bytes
  float floats[cara_vk_page_len / sizeof(float)];    // 131072 floats
  double doubles[cara_vk_page_len / sizeof(double)]; // 65536 doubles
};


/**
 * \brief VkCommandPool, VkCommandBuffer, VkFramebuffer, VkImageView, VkImage
 */
struct vk_frame_info {
  VkCommandPool * arr_cmd_pools;  ///< The number of the entries in the array is equal to
                                      ///<  1+[the number of workers]
                                      ///< Command pools cannot be accessed at the same time from
                                      ///< multiple threads (command buffers from the same pool
                                      ///< cannot be recorded on multiple threads at the same time).
  //VkCommandBuffer * arr_cmd_bufs;  ///< The number of the entries in the array is equal to
                                          ///< cara_max_nr_subpasses*[the number of workers].
                                          ///< Use cara_get_arr_cmd_bufs_for_graphics
                                          ///< to get it.
  static_assert( alignof(void*) >= alignof(VkCommandBuffer), "Sort the struct members!" );
  VkCommandBuffer primary_cmd_buf; ///< A primary cmd buffer coming from the last entry
                                          ///< in arr_cmd_pools
  static_assert( alignof(VkCommandBuffer) >= alignof(VkImageView), "Sort the struct members!" );
  VkImageView iv_z_buf;
  static_assert( alignof(VkImageView) >= alignof(VkImage), "Sort the struct members!" );
  VkImage z_buf;
};

/**
 * \brief VkCommandPool, VkCommandBuffer for transferring data
 */
struct vk_transfer_buf {
  VkCommandPool * arr_cmd_pools;  ///< The number of the entries in the array is equal to
                                  ///< 1+[the number of workers] Command pools cannot be accessed
                                  ///< at the same time from multiple threads (command buffers from
                                  ///< the same pool cannot be recorded on multiple threads at the
                                  ///< same time).
  //VkCommandBuffer * arr_cmd_bufs;  ///< The number of the entries in the array is equal to
                                          ///< cara_max_nr_subpasses*[the number of workers].
                                          ///< Use cara_get_arr_cmd_bufs_for_transfer
                                          ///< to get it.
  static_assert( alignof(void*) >= alignof(VkCommandBuffer), "Sort the struct members!" );
  VkCommandBuffer primary_cmd_buf; ///< A primary command buffer coming from the last entry
                                          ///< in arr_cmd_pools
};


/*
 * \brief A singleton data storage for Vulkan
 */
struct cara_vk_single {
  int64_t asset_size;  // const unsigned int len_mipmapped_2048 = 16 + 16 + (4*4)*4/4 + (8*8)*4/4 + (16*16)*4/4 + (32*32)*4/4 + (64*64)*4/4 + (128*128)*4/4 + (256*256)*4/4 + (512*512)*4/4 + (1024*1024)*4/4 + (2048*2048)*4/4; ..Plus header size for now
  static_assert( alignof(int64_t) >= alignof(void*), "Sort the struct members!" );
  char * stone_texture;
  char * sand_texture;
  SDL_Window * window;
  VkImage * arr_swapchain_imgs;
  static_assert( alignof(void*) >= alignof(struct ma_vk_mem_mgr),
      "Sort the struct members!" );
  struct ma_vk_mem_mgr vmm;
  static_assert( alignof(struct ma_vk_mem_mgr) >= alignof(VkSwapchainKHR),
      "Sort the struct members!" );
  VkSwapchainKHR swapchain;
  // Images represent data that can have one, two, or three dimensions, and can have additional
  // mipmap levels and layers. Each element of an image's data (a texel) can also have one or more samples.
  // Images can be used for many different purposes. We can use them as a source of data for copy
  // operations. We can bind images to pipelines via descriptor sets and use them as textures
  // (similarly to OpenGL). We can render into images, in which case we use images as color or depth
  // attachments (render targets).

  VkImage vki_8_2048_tex;  // 8-layer image of 2048*2048
  VkImage vki_main_render_tgt;  // TODO
  VkImage vki_depth;  // TODO
  VkImageView iv_8_2048_tex;
  VkImageView iv_main_render_tgt;  // TODO
  VkImageView iv_depth;  // TODO
  VkImageView iv_swapchain_img;  // TODO
  VkSampler sampler_8_2048_tex;
  VkDescriptorSetLayout dsl_first;
  VkDescriptorPool dp_first;
  VkDescriptorSet ds_first;
  VkPipelineLayout pipeline_layout;
  VkRenderPass render_pass;
  VkFramebuffer framebuffer;

  VkSurfaceKHR presentation_surface;
  VkSemaphore main_semaphore_before_A;  // a timeline swapchain
  VkSemaphore main_semaphore_after_A;   // a timeline swapchain
  VkSemaphore wsi_semaphore_acquire;    // a binary swapchain
  VkSemaphore wsi_semaphore_present;    // a binary swapchain
  VkQueue graphics_q;  ///< WARNING It can point to the same queue compute_q or transfer_q is
                       ///< pointing to.
  VkQueue transfer_q;  ///< WARNING It can point to the same queue graphics_q or compute_q is
                       ///< pointing to.
  VkQueue compute_q;   ///< WARNING It can point to the same queue transfer_q or graphics_q is
                       ///< pointing to.
  static_assert( alignof(VkQueue) >= alignof(struct vk_frame_info),
      "Sort the struct members!" );
  struct vk_frame_info arr_frame_infos[cara_nr_cmd_centers];
  static_assert( alignof(struct vk_frame_info) >= alignof(struct vk_transfer_buf),
      "Sort the struct members!" );
  struct vk_transfer_buf trans_bufs;
  static_assert( alignof(struct vk_transfer_buf) >= alignof(VkDevice),
      "Sort the struct members!" );
  VkDevice log_dev;  // A logical device. #pointer-size-aligned
  VkInstance vk_instance;  // #pointer-size-aligned
  static_assert( alignof(VkInstance) >= alignof(shared_lib), "Sort the struct members!" );
  shared_lib vk_library;
  static_assert( alignof(shared_lib) >= alignof(VkAllocationCallbacks),
      "Sort the struct members!" );
  VkAllocationCallbacks vk_alloc_callbacks;  // XXX Keep it here instead of in cara_engine?
  static_assert( alignof(VkAllocationCallbacks) >= alignof(VkFormat), "Sort the struct members!" );
  VkFormat color_att_format;  // 32-bit aligned
  static_assert( alignof(VkFormat) >= alignof(VkExtent2D), "Sort the struct members!" );
  VkExtent2D dims_swapchain_imgs;  // VkExtent2D is 32-bit aligned
  static_assert( alignof(VkExtent2D) >= alignof(uint32_t), "Sort the struct members!" );
  uint32_t graphics_q_fam_idx;  // I guess I need this for memory barriers
  uint32_t trans_q_fam_idx;
  uint32_t compute_q_fam_idx;
  float max_anisotropy;
  atomic_uint_least8_t tmp_var_ready;
  uint8_t nr_vk_img;
  uint8_t texture_format_chosen; // either texture_format_astc or texture_format_s3tc
};

// In carbonara-vulkan.c
extern struct cara_vk_single cara_vk;


enum cara_subpass : unsigned char {
  cs_depth_prepass = 0,
  cs_main_render_pass,
  cs_copy_to_swapchain_img,
  cara_max_nr_subpasses
};


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline VkCommandBuffer * cara_get_arr_cmd_buf_for_graphics(
    uint_fast8_t nr_workers, uint_fast8_t fb_idx )
{
  const VkCommandPool * first_pool = cara_vk.arr_frame_infos[0].arr_cmd_pools;

  VkCommandBuffer * ret;
  ret = (void*)( first_pool + ( 1 + nr_workers ) * cara_nr_cmd_centers );
  ret = ret + fb_idx * nr_workers * cara_max_nr_subpasses;

  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline VkCommandBuffer * cara_get_arr_cmd_bufs_for_transfer(
    uint_fast8_t nr_workers )
{
  const VkCommandPool * first_pool = cara_vk.trans_bufs.arr_cmd_pools;

  VkCommandBuffer * first_cmd_buf;
  first_cmd_buf = (void*)( first_pool + ( 1 + nr_workers ) );

  return first_cmd_buf;
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief Call it once, before init_vulkan
 */
//void init_graphics(void);

//-------------------------------------------------------------------------------------------------
/*
 * \brief Initialize the part of the engine that utilizes vulkan
 */
uint8_t init_vulkan( uint32_t vendor_id_pref, uint32_t device_id_pref,
    memorable_arena_t * restrict arena,
    struct cara_worker_state * restrict worker_state );

//-------------------------------------------------------------------------------------------------
void destroy_vulkan_objects( struct ms_worker_info * worker_info );
//void acquire_swapchain_image();
//void present_image();
//void prepare_thread_local_vulkan_objects();

//-------------------------------------------------------------------------------------------------
void jf_prepare_vulkan( struct cara_worker_state * ws, uint8_t * nr_workers );



#endif  // SALAMI_GRAPHICS_H_
