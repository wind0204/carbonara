#ifdef _MSC_VER
  #include <intrin.h>
#else
  #include <x86intrin.h>
#endif

#include <emmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>

#include "compute-magic.h"
#include "appdev_support-timer.h"
#include "sorter.h"
#include "squirrel_noise-5.h"
#include "util/util-c_lang-basic.h"
#include "util/util-c_lang-bits.h"
#include "util/util-c_lang-ieee754.h"
#include "util-c_lang-bits2.h"
#include "info_struct.h"
#include "util-c_lang-x86.h"
#include "err_msg.h"
#include "carbonara-math.h"


//-------------------------------------------------------------------------------------------------
static uint32_t compiler_reverse_plz( uint32_t number )
{
  uint32_t ret = 0;

  for ( int i = 0; i < 16; i++ ) {
    ret = ret | ( (number & 1<<i) << (31 - i - i) );
  }
  for ( int i = 16; i < 32; i++ ) {
    ret = ret | ( (number & 1<<i) >> ((i - 16)*2 + 1) );
  }

  return ret;
}

//-------------------------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  fprintf( stdout, "argc=%d argv[0]='%s'\n", argc, argv[0] );
  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = init_util_c_lang();
  init_info_struct(x86_proc_info.whole);

  int argv1 = 64-1;
  if ( argc > 1 ) {
    fprintf( stdout, "[TEST] argv[1] = '%s'\n", argv[1] );
    argv1 = atoi(argv[1]);
  }

  unsigned int test_nr = 1;

  //
  // Binary search tree (sorted array)
  //
  {
    fprintf( stdout, "\n\n\n ## binary search tree (sorted array)\n\n" );
    uint_fast32_t ncount = argv1;
    uint16_t numbers[ncount];
    for ( uint_fast16_t i = 0; i < ncount; i=i+1 ) {
      numbers[i] = i+1;
    }
    numbers[ncount/2] = UINT_MAX_(*numbers);
    for ( uint_fast16_t i = 0; i < ncount; i=i+1 ) {
      fprintf( stdout, "%u ", numbers[i] );
    }
    fprintf( stdout, "\n" );


    struct infos_bst * bst = infos_bst_create( 0, 0, numbers, ncount, 64-1, sizeof(*numbers) );
    infos_bst_print(bst, 3);

    fprintf( stdout, "[TEST] infos_bst_search(bst, 0x4) says the sepd(value) is %u"
        " (%lu means a node with the same key was not found)\n",
        infos_bst_search(bst, (typeof(*numbers))4),
        UINT_MAX_(infos_bst_search_u16(bst, (typeof(*numbers))4)) );
    for ( typeof(ncount) i = 1; i <= ncount; i++ ) {
      typeof(ncount) j = infos_bst_search_u16(bst, i);
      fprintf( stdout, "[TEST] infos_bst_search(bst, %lu) = %lu\n", i, j );
      if ( j != i - 1 ) {
        if ( i == ncount/2 + 1 ) {
          j = infos_bst_search_u16( bst, UINT_MAX_(*numbers) );
          if ( j != i - 1 ) {
            fprintf( stderr, "[TEST]  Error! It has to be %lu instead.\n", i - 1 );
            return test_nr;
          } else {
            fprintf( stdout, "[TEST] infos_bst_search(bst, %lu) = %lu\n", UINT_MAX_(*numbers), j );
          }
        } else {
          fprintf( stderr, "[TEST]  Error! It has to be %lu instead.\n", i - 1 );
          return test_nr;
        }
      }
    }
    infos_bst_print(bst, 3);
    fprintf( stdout, "\nTrying to remove the 8th item from the sorted array...\n" );
    infos_bst_remove(bst, 0x07);
    infos_bst_print(bst, 3);
    fprintf( stdout, "\nTrying to remove the 7th item from the sorted array...(no-op)\n" );
    infos_bst_remove(bst, 0x06);
    infos_bst_print(bst, 3);
    fprintf( stdout, "\nTrying to remove the 16th item from the sorted array...\n" );
    infos_bst_remove(bst, 0x0f);
    infos_bst_print(bst, 3);
    fprintf( stdout, "\nTrying to change the key of the 16th item to %lu...\n",
        UINT_MAX_(*numbers) - 1 );
    infos_bst_change_key_u16( bst, 0x0f, UINT_MAX_(*numbers) - 1 );
    infos_bst_print(bst, 3);
    fprintf( stdout, "\nTrying to remove the 8th item from the sorted array...\n" );
    infos_bst_remove(bst, 0x07);
    infos_bst_print(bst, 3);
    fprintf( stdout, "[TEST] infos_bst_insert(bst, %lu, %u(bit-field overflow)) = "
        "%u (The index of the element in the sorted array)\n",
        ncount+1, 1u<<(bst->height_cap + 1), infos_bst_insert_u16(bst, ncount+1,
          1u<<(bst->height_cap + 1)) );
    infos_bst_print(bst, 3);
    fprintf( stdout, "[TEST] infos_bst_search(bst, %lu) says the sepd(value) is "
        "%u instead of %u\n", ncount+1, infos_bst_search_u16(bst, ncount+1),
        1u<<(bst->height_cap + 1) );
    fprintf( stdout, "[TEST] infos_bst_search(bst, %u) says the sepd(value) is %u\n",
        3u, infos_bst_search_u16(bst, 3) );
    fprintf( stdout, "[TEST] infos_bst_search(bst, %u) says the sepd(value) is %u\n",
        8u, infos_bst_search_u16(bst, 8) );
    fprintf( stdout, "[TEST] infos_bst_search(bst, %lu) says the sepd(value) is %u\n",
        UINT_MAX_(*numbers), infos_bst_search_u16(bst, UINT_MAX_(*numbers)) );

    fprintf( stdout, "[TEST] call infos_bst_insert(bst, new_key) until it refuses.\n"
        "  It should refuse when nr_active_nodes is    %u...\n",
        infos_bst_calc_capacity(bst->height_cap) );
    for ( bst_node_idx_t i = ncount+2; i < 1000; i++ ) {
//if ( _is_power_of_two(bst->nr_active_nodes) || _is_power_of_two(bst->nr_active_nodes + 1) || _is_power_of_two(bst->nr_active_nodes - 1) ) { infos_bst_print( bst, 3 ); }
      auto ret = infos_bst_insert_u16(bst, i, i-1);
      if ( ret == (typeof(ret))~0 ) {
        if ( bst->nr_active_nodes == infos_bst_calc_capacity(bst->height_cap) ) {
          fprintf( stdout, "[TEST] rejected when bst->nr_active_nodes was %u\n",
              bst->nr_active_nodes );
        } else {
          fprintf( stderr, "[TEST] Error detected! rejected when bst->nr_active_nodes was %u. "
              "but it was actually supposed to do so when bst->nr_active_nodes was %u.\n",
              bst->nr_active_nodes, infos_bst_calc_capacity(bst->height_cap) );
          return test_nr;
        }
        break;
      }
    }
    infos_bst_print( bst, 3 );
    typeof(&*numbers) keys = __builtin_assume_aligned( bst->keys, ALIGN_FOR_AVX );
    fprintf( stdout, "[%03x]", keys[0] );
    for ( bst_node_idx_t i = 1; i < infos_bst_calc_capacity(bst->height_cap); i++ ) {
      fprintf( stdout, "[%03x]", keys[i] );
    }
    fprintf( stdout, "\n= %u", infos_bst_search(bst, keys[0]) );
    for ( bst_node_idx_t i = 1; i < infos_bst_calc_capacity(bst->height_cap); i++ ) {
      fprintf( stdout, ", %u", infos_bst_search(bst, keys[i]) );
    }
    infos_bst_destroy(bst, 0, 0);
  }



  test_nr = 2;
  //
  // Reverse a bitfield using a bunch of RCR and RCL
  //
  {
    fprintf( stdout, "\n\n\n ## Reverse a bitfield using a bunch of RCR and RCL\n\n" );
    // _reverse_field(6) (RCR and RCL on x86-64)
    int rf = argc - 2;
    rf += 6;
    fprintf( stdout, "_reverse_field(%d) = %lu (Must be 1610612736 when argc is 2)"
        " (argc was %d)\n", rf, _reverse_field(rf), argc );
  }



  test_nr = 3;
  //
  // vpcmpeqd vmovmskps tzcnt combo
  //
  {
    fprintf( stdout, "\n\n\n ## Combo. of vpcmpeqd vmovmskps tzcnt\n\n" );
    alignas(ALIGN_FOR_AVX) unsigned int array[ALIGN_FOR_AVX / sizeof(unsigned int)] = {0};
    simde__m256i seven = simde_mm256_set1_epi32( 7 );
    simde__m256i m256i;
    unsigned int mask, idx;
    idx = 1;
    array[idx] = 7;
    m256i = simde_mm256_load_si256( (void*)(array) );
    m256i = simde_mm256_cmpeq_epi32( m256i, seven );
    mask = simde_mm256_movemask_ps( (simde__m256) m256i );
    fprintf( stdout, "%u: %u\n", idx, __tzcnt_u32(mask) );
    array[idx] = 0;

    idx = 2;
    array[idx] = 7;
    m256i = simde_mm256_load_si256( (void*)(array) );
    m256i = simde_mm256_cmpeq_epi32( m256i, seven );
    mask = simde_mm256_movemask_ps( (simde__m256) m256i );
    fprintf( stdout, "%u: %u\n", idx, __tzcnt_u32(mask) );
    array[idx] = 0;

    idx = 3;
    array[idx] = 7;
    m256i = simde_mm256_load_si256( (void*)(array) );
    m256i = simde_mm256_cmpeq_epi32( m256i, seven );
    mask = simde_mm256_movemask_ps( (simde__m256) m256i );
    fprintf( stdout, "%u: %u\n", idx, __tzcnt_u32(mask) );
    array[idx] = 0;

    idx = 7;
    array[idx] = 7;
    m256i = simde_mm256_load_si256( (void*)(array) );
    m256i = simde_mm256_cmpeq_epi32( m256i, seven );
    mask = simde_mm256_movemask_ps( (simde__m256) m256i );
    fprintf( stdout, "%u: %u\n", idx, __tzcnt_u32(mask) );
    array[idx] = 0;
  }



  test_nr = 4;
  //
  // sorter
  //
  fprintf( stdout, "\n\n\n ## sorter\n\n" );
  {
    // Shell sort.
    unsigned int arr[] = { 5, 2, 4, 7, 9, 8, 5, 1 };
    for ( unsigned long i = 0; i < _arr_elem_cnt(arr); i++ ) {
      fprintf( stdout, " %u", arr[i] );
    }
    fprintf( stdout, "\n" );

    shell_sort_auto( _arr_elem_cnt(arr), arr );
    for ( unsigned long i = 0; i < _arr_elem_cnt(arr); i++ ) {
      fprintf( stdout, " %u", arr[i] );
    }
    fprintf( stdout, " (Sorted)\n" );
  }

  {
    // Shell sort with separate data bits
    unsigned int len_sepd_in_bits = 5;
    unsigned int arr[] = { 5, 2, 4, 7, 9, 8, 5, 1 };
    char arr_separate_data[
      sizeof(uint64_t)/sizeof(char) +
      _align_to_power_of_two( (len_sepd_in_bits * _arr_elem_cnt(arr) + 8 - 1) / 8, 8 )
    ];
    for ( unsigned long i = 0; i < _arr_elem_cnt(arr); i++ ) {
      set_bits_sp_u32( arr_separate_data, i, len_sepd_in_bits, i+1 );
    }
    for ( unsigned long i = 0; i < _arr_elem_cnt(arr); i++ ) {
      fprintf( stdout, " %u(%u)", arr[i], get_bits_sp_u32(arr_separate_data, i, len_sepd_in_bits) );
    }
    fprintf( stdout, "\n" );

    shell_sort_u32_sepd_bits( _arr_elem_cnt(arr), arr, arr_separate_data, len_sepd_in_bits );
    for ( unsigned long i = 0; i < _arr_elem_cnt(arr); i++ ) {
      fprintf( stdout, " %u(%u)", arr[i], get_bits_sp_u32(arr_separate_data, i, len_sepd_in_bits) );
    }
    fprintf( stdout, " (Sorted)\n" );
  }



  //
  // Hashtable
  //
  fprintf( stdout, "\n\n\n ## hashtable\n\n" );
  struct infos_hashtable * hashtable = infos_hashtable_create(
      0, 0, 0, 64*sizeof(unsigned int), sizeof(unsigned int), 7 );

  test_nr = 5;
  // infos_hashtable_ Insert and search
  {
    unsigned int arr[] = { 5, 2, 4, 7, 9, 8, 5, 1 };
    enum err_msg err;
    for ( unsigned int i = 0; i < _arr_elem_cnt(arr); i++ ) {
      err = infos_hashtable_insert_u32( hashtable, nullptr, nullptr, i+1, arr[i] );
      if ( err ) {
        fprintf( stderr, "Error: %s\n", arr_emsg_txt[err] );
        return test_nr;
      }
    }

    unsigned int * ptr;
    for ( unsigned int i = 1; i <= _arr_elem_cnt(arr); i++ ) {
      ptr = (unsigned int*)infos_hashtable_search(hashtable, i);
      if ( ptr ) fprintf( stdout, "hashtable_search(%u) = %u\n", i, *ptr );
      else fprintf( stdout, "hashtable_search(%u) = _\n", i );
    }
  }

  test_nr = 6;
  // infos_hashtable_ Remove
  {
    enum err_msg err;
    unsigned int * ptr;
    fprintf( stdout, "call infos_hashtable_remove( hashtable, 1 )\n" );
    err = infos_hashtable_remove( hashtable, 1 );
    if ( err ) {
      fprintf( stderr, "Error: %s\n", arr_emsg_txt[err] );
      return test_nr;
    }
    for ( unsigned int i = 1; i <= 8; i++ ) {
      ptr = (unsigned int*)infos_hashtable_search(hashtable, i);
      if ( ptr ) fprintf( stdout, "  %u(%u)", i, *ptr );
      else fprintf( stdout, "  _(_)"  );
    }
    fprintf( stdout, "\n" );

    fprintf( stdout, "Trying to remove the item with key 2 from the hashtable..." );
    err = infos_hashtable_remove( hashtable, 2 );
    if ( err ) {
      fprintf( stderr, "Error: %s\n", arr_emsg_txt[err] );
      return test_nr;
    }
    for ( unsigned int i = 1; i <= 8; i++ ) {
      ptr = (unsigned int*)infos_hashtable_search(hashtable, i);
      if ( ptr ) fprintf( stdout, "  %u(%u)", i, *ptr );
      else fprintf( stdout, "  _(_)"  );
    }
    fprintf( stdout, "\n" );

    err = infos_hashtable_remove( hashtable, 2 );
    fprintf( stdout, "Trying to remove the item with key 2 again... " );
    if ( err ) {
      fprintf( stdout, "expected error: \"%s\"", arr_emsg_txt[err] );
    } else {
      fprintf( stderr, "The expected error \"%s\" didn't show up!",
          arr_emsg_txt[emsg_no_such_entry] );
      return test_nr;
    }
    fprintf( stdout, "\n" );
    for ( unsigned int i = 1; i <= 8; i++ ) {
      ptr = (unsigned int*)infos_hashtable_search(hashtable, i);
      if ( ptr ) fprintf( stdout, "  %u(%u)", i, *ptr );
      else fprintf( stdout, "  _(_)"  );
    }
    fprintf( stdout, "\n" );
  }

  infos_hashtable_destroy( hashtable, 0, 0 );


  test_nr = 7;
  //
  // unsigned_division_magic_..
  //
  {
    fprintf( stdout, "\n\n\n ## unsigned_division_magic_\\d\\d\n\n" );

    struct unsigned_division_magic_64 md_64;
    unsigned_division_magic_64(&md_64, argv1);
    uint64_t result_64 = div_via_mul_64(&md_64, 1000000000);
    uint64_t expected_64 = div_via_mul_64(&md_64, 1000000000);
    fprintf( stdout, "[TEST] div_via_mul_64(md_64, 1000000000) = %lu(%lu was expected)\n",
        result_64, expected_64 );
    for ( unsigned int i = argv1; i < 1000; i++ ) {
      result_64 = div_via_mul_64(&md_64, 1000000000 + i);
      expected_64 = div_via_mul_64(&md_64, 1000000000 + i);
      if ( expected_64 != result_64 ) {
        fprintf( stderr, "[TEST] Error! div_via_mul_64(md_64, 1000000000) "
            "= %lu(%lu was expected)\n", result_64, expected_64 );
        return test_nr;
      }
    }

    struct unsigned_division_magic_32 md_32;
    unsigned_division_magic_32(&md_32, argv1);
    uint32_t result_32 = div_via_mul_32(&md_32, 1000000000);
    uint32_t expected_32 = div_via_mul_32(&md_32, 1000000000);
    fprintf( stdout, "[TEST] div_via_mul_32(md_32, 1000000000) = %u(%u was expected)\n",
        result_32, expected_32 );
    for ( unsigned int i = argv1; i < 1000; i++ ) {
      result_32 = div_via_mul_32(&md_32, 1000000000 + i);
      expected_32 = div_via_mul_32(&md_32, 1000000000 + i);
      if ( expected_32 != result_32 ) {
        fprintf( stdout, "[TEST] div_via_mul_32(md_32, 1000000000) = %u(%u was expected)\n",
            result_32, expected_32 );
      }
    }
  }


  test_nr = 8;
  // Prepare benchmarking
  fprintf( stdout, "\n\n\n ## Prepare to utilize the time-stamp counter\n\n" );
  enum err_msg err;
  err = init_appdev_support_timer(x86_proc_info.whole, 0);
  const __float128 tsc_freq = nr_tsc_in_1s;
  const __float128 timestamp_to_ns = 1.0 / ( (1.0 / 1000000000.0) * tsc_freq );
  if ( err ) {
    fprintf( stderr, "[ADS-timer] err_msg from init_appdev_support_timer = \"%s\"\n",
        arr_emsg_txt[err] );
    switch ( err ) {
      case emsg_not_found:
        fprintf( stderr, "  We could not find /dev/cpu/0/msr\n" );
        break;
      case emsg_permission_denied:
        fprintf( stderr, "  We could not determine the non-turbo frequency of the processor.\n" );
        break;
      default:
        unreachable();
    }
    fprintf( stderr, "Please contact the developer to get support.\n" );

    return test_nr;
  }


  test_nr = 9;
  // Benchmark - _reverse_field
  {
    constexpr int nr_rounds = 32;
    fprintf( stdout, "\n\n\n ## Benchmark - reverse bits\n\n" );
    fprintf( stdout, "The seed given to the pseudo-random number generator: %d\n", argv1 );
    fprintf( stdout, "nr_rounds(each round consists of 640000 turns): %d\n",
        nr_rounds );
    uint64_t nr_tsc_from_rng = 0;
    uint64_t nr_tsc_from_reverse32 = 0;
    uint64_t nr_tsc_from_compilerplz = 0;
    uint64_t nr_tsc_from_compilerplz_nsimd = 0;
    for ( int round = 1; round <= nr_rounds; round++ ) {
      uint64_t bait_for_compiler;
      uint64_t starting_tsc, new_tsc;
      uint32_t numbers[640000];

      // Random numbers
      _mm_mfence();
      _mm_lfence();
      starting_tsc = _rdtsc();
      for ( unsigned int i = 0; i < _arr_elem_cnt(numbers); i++ ) {
        numbers[i] = sq5_magic( i, round+argv1 );
      }
      _mm_mfence();
      _mm_lfence();
      new_tsc = _rdtsc();
      nr_tsc_from_rng += new_tsc - starting_tsc;

      // _reverse_field
      bait_for_compiler = 0;
      _mm_mfence();
      _mm_lfence();
      starting_tsc = _rdtsc();
      for ( unsigned int i = 0; i < 10000; i++ ) {
        for ( unsigned int j = 0; j < 64; j++ ) {
          bait_for_compiler += _reverse_field( numbers[j+i*64] );
        }
      }
      _mm_mfence();
      _mm_lfence();
      new_tsc = _rdtsc();
      nr_tsc_from_reverse32 += new_tsc - starting_tsc;
      if ( bait_for_compiler == 0 ) {
        fprintf( stdout, "_\n" );
      }

      // compiler_reverse_plz
      bait_for_compiler = 0;
      _mm_mfence();
      _mm_lfence();
      starting_tsc = _rdtsc();
      for ( unsigned int i = 0; i < 10000; i++ ) {
        for ( unsigned int j = 0; j < 64; j++ ) {
          bait_for_compiler += compiler_reverse_plz( numbers[j+i*64] );
        }
      }
      _mm_mfence();
      _mm_lfence();
      new_tsc = _rdtsc();
      nr_tsc_from_compilerplz += new_tsc - starting_tsc;
      if ( bait_for_compiler == 0 ) {
        fprintf( stdout, ".\n" );
      }

      // compiler_reverse_plz (not simd)
      bait_for_compiler = 0;
      _mm_mfence();
      _mm_lfence();
      starting_tsc = _rdtsc();
      for ( unsigned int i = 0; i < 10000; i++ ) {
        for ( unsigned int j = 0; j < 64; j++ ) {
          // 2**19 is close to 640000
          bait_for_compiler += compiler_reverse_plz( numbers[bait_for_compiler % (1<<19)] );
        }
      }
      _mm_mfence();
      _mm_lfence();
      new_tsc = _rdtsc();
      nr_tsc_from_compilerplz_nsimd += new_tsc - starting_tsc;
      if ( bait_for_compiler == 0 ) {
        fprintf( stdout, ",\n" );
      }

    }

    // The result of benchmark - _reverse_field
    fprintf( stdout, "time spent in RNG = %.1fns\n"
        "time spent in reverse_32_rcr_rcl(unvectorizable) = %.1fns\n"
        "time spent in compiler_reverse_plz(autovectorized) = %.1fns\n"
        "time spent in compiler_reverse_plz(unsimded) = %.1fns\n"
        "Now we know that repeating RCR and RCL for reversing bits is a bad idea!\n"
        "reverse_32(unvectorizable) / compiler_reverse_plz(unsimded) = %f+\n"
        "compiler_reverse_plz(unsimded) / compiler_reverse_plz(autovectorized) = %f+\n",
        (double)(timestamp_to_ns * nr_tsc_from_rng),
        (double)(timestamp_to_ns * nr_tsc_from_reverse32),
        (double)(timestamp_to_ns * nr_tsc_from_compilerplz),
        (double)(timestamp_to_ns * nr_tsc_from_compilerplz_nsimd),
        (double)nr_tsc_from_reverse32 / (double)nr_tsc_from_compilerplz_nsimd,
        (double)nr_tsc_from_compilerplz_nsimd / (double)nr_tsc_from_compilerplz
        );
  }



  test_nr = 10;
  // (max|min)_without_cmp_(d|f)
  {
    fprintf( stdout, "\n\n\n ## (max|min)_without_cmp_(d|f) \n\n" );
    float fa = 0.5, fb = 2;
    fprintf( stdout, "max_without_cmp_f( 0.5, 2 ) = %f\n", max_without_cmp_f( fa, fb ) );
    fprintf( stdout, "min_without_cmp_f( 0.5, 2 ) = %f\n", min_without_cmp_f( fa, fb ) );
    fprintf( stdout, "max_without_cmp_f( 2, 0.5 ) = %f\n", max_without_cmp_f( fb, fa ) );
    fprintf( stdout, "min_without_cmp_f( 2, 0.5 ) = %f\n", min_without_cmp_f( fb, fa ) );
    double da = 0.5, db = 2;
    fprintf( stdout, "max_without_cmp_d( 0.5, 2 ) = %f\n", max_without_cmp_d( da, db ) );
    fprintf( stdout, "min_without_cmp_d( 0.5, 2 ) = %f\n", min_without_cmp_d( da, db ) );
    fprintf( stdout, "max_without_cmp_d( 2, 0.5 ) = %f\n", max_without_cmp_d( db, da ) );
    fprintf( stdout, "min_without_cmp_d( 2, 0.5 ) = %f\n", min_without_cmp_d( db, da ) );
  }



  test_nr = 11;
  // det_3, get_inv
  {
    fprintf( stdout, "\n\n\n ## det_3 \n\n" );
    struct mat3x3 mat = {
      (vec3){-4, -3, 3},
      (vec3){0, 2, -2},
      (vec3){1, 4, -1}
    };
    fprintf( stdout, "mat = {\n"
        "  {%.01f, %.01f, %.01f},\n"
        "  {%.01f, %.01f, %.01f},\n"
        "  {%.01f, %.01f, %.01f}\n"
        "}\n",
        mat.rows[0][0], mat.rows[0][1], mat.rows[0][2],
        mat.rows[1][0], mat.rows[1][1], mat.rows[1][2],
        mat.rows[2][0], mat.rows[2][1], mat.rows[2][2] );
    float det = det_3( mat );
    fprintf( stdout, "det_3(mat) = %f\n", det );
    struct mat3x3 inv = get_inv( mat );
    fprintf( stdout, "inv = {\n"
        "  {%.04f, %.04f, %.04f},\n"
        "  {%.04f, %.04f, %.04f},\n"
        "  {%.04f, %.04f, %.04f}\n"
        "}\n",
        inv.rows[0][0], inv.rows[0][1], inv.rows[0][2],
        inv.rows[1][0], inv.rows[1][1], inv.rows[1][2],
        inv.rows[2][0], inv.rows[2][1], inv.rows[2][2] );
  }



  test_nr = 12;


  return 0;
}
