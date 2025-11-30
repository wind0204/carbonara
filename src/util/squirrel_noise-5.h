#if !defined SQUIRREL_5_H_
#define SQUIRREL_5_H_

// From SquirrelNoise5.hpp:
/////////////////////////////////////////////////////////////////////////////////////////////////
// SquirrelNoise5 - Squirrel's Raw Noise utilities (version 5)
//
// This code is made available under the Creative Commons attribution 3.0 license (CC-BY-3.0 US):
//  Attribution in source code comments (even closed-source/commercial code) is sufficient.
//  License summary and text available at: https://creativecommons.org/licenses/by/3.0/us/
//
// These noise functions were written by Squirrel Eiserloh as a cheap and simple substitute for
//  the [sometimes awful] bit-noise sample code functions commonly found on the web, many of which
//  are hugely biased or terribly patterned, e.g. having bits which are on (or off) 75% or even
//  100% of the time (or are excessively overkill/slow for our needs, such as MD5 or SHA).
//
// Note: This is work in progress; not all functions have been tested.  Use at your own risk.
//  Please report any bugs, issues, or bothersome cases to SquirrelEiserloh at gmail.com.
//
// The following functions are all based on a simple bit-noise hash function which returns an
//  unsigned integer containing 32 reasonably-well-scrambled bits, based on a given (signed)
//  integer input parameter (position/index) and [optional] seed.  Kind of like looking up a
//  value in an infinitely large [non-existent] table of previously rolled random numbers.
//
// These functions are deterministic and random-access / order-independent (i.e. state-free),
//  so they are particularly well-suited for use in smoothed/fractal/simplex/Perlin noise
//  functions and out-of-order (or or-demand) procedural content generation (i.e. that mountain
//  village is the same whether you generated it first or last, ahead of time or just now).
//
// The N-dimensional variations simply hash their multidimensional coordinates down to a single
//  32-bit index and then proceed as usual, so while results are not unique they should
//  (hopefully) not seem locally predictable or repetitive.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>



//-------------------------------------------------------------------------------------------------
// Raw pseudorandom noise functions (random-access / deterministic).  Basis of all other noise.
//
__attribute__((always_inline))
inline uint32_t sq5_1d_u32( uint32_t index, uint32_t seed );

__attribute__((always_inline))
inline uint32_t sq5_2d_u32( uint32_t index_x, uint32_t index_y, uint32_t seed );

__attribute__((always_inline))
inline uint32_t sq5_3d_u32( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t seed );

__attribute__((always_inline))
inline uint32_t sq5_4d_u32( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t index_t,
    uint32_t seed );


//-------------------------------------------------------------------------------------------------
// Same functions, mapped to floats in [0,1] for convenience.
//
__attribute__((always_inline))
inline float sq5_1d_0to1( uint32_t index, uint32_t seed );

__attribute__((always_inline))
inline float sq5_2d_0to1( uint32_t index_x, uint32_t index_y, uint32_t seed );

__attribute__((always_inline))
inline float sq5_3d_0to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t seed );

__attribute__((always_inline))
inline float sq5_4d_0to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t index_t,
    uint32_t seed );


//-------------------------------------------------------------------------------------------------
// Same functions, mapped to floats in [-1,1] for convenience.
//
__attribute__((always_inline))
inline float sq5_1d_n1to1( uint32_t index, uint32_t seed );

__attribute__((always_inline))
inline float sq5_2d_n1to1( uint32_t index_x, uint32_t index_y, uint32_t seed );

__attribute__((always_inline))
inline float sq5_3d_n1to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t seed );

__attribute__((always_inline))
inline float sq5_4d_n1to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t index_t,
    uint32_t seed );



// From SquirrelNoise5.hpp:
/////////////////////////////////////////////////////////////////////////////////////////////////
// Inline function definitions below
/////////////////////////////////////////////////////////////////////////////////////////////////
//
//-----------------------------------------------------------------------------------------------
// Fast hash of an int32 into a different (unrecognizable) uint32.
//
// Returns an unsigned integer containing 32 reasonably-well-scrambled bits, based on the hash
//  of a given (signed) integer input parameter (position/index) and [optional] seed.  Kind of
//  like looking up a value in an infinitely large table of previously generated random numbers.
//
// I call this particular approach SquirrelNoise5 (5th iteration of my 1D raw noise function).
//
// Many thanks to Peter Schmidt-Nielsen whose outstanding analysis helped identify a weakness
//  in the SquirrelNoise3 code I originally used in my GDC 2017 talk, "Noise-based RNG".
//  Version 5 avoids a noise repetition found in version 3 at extremely high position values
//  caused by a lack of influence by some of the high input bits onto some of the low output bits.
//
// The revised SquirrelNoise5 function ensures all input bits affect all output bits, and to
//  (for me) a statistically acceptable degree.  I believe the worst-case here is in the amount
//  of influence input position bit #30 has on output noise bit #0 (49.99%, vs. 50% ideal).
//
__attribute__((always_inline))
inline uint32_t sq5_magic( uint32_t index, uint32_t seed )
{
  constexpr uint32_t SQ5_BIT_NOISE1 = 0xd2a80a3f; // 11010010101010000000101000111111
  constexpr uint32_t SQ5_BIT_NOISE2 = 0xa884f197; // 10101000100001001111000110010111
  constexpr uint32_t SQ5_BIT_NOISE3 = 0x6C736F4B; // 01101100011100110110111101001011
  constexpr uint32_t SQ5_BIT_NOISE4 = 0xB79F3ABB; // 10110111100111110011101010111011
  constexpr uint32_t SQ5_BIT_NOISE5 = 0x1b56c4f5; // 00011011010101101100010011110101

  uint32_t mangled_bits = (uint32_t) index;
  mangled_bits *= SQ5_BIT_NOISE1;
  mangled_bits += seed;
  mangled_bits ^= (mangled_bits >> 9);
  mangled_bits += SQ5_BIT_NOISE2;
  mangled_bits ^= (mangled_bits >> 11);
  mangled_bits *= SQ5_BIT_NOISE3;
  mangled_bits ^= (mangled_bits >> 13);
  mangled_bits += SQ5_BIT_NOISE4;
  mangled_bits ^= (mangled_bits >> 15);
  mangled_bits *= SQ5_BIT_NOISE5;
  mangled_bits ^= (mangled_bits >> 17);
  return mangled_bits;
}



//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline uint32_t sq5_1d_u32( uint32_t index, uint32_t seed )
{
  return sq5_magic( index, seed );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline uint32_t sq5_2d_u32( uint32_t index_x, uint32_t index_y, uint32_t seed )
{
  constexpr uint32_t PRIME_NUMBER = 198491317; // Large prime number with non-boring bits
  return sq5_magic( index_x + (PRIME_NUMBER * index_y), seed );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline uint32_t sq5_3d_u32( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t seed )
{
  constexpr uint32_t PRIME1 = 198491317; // Large prime number with non-boring bits
  constexpr uint32_t PRIME2 = 6542989; // Large prime number with distinct and non-boring bits
  return sq5_magic( index_x + (PRIME1 * index_y) + (PRIME2 * index_z), seed );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline uint32_t sq5_4d_u32( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t index_t,
    uint32_t seed )
{
  constexpr uint32_t PRIME1 = 198491317; // Large prime number with non-boring bits
  constexpr uint32_t PRIME2 = 6542989; // Large prime number with distinct and non-boring bits
  constexpr uint32_t PRIME3 = 357239; // Large prime number with distinct and non-boring bits
  return sq5_magic( index_x + (PRIME1 * index_y) + (PRIME2 * index_z) + (PRIME3 * index_t), seed );
}



//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_1d_0to1( uint32_t index, uint32_t seed )
{
  constexpr double ONE_OVER_MAX_UINT = (1.0 / (double) 0xFFFFFFFFu);
  uint32_t mangled_bits = sq5_magic( index, seed );
  return (float)( ONE_OVER_MAX_UINT * (double) mangled_bits );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_2d_0to1( uint32_t index_x, uint32_t index_y, uint32_t seed )
{
  constexpr double ONE_OVER_MAX_UINT = (1.0 / (double) 0xFFFFFFFFu);
  uint32_t mangled_bits = sq5_2d_u32( index_x, index_y, seed );
  return (float)( ONE_OVER_MAX_UINT * (double) mangled_bits );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_3d_0to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t seed )
{
  constexpr double ONE_OVER_MAX_UINT = (1.0 / (double) 0xFFFFFFFFu);
  uint32_t mangled_bits = sq5_3d_u32( index_x, index_y, index_z, seed );
  return (float)( ONE_OVER_MAX_UINT * (double) mangled_bits );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_4d_0to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t index_t,
    uint32_t seed )
{
  constexpr double ONE_OVER_MAX_UINT = (1.0 / (double) 0xFFFFFFFFu);
  uint32_t mangled_bits = sq5_4d_u32( index_x, index_y, index_z, index_t, seed );
  return (float)( ONE_OVER_MAX_UINT * (double) mangled_bits );
}



//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_1d_n1to1( uint32_t index, uint32_t seed )
{
  constexpr double ONE_OVER_MAX_INT = (1.0 / (double) 0x7FFFFFFF);
  return (float)( ONE_OVER_MAX_INT * (double) (int32_t)sq5_magic( index, seed ) );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_2d_n1to1( uint32_t index_x, uint32_t index_y, uint32_t seed )
{
  constexpr double ONE_OVER_MAX_INT = (1.0 / (double) 0x7FFFFFFF);
  return (float)( ONE_OVER_MAX_INT * (double) (int32_t)sq5_2d_u32( index_x, index_y, seed ) );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_3d_n1to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t seed )
{
  constexpr double ONE_OVER_MAX_INT = (1.0 / (double) 0x7FFFFFFF);
  return (float)( ONE_OVER_MAX_INT *
      (double) (int32_t)sq5_3d_u32( index_x, index_y, index_z, seed ) );
}


//-------------------------------------------------------------------------------------------------
__attribute__((always_inline))
inline float sq5_4d_n1to1( uint32_t index_x, uint32_t index_y, uint32_t index_z, uint32_t index_t,
    uint32_t seed )
{
  constexpr double ONE_OVER_MAX_INT = (1.0 / (double) 0x7FFFFFFF);
  return (float)( ONE_OVER_MAX_INT *
      (double) (int32_t)sq5_4d_u32( index_x, index_y, index_z, index_t, seed ) );
}


#endif  // SQUIRREL_5_H_
