#include <assert.h>
#if !defined CARBONARA_RENDERER_H_
#define CARBONARA_RENDERER_H_


#include <stdint.h>


//-------------------------------------------------------------------------------------------------

// 3D Vertex position
struct v_pos {
  float x, y, z;
};
// Euler angles
struct euler_ang {
  float x;  // pitch
  float y;  // yaw
  float z;  // roll
};
// 2D Texture coordinates
struct uv_coords {
  float u, v;
};
// Vertex indices for each triangle. Let us keep the order counter-clockwise.
struct v_idx {
  uint16_t indices[3];
};
typedef uint32_t tex_id_t;
typedef float cull_bound_t;

// It tells you how to access the vertex buffer
struct cara_mesh {
  uint32_t start_pos;
  uint16_t v_cnt;
};
// It contains cull bounds and struct cara_mesh indices for each mip level.
struct cara_meshinfo {
  cull_bound_t cb;  // cull bounds (just a radius atm)
  uint32_t arr_mesh_indices[8 - 1];  // The first element contains the idx to the mesh for mip lv 0
};


// It can contain up to 8 struct cara_meshinfo
struct cara_render_obj {
  uint32_t arr_meshinfo_indices[8];  // 0 means no more meshinfo
};
// The first struct cara_render_obj in an entity has cull bounds and the info about root mesh.
struct first_render_obj {
  cull_bound_t cb;  // cull bounds (just a radius atm)
  uint32_t arr_meshinfo_indices[8 - 1];  // 0 means no more meshinfo
};
static_assert( sizeof(struct cara_render_obj) == sizeof(struct first_render_obj) );
// Union of struct first_render_obj and struct cara_render_obj
union u_cara_render_obj {
  struct cara_render_obj non_first;
  struct first_render_obj first;
};

struct cara_render_obj_pkg {
  cull_bound_t cb; 
  uint32_t root_meshinfo_idx;
  uint32_t root_tex_id_idx;
  uint32_t root_transform_idx;
  uint8_t nr_meshinfos;
  uint8_t flags;
};

struct bone {
  struct v_pos joints[4];
  struct euler_ang joint_ang[4];
};

// linked list of lists




#endif  // CARBONARA_RENDERER_H_
