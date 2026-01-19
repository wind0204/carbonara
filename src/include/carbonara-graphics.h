#if !defined SALAMI_GRAPHICS_H_
#define SALAMI_GRAPHICS_H_


#include <stdint.h>
#include <vulkan/vulkan.h>

#include <SDL3/SDL_video.h>

#include "util-c_lang-basic.h"
#include "appdev_support-processor_map.h"
#include "carbonara-job.h"
#include "carbonara-asset.h"
//#include "appdev_support-shared_library.h"
#include "memorable_arena.h"


//-------------------------------------------------------------------------------------------------

// WSVGA
constexpr unsigned int initial_win_width = 384;
constexpr unsigned int initial_win_height = 240;  // WQVGA

// One meter in game coordinates
constexpr float cara_1m = 1.0 / 2;
// Two meters in game coordinates
constexpr float two_m = 1.0;
// Four meters in game coordinates
constexpr float fourm = 1.0 * 2;
// The length of the side of each square tile
constexpr float cara_tilesize = cara_1m * 8;
// 1.5625 centimeters in game coordinates
constexpr float cara_1m_over_64 = 1.0 / 2 / 64;
// 0.390625 centimeters in game coordinates
constexpr float cara_1m_over_256 = 1.0 / 2 / 256;

// Two frames in flight, two sets of command pools?
constexpr unsigned int cara_max_nr_fif = 2;
// The number of asset loader command pools
constexpr unsigned int nr_al_cmd_pool_sets = 2;
  //_arr_elem_cnt(
  //    _typeof_member(struct cara_io_worker, nr_vram_jobs_to_get_done)
  //    );

// Granularity of the length of framebuffers' sides and swapchain images' sides
constexpr unsigned int framebuf_dims_granularity = 16;
static_assert( _is_power_of_two(framebuf_dims_granularity) );

// graphics-related signs for semaphore
enum cara_signs_born_gr : uint64_t {
  csb_top_of_pipeline,
  csb_swapchain_img_acquired,
  csb_main_render_done,
  csb_present_swapchain_img,
  cara_nr_signs_gr,
};
static_assert( _is_power_of_two(cara_nr_signs_gr) );
// staging-related signs for semaphore
enum cara_signs_born_tr : uint64_t {
  csb_data_trans_has_started,
  csb_data_trans_done,
  cara_nr_signs_tr,
};
static_assert( _is_power_of_two(cara_nr_signs_tr) );

// frustum_cull + lowres_cull  TODO it's unimplemented yet
enum cara_compute_pass : unsigned char {
  cp_cull,  // Frustum cull and low-res cull
  cara_nr_compute_passes,
};
// cs_z_only -> cluster_cull -> cs_opaq_objs -> cs_non_opaque_objs
enum cara_subpass : unsigned char {
  cs_z_only,  // Depth pre-pass
  cs_opaq_objs,  // Render opaque objects
  cara_nr_subpasses,
};
constexpr unsigned int cara_nr_renderpasses = 2;
// cs2_copy_to_sc -> cs2_draw_text
enum cara_subpass_in_2nd_rp : unsigned char {
  cs2_copy_to_sc,  // Copy from color buffer to the swapchain image
  //cs2_draw_text,  // Render MC-SDF text  TODO implement it
  cara_nr_subpasses_in_2nd_rp,
};

enum cara_framebuffers : unsigned char {
  fb_zbuf,  // Depth
  fb_cbuf,  // Color
  fb_swapchain,  // Swapchain image
  cara_nr_framebuffers,
};

constexpr unsigned int cara_nr_tex_in_texarr = 4;
constexpr unsigned int cara_max_nr_albmaps = 128;

//-------------------------------------------------------------------------------------------------

/**
 * \brief VkCommandPool, VkCommandBuffer, VkFramebuffer, VkImageView, VkImage
 */
struct graphics_cmd_bufs {
  VkCommandPool * arr_cmd_pools;  ///< The number of the entries in the array is equal to
                                  ///<  1+[the number of workers] Command pools cannot be accessed at
                                  ///< the same time from multiple threads (command buffers from the
                                  ///< same pool cannot be recorded on multiple threads at the same
                                  ///< time).
  //VkCommandBuffer * arr_cmd_bufs;  ///< The number of the entries in the array is equal to
                                     ///< cara_max_nr_subpasses*[the number of workers].
                                     ///< Use cara_get_arr_cmd_bufs_for_graphics to get a pointer to
                                     ///< it
  static_assert( alignof(void*) >= alignof(VkCommandBuffer) );
  VkCommandBuffer primary_cmd_buf;  ///< Primary command buffer coming from the last element of
                                    ///<  `arr_cmd_pools`
};

/**
 * \brief VkCommandPool, VkCommandBuffer for transferring data
 */
struct trans_cmd_bufs {
  VkCommandPool * arr_cmd_pools;  ///< The number of the entries in the array is equal to
                                  ///< 1+(2*[the number of workers]) Command pools cannot be accessed
                                  ///< at the same time from multiple threads (command buffers from
                                  ///< the same pool cannot be recorded on multiple threads at the
                                  ///< same time).
  //VkCommandBuffer * arr_cmd_bufs;  ///< The number of the entries in the array is equal to
                                     ///< cara_max_nr_subpasses*[the number of workers].
                                     ///< Use cara_get_arr_cmd_bufs_for_transfer to get a pointer to
                                     ///< it
  static_assert( alignof(void*) >= alignof(VkCommandBuffer) );
  VkCommandBuffer primary_cmd_buf;  ///< A primary command buffer coming from the last element of
                                    ///<  `arr_cmd_pools`
};


union extent_2d {
  struct {
    uint16_t width;
    uint16_t height;
  } parts;
  uint32_t whole;
  atomic_uint_least32_t atomic_whole;
  static_assert( sizeof(atomic_uint_least32_t) == sizeof(uint32_t) );
};

typedef uint8_t cara_renderer_flag_bits_t;
static const cara_renderer_flag_bits_t cara_renderer_separate_trans_q_for_ass_ld =
  (typeof(cara_renderer_flag_bits_t))1 << 0;

/*
 * \brief A data storage for Vulkan rendering
 */
struct cara_vk_renderer {
  int64_t asset_size;  // const unsigned int len_mipmapped_2048 = 16 + 16 + (4*4)*4/4 + (8*8)*4/4 + (16*16)*4/4 + (32*32)*4/4 + (64*64)*4/4 + (128*128)*4/4 + (256*256)*4/4 + (512*512)*4/4 + (1024*1024)*4/4 + (2048*2048)*4/4; ..Plus header size for now
  int64_t len_z_only;
  int64_t len_z_only_frag;
  int64_t len_simple_vert;
  int64_t len_simple_frag;
  int64_t len_swapchain_vert;
  int64_t len_swapchain_frag;
  int64_t len_mcsdf_font_vert;
  int64_t len_mcsdf_font_frag;
  static_assert( alignof(int64_t) >= alignof(void*) );
  char * alb_stone;
  char * alb_sand;
  void * spv_z_only;
  void * spv_z_only_frag;
  void * spv_simple_vert;
  void * spv_simple_frag;
  void * spv_swapchain_vert;
  void * spv_swapchain_frag;
  void * spv_mcsdf_font_vert;
  void * spv_mcsdf_font_frag;
  // TODO mrange-> mbi
  struct vk_mbind_info mbi_vbuf;  // Memory allocation range in the staging buffer
  struct vk_mbind_info mbi_abuf;
  struct vk_mbind_info mbi_ibuf;  // Memory allocation range in the staging buffer
  struct vk_mbind_info mbi_albmaps[cara_max_nr_albmaps];  // Memory allocation range in staging buffer
  struct vk_mbind_info mbi_zbuf_n_cbuf[cara_max_nr_fif][2];  ///> Memory allocation ranges in the
                                                             ///>  staging buffer
  SDL_Window * window;
  VkImage * arr_swapchain_imgs;
  VkImageView * arr_swapchain_iv;
  VkSemaphore * arr_sc_present_semphos;  // One binary semaphore per swapchain img
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

  static_assert( alignof(VkSwapchainKHR) >=
      alignof(VkPhysicalDeviceDescriptorIndexingPropertiesEXT) );
  VkPhysicalDeviceDescriptorIndexingPropertiesEXT desc_indexing_props;

  VkBuffer vert_buf;  // vertex attributes
  VkBuffer attr_buf;
  VkBuffer ind_buf;  // vertex indices
  VkImage zbuf_n_cbuf[cara_max_nr_fif][2];
  VkImageView zbuf_n_cbuf_iv[cara_max_nr_fif][2];
  VkSampler cbuf_sampler;
  VkImage albmaps[cara_max_nr_albmaps];  // n-layer image of 2048*2048
  VkImageView albmap_ivs[cara_max_nr_albmaps];
  VkSampler albmap_sampler;
  VkDescriptorSetLayout dsl_z_only;
  VkDescriptorSetLayout dsl_main_render_img;
  VkDescriptorSetLayout dsl_main_render_sampler;
  VkDescriptorSetLayout dsl_cp2sc;
  VkDescriptorSetLayout dsl_mcsdf_font;
  VkDescriptorPool descriptor_pool_main_img;
  VkDescriptorPool descriptor_pool_main_sampler;
  VkDescriptorPool descriptor_pool_cbuf;
  VkDescriptorSet ds_main_render_img;
  VkDescriptorSet ds_main_render_sampler;
  VkDescriptorSet ds_cp2sc[cara_max_nr_fif];
  VkDescriptorSet ds_mcsdf_font;
  VkPipelineLayout pll_z_only;
  VkPipelineLayout pll_main_render;
  VkPipelineLayout pll_cp2sc;
  VkPipelineLayout pll_mcsdf_font;
  VkPipeline arr_pipelines[(unsigned int)cara_nr_subpasses + cara_nr_subpasses_in_2nd_rp];
  //VkPipeline arr_pipelines2[cara_nr_subpasses_in_2nd_rp];
  VkRenderPass arr_renderpasses[cara_nr_renderpasses];
  VkFramebuffer arr_framebuffers[cara_max_nr_fif][cara_nr_renderpasses];
  VkShaderModule sm_z_only;
  VkShaderModule sm_z_only_frag;
  VkShaderModule sm_simple_vert;
  VkShaderModule sm_simple_frag;
  VkShaderModule sm_cp2sc_vert;
  VkShaderModule sm_cp2sc_frag;
  VkShaderModule sm_mcsdf_font_vert;
  VkShaderModule sm_mcsdf_font_frag;

  VkCommandPool dummy_pool[cara_max_nr_fif];
  VkCommandBuffer dummy_cmdbuf[cara_max_nr_fif];
  VkSurfaceKHR presentation_surface;
  VkSemaphore arr_semphos_tr[cara_max_nr_fif];  // a timeline semaphore for transfer
  VkSemaphore arr_semphos_gr[cara_max_nr_fif];   // a timeline semaphore for graphics
  VkSemaphore arr_sc_acquire_semphos[cara_max_nr_fif];  // binary semaphores
  VkQueue graphics_q;  ///< WARNING This queue can be shared for other tasks
  VkQueue transfer_q;  ///< WARNING This queue can be shared for other tasks
  VkQueue loader_q;  ///< WARNING A queue for asset loading; It can be same with another queue
  VkQueue compute_q;   ///< WARNING This queue can be shared for other tasks
  static_assert( alignof(VkQueue) >= alignof(struct graphics_cmd_bufs) );
  struct graphics_cmd_bufs arr_graphics_cmdbuf_sets[cara_max_nr_fif];
  static_assert( alignof(struct graphics_cmd_bufs) >= alignof(struct trans_cmd_bufs) );
  struct trans_cmd_bufs arr_trans_cmdbuf_sets[nr_al_cmd_pool_sets + cara_max_nr_fif];  //\
                                      // The last cara_max_nr_fif sets are for general graphics work
                                      // The first few are for asset loading
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
  uint32_t transfer_q_fam_idx;
  uint32_t loader_q_fam_idx;
  uint32_t compute_q_fam_idx;
  uint32_t nr_old_swapchains;  // It tells how many slots in `old_swapchains` are being used.
  atomic_uint_least32_t frame_id;  // the frame id of the 1st frame rendered will be cara_max_nr_fif.
  static_assert( sizeof(atomic_uint_least32_t) == sizeof(uint32_t) );
  float max_anisotropy;
  union extent_2d win_size;  ///< The current dimensions of the window, it can be different from the
                            ///<  the framebuffers and swapchain images. See dims_swapchain_imgs
  union extent_2d max_win_size;  // The width and height of the framebuffers and swapchain images
  union extent_2d max_win_size_avail;  // The maximum size available on the presentation surface
  static_assert( alignof(int32_t) == alignof(float) && alignof(float) >= alignof(union extent_2d) &&
      alignof(union extent_2d) >= alignof(VkSurfaceTransformFlagBitsKHR));
  VkSurfaceTransformFlagBitsKHR vk_surface_transform_flags;  // Can I assume it's always .*identity?
  static_assert( alignof(VkSurfaceTransformFlagBitsKHR) >= alignof(VkPresentModeKHR) );
  VkPresentModeKHR present_mode;
  static_assert( alignof(VkPresentModeKHR) >= alignof(VkColorSpaceKHR) );
  VkColorSpaceKHR swapchain_color_space;
  uint8_t nr_swapchain_imgs;
  uint8_t nr_swapchain_imgs_we_request;
  uint8_t tex_fmt_chosen; // either texture_format_astc or texture_format_s3tc
  uint8_t flags;
};

struct dead_swapchain {
  struct vk_mbind_info arr_mbi_zbuf_n_cbuf[cara_max_nr_fif][2];
  static_assert( alignof(struct vk_mbind_info) >= alignof(VkImage) );
  VkImage arr_zbuf_n_cbuf[cara_max_nr_fif][2];
  static_assert( alignof(VkImage) >= alignof(VkImageView) );
  VkImageView arr_zbuf_n_cbuf_iv[cara_max_nr_fif][2];
  static_assert( alignof(VkImageView) >= alignof(VkFramebuffer) );
  VkFramebuffer arr_framebuffers[cara_max_nr_fif][cara_nr_renderpasses];
  static_assert( alignof(VkFramebuffer) >= alignof(VkSwapchainKHR) );
  VkSwapchainKHR swapchain;
  static_assert( alignof(VkSwapchainKHR) >=
      alignof(_typeof_member(struct cara_vk_renderer, frame_id)) &&
      alignof(uint32_t) == alignof(_typeof_member(struct cara_vk_renderer, frame_id)) );
  uint32_t mtype[cara_max_nr_fif][2];
  uint32_t destruct_time;
};
static_assert( _sizeof_member(struct dead_swapchain, destruct_time) ==
    _sizeof_member(struct cara_vk_renderer, frame_id) );

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
static inline VkCommandBuffer * cara_get_arr_cmd_bufs_for_graphics(
    uint_fast8_t nr_workers, uint_fast8_t fif_idx )
{
  const VkCommandPool * first_pool = cara_vk.arr_graphics_cmdbuf_sets[0].arr_cmd_pools;

  VkCommandBuffer * ret;
  ret = (void*)( first_pool + ( 1 + nr_workers ) * cara_max_nr_fif );
  ret = ret + fif_idx * nr_workers * cara_nr_subpasses;

  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline VkCommandBuffer * cara_get_arr_cmd_bufs_for_transfer(
    uint_fast8_t nr_workers, uint_fast8_t set_idx )
{
  const VkCommandPool * first_pool = cara_vk.arr_trans_cmdbuf_sets[set_idx].arr_cmd_pools;

  VkCommandBuffer * first_cmd_buf;
  first_cmd_buf = (void*)( first_pool + ( 1 + nr_workers ) );

  return first_cmd_buf;
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief Create a VkRenderPass and a VkFramebuffer
 */
enum err_msg create_renderpass_and_framebuffer_first_time( struct cara_vk_renderer * renderer );

//-------------------------------------------------------------------------------------------------
/*
 * \brief Create a VkRenderPass and a VkFramebuffer
 */
enum err_msg expand_swapchain_and_framebuffer( struct cara_vk_renderer * renderer,
    union extent_2d cur_dims );

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
