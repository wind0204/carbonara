#include "carbonara-math.h"



extern inline void normalize_vector( const float * restrict src, float * restrict dst );

extern void get_translation_matrix( float x, float y, float z, float * dst );
// void get_translation_matrices(float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);
// void get_translation_matrices_ap(float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);

__attribute__((always_inline))
static inline void get_rotaion_matrix_impl(
    const float * restrict pivot,
    float angle,
    float * restrict dst )
{
  const float cos = cosf(angle);
  const float one_minus_c = 1.0f - cos;
  const float sin = sinf(angle);

  const float x = pivot[0];
  const float y = pivot[1];
  const float z = pivot[2];

  // I guess the compiler would probably vectorize it since there is a
  // quite a bit of computation with a very small number of memory loads.
  // # of Mult :  3 +6 +3 = 12
  // # of Add/Sub :  3 +3*2 = 9
  // # of Load :  3 float

  // x * x*(1-c) + c
  // y * y*(1-c) + c
  // z * z*(1-c) + c
  // x * y*(1-c) [(-zs)|(+zs)]
  // x * z*(1-c) [(-ys)|(+ys)]
  // y * z*(1-c) [(-xs)|(+xs)]

  dst[4*0 + 0] = x * x * one_minus_c + cos;
  dst[4*0 + 1] = y * x * one_minus_c - z * sin;
  dst[4*0 + 2] = z * x * one_minus_c + y * sin;
  dst[4*0 + 3] = 0.0f;

  dst[4*1 + 0] = x * y * one_minus_c + z * sin;
  dst[4*1 + 1] = y * y * one_minus_c + cos;
  dst[4*1 + 2] = z * y * one_minus_c - x * sin;
  dst[4*1 + 3] = 0.0f;

  dst[4*2 + 0] =  x * z * one_minus_c - y * sin;
  dst[4*2 + 1] =  y * z * one_minus_c + x * sin;
  dst[4*2 + 2] = z * z * one_minus_c + cos;
  dst[4*2 + 3] = 0.0f;

  dst[4*3 + 0] = 0.0f;
  dst[4*3 + 1] = 0.0f;
  dst[4*3 + 2] = 0.0f;
  dst[4*3 + 3] = 1.0f;
}

void get_rotaion_matrix( const float * restrict pivot,
    float angle,
    float * restrict dst )
{
  get_rotaion_matrix_impl(pivot, angle, dst);
}

void get_rotaion_matrix_ap( const float * restrict pivot,
    float angle,
    float * restrict dst )
{
  pivot = __builtin_assume_aligned(pivot, 128/8);
  dst = __builtin_assume_aligned(dst, 128/8);

  get_rotaion_matrix_impl(pivot, angle, dst);
}

//void get_rotaion_matrices(const float * restrict arr_pivots, const float * restrict angles,
//    float * restrict dst, uint_fast16_t nr_matrices);
//void get_rotaion_matrices_ap(const float * restrict arr_pivots, const float * restrict angles,
//    float * restrict dst, uint_fast16_t nr_matrices);

extern void get_scaling_matrix( float x, float y, float z, float * dst );
//void get_scaling_matrices(const float * restrict arr_xyz, float * restrict dst, uint_fast16_t nr_matrices);
