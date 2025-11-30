#if !defined SALAMI_VERTEX_H_
#define SALAMI_VERTEX_H_


#include <stdint.h>
#include <math.h>


//-------------------------------------------------------------------------------------------------
//struct vec3 {
//  float x;
//  float y;
//  float z;
//};

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline void normalize_vector( const float * restrict src, float * restrict dst )
{
  float length = sqrtf(src[0]*src[0] + src[1]*src[1] + src[2]*src[2]);
  dst[0] = src[0] / length;
  dst[1] = src[1] / length;
  dst[2] = src[2] / length;
}
//void normalize_vectors( const float * restrict src, float * restrict dst );
//void normalize_vectors_ap( const float * restrict src, float * restrict dst );


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

constexpr float identity_matrix[] = {
  1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};
//struct matrix translation_matrix = {
//  {1.0f, 0.0f, 0.0f, 0.0f},
//  {0.0f, 1.0f, 0.0f, 0.0f},
//  {0.0f, 0.0f, 1.0f, 0.0f},
//  {   x,    y,    z, 1.0f},
//};

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline void get_translation_matrix( float x, float y, float z, float * dst )
{
  dst[4*0 + 0] = 1.0f;
  dst[4*0 + 1] = 0.0f;
  dst[4*0 + 2] = 0.0f;
  dst[4*0 + 3] = 0.0f;

  dst[4*1 + 0] = 0.0f;
  dst[4*1 + 1] = 1.0f;
  dst[4*1 + 2] = 0.0f;
  dst[4*1 + 3] = 0.0f;

  dst[4*2 + 0] = 0.0f;
  dst[4*2 + 1] = 0.0f;
  dst[4*2 + 2] = 1.0f;
  dst[4*2 + 3] = 0.0f;

  dst[4*3 + 0] = x;
  dst[4*3 + 1] = y;
  dst[4*3 + 2] = z;
  dst[4*3 + 3] = 1.0f;
}
//void get_translation_matrices(const float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);
//void get_translation_matrices_ap(const float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);

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
  dst[4*0 + 1] = 0.0f;
  dst[4*0 + 2] = 0.0f;
  dst[4*0 + 3] = 0.0f;

  dst[4*1 + 0] = 0.0f;
  dst[4*1 + 1] = y;
  dst[4*1 + 2] = 0.0f;
  dst[4*1 + 3] = 0.0f;

  dst[4*2 + 0] = 0.0f;
  dst[4*2 + 1] = 0.0f;
  dst[4*2 + 2] = z;
  dst[4*2 + 3] = 0.0f;

  dst[4*3 + 0] = 0.0f;
  dst[4*3 + 1] = 0.0f;
  dst[4*3 + 2] = 0.0f;
  dst[4*3 + 3] = 1.0f;
}
//void get_scaling_matrices(const float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);
//void get_scaling_matrices_ap(const float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);

//-------------------------------------------------------------------------------------------------
/**
 * \param fov The angle of the vertical field of view.
 * \param aspect_raito The width divided by the height of the renderable area.
 * \param near_plane The distance from the viewer to the near clipping plane.
 * \param far_plane The distance from the viewer to the far clipping plane.
 */
__attribute__((always_inline))
inline void get_perspective_proj_matrix(
    float fov,
    float aspect_raito,
    float near_plane,
    float far_plane,
    float * dst )
{
  const float f = 1.0f / tanf(0.5f * fov);

  dst[4*0 + 0] = f / aspect_raito;
  dst[4*0 + 1] = 0.0f;
  dst[4*0 + 2] = 0.0f;
  dst[4*0 + 3] = 0.0f;

  dst[4*1 + 0] = 0.0f;
  dst[4*1 + 1] = -f;
  dst[4*1 + 2] = 0.0f;
  dst[4*1 + 3] = 0.0f;

  dst[4*2 + 0] = 0.0f;
  dst[4*2 + 1] = 0.0f;
  dst[4*2 + 2] = far_plane / (near_plane - far_plane);
  dst[4*2 + 3] = -1.0f;

  dst[4*3 + 0] = 0.0f;
  dst[4*3 + 1] = 0.0f;
  dst[4*3 + 2] = (near_plane * far_plane) / (near_plane - far_plane);
  dst[4*3 + 3] = 0.0f;
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
  dst[4*0 + 0] = 2.0f / (right - left);
  dst[4*0 + 1] = 0.0f;
  dst[4*0 + 2] = 0.0f;
  dst[4*0 + 3] = 0.0f;

  dst[4*1 + 0] = 0.0f;
  dst[4*1 + 1] = 2.0f / (bottom - top);
  dst[4*1 + 2] = 0.0f;
  dst[4*1 + 3] = 0.0f;

  dst[4*2 + 0] = 0.0f;
  dst[4*2 + 1] = 0.0f;
  dst[4*2 + 2] = 1.0f / (near_plane - far_plane);
  dst[4*2 + 3] = 0.0f;

  dst[4*3 + 0] = -(right + left) / (right - left);
  dst[4*3 + 1] = -(bottom + top) / (bottom - top);
  dst[4*3 + 2] = near_plane / (near_plane - far_plane);
  dst[4*3 + 3] = 1.0f;
}


#endif  // SALAMI_VERTEX_H_
