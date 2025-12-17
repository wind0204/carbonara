#if !defined SALAMI_GRAPHICS_H_
#define SALAMI_GRAPHICS_H_


#include <stdint.h>
#include <vulkan/vulkan.h>

#include <SDL3/SDL_video.h>

#include "carbonara-singleton.h"
#include "util-c_lang-basic.h"
#include "appdev_support-processor_map.h"
#include "carbonara.h"
//#include "appdev_support-shared_library.h"
#include "memorable_arena.h"


//-------------------------------------------------------------------------------------------------

// WSVGA
constexpr unsigned int small_win_width = 1024;
constexpr unsigned int small_win_height = 576;  // WSVGA

// One meter in game coordinates
constexpr float cara_1m = 1.0 / 2;
// The length of the side of each square tile
constexpr float cara_tilesize = cara_1m * 8;

// Two frames in flight, two sets of command pools?
constexpr unsigned int cara_max_nr_fif = 2;

// Granularity of the length of framebuffers' sides and swapchain images' sides used when expanding
constexpr unsigned int framebuf_dims_granularity = 16;
static_assert( _is_power_of_two(framebuf_dims_granularity) );

enum cara_signs_born : uint64_t {
  csb_origin = 0,
  csb_data_transfer_done,
  csb_acquire_swapchain_img_done,
  csb_rast_done,
  csb_present_swapchain_img,
};

// cs_build_lowres_cluster_cull -> cs_z_only -> cs_build_cluster_cull -> cs_draw_opaq_objs -> cs_non_opaque_objs -> cs_copy_to_swapchain_img
enum cara_subpass : unsigned char {
  cs_z_only = 0,
  cs_draw_opaq_objs,
  cs_copy_to_swapchain_img,
  cara_nr_subpasses,
};

enum cara_framebuffers : unsigned char {
  fb_zbuf = 0,  // Depth
  fb_cbuf,  // Color
  fb_swapchain,  // Present on screen
  cara_nr_famebuffers,
};

//constexpr size_t cara_vk_page_len = 512 * 1024;
//struct vk_buf_512kb {
//  alignas(cara_vk_page_len) unsigned char bytes[cara_vk_page_len];
//};
//union vk_page {  // TODO Do we really need a union here?
//  struct vk_buf_512kb page;  // cara_vk_page_len bytes
//  float floats[cara_vk_page_len / sizeof(float)];    // 131072 floats
//  double doubles[cara_vk_page_len / sizeof(double)]; // 65536 doubles
//};


//-------------------------------------------------------------------------------------------------

/**
 * \brief VkCommandPool, VkCommandBuffer, VkFramebuffer, VkImageView, VkImage
 */
struct graphics_cmd_bufs {
  VkCommandPool * arr_cmd_pools;  ///< The number of the entries in the array is equal to
                                      ///<  1+[the number of workers]
                                      ///< Command pools cannot be accessed at the same time from
                                      ///< multiple threads (command buffers from the same pool
                                      ///< cannot be recorded on multiple threads at the same time).
  //VkCommandBuffer * arr_cmd_bufs;  ///< The number of the entries in the array is equal to
                                          ///< cara_max_nr_subpasses*[the number of workers].
                                          ///< Use cara_get_arr_cmd_bufs_for_graphics
                                          ///< to get it.
  static_assert( alignof(void*) >= alignof(VkCommandBuffer) );
  VkCommandBuffer primary_cmd_buf; ///< A primary cmd buffer coming from the last entry
                                          ///< in arr_cmd_pools
};

/**
 * \brief VkCommandPool, VkCommandBuffer for transferring data
 */
struct trans_cmd_bufs {
  VkCommandPool * arr_cmd_pools;  ///< The number of the entries in the array is equal to
                                  ///< 1+[the number of workers] Command pools cannot be accessed
                                  ///< at the same time from multiple threads (command buffers from
                                  ///< the same pool cannot be recorded on multiple threads at the
                                  ///< same time).
  //VkCommandBuffer * arr_cmd_bufs;  ///< The number of the entries in the array is equal to
                                          ///< cara_max_nr_subpasses*[the number of workers].
                                          ///< Use cara_get_arr_cmd_bufs_for_transfer
                                          ///< to get it.
  static_assert( alignof(void*) >= alignof(VkCommandBuffer) );
  VkCommandBuffer primary_cmd_buf; ///< A primary command buffer coming from the last entry
                                          ///< in arr_cmd_pools
};


union win_dims {
  struct {
    uint16_t width;
    uint16_t height;
  } parts;
  uint32_t whole;
};

/*
 * \brief A data storage for Vulkan rendering
 */
struct cara_vk_renderer {
  int64_t asset_size;  // const unsigned int len_mipmapped_2048 = 16 + 16 + (4*4)*4/4 + (8*8)*4/4 + (16*16)*4/4 + (32*32)*4/4 + (64*64)*4/4 + (128*128)*4/4 + (256*256)*4/4 + (512*512)*4/4 + (1024*1024)*4/4 + (2048*2048)*4/4; ..Plus header size for now
  int64_t len_z_only;
  int64_t len_z_only_frag;
  int64_t len_simple_vert;
  int64_t len_simple_frag;
  int64_t len_copy2sc_vert;
  int64_t len_copy2sc_frag;
  static_assert( alignof(int64_t) >= alignof(void*) );
  char * stone_texture;
  char * sand_texture;
  void * spv_z_only;
  void * spv_z_only_frag;
  void * spv_simple_vert;
  void * spv_simple_frag;
  void * spv_copy2sc_vert;
  void * spv_copy2sc_frag;
  struct range_u32 * p_malloc_range_tex8_2048;
  struct range_u32 * p_malloc_range_zbuf_and_cbuf[2];
  SDL_Window * window;
  VkImage * arr_swapchain_imgs;
  VkImageView * arr_swapchain_iv;
  VkSemaphore * arr_wsi_present_semaphores;  // One binary semaphore per swapchain img
  static_assert( alignof(void*) >= alignof(struct ma_vk_mem_mgr) );
  struct ma_vk_mem_mgr vmm;
  static_assert( alignof(struct ma_vk_mem_mgr) >= alignof(struct infos_arr_list) );
  struct infos_arr_list old_swapchains;  ///< It contains `nr_old_swapchains` instances of `struct
                                         ///< dead_swapchain`
  static_assert( alignof(struct infos_arr_list) >= alignof(VkSwapchainKHR) );
  VkSwapchainKHR swapchain;
  // Images represent data that can have one, two, or three dimensions, and can have additional
  // mipmap levels and layers. Each element of an image's data (a texel) can also have one or more samples.
  // Images can be used for many different purposes. We can use them as a source of data for copy
  // operations. We can bind images to pipelines via descriptor sets and use them as textures
  // (similarly to OpenGL). We can render into images, in which case we use images as color or depth
  // attachments (render targets).

  VkImage zbuf_n_cbuf[2];
  VkImage tex8_2048;  // 8-layer image of 2048*2048
  VkImageView tex8_2048_iv;
  VkImageView zbuf_n_cbuf_iv[2];
  VkSampler sampler8_2048;
  VkDescriptorSetLayout dsl_z_only;
  VkDescriptorSetLayout dsl_main_render;
  VkDescriptorSetLayout dsl_copy2sc;
  VkDescriptorPool dp_main_render;
  VkDescriptorSet ds_main_render;
  VkPipelineLayout pll_z_only;
  VkPipelineLayout pll_main_render;
  VkPipelineLayout pll_copy2sc;
  VkPipeline arr_pipelines[cara_nr_subpasses];
  VkRenderPass render_pass;
  VkFramebuffer framebuffer;
  VkShaderModule sm_z_only;
  VkShaderModule sm_z_only_frag;
  VkShaderModule sm_simple_vert;
  VkShaderModule sm_simple_frag;
  VkShaderModule sm_copy2sc_vert;
  VkShaderModule sm_copy2sc_frag;

  VkSurfaceKHR presentation_surface;
  VkSemaphore arr_semaphore_before_A[cara_max_nr_fif];  // a timeline semaphore
  VkSemaphore arr_semaphore_after_A[cara_max_nr_fif];   // a timeline semaphore
  VkSemaphore arr_acquire_semaphores[cara_max_nr_fif];  // binary semaphores
  VkQueue graphics_q;  ///< WARNING It can point to the same queue compute_q or transfer_q is
                       ///< pointing to.
  VkQueue transfer_q;  ///< WARNING It can point to the same queue graphics_q or compute_q is
                       ///< pointing to.
  VkQueue compute_q;   ///< WARNING It can point to the same queue transfer_q or graphics_q is
                       ///< pointing to.
  static_assert( alignof(VkQueue) >= alignof(struct graphics_cmd_bufs) );
  struct graphics_cmd_bufs arr_frame_infos[cara_max_nr_fif];
  static_assert( alignof(struct graphics_cmd_bufs) >= alignof(struct trans_cmd_bufs) );
  struct trans_cmd_bufs trans_buf;
  static_assert( alignof(struct trans_cmd_bufs) >= alignof(VkDevice) );
  VkDevice log_dev;  // A logical device. #pointer-size-aligned
  VkInstance vk_instance;  // #pointer-size-aligned
  //static_assert( alignof(VkInstance) >= alignof(shared_lib) );
  //shared_lib vk_library;
  //static_assert( alignof(shared_lib) >= alignof(VkAllocationCallbacks),
  static_assert( alignof(VkInstance) >= alignof(VkFormat) );
  VkFormat swapchain_img_fmt;  // 32-bit aligned
  VkFormat zbuf_fmt;  // 32-bit aligned
  static_assert( alignof(VkFormat) >= alignof(VkExtent2D) );
  VkExtent2D dims_swapchain_imgs;  // VkExtent2D is 32-bit aligned
  static_assert( alignof(VkExtent2D) >= alignof(uint32_t) );
  uint32_t graphics_q_fam_idx;  // I guess I need this for memory barriers
  uint32_t trans_q_fam_idx;
  uint32_t compute_q_fam_idx;
  uint32_t nr_old_swapchains;  // It tells how many slots in `old_swapchains` are being used.
  uint32_t frame_id;  // the frame id of the 1st frame rendered will most likely be 1.
  float max_anisotropy;
  union win_dims win_size;  ///< The current dimensions of the window, it can be different from the
                            ///<  the framebuffers and swapchain images. See dims_swapchain_imgs
  union win_dims max_win_size;  // The width and height of the framebuffers and swapchain images
  union win_dims max_win_size_avail;  // The maximum size available on the presentation surface
  static_assert( alignof(int32_t) == alignof(float) && alignof(float) >= alignof(union win_dims) &&
      alignof(union win_dims) >= alignof(VkSurfaceTransformFlagBitsKHR));
  VkSurfaceTransformFlagBitsKHR vk_surface_transform_flags;  // Can I assume it's always .*identity?
  static_assert( alignof(VkSurfaceTransformFlagBitsKHR) >= alignof(VkPresentModeKHR) );
  VkPresentModeKHR vk_present_mode;
  static_assert( alignof(VkPresentModeKHR) >= alignof(VkColorSpaceKHR) );
  VkColorSpaceKHR vk_color_space;
  uint8_t nr_swapchain_imgs;
  uint8_t nr_swapchain_imgs_we_request;
  uint8_t texture_format_chosen; // either texture_format_astc or texture_format_s3tc
};

struct dead_swapchain {
  VkSwapchainKHR swapchain;
  static_assert( alignof(VkSwapchainKHR) >=
      alignof(_typeof_member(struct cara_vk_renderer, frame_id)) );
  _typeof_member(struct cara_vk_renderer, frame_id) destruct_time;
};

extern struct cara_vk_renderer cara_vk;
extern thread_local VkAllocationCallbacks vk_alloc_callbacks;


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline)) [[maybe_unused]]
static bool has_stencil_buf( VkFormat format )
{
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline VkCommandBuffer * cara_get_arr_cmd_buf_for_graphics(
    uint_fast8_t nr_workers, uint_fast8_t fb_idx )
{
  const VkCommandPool * first_pool = cara_vk.arr_frame_infos[0].arr_cmd_pools;

  VkCommandBuffer * ret;
  ret = (void*)( first_pool + ( 1 + nr_workers ) * cara_max_nr_fif );
  ret = ret + fb_idx * nr_workers * cara_nr_subpasses;

  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline VkCommandBuffer * cara_get_arr_cmd_bufs_for_transfer(
    uint_fast8_t nr_workers )
{
  const VkCommandPool * first_pool = cara_vk.trans_buf.arr_cmd_pools;

  VkCommandBuffer * first_cmd_buf;
  first_cmd_buf = (void*)( first_pool + ( 1 + nr_workers ) );

  return first_cmd_buf;
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief Create a VkRenderPass and a VkFramebuffer
 */
enum err_msg create_1st_renderpass_and_framebuffer( struct cara_vk_renderer * renderer );

//-------------------------------------------------------------------------------------------------
/*
 * \brief Create a VkRenderPass and a VkFramebuffer
 */
enum err_msg expand_swapchain_and_framebuffer( struct cara_vk_renderer * renderer );

//-------------------------------------------------------------------------------------------------
/*
 * \brief Call it once, before init_vulkan
 */
//void init_graphics(void);

//-------------------------------------------------------------------------------------------------
/*
 * \brief Initialize the vullkan part
 */
uint8_t init_vulkan( uint32_t vendor_id_pref, uint32_t device_id_pref,
    memorable_arena_t * restrict arena,
    struct cara_worker_state * restrict worker_state );

//-------------------------------------------------------------------------------------------------
void destroy_vk_objs( _typeof_member(struct ms_worker_info, idx_self) worker_idx );
//void acquire_swapchain_image();
//void present_image();
//void prepare_thread_local_vulkan_objects();

//-------------------------------------------------------------------------------------------------
void jf_prepare_vulkan( struct cara_worker_state * ws, uint8_t * nr_workers );



#endif  // SALAMI_GRAPHICS_H_
