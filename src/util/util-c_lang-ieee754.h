#if !defined UTIL_C_LANG_IEEE754_H_
#define UTIL_C_LANG_IEEE754_H_


#include <stdint.h>
#include <math.h>

#include "util-c_lang-basic.h"


//-------------------------------------------------------------------------------------------------
#if !defined( __clang__ ) && !defined( __GNUC__ )
  #error I am not sure about the bit-field packing order of other compilers.
#endif
#if !defined( TARGET_ENDIANNESS )
  #error Define TARGET_ENDIANNESS! 1: Little Endian, 2: Big Endian, 3: PDP Endian
#endif

#if TARGET_ENDIANNESS==1
  typedef union ieee754_binary32_le union_ieee754_binary32;
  typedef union ieee754_binary64_le union_ieee754_binary64;
#elif TARGET_ENDIANNESS==2
  typedef union ieee754_binary32_be union_ieee754_binary32;
  typedef union ieee754_binary64_be union_ieee754_binary64;
#else
  #error TARGET_ENDIANNESS PDP Endian is not supported!
#endif

/*
 * \brief for little endian systems
 */
union ieee754_binary64_le {
  struct {
    uint64_t fraction: 52;
    uint16_t exponent: 11;
    uint8_t sign: 1;
  };
  double whole;
};

/*
 * \brief for little endian systems
 */
union ieee754_binary32_le {
  struct {
    uint32_t fraction: 23;
    uint16_t exponent: 8;
    uint8_t sign: 1;
  };
  float whole;
};

/*
 * \brief for big endian systems
 */
union ieee754_binary64_be {
  struct {
    uint8_t sign: 1;
    uint16_t exponent: 11;
    uint64_t fraction: 52;
  };
  double whole;
};

/*
 * \brief for big endian systems
 */
union ieee754_binary32_be {
  struct {
    uint8_t sign: 1;
    uint16_t exponent: 8;
    uint32_t fraction: 23;
  };
  float whole;
};

// NOTE Do we need another set for PDP_ENDIAN?


#define DEF_ABS_TOL 0.00000095367431640625
#define DEF_REL_TOL ((DEF_ABS_TOL) / 256)

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bool is_close_f( float a, float b, float rel_tol, float abs_tol )
{
  return fabsf( a - b ) <= max( rel_tol * max(fabsf(a), fabsf(b)), abs_tol );
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline bool is_close_d( double a, double b, double rel_tol, double abs_tol )
{
  return fabs( a - b ) <= max( rel_tol * max(fabs(a), fabs(b)), abs_tol );
}

//-------------------------------------------------------------------------------------------------
#define is_close( a, b ) _Generic((a), \
    float : \
        is_close_f, \
    double : \
        is_close_d, \
    )( (a), (b), DEF_REL_TOL, DEF_ABS_TOL )

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline double min_without_cmp_d( double a, double b )
{
  const typeof(a) sum = a + b;
  union_ieee754_binary64 diff = { .whole = (a - b) };
  diff.sign = 0;
  const typeof(a) abs_diff = diff.whole;
  return (sum - abs_diff) / 2;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline double max_without_cmp_d( double a, double b )
{
  const typeof(a) sum = a + b;
  union_ieee754_binary64 diff = { .whole = (a - b) };
  diff.sign = 0;
  const typeof(a) abs_diff = diff.whole;
  return (sum + abs_diff) / 2;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline float min_without_cmp_f( float a, float b )
{
  const typeof(a) sum = a + b;
  union_ieee754_binary32 diff = { .whole = (a - b) };
  diff.sign = 0;
  const typeof(a) abs_diff = diff.whole;
  return (sum - abs_diff) / 2;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline float max_without_cmp_f( float a, float b )
{
  const typeof(a) sum = a + b;
  union_ieee754_binary32 diff = { .whole = (a - b) };
  diff.sign = 0;
  const typeof(a) abs_diff = diff.whole;
  return (sum + abs_diff) / 2;
}

#endif  // UTIL_C_LANG_IEEE754_H_
