#if !defined UTIL_C_LANG_X86_H_
#define UTIL_C_LANG_X86_H_


#include <stdint.h>


//-------------------------------------------------------------------------------------------------
union ucl_x86_proc_info {
  struct {
    // byte 0
    uint8_t maxleaf: 6;
    uint8_t from_amd: 1;
    uint8_t popcnt: 1;

    // byte 1:2
    uint16_t family;

    // byte 3
    uint8_t model;

    // byte 4
    uint8_t bmi1: 1;
    uint8_t cx8: 1;
    uint8_t tsc: 1;
    uint8_t msr: 1;
    uint8_t rdtscp: 1;
    uint8_t perftsc: 1;
    uint8_t avx: 1;
    uint8_t sse42: 1;

    // byte 5
    uint8_t tsc_invariant: 1;
    uint8_t large_page_size_class: 4;
    uint8_t reserved_bits_for_future: 3;

    // byte 6
    uint8_t bmi2: 1;
    uint8_t avx2: 1;
  };
  uint64_t whole;
};

//-------------------------------------------------------------------------------------------------
/**
 * \brief Use this instead of the official `__cpuid` when you pass -masm=intel to clang-20
 */
#define intel_dialect_cpuid(__leaf, __eax, __ebx, __ecx, __edx)           \
  _Pragma ( "GCC diagnostic push" )                                       \
  _Pragma ( "GCC diagnostic ignored \"-Wlanguage-extension-token\"" )     \
  asm(                                                                    \
     ".intel_syntax noprefix\n"                                           \
     "xchg  %q1,rbx\n"                                                    \
     "cpuid\n"                                                            \
     "xchg  %q1,rbx"                                                      \
     : "=a"(__eax), "=r" (__ebx), "=c"(__ecx), "=d"(__edx)                \
     : "0"(__leaf))                                                       \
  _Pragma ( "GCC diagnostic pop" )


#endif  // UTIL_C_LANG_X86_H_
