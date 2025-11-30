#if !defined UTIL_C_LANG_BITS2_H_
#define UTIL_C_LANG_BITS2_H_


#include <stdint.h>


//-------------------------------------------------------------------------------------------------
static_assert(true);
__attribute__((always_inline))
static inline uint64_t reverse_64( uint64_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 64;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor ecx, ecx\n"              // ECX = 0
      "xor %[ret], %[ret]\n"        // ret = 0
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "inc ecx\n"                   // ECX++
      "cmp ecx, %[loop_cnt]\n"      // ECX - 64
      "jl loop_start_%=\n"
      : [ret] "=&r" (ret), [field] "+r" (field)   // `field` is read first and then overwritten.
                                                  //  `ret` is clobbered early in the asm{}
      : [loop_cnt] "r" (loop_cnt)   // Input operand
      : "cc", "ecx" );  // Clobbers: The condition codes ("cc") are modified by RCR
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

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint32_t reverse_32( uint32_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 32;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor ecx, ecx\n"              // ECX = 0
      "xor %[ret], %[ret]\n"        // ret = 0
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "inc ecx\n"                   // ECX++
      "cmp ecx, %[loop_cnt]\n"      // ECX - 32
      "jl loop_start_%=\n"
      : [ret] "=&r" (ret), [field] "+r" (field)   // `field` is read first and then overwritten.
                                                  //  `ret` is clobbered early in the asm{}
      : [loop_cnt] "r" (loop_cnt)   // Input operand
      : "cc", "ecx" );  // Clobbers: The condition codes ("cc") are modified by RCR
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

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
static inline uint16_t reverse_16( uint16_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 16;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor ecx, ecx\n"              // ECX = 0
      "xor %[ret], %[ret]\n"        // ret = 0
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "inc ecx\n"                   // ECX++
      "cmp ecx, %[loop_cnt]\n"      // ECX - 16
      "jl loop_start_%=\n"
      : [ret] "=&r" (ret), [field] "+r" (field)   // `field` is read first and then overwritten.
                                                  //  `ret` is clobbered early in the asm{}
      : [loop_cnt] "r" (loop_cnt)   // Input operand
      : "cc", "ecx" );  // Clobbers: The condition codes ("cc") are modified by RCR
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
static inline uint8_t reverse_8( uint8_t field )
{
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(field) ret;
  unsigned int loop_cnt = 8;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
  asm (
      ".intel_syntax noprefix\n"
      "xor ecx, ecx\n"              // ECX = 0
      "xor %[ret], %[ret]\n"        // ret = 0
      "loop_start_%=:\n"
      "rcr %[field], 1\n"           // Rotate field right, moving LSB to CF
      "rcl %[ret], 1\n"             // Rotate ret left, moving CF to LSB
      "inc ecx\n"                   // ECX++
      "cmp ecx, %[loop_cnt]\n"      // ECX - 8
      "jl loop_start_%=\n"
      : [ret] "=&r" (ret), [field] "+r" (field)   // `field` is read first and then overwritten.
                                                  //  `ret` is clobbered early in the asm{}
      : [loop_cnt] "r" (loop_cnt)   // Input operand
      : "cc", "ecx" );  // Clobbers: The condition codes ("cc") are modified by RCR
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
#define _reverse_field(field)                                 \
  sizeof(field) == 1 ? reverse_8(field) :                     \
  sizeof(field) == 2 ? reverse_16(field) :                    \
  sizeof(field) == 4 ? reverse_32(field) : reverse_64(field)


#endif  // UTIL_C_LANG_BITS2_H_
