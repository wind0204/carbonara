#if !defined UTIL_C_LANG_BITS2_H_
#define UTIL_C_LANG_BITS2_H_


#include <stdint.h>


/**
 * \brief Slow and inefficient compared to the simple SH.-OR version and vectorized versions
 * \warning WARNING Do not use this inefficient function
 */
__attribute__((always_inline))
static inline uint64_t reverse_rcr_rcl_64( uint64_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 64;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor %[ret], %[ret]\n"        // ret = 0 (also clears CF)
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "dec %[i]\n"           // Decrement loop counter
      "jnz loop_start_%=\n"         // Jump if not zero
      : [ret] "=&r" (ret), [field] "+r" (field), [i] "+r" (loop_cnt)  // `field` is read first and\
                                                                      // then overwritten.\
                                                                      // `ret` is clobbered early\
                                                                      // in the asm{}
      :  // No input-only operands
      : "cc" );  // Clobbers: The condition codes ("cc") are modified by RCR
#pragma GCC diagnostic pop
  return ret;
#elif defined(TARGETING_ARM_ARCHITECTURE)
  #error Utilize RBIT here unless the compiler understands your intent and writes RBIT for the \
  code below.
#else
  typeof(field) ret = 0;
  for ( uint_fast8_t i = 0; i < sizeof(field)*8; i++ ) {
    ret = ret << 1;
    ret |= (field & 1);
    field = field >> 1;
  }
  return ret;
#endif
}

/**
 * \brief Slow and inefficient compared to the simple SH.-OR version and vectorized versions
 * \warning WARNING Do not use this inefficient function
 */
__attribute__((always_inline))
static inline uint32_t reverse_rcr_rcl_32( uint32_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 32;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor %[ret], %[ret]\n"        // ret = 0 (also clears CF)
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "dec %[i]\n"           // Decrement loop counter
      "jnz loop_start_%=\n"         // Jump if not zero
      : [ret] "=&r" (ret), [field] "+r" (field), [i] "+r" (loop_cnt)  // `field` is read first and\
                                                                      // then overwritten.\
                                                                      // `ret` is clobbered early\
                                                                      // in the asm{}
      :  // No input-only operands
      : "cc" );  // Clobbers: The condition codes ("cc") are modified by RCR
#pragma GCC diagnostic pop
  return ret;
#elif defined(TARGETING_ARM_ARCHITECTURE)
  #error Utilize RBIT here unless the compiler understands your intent and writes RBIT for the \
  code below.
#else
  typeof(field) ret = 0;
  for ( uint_fast8_t i = 0; i < sizeof(field)*8; i++ ) {
    ret = ret << 1;
    ret |= (field & 1);
    field = field >> 1;
  }
  return ret;
#endif
}

/**
 * \brief Slow and inefficient compared to the simple SH.-OR version and vectorized versions
 * \warning WARNING Do not use this inefficient function
 */
__attribute__((always_inline))
static inline uint16_t reverse_rcr_rcl_16( uint16_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 16;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor %[ret], %[ret]\n"        // ret = 0 (also clears CF)
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "dec %[i]\n"           // Decrement loop counter
      "jnz loop_start_%=\n"         // Jump if not zero
      : [ret] "=&r" (ret), [field] "+r" (field), [i] "+r" (loop_cnt)  // `field` is read first and\
                                                                      // then overwritten.\
                                                                      // `ret` is clobbered early\
                                                                      // in the asm{}
      :  // No input-only operands
      : "cc" );  // Clobbers: The condition codes ("cc") are modified by RCR
#pragma GCC diagnostic pop
  return ret;
#elif defined(TARGETING_ARM_ARCHITECTURE)
  #error Utilize RBIT unless the compiler understands your intent and writes RBIT for the code \
  below.
#else
  typeof(field) ret = 0;
  for ( uint_fast8_t i = 0; i < sizeof(field)*8; i++ ) {
    ret = ret << 1;
    ret |= (field & 1);
    field = field >> 1;
  }
  return ret;
#endif
}

/**
 * \brief Slow and inefficient compared to the simple SH.-OR version and vectorized versions
 * \warning WARNING Do not use this inefficient function
 */
__attribute__((always_inline))
static inline uint8_t reverse_rcr_rcl_8( uint8_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 8;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor %[ret], %[ret]\n"        // ret = 0 (also clears CF)
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "dec %[i]\n"           // Decrement loop counter
      "jnz loop_start_%=\n"         // Jump if not zero
      : [ret] "=&r" (ret), [field] "+r" (field), [i] "+r" (loop_cnt)  // `field` is read first and\
                                                                      // then overwritten.\
                                                                      // `ret` is clobbered early\
                                                                      // in the asm{}
      :  // No input-only operands
      : "cc" );  // Clobbers: The condition codes ("cc") are modified by RCR
#pragma GCC diagnostic pop
  return ret;
#elif defined(TARGETING_ARM_ARCHITECTURE)
  #error Utilize RBIT unless the compiler understands your intent and writes RBIT for the code \
  below.
#else
  typeof(field) ret = 0;
  for ( uint_fast8_t i = 0; i < sizeof(field)*8; i++ ) {
    ret = ret << 1;
    ret |= (field & 1);
    field = field >> 1;
  }
  return ret;
#endif
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint8_t reverse_u8( uint8_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

__attribute__((always_inline))
static inline uint16_t reverse_u16( uint16_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

__attribute__((always_inline))
static inline uint32_t reverse_u32( uint32_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

__attribute__((always_inline))
static inline uint64_t reverse_u64( uint64_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline int8_t reverse_i8( int8_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

__attribute__((always_inline))
static inline int16_t reverse_i16( int16_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

__attribute__((always_inline))
static inline int32_t reverse_i32( int32_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

__attribute__((always_inline))
static inline int64_t reverse_i64( int64_t field )
{
  typeof(field) ret = 0;

  constexpr unsigned int nr_bits = 8 * sizeof(field);
  for ( unsigned int i = 0; i < nr_bits/2; i++ ) {
    ret = ret | ( (field & 1<<i) << (31 - i - i) );
  }
  for ( unsigned int i = nr_bits/2; i < nr_bits; i++ ) {
    ret = ret | ( (field & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

/**
 * \brief Reverse the bits in a variable
 */
#define reverse(field) _Generic((field), \
    uint64_t : reverse_u64,              \
    uint32_t : reverse_u32,              \
    uint16_t : reverse_u16,              \
    uint8_t  : reverse_u8,               \
    int64_t  : reverse_i64,              \
    int32_t  : reverse_i32,              \
    int16_t  : reverse_i16,              \
    int8_t   : reverse_i8                \
    )( field )


#endif  // UTIL_C_LANG_BITS2_H_
