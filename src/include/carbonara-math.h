#if !defined SALAMI_VERTEX_H_
#define SALAMI_VERTEX_H_


// Mesh space ( original mesh + local transformation including rotation around an entity's center )
//   + Entity space ( a group of meshes with a center point )
//   + Group space ( a group of entities with a center point )
// World space ( mesh space + translation relative to camera )
//   + Canonical World-view space ( mesh space + translation relative to a canonical origin )
// Camera-view space ( world space + camera orientation )
// NDC space ( We finally get the normalized device coordinates )


#include <stdint.h>
#include <math.h>

#include "util-c_lang-basic.h"


typedef float float4 __attribute__((ext_vector_type(4)));
typedef float float3 __attribute__((ext_vector_type(3)));
typedef float float2 __attribute__((ext_vector_type(2)));
typedef float4 vec4;
typedef float3 vec3;
typedef float2 vec2;


//-------------------------------------------------------------------------------------------------
// Dot Product
// a.b = ‖a‖ ‖b‖ cosθ,  Derived formula: θ = acos( a.b/(‖a‖‖b‖) )
// k(a.b) = ka.b = a.kb  (k is a scalar)
// a.b = b.a
// a.(b+c) = a.b + a.c
//
// sift(a, p) = a.p/‖p‖/‖p‖;  // Get the p-coordinate of a, note that when ‖p‖ is 1 (We've got an
//                            //   orthonormal basis) It becomes just one dot product with no div.
//                            // WARNING `sift` won't work if the basis vectors are not orthogonal.
// ‖v‖² = v.v,  ‖v‖ = sqrt(v.v)
// The result is 0 when one is perpendicular to the other vector or is 0

__attribute__((always_inline))
inline float dot_3( vec3 v1, vec3 v2 )
{
  float ret = 0;
  for ( unsigned int idx = 0; idx < 3; idx++ ) {
    ret += v1[idx] * v2[idx];
  }
  return ret;
}
__attribute__((always_inline))
inline float dot_2( vec2 v1, vec2 v2 )
{
  float ret = 0;
  for ( unsigned int idx = 0; idx < 2; idx++ ) {
    ret += v1[idx] * v2[idx];
  }
  return ret;
}

//-------------------------------------------------------------------------------------------------
// Cross Product
// ‖axb‖ = ‖a‖ ‖b‖ sinθ
// k(axb) = kaxb = axkb  (k is a scalar)
// ax(b+c) = axb + axc
//
// if ( "a ↷(Cw)θ = b" from your PoV when you have placed the tail of b at the head of a )
//   axb is directed towards you (in a left-handed coordinate system)
// if ( "a ↶(CCw)θ = b" from your PoV when you have placed the tail of b at the head of a )
//   axb is headed away from you (in a left-handed coordinate system)
// Examples: x̂ x ŷ = ẑ,  ŷ x x̂ = -ẑ,  ŷ x ẑ = x̂,  ẑ x ŷ = -x̂,  ẑ x x̂ = ŷ,  x̂ x ẑ = -ŷ
// axb = -(bxa)
// axb = -ax-b
//
// (axb)xc != ax(bxc)
// The result is 0 when one is parallel to the other vector or is 0

__attribute__((always_inline))
inline vec3 cross_3( vec3 v1, vec3 v2 )
{
  vec3 ret = {
    v1.y * v2.z - v1.z * v2.y,  // x
    v1.z * v2.x - v1.x * v2.z,  // y
    v1.x * v2.y - v1.y * v2.x,  // z
  };
  return ret;
}

//-------------------------------------------------------------------------------------------------
//The operation a.bxc is known as the triple product
//

//-------------------------------------------------------------------------------------------------
struct mat3x3 {
  vec3 rows[3];
};
struct mat4x4 {
  alignas(ALIGN_FOR_AVX)
  vec4 rows[4];
};
union u_mat4x4 {
  alignas(ALIGN_FOR_AVX)
  float simple_arr[4*4];
  struct mat4x4 mat;
};


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline struct mat4x4 mul_4x4_4x4(
    struct mat4x4 a, struct mat4x4 b )
{
  struct mat4x4 ret;

  ret.rows[0].x =
    a.rows[0].x * b.rows[0].x +  a.rows[0].y * b.rows[1].x +
    a.rows[0].z * b.rows[2].x +  a.rows[0].w * b.rows[3].x;
  ret.rows[0].y =
    a.rows[0].x * b.rows[0].y +  a.rows[0].y * b.rows[1].y +
    a.rows[0].z * b.rows[2].y +  a.rows[0].w * b.rows[3].y;
  ret.rows[0].z =
    a.rows[0].x * b.rows[0].z +  a.rows[0].y * b.rows[1].z +
    a.rows[0].z * b.rows[2].z +  a.rows[0].w * b.rows[3].z;
  ret.rows[0].w =
    a.rows[0].x * b.rows[0].w +  a.rows[0].y * b.rows[1].w +
    a.rows[0].z * b.rows[2].w +  a.rows[0].w * b.rows[3].w;

  ret.rows[1].x =
    a.rows[1].x * b.rows[0].x +  a.rows[1].y * b.rows[1].x +
    a.rows[1].z * b.rows[2].x +  a.rows[1].w * b.rows[3].x;
  ret.rows[1].y =
    a.rows[1].x * b.rows[0].y +  a.rows[1].y * b.rows[1].y +
    a.rows[1].z * b.rows[2].y +  a.rows[1].w * b.rows[3].y;
  ret.rows[1].z =
    a.rows[1].x * b.rows[0].z +  a.rows[1].y * b.rows[1].z +
    a.rows[1].z * b.rows[2].z +  a.rows[1].w * b.rows[3].z;
  ret.rows[1].w =
    a.rows[1].x * b.rows[0].w +  a.rows[1].y * b.rows[1].w +
    a.rows[1].z * b.rows[2].w +  a.rows[1].w * b.rows[3].w;

  ret.rows[2].x =
    a.rows[2].x * b.rows[0].x +  a.rows[2].y * b.rows[1].x +
    a.rows[2].z * b.rows[2].x +  a.rows[2].w * b.rows[3].x;
  ret.rows[2].y =
    a.rows[2].x * b.rows[0].y +  a.rows[2].y * b.rows[1].y +
    a.rows[2].z * b.rows[2].y +  a.rows[2].w * b.rows[3].y;
  ret.rows[2].z =
    a.rows[2].x * b.rows[0].z +  a.rows[2].y * b.rows[1].z +
    a.rows[2].z * b.rows[2].z +  a.rows[2].w * b.rows[3].z;
  ret.rows[2].w =
    a.rows[2].x * b.rows[0].w +  a.rows[2].y * b.rows[1].w +
    a.rows[2].z * b.rows[2].w +  a.rows[2].w * b.rows[3].w;

  ret.rows[3].x =
    a.rows[3].x * b.rows[0].x +  a.rows[3].y * b.rows[1].x +
    a.rows[3].z * b.rows[2].x +  a.rows[3].w * b.rows[3].x;
  ret.rows[3].y =
    a.rows[3].x * b.rows[0].y +  a.rows[3].y * b.rows[1].y +
    a.rows[3].z * b.rows[2].y +  a.rows[3].w * b.rows[3].y;
  ret.rows[3].z =
    a.rows[3].x * b.rows[0].z +  a.rows[3].y * b.rows[1].z +
    a.rows[3].z * b.rows[2].z +  a.rows[3].w * b.rows[3].z;
  ret.rows[3].w =
    a.rows[3].x * b.rows[0].w +  a.rows[3].y * b.rows[1].w +
    a.rows[3].z * b.rows[2].w +  a.rows[3].w * b.rows[3].w;

  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline void normalize_vec3( const vec3 * restrict src, vec3 * restrict dst )
{
  float length = sqrtf( src->x * src->x +  src->y * src->y +  src->z * src->z );
  dst->x = src->x / length;
  dst->y = src->y / length;
  dst->z = src->z / length;
}


//-------------------------------------------------------------------------------------------------
//struct matrix {
//  float c1[4];
//  float c2[4];
//  float c3[4];
//  float c4[4];
//};

//   Identity  Translation   Rotation    Scaling   Perspective Orthographic
// ┌         ┐ ┌         ┐ ┌         ┐ ┌         ┐ ┌         ┐ ┌         ┐
// │ 1 0 0 0 │ │ 1 0 0 x │ │ ? ? ? 0 │ │ x 0 0 0 │ │ 1 0 0 0 │ │ ? 0 0 ? │
// │ 0 1 0 0 │ │ 0 1 0 y │ │ ? ? ? 0 │ │ 0 y 0 0 │ │ 0 1 0 0 │ │ 0 ? 0 ? │
// │ 0 0 1 0 │ │ 0 0 1 z │ │ ? ? ? 0 │ │ 0 0 z 0 │ │ 0 0 1 0 │ │ 0 0 ? ? │
// │ 0 0 0 1 │ │ 0 0 0 1 │ │ 0 0 0 1 │ │ 0 0 0 1 │ │ 0 0-1 0 │ │ 0 0 0 1 │
// └         ┘ └         ┘ └         ┘ └         ┘ └ Mv=v'   ┘ └         ┘

constexpr struct mat4x4 identity_4x4 = {
  .rows = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
  },
};

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline struct mat4x4 translate_4x4( float x, float y, float z )
{
  struct mat4x4 ret = {
    (vec4) { 1, 0, 0, 0 },
    (vec4) { 0, 1, 0, 0 },
    (vec4) { 0, 0, 1, 0 },
    (vec4) { x, y, z, 1 },
  };
  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline struct mat4x4 rot_axa_4x4( float angle )
{
  float sin = sinf(angle);
  float cos = cosf(angle);
  struct mat4x4 ret = {
    (vec4) { 1, 0, 0, 0 },
    (vec4) { 0, cos, sin, 0 },
    (vec4) { 0, -sin, cos, 0 },
    (vec4) { 0, 0, 0, 1 },
  };
  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline struct mat4x4 rot_aya_4x4( float angle )
{
  float sin = sinf(angle);
  float cos = cosf(angle);
  struct mat4x4 ret = {
    (vec4) { cos, 0, -sin, 0 },
    (vec4) { 0, 1, 0, 0 },
    (vec4) { sin, 0, cos, 0 },
    (vec4) { 0, 0, 0, 1 },
  };
  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline struct mat4x4 rot_aza_4x4( float angle )
{
  float sin = sinf(angle);
  float cos = cosf(angle);
  struct mat4x4 ret = {
    (vec4) { cos, sin, 0, 0 },
    (vec4) { -sin, cos, 0, 0 },
    (vec4) { 0, 0, 1, 0 },
    (vec4) { 0, 0, 0, 1 },
  };
  return ret;
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Get a rotation matrix.
 * \param pivot The pivot vector to rotate things around, it should be normalized
 */
void get_rotaion_matrix( const float * restrict pivot, float angle, float * restrict dst );
void get_rotaion_matrix_ap( const float * restrict pivot, float angle, float * restrict dst );
//void get_rotaion_matrices(const float * restrict arr_pivots, const float * restrict angles,
//    float * restrict dst, uint_fast16_t nr_matrices);
//void get_rotaion_matrices_ap(const float * restrict arr_pivots, const float * restrict angles,
//    float * restrict dst, uint_fast16_t nr_matrices);

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline void get_scaling_matrix( float x, float y, float z, float * dst )
{
  dst[4*0 + 0] = x;
  dst[4*0 + 1] = 0;
  dst[4*0 + 2] = 0;
  dst[4*0 + 3] = 0;

  dst[4*1 + 0] = 0;
  dst[4*1 + 1] = y;
  dst[4*1 + 2] = 0;
  dst[4*1 + 3] = 0;

  dst[4*2 + 0] = 0;
  dst[4*2 + 1] = 0;
  dst[4*2 + 2] = z;
  dst[4*2 + 3] = 0;

  dst[4*3 + 0] = 0;
  dst[4*3 + 1] = 0;
  dst[4*3 + 2] = 0;
  dst[4*3 + 3] = 1;
}
//void get_scaling_matrices(const float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);
//void get_scaling_matrices_ap(const float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);

//-------------------------------------------------------------------------------------------------
/**
 * \param vfov The angle of the vertical field of view.
 * \param aspect_ratio The width divided by the height of the render target.
 * \param near The distance from the viewer to the near clipping plane.
 * \param far The distance from the viewer to the far clipping plane.
 */
__attribute__((always_inline))
inline struct mat4x4 get_perspective_proj_mat_vk(
    float vfov,
    float aspect_ratio,
    float near,
    float far,
    bool reverse_z
    )
{
  float tan_half_theta = tanf( vfov * (1/2.0f) );
  if ( reverse_z ) {
    _swap( &near, &far );
  }
  struct mat4x4 ret = {
    (vec4) { 1 / (aspect_ratio * tan_half_theta), 0,                  0,                        0 },
    (vec4) { 0,                                   -1/tan_half_theta,  0,                        0 },
    (vec4) { 0,                                   0,                  far / (far - near),       1 },
    (vec4) { 0,                                   0,                  -far*near / (far - near), 0 },
  };

  return ret;
}

//-------------------------------------------------------------------------------------------------
/**
 * \param left The coordinate for the left vertical clipping plane.
 * \param right The coordinate for the right vertical clipping plane.
 * \param top The coordinate for the top horizontal clipping plane.
 * \param bottom The coordinate for the bottom horizontal clipping plane.
 * \param near_plane The distance from the viewer to the near clipping plane.
 * \param far_plane The distance from the viewer to the far clipping plane.
 */
__attribute__((always_inline))
inline void get_orthographic_proj_matrix(
    float left, float right, float top, float bottom,
    float near_plane, float far_plane, float * dst )
{
  dst[4*0 + 0] = 2 / (right - left);
  dst[4*0 + 1] = 0;
  dst[4*0 + 2] = 0;
  dst[4*0 + 3] = 0;

  dst[4*1 + 0] = 0;
  dst[4*1 + 1] = 2 / (bottom - top);
  dst[4*1 + 2] = 0;
  dst[4*1 + 3] = 0;

  dst[4*2 + 0] = 0;
  dst[4*2 + 1] = 0;
  dst[4*2 + 2] = 1 / (near_plane - far_plane);
  dst[4*2 + 3] = 0;

  dst[4*3 + 0] = -(right + left) / (right - left);
  dst[4*3 + 1] = -(bottom + top) / (bottom - top);
  dst[4*3 + 2] = near_plane / (near_plane - far_plane);
  dst[4*3 + 3] = 1;
}


#endif  // SALAMI_VERTEX_H_
