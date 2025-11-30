#include <math.h>
#include <stdint.h>

#include "util-c_lang-bits.h"
#include "sorter.h"

#ifdef DEBUG_MODE
//#include <stdio.h>
#endif


//-------------------------------------------------------------------------------------------------
static uint16_t prepare_tokuda_seq( uint32_t nr_items, uint8_t item_len_in_bytes, void * array );
static uint16_t emprically_best_seq( uint32_t nr_items, uint8_t item_len_in_bytes, void * array );

//-------------------------------------------------------------------------------------------------
[[maybe_unused]]
static uint16_t prepare_tokuda_seq( uint32_t nr_items, uint8_t item_len_in_bytes, void * array)
{
  const uint8_t nr_elems_in_seq = item_len_in_bytes<=4 ? 27 : 54;  // Prevent overflow

  uint16_t last_exp_tokuda = 0;
  float last_power_of_the_tokuda_value = 1.0f;
  if ( item_len_in_bytes == 4 ) {
    uint32_t * tokuda_sequence = array;
    tokuda_sequence[0] = 1;
  } else {
    uint64_t * tokuda_sequence = array;
    tokuda_sequence[0] = 1;
  }

  uint_fast16_t outer_idx;
  while(-7) {

    outer_idx = last_exp_tokuda + 1;

    constexpr uint8_t set_size = 8;
    float new_gaps[set_size];
    for ( uint_fast8_t i = 0; i < set_size; i++ ) {
      last_power_of_the_tokuda_value = 9.0f/4.0f * last_power_of_the_tokuda_value;
      // Wrote a long expression but actually each gap is just: ( (9/4)**k - 1 )/( 9/4 - 1 )
      new_gaps[i] = 1.0f/5.0f * ( 9.0f * last_power_of_the_tokuda_value - 4.0f );
      if ( outer_idx + i >= nr_elems_in_seq ) break;
    }
    last_exp_tokuda += set_size;

    for ( uint_fast8_t i = 0; i < set_size; i++ ) {
      if ( outer_idx + i >= nr_elems_in_seq ) {
        outer_idx = outer_idx + i - 1;
        return outer_idx;
      }

      uint32_t max_nr_items_for_shell_sort;
      if ( item_len_in_bytes == 4 ) {
        uint32_t * tokuda_sequence = array;
        typeof(*new_gaps) integer = ceilf(new_gaps[i]);
        tokuda_sequence[outer_idx + i] = (typeof(*tokuda_sequence)) integer;
        max_nr_items_for_shell_sort = tokuda_sequence[outer_idx + i] * 3u;
      } else {
        uint64_t * tokuda_sequence = array;
        typeof(*new_gaps) integer = ceilf(new_gaps[i]);
        tokuda_sequence[outer_idx + i] = (typeof(*tokuda_sequence)) integer;
        max_nr_items_for_shell_sort = tokuda_sequence[outer_idx + i] * 3u;
      }

      if ( max_nr_items_for_shell_sort >= nr_items ) {
        outer_idx = outer_idx + i;
        return outer_idx;
      }
    }

  }

}

//-------------------------------------------------------------------------------------------------
static uint16_t emprically_best_seq( uint32_t nr_items, uint8_t item_len_in_bytes, void * array)
{
  // Copied from Empirically Improved Tokuda Gap Sequence In Shellsort by Ying Wai Lee
  constexpr double best_gamma = 2.243609061420001;

  const uint8_t nr_elems_in_seq = item_len_in_bytes<=4 ? 27 : 54;  // Prevent overflow
  uint16_t last_exp = 0;
  float last_power_of_gamma = 1.0f;
  if ( item_len_in_bytes == 4 ) {
    uint32_t * the_sequence = array;
    the_sequence[0] = 1;
  } else {
    uint64_t * the_sequence = array;
    the_sequence[0] = 1;
  }

  uint_fast16_t outer_idx;
  while(-7) {

    outer_idx = last_exp + 1;

    constexpr uint8_t set_size = 8;
    float new_gaps[set_size];
    for ( uint_fast8_t idx = 0; idx < set_size; idx++ ) {
      last_power_of_gamma = (float)best_gamma * last_power_of_gamma;
      new_gaps[idx] = (last_power_of_gamma - 1) / ((float)best_gamma - 1);
      if ( outer_idx + idx >= nr_elems_in_seq ) break;
    }
    last_exp += set_size;

    for ( uint_fast8_t idx = 0; idx < set_size; idx++ ) {
      if ( outer_idx + idx >= nr_elems_in_seq ) {
        outer_idx = outer_idx + idx - 1;
        return outer_idx;
      }

      uint32_t max_nr_items_for_shell_sort;
      if ( item_len_in_bytes == 4 ) {
        uint32_t * the_sequence = array;
        typeof(*new_gaps) integer = ceilf(new_gaps[idx]);
        the_sequence[outer_idx + idx] = (typeof(*the_sequence)) integer;
        max_nr_items_for_shell_sort = the_sequence[outer_idx + idx] * 3u;
      } else {
        uint64_t * the_sequence = array;
        typeof(*new_gaps) integer = ceilf(new_gaps[idx]);
        the_sequence[outer_idx + idx] = (typeof(*the_sequence)) integer;
        max_nr_items_for_shell_sort = the_sequence[outer_idx + idx] * 3u;
      }

      if ( max_nr_items_for_shell_sort >= nr_items ) {
        outer_idx = outer_idx + idx;
        return outer_idx;
      }
    }

  }

}


//-------------------------------------------------------------------------------------------------
void shell_sort_u8( uint32_t nr_items, void * data )
{
  uint8_t * restrict const arr = data;

  uint32_t gap;  // You can also use uint64_t for huge numbers of items
  const uint8_t nr_elems_in_seq = sizeof(gap)<=4 ? 27 : 54;  // Prevent the values from overflowing
  typeof(gap) the_sequence[nr_elems_in_seq];

  uint_fast16_t seq_idx = emprically_best_seq(nr_items, sizeof(gap), the_sequence);

  while(-7) {
    gap = the_sequence[seq_idx];

    // Do gapped insertion sorts
    for ( typeof(*the_sequence) i = gap; i < nr_items; i++ ) {
      const typeof(*arr) tmp = arr[i];

      typeof(i) j = i;
      while(-7) {
        if ( arr[j - gap] <= tmp ) {
          arr[j] = tmp;
          break;
        }

        arr[j] = arr[j - gap];
        if ( j < gap * 2 ) {
          arr[j - gap] = tmp;
          break;
        }
        j -= gap;
      }

    }

    if ( seq_idx == 0 ) {
      break;
    }

    seq_idx--;
  }

}

//-------------------------------------------------------------------------------------------------
void shell_sort_u16( uint32_t nr_items, void * data )
{
  uint16_t * restrict const arr = data;
#define sort_shell_template                                                         \
  uint32_t gap;                                                                     \
  const uint8_t nr_elems_in_seq = sizeof(gap)<=4 ? 27 : 54;                         \
  typeof(gap) the_sequence[nr_elems_in_seq];                                        \
                                                                                    \
  uint_fast16_t seq_idx = emprically_best_seq(nr_items, sizeof(gap), the_sequence); \
                                                                                    \
  while(-7) {                                                                       \
    gap = the_sequence[seq_idx];                                                    \
                                                                                    \
    for ( typeof(*the_sequence) i = gap; i < nr_items; i++ ) {                      \
      const typeof(*arr) tmp = arr[i];                                              \
                                                                                    \
      typeof(i) j = i;                                                              \
      while(-7) {                                                                   \
        if ( arr[j - gap] <= tmp ) {                                                \
          arr[j] = tmp;                                                             \
          break;                                                                    \
        }                                                                           \
                                                                                    \
        arr[j] = arr[j - gap];                                                      \
        if ( j < gap * 2 ) {                                                        \
          arr[j - gap] = tmp;                                                       \
          break;                                                                    \
        }                                                                           \
        j -= gap;                                                                   \
      }                                                                             \
                                                                                    \
    }                                                                               \
                                                                                    \
    if ( seq_idx == 0 ) {                                                           \
      break;                                                                        \
    }                                                                               \
                                                                                    \
    seq_idx--;                                                                      \
  }
  sort_shell_template
}

//-------------------------------------------------------------------------------------------------
void shell_sort_u32( uint32_t nr_items, void * data )
{
  uint32_t * restrict const arr = data;
  sort_shell_template
}

//-------------------------------------------------------------------------------------------------
void shell_sort_u64( uint32_t nr_items, void * data )
{
  uint64_t * restrict const arr = data;
  sort_shell_template
}
#undef sort_shell_template


//uint8_t sort_shell_aligned_u64_u64_sepd_bits(uint32_t nr_items,
//    void * data, void * sep_data, uint8_t len_sepd_in_bits) {
//  return sort_shell_aligned_u64_u64_sepd_bits_impl( nr_items, data, sep_data, len_sepd_in_bits);
//}

//-------------------------------------------------------------------------------------------------
//__attribute__ ((target ("sse4.2")))
void shell_sort_u64_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  //uint64_t * restrict const arr = __builtin_assume_aligned(data, 128u/8u);
  uint64_t * restrict const arr = data;

  uint32_t gap;  // You can also use uint64_t for huge numbers of items
  static_assert(sizeof(gap) <= sizeof(nr_items), "There is a chance that it overflows!");
  const uint8_t nr_elems_in_seq = sizeof(gap)<=4 ? 27 : 54;  // Prevent the values from overflowing
  typeof(gap) the_sequence[nr_elems_in_seq];

  uint_fast16_t seq_idx = emprically_best_seq(nr_items, sizeof(gap), the_sequence);

  while(-7) {
    gap = the_sequence[seq_idx];

    // Do gapped insertion sorts
    for ( typeof(gap) i = gap; i < nr_items; i++ ) {
      const typeof(*arr) tmp = arr[i];
      const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);

      typeof(i) j = i;
      while(-7) {
        if ( arr[j - gap] <= tmp ) {
          arr[j] = tmp;
          set_bits_sp(sep_data, j, len_sepd_in_bits, tmp_sepd);
          break;
        }

        arr[j] = arr[j - gap];
        typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - gap, len_sepd_in_bits);
        set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);
        if ( j < gap * 2 ) {
          arr[j - gap] = tmp;
          set_bits_sp(sep_data, j - gap, len_sepd_in_bits, tmp_sepd);
          break;
        }
        j -= gap;
      }

    }

    if ( seq_idx == 0 ) {
      break;
    }

    seq_idx--;
  }

}

//__attribute__ ((target ("avx")))
//void sort_shell_p256_u64_u64_sepd_bits(uint32_t nr_items,
//    void * restrict data, void * restrict sep_data, uint8_t len_sepd_in_bits) {
  //uint64_t * restrict arr = __builtin_assume_aligned(data, 256u/8u);

//-------------------------------------------------------------------------------------------------
void shell_sort_u32_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  uint32_t * restrict const arr = data;

  uint32_t gap;  // You can also use uint64_t for huge numbers of items
  static_assert(sizeof(gap) <= sizeof(nr_items), "There is a chance that it overflows!");
  const uint8_t nr_elems_in_seq = sizeof(gap)<=4 ? 27 : 54;  // Prevent the values from overflowing
  typeof(gap) the_sequence[nr_elems_in_seq];

  uint_fast16_t seq_idx = emprically_best_seq(nr_items, sizeof(gap), the_sequence);

  while(-7) {
    gap = the_sequence[seq_idx];

    // Do gapped insertion sorts
    for ( typeof(gap) i = gap; i < nr_items; i++ ) {
      const typeof(*arr) tmp = arr[i];
      const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);

      typeof(i) j = i;
      while(-7) {
        if ( arr[j - gap] <= tmp ) {
          arr[j] = tmp;
          set_bits_sp(sep_data, j, len_sepd_in_bits, tmp_sepd);
          break;
        }

        arr[j] = arr[j - gap];
        typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - gap, len_sepd_in_bits);
        set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);
        if ( j < gap * 2 ) {
          arr[j - gap] = tmp;
          set_bits_sp(sep_data, j - gap, len_sepd_in_bits, tmp_sepd);
          break;
        }
        j -= gap;
      }

    }

    if ( seq_idx == 0 ) {
      break;
    }

    seq_idx--;
  }

}

//__attribute__ ((target ("sse4.2")))
//void sort_shell_p128_u32_u32_sepd_bits(uint32_t nr_items,
//    void * restrict data, void * restrict sep_data, uint8_t len_sepd_in_bits) {
  //uint32_t * restrict arr = __builtin_assume_aligned(data, 128u/8u);

//-------------------------------------------------------------------------------------------------
//__attribute__ ((target ("sse4.2")))
void shell_sort_u16_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  //uint16_t * restrict arr = __builtin_assume_aligned(data, 128u/8u);
  uint16_t * restrict const arr = data;

  uint32_t gap;  // You can also use uint64_t for huge numbers of items
  static_assert(sizeof(gap) <= sizeof(nr_items), "There is a chance that it overflows!");
  const uint_fast8_t nr_elems_in_seq = sizeof(gap)<=4 ? 27 : 54;
  typeof(gap) the_sequence[nr_elems_in_seq];

  uint_fast16_t seq_idx = emprically_best_seq(nr_items, sizeof(gap), the_sequence);

  while(-7) {

    gap = the_sequence[seq_idx];

    for ( typeof(gap) i = gap; i < nr_items; i++ ) {
      const typeof(*arr) tmp = arr[i];
      const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);

      typeof(i) j = i;
      while(-7) {
        if ( arr[j - gap] <= tmp ) {
          arr[j] = tmp;
          set_bits_sp(sep_data, j, len_sepd_in_bits, tmp_sepd);
          break;
        }

        arr[j] = arr[j - gap];
        typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - gap, len_sepd_in_bits);
        set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);
        if ( j < gap * 2 ) {
          arr[j - gap] = tmp;
          set_bits_sp(sep_data, j - gap, len_sepd_in_bits, tmp_sepd);
          break;
        }
        j -= gap;
      }

    }

    if ( seq_idx == 0 ) {
      break;
    }

    seq_idx--;
  }
}

//-------------------------------------------------------------------------------------------------
//__attribute__ ((target ("sse4.2")))
void shell_sort_u8_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  //uint8_t * restrict arr = __builtin_assume_aligned(data, 128u/8u);
  uint8_t * restrict const arr = data;

  uint32_t gap;  // You can also use uint64_t for huge numbers of items
  static_assert(sizeof(gap) <= sizeof(nr_items), "There is a chance that it overflows!");
  const uint8_t nr_elems_in_seq = sizeof(gap)<=4 ? 27 : 54;  // Prevent the values from overflowing
  typeof(gap) the_sequence[nr_elems_in_seq];

  uint_fast16_t seq_idx = emprically_best_seq(nr_items, sizeof(gap), the_sequence);

  while(-7) {
    gap = the_sequence[seq_idx];

    // Do gapped insertion sorts
    for ( typeof(gap) i = gap; i < nr_items; i++ ) {
      const typeof(*arr) tmp = arr[i];
      const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);

      typeof(i) j = i;
      while(-7) {
        if ( arr[j - gap] <= tmp ) {
          arr[j] = tmp;
          set_bits_sp(sep_data, j, len_sepd_in_bits, tmp_sepd);
          break;
        }

        arr[j] = arr[j - gap];
        typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - gap, len_sepd_in_bits);
        set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);
        if ( j < gap * 2 ) {
          arr[j - gap] = tmp;
          set_bits_sp(sep_data, j - gap, len_sepd_in_bits, tmp_sepd);
          break;
        }
        j -= gap;
      }

    }

    if ( seq_idx == 0 ) {
      break;
    }

    seq_idx--;
  }

}



//-------------------------------------------------------------------------------------------------
void insert_sort_u64( uint32_t nr_items, void * data )
{
  uint64_t * restrict const arr = data;

  for ( typeof(nr_items) i = 1; i < nr_items; i++ ) {
    const typeof(*arr) tmp = arr[i];

    typeof(i) j = i;
    while(-7) {
      if ( arr[j - 1] <= tmp ) {
        const typeof(i) x = j;
        arr[x] = tmp;
        break;
      }

      arr[j] = arr[j - 1];
      if ( j < 2 ) {
        //const typeof(i) x = j - 1;  // 0
        const typeof(i) x = 0;
        arr[x] = tmp;
        break;
      }
      j--;
    }
  }

}

//-------------------------------------------------------------------------------------------------
void insert_sort_u32( uint32_t nr_items, void * data )
{
  uint32_t * restrict const arr = data;

#define sort_insert_template                          \
  for ( typeof(nr_items) i = 1; i < nr_items; i++ ) { \
    const typeof(*arr) tmp = arr[i];                  \
                                                      \
    typeof(i) j = i;                                  \
    while(-7) {                                       \
      if ( arr[j - 1] <= tmp ) {                      \
        const typeof(i) x = j;                        \
        arr[x] = tmp;                                 \
        break;                                        \
      }                                               \
                                                      \
      arr[j] = arr[j - 1];                            \
      if ( j < 2 ) {                                  \
        const typeof(i) x = 0;                        \
        arr[x] = tmp;                                 \
        break;                                        \
      }                                               \
      j--;                                            \
    }                                                 \
  }
  sort_insert_template
}

//-------------------------------------------------------------------------------------------------
void insert_sort_u16( uint32_t nr_items, void * data )
{
  uint16_t * restrict const arr = data;
  sort_insert_template
}

//-------------------------------------------------------------------------------------------------
void insert_sort_u8( uint32_t nr_items, void * data )
{
  uint8_t * restrict const arr = data;
  sort_insert_template
#undef sort_insert_template
}


//-------------------------------------------------------------------------------------------------
void insert_sort_u64_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  uint64_t * restrict const arr = data;

  for ( typeof(nr_items) i = 1; i < nr_items; i++ ) {
    const typeof(*arr) tmp = arr[i];
    const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);

    typeof(i) j = i;
    while(-7) {
      if ( arr[j - 1] <= tmp ) {
        const typeof(i) x = j;
        arr[x] = tmp;
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);
        break;
      }

      arr[j] = arr[j - 1];
      typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - 1, len_sepd_in_bits);
      set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);
      if ( j < 2 ) {
        //const typeof(i) x = j - 1;  // 0
        const typeof(i) x = 0;
        arr[x] = tmp;
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);
        break;
      }
      j--;
    }
  }

}

//-------------------------------------------------------------------------------------------------
void insert_sort_u32_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  uint32_t * restrict const arr = data;

#define sort_insert_sepd_bits_template                                                    \
  for ( typeof(nr_items) i = 1; i < nr_items; i++ ) {                                     \
    const typeof(*arr) tmp = arr[i];                                                      \
    const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);         \
                                                                                          \
    typeof(i) j = i;                                                                      \
    while(-7) {                                                                           \
      if ( arr[j - 1] <= tmp ) {                                                          \
        const typeof(i) x = j;                                                            \
        arr[x] = tmp;                                                                     \
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);                             \
        break;                                                                            \
      }                                                                                   \
                                                                                          \
      arr[j] = arr[j - 1];                                                                \
      typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - 1, len_sepd_in_bits);  \
      set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);                        \
      if ( j < 2 ) {                                                                      \
        const typeof(i) x = 0;                                                            \
        arr[x] = tmp;                                                                     \
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);                             \
        break;                                                                            \
      }                                                                                   \
      j--;                                                                                \
    }                                                                                     \
  }
  sort_insert_sepd_bits_template
}

//-------------------------------------------------------------------------------------------------
void insert_sort_u16_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  uint16_t * restrict const arr = data;
  sort_insert_sepd_bits_template
}

//-------------------------------------------------------------------------------------------------
void insert_sort_u8_sepd_bits( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits )
{
  uint8_t * restrict const arr = data;
  sort_insert_sepd_bits_template
#undef sort_insert_sepd_bits_template
}


//-------------------------------------------------------------------------------------------------
uint32_t insert_sort_u64_sepd_bits_track( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx )
{
  uint64_t * restrict arr = data;

  for ( typeof(nr_items) i = 1; i < nr_items; i++ ) {
    const typeof(*arr) tmp = arr[i];
    const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);
    const typeof(node_idx) orig_val = node_idx;

    typeof(i) j = i;
    while(-7) {
      if ( arr[j - 1] <= tmp ) {
        const typeof(i) x = j;
        arr[x] = tmp;
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);
        break;
      }

      arr[j] = arr[j - 1];
      node_idx = node_idx == j - 1 ? j : node_idx;
      typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - 1, len_sepd_in_bits);
      set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);
      if ( j < 2 ) {
        //const typeof(i) x = j - 1;  // 0
        const typeof(i) x = 0;
        arr[x] = tmp;
        node_idx = orig_val == i ? x : node_idx;
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);
        break;
      }
      j--;
    }
  }

  return node_idx;
}

//-------------------------------------------------------------------------------------------------
uint32_t insert_sort_u32_sepd_bits_track( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx )
{
  uint32_t * restrict const arr = data;

#define sort_insert_sepd_bits_track_template                                              \
  for ( typeof(nr_items) i = 1; i < nr_items; i++ ) {                                     \
    const typeof(*arr) tmp = arr[i];                                                      \
    const typeof(nr_items) tmp_sepd = get_bits_sp(sep_data, i, len_sepd_in_bits);         \
    const typeof(node_idx) orig_val = node_idx;                                           \
                                                                                          \
    typeof(i) j = i;                                                                      \
    while(-7) {                                                                           \
      if ( arr[j - 1] <= tmp ) {                                                          \
        const typeof(i) x = j;                                                            \
        arr[x] = tmp;                                                                     \
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);                             \
        break;                                                                            \
      }                                                                                   \
                                                                                          \
      arr[j] = arr[j - 1];                                                                \
      node_idx = node_idx == j - 1 ? j : node_idx;                                        \
      typeof(nr_items) value_from_next = get_bits_sp(sep_data, j - 1, len_sepd_in_bits);  \
      set_bits_sp(sep_data, j, len_sepd_in_bits, value_from_next);                        \
      if ( j < 2 ) {                                                                      \
        const typeof(i) x = 0;                                                            \
        arr[x] = tmp;                                                                     \
        node_idx = orig_val == i ? x : node_idx;                                          \
        set_bits_sp(sep_data, x, len_sepd_in_bits, tmp_sepd);                             \
        break;                                                                            \
      }                                                                                   \
      j--;                                                                                \
    }                                                                                     \
  }                                                                                       \
                                                                                          \
  return node_idx;
  sort_insert_sepd_bits_track_template
}

//-------------------------------------------------------------------------------------------------
uint32_t insert_sort_u16_sepd_bits_track( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx )
{
  uint16_t * restrict const arr = data;
  sort_insert_sepd_bits_track_template
}

//-------------------------------------------------------------------------------------------------
uint32_t insert_sort_u8_sepd_bits_track( uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx )
{
  uint8_t * restrict const arr = data;
  sort_insert_sepd_bits_track_template
#undef sort_insert_sepd_bits_track_template
}


/*
#if defined(__clang__)
__attribute__ ((minsize))
#elif defined(__GNUC__) || defined(__GNUG__)
__attribute__ ((optimize("s")))
#endif
void init_sorter(void) {
  uint8_t alignment;
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64)\
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  alignment = x86_flags.avx ? ALIGN_FOR_AVX : ALIGN_FOR_SSE;
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM)
  alignment = ALIGN_FOR_NEON;
#endif

  sort_shell_aligned_u64_u64_sepd_bits_impl = alignment == 32 ? sort_shell_aligned_u64_u64_sepd_bits_256 : sort_shell_aligned_u64_u64_sepd_bits_128;

}
*/


