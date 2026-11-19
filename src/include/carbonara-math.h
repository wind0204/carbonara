#if !defined CARBONARA_MATH_H_
#define CARBONARA_MATH_H_


#include <stdint.h>
#include <math.h>

#include "util-c_lang-basic.h"
#include "util-c_lang-ieee754.h"


//-------------------------------------------------------------------------------------------------
// Coordinate spaces
//
// Mesh space: original mesh + local transformation including the rotation around entity's center
//  and the rotation around its root point
//   + Entity space: a group of meshes with a designated center point
//   + Group space: a group of entities with a designated center point
//
// Camera-as-origin space: mesh space + translation relative to camera
//   + World space: mesh space + translation relative to a global origin
//
// View space: camera-as-origin space + camera rotation
//
// Screen space: We finally get the normalized device coordinates

typedef float float4 __attribute__((ext_vector_type(4)));
typedef float float3 __attribute__((ext_vector_type(3)));
typedef float float2 __attribute__((ext_vector_type(2)));
typedef float4 vec4;
typedef float3 vec3;
typedef float2 vec2;


//-------------------------------------------------------------------------------------------------
// Dot Product
//
// a·b = ‖a‖ ‖b‖ cosθ,  Derived formula: θ = acos( a·b/(‖a‖‖b‖) )
// k(a·b) = ka·b = a·kb  (k is a scalar)
// a·b = b·a
// a·(b+c) = a·b + a·c
//
// sift(a, p) = a·p/‖p‖/‖p‖;  // Get the p-coordinate of a, note that when ‖p‖ is 1(We've got an
//                            //  orthonormal basis) it becomes just one dot product with no div.
//                            // WARNING `sift` won't work if the basis vectors are not orthogonal.
// ‖v‖² = v·v,  ‖v‖ = sqrt(v·v)

__attribute__((always_inline))
static inline float dot_3( vec3 v1, vec3 v2 )
{
  float ret = 0;
  for ( unsigned int idx = 0; idx < 3; idx++ ) {
    ret += v1[idx] * v2[idx];
  }
  return ret;
}
__attribute__((always_inline))
static inline float dot_2( vec2 v1, vec2 v2 )
{
  float ret = 0;
  for ( unsigned int idx = 0; idx < 2; idx++ ) {
    ret += v1[idx] * v2[idx];
  }
  return ret;
}


//-------------------------------------------------------------------------------------------------
// Cross Product
//
// ‖axb‖ = ‖a‖ ‖b‖ sinθ
// k(axb) = kaxb = axkb  (k is a scalar)
// ax(b+c) = axb + axc
//
// if ( "b turns clockwise by θ from the direction of a" from your PoV when you have placed the
//  tail of b at the head of a ): axb is directed towards you (in a left-handed coordinate system)
// if ( "b turns counterclockwise by θ from the direction of a" from your PoV when you have placed
//  the tail of b at the head of a ): axb is headed away from you (in a left-handed coordinate
//  system)
// Examples: x̂ x ŷ = ẑ,  ŷ x x̂ = -ẑ,  ŷ x ẑ = x̂,  ẑ x ŷ = -x̂,  ẑ x x̂ = ŷ,  x̂ x ẑ = -ŷ
// axb = -(bxa)
// axb = -ax-b
// (axb)xc != ax(bxc)

__attribute__((always_inline))
static inline vec3 cross_3( vec3 v1, vec3 v2 )
{
  vec3 ret = {
    v1.y * v2.z - v1.z * v2.y,  // x
    v1.z * v2.x - v1.x * v2.z,  // y
    v1.x * v2.y - v1.y * v2.x,  // z
  };
  return ret;
}


//-------------------------------------------------------------------------------------------------
//The operation a·bxc is known as the triple product
//


//-------------------------------------------------------------------------------------------------
// Scalar Multiplication

/**
 * \return \a s * \a v
 */
__attribute__((always_inline))
static inline vec3 scalar_mul_3( vec3 v, float s )
{
  constexpr size_t col_cnt = 3;  // _arr_elem_cnt won't work on ext_vector_type
  typeof(v) ret = v;
  for ( unsigned int col_idx = 0; col_idx < col_cnt; col_idx++ ) {
    ret[col_idx] *= s;
  }
  return ret;
}

/**
 * \return \a s * \a v
 */
__attribute__((always_inline))
static inline vec2 scalar_mul_2( vec2 v, float s )
{
  constexpr size_t col_cnt = _arr_elem_cnt(v);
  typeof(v) ret = v;
  for ( unsigned int col_idx = 0; col_idx < col_cnt; col_idx++ ) {
    ret[col_idx] *= s;
  }
  return ret;
}


//-------------------------------------------------------------------------------------------------
// Matrices
//
// MI = IM = M,  (Mᵀ)ᵀ = M,  Dᵀ=D (D is a diagonal matrix)
// MS = Another_matrix_with_same_dimension_count (M and S are square matrices)
// AB != BA
// (AB)C = A(BC)  // Note that if (AB)C is defined, then A(BC) is always defined
// (vA)B = v(AB)  // v is a vector
// (kA)B = k(AB) = A(kB)  // k is a scalar
// (AB)ᵀ = BᵀAᵀ  // This phenomenon of the individual transposed matrices in a reverse order can be
//                  extended to more than just 2 matrices
// (v+w)M = vM + wM

// a row-major 2x2 matrix
struct mat2x2 {
  alignas(ALIGN_FOR_AVX)
  vec2 rows[2];
};
// a row-major 3x3 matrix
struct mat3x3 {
  vec3 rows[3];
};
// a row-major 4x4 matrix
struct mat4x4 {
  alignas(ALIGN_FOR_AVX)
  vec4 rows[4];
};
union u_mat4x4 {
  alignas(ALIGN_FOR_AVX)
  float simple_arr[4*4];
  struct mat4x4 mat;
};

// Assume the last column of the 4x4 matrix is {0, 0, 0, 1}.
struct mat4x3 {
  vec3 rows[4];
};
/**
 * \brief Assume the last column of the 4x4 matrix is {0, 0, 0, 1}.
 *  I can consider using this struct for serialization
 */
struct mat4x3_slim {
  float rows[4][3];
};



// Matrix-Matrix Multiplication
// NOTE Consider using `matrix_type` attribute in clang

/**
 * \brief returns AB
 */
__attribute__((always_inline))
static inline struct mat4x4 mul_4x4_4x4(
    struct mat4x4 A, struct mat4x4 B )
{
  struct mat4x4 ret;
  constexpr size_t row_cnt = _arr_elem_cnt(ret.rows);

  for ( unsigned int row_idx = 0; row_idx < row_cnt; row_idx++ ) {
    ret.rows[row_idx].x =
      A.rows[row_idx].x * B.rows[0].x +  A.rows[row_idx].y * B.rows[1].x +
      A.rows[row_idx].z * B.rows[2].x +  A.rows[row_idx].w * B.rows[3].x;
    ret.rows[row_idx].y =
      A.rows[row_idx].x * B.rows[0].y +  A.rows[row_idx].y * B.rows[1].y +
      A.rows[row_idx].z * B.rows[2].y +  A.rows[row_idx].w * B.rows[3].y;
    ret.rows[row_idx].z =
      A.rows[row_idx].x * B.rows[0].z +  A.rows[row_idx].y * B.rows[1].z +
      A.rows[row_idx].z * B.rows[2].z +  A.rows[row_idx].w * B.rows[3].z;
    ret.rows[row_idx].w =
      A.rows[row_idx].x * B.rows[0].w +  A.rows[row_idx].y * B.rows[1].w +
      A.rows[row_idx].z * B.rows[2].w +  A.rows[row_idx].w * B.rows[3].w;
  }

#pragma GCC diagnostic push
// Ignore the false-positive warning
// https://github.com/llvm/llvm-project/issues/143121#issuecomment-3817033060
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
  return ret;
#pragma GCC diagnostic pop
}


//-------------------------------------------------------------------------------------------------
// Vecotr-Matrix Multiplication (Multiply a single-row matrix by a square matrix)

__attribute__((always_inline))
static inline vec4 mul_v_4x4( vec4 row, struct mat4x4 mat )
{
  vec4 ret;

  ret.x =
    row.x * mat.rows[0].x +  row.y * mat.rows[1].x +
    row.z * mat.rows[2].x +  row.w * mat.rows[3].x;
  ret.y =
    row.x * mat.rows[0].y +  row.y * mat.rows[1].y +
    row.z * mat.rows[2].y +  row.w * mat.rows[3].y;
  ret.z =
    row.x * mat.rows[0].z +  row.y * mat.rows[1].z +
    row.z * mat.rows[2].z +  row.w * mat.rows[3].z;
  ret.w =
    row.x * mat.rows[0].w +  row.y * mat.rows[1].w +
    row.z * mat.rows[2].w +  row.w * mat.rows[3].w;

  return ret;
}

__attribute__((always_inline))
static inline vec3 mul_v_3x3( vec3 row, struct mat3x3 mat )
{
  vec3 ret;

  ret.x =
    row.x * mat.rows[0].x +  row.y * mat.rows[1].x +
    row.z * mat.rows[2].x;
  ret.y =
    row.x * mat.rows[0].y +  row.y * mat.rows[1].y +
    row.z * mat.rows[2].y;
  ret.z =
    row.x * mat.rows[0].z +  row.y * mat.rows[1].z +
    row.z * mat.rows[2].z;

  return ret;
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline vec3 normalize_vec3( const vec3 src )
{
  vec3 ret;

  float length = sqrtf( src.x * src.x +  src.y * src.y +  src.z * src.z );
  ret.x = src.x / length;
  ret.y = src.y / length;
  ret.z = src.z / length;

  return ret;
}

//-------------------------------------------------------------------------------------------------
// Determinants

// The determinant of an identity matrix of any dimension is 1: |I| = 1
// The determinant of a matrix product is equal to the product of the determinants: |AB| = |A||B|
//  (This extends to more than just two matrices)
// The determinant of the transpose of a matrix is equal to the original determinant: |Mᵀ| = |M|
// If any row or column in a matrix contains all 0s, then the determinant of that matrix is 0.
// Exchanging any pair of rows negates the determinant—This same rule applies for exchanging a
//  pair of columns.
// Adding any multiple of a row(column) to another row(column) does not change the value of the
//  determinant
__attribute__((always_inline))
static inline float det_2( struct mat2x2 mat )
{
  return mat.rows[0][0] * mat.rows[1][1] - mat.rows[0][1] * mat.rows[1][0];
}

/**
 * \param mat The original matrix
 * \param r The index number of the row to remove, it must be < 3
 * \param c The index number of the column to remove, it must be < 3
 */
__attribute__((always_inline))
static inline float get_minor( struct mat3x3 mat, unsigned int r, unsigned int c )
{
  constexpr size_t row_cnt = _arr_elem_cnt(mat.rows);
  constexpr size_t col_cnt = 3;  // _arr_elem_cnt won't work on ext_vector_type
  assume( r < row_cnt );
  assume( c < col_cnt );
  struct mat2x2 submat;
  for ( typeof(r) row_idx = 0, cur_row = 0; row_idx < row_cnt; row_idx++ ) {
    if ( row_idx == r ) {
      continue;
    }
    for ( typeof(c) col_idx = 0, cur_col = 0; col_idx < row_cnt; col_idx++ ) {
      if ( col_idx == c ) {
        continue;
      }
      submat.rows[cur_row][cur_col] = mat.rows[row_idx][col_idx];
      cur_col++;
    }
    cur_row++;
  }

#pragma GCC diagnostic push
// Ignore the false-positive warning
// https://github.com/llvm/llvm-project/issues/143121#issuecomment-3817033060
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
  float det_of_submat = det_2( submat );
#pragma GCC diagnostic pop

  return det_of_submat;
}

/**
 * \return An union64 object with its arr_f[0] set to the minor and arr_f[1] set to the cofactor
 * \param mat The original matrix
 * \param r The index number of the row to remove, it must be < 3
 * \param c The index number of the column to remove, it must be < 3
 */
__attribute__((always_inline))
static inline union union64 get_minor_and_cofactor(
    struct mat3x3 mat, unsigned int r, unsigned int c )
{
  union union64 arr_b32;  // Use the array of two binary32 elements.
  float minor = get_minor( mat, r, c );
  arr_b32.arr_f[0] = minor;

  // r c r+c (r+c)&1 -1**(r+c)
  // 0 0  0      0       1
  // 0 1  1      1       -1
  // 0 2  2      0       1
  // 1 0  1      1       -1
  // 1 1  2      0       1
  // 1 2  3      1       -1
  // 2 0  2      0       1
  // 2 1  3      1       -1
  // 2 2  4      0       1
  float cofactor = (r+c)&1 ? -minor : minor;
  arr_b32.arr_f[1] = cofactor;

  return arr_b32;
}
/**
 * \return $C_{\a r \a c}$ of \a mat
 * \param mat The original matrix
 * \param r The index number of the row to remove, it must be < 3
 * \param c The index number of the column to remove, it must be < 3
 */
__attribute__((always_inline))
static inline float get_cofactor( struct mat3x3 mat, unsigned int r, unsigned int c )
{
  union union64 minor_and_cofactor = get_minor_and_cofactor( mat, r, c );
  return minor_and_cofactor.arr_f[1];
}

__attribute__((always_inline))
static inline float det_3_via_row( struct mat3x3 mat, unsigned int r )
{
  constexpr size_t col_cnt = 3;  // _arr_elem_cnt won't work on ext_vector_type

  float ret = mat.rows[r][0] * get_cofactor( mat, r, 0 );
  for ( unsigned int col_idx = 0+1; col_idx < col_cnt; col_idx++ ) {
    //arr_cofactors[col_idx] = get_cofactor( mat, r, col_idx );
    ret += mat.rows[r][col_idx] * get_cofactor( mat, r, col_idx );
  }

  return ret;
}

__attribute__((always_inline))
static inline float det_3_via_col( struct mat3x3 mat, unsigned int c )
{
  constexpr size_t row_cnt = _arr_elem_cnt(mat.rows);

  float ret = mat.rows[0][c] * get_cofactor( mat, 0, c );
  for ( unsigned int row_idx = 0+1; row_idx < row_cnt; row_idx++ ) {
    //arr_cofactors[row_idx] = get_cofactor( mat, row_idx, c );
    ret += mat.rows[row_idx][c] * get_cofactor( mat, row_idx, c );
  }

  return ret;
}

// If we interpret the rows of a 3x3 matrix as three vectors, then the determinant of the matrix is
//  equivalent to the so-called “triple product” of the three vectors: |M| = (r1 x r2)·r3
//  (And since |M| = |Mᵀ|, You can tell this property applies to the columns too.
//  The scalar triple product calculates the signed volume of a parallelepiped defined by three
//  vectors, so you can infer that the volumes of the two parallelepipeds formed by two different
//  sets of vectors from a same matrix be the same! although the shapes are not always the same.)
//  More insights: |M| = (r1 x r2)·r3 = r1·(r2 x r3) = (r3 x r1)·r2 = r2·(r3 x r1) ...
//
// The determinant of a singular matrix is zero and the determinant of a nonsingular matrix is
//  nonzero. Checking the magnitude of the determinant is the most commonly used test for
//  invertibility because it's the easiest and quickest. In ordinary circumstances, this is OK, but
//  please note that the method can break down. An example is an extreme shear matrix with basis
//  vectors that form a very long, thin parallelepiped with unit volume. This ill-conditioned
//  matrix is nearly singular, even though its determinant is 1. The condition number is the proper
//  tool for detecting such cases.
/**
 * \warning You might wanna use det_3_pivoting instead, for an optimal performance!
 */
__attribute__((always_inline))
static inline float det_3( struct mat3x3 mat )
{
  // TODO check if it's optimized and reduced down to 6+3 mul, 3 sub and 2 add
  //  The naive approach is 12 mul, 3 sub and 2 add
  return det_3_via_row( mat, 0 );
}

/**
 * \brief The more 0's the lighter the computation can get
 * \param use_row This parameter decides the meaning of \a idx
 * \param idx The index of the row to use when computing the determinant if \a use_row is true.
 *            The index of the column to use otherwise.
 */
__attribute__((always_inline))
static inline float det_3_pivoting( struct mat3x3 mat, bool use_row, unsigned int idx )
{
  if ( use_row ) {
    return det_3_via_row( mat, idx );
  } else {
    return det_3_via_col( mat, idx );
  }
}


/**
 * \return The classical adjoint of \a mat
 */
__attribute__((always_inline))
static inline struct mat3x3 get_adj( struct mat3x3 mat )
{
  constexpr size_t row_cnt = _arr_elem_cnt(mat.rows);
  constexpr size_t col_cnt = 3;  // _arr_elem_cnt won't work on ext_vector_type

  struct mat3x3 ret;
  for ( unsigned int row_idx = 0; row_idx < row_cnt; row_idx++ ) {
    for ( unsigned int col_idx = 0; col_idx < col_cnt; col_idx++ ) {
      ret.rows[col_idx][row_idx] = get_cofactor( mat, row_idx, col_idx );
    }
  }

#pragma GCC diagnostic push
// Ignore the false-positive warning
// https://github.com/llvm/llvm-project/issues/143121#issuecomment-3817033060
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
  return ret;
#pragma GCC diagnostic pop
}


//
// M⁻¹ M = I
// (M⁻¹)⁻¹ = M
// I⁻¹ = I   (Note that there are other matrices that are their own inverse. For example, consider
//           any reflection matrix, or a matrix that rotates 180 degrees about any axis.)
// (Mᵀ)⁻¹ = (M⁻¹)ᵀ
// (AB)⁻¹ = B⁻¹ A⁻¹   (This extends to more than two matrices)
// |M⁻¹| = 1/|M|
//
// There are several ways to compute the inverse of a matrix. The one we use is based on the
//  classical adjoint.
// Our method for computing the inverse of a matrix M is based on the classical adjoint. The
//  classical adjoint of a matrix M, denoted "adj M" is defined as the transpose of the matrix of
//  cofactors of M.
// There are other techniques that can be used to compute the inverse of a matrix, such as Gaussian
//  elimination. Many linear algebra textbooks assert that such techniques are better suited for
//  implementation on a computer because they require fewer arithmetic operations, and this
//  assertion is true for larger matrices and matrices with a structure that may be exploited.
//  However, for arbitrary matrices of smaller order, such as the 2x2, 3x3, and 4x4 matrices
//  encountered most often in geometric applications, the classical adjoint method is generally the
//  method of choice. The reason is that the classical adjoint method provides for a branchless
//  implementation, meaning there are no if statements, or loops that cannot be unrolled
//  statically. On today's superscalar architectures and dedicated vector processors, this is a big
//  win. 
/**
 * \warning If a matrix is orthogonal, its transpose and inverse are equal; Just obtaining the
 *  transpose is probably enough in this case.
 *  Rotation and reflection matrices are orthogonal. If we know that our matrix is orthogonal, we
 *  can essentially avoid computing the inverse, which is a relatively costly computation.
 * \return  The inverse of \a mat
 */
__attribute__((always_inline))
static inline struct mat3x3 get_inv( struct mat3x3 mat )
{
  constexpr size_t row_cnt = _arr_elem_cnt(mat.rows);
  constexpr size_t col_cnt = 3;  // _arr_elem_cnt won't work on ext_vector_type

  struct mat3x3 ret = get_adj( mat );
  float det = det_3( mat );
  assume( !is_close_norel(det, 0) );  // Matrices with a zero determinant are noninvertible.

  float divider = 1.0f / det;
  for ( unsigned int row_idx = 0; row_idx < row_cnt; row_idx++ ) {
    for ( unsigned int col_idx = 0; col_idx < col_cnt; col_idx++ ) {
      ret.rows[row_idx][col_idx] *= divider;
    }
  }
  return ret;
}


/**
 * \return  true if \a mat is an orthogonal matrix i.e. the set of rows/columns is orthonormal
 *  false otherwise.
 * \warning When computing a matrix inverse, we will usually only take advantage of orthogonality
 *  if we know a priori that a matrix is orthogonal. If we don't know in advance, it's probably
 *  a waste of time to check. In the best case, we check for orthogonality and find that the
 *  matrix is indeed orthogonal, and then we transpose the matrix. But this may take almost
 *  as much time as doing the inversion. In the worst case, the matrix is not orthogonal,
 *  and any time we spent checking was definitely wasted. Finally, even matrices that are
 *  orthogonal in the abstract may not be exactly orthogonal when represented in floating
 *  point, and so we must use tolerances, which have to be tuned.
 */
__attribute__((always_inline))
static inline bool is_orthogonal( struct mat3x3 mat )
{
  return is_close_norel( dot_3(mat.rows[0], mat.rows[0]), 1 ) &&
    is_close_norel( dot_3(mat.rows[0], mat.rows[1]), 0 ) &&
    is_close_norel( dot_3(mat.rows[0], mat.rows[2]), 0 ) &&
    is_close_norel( dot_3(mat.rows[1], mat.rows[1]), 1 ) &&
    is_close_norel( dot_3(mat.rows[1], mat.rows[2]), 0 ) &&
    is_close_norel( dot_3(mat.rows[2], mat.rows[2]), 1 );
}


/**
 * \brief Gram-Schmidt orthogonalization
 * \details The function produces a similar matrix with orthogonal basis vectors. The standard
 *  algorithm for constructing a set of orthogonal basis vectors is Gram-Schmidt orthogonalization.
 *  The basic idea is to go through the basis vectors in order. For each basis vector, we subtract
 *  off the portion of that vector that is parallel to the preceding basis vectors, which must
 *  result in a perpendicular vector.
 * \warning (NOTE) We need an orthonormal basis to form an orthogonal matrix, and so we should
 *  probably normalize the vectors in addition to orthogonalizing. Notice that if we normalize the
 *  vectors as we go, rather than in a second pass, then we can avoid all of the divisions.
 *  (TODO) Also, a trick that works in 3D (but not in higher dimensions) is to compute the third
 *  basis vector using the cross product.
 *  The fundtion is biased, the first row will be unchanged and the last row will be changed
 *  the most. We can implement an unbiased version at the cost of a big loop of computation.
 */
__attribute__((always_inline))
static inline struct mat3x3 orthogonalize( struct mat3x3 mat )
{
  struct mat3x3 ret;
  constexpr size_t row_cnt = _arr_elem_cnt(mat.rows);
  float arr_self_dp[row_cnt - 1];

  ret.rows[0] = mat.rows[0];

  arr_self_dp[0] = dot_3( ret.rows[0], ret.rows[0] );
  ret.rows[1] = mat.rows[1];
  ret.rows[1] -= scalar_mul_3( ret.rows[0], dot_3(mat.rows[1], ret.rows[0]) / arr_self_dp[0] );

  arr_self_dp[1] = dot_3( ret.rows[1], ret.rows[1] );
  ret.rows[2] = mat.rows[2];
  ret.rows[2] -= scalar_mul_3( ret.rows[0], dot_3(mat.rows[2], ret.rows[0]) / arr_self_dp[0] );
  ret.rows[2] -= scalar_mul_3( ret.rows[1], dot_3(mat.rows[2], ret.rows[1]) / arr_self_dp[1] );

  return ret;
}



//-------------------------------------------------------------------------------------------------
//   Identity  Translation   Rotation    Scaling
// ┌         ┐ ┌         ┐ ┌         ┐ ┌         ┐
// │ 1 0 0 0 │ │ 1 0 0 0 │ │ ? ? ? 0 │ │ x 0 0 0 │
// │ 0 1 0 0 │ │ 0 1 0 0 │ │ ? ? ? 0 │ │ 0 y 0 0 │
// │ 0 0 1 0 │ │ 0 0 1 0 │ │ ? ? ? 0 │ │ 0 0 z 0 │
// │ 0 0 0 1 │ │ x y z 1 │ │ 0 0 0 1 │ │ 0 0 0 1 │
// └         ┘ └         ┘ └         ┘ └         ┘

constexpr struct mat4x4 identity_4x4 = {
  .rows = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
  }
};

/**
 * \brief returns a matrix for translation(a transformation of coordinates in which the new axes
 *  are parallel to the old ones)
 */
__attribute__((always_inline))
static inline struct mat4x4 translate_4x4( float x, float y, float z )
{
  struct mat4x4 ret = {
    .rows = {
      { 1, 0, 0, 0 },
      { 0, 1, 0, 0 },
      { 0, 0, 1, 0 },
      { x, y, z, 1 },
    }
  };
  return ret;
}

/**
 * \brief returns a matrix for rotation around x axis
 */
__attribute__((always_inline))
static inline struct mat4x4 rot_axa_4x4( float theta )
{
  float sin = sinf(theta);
  float cos = cosf(theta);
  struct mat4x4 ret = {
    (vec4) { 1, 0,    0,  0 },
    (vec4) { 0, cos,  sin, 0 },
    (vec4) { 0, -sin, cos, 0 },
    (vec4) { 0, 0,    0,  1 },
  };
  return ret;
}

/**
 * \brief returns a matrix for rotation around y axis
 */
__attribute__((always_inline))
static inline struct mat4x4 rot_aya_4x4( float theta )
{
  float sin = sinf(theta);
  float cos = cosf(theta);
  struct mat4x4 ret = {
    (vec4) { cos, 0, -sin, 0 },
    (vec4) { 0,   1, 0,   0 },
    (vec4) { sin, 0, cos, 0 },
    (vec4) { 0,   0, 0,   1 },
  };
  return ret;
}

/**
 * \brief returns a matrix for rotation around z axis
 */
__attribute__((always_inline))
static inline struct mat4x4 rot_aza_4x4( float theta )
{
  float sin = sinf(theta);
  float cos = cosf(theta);
  struct mat4x4 ret = {
    (vec4) { cos,   sin, 0, 0 },
    (vec4) { -sin, cos, 0, 0 },
    (vec4) { 0,     0,  1, 0 },
    (vec4) { 0,     0,  0, 1 },
  };
  return ret;
}

/**
 * \brief returns a rotation matrix.
 * \param pivot The pivot vector around which things move, it should be a normalized vector
 */
__attribute__((always_inline))
static inline struct mat4x4 get_rot_mat(
    const vec3 pivot,
    float angle )
{
  const float cos = cosf(angle);
  const float one_minus_cos = 1.0f - cos;
  const float sin = sinf(angle);

  const float x = pivot[0];
  const float y = pivot[1];
  const float z = pivot[2];

  // # of Mult :  3 +6 +3 = 12
  // # of Add/Sub :  3 +3*2 = 9
  // # of Load :  3 float

  // x * x*(1-c) + c
  // y * y*(1-c) + c
  // z * z*(1-c) + c
  // x * y*(1-c) [(-zs)|(+zs)]
  // x * z*(1-c) [(-ys)|(+ys)]
  // y * z*(1-c) [(-xs)|(+xs)]

  struct mat4x4 dst;
  dst.rows[4][0] = x * x * one_minus_cos + cos;
  dst.rows[4][1] = y * x * one_minus_cos - z * sin;
  dst.rows[4][2] = z * x * one_minus_cos + y * sin;
  dst.rows[4][3] = 0.0f;

  dst.rows[4][0] = x * y * one_minus_cos + z * sin;
  dst.rows[4][1] = y * y * one_minus_cos + cos;
  dst.rows[4][2] = z * y * one_minus_cos - x * sin;
  dst.rows[4][3] = 0.0f;

  dst.rows[4][0] =  x * z * one_minus_cos - y * sin;
  dst.rows[4][1] =  y * z * one_minus_cos + x * sin;
  dst.rows[4][2] = z * z * one_minus_cos + cos;
  dst.rows[4][3] = 0.0f;

  dst.rows[4][0] = 0.0f;
  dst.rows[4][1] = 0.0f;
  dst.rows[4][2] = 0.0f;
  dst.rows[4][3] = 1.0f;

  return dst;
}

/**
 * \brief returns a scaling matrix.
 */
__attribute__((always_inline))
static inline struct mat4x4 get_sca_mat( float x, float y, float z )
{
  struct mat4x4 ret;

  ret.rows[0][0] = x;
  ret.rows[0][1] = 0;
  ret.rows[0][2] = 0;
  ret.rows[0][3] = 0;

  ret.rows[1][0] = 0;
  ret.rows[1][1] = y;
  ret.rows[1][2] = 0;
  ret.rows[1][3] = 0;

  ret.rows[2][0] = 0;
  ret.rows[2][1] = 0;
  ret.rows[2][2] = z;
  ret.rows[2][3] = 0;

  ret.rows[3][0] = 0;
  ret.rows[3][1] = 0;
  ret.rows[3][2] = 0;
  ret.rows[3][3] = 1;

  return ret;
}

/**
 * \brief returns the matrix for converting our world volume to Vulkan's canonical viewing volume.
 *  perspective_projection_matrix = perspective-to-ortho * ortho-to-vulkan_cvv
 * \param vfov The angle of the vertical field of view.
 * \param aspect_ratio The width divided by the height of the render target.
 * \param near The distance from the viewer to the near clipping plane.
 * \param far The distance from the viewer to the far clipping plane.
 */
__attribute__((always_inline))
static inline struct mat4x4 get_perspective_proj_mat_vk(
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
    (vec4) { 1 / (aspect_ratio * tan_half_theta), 0,                 0,                        0 },
    (vec4) { 0,                                   -1/tan_half_theta, 0,                        0 },
    (vec4) { 0,                                   0,                 far / (far - near),       1 },
    (vec4) { 0,                                   0,                 -near*far / (far - near), 0 },
  };

  return ret;
}



//-------------------------------------------------------------------------------------------------
// Polar Coordinates
//

// 2D Polar Coordinates
struct p_c_2 {
  float h;  // Heading, When h=0 it's at [x=0, y>0], as h gets greater it turns clockwise.
  float r;  // Radial distance, the distance from origin.
};

// Spherical Coordinates (Polar Coordinates)
struct p_c_3 {
  float h;  ///> Heading, When h=p=0, it's at [x=0, z>0].
            ///> As h gets greater it turns clockwise(when seen from above i.e. from +y).
  float r;  // Radial distance, the distance from origin.
  float p;  // Pitch i.e. declination starting from y=0
};

__attribute__((always_inline))
static inline struct p_c_3 v2pc( vec3 v )
{
  struct p_c_3 ret;

  // The radial distance
  float magnitude = sqrtf( v.x * v.x +  v.y * v.y +  v.z * v.z );
  if ( magnitude <= 0.0f ) {
    ret = (typeof(ret)) {0};
    return ret;
  }
  ret.r = magnitude;

  // The declination
  float pitch = asinf( -v.y / magnitude );
  ret.p = pitch;
  if ( fabsf(pitch) >= (typeof(pitch))(M_PI / 2 * 0.9999) ) {
    ret.h = 0;
  } else {
    ret.h = atan2f( v.x, v.z );
  }

  return ret;
}

__attribute__((always_inline))
static inline vec3 pc2v( struct p_c_3 pc )
{
  float cosp = cosf( pc.p );
  assume( cosp >= 0 );
  assume( pc.r >= 0 );

  vec3 ret;
  ret.x = pc.r * cosp * sinf(pc.h);
  ret.z = pc.r * cosp * cosf(pc.h);
  ret.y = -( pc.r * sinf(pc.p) );
  return ret;
}



#endif  // CARBONARA_MATH_H_
