#if !defined UTIL_C_LANG_BASIC_H_
#define UTIL_C_LANG_BASIC_H_


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>  // for snprintf
#include <stdlib.h>  // for abort


//-------------------------------------------------------------------------------------------------
constexpr size_t ALIGN_FOR_SSE = 16;
constexpr size_t ALIGN_FOR_AVX = 32;
constexpr size_t ALIGN_FOR_AVX512 = 64;
constexpr size_t ALIGN_FOR_NEON = 16;


//-------------------------------------------------------------------------------------------------
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#ifdef _MSC_VER
  #define unreachable() __assume(0)
#else
  #define unreachable() __builtin_unreachable()
#endif


//-------------------------------------------------------------------------------------------------
constexpr size_t page_sizes[] = {
  // 0:7
  0,
  UINT64_C(1024) * 4,
  UINT64_C(1024) * 16,
  UINT64_C(1024) * 64,
  UINT64_C(1024) * 1024,
  UINT64_C(1024) * 1024 * 2,
  UINT64_C(1024) * 1024 * 16,
  UINT64_C(1024) * 1024 * 64,

  // 8:15
  UINT64_C(1024) * 1024 * 128,
  UINT64_C(1024) * 1024 * 256,
  UINT64_C(1024) * 1024 * 512,
  UINT64_C(1024) * 1024 * 1024,
  UINT64_C(1024) * 1024 * 1024 * 2,
  UINT64_C(1024) * 1024 * 1024 * 4,
  UINT64_C(1024) * 1024 * 1024 * 64,
  UINT64_C(1024) * 1024 * 1024 * 128,
};


//-------------------------------------------------------------------------------------------------
// Unions

static_assert( sizeof(void*) == sizeof(uintptr_t),
    "sizeof(void*) should be equal to sizeof(uintptr_t)" );

union union64 {
  uint64_t u;
  int64_t i;
  double f;
#if UINTPTR_WIDTH/8 == 8
  void * ptr;
#endif
  uint32_t arr_u32[2];
  uint16_t arr_u16[4];
  uint8_t arr_u8[8];
  int32_t arr_i32[2];
  int16_t arr_i16[4];
  int8_t arr_i8[8];
};
union union32 {
  uint32_t u;
  int32_t i;
  float f;
#if UINTPTR_WIDTH/8 == 4
  void * ptr;
  uint16_t arr_u16[2];
  uint8_t arr_u8[4];
  int16_t arr_i16[2];
  int8_t arr_i8[4];
#endif
};
union union16 {
  uint16_t u;
  int16_t i;
#if UINTPTR_WIDTH/8 == 2
  void * ptr;
#endif
  uint8_t arr_u8[2];
  int8_t arr_i8[2];
};
union union8 {
  uint8_t u;
  int8_t i;
};
#if UINTPTR_WIDTH/8 == 2
typedef union union16 union_ptr;
#elif UINTPTR_WIDTH/8 == 4
typedef union union32 union_ptr;
#elif UINTPTR_WIDTH/8 == 8
typedef union union64 union_ptr;
#endif

struct double_union {
#if UINTPTR_WIDTH/8 == 4
  union union32 data1;
  union union32 data2;
#elif UINTPTR_WIDTH/8 == 2
  union union16 data1;
  union union16 data2;
#else
  union union64 data1;
  union union64 data2;
#endif
};


typedef uint8_t offset_tag;


//-------------------------------------------------------------------------------------------------
uint64_t init_util_c_lang(void);

//-------------------------------------------------------------------------------------------------
#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) < (b) ? (b) : (a))

//-------------------------------------------------------------------------------------------------
//#define DECONST_TYPE_PLZ(x) typeof({typeof(x + 0) y; y = x; y;})
/**
 * brief It works only on a non-pointer value, for a const pointer you need to dereference.
 */
//#define DECONST_TYPE_PLZ2( x ) \
//  _Pragma ( "GCC diagnostic push" ) \
//  _Pragma ( "GCC diagnostic ignored \"-Wgnu-statement-expression-from-macro-expansion\"" ) \
//  DECONST_TYPE_PLZ(x) \
//  _Pragma ( "GCC diagnostic pop" )

#define DECONST_TYPE_PLZ( x ) _Generic((x),         \
    const float: ({typeof(float) y; y;}),           \
    const double: ({typeof(double) y; y;}),         \
    const uint64_t: ({typeof(uint64_t) y; y;}),     \
    const uint32_t: ({typeof(uint32_t) y; y;}),     \
    const uint16_t: ({typeof(uint16_t) y; y;}),     \
    const uint8_t: ({typeof(uint8_t) y; y;}),       \
    const int64_t: ({typeof(int64_t) y; y;}),       \
    const int32_t: ({typeof(int32_t) y; y;}),       \
    const int16_t: ({typeof(int16_t) y; y;}),       \
    const int8_t: ({typeof(int8_t) y; y;}),         \
    const void *: ({typeof(void *) y; y;}),         \
    const uint64_t *: ({typeof(uint64_t *) y; y;}), \
    const uint32_t *: ({typeof(uint32_t *) y; y;}), \
    const uint16_t *: ({typeof(uint16_t *) y; y;}), \
    const uint8_t *: ({typeof(uint8_t *) y; y;}),   \
    const int64_t *: ({typeof(int64_t *) y; y;}),   \
    const int32_t *: ({typeof(int32_t *) y; y;}),   \
    const int16_t *: ({typeof(int16_t *) y; y;}),   \
    const int8_t *: ({typeof(int8_t *) y; y;}),     \
    default: ({typeof((x + 0)) y; y;})              \
    )
//-------------------------------------------------------------------------------------------------
#define deconst_typeof( x )                                                                 \
  _Pragma ( "GCC diagnostic push" )                                                         \
  _Pragma ( "GCC diagnostic ignored \"-Wgnu-statement-expression-from-macro-expansion\"" )  \
  _Pragma ( "GCC diagnostic ignored \"-Wint-conversion\"" )                                 \
  _Pragma ( "GCC diagnostic ignored \"-Wunreachable-code-generic-assoc\"" )                 \
  _Pragma ( "GCC diagnostic ignored \"-Wincompatible-pointer-types\"" )                     \
  typeof( DECONST_TYPE_PLZ( x ) )                                                           \
  _Pragma ( "GCC diagnostic pop" )

/*
    const int: ({typeof(int) y; y;}), \
    const unsigned int: ({typeof(unsigned int) y; y;}), \
    const long: ({typeof(long) y; y;}), \
    const unsigned long: ({typeof(unsigned long) y; y;}), \
    const short: ({typeof(short) y; y;}), \
    const unsigned short: ({typeof(unsigned short) y; y;}), \
    const char: ({typeof(char) y; y;}), \
    const unsigned char: ({typeof(unsigned char) y; y;}), \
 */

//-------------------------------------------------------------------------------------------------
/**
 * Cast a pointer to an unsigned integer
 */
#define ptr2ui(pointer)                                                                     \
  _Pragma ( "GCC diagnostic push" )                                                         \
  _Pragma ( "GCC diagnostic ignored \"-Wgnu-statement-expression-from-macro-expansion\"" )  \
  ( ({union_ptr uni; uni.ptr = (void*)(pointer); uni.u;}) )                                 \
  _Pragma ( "GCC diagnostic pop" )
//#define ptr2ui(ptr) \
//  _Pragma ( "GCC diagnostic push" ) \
//  _Pragma ( "GCC diagnostic ignored \"-Wvoid-pointer-to-int-cast\"" ) \
//  _Pragma ( "GCC diagnostic ignored \"-Wpointer-to-int-cast\"" ) \
//  ( sizeof(ptr) == 2 ? (uint16_t)(ptr) : (sizeof(ptr) == 4 ? (uint32_t)(ptr) : (uint64_t)(ptr) ) )\
//  _Pragma ( "GCC diagnostic pop" )
  //_Pragma ( "GCC diagnostic ignored \"-Wbugprone-sizeof-expression\"" ) \
  //

//-------------------------------------------------------------------------------------------------
/**
 * Cast unsigned integer to void*
 */
#define ui2ptr(value)                                                                       \
  _Pragma ( "GCC diagnostic push" )                                                         \
  _Pragma ( "GCC diagnostic ignored \"-Wgnu-statement-expression-from-macro-expansion\"" )  \
  ( ({union_ptr uni; uni.u = (value); uni.ptr;}) )                                          \
  _Pragma ( "GCC diagnostic pop" )


//-------------------------------------------------------------------------------------------------
#define always_assert(x)                                                \
  do {                                                                  \
    if ( !(x) ) {                                                       \
        fprintf( stderr, "Assertion failed in \"%s\", line %d.\n"       \
                 "\tProbably a bug.\n",                                 \
                 __FILE__, __LINE__ );                                  \
        abort();                                                        \
    }                                                                   \
  } while(0)

#if defined(ARE_WE_SHIPPING_IT)
  #define assume(expr) __builtin_assume(expr)
#else
  #define assume(expr) always_assert(expr)
#endif

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain an increased index that repeats iteration under a power of two
 */
#define _reiterate_under_power_of_two(growing_index, power_of_two) \
  ( (growing_index) & ((power_of_two)-1) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Iterate from the middle towards alternating sides (e.g. 4,3,5,2,6,1,7,0 )
 */
#define _iterate_from_mid(growing_index, nr_indices)                      \
  ( (growing_index) & 1 ? ((nr_indices)>>1) - ((growing_index)>>1) - 1 :  \
    ((nr_indices)>>1) + ((growing_index)>>1) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Tell which one of the 0-to-N is on a specific slot when the 0-to-n array is spread
 *  under a power of two elements that's bigger than N.
 * _spread_under_power_of_two(0-->63, dbb(64,16)) =
 * { 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3 ... 14,14,14,14,15,15,15,15 } [0-->63]
 *
 * You can obtain the value of distance_between_bits like below:
 *   _get_distance_between_bit_locations(power_of_two, nr_indices, &dbb)
 *
 */
#define _spread_under_power_of_two(growing_index, distance_between_bits) \
  ( (growing_index) >> (distance_between_bits) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief I've seen this be compiled into one xchg instruction in gcc 4.6.3 Athlon X2
 */
#define _xorswap(p1,p2) *(p1) ^= *(p2); *(p2) ^= *(p1); *(p1) ^= *(p2);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Swap the values assigned to two different variables of same type
 */
#define _swap(p1,p2)                                                              \
{                                                                                 \
  static_assert( sizeof(*(p2)) == sizeof(*(p1)), "They are different in size!");  \
  typeof(*(p1)) _tmpvar = *(p1);                                                  \
  *(p1) = *(p2);                                                                  \
  *(p2) = _tmpvar;                                                                \
}

//-------------------------------------------------------------------------------------------------
/**
 * brief Obtain the offset of a field in a structure
 */
//#ifndef offsetof
//#define offsetof(the_struct, field_name) \
//  ( (0u + &((typeof(the_struct)*)0)->field_name) )
//#endif

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the size of a field in a structure
 */
#define _sizeof_member(the_struct, member_name) \
  ( sizeof( ((typeof(the_struct)*)0)->member_name ) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the type of a field in a structure
 */
#define _typeof_member(the_struct, member_name) \
  typeof( ((typeof(the_struct)*)0)->member_name )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the type of a field in a structure
 */
#define _alignof_member(the_struct, member_name) \
  ( alignof( ((typeof(the_struct)*)0)->member_name ) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the type of a field in a structure
 */
#define _sizeof_arr_elem(array) \
  ( sizeof( (*((typeof(array)*)0))[0] ) )
//  ( sizeof( ((typeof(array))0)[0] ) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the type of a field in a structure
 */
#define _alignof_arr_elem(array) \
  ( alignof( (*((typeof(array)*)0))[0] ) )
//  ( alignof( ((typeof(array))0)[0] ) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the number of elements in the given C array
 */
#define _arr_elem_cnt(array) \
  ( sizeof(array) / _sizeof_arr_elem(array) )
//  ( sizeof(array) / sizeof(array[0]) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief The maximum value representable in unsigned integer form by a variable.
 * \warning Be careful about uint_(fast|least).+_t
 */
#define UINT_MAX_(var) (                            \
  sizeof((var)) == 1 ? UINT8_MAX : (                \
  sizeof((var)) == 2 ? UINT16_MAX : (               \
  sizeof((var)) == 4 ? UINT32_MAX : UINT64_MAX )) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief No doxygen for macro function :(
 */
#define is_signed(var) _Generic((var),  \
    uint64_t  : false,                  \
    uint32_t  : false,                  \
    uint16_t  : false,                  \
    uint8_t   : false,                  \
    int64_t  : true,                    \
    int32_t  : true,                    \
    int16_t  : true,                    \
    int8_t   : true                     \
)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Align the given length to the specified \a power_of_two, rounding up.
 * \a n_to_align must be greater than 0 and \a power_of_two must be a power of two.
 */
#define _align_to_power_of_two(n_to_align, power_of_two) \
  ( ((n_to_align)+(power_of_two)-(typeof(n_to_align))1) & ~((power_of_two)-(typeof(n_to_align))1) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Align the given length to the specified \a power_of_two, rounding down.
 * \a n_to_align must be greater than 0 and \a power_of_two must be a power of two
 */
#define _align_to_power_of_two_downward(n_to_align, power_of_two) \
  ( (n_to_align) & ~((power_of_two)-1) )


//-------------------------------------------------------------------------------------------------
#define _align_ptr_to_power_of_two(ptr, power_of_two)                   \
{                                                                       \
  uintptr_t n_to_align = (uintptr_t)(ptr);                              \
  (ptr) = (void*)_align_to_power_of_two( n_to_align, (power_of_two) );  \
}

//-------------------------------------------------------------------------------------------------
#define _align_ptr(ptr)                                       \
{                                                             \
  _align_ptr_to_power_of_two( ptr, alignof(typeof(*(ptr))) ); \
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Check whether the given length is aligned to the specified \a power_of_two
 * \a n_to_test must be greater than 0 and \a power_of_two must be a power of two
 */
#define _is_aligned_to_power_of_two(n_to_test, power_of_two) \
  ( ((n_to_test) & ((power_of_two)-1)) == 0 )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Check whether the given length is aligned to the specified \a power_of_two
 * \a n_to_test must be greater than 0
 */
#define _is_power_of_two(n_to_test) \
  ( ((n_to_test) & ((n_to_test)-1)) == 0 )

//-------------------------------------------------------------------------------------------------
/**
 * \brief https://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
 */
static constexpr uint8_t piece_of_a_de_Bruijn_sequence[32] = {
  0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
  8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
};
#define ct_log2(number) piece_of_a_de_Bruijn_sequence[(                         \
    ( (uint32_t)(number) | ((uint32_t)(number) >> 1) |                          \
    ((uint32_t)(number) | (uint32_t)(number) >> 1) >> 2 |                       \
    ( (uint32_t)(number) | ((uint32_t)(number) >> 1) |                          \
    (((uint32_t)(number) | (uint32_t)(number) >> 1) >> 2) ) >> 4 |              \
    (( (uint32_t)(number) | ((uint32_t)(number) >> 1) |                         \
    (((uint32_t)(number) | (uint32_t)(number) >> 1) >> 2) ) >> 4) >> 8 |        \
    ((( (uint32_t)(number) | ((uint32_t)(number) >> 1) |                        \
    (((uint32_t)(number) | (uint32_t)(number) >> 1) >> 2) ) >> 4) >> 8) >> 16 ) \
    * 0x07c4acddu) >> 27]

//-------------------------------------------------------------------------------------------------
/**
 * \brief Align to the closest power of two that is bigger than \a number
 * \a number must be greater than 0
 */
#define _align_to_the_close_power_of_two_32(number) \
  ( _is_power_of_two((number)) ? (number) : 1 << (31 - _lzcnt_u32((number)) + 1) )
#define _align_to_the_close_power_of_two_64(number) \
  ( _is_power_of_two((number)) ? (number) : 1 << (63 - _lzcnt_u64((number)) + 1) )
#define _align_to_the_close_power_of_two(number)                                                  \
  ( sizeof((number)) > 4 ?                                                                        \
    _align_to_the_close_power_of_two_64((number)) : _align_to_the_close_power_of_two_32((number)) )
#define _ct_align_to_the_close_power_of_two(number) \
  ( _is_power_of_two((number)) ? (number) : 1 << (ct_log2((number)) + 1) )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Get the number of units to cover a group of smaller units
 */
#define _get_nr_units_to_cover( nr_small_units, max_nr_small_units_the_target_unit_can_take ) \
  (  ( (nr_small_units) + ((max_nr_small_units_the_target_unit_can_take)-1) )                 \
    / ( max_nr_small_units_the_target_unit_can_take )  )

//-------------------------------------------------------------------------------------------------
/**
 * \brief I recommend using alignas() instead
 */
#define _get_nr_bytes_for_struct_padding( alignment, nr_bytes_for_other_members ) \
  ( ( (nr_bytes_for_other_members)/(alignment)*(alignment) + (alignment) )        \
    - (nr_bytes_for_other_members) == (alignment) ? 0 :                           \
    ( (nr_bytes_for_other_members)/(alignment)*(alignment) + (alignment) )        \
    - (nr_bytes_for_other_members) )

//-------------------------------------------------------------------------------------------------
/**
 * Ignore Vector Dependencies
 */
#if defined(__clang__)
#define IVDep #pragma clang loop ivdep(enable)
#else
  #if defined(__GNUC__)
  #define IVDep #pragma GCC ivdep
  #else
    #if defined(_MSC_VER)
    #define IVDep #pragma loop(ivdep)
    #endif
  #endif
#endif


//-------------------------------------------------------------------------------------------------
/**
 * \brief Get an aligned pointer with an offset_tag attached right in front.
 * Use _get_orig_ptr(returned_ptr) macro to get the original address.
 * 
 * \param alignment The number of bytes to align the address to.
 *     It must be a power of two.
 */
__attribute__((always_inline))
static inline void * get_offset_pointer( void * ptr, uint16_t alignment, uint8_t leave_a_tag )
{
  assume( _is_power_of_two(alignment) );
  void * new_ptr;
  if ( leave_a_tag ) {
    new_ptr = (void*) _align_to_power_of_two(ptr2ui(ptr) + 1, alignment);
    offset_tag * ptr_offset_tag = new_ptr - sizeof(offset_tag);
    *ptr_offset_tag = (offset_tag) (new_ptr - ptr);
  } else {
    new_ptr = (void*) _align_to_power_of_two(ptr2ui(ptr), alignment);
  }
  return new_ptr;
}
#define _get_orig_ptr(ptr) \
  ((void*)ptr - *((offset_tag *)(ptr) - 1))


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline unsigned int log_func_unsigned( uint8_t base, unsigned int n )
{
  unsigned int exp = 0;
  assume(n >= base);
  while(-7) {
    if ( n <= 1 ) {
      return exp;
    }

    exp++;
    n /= base;
  }
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline unsigned int pow_unsigned( unsigned int n, uint8_t exp )
{
  assume(exp > 0);

  unsigned int ret = n;
  for ( uint_fast8_t i = 1; i < exp; i++ ) {
    ret *= n;
  }

  return ret;
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the absolute value of the given floating point value
 */
__attribute__((always_inline))
inline uint64_t absd( double d )
{
  union union64 uni;
  uni.f = d;
  uni.u &= UINT64_C(0x7fffffffffffffff);
  return (uint64_t) uni.f;
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the absolute value of the given floating point value
 */
__attribute__((always_inline))
inline uint32_t absf( float f )
{
  union union32 uni;
  uni.f = f;
  uni.u &= 0x7fffffffu;
  return (uint32_t) uni.f;
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief You don't have to include string.h when you call this function
 * But you shouldn't use it on longer strings often, since the performance is not optimal.
 */
uint32_t util_strlen_u8( const uint8_t * str );

//-------------------------------------------------------------------------------------------------
/**
 * \brief You don't have to include string.h when you call this function
 * But you shouldn't use it on longer strings often, since the performance is not optimal.
 */
uint32_t util_strlen_u16( const uint16_t * str );

//-------------------------------------------------------------------------------------------------
/**
 * \brief You don't have to include string.h when you call this function
 * But you shouldn't use it on longer strings often, since the performance is not optimal.
 */
#define util_strlen(str) _Generic((str),  \
    const char* : util_strlen_u8,         \
    char* : util_strlen_u8,               \
    const uint8_t* : util_strlen_u8,      \
    uint8_t* : util_strlen_u8,            \
    const short* : util_strlen_u16,       \
    short* : util_strlen_u16,             \
    const uint16_t* : util_strlen_u16,    \
    uint16_t* : util_strlen_u16           \
    )((void*)str)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the index of the \a n'th null on a null-separated string
 * \return 0 if \a n is 0, otherwise the index of the \a n'th null
 */
uint32_t get_idx_of_null_c( const char * str, uint8_t n );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the index of the \a n'th null on a null-separated string
 * \return 0 if \a n is 0, otherwise the index of the \a n'th null
 */
uint32_t get_idx_of_null_s( const uint16_t * str, uint8_t n );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the index of the \a n'th null on a null-separated string
 * \return 0 if \a n is 0, otherwise the index of the \a n'th null
 */
#define get_idx_of_null(str, n) _Generic((str), \
    const char* : get_idx_of_null_c,            \
    char* : get_idx_of_null_c,                  \
    const uint8_t* : get_idx_of_null_c,         \
    uint8_t* : get_idx_of_null_c,               \
    const short* : get_idx_of_null_s,           \
    short* : get_idx_of_null_s,                 \
    const uint16_t* : get_idx_of_null_s,        \
    uint16_t* : get_idx_of_null_s               \
    )(str, n)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the index of the first position among the byte positions in which c is written.
 * \return UINT32_MAX if a 0 is met first, otherwise the index of the first c found.
 */
uint32_t get_idx_of_char( const char * string, char c );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the index of the \a n'th \a c in string.
 * \param n when it's less than 0, the search is in inverse order
 * \return UINT32_MAX when \a n is 0, or when it failed to search \a n'th \a c.
 */
uint32_t get_idx_of_nth_char( const char * string, char c, int n );

//-------------------------------------------------------------------------------------------------
/**
 * \brief A simple implementation of sprintf(s, "%d", n)
 */
void itoa( int n, char * s );


#endif
