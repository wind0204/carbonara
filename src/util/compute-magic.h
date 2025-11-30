#if !defined COMPUTE_MAGIC_H_
#define COMPUTE_MAGIC_H_


//-------------------------------------------------------------------------------------------------
// NOTE Signed division is not implemented yet because there hasn't been any
//  repeated signed division by a same divisor in the codebase.
//-------------------------------------------------------------------------------------------------


#include <stdint.h>

#include "util-c_lang-basic.h"


//-------------------------------------------------------------------------------------------------
struct unsigned_division_magic_64 {
  uint64_t multiply;
  uint32_t shift_minus1;
  uint32_t add;
};

struct unsigned_division_magic_32 {
  uint32_t multiply;
  uint16_t shift_minus1;
  uint16_t add;
};


//-------------------------------------------------------------------------------------------------
/*
 * Copied from Hacker's Delight by Henry S. Warren, Jr. (Figure 10-2)
 */
__attribute__((always_inline)) [[maybe_unused]]
static void unsigned_division_magic_64(struct unsigned_division_magic_64 * mag, uint64_t divisor)
{
  assume( 1 <= divisor && divisor <= UINT64_MAX );
  uint32_t p, add;
  uint64_t nc, delta, q1, r1, q2, r2;

  add = 0;
  nc = ~0 - (-divisor)%divisor;
  p = 64 - 1;
  q1 = (1ull << p) / nc;
  r1 = (1ull << p) - q1*nc;
  q2 = ((1ull << p) - 1) / divisor;
  r2 = ((1ull << p) - 1) - q2*divisor;
  do {
    p = p + 1;
    if ( r1 >= nc - r1 ) {
      q1 = 2 * q1 + 1;
      r1 = 2 * r1 - nc;
    } else {
      q1 = 2 * q1;
      r1 = 2 * r1;
    }
    if ( r2 + 1 >= divisor - r2 ) {
      if ( q2 >= (1ull << (64-1)) - 1 )
        add = ~0;
      q2 = 2 * q2 + 1;
      r2 = 2 * r2 + 1 - divisor;
    } else {
      if ( q2 >= 1ull << (64-1) )
        add = ~0;
      q2 = 2 * q2;
      r2 = 2 * r2 + 1;
    }
    delta = divisor - 1 - r2;
  } while ( p < 64*2 &&
      (q1 < delta || (q1 == delta && r1 == 0)) );

  mag->add = add;
  mag->multiply = q2 + 1;
  mag->shift_minus1 = p - 64 - 1;
}

//-------------------------------------------------------------------------------------------------
/*
 * Copied from Hacker's Delight by Henry S. Warren, Jr. (Figure 10-2)
 */
__attribute__((always_inline)) [[maybe_unused]]
static void unsigned_division_magic_32(struct unsigned_division_magic_32 * mag, uint32_t divisor)
{
  assume( 1 <= divisor && divisor <= UINT32_MAX );
  uint32_t p, add;
  uint32_t nc, delta, q1, r1, q2, r2;

  add = 0;
  nc = ~0 - (-divisor)%divisor;
  p = 32 - 1;
  q1 = (1ull << p) / nc;
  r1 = (1ull << p) - q1*nc;
  q2 = ((1ull << p) - 1) / divisor;
  r2 = ((1ull << p) - 1) - q2*divisor;
  do {
    p = p + 1;
    if ( r1 >= nc - r1 ) {
      q1 = 2 * q1 + 1;
      r1 = 2 * r1 - nc;
    } else {
      q1 = 2 * q1;
      r1 = 2 * r1;
    }
    if ( r2 + 1 >= divisor - r2 ) {
      if ( q2 >= (1ull << (32-1)) - 1 )
        add = ~0;
      q2 = 2 * q2 + 1;
      r2 = 2 * r2 + 1 - divisor;
    } else {
      if ( q2 >= 1ull << (32-1) )
        add = ~0;
      q2 = 2 * q2;
      r2 = 2 * r2 + 1;
    }
    delta = divisor - 1 - r2;
  } while ( p < 32*2 &&
      (q1 < delta || (q1 == delta && r1 == 0)) );

  mag->add = add;
  mag->multiply = q2 + 1;
  mag->shift_minus1 = p - 32 - 1;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline)) [[maybe_unused]]
static uint64_t div_via_mul_64(struct unsigned_division_magic_64 * mag, uint64_t n)
{
  typeof(n) q;

  // We need to obtain `n * mag->multiply / 2**[nr_bits]`
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  asm (
      ".intel_syntax noprefix\n"
      "mov rax, %[n]\n"   // RAX = n
      "mul %2\n"          // RDX:RAX = n * mag->multiply
      //"mov %0, rdx\n"   // Store the most significant 64 bits in `q`
      : "=d" (q)          // Output: The result is not read but only overwritten (=).
      : [n] "r" (n), "r" (mag->multiply)  // Input
      : "cc", "rax" );    // Clobbers: The condition codes ("cc") are modified by MUL
#else
  #error Does this platform support RCR and SHR?
#endif

  // Add n to q, and shift right with the carry from the add if mag->add is not 0.
  // I guess using `__builtin_add_overflow` and & and | instead of writing inline assembly could
  //  get the C compiler to understand the desired assembly (RCR and SHR after ADD)
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(q) a = n & ( (uint64_t)mag->add | (uint64_t)(mag->add)<<32 );
  asm (
      ".intel_syntax noprefix\n"
      "add %[q], %[a]\n"  // q + a
      "rcr %[q], 1\n"     // Rotate right through carry the operand in `%0` by 1 bit.
      "shr %[q], %b2\n"   // Shift right by `mag->shift - 1` bits. %b means the lower byte
      : [q] "+r" (q)      // Output: The result is read and then overwritten
      : [a] "r" (a), "c" (mag->shift_minus1)  // Input
      : "cc" );           // Clobbers: The condition codes ("cc") are modified by RCR
#else
  #error Does this platform support RCR and SHR?
  // NOTE if mag->add=0 just do a shift right of mag->shift
  //  else(add!=0) and "shrx"(=rcr+shr) is not available—Would we support an ISA other than x86-64?
  //typeof(n) t = n - q;
  //t = t >> 1;  // (n-q)/2
  //t = t + q;  // (n-q)/2 + q = (n+q)/2
  //q = t >> (mag->shift_minus1);  // ( n+Mn/2**[nr_bits] ) / ( 2*(2**(mag->shift_minus1)) ) =
  //                               //  floor(n/divisor)
#endif

  return q;
}

//-------------------------------------------------------------------------------------------------
__attribute__((always_inline)) [[maybe_unused]]
static uint32_t div_via_mul_32(struct unsigned_division_magic_32 * mag, uint32_t n)
{
  typeof(n) q;

  // We need to obtain `n * mag->multiply / 2**[nr_bits]`
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  asm (
      ".intel_syntax noprefix\n"
      "mov eax, %[n]\n"   // EAX = n
      "mul %2\n"          // EDX:EAX = n * mag->multiply
      //"mov %0, edx\n"   // Store the most significant 64 bits in `q`
      : "=d" (q)          // Output: The result is not read but only overwritten (=).
      : [n] "r" (n), "r" (mag->multiply)  // Input
      : "cc", "eax" );    // Clobbers: The condition codes ("cc") are modified by MUL
#else
  #error Does this platform support RCR and SHR?
#endif

  // Add n to q and shift right with the carry from the add.
  // I guess using `__builtin_add_overflow` and & and | instead of writing inline assembly could
  //  get the C compiler to understand the desired assembly (RCR and SHR after ADD)
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64) \
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  typeof(q) a = mag->add & n;
  asm (
      ".intel_syntax noprefix\n"
      "add %[q], %[a]\n"  // q + a
      "rcr %[q], 1\n"     // Rotate right through carry the operand in `%0` by 1 bit.
      "shr %[q], %b2\n"   // Shift right by `mag->shift - 1` bits. %b means the lower byte
      : [q] "+r" (q)      // Output: The result is read and then overwritten
      : [a] "r" (a), "c" (mag->shift_minus1)  // Input
      : "cc" );           // Clobbers: The condition codes ("cc") are modified by RCR
#else
  #error Does this platform support RCR and SHR?
  // NOTE if mag->add=0 just do a shift right of mag->shift
  //  if add!=0 and "shrx"(=rcr+shr) is not available—Would we support an ISA other than x86-64?
  //typeof(n) t = n - q;
  //t = t >> 1;  // (n-q)/2
  //t = t + q;  // (n-q)/2 + q = (n+q)/2
  //q = t >> (mag->shift_minus1);  // ( n+Mn/2**[nr_bits] ) / ( 2*(2**(mag->shift_minus1)) ) =
  //                               //  floor(n/divisor)
#endif

  return q;
}


#pragma GCC diagnostic pop


#endif  // #if !defined COMPUTE_MAGIC_H_
