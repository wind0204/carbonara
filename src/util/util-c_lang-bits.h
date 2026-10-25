#if !defined UTIL_C_LANG_BITS_H_
#define UTIL_C_LANG_BITS_H_


#include <stdint.h>
#include <immintrin.h>
//#include <stdio.h>

#include "util-c_lang-basic.h"

//#define __bswap_template \
//{\
//  uint64_t new_value = 0;\
//\
//  for ( uint_fast8_t i = 1; i <= sizeof(typeof(field)) / 2u; i++ ) {\
//    new_value |= (field & (0xffu << (sizeof(typeof(field)) - i)*8u))\
//      >> (sizeof(typeof(field)) - i*2u - 1u)*8u;\
//  }\
//  for ( uint_fast8_t i = sizeof(typeof(field)) / 2u + 1; i <= sizeof(typeof(field)); i++ ) {\
//    new_value |= (field & (0xffu << (sizeof(typeof(field)) - i)*8u))\
//      << ((i - sizeof(typeof(field)) / 2u) * 2u - 1u) * 8u;\
//  }\
//\
//  return new_value;\
//}
//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint64_t bswap_64( uint64_t field )
{
  field =
    ( (field & UINT64_C(0xff00000000000000)) >> 56u ) |
    ( (field & UINT64_C(0x00ff000000000000)) >> 40u ) |
    ( (field & UINT64_C(0x0000ff0000000000)) >> 24u ) |
    ( (field & UINT64_C(0x000000ff00000000)) >>  8u ) |
    ( (field & UINT64_C(0x00000000ff000000)) <<  8u ) |
    ( (field & UINT64_C(0x0000000000ff0000)) << 24u ) |
    ( (field & UINT64_C(0x000000000000ff00)) << 40u ) |
    ( (field & UINT64_C(0x00000000000000ff)) << 56u );
  return field;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint32_t bswap_32( uint32_t field )
{
  field =
    ((field & 0xff000000u) >> 24u) |
    ((field & 0x00ff0000u) >>  8u) |
    ((field & 0x0000ff00u) <<  8u) |
    ((field & 0x000000ffu) << 24u);
  return field;
}

//-------------------------------------------------------------------------------------------------
/**
 * BSWAP on 16-bit field is not supported in x86.
 * XCHG or ROLW+RORW are the instructions I wanted the compiler to write.
 */
__attribute__((always_inline))
static inline uint16_t bswap_16( uint16_t field )
{
  field =
    ((field & 0xff00u) >>  8u) |
    ((field & 0x00ffu) <<  8u);
  return field;
}


//-------------------------------------------------------------------------------------------------
enum util_endian_byte_order : unsigned int {
  util_endian_byte_order_LOCAL,  // used when passing 0 to function to_another_byte_order
  util_endian_LITTLE_ENDIAN, // 1 out of 0x03020100u
  util_endian_BIG_ENDIAN,  // 2 out of 0x00010203u
  util_endian_PDP_ENDIAN,  // 3 out of 0x01000302u (16-bit little-endian)
	util_endian_byte_order_cap
};
//#define util_endian_LITTLE_ENDIAN 1
//#define util_endian_BIG_ENDIAN 2
//#define util_endian_PDP_ENDIAN 3

static const uint32_t ENDIAN_TEST_VALUE = 0x00010203u;
//static uint32_t util_endian_LOCAL_ENDIAN = *(((uint8_t*)&ENDIAN_TEST_VALUE)+2);
#define util_endian_LOCAL_ENDIAN ( *(((uint8_t*)&ENDIAN_TEST_VALUE)+2) )


//-------------------------------------------------------------------------------------------------
/**
 * \brief Use this macro to rearrange bytes to another order.
 *
 * param orig_order,new_order An enum util_endian_byte_order, if you set \a orig_order
 *    or \a new_order to 0 (i.e. util_endian_byte_order_LOCAL),
 *    it will be replaced with the corrosponding local endian value.
 * param ptr The pointer to the storage in which you want to rearrange the data.
 * param length The number of bytes you want to rearrange. It must not be an odd number.
 */
__attribute__((always_inline))
static inline void to_another_byte_order(
    enum util_endian_byte_order orig_order, enum util_endian_byte_order new_order,
    void * ptr, unsigned int length )
{
  //__ct_assert((orig_order)<util_endian_byte_order_cap && (orig_order)>=1, util_c_lang_bits);
  //__ct_assert(((new_order)<=util_endian_byte_order_cap&& (new_order)>=1), util_c_lang_bits);
  //__ct_assert(!(length&1), util_c_lang_bits);
  if ( orig_order == 0 ) {
    orig_order = util_endian_LOCAL_ENDIAN;
  }
  if ( new_order == 0 ) {
    new_order = util_endian_LOCAL_ENDIAN;
  }

  if ( orig_order != new_order ) {
    unsigned int i, j;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
    switch ( new_order ) {
      case util_endian_LITTLE_ENDIAN:
        if (orig_order == util_endian_BIG_ENDIAN) {
reverse_order:
          switch ( length ) {
            case 8:
              *(uint64_t*)ptr = bswap_64(*(uint64_t*)ptr);
              break;
            case 4:
              *(uint32_t*)ptr = bswap_32(*(uint32_t*)ptr);
              break;
            case 2:
              *(uint16_t*)ptr = bswap_16(*(uint16_t*)ptr);
              break;
            case 1:
              for (i=0, j=(length)-1; i<j; i++, j--) {
                _swap((uint8_t*)(ptr)+i, (uint8_t*)(ptr)+j)
              }
              break;
          }
        } else {
          for (i=0, j=((length)>>1)-1; i<j; i++, j--) {
            _swap((uint16_t*)(ptr)+i, (uint16_t*)(ptr)+j)
          }
        }
        break;
      case util_endian_BIG_ENDIAN:
        if (orig_order == util_endian_LITTLE_ENDIAN) {
          goto reverse_order;
        } else {
          for (i=0, j=1; j<(length); i=++j, j++) {
            _swap((uint8_t*)(ptr)+i, (uint8_t*)(ptr)+j)
          }
        }
        break;
      case util_endian_PDP_ENDIAN:
        if (orig_order == util_endian_BIG_ENDIAN) {
          for (i=0, j=1; j<(length); i=++j, j++) {
            _swap((uint8_t*)(ptr)+i, (uint8_t*)(ptr)+j)
          }
        } else {
          for (i=0, j=((length)>>1)-1; i<j; i++, j--) {
            _swap((uint16_t*)(ptr)+i, (uint16_t*)(ptr)+j)
          }
        }
        break;
    }
#pragma GCC diagnostic pop
  }
}

//static_assert((from)<=util_endian_byte_order_cap && (from)>=1, "Invalid byte order");\
//static_assert(((to)<=util_endian_byte_order_cap && (to)>=1), "Invalid byte order");\
//static_assert(!(length&1), "length must be an even number");
//#define __impl_to_another_byte_order(from, to, ptr, length)\
//{\
//  if ( from != to ) {\
//    int _i, _j;\
//    switch (to) {\
//      case util_endian_LITTLE_ENDIAN:\
//        if (from == util_endian_BIG_ENDIAN) {\
//          for (_i=0, _j=(length)-1; _i<_j; _i++, _j--) {\
//            _swap((uint8_t*)(ptr)+_i, (uint8_t*)(ptr)+_j)\
//          }\
//        } else {\
//          for (_i=0, _j=((length)>>1)-1; _i<_j; _i++, _j--) {\
//            _swap((uint16_t*)(ptr)+_i, (uint16_t*)(ptr)+_j)\
//          }\
//        }\
//        break;\
//      case util_endian_BIG_ENDIAN:\
//        if (from == util_endian_LITTLE_ENDIAN) {\
//          for (_i=0, _j=(length)-1; _i<_j; _i++, _j--) {\
//            _swap((uint8_t*)(ptr)+_i, (uint8_t*)(ptr)+_j)\
//          }\
//        } else {\
//          for (_i=0, _j=1; _j<(length); _i=++_j, _j++) {\
//            _swap((uint8_t*)(ptr)+_i, (uint8_t*)(ptr)+_j)\
//          }\
//        }\
//        break;\
//      case util_endian_PDP_ENDIAN:\
//        if (from == util_endian_BIG_ENDIAN) {\
//          for (_i=0, _j=1; _j<(length); _i=++_j, _j++) {\
//            _swap((uint8_t*)(ptr)+_i, (uint8_t*)(ptr)+_j)\
//          }\
//        } else {\
//          for (_i=0, _j=((length)>>1)-1; _i<_j; _i++, _j--) {\
//            _swap((uint16_t*)(ptr)+_i, (uint16_t*)(ptr)+_j)\
//          }\
//        }\
//        break;\
//    }\
//  }\
//}
///**
// * Use this macro to rearrange bytes to another order.
// *
// * Oparam from,to An enum util_endian_byte_order, if you set 'from' or 'to'
// *    to 0 (i.e. util_endian_byte_order_LOCAL),
// *    it will be replaced with util_endian_LOCAL_ENDIAN
// * Oparam ptr The pointer to the storage at which you want to rearrange the data.
// * Oparam length The number of bytes you want to rearrange. I must not be an odd number.
// */
//#define __to_another_byte_order(from, to, ptr, length)\
//  __impl_to_another_byte_order((from?from:util_endian_LOCAL_ENDIAN),\
//      to?to:util_endian_LOCAL_ENDIAN, ptr, length)


extern uint8_t lut_byte_flip[16];
//-------------------------------------------------------------------------------------------------
/**
 * \brief Flip a byte
 */
#define _flip_byte(byte) \
  (( (lut_byte_flip[(byte)&0x0f]) << 8/2 ) | ( lut_byte_flip[(byte)>>8/2] ))

//-------------------------------------------------------------------------------------------------
/**
 * \warning \a bit_left and \a bit_right must have one and only one bit set to 1.
 *
 * \param bit_left must be a power of two and equal or greather than \a bit_right
 * \param bit_right must be a power of two and equal or less than \a bit_left
 */
#define _get_distance_between_bit_locations( bit_left, bit_right, ptr_retval )  \
{                                                                               \
  typeof(*(ptr_retval)) nr_shifts = 0;                                          \
  typeof(bit_left) bit = (bit_left);                                            \
  while(-7) {                                                                   \
    if ( bit == (bit_right) ) {                                                 \
      break;                                                                    \
    }                                                                           \
    nr_shifts++;                                                                \
    bit = bit >> 1;                                                             \
  }                                                                             \
  *(ptr_retval) = (typeof(*ptr_retval))nr_shifts;                               \
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Obtain the least significant /a nr_bits_to_obtain bits
 */
#define _least_significant_bits( number, nr_bits_to_obtain ) \
  ( (unsigned) (typeof((number)))(0 - 1) >> (8*sizeof((number)) - (nr_bits_to_obtain)) & (number) )

//#define _is_aligned_to_least_significant_mask(n_to_test, mask, nr_bits_to_obtain) \
//  ( _least_significant_bits((n_to_test), (nr_bits_to_obtain)) == (mask) )

//-------------------------------------------------------------------------------------------------
// TODO This function has never been tested.
#define align_lsbs_downward( number, nr_bytes_aligned_to, mask, ptr_retval )  \
{                                                                             \
  typeof(*(ptr_retval)) retval = (number);                                    \
  if ( ((number) & ((nr_bytes_aligned_to) - 1)) != (mask) ) {                 \
    if ( ((number) & ((nr_bytes_aligned_to) - 1)) < (mask) ) {                \
      retval = (number) - (nr_bytes_aligned_to);                              \
    }                                                                         \
    retval = retval & ~((nr_bytes_aligned_to) - 1) | (mask);                  \
  }                                                                           \
  *(ptr_retval) = retval;                                                     \
}

//-------------------------------------------------------------------------------------------------
// TODO This function has never been tested.
#define align_lsbs_upward( number, nr_bytes_aligned_to, mask, ptr_retval )  \
{                                                                           \
  typeof(*(ptr_retval)) retval = (number);                                  \
  if ( ((number) & ((nr_bytes_aligned_to) - 1)) != (mask) ) {               \
    if ( ((number) & ((nr_bytes_aligned_to) - 1)) > (mask) ) {              \
      retval = (number) + (nr_bytes_aligned_to);                            \
    }                                                                       \
    retval = retval & ~((nr_bytes_aligned_to) - 1) | (mask);                \
  }                                                                         \
  *(ptr_retval) = retval;                                                   \
}

//-------------------------------------------------------------------------------------------------
/**
 * \brief Count the number of 1-bits
 * \param length The length of \a str in bytes.
 */
extern unsigned int (*count_1bits)( unsigned int length, const uint8_t string[static length] );

//void rotate_bits_left(unsigned int string_len, uint8_t string[static string_len], uint8_t nr_bits_to_shift);

//-------------------------------------------------------------------------------------------------
/**
 * \brief Try to clear the first \a nr_bits_to_clear bits that are already set as 1
 *    on the left part of \a string -the most siginicant part in big endian systems-
 * \param string_len The length of string in bytes
 * \return 0 if it was successful, "1 + the number of bits cleared" otherwise
 */
uint16_t clear_bits_left( unsigned int string_len, uint8_t string[static string_len],
    uint16_t nr_bits_to_clear );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Count the number of consecutive bits with the value set to 1
 *
 * \param starting_idx The index of the bit on which the code will start the scan
 */
uint_fast8_t count_nr_consec_1bits(
    unsigned int string_len,
    const uint8_t string[static string_len],
    uint8_t starting_idx );

//-------------------------------------------------------------------------------------------------
/**
 * \brief Count the number of consecutive bits with the value set to 0
 *
 * \param starting_idx The index of the bit on which the code will start the scan
 */
uint_fast8_t count_nr_consec_0bits(
    unsigned int string_len,
    const uint8_t string[static string_len],
    uint8_t starting_idx );


//-------------------------------------------------------------------------------------------------
/**
 *
 */
#define get_log2( number )                                                        \
  ( is_signed((number)) ? 1/(true^is_signed((number))) :                          \
    ( (sizeof(typeof((number))) > 4) ?                                                    \
      UINT64_C(63) - _lzcnt_u64((number)) : UINT32_C(31) - _lzcnt_u32((number)) ) \
  )
static_assert( true == is_signed(-7) );

//-------------------------------------------------------------------------------------------------
/**
 * \return UINT16_MAX if it couldn't find nth bit with the value set to 1
 *     The index of the nth bit otherwise.
 * \param n_minus_one n - 1 for the nth 1-bit, //e.g. 0 for the first 1.
 */
uint16_t get_idx_of_nth_1( unsigned int string_len, const uint8_t string[static string_len],
    uint8_t n_minus_one );

////#define _declare_vars_for_set_bitfield(len) \
////  uint ## len ## _t mask, item;
////#define _set_bitfield_template \
////  uint64_t head_bit_idx = item_idx * item_len_in_bits;\
////  uint32_t byte_idx = head_bit_idx / 8;\
////  uint32_t pos_head_bit_in_head_byte = head_bit_idx % 8;\
////  switch ( (item_len_in_bits + 8 - 1) / 8 ) {\
////    case 1:\
////      {\
////        _declare_vars_for_set_bitfield(16)\
////\
////        mask = ~0 << (sizeof(mask)*8 - item_len_in_bits);\
////        mask = mask >> pos_head_bit_in_head_byte;\
////        mask = ~mask;\
////\
////        item = value << (sizeof(item)*8 - item_len_in_bits);\
////        item = item  >> pos_head_bit_in_head_byte;\
////\
////        to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask, sizeof(mask));\
////        to_another_byte_order(0, util_endian_BIG_ENDIAN, &item, sizeof(item));\
////        typeof(&item) ptr = (typeof(&item))(arr_bits + byte_idx);\
////        *ptr &= mask;\
////        *ptr |= item;\
////      }\
////      break;\
////    case 2:\
////    case 3:\
////      {\
////        _declare_vars_for_set_bitfield(32)\
////\
////        mask = ~0 << (sizeof(mask)*8 - item_len_in_bits);\
////        mask = mask >> pos_head_bit_in_head_byte;\
////        mask = ~mask;\
////\
////        item = value << (sizeof(item)*8 - item_len_in_bits);\
////        item = item  >> pos_head_bit_in_head_byte;\
////\
////        to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask, sizeof(mask));\
////        to_another_byte_order(0, util_endian_BIG_ENDIAN, &item, sizeof(item));\
////        typeof(&item) ptr = (typeof(&item))(arr_bits + byte_idx);\
////        *ptr &= mask;\
////        *ptr |= item;\
////      }\
////      break;\
////    case 4:\
////    case 5:\
////    case 6:\
////    case 7:\
////      {\
////        _declare_vars_for_set_bitfield(64)\
////\
////        mask = ~0 << (sizeof(mask)*8 - item_len_in_bits);\
////        mask = mask >> pos_head_bit_in_head_byte;\
////        mask = ~mask;\
////\
////        item = value << (sizeof(item)*8 - item_len_in_bits);\
////        item = item  >> pos_head_bit_in_head_byte;\
////\
////        to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask, sizeof(mask));\
////        to_another_byte_order(0, util_endian_BIG_ENDIAN, &item, sizeof(item));\
////        typeof(&item) ptr = (typeof(&item))(arr_bits + byte_idx);\
////        *ptr &= mask;\
////        *ptr |= item;\
////      }\
////      break;\
////  }

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_u64( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, uint64_t value )
{
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;
  const uint32_t byte_idx = head_bit_idx / 8;
  const uint32_t qword_idx = byte_idx / (64/8);
  const uint32_t pos_msb_in_qword = head_bit_idx % 64;

  uint64_t mask, item;
  uint64_t mask_next_qword, item_next_qword;
  mask = ~(uint64_t)0 << (sizeof(mask)*8 - item_len_in_bits);
  mask = mask >> pos_msb_in_qword;
  mask = ~mask;
  mask_next_qword = ~(uint64_t)0 << (sizeof(mask)*8 - item_len_in_bits);
  mask_next_qword = mask_next_qword << (sizeof(mask)*8 - (pos_msb_in_qword?pos_msb_in_qword:1));
  mask_next_qword = ~mask_next_qword;

  item = value << (sizeof(item)*8 - item_len_in_bits);
  item = item  >> pos_msb_in_qword;
  item_next_qword = value << (sizeof(item)*8 - item_len_in_bits);
  item_next_qword = item_next_qword  << (sizeof(item)*8 - (pos_msb_in_qword?pos_msb_in_qword:1));

  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask, sizeof(mask));
  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &item, sizeof(item));
  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_next_qword, sizeof(mask_next_qword));
  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &item_next_qword, sizeof(item_next_qword));

  const typeof(&item) ptr = (typeof(ptr))arr_bits + qword_idx;
  const typeof(ptr) ptr_next = (typeof(ptr))arr_bits + qword_idx + 1;
  uint64_t first_qword = *ptr;
  uint64_t next_qword = *ptr_next;
  first_qword &= mask;
  first_qword |= item;
  next_qword &= mask_next_qword;
  next_qword |= item_next_qword;
  *ptr = first_qword;
  *ptr_next = next_qword;
#if defined DEBUG_MODE
//fprintf( stdout, "[DEBUG] first_qword = [%#018lx]\n", *ptr );
//fprintf( stdout, "[DEBUG] second_qword = [%#018lx]\n", *ptr_next );
#endif
}

//#define _set_bitfield_template \
//  uint64_t mask, item;\
//  uint64_t mask_next_qword, item_next_qword;\
//  uint64_t head_bit_idx = item_idx * item_len_in_bits;\
//  uint32_t byte_idx = head_bit_idx / 8;\
//  uint32_t qword_idx = byte_idx / 8;\
//  uint32_t pos_msb_in_qword = head_bit_idx % 64;\
//\
//  mask = ~0 << (sizeof(mask)*8 - item_len_in_bits);\
//  mask = mask >> pos_msb_in_qword;\
//  mask = ~mask;\
//  mask_next_qword = ~0 << (sizeof(mask)*8 - item_len_in_bits);\
//  mask_next_qword = mask_next_qword << (sizeof(mask)*8 - pos_msb_in_qword);\
//  mask_next_qword = ~mask_next_qword;\
//\
//  item = value << (sizeof(item)*8 - item_len_in_bits);\
//  item = item  >> pos_msb_in_qword;\
//  item_next_qword = value << (sizeof(item)*8 - item_len_in_bits);\
//  item_next_qword = item_next_qword  << (sizeof(item)*8 - pos_msb_in_qword);\
//\
//  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask, sizeof(mask));\
//  to_another_byte_order(0, util_endian_BIG_ENDIAN, &item, sizeof(item));\
//  to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_next_qword, sizeof(mask_next_qword));\
//  to_another_byte_order(0, util_endian_BIG_ENDIAN, &item_next_qword, sizeof(item_next_qword));\
//  typeof(&item) ptr = (typeof(&item))(arr_bits + qword_idx*64/8);\
//  *ptr &= mask;\
//  *ptr |= item;\
//  ptr = (typeof(&item))(arr_bits + qword_idx*64/8 + 64/8);\
//  *ptr &= mask_next_qword;\
//  *ptr |= item_next_qword;

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_u32( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, uint32_t value )
{
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;
  const uint32_t byte_idx = head_bit_idx / 8;
  const uint32_t dword_idx = byte_idx / (32/8);
  const uint32_t pos_msb_in_dword = head_bit_idx % 32;

  uint32_t mask, item;
  uint32_t mask_next_dword, item_next_dword;
  mask = ~(uint32_t)0 << (sizeof(mask)*8 - item_len_in_bits);
  mask = mask >> pos_msb_in_dword;
  mask = ~mask;
  mask_next_dword = ~(uint32_t)0 << (sizeof(mask)*8 - item_len_in_bits);
  mask_next_dword = mask_next_dword << (sizeof(mask)*8 - (pos_msb_in_dword?pos_msb_in_dword:1));
  mask_next_dword = ~mask_next_dword;

  item = value << (sizeof(item)*8 - item_len_in_bits);
  item = item  >> pos_msb_in_dword;
  item_next_dword = value << (sizeof(item)*8 - item_len_in_bits);
  item_next_dword = item_next_dword  << (sizeof(item)*8 - (pos_msb_in_dword?pos_msb_in_dword:1));

  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask, sizeof(mask));
  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &item, sizeof(item));
  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &mask_next_dword, sizeof(mask_next_dword));
  //to_another_byte_order(0, util_endian_BIG_ENDIAN, &item_next_dword, sizeof(item_next_dword));

  const typeof(&item) ptr = (typeof(ptr))arr_bits + dword_idx;
  const typeof(ptr) ptr_next = (typeof(ptr))arr_bits + dword_idx + 1;
  uint32_t first_dword = *ptr;
  uint32_t next_dword = *ptr_next;
  first_dword &= mask;
  first_dword |= item;
  next_dword &= mask_next_dword;
  next_dword |= item_next_dword;
  *ptr = first_dword;
  *ptr_next = next_dword;
#if defined DEBUG_MODE
//fprintf( stdout, "[DEBUG] first_dword = [%#018lx]\n", *ptr );
//fprintf( stdout, "[DEBUG] second_dword = [%#018lx]\n", *ptr_next );
#endif
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_u16( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, uint16_t value )
{
  set_bits_sp_u32(arr_bits, item_idx, item_len_in_bits, value);
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_u8( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, uint8_t value )
{
  set_bits_sp_u32(arr_bits, item_idx, item_len_in_bits, value);
}
//#undef _declare_vars_for_set_bitfield
//#define _declare_vars_for_set_bitfield(len) \
//  uint ## len ## _t mask; int ## len ## _t item;

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_i64( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, int64_t value )
{
  set_bits_sp_u64(arr_bits, item_idx, item_len_in_bits, value);
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_i32( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, int32_t value )
{
  set_bits_sp_u32(arr_bits, item_idx, item_len_in_bits, value);
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_i16( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, int16_t value )
{
  set_bits_sp_u32(arr_bits, item_idx, item_len_in_bits, value);
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline void set_bits_sp_i8( void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits, int8_t value )
{
  set_bits_sp_u32(arr_bits, item_idx, item_len_in_bits, value);
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief set_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization.
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
#define set_bits_sp( arr_bits, item_idx, item_len, value )                            \
  _Pragma ( "GCC diagnostic push" )                                                   \
  _Pragma ( "GCC diagnostic ignored \"-Wconstant-conversion\"" )                      \
  ( (item_len) > 32 ? set_bits_sp_u64((arr_bits), (item_idx), (item_len), (value)) :  \
   set_bits_sp_u32( (arr_bits), (item_idx), (item_len), (value) ) )                   \
  _Pragma ( "GCC diagnostic pop" )
//#define set_bits_sp( arr_bits, item_idx, item_len, value ) _Generic((value), \
//    uint64_t : set_bits_sp_u64,\
//    uint32_t : set_bits_sp_u32,\
//    uint16_t : set_bits_sp_u16,\
//    uint8_t : set_bits_sp_u8,\
//    int64_t : set_bits_sp_i64,\
//    int32_t : set_bits_sp_i32,\
//    int16_t : set_bits_sp_i16,\
//    int8_t : set_bits_sp_i8\
//    )( (arr_bits), (item_idx), (item_len), (value) )
//#undef _declare_vars_for_set_bitfield
//#undef _set_bitfield_template

#define _uintlen_t_item(len) \
  uint ## len ## _t item;
#define _get_bits_unaligned_template                                            \
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;                          \
  const uint32_t byte_idx = head_bit_idx / 8;                                         \
  const uint32_t pos_head_bit_in_head_byte = head_bit_idx % 8;                        \
  switch ( (item_len_in_bits + 8 - 1) / 8 ) {                                   \
    case 1:                                                                     \
      {                                                                         \
        _uintlen_t_item(16)                                                     \
                                                                                \
        item = *(typeof(&item))(arr_bits + byte_idx);                           \
        to_another_byte_order(util_endian_BIG_ENDIAN, 0, &item, sizeof(item));  \
                                                                                \
        item = item << pos_head_bit_in_head_byte;                               \
        item = item >> (sizeof(item)*8 - item_len_in_bits);                     \
                                                                                \
        return item;                                                            \
      }                                                                         \
    case 2:                                                                     \
    case 3:                                                                     \
      {                                                                         \
        _uintlen_t_item(32)                                                     \
                                                                                \
        item = *(typeof(&item))(arr_bits + byte_idx);                           \
        to_another_byte_order(util_endian_BIG_ENDIAN, 0, &item, sizeof(item));  \
                                                                                \
        item = item << pos_head_bit_in_head_byte;                               \
        item = item >> (sizeof(item)*8 - item_len_in_bits);                     \
                                                                                \
        return item;                                                            \
      }                                                                         \
    case 4:                                                                     \
    case 5:                                                                     \
    case 6:                                                                     \
    case 7:                                                                     \
      {                                                                         \
        _uintlen_t_item(64)                                                     \
                                                                                \
        item = *(typeof(&item))(arr_bits + byte_idx);                           \
        to_another_byte_order(util_endian_BIG_ENDIAN, 0, &item, sizeof(item));  \
                                                                                \
        item = item << pos_head_bit_in_head_byte;                               \
        item = item >> (sizeof(item)*8 - item_len_in_bits);                     \
                                                                                \
        return item;                                                            \
      }                                                                         \
  }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint64_t get_bits_unaligned_u64( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits ) { _get_bits_unaligned_template }

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint32_t get_bits_unaligned_u32( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits ) { _get_bits_unaligned_template }

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint16_t get_bits_unaligned_u16( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits ) { _get_bits_unaligned_template }

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint8_t get_bits_unaligned_u8( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits )
{
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;
  const uint32_t byte_idx = head_bit_idx / 8;
  const uint32_t pos_head_bit_in_head_byte = head_bit_idx % 8;
        _uintlen_t_item(8)

        item = *(typeof(&item))(arr_bits + byte_idx);

        item = item << pos_head_bit_in_head_byte;
        item = item >> (sizeof(item)*8 - item_len_in_bits);

        return item;
}
#pragma GCC diagnostic pop

#undef _uintlen_t_item
#undef _get_bits_unaligned_template
#define _for_get_bits_unaligned_signed_template( len ) \
  uint ## len ## _t item; int ## len ## _t item_s;
#define _get_bits_unaligned_signed_template                                     \
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;                          \
  const uint32_t byte_idx = head_bit_idx / 8;                                         \
  const uint32_t pos_head_bit_in_head_byte = head_bit_idx % 8;                        \
  switch ( (item_len_in_bits + 8 - 1) / 8 ) {                                   \
    case 1:                                                                     \
      {                                                                         \
        _for_get_bits_unaligned_signed_template(16)                             \
                                                                                \
        item = *(typeof(&item))(arr_bits + byte_idx);                           \
        to_another_byte_order(util_endian_BIG_ENDIAN, 0, &item, sizeof(item));  \
                                                                                \
        item = item << pos_head_bit_in_head_byte;                               \
        item = item >> (sizeof(item)*8 - item_len_in_bits);                     \
                                                                                \
        item_s = ~0 << item_len_in_bits;                                        \
        item_s |= item;                                                         \
                                                                                \
        return item_s;                                                          \
      }                                                                         \
    case 2:                                                                     \
    case 3:                                                                     \
      {                                                                         \
        _for_get_bits_unaligned_signed_template(32)                             \
                                                                                \
        item = *(typeof(&item))(arr_bits + byte_idx);                           \
        to_another_byte_order(util_endian_BIG_ENDIAN, 0, &item, sizeof(item));  \
                                                                                \
        item = item << pos_head_bit_in_head_byte;                               \
        item = item >> (sizeof(item)*8 - item_len_in_bits);                     \
                                                                                \
        item_s = ~0 << item_len_in_bits;                                        \
        item_s |= item;                                                         \
                                                                                \
        return item_s;                                                          \
      }                                                                         \
    case 4:                                                                     \
    case 5:                                                                     \
    case 6:                                                                     \
    case 7:                                                                     \
      {                                                                         \
        _for_get_bits_unaligned_signed_template(64)                             \
                                                                                \
        item = *(typeof(&item))(arr_bits + byte_idx);                           \
        to_another_byte_order(util_endian_BIG_ENDIAN, 0, &item, sizeof(item));  \
                                                                                \
        item = item << pos_head_bit_in_head_byte;                               \
        item = item >> (sizeof(item)*8 - item_len_in_bits);                     \
                                                                                \
        item_s = ~0 << item_len_in_bits;                                        \
        item_s |= item;                                                         \
                                                                                \
        return item_s;                                                          \
      }                                                                         \
  }
// NOTE item_len_in_bits 64 is not supported.
// NOTE No effective bit-shift would be done when item_len_in_bits%64==0 with the current code,
//  simply adding "case 8:" here would not do it.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline int64_t get_bits_unaligned_i64( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits ) { _get_bits_unaligned_signed_template }

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline int32_t get_bits_unaligned_i32( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits ) { _get_bits_unaligned_signed_template }

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline int16_t get_bits_unaligned_i16( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits ) { _get_bits_unaligned_signed_template }

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline int8_t get_bits_unaligned_i8( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits )
{
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;
  const uint32_t byte_idx = head_bit_idx / 8;
  const uint32_t pos_head_bit_in_head_byte = head_bit_idx % 8;
        _for_get_bits_unaligned_signed_template(8)

        item = *(typeof(&item))(arr_bits + byte_idx);

        item = item << pos_head_bit_in_head_byte;
        item = item >> (sizeof(item)*8 - item_len_in_bits);

        item_s = ~0 << item_len_in_bits;
        item_s |= item;

        return item_s;
}
#pragma GCC diagnostic pop
#undef _for_get_bits_unaligned_signed_template
#undef _get_bits_unaligned_signed_template

//-------------------------------------------------------------------------------------------------
/*
 * \brief get_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline uint64_t get_bits_sp_u64( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits )
{
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;
  const uint32_t byte_idx = head_bit_idx / 8;
  const uint32_t qword_idx = byte_idx / (64/8);
  const uint32_t pos_msb_in_qword = head_bit_idx % 64;
  const uint32_t nr_bits_extracted_from_first_if_counted_from_pos_msb_in_qword_through_the_lsb_of_the_word =
    64 - pos_msb_in_qword;
  const uint32_t nr_bits_extracted_from_next_qword =
    nr_bits_extracted_from_first_if_counted_from_pos_msb_in_qword_through_the_lsb_of_the_word < item_len_in_bits ?
    item_len_in_bits - nr_bits_extracted_from_first_if_counted_from_pos_msb_in_qword_through_the_lsb_of_the_word : 0;
#if defined DEBUG_MODE
//fprintf( stdout, "[DEBUG] qword_idx = %u\n", qword_idx );
//fprintf( stdout, "[DEBUG] pos_msb_in_qword = %u\n", pos_msb_in_qword );
//fprintf( stdout, "[DEBUG] nr_bits_extracted_from_first_if_counted_from_pos_msb_.."
//    " = %u\n", nr_bits_extracted_from_first_if_counted_from_pos_msb_in_qword_through_the_lsb_of_the_word );
#endif

  const uint64_t first_qword = *((uint64_t*)arr_bits + qword_idx);
  const uint64_t second_qword = *((uint64_t*)arr_bits + qword_idx + 1);
  // MOVBE on little-endian systems
  //to_another_byte_order(util_endian_BIG_ENDIAN, 0, &first_qword, sizeof(uint64_t));
  //to_another_byte_order(util_endian_BIG_ENDIAN, 0, &second_qword, sizeof(uint64_t));
#if defined DEBUG_MODE
//fprintf( stdout, "[DEBUG] first_qword = [%#018lx]\n", first_qword );
//fprintf( stdout, "[DEBUG] second_qword = [%#018lx]\n", second_qword );
#endif

  // Prepare the primary qword
  //uint_fast8_t nr_bits_extracted_from_first = item_len_in_bits - nr_bits_extracted_from_next_qword;
  //uint64_t tmp = _bextr_u64( first_qword,
  //    63-pos_msb_in_qword -(nr_bits_extracted_from_first-1),
  //    nr_bits_extracted_from_first );
  //tmp = tmp << nr_bits_extracted_from_next_qword;
  uint32_t tmp = first_qword << pos_msb_in_qword;
  tmp = tmp >> (32 - (item_len_in_bits - nr_bits_extracted_from_next_qword));
  tmp = tmp << nr_bits_extracted_from_next_qword;
#if defined DEBUG_MODE
//fprintf( stdout, "[DEBUG] extracted from the primary = [%#018lx]\n", tmp );
#endif

  // Prepare the optional qword
  uint64_t ret = _bextr_u64( second_qword,
      63-0 -(nr_bits_extracted_from_next_qword-1),
      nr_bits_extracted_from_next_qword );
#if defined DEBUG_MODE
//fprintf( stdout, "[DEBUG] extracted from the secondary = [%#018lx]\n", ret );
#endif

  ret = ret | tmp;
#if defined DEBUG_MODE
//fprintf( stdout, "[DEBUG] ret = [%#018lx]\n", ret );
#endif
  return ret;
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief get_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline uint32_t get_bits_sp_u32( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits )
{
  const uint64_t head_bit_idx = item_idx * item_len_in_bits;
  const uint32_t byte_idx = head_bit_idx / 8;
  const uint32_t dword_idx = byte_idx / (32/8);
  const uint32_t pos_msb_in_dword = head_bit_idx % 32;
  const uint32_t nr_bits_extracted_from_first_if_counted_from_pos_msb_in_dword_through_the_lsb_of_the_word =
    32 - pos_msb_in_dword;
  const uint32_t nr_bits_extracted_from_next_dword =
    nr_bits_extracted_from_first_if_counted_from_pos_msb_in_dword_through_the_lsb_of_the_word < item_len_in_bits ?
    item_len_in_bits - nr_bits_extracted_from_first_if_counted_from_pos_msb_in_dword_through_the_lsb_of_the_word : 0;

  const uint32_t first_dword = *((uint32_t*)arr_bits + dword_idx);
  const uint32_t second_dword = *((uint32_t*)arr_bits + dword_idx + 1);
  // MOVBE on little-endian systems
  //to_another_byte_order(util_endian_BIG_ENDIAN, 0, &first_dword, sizeof(uint32_t));
  //to_another_byte_order(util_endian_BIG_ENDIAN, 0, &second_dword, sizeof(uint32_t));

  // Prepare the primary dword
  //uint_fast8_t nr_bits_extracted_from_first = item_len_in_bits - nr_bits_extracted_from_next_dword;
  //uint32_t tmp = _bextr_u32( first_dword,
  //    31-pos_msb_in_dword -(nr_bits_extracted_from_first-1),
  //    nr_bits_extracted_from_first );
  uint32_t tmp = first_dword << pos_msb_in_dword;
  tmp = tmp >> (32 - (item_len_in_bits - nr_bits_extracted_from_next_dword));
  tmp = tmp << nr_bits_extracted_from_next_dword;

  // Prepare the optional dword
  uint32_t ret = _bextr_u32( second_dword,
      31-0 -(nr_bits_extracted_from_next_dword-1),
      nr_bits_extracted_from_next_dword );

  ret = ret | tmp;
  return ret;
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief get_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline uint16_t get_bits_sp_u16( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits )
{
  return get_bits_sp_u32( arr_bits, item_idx, item_len_in_bits );
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief get_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
__attribute__((always_inline))
static inline uint8_t get_bits_sp_u8( const void * arr_bits,
    uint32_t item_idx, uint8_t item_len_in_bits )
{
  return get_bits_sp_u32( arr_bits, item_idx, item_len_in_bits );
}

//-------------------------------------------------------------------------------------------------
/*
 * \brief get_bits for the insane version (Super Poor version) where bits are squashed
 *  without the order of power of two, for the maximum space utilization..
 * \warning \a arr_bits must have enough space to cover 1 mask after the last word at the end in
 *  order to prevent out-of-bounds hit
 */
#define get_bits_sp(arr_bits, item_idx, item_len_in_bits)                           \
  ( (item_len_in_bits) > 32 ? get_bits_sp_u64((arr_bits), (item_idx), (item_len_in_bits)) : \
   get_bits_sp_u32((arr_bits), (item_idx), (item_len_in_bits)) )



#endif  // UTIL_C_LANG_BITS_H_
