#include <stdint.h>
#include <cpuid.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

//#include "util-c_lang-basic.h"
#include "util-c_lang-basic.h"
#include "util-c_lang-x86.h"
#include "util-c_lang-bits.h"

#ifdef DEBUG_MODE
#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
uint8_t lut_byte_flip[16];


//-------------------------------------------------------------------------------------------------
unsigned int (*count_1bits)( unsigned int length, const uint8_t string[static length] );
static unsigned int count_1bits_brute( unsigned int length, const uint8_t string[static length] );
static unsigned int count_1bits_POPCNT( unsigned int length, const uint8_t string[static length] );


//-------------------------------------------------------------------------------------------------
uint32_t util_strlen_u8( const uint8_t * string )
{
  uint32_t mask_first_byte = 0xffUL << 8*(sizeof(mask_first_byte)-1);
  uint8_t unit_len = sizeof(mask_first_byte);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_first_byte, unit_len);
  uint32_t mask_second_byte = 0xffUL << 8*(unit_len-2);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_second_byte, unit_len);
  uint32_t mask_third_byte = 0xffUL << 8*(unit_len-3);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_third_byte, unit_len);
  uint32_t mask_fourth_byte = 0xffUL << 8*(unit_len-4);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_fourth_byte, unit_len);
  /*
  uint64_t mask_fifth_byte = 0xffUL << 8*(unit_len-5);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_fifth_byte, sizeof(mask_first_byte));
  uint64_t mask_sixth_byte = 0xffUL << 8*(unit_len-6);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_sixth_byte, sizeof(mask_first_byte));
  uint64_t mask_seventh_byte = 0xffUL << 8*(unit_len-7);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_seventh_byte, sizeof(mask_first_byte));
  uint64_t mask_eighth_byte = 0xffUL << 8*(unit_len-8);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_eighth_byte, sizeof(mask_first_byte));
  */
  unsigned int idx = 0;
#ifdef UTILIZE_AVX2
  while( !_is_aligned_to_power_of_two(ptr2ui(string) + idx, ALIGN_FOR_AVX) ) {
#else
  while( !_is_aligned_to_power_of_two(ptr2ui(string) + idx, ALIGN_FOR_SSE) ) {
#endif
    if ( string[idx] == 0 )
      return idx;
    idx++;
  }
  unsigned int b4_align = idx;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
  const uint32_t * string2 = (const uint32_t *) (string + idx);
#pragma GCC diagnostic pop
#ifdef UTILIZE_AVX2
  string2 = __builtin_assume_aligned( string2, ALIGN_FOR_AVX );
#else
  string2 = __builtin_assume_aligned( string2, ALIGN_FOR_SSE );
#endif
  idx = 0;
  while(-7) {
    if ( 0 == (mask_first_byte & string2[idx]) )
      return idx * sizeof(*string2) + 0 + b4_align;
    if ( 0 == (mask_second_byte & string2[idx]) )
      return idx * sizeof(*string2) + 1 + b4_align;
    if ( 0 == (mask_third_byte & string2[idx]) )
      return idx * sizeof(*string2) + 2 + b4_align;
    if ( 0 == (mask_fourth_byte & string2[idx]) )
      return idx * sizeof(*string2) + 3 + b4_align;
    /*
    if ( 0 == (mask_fifth_byte & string2[idx]) )
      return idx * sizeof(*string2) + 4;
    if ( 0 == (mask_sixth_byte & string2[idx]) )
      return idx * sizeof(*string2) + 5;
    if ( 0 == (mask_seventh_byte & string2[idx]) )
      return idx * sizeof(*string2) + 6;
    if ( 0 == (mask_eighth_byte & string2[idx]) )
      return idx * sizeof(*string2) + 7;
    */
    idx++;
  }
}

//-------------------------------------------------------------------------------------------------
uint32_t util_strlen_u16( const uint16_t * string )
{
  uint64_t mask_first_byte = 0xffffUL << 8*(sizeof(mask_first_byte)-1*2);
  uint8_t unit_len = sizeof(mask_first_byte);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_first_byte, unit_len);
  uint64_t mask_second_byte = 0xffffUL << 8*(unit_len-2*2);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_second_byte, unit_len);
  uint64_t mask_third_byte = 0xffffUL << 8*(unit_len-3*2);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_third_byte, unit_len);
  uint64_t mask_fourth_byte = 0xffffUL << 8*(unit_len-4*2);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_fourth_byte, unit_len);

  unsigned int idx = 0;
#ifdef UTILIZE_AVX2
  while( !_is_aligned_to_power_of_two(ptr2ui(string) + idx, ALIGN_FOR_AVX) ) {
#else
  while( !_is_aligned_to_power_of_two(ptr2ui(string) + idx, ALIGN_FOR_SSE) ) {
#endif
    if ( string[idx] == 0 )
      return idx;
    idx++;
  }
  unsigned int b4_align = idx;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
  const uint64_t * string2 = (const uint64_t *) ( string + idx );
#pragma GCC diagnostic pop
#ifdef UTILIZE_AVX2
  string2 = __builtin_assume_aligned( string2, ALIGN_FOR_AVX );
#else
  string2 = __builtin_assume_aligned( string2, ALIGN_FOR_SSE );
#endif
  idx = 0;
  while(-7) {
    if ( 0 == (mask_first_byte & string2[idx]) )
      return idx * sizeof(*string2) + 0 + b4_align;
    if ( 0 == (mask_second_byte & string2[idx]) )
      return idx * sizeof(*string2) + 1 + b4_align;
    if ( 0 == (mask_third_byte & string2[idx]) )
      return idx * sizeof(*string2) + 2 + b4_align;
    if ( 0 == (mask_fourth_byte & string2[idx]) )
      return idx * sizeof(*string2) + 3 + b4_align;
    idx++;
  }
}

//-------------------------------------------------------------------------------------------------
uint32_t get_idx_of_null_c( const char * string, uint8_t n )
{
  const char * ptr = string;
  uint32_t idx_of_null = 0;
  while(-7) {
    if ( n == 0 ) {
      break;
    }
    idx_of_null += util_strlen(ptr);
    idx_of_null += 1;

    ptr = ptr + idx_of_null;
    n--;
  }

  return idx_of_null ? idx_of_null - 1 : 0;
}

//-------------------------------------------------------------------------------------------------
uint32_t get_idx_of_null_s( const uint16_t * string, uint8_t n )
{
  const uint16_t * ptr = string;
  uint32_t idx_of_null = 0;
  while(-7) {
    if ( n == 0 ) {
      break;
    }
    idx_of_null += util_strlen(ptr);
    idx_of_null += 1;

    ptr = ptr + idx_of_null;
    n--;
  }

  return idx_of_null ? idx_of_null - 1 : 0;
}

//-------------------------------------------------------------------------------------------------
uint32_t get_idx_of_char( const char * string, char c )
{
  uint32_t mask_first_byte = 0xffUL << 8*(sizeof(mask_first_byte)-1);
  uint8_t unit_len = sizeof(mask_first_byte);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_first_byte, unit_len);
  uint32_t mask_second_byte = 0xffUL << 8*(unit_len-2);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_second_byte, unit_len);
  uint32_t mask_third_byte = 0xffUL << 8*(unit_len-3);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_third_byte, unit_len);
  uint32_t mask_fourth_byte = 0xffUL << 8*(unit_len-4);
  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_fourth_byte, unit_len);

  uint32_t idx_null = UINT32_MAX;
  unsigned int idx = 0;
#ifdef UTILIZE_AVX2
  while( !_is_aligned_to_power_of_two(ptr2ui(string) + idx, ALIGN_FOR_AVX) ) {
#else
  while( !_is_aligned_to_power_of_two(ptr2ui(string) + idx, ALIGN_FOR_SSE) ) {
#endif
    if ( string[idx] == 0 )
      return UINT32_MAX;
    if ( string[idx] == c )
      return idx;

    idx++;
  }
  unsigned int b4_align = idx;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
  const uint32_t * string2 = (const uint32_t *) ( string + idx );
#pragma GCC diagnostic pop
#ifdef UTILIZE_AVX2
  string2 = __builtin_assume_aligned( string2, ALIGN_FOR_AVX );
#else
  string2 = __builtin_assume_aligned( string2, ALIGN_FOR_SSE );
#endif
  idx = 0;
  while(-7) {
    uint32_t idx_c = UINT32_MAX;

    if ( 0 == (mask_first_byte & string2[idx]) )
      idx_null = idx * sizeof(*string2) + 0 + b4_align;
    else if ( 0 == (mask_second_byte & string2[idx]) )
      idx_null = idx * sizeof(*string2) + 1 + b4_align;
    else if ( 0 == (mask_third_byte & string2[idx]) )
      idx_null = idx * sizeof(*string2) + 2 + b4_align;
    else if ( 0 == (mask_fourth_byte & string2[idx]) )
      idx_null = idx * sizeof(*string2) + 3 + b4_align;

    if ( (uint8_t)c == (mask_first_byte & string2[idx]) )
      idx_c = idx * sizeof(*string2) + 0 + b4_align;
    else if ( c == (mask_second_byte & string2[idx]) >> 8*1 )
      idx_c = idx * sizeof(*string2) + 1 + b4_align;
    else if ( c == (mask_third_byte & string2[idx]) >> 8*2 )
      idx_c = idx * sizeof(*string2) + 2 + b4_align;
    else if ( c == (mask_fourth_byte & string2[idx]) >> 8*3 )
      idx_c = idx * sizeof(*string2) + 3 + b4_align;

    if ( idx_c != UINT32_MAX ) {
      if ( idx_null < idx_c ) {
        return UINT32_MAX;
      }
      return idx_c;
    }

    if ( idx_null != UINT32_MAX ) {
      return UINT32_MAX;
    }

    idx++;
  }
}

//-------------------------------------------------------------------------------------------------
uint32_t get_idx_of_nth_char( const char * string, char c, int n )
{
  if ( n == 0 ) return UINT32_MAX;
  if ( n > 0 ) {
    for ( uint_fast32_t idx_char = 0; string[idx_char] != 0; idx_char++ ) {
      if ( string[idx_char] == c ) {
        n--;
        if ( n == 0 ) {
          return idx_char;
        }
      }
    }
  } else {
    n = -n;
    uint_fast32_t len = util_strlen(string);
    for ( uint_fast32_t idx_char = len - 1; idx_char != UINT_FAST32_MAX; idx_char-- ) {
      if ( string[idx_char] == c ) {
        n--;
        if ( n == 0 ) {
          return idx_char;
        }
      }
    }
  }
  return UINT32_MAX;
}

//-------------------------------------------------------------------------------------------------
void itoa( int n, char * s )
{
  int i, sign;

  if ( (sign = n) < 0 ) {
    n = -n;
  }

  i = 0;
  do {
    s[i] = n % 10 + '0';
    i++;
  } while ( (n /= 10) > 0 );

  if ( sign < 0 ) {
    s[i] = '-';
    i++;
  }
  s[i] = 0;

  to_another_byte_order(util_endian_BIG_ENDIAN, util_endian_LITTLE_ENDIAN, s, i);
}

//-------------------------------------------------------------------------------------------------
static unsigned int count_1bits_brute( unsigned int length, const uint8_t string[static length] )
{
  unsigned int count = 0;
  for ( unsigned int idx_byte = 0; idx_byte < length; idx_byte++ ) {
    for ( uint_fast8_t idx_bit = 0; idx_bit < 8; idx_bit++ ) {
      if ( string[idx_byte] & 1<<idx_bit ) {
        count++;
      }
    }
  }
  return count;
}

//-------------------------------------------------------------------------------------------------
__attribute__ ((target ("popcnt")))
static unsigned int count_1bits_POPCNT( unsigned int length, const uint8_t string[static length] )
{
  unsigned int iter_cap = length / sizeof(unsigned int) * sizeof(unsigned int);
  unsigned int count = 0;

  unsigned int idx_byte = 0;
  for ( ; idx_byte != iter_cap; idx_byte += sizeof(unsigned int) ) {
    const unsigned int * ptr = (void*) string + idx_byte;
    count += _mm_popcnt_u32(*ptr);
  }

  const unsigned int * ptr = (void*) string + idx_byte;
  unsigned int dword = *ptr;
  to_another_byte_order( util_endian_BIG_ENDIAN, util_endian_LOCAL_ENDIAN,
      &dword, sizeof(typeof(dword)) );

  typeof(dword) mask = ~0;
  dword = dword & mask << 8*(sizeof(typeof(dword)) - (length-idx_byte));

  count += _mm_popcnt_u32( dword );

  return count;
}

/*
__attribute__((always_inline))
static inline void rotate_bits_left_1byte(
    unsigned int string_len, uint8_t string[static string_len], uint8_t nr_bits_to_rotate) {
  uint8_t typeref; uint_fast8_t typeref2;
  for ( uint_fast8_t nr_bits_shifted = 0; nr_bits_shifted < nr_bits_to_rotate; ) {
    uint_fast8_t nr_bits_to_rotate_this_round =
      nr_bits_to_rotate - nr_bits_shifted <= sizeof(typeref)*8-1 ?
      nr_bits_to_rotate - nr_bits_shifted : sizeof(typeref)*8-1;

    for ( uint_fast16_t idx_byte = 0; idx_byte < string_len; idx_byte += sizeof(typeref) ) {
      typeof(typeref) * ptr = string + idx_byte;
      uint16_t tmp = _rotwl((uint16_t)(*ptr)<<8, nr_bits_to_rotate_this_round);
      *ptr = tmp >> 8 | tmp;
    }

    typeof(typeref) buffer = *(typeof(typeref)*)string;
    typeof(typeref) mask = ~0;
    mask = mask << nr_bits_to_rotate_this_round;
    uint_fast16_t idx_byte = 0;
    for ( ; idx_byte != string_len - sizeof(typeref);
        idx_byte += sizeof(typeref) ) {
      typeof(typeref) * ptr = string + idx_byte;
      *ptr &= mask;
      *ptr |= ~mask & *(ptr + 1);
    }
    typeof(typeref) * ptr = string + idx_byte;
    *ptr &= mask;
    *ptr |= ~mask & buffer;

    nr_bits_shifted += nr_bits_to_rotate_this_round;
  }
}

__attribute__((always_inline))
static inline void rotate_bits_left_2byte(
    unsigned int string_len, uint8_t string[static string_len], uint8_t nr_bits_to_rotate) {
  uint16_t typeref; uint_fast16_t typeref2;
  for ( uint_fast16_t idx_word = 0; idx_word < string_len; idx_word += sizeof(typeref) ) {
    typeof(typeref) * ptr = (void*) string + idx_word;
    to_another_byte_order(0, util_endian_BIG_ENDIAN, ptr, sizeof(*ptr));
  }
  for ( uint_fast8_t nr_bits_shifted = 0; nr_bits_shifted < nr_bits_to_rotate; ) {
    uint_fast8_t nr_bits_to_rotate_this_round =
      nr_bits_to_rotate - nr_bits_shifted <= sizeof(typeref)*8-1 ?
      nr_bits_to_rotate - nr_bits_shifted : sizeof(typeref)*8-1;

    for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
      typeof(typeref) * ptr = (void*) string + idx_dword;
      *ptr = _rotwl(*ptr, nr_bits_to_rotate_this_round);
    }

    typeof(typeref) buffer = *(typeof(typeref)*)string;
    typeof(typeref) mask = ~0;
    mask = mask << nr_bits_to_rotate_this_round;
    uint_fast16_t idx_word = 0;
    for ( ; idx_word != string_len - sizeof(typeref);
        idx_word += sizeof(typeref) ) {
      typeof(typeref) * ptr = (void*) string + idx_word;
      *ptr &= mask;
      *ptr |= ~mask & *(ptr + 1);
    }
    typeof(typeref) * ptr = (void*) string + idx_word;
    *ptr &= mask;
    *ptr |= ~mask & buffer;

    nr_bits_shifted += nr_bits_to_rotate_this_round;
  }
  for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
    typeof(typeref) * ptr = (void*) string + idx_dword;
    to_another_byte_order(util_endian_BIG_ENDIAN, 0, ptr, sizeof(*ptr));
  }
}

__attribute__((always_inline))
static inline void rotate_bits_left_4byte(
    unsigned int string_len, uint8_t string[static string_len], uint8_t nr_bits_to_rotate) {
  uint32_t typeref; uint32_t typeref2;
  for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
    typeof(typeref) * ptr = (void*) string + idx_dword;
    to_another_byte_order(0, util_endian_BIG_ENDIAN, ptr, sizeof(*ptr));
  }
  for ( uint_fast8_t nr_bits_shifted = 0; nr_bits_shifted < nr_bits_to_rotate; ) {
    uint_fast8_t nr_bits_to_rotate_this_round =
      nr_bits_to_rotate - nr_bits_shifted <= sizeof(typeref)*8-1 ?
      nr_bits_to_rotate - nr_bits_shifted : sizeof(typeref)*8-1;

    for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
      typeof(typeref) * ptr = (void*) string + idx_dword;
      *ptr = __builtin_rotateleft64(*ptr, nr_bits_to_rotate_this_round);
    }

    typeof(typeref) buffer = *(typeof(typeref)*)string;
    typeof(typeref) mask = ~0;
    mask = mask << nr_bits_to_rotate_this_round;
    uint_fast16_t idx_dword = 0;
    for ( ; idx_dword != string_len - sizeof(typeref);
        idx_dword += sizeof(typeref) ) {
      typeof(typeref) * ptr = (void*) string + idx_dword;
      *ptr &= mask;
      *ptr |= ~mask & *(ptr + 1);
    }
    typeof(typeref) * ptr = (void*) string + idx_dword;
    *ptr &= mask;
    *ptr |= ~mask & buffer;

    nr_bits_shifted += nr_bits_to_rotate_this_round;
  }
  for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
    typeof(typeref) * ptr = (void*) string + idx_dword;
    to_another_byte_order(util_endian_BIG_ENDIAN, 0, ptr, sizeof(*ptr));
  }
}

__attribute__((always_inline))
static inline void rotate_bits_left_8byte(
    unsigned int string_len, uint8_t string[static string_len], uint8_t nr_bits_to_rotate) {
  uint64_t typeref; uint64_t typeref2;
  for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
    typeof(typeref) * ptr = (void*) string + idx_dword;
    to_another_byte_order(0, util_endian_BIG_ENDIAN, ptr, sizeof(*ptr));
  }
  for ( uint_fast8_t nr_bits_shifted = 0; nr_bits_shifted < nr_bits_to_rotate; ) {
    uint_fast8_t nr_bits_to_rotate_this_round =
      nr_bits_to_rotate - nr_bits_shifted <= sizeof(typeref)*8-1 ?
      nr_bits_to_rotate - nr_bits_shifted : sizeof(typeref)*8-1;

    for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
      typeof(typeref) * ptr = (void*) string + idx_dword;
      *ptr = _rotl(*ptr, nr_bits_to_rotate_this_round);
    }

    typeof(typeref) buffer = *(typeof(typeref)*)string;
    typeof(typeref) mask = ~0;
    mask = mask << nr_bits_to_rotate_this_round;
    uint_fast16_t idx_dword = 0;
    for ( ; idx_dword != string_len - sizeof(typeref);
        idx_dword += sizeof(typeref) ) {
      typeof(typeref) * ptr = (void*) string + idx_dword;
      *ptr &= mask;
      *ptr |= ~mask & *(ptr + 1);
    }
    typeof(typeref) * ptr = (void*) string + idx_dword;
    *ptr &= mask;
    *ptr |= ~mask & buffer;

    nr_bits_shifted += nr_bits_to_rotate_this_round;
  }
  for ( uint_fast16_t idx_dword = 0; idx_dword < string_len; idx_dword += sizeof(typeref) ) {
    typeof(typeref) * ptr = (void*) string + idx_dword;
    to_another_byte_order(util_endian_BIG_ENDIAN, 0, ptr, sizeof(*ptr));
  }
}

// 01011001 11000111 10000111 11000001
// 11001010 00111110 00111100 00001110
//      to3      to0      to1      to2
// 11001!10 001111o0 001111!0 00001o10
void rotate_bits_left(unsigned int string_len, uint8_t string[static string_len], uint8_t nr_bits_to_shift) {
  uint8_t nr_bytes_each_round;
  if ( _is_aligned_to_power_of_two(string_len, 4) ) {
    nr_bytes_each_round = 4;
  } else if ( util_endian_LOCAL_ENDIAN == util_endian_BIG_ENDIAN ) {
    if ( _is_aligned_to_power_of_two(string_len, 2) ) {
      nr_bytes_each_round = 2;
    }
  } else {
    nr_bytes_each_round = 1;
  }

  switch ( nr_bytes_each_round ) {
    case 1:
      rotate_bits_left_1byte(string_len, string, nr_bits_to_shift);
      break;
    case 2:
      rotate_bits_left_2byte(string_len, string, nr_bits_to_shift);
      break;
    case 4:
      rotate_bits_left_4byte(string_len, string, nr_bits_to_shift);
      break;
  }
}
*/

//-------------------------------------------------------------------------------------------------
uint16_t clear_bits_left( unsigned int string_len, uint8_t string[static string_len],
    uint16_t nr_bits_to_clear )
{
  uint16_t nr_bits_cleared = 0;
  for ( uint_fast8_t idx_byte = 0; idx_byte < string_len; idx_byte++ ) {
    uint8_t nr_bits = count_1bits(1, &string[idx_byte]);
    uint8_t nr_bits_to_clear_this_round = nr_bits;
    if ( nr_bits_cleared + nr_bits_to_clear_this_round > nr_bits_to_clear ) {
      nr_bits_to_clear_this_round = nr_bits_to_clear - nr_bits_cleared;
    }

    uint8_t nr_bits_cleared_this_round = 0;
    for ( uint8_t idx_bit = 0; idx_bit < 8; idx_bit++ ) {
      if ( string[idx_byte] & (0x80 >> idx_bit) ) {
        string[idx_byte] &= ~(0x80 >> idx_bit);
        nr_bits_cleared_this_round++;
        if ( nr_bits_cleared_this_round == nr_bits_to_clear_this_round ) {
          break;
        }
      }
    }

    nr_bits_cleared += nr_bits_cleared_this_round;

    if ( nr_bits_cleared == nr_bits_to_clear ) {
      return 0;
    }
  }

  return nr_bits_cleared+1;
}

//-------------------------------------------------------------------------------------------------
uint_fast8_t count_nr_consec_1bits(
    unsigned int string_len,
    const uint8_t string[static string_len],
    uint8_t idx_start )
{
  uint_fast8_t nr_consec_bits = 0;
  for ( uint_fast8_t idx_bit = idx_start; idx_bit < string_len*8; idx_bit++ ) {
    if ( string[idx_bit/8] & 0x80 >> idx_bit%8 ) {
      nr_consec_bits++;
    } else {
      return nr_consec_bits;
    }
  }
  return nr_consec_bits;
}

//-------------------------------------------------------------------------------------------------
uint_fast8_t count_nr_consec_0bits(
    unsigned int string_len,
    const uint8_t string[static string_len],
    uint8_t idx_start )
{
  uint_fast8_t nr_consec_bits = 0;
  for ( uint_fast8_t idx_bit = idx_start; idx_bit < string_len*8; idx_bit++ ) {
    if ( (string[idx_bit/8] & 0x80 >> idx_bit%8) == 0 ) {
      nr_consec_bits++;
    } else {
      return nr_consec_bits;
    }
  }
  return nr_consec_bits;
}

//-------------------------------------------------------------------------------------------------
//uint_fast8_t a_way_to_get_log2_u32( uint32_t number )
//{
//  constexpr uint8_t piece_of_a_de_Bruijn_sequence[32] =
//  {
//    0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
//    8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
//  };
//
//  number |= number >> 1;
//  number |= number >> 2;
//  number |= number >> 4;
//  number |= number >> 8;
//  number |= number >> 16;
//
//  return piece_of_a_de_Bruijn_sequence[(uint32_t)(number * 0x07c4acddu) >> 27];
//}

//-------------------------------------------------------------------------------------------------
uint16_t get_idx_of_nth_1( unsigned int string_len, const uint8_t string[static string_len],
    uint8_t n_minus_one )
{
  uint16_t count = ~0;
  uint_fast16_t idx_byte = 0;
  for (; idx_byte < string_len; idx_byte++ ) {
    for ( uint_fast8_t idx_bit = 0; idx_bit < 8; idx_bit++ ) {
      if ( string[idx_byte] & 1<<idx_bit ) {
        count++;
        if ( count == n_minus_one ) {
          return idx_byte*8 + idx_bit;
        }
      }
    }
  }
  return UINT16_MAX;
  // TODO Implement different types because when bits are few and far between, you might be able to
  // consider using the combination of _tzcnt_u32 and log2_ui32, or another method.
  //uint32_t buffer;
  //for (; idx_byte < string_len; idx_byte+=sizeof(buffer) ) {
  //  if ( idx_byte+sizeof(buffer)-1 >= string_len ) {
  //    break;
  //  }

  //  buffer = *(uint32_t*)(string + idx_byte);
  //  if ( buffer == 0 ) {
  //    continue;
  //  }
  //  to_another_byte_order( util_endian_BIG_ENDIAN, 0, &buffer, sizeof(buffer) );
  //  uint_fast8_t position = log2_ui32(buffer);
  //}
}

//-------------------------------------------------------------------------------------------------
#if defined(__clang__)
__attribute__ ((minsize))
#elif defined(__GNUC__) || defined(__GNUG__)
__attribute__ ((optimize("s")))
#endif
static uint64_t init_x86(void)
{
  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = 0;

  {
    union {
      char text[16];
      uint32_t reg[4];  // ebx, ecx, edx, padding;
    } vendor = { .text = {0} };

    uint32_t maxleaf;

    // maxleaf=eax, VenderID=ebx.edx.ecx
    __cpuid(0, maxleaf, vendor.reg[0], vendor.reg[2], vendor.reg[1]);

#ifdef DEBUG_MODE
    fprintf(stdout, "vendor:%s maxleaf=%u\n", vendor.text, maxleaf);
#endif
    if ( strcmp(vendor.text, "AuthenticAMD") == 0 ) {
      x86_proc_info.from_amd = 1;
    }
    x86_proc_info.maxleaf = maxleaf;
  }

#if !defined(__clang__) && !defined(__GNUC__)
  #error I am not sure about the bit-field packing order of other compilers.
#endif
  {
    union {
      struct {  // EAX
        uint8_t step: 4;
        uint8_t model: 4;
        uint8_t family: 4;
        uint8_t type: 2;
        uint8_t pad1: 2;
        uint8_t emodel: 4;
        uint8_t efamily: 8;
        uint8_t pad2: 4;
      };
      uint32_t reg;
    } eax = { .reg = 1 };
    union {
      struct {  // ECX
        uint32_t sse3: 1;
        uint32_t pclmulqdq: 1;
        uint32_t dtes64: 1;
        uint32_t monitor: 1;
        uint32_t dscpl: 1;
        uint32_t vmx: 1;
        uint32_t smx: 1;
        uint32_t est: 1;
        uint32_t tm2: 1;
        uint32_t ssse3: 1;
        uint32_t cntxid: 1;
        uint32_t sdbg: 1;
        uint32_t fma: 1;
        uint32_t cx16: 1;
        uint32_t xtpr: 1;
        uint32_t pdcm: 1;
        uint32_t reserved1: 1;
        uint32_t pcid: 1;
        uint32_t dca: 1;
        uint32_t sse41: 1;
        uint32_t sse42: 1;
        uint32_t x2apic: 1;
        uint32_t movbe: 1;
        uint32_t popcnt: 1;
        uint32_t tscdadline: 1;
        uint32_t aes: 1;
        uint32_t xsave: 1;
        uint32_t osxsave: 1;
        uint32_t avx: 1;
        uint32_t f16c: 1;
        uint32_t rdrnd: 1;
        uint32_t hypervisor: 1;
      };
      uint32_t reg;
    } ecx = { .reg = 0 };
    union {  // EDX
      struct {
        uint8_t fpu: 1;
        uint8_t vme: 1;
        uint8_t de: 1;
        uint8_t pse: 1;
        uint8_t tsc: 1;
        uint8_t msr: 1;
        uint8_t pae: 1;
        uint8_t mce: 1;
        uint8_t cx8: 1;
        uint8_t apic: 1;
        uint8_t reserved1: 1;
        uint8_t sep: 1;
        uint8_t mtrr: 1;
        uint8_t pge: 1;
        uint8_t mca: 1;
        uint8_t cmov: 1;
        uint8_t pat: 1;
        uint8_t pse36: 1;
        uint8_t psn: 1;
        uint8_t clfsh: 1;
        uint8_t reserved2: 1;
        uint8_t ds: 1;
        uint8_t acpi: 1;
        uint8_t mmx: 1;
        uint8_t fxsr: 1;
        uint8_t sse: 1;
        uint8_t sse2: 1;
        uint8_t ss: 1;
        uint8_t htt: 1;
        uint8_t tm: 1;
        uint8_t ia64: 1;
        uint8_t pbe: 1;
      };
      uint32_t reg;
    } edx = { .reg = 0 };

    uint32_t ebx = 0;
    __cpuid(1, eax.reg, ebx, ecx.reg, edx.reg);
    uint8_t model = (eax.family == 6 || eax.family == 15 ? eax.emodel << 4 : 0)
      + eax.model;
    uint16_t family = (eax.family == 15 ? eax.efamily : 0) + eax.family;
#ifdef DEBUG_MODE
    fprintf(stdout, "step: %u model: %u family: %u type: %u\n",
        eax.step, model, family, eax.type);
    fprintf(stdout, "fpu:%u mmx:%u sse:%u sse2:%u htt:%u ia64:%u\n",
        edx.fpu, edx.mmx, edx.sse, edx.sse2, edx.htt, edx.ia64);
    fprintf(stdout, "sse3:%d vmx:%d ssse3:%d fma:%d sse41:%d sse42:%d popcnt:%d aes:%d "
        "avx:%d hypervisor:%d\n",
        ecx.sse3, ecx.vmx, ecx.ssse3, ecx.fma, ecx.sse41, ecx.sse42,
        ecx.popcnt, ecx.aes, ecx.avx, ecx.hypervisor);
#endif

    x86_proc_info.cx8 = edx.cx8;
    x86_proc_info.tsc = edx.tsc;
    x86_proc_info.sse42 = ecx.sse42;
    x86_proc_info.popcnt = ecx.popcnt;
    x86_proc_info.avx = ecx.avx;
    x86_proc_info.model = model;
    x86_proc_info.family = family;
  }

  {
    union {
      struct {  // ECX
        uint8_t lahf_lm: 1;
        uint8_t cmp_legacy: 1;
        uint8_t svm: 1;
        uint8_t extapic: 1;
        uint8_t cr8_legacy: 1;
        uint8_t abm: 1;
        uint8_t sse4a: 1;
        uint8_t misalignsse: 1;
        uint8_t _3dnowprefetch: 1;
        uint8_t osvw: 1;
        uint8_t ibs: 1;
        uint8_t xop: 1;
        uint8_t skinit: 1;
        uint8_t wdt: 1;
        uint8_t reserved1: 1;
        uint8_t lwp: 1;
        uint8_t fma4: 1;
        uint8_t tce: 1;
        uint8_t reserved2: 1;
        uint8_t nodeid_msr: 1;
        uint8_t reserved3: 1;
        uint8_t tbm: 1;
        uint8_t topoext: 1;
        uint8_t perfctr_core: 1;
        uint8_t perfctr_nb: 1;
        uint8_t reserved4: 1;
        uint8_t dbx: 1;
        uint8_t perftsc: 1;
        uint8_t pcx_l2i: 1;
        uint8_t reserved5: 3;
      };
      uint32_t reg;
    } ecx = { .reg = 0 };
    union {
      struct {  // EDX
        uint8_t fpu: 1;
        uint8_t vme: 1;
        uint8_t de: 1;
        uint8_t pse: 1;
        uint8_t tsc: 1;
        uint8_t msr: 1;
        uint8_t pae: 1;
        uint8_t mce: 1;
        uint8_t cx8: 1;
        uint8_t apic: 1;
        uint8_t reserved1: 1;
        uint8_t syscall: 1;
        uint8_t mtrr: 1;
        uint8_t pge: 1;
        uint8_t mca: 1;
        uint8_t cmov: 1;
        uint8_t pat: 1;
        uint8_t pse36: 1;
        uint8_t reserved2: 1;
        uint8_t mp: 1;
        uint8_t nx: 1;
        uint8_t reserved3: 1;
        uint8_t mmxext: 1;
        uint8_t mmx: 1;
        uint8_t fxsr: 1;
        uint8_t fxsr_opt: 1;
        uint8_t pdpe1gb: 1;
        uint8_t rdtscp: 1;
        uint8_t reserved4: 1;
        uint8_t lm: 1;
        uint8_t _3dnowext: 1;
        uint8_t _3dnow: 1;
      };
      uint32_t reg;
    } edx = { .reg=0 };
    uint32_t ebx = 0;
    uint32_t eax = 0x80000001u;
    __cpuid(0x80000001u, eax, ebx, ecx.reg, edx.reg);

    x86_proc_info.cx8 = edx.cx8;
    x86_proc_info.tsc = edx.tsc;
    x86_proc_info.rdtscp = edx.rdtscp;
    x86_proc_info.perftsc = ecx.perftsc;
    x86_proc_info.msr = edx.msr;
#ifdef DEBUG_MODE
    fprintf(stdout, "abm:%u fma4:%u perfctr_core:%u perfctr_nb:%u perftsc:%u pcx_l2i:%u\n"
        "rdtscp:%u ",
        ecx.abm, ecx.fma4, ecx.perfctr_core, ecx.perfctr_nb, ecx.perftsc,
        ecx.pcx_l2i, edx.rdtscp);
    fprintf(stdout, "tsc:%u cx8:%u msr:%u ...\n",
        x86_proc_info.tsc, x86_proc_info.cx8, x86_proc_info.msr);
#endif

  }

  {
    union {  // EBX
      struct {
        uint8_t fsgsbase: 1;
        uint8_t ia32tscadjust: 1;
        uint8_t sgx: 1;
        uint8_t bmi1: 1;
        uint8_t hle: 1;
        uint8_t avx2: 1;
        uint8_t reserved1: 1;
        uint8_t smep: 1;
        uint8_t bmi2: 1;
        uint8_t erms: 1;
        uint8_t invpcid: 1;
        uint8_t rtm: 1;
        uint8_t pqm: 1;
        uint8_t fpucsdsdeprecated: 1;
        uint8_t mpx: 1;
        uint8_t pqe: 1;
        uint8_t avx512f: 1;
        uint8_t avx512dq: 1;
        uint8_t rdseed: 1;
        uint8_t adx: 1;
        uint8_t smap: 1;
        uint8_t avx512ifma: 1;
        uint8_t pcommit: 1;
        uint8_t clflushopt: 1;
        uint8_t clwb: 1;
        uint8_t intelproctrace: 1;
        uint8_t avx512pf: 1;
        uint8_t avx512er: 1;
        uint8_t avx512cd: 1;
        uint8_t sha: 1;
        uint8_t avx512bw: 1;
        uint8_t avx512vl: 1;
      };
      uint32_t reg;
    } ebx = { .reg = 0 };
    union {  // ECX
      struct {
        uint8_t prefetchwt1: 1;
        uint8_t avx512vbmi: 1;
        uint32_t reserved: 30;
      };
      uint32_t reg;
    } ecx = { .reg=0 };
    uint32_t edx = 0;
    uint32_t eax = 7;
    __cpuid_count(7, 0, eax, ebx.reg, ecx.reg, edx);  // leaf and count = EAX:0x7 ECX:0x0
#ifdef DEBUG_MODE
    fprintf(stdout, "bmi1:%u avx2:%u bmi2:%u avx512f:%u avx512dq:%u avx512ifma:%u\n"
        "avx512pf:%u avx512er:%u avx512cd:%u sha:%u avx512bw:%u "
        "avx512vl:%u avx512vbmi:%u\n",
        ebx.bmi1, ebx.avx2, ebx.bmi2, ebx.avx512f, ebx.avx512dq,
        ebx.avx512ifma, ebx.avx512pf, ebx.avx512er, ebx.avx512cd, ebx.sha,
        ebx.avx512bw, ebx.avx512vl, ecx.avx512vbmi);
#endif

    x86_proc_info.bmi1 = ebx.bmi1;
    x86_proc_info.bmi2 = ebx.bmi2;
    x86_proc_info.avx2 = ebx.avx2;
  }

  // EAX=80000007h: Processor Power Management Information and RAS Capabilities
  {
    union {  // EDX
      struct {
        uint8_t ts: 1;
        uint8_t fid: 1;
        uint8_t vid: 1;
        uint8_t ttp: 1;
        uint8_t tm: 1;
        uint8_t stc: 1;
        uint8_t one_hundred_mhz_steps: 1;
        uint8_t hw_p_state: 1;
        uint8_t tsc_invariant: 1;
        uint8_t cpb: 1;
        uint8_t eff_frq_ro: 1;
        uint8_t proc_feedback_interface: 1;
        uint8_t proc_power_reporting: 1;
        uint8_t connected_standby: 1;
        uint8_t rapl: 1;
        uint8_t fast_cppc: 1;
        uint8_t succor: 1;
        uint8_t hwa: 1;
        uint8_t scalable_mca: 1;
        uint16_t reserved: 16;
      };
      uint32_t reg;
    } edx = { .reg = 0 };
    uint32_t ecx = 0;
    uint32_t ebx = 0;
    uint32_t eax = 0x80000007u;
    __cpuid(0x80000007u, eax, ebx, ecx, edx.reg);
#ifdef DEBUG_MODE
    fprintf(stdout, "tsc_invariant:%u\n", edx.tsc_invariant);
#endif

    x86_proc_info.tsc_invariant = edx.tsc_invariant;

  }

  // appdev_support-timer.c will call cpuid themself.
#ifdef DEBUG_MODE
  if ( x86_proc_info.maxleaf < 0x15 ) goto end_of_probing;
  // EAX=15h: Time Stamp Counter and Nominal Core Crystal Clock Information Leaf
  // (Initial EAX Value = 15H)
  {
    uint32_t eax, ebx, ecx, edx;
    __cpuid(0x15, eax, ebx, ecx, edx);
    fprintf(stdout, "EAX=15h; eax(art_to_tsc_denominator):%u ebx(art_to_tsc_numerator):%u "
        "ecx(art_to_tsc_hz):%u edx(unused):%u\n",
        eax, ebx, ecx, edx);
  }

  if ( x86_proc_info.maxleaf < 0x16 ) goto end_of_probing;
  // EAX=16h: Processor Frequency Information Leaf (Initial EAX Value = 16H)
  {
    union {
      struct {
        uint16_t proc_base_freq: 16;
        uint16_t reserved: 16;
      };
      uint32_t reg;
    } eax = { .reg = 0 };
    union {
      struct {
        uint16_t max_freq: 16;
        uint16_t reserved: 16;
      };
      uint32_t reg;
    } ebx = { .reg = 0 };
    union {
      struct {
        uint16_t bus_freq: 16;
        uint16_t reserved: 16;
      };
      uint32_t reg;
    } ecx = { .reg = 0 };

    uint32_t edx = 0;

    __cpuid(0x16, eax.reg, ebx.reg, ecx.reg, edx);
    fprintf(stdout, "Processor Base Frequency (in MHz):%u\n"
        "Maximum Frequency (in MHz):%u\n"
        "Bus (Reference) Frequency (in MHz):%u\n",
        eax.proc_base_freq, ebx.max_freq, ecx.bus_freq);
  }
#endif


#ifdef DEBUG_MODE
end_of_probing:
#endif

  // Use POPCNT instruction if available.
  if ( x86_proc_info.popcnt ) {
    count_1bits = count_1bits_POPCNT;
  }

  return x86_proc_info.whole;

}


//-------------------------------------------------------------------------------------------------
#if defined(__clang__)
__attribute__ ((minsize))
#elif defined(__GNUC__) || defined(__GNUG__)
__attribute__ ((optimize("s")))
#endif
static void init_lut_byte_flip(void)
{
  for ( uint_fast8_t idx = 0; idx < 16; idx++ ) {
    uint8_t src = idx;
    lut_byte_flip[idx] = src >> (4-1)
      | (src << (4+1) & 0xff) >> ((4+1)+(4-1-1)) << 1
      | (src << (4+2) & 0xff) >> ((4+2)+(4-1-2)) << 2
      | (src << (4+3) & 0xff) >> ((4+3)+(4-1-3)) << 3;
    // The part with the format of `(4+[1-3])+(4-1-[1-3])` is always 7

#ifdef DEBUG_MODE
    //fprintf(stdout, "lut_byte_flip[%u] = %u\n", idx, lut_byte_flip[idx]);
#endif
  }
}


//-------------------------------------------------------------------------------------------------
uint64_t init_util_c_lang(void)
{
  count_1bits = count_1bits_brute;

  uint64_t ret;
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64)\
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  ret = init_x86();
#endif

  // Prepare the look-up table for byte flips.
  init_lut_byte_flip();

  return ret;
}


