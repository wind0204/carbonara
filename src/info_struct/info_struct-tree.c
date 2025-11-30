#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "info_struct.h"
#include "sorter.h"
#include "util-c_lang-bits.h"
#include "util-c_lang-x86.h"
#include "util-c_lang-basic.h"
#include "util-c_lang-bits2.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif



//-------------------------------------------------------------------------------------------------
void infos_insert_sort_bst( struct infos_bst * tree );
static void shell_sort_tree( struct infos_bst * tree );
static bst_node_idx_t insert_sort_tree_while_tracking(
    struct infos_bst * tree, bst_node_idx_t node_idx );
static void remove_left_side( struct infos_bst * indices, bst_node_idx_t idx, uint8_t height );
static void remove_right_side( struct infos_bst * indices, bst_node_idx_t idx, uint8_t height );
static struct infos_bst * (*infos_bst_create_impl)(
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    //void (*custom_free)(void * restrict userdata, void * restrict ptr),
    void * restrict userdata,
    const void * restrict keys, bst_node_idx_t nr_active_nodes, bst_node_idx_t max_nr_nodes,
    uint8_t key_len );
struct infos_bst * infos_bst_create_default( //void * restrict data,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    //void (*custom_free)(void * restrict userdata, void * restrict ptr),
    void * restrict userdata,
    const void * restrict keys, bst_node_idx_t nr_active_nodes, bst_node_idx_t max_nr_nodes,
    uint8_t key_len );
void init_infos_tree(uint64_t proc_info);



//-------------------------------------------------------------------------------------------------
struct infos_bst * infos_bst_create( //void * restrict data,
    void * restrict userdata,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    //void (*custom_free)(void * restrict userdata, void * restrict ptr),
    const void * restrict keys, bst_node_idx_t nr_active_nodes, bst_node_idx_t max_nr_nodes,
    uint8_t key_len  ///< The length of each key in bytes
    )
{
  return infos_bst_create_impl(
      custom_malloc, userdata, keys, nr_active_nodes, max_nr_nodes, key_len );
}

//-------------------------------------------------------------------------------------------------
void infos_bst_destroy( struct infos_bst * restrict tree,
    void * restrict userdata,
    void (*custom_free)(void * restrict userdata, void * restrict ptr) )
{
  void (* const local_free)(void * restrict userdata, void * restrict ptr) =
    custom_free ? custom_free : default_free;

  void * ptr = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

  const bst_node_idx_t capacity = infos_bst_calc_capacity( tree->height_cap );
  const size_t key_buf_len = (capacity+1) * tree->key_len;
  const uint_fast8_t index_len_in_bits = tree->height_cap + 1;  // The number of bits needed to
                                                                // represent each index
  const size_t idx_buf_len = (index_len_in_bits * capacity + 8 - 1) / 8;
  size_t string_len = key_buf_len + idx_buf_len + sizeof(struct infos_bst);
  string_len = _align_to_power_of_two(string_len, 128/8);
  local_free( userdata, ptr );
}


//-------------------------------------------------------------------------------------------------
//__attribute__ ((target ("lzcnt, sse4.2")))
//__attribute__ ((target ("lzcnt")))  Yes I still do use _lzcnt_u32
struct infos_bst * infos_bst_create_default( //void * restrict data,
    void * (*custom_malloc)(void * userdata, size_t alignment, size_t size),
    //void (*custom_free)(void * restrict userdata, void * restrict ptr),
    void * restrict userdata,
    const void * restrict keys, bst_node_idx_t nr_active_nodes, bst_node_idx_t max_nr_nodes,
    uint8_t key_len )
{
  assume( max_nr_nodes >= nr_active_nodes );
  assume( _is_power_of_two(max_nr_nodes + 1) );
  const uint_fast8_t height_cap = get_log2(max_nr_nodes);
  const bst_node_idx_t capacity = infos_bst_calc_capacity( height_cap );
  const uint_fast8_t idx_len_in_bits = height_cap + 1;  // The number of bits needed to represent
                                                        // each index
  //size_t string_len = capacity * key_len
  const size_t key_buf_len = (capacity+1) * key_len;

  // The length of the buffer to store data(index/sepd) associated with keys
  size_t idx_buf_len = (idx_len_in_bits * capacity + 8 - 1) / 8;

  // Allocate more memory so that I don't touch out of bounds when utilizing [gs]et_bits_sp
  // _sp stands for _Super_Poor, which is used when you want to maximize space utilization
  const size_t sepd_storage_unit_len = idx_len_in_bits > 32 ? sizeof(uint64_t) : sizeof(uint32_t);
  idx_buf_len += sepd_storage_unit_len;
  idx_buf_len = _align_to_power_of_two( idx_buf_len, sepd_storage_unit_len );

  // I'm putting the array of keys in the front just in case. ofs_key_buf = 0;
  // The struct in the middle.
  uint64_t ofs_itself;
  // The indices/sepd of the keys in the back.
  uint64_t ofs_idx_buf;
  // There can be some paddings placed between them.
  unsigned int out_of_align_diff1 = 0, out_of_align_diff2 = 0;
  if ( key_buf_len % alignof(struct infos_bst) ) {
    out_of_align_diff1 = _align_to_power_of_two( key_buf_len, alignof(struct infos_bst) )
      - key_buf_len;
  }
  if ( (key_buf_len + out_of_align_diff1 + sizeof(struct infos_bst)) % sepd_storage_unit_len ) {
    out_of_align_diff2 = _align_to_power_of_two(
        (key_buf_len + out_of_align_diff1 + sizeof(struct infos_bst)), sepd_storage_unit_len
        ) - (key_buf_len + out_of_align_diff1 + sizeof(struct infos_bst));
  }
  ofs_itself = key_buf_len + out_of_align_diff1;
  ofs_idx_buf = key_buf_len + out_of_align_diff1 + sizeof(struct infos_bst) + out_of_align_diff2;

  size_t string_len = key_buf_len + out_of_align_diff1 + sizeof(struct infos_bst)
    + out_of_align_diff2 + idx_buf_len;

  void * (* const local_malloc)(void * userdata, size_t alignment, size_t size) =
    custom_malloc ? custom_malloc : default_malloc;


  string_len = _align_to_power_of_two(string_len, ALIGN_FOR_AVX);
  void * ptr = local_malloc(userdata, ALIGN_FOR_AVX, string_len);
  if ( !ptr ) return 0;

  struct infos_bst * const tree = ptr + ofs_itself;
  tree->keys = ptr;
  tree->ofs_idx_buf = out_of_align_diff2;

  //tree->data = data;
  tree->height_cap = height_cap;
  tree->key_len = key_len;
  if ( nr_active_nodes > 1 ) {
    tree->max_active_depth = get_log2(nr_active_nodes);  // = The height of the tree
    //tree->min_depth = _mm_popcnt_u32(nr_active_nodes) == tree->max_active_depth+1 ? tree->max_active_depth : tree->max_active_depth - 1;
  } else {
    tree->max_active_depth = 0;  // = The height of the tree
    //tree->min_depth = 0;
  }
  tree->nr_active_nodes = nr_active_nodes;


  // Copy the keys if provided by user.
  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;
  switch ( key_len ) {
    case 8/8:
      {
        // The only one line that is different across the switch block
        uint8_t * restrict const il_ptr = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

        if ( keys ) {
          for ( bst_node_idx_t key_idx = 0; key_idx < nr_active_nodes; key_idx++ ) {
            assume( ( (typeof(il_ptr))(keys) )[key_idx] > 0 );
            //assume( ( (typeof(il_ptr))(keys) )[key_idx] > 0 &&
            //    ( (typeof(il_ptr))(keys) )[key_idx] < UINT_MAX_(*il_ptr) );
            il_ptr[key_idx] = ( (typeof(il_ptr))(keys) )[key_idx];
          }
          memset(il_ptr + nr_active_nodes, 0, (nr_fields - nr_active_nodes) * tree->key_len);
          //memset(ptr + nr_fields, ~0, (capacity + 1 - nr_fields) * tree->key_len);
          memset(il_ptr + nr_fields, 0, (capacity - nr_fields) * tree->key_len);
        } else {
          memset(il_ptr, 0, nr_fields * tree->key_len);
          //memset(ptr + nr_fields, ~0, (capacity + 1 - nr_fields) * tree->key_len);
          memset(il_ptr + nr_fields, 0, (capacity - nr_fields) * tree->key_len);
        }

        // Put ~0 in case they get sorted away from its original position.
        il_ptr[capacity] = 0;
        il_ptr[capacity] = ~il_ptr[capacity];
      }
      break;
    case 16/8:
      {
        // The only one line that is different across the switch block
        uint16_t * restrict const il_ptr = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
#define _template_                                                                            \
        if ( keys ) {                                                                         \
          for ( bst_node_idx_t key_idx = 0; key_idx < nr_active_nodes; key_idx++ ) {          \
            assume( ( (typeof(il_ptr))(keys) )[key_idx] > 0 );                                \
            il_ptr[key_idx] = ( (typeof(il_ptr))(keys) )[key_idx];                            \
          }                                                                                   \
          memset(il_ptr + nr_active_nodes, 0, (nr_fields - nr_active_nodes) * tree->key_len); \
          memset(il_ptr + nr_fields, 0, (capacity - nr_fields) * tree->key_len);              \
        } else {                                                                              \
          memset(il_ptr, 0, nr_fields * tree->key_len);                                       \
          memset(il_ptr + nr_fields, 0, (capacity - nr_fields) * tree->key_len);              \
        }                                                                                     \
                                                                                              \
        il_ptr[capacity] = 0;                                                                 \
        il_ptr[capacity] = ~il_ptr[capacity];
        _template_
      }
      break;
    case 32/8:
      {
        // The only one line that is different across the switch block
        uint32_t * restrict const il_ptr = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_
      }
      break;
    case 64/8:
      {
        // The only one line that is different across the switch block
        uint64_t * restrict const il_ptr = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_
#undef _template_
      }
      break;
    default:
      unreachable();
  }


  // Store indices, they're hidden right after the struct
  void * restrict const indices = ptr + ofs_idx_buf;
  const bst_node_idx_t nr_ids = keys ? nr_active_nodes : 0;
  memset( indices, 0,
      !nr_ids ? 0: _align_to_power_of_two(
        (nr_ids*idx_len_in_bits + 8 - 1)/8, (idx_len_in_bits > 32 ? 64/8 : 32/8) )
      );

  // OR stamping
  if ( idx_len_in_bits > 32 ) {
    uint64_t first_qword = 0;
    uint64_t cur_idx = 1;
    uint32_t qword_idx = idx_len_in_bits == 64 ? 1 : 0;
    const uint32_t threshold = 64 - idx_len_in_bits;
    uint64_t head_bit_idx = cur_idx * idx_len_in_bits;
    while (-7) {
      //const uint64_t head_bit_idx = cur_idx * idx_len_in_bits;
      const uint32_t pos_msb_in_qword = head_bit_idx % 64;
      uint64_t stamp = cur_idx;
      stamp = stamp << (sizeof(stamp)*8 - idx_len_in_bits);
      stamp = stamp >> pos_msb_in_qword;
      first_qword |= stamp;

      // Commit to memory when jumping to the next qword.
      if ( pos_msb_in_qword >= threshold ) {
        //to_another_byte_order(0, util_endian_BIG_ENDIAN, &first_qword, sizeof(first_qword));
        *((uint64_t*)indices + qword_idx) = first_qword;
        qword_idx++;
        uint64_t stamp_next_qword = cur_idx;
        stamp_next_qword = stamp_next_qword << ( sizeof(stamp)*8 - idx_len_in_bits );
        uint_fast8_t nr_bits_in_first_qword =  // Avoid 64 because SH[LR] 64 is a no-op.
          sizeof(stamp)*8 - (pos_msb_in_qword?pos_msb_in_qword:1);
        stamp_next_qword = stamp_next_qword << nr_bits_in_first_qword;
        first_qword = stamp_next_qword;
      }

      cur_idx++;
      head_bit_idx += idx_len_in_bits;
      if ( cur_idx >= nr_ids ) {
        break;
      }
    }

    // Assign 1's in the remaining bits
    const uint32_t pos_msb_in_qword = head_bit_idx % 64;
    uint64_t filler = ~(uint64_t)0;
    filler = filler >> pos_msb_in_qword;
    first_qword |= filler;

    //to_another_byte_order(0, util_endian_BIG_ENDIAN, &first_qword, sizeof(first_qword));
    *((uint64_t*)indices + qword_idx) |= first_qword;

    memset( indices + qword_idx*64/8 + 64/8, ~0, idx_buf_len - (qword_idx+1)*64/8 );

  } else {  // idx_len_in_bits <= 32

    uint32_t first_dword = 0;
    uint32_t cur_idx = 1;
    uint32_t dword_idx = idx_len_in_bits == 32 ? 1 : 0;
    const uint32_t threshold = 32 - idx_len_in_bits;
    uint64_t head_bit_idx = cur_idx * idx_len_in_bits;
    while (-7) {
      //const uint64_t head_bit_idx = cur_idx * idx_len_in_bits;
      const uint32_t pos_msb_in_dword = head_bit_idx % 32;
      uint32_t stamp = cur_idx;
      stamp = stamp << (sizeof(stamp)*8 - idx_len_in_bits);
      stamp = stamp >> pos_msb_in_dword;
      first_dword |= stamp;

      // Commit to memory when jumping to the next dword.
      if ( pos_msb_in_dword >= threshold ) {
        //to_another_byte_order(0, util_endian_BIG_ENDIAN, &first_dword, sizeof(first_dword));
        *((uint32_t*)indices + dword_idx) = first_dword;
        dword_idx++;
        uint32_t stamp_next_dword = cur_idx;
        stamp_next_dword = stamp_next_dword << ( sizeof(stamp)*8 - idx_len_in_bits );
        uint_fast8_t nr_bits_in_first_dword =  // Avoid 32 because SH[LR] 32 is a no-op.
          sizeof(stamp)*8 - (pos_msb_in_dword?pos_msb_in_dword:1);
        stamp_next_dword = stamp_next_dword << nr_bits_in_first_dword;
        first_dword = stamp_next_dword;
      }

      cur_idx++;
      head_bit_idx += idx_len_in_bits;
      if ( cur_idx >= nr_ids ) {
        break;
      }
    }

    // Assign 1's in the remaining bits
    const uint32_t pos_msb_in_dword = head_bit_idx % 32;
    uint32_t filler = ~(uint32_t)0;
    filler = filler >> pos_msb_in_dword;
    first_dword |= filler;

    //to_another_byte_order(0, util_endian_BIG_ENDIAN, &first_dword, sizeof(first_dword));
    *((uint32_t*)indices + dword_idx) |= first_dword;

    memset( indices + dword_idx*32/8 + 32/8, ~0, idx_buf_len - (dword_idx+1)*32/8 );
  }

  // Sort them, the node with the smallest key goes first
  shell_sort_tree(tree);

  return tree;
}


//__attribute__ ((target ("lzcnt, popcnt, avx")))
//struct infos_bst * infos_bst_create_256( //void * restrict data,
//    struct infos_bst_create_params params[restrict static 1], uint8_t with_help_from_aligned_malloc) {
//}


//-------------------------------------------------------------------------------------------------
void infos_bst_remove_3(
    struct infos_bst * tree, bst_node_idx_t cur_idx, uint_fast8_t cur_height )
{
  if ( cur_height == 0 ) {
    switch ( tree->key_len ) {
      case 8/8:
        {
          // The only one line that is different across the switch block
          uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
          if ( keys[cur_idx] == 0 )
          {
            return;
          }
          keys[cur_idx] = 0;
        }
        break;
      case 16/8:
        {
          // The only one line that is different across the switch block
          uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
          if ( keys[cur_idx] == 0 )
          {
            return;
          }
          keys[cur_idx] = 0;
        }
        break;
      case 32/8:
        {
          // The only one line that is different across the switch block
          uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
          if ( keys[cur_idx] == 0 )
          {
            return;
          }
          keys[cur_idx] = 0;
        }
        break;
      case 64/8:
        {
          // The only one line that is different across the switch block
          uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
          if ( keys[cur_idx] == 0 )
          {
            return;
          }
          keys[cur_idx] = 0;
        }
        break;
      default:
        unreachable();
    }
    tree->nr_active_nodes--;
    void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
    const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
    set_bits_sp(indices, cur_idx, idx_len_in_bits, ~(uint64_t)0);
    return;
  }

  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
  switch ( tree->key_len ) {
    case 8/8:
      {
        // The only one line that is different across the switch block
        uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        if ( keys[cur_idx] == 0 )
        {
          return;
        }
        tree->nr_active_nodes--;

        for ( uint_fast8_t h = 0; h < cur_height; h++ ) {

          if ( keys[cur_idx - (1<<h)] != 0 ) {
            keys[cur_idx] = keys[cur_idx - (1<<h)];
            set_bits_sp( indices, cur_idx, idx_len_in_bits,
                get_bits_sp(indices, cur_idx - (1<<h), idx_len_in_bits) );
            remove_left_side( tree, cur_idx - (1<<h), h );

            return;
          } else if ( keys[cur_idx + (1<<h)] != 0 ) {
            keys[cur_idx] = keys[cur_idx + (1<<h)];
            set_bits_sp( indices, cur_idx, idx_len_in_bits,
                get_bits_sp(indices, cur_idx + (1<<h), idx_len_in_bits) );
            remove_right_side( tree, cur_idx + (1<<h), h );

            return;
          }
        }

        keys[cur_idx] = 0;
        set_bits_sp(indices, cur_idx, idx_len_in_bits, ~(uint64_t)0);
      }
      break;
    case 16/8:
      {
        // The only one line that is different across the switch block
        uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
#define _template_                                                          \
        if ( keys[cur_idx] == 0 )                                           \
        {                                                                   \
          return;                                                           \
        }                                                                   \
        tree->nr_active_nodes--;                                            \
        for ( uint_fast8_t h = 0; h < cur_height; h++ ) {                   \
                                                                            \
          if ( keys[cur_idx - (1<<h)] != 0 ) {                              \
            keys[cur_idx] = keys[cur_idx - (1<<h)];                         \
            set_bits_sp( indices, cur_idx, idx_len_in_bits,                 \
                get_bits_sp(indices, cur_idx - (1<<h), idx_len_in_bits) );  \
            remove_left_side( tree, cur_idx - (1<<h), h );                  \
                                                                            \
            return;                                                         \
          } else if ( keys[cur_idx + (1<<h)] != 0 ) {                       \
            keys[cur_idx] = keys[cur_idx + (1<<h)];                         \
            set_bits_sp( indices, cur_idx, idx_len_in_bits,                 \
                get_bits_sp(indices, cur_idx + (1<<h), idx_len_in_bits) );  \
            remove_right_side( tree, cur_idx + (1<<h), h );                 \
                                                                            \
            return;                                                         \
          }                                                                 \
        }                                                                   \
                                                                            \
        keys[cur_idx] = 0;                                                  \
        set_bits_sp(indices, cur_idx, idx_len_in_bits, ~(uint64_t)0);
        _template_
      }
      break;
    case 32/8:
      {
        // The only one line that is different across the switch block
        uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_
      }
      break;
    case 64/8:
      {
        // The only one line that is different across the switch block
        uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_
#undef _template_
      }
      break;
    default:
      unreachable();
  }
  return;
}


//-------------------------------------------------------------------------------------------------
static void remove_left_side( struct infos_bst * tree, bst_node_idx_t idx, uint8_t height )
{
  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
  switch ( tree->key_len ) {
    case 8/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

remove_8:
        for ( uint_fast8_t h = 0; h < height; h++ ) {

          if ( keys[idx - (1<<h)] != 0 ) {
            keys[idx] = keys[idx - (1<<h)];
            set_bits_sp( indices, idx, idx_len_in_bits,
                get_bits_sp(indices, idx - (1<<h), idx_len_in_bits) );

            idx = idx - (1<<h);
            height = h;
            goto remove_8;
          }
        }

        keys[idx] = 0;
        set_bits_sp(indices, idx, idx_len_in_bits, ~(uint64_t)0);
      }
      break;
    case 16/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
#define _template_(nr_bits)                                             \
remove_ ## nr_bits :                                                    \
        for ( uint_fast8_t h = 0; h < height; h++ ) {                   \
                                                                        \
          if ( keys[idx - (1<<h)] != 0 ) {                              \
            keys[idx] = keys[idx - (1<<h)];                             \
            set_bits_sp( indices, idx, idx_len_in_bits,                 \
                get_bits_sp(indices, idx - (1<<h), idx_len_in_bits) );  \
                                                                        \
            idx = idx - (1<<h);                                         \
            height = h;                                                 \
            goto remove_ ## nr_bits ;                                   \
          }                                                             \
        }                                                               \
                                                                        \
        keys[idx] = 0;                                                  \
        set_bits_sp(indices, idx, idx_len_in_bits, ~(uint64_t)0);
        _template_(16)
      }
      break;
    case 32/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(32)
      }
      break;
    case 64/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(64)
#undef _template_
      }
      break;
    default:
      unreachable();
  }
}


//-------------------------------------------------------------------------------------------------
static void remove_right_side( struct infos_bst * tree, bst_node_idx_t idx, uint8_t height )
{
  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;
  switch ( tree->key_len ) {
    case 8/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

remove_8:
        for ( uint_fast8_t h = 0; h < height; h++ ) {

          if ( keys[idx + (1<<h)] != 0 ) {
            keys[idx] = keys[idx + (1<<h)];
            set_bits_sp( indices, idx, idx_len_in_bits,
                get_bits_sp(indices, idx + (1<<h), idx_len_in_bits) );

            idx = idx + (1<<h);
            height = h;
            goto remove_8;
          }
        }

        keys[idx] = 0;
        set_bits_sp(indices, idx, idx_len_in_bits, ~(uint64_t)0);
      }
      break;
    case 16/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
#define _template_(nr_bits)                                             \
remove_ ## nr_bits :                                                    \
        for ( uint_fast8_t h = 0; h < height; h++ ) {                   \
                                                                        \
          if ( keys[idx + (1<<h)] != 0 ) {                              \
            keys[idx] = keys[idx + (1<<h)];                             \
            set_bits_sp( indices, idx, idx_len_in_bits,                 \
                get_bits_sp(indices, idx + (1<<h), idx_len_in_bits) );  \
                                                                        \
            idx = idx + (1<<h);                                         \
            height = h;                                                 \
            goto remove_ ## nr_bits ;                                   \
          }                                                             \
        }                                                               \
                                                                        \
        keys[idx] = 0;                                                  \
        set_bits_sp(indices, idx, idx_len_in_bits, ~(uint64_t)0);
        _template_(16)
      }
      break;
    case 32/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(32)
      }
      break;
    case 64/8:
      {
        // The only one line that is different across the switch block except the goto label.
        uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(64)
#undef _template_
      }
      break;
    default:
      unreachable();
  }
}


//-------------------------------------------------------------------------------------------------
void infos_bst_print( const struct infos_bst * tree, uint_fast8_t nr_digits )
{
  nr_digits |= 1;
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;
  uint_fast8_t cur_height = height;

  char strfmt[6];
  sprintf( strfmt, "%%0%dx", nr_digits );

  // The spacing before the first element
  uint_fast8_t spacing = 1;
  for ( uint_fast8_t i = 1; i <= cur_height; i++ )
    spacing = spacing * 2 + (nr_digits / 2);

  const bst_node_idx_t tip_idx = infos_bst_locate_bottom_right_corner( height );
  for ( ; cur_height <= height; cur_height-- ) {
    for ( uint_fast16_t i = 0; i < spacing; i++ )
      fprintf(stdout, " ");

    // The spacing between elements
    spacing = spacing * 2 - 1;

    bst_node_idx_t idx = infos_bst_locate_first_node_in_row(cur_height);
    while(-7) {
      switch ( tree->key_len ) {
        case 8/8:
          {
            uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
            fprintf(stdout, strfmt, keys[idx]);
          }
          break;
        case 16/8:
          {
            uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
            fprintf(stdout, strfmt, keys[idx]);
          }
          break;
        case 32/8:
          {
            uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
            fprintf(stdout, strfmt, keys[idx]);
          }
          break;
        case 64/8:
          {
            uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
            fprintf(stdout, strfmt, keys[idx]);
#pragma GCC diagnostic pop
          }
          break;
        default:
          unreachable();
      }

      idx = infos_bst_tr_next(idx, cur_height);
      if ( idx > tip_idx ) {
        fprintf(stdout, "\n");
        break;
      }

      for ( uint_fast16_t i = 0; i < spacing; i++ )
        fprintf(stdout, " ");
    }

    // The spacing before the first element
    spacing = (spacing + 1) / 2;
    // The spacing for the next loop i.e. lower height
    spacing = (spacing - (nr_digits / 2)) / 2;
  }
}


//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_u64( const struct infos_bst * tree, uint64_t key )
{
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );
  uint_fast8_t cur_height = height;

  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;

  typeof(&key) restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

  while(-7) {
    if ( keys[cur_idx] == key ) {
      return get_bits_sp((void*)(tree + 1) + tree->ofs_idx_buf, cur_idx, idx_len_in_bits);
    }

    if ( cur_height == 0 )
      return ~0;

    if ( key < keys[cur_idx] ) {
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
    } else {
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
    }
    cur_height--;
  }

}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_u32( const struct infos_bst * tree, uint32_t key ) {
#define infos_bst_search_template                                                           \
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;      \
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );                                 \
  uint_fast8_t cur_height = height;                                                         \
                                                                                            \
  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;                                \
                                                                                            \
  typeof(&key) restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX ); \
                                                                                            \
  while(-7) {                                                                               \
    if ( keys[cur_idx] == key ) {                                                           \
      return get_bits_sp((void*)(tree + 1) + tree->ofs_idx_buf, cur_idx, idx_len_in_bits);  \
    }                                                                                       \
                                                                                            \
    if ( cur_height == 0 )                                                                  \
      return ~0;                                                                            \
                                                                                            \
    if ( key < keys[cur_idx] ) {                                                            \
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);                                \
    } else {                                                                                \
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);                               \
    }                                                                                       \
    cur_height--;                                                                           \
  }
  infos_bst_search_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_u16( const struct infos_bst * tree, uint16_t key )
{
  infos_bst_search_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_u8( const struct infos_bst * tree, uint8_t key ) {
  infos_bst_search_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_i64( const struct infos_bst * tree, int64_t key ) {
  infos_bst_search_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_i32( const struct infos_bst * tree, int32_t key ) {
  infos_bst_search_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_i16( const struct infos_bst * tree, int16_t key ) {
  infos_bst_search_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_i8( const struct infos_bst * tree, int8_t key ) {
  infos_bst_search_template
}
#undef infos_bst_search_template


//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_u64( const struct infos_bst * tree, uint64_t key )
{
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );
  uint_fast8_t cur_height = height;

  //uint_fast8_t idx_len_in_bits = tree->height_cap + 1;

  typeof(&key) restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

  while(-7) {
    if ( keys[cur_idx] == key ) {
      return cur_idx;
    }

    if ( cur_height == 0 )
      return ~0;

    if ( key < keys[cur_idx] ) {
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
    } else {
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
    }
    cur_height--;
  }

}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_u32( const struct infos_bst * tree, uint32_t key ) {
#define infos_bst_search_for_node_idx_template                                              \
  uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;            \
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );                                 \
  uint_fast8_t cur_height = height;                                                         \
                                                                                            \
  typeof(&key) restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX ); \
                                                                                            \
  while(-7) {                                                                               \
    if ( keys[cur_idx] == key ) {                                                           \
      return cur_idx;                                                                       \
    }                                                                                       \
                                                                                            \
    if ( cur_height == 0 )                                                                  \
      return ~0;                                                                            \
                                                                                            \
    if ( key < keys[cur_idx] ) {                                                            \
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);                                \
    } else {                                                                                \
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);                               \
    }                                                                                       \
    cur_height--;                                                                           \
  }
  infos_bst_search_for_node_idx_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_u16( const struct infos_bst * tree, uint16_t key ) {
  infos_bst_search_for_node_idx_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_u8( const struct infos_bst * tree, uint8_t key ) {
  infos_bst_search_for_node_idx_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_i64( const struct infos_bst * tree, int64_t key ) {
  infos_bst_search_for_node_idx_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_i32( const struct infos_bst * tree, int32_t key ) {
  infos_bst_search_for_node_idx_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_i16( const struct infos_bst * tree, int16_t key ) {
  infos_bst_search_for_node_idx_template
}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_search_for_node_idx_i8( const struct infos_bst * tree, int8_t key ) {
  infos_bst_search_for_node_idx_template
}
#undef infos_bst_search_for_node_idx_template


//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_insert_u64( struct infos_bst * tree,
    uint64_t new_key, bst_node_idx_t sepd )
{
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );
  uint_fast8_t cur_height = height;

  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;

  assume( new_key > 0 && new_key != (typeof(new_key))~0u );

  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const typeof(&new_key) restrict keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;

  // The array of keys is full?
  if ( tree->nr_active_nodes >= nr_fields ) {
    // No space left for growth?
    if ( tree->height_cap <= tree->max_active_depth )
      return ~(bst_node_idx_t)0;

    //const bst_node_idx_t corner_node_idx = infos_bst_locate_bottom_right_corner( height );
    //keys[corner_node_idx + 1] = new_key;
    //set_bits_sp(indices, corner_node_idx + 1, idx_len_in_bits, sepd);
    //tree->nr_active_nodes++;
    tree->max_active_depth++;

    const bst_node_idx_t orig_nr_fields = nr_fields;
    //return insert_sort_tree_while_tracking(tree, corner_node_idx + 1);
    nr_fields = (2 << tree->max_active_depth) - 1;

    // A simple binary search
    while(-7) {
      if ( cur_height == 0 ) {
        break;
      }
      if ( new_key < keys[cur_idx] ) {
        cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
      } else {
        cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
      }
      cur_height--;
    }

    // Make space for the new key
    push_keys_and_sepd_sp( keys, cur_idx, orig_nr_fields - cur_idx, 1,
      indices, 31 - _lzcnt_u32(sizeof(new_key)), idx_len_in_bits );

    // Store new_key at cur_idx
    goto a_slot_for_the_new_key_is_ready;
  }

  while(-7) {
    if ( keys[cur_idx] == 0 ) {
a_slot_for_the_new_key_is_ready:
      // Store values.
      keys[cur_idx] = new_key;
      set_bits_sp( indices, cur_idx, idx_len_in_bits, sepd );
      tree->nr_active_nodes++;

      return cur_idx;
    }

    //if ( keys[cur_idx] == new_key ) {
    //  // Check the neighbor slots
    //  if ( keys[cur_idx - 1] == 0 ) {
    //    cur_idx = cur_idx - 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else if ( keys[cur_idx + 1] == 0 ) {
    //    cur_idx = cur_idx + 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else {
    //    break;
    //  }
    //}

    if ( cur_height == 0 ) {
      // Find a field to populate the new key
      const bst_node_idx_t dist_head = cur_idx;
      const bst_node_idx_t dist_tail = nr_fields - cur_idx - 1;
      const bst_node_idx_t end = min(dist_head, dist_tail);
      bst_node_idx_t i = 1;
      for ( ; i <= end; i++ ) {
        if ( keys[cur_idx - i] == 0 ) {
          keys[cur_idx - i] = new_key;
          set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx - i);
        }

        if ( keys[cur_idx + i] == 0 ) {
          keys[cur_idx + i] = new_key;
          set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx + i);
        }
      }

      if ( dist_tail < dist_head ) {
        for ( ; i <= dist_head; i++ ) {
          if ( keys[cur_idx - i] == 0 ) {
            keys[cur_idx - i] = new_key;
            set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx - i);
          }
        }
      } else {
        for ( ; i <= dist_tail; i++ ) {
          if ( keys[cur_idx + i] == 0 ) {
            keys[cur_idx + i] = new_key;
            set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx + i);
          }
        }
      }
    }

    if ( new_key < keys[cur_idx] ) {
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
    } else {
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
    }
    cur_height--;
  }

}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_insert_u32( struct infos_bst * tree,
    uint32_t new_key, bst_node_idx_t sepd )
{
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );
  uint_fast8_t cur_height = height;

  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;

  assume( new_key > 0 && new_key != (typeof(new_key))~0u );

  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const typeof(&new_key) restrict keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;

  // The array of keys is full?
  if ( tree->nr_active_nodes >= nr_fields ) {
    // No space left for growth?
    if ( tree->height_cap <= tree->max_active_depth )
      return ~(bst_node_idx_t)0;

    //const bst_node_idx_t corner_node_idx = infos_bst_locate_bottom_right_corner( height );
    //keys[corner_node_idx + 1] = new_key;
    //set_bits_sp(indices, corner_node_idx + 1, idx_len_in_bits, sepd);
    //tree->nr_active_nodes++;
    tree->max_active_depth++;

    const bst_node_idx_t orig_nr_fields = nr_fields;
    //return insert_sort_tree_while_tracking(tree, corner_node_idx + 1);
    nr_fields = (2 << tree->max_active_depth) - 1;

    // A simple binary search
    while(-7) {
      if ( cur_height == 0 ) {
        break;
      }
      if ( new_key < keys[cur_idx] ) {
        cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
      } else {
        cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
      }
      cur_height--;
    }

    // Make space for the new key
    push_keys_and_sepd_sp( keys, cur_idx, orig_nr_fields - cur_idx, 1,
      indices, 31 - _lzcnt_u32(sizeof(new_key)), idx_len_in_bits );

    // Store new_key at cur_idx
    goto a_slot_for_the_new_key_is_ready;
  }

  while(-7) {
    if ( keys[cur_idx] == 0 ) {
a_slot_for_the_new_key_is_ready:
      // Store values.
      keys[cur_idx] = new_key;
      set_bits_sp( indices, cur_idx, idx_len_in_bits, sepd );
      tree->nr_active_nodes++;

      return cur_idx;
    }

    //if ( keys[cur_idx] == new_key ) {
    //  // Check the neighbor slots
    //  if ( keys[cur_idx - 1] == 0 ) {
    //    cur_idx = cur_idx - 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else if ( keys[cur_idx + 1] == 0 ) {
    //    cur_idx = cur_idx + 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else {
    //    break;
    //  }
    //}

    if ( cur_height == 0 ) {
      // Find a field to populate the new key
      const bst_node_idx_t dist_head = cur_idx;
      const bst_node_idx_t dist_tail = nr_fields - cur_idx - 1;
      const bst_node_idx_t end = min(dist_head, dist_tail);
      bst_node_idx_t i = 1;
      for ( ; i <= end; i++ ) {
        if ( keys[cur_idx - i] == 0 ) {
          keys[cur_idx - i] = new_key;
          set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx - i);
        }

        if ( keys[cur_idx + i] == 0 ) {
          keys[cur_idx + i] = new_key;
          set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx + i);
        }
      }

      if ( dist_tail < dist_head ) {
        for ( ; i <= dist_head; i++ ) {
          if ( keys[cur_idx - i] == 0 ) {
            keys[cur_idx - i] = new_key;
            set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx - i);
          }
        }
      } else {
        for ( ; i <= dist_tail; i++ ) {
          if ( keys[cur_idx + i] == 0 ) {
            keys[cur_idx + i] = new_key;
            set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx + i);
          }
        }
      }
    }

    if ( new_key < keys[cur_idx] ) {
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
    } else {
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
    }
    cur_height--;
  }

}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_insert_u16( struct infos_bst * tree,
    uint16_t new_key, bst_node_idx_t sepd )
{
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );
  uint_fast8_t cur_height = height;

  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;

  assume( new_key > 0 && new_key != (typeof(new_key))~0u );

  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const typeof(&new_key) restrict keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;

  // The array of keys is full?
  if ( tree->nr_active_nodes >= nr_fields ) {
    // No space left for growth?
    if ( tree->height_cap <= tree->max_active_depth )
      return ~(bst_node_idx_t)0;

    //const bst_node_idx_t corner_node_idx = infos_bst_locate_bottom_right_corner( height );
    //keys[corner_node_idx + 1] = new_key;
    //set_bits_sp(indices, corner_node_idx + 1, idx_len_in_bits, sepd);
    //tree->nr_active_nodes++;
    tree->max_active_depth++;

    const bst_node_idx_t orig_nr_fields = nr_fields;
    //return insert_sort_tree_while_tracking(tree, corner_node_idx + 1);
    nr_fields = (2 << tree->max_active_depth) - 1;

    // A simple binary search
    while(-7) {
      if ( cur_height == 0 ) {
        break;
      }
      if ( new_key < keys[cur_idx] ) {
        cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
      } else {
        cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
      }
      cur_height--;
    }

    // Make space for the new key
    push_keys_and_sepd_sp( keys, cur_idx, orig_nr_fields - cur_idx, 1,
      indices, 31 - _lzcnt_u32(sizeof(new_key)), idx_len_in_bits );

    // Store new_key at cur_idx
    goto a_slot_for_the_new_key_is_ready;
  }

  while(-7) {
    if ( keys[cur_idx] == 0 ) {
a_slot_for_the_new_key_is_ready:
      // Store values.
      keys[cur_idx] = new_key;
      set_bits_sp( indices, cur_idx, idx_len_in_bits, sepd );
      tree->nr_active_nodes++;

      return cur_idx;
    }

    //if ( keys[cur_idx] == new_key ) {
    //  // Check the neighbor slots
    //  if ( keys[cur_idx - 1] == 0 ) {
    //    cur_idx = cur_idx - 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else if ( keys[cur_idx + 1] == 0 ) {
    //    cur_idx = cur_idx + 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else {
    //    break;
    //  }
    //}

    if ( cur_height == 0 ) {
      // Find a field to populate the new key
      const bst_node_idx_t dist_head = cur_idx;
      const bst_node_idx_t dist_tail = nr_fields - cur_idx - 1;
      const bst_node_idx_t end = min(dist_head, dist_tail);
      bst_node_idx_t i = 1;
      for ( ; i <= end; i++ ) {
        if ( keys[cur_idx - i] == 0 ) {
          keys[cur_idx - i] = new_key;
          set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx - i);
        }

        if ( keys[cur_idx + i] == 0 ) {
          keys[cur_idx + i] = new_key;
          set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx + i);
        }
      }

      if ( dist_tail < dist_head ) {
        for ( ; i <= dist_head; i++ ) {
          if ( keys[cur_idx - i] == 0 ) {
            keys[cur_idx - i] = new_key;
            set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx - i);
          }
        }
      } else {
        for ( ; i <= dist_tail; i++ ) {
          if ( keys[cur_idx + i] == 0 ) {
            keys[cur_idx + i] = new_key;
            set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx + i);
          }
        }
      }
    }

    if ( new_key < keys[cur_idx] ) {
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
    } else {
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
    }
    cur_height--;
  }

}

//-------------------------------------------------------------------------------------------------
bst_node_idx_t infos_bst_insert_u8( struct infos_bst * tree,
    uint8_t new_key, bst_node_idx_t sepd )
{
  const uint_fast8_t height = tree->max_active_depth < 1 ? 1 : tree->max_active_depth;
  bst_node_idx_t cur_idx = infos_bst_locate_root( height );
  uint_fast8_t cur_height = height;

  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;

  assume( new_key > 0 && new_key != (typeof(new_key))~0u );

  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const typeof(&new_key) restrict keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;

  // The array of keys is full?
  if ( tree->nr_active_nodes >= nr_fields ) {
    // No space left for growth?
    if ( tree->height_cap <= tree->max_active_depth )
      return ~(bst_node_idx_t)0;

    //const bst_node_idx_t corner_node_idx = infos_bst_locate_bottom_right_corner( height );
    //keys[corner_node_idx + 1] = new_key;
    //set_bits_sp(indices, corner_node_idx + 1, idx_len_in_bits, sepd);
    //tree->nr_active_nodes++;
    tree->max_active_depth++;

    const bst_node_idx_t orig_nr_fields = nr_fields;
    //return insert_sort_tree_while_tracking(tree, corner_node_idx + 1);
    nr_fields = (2 << tree->max_active_depth) - 1;

    // A simple binary search
    while(-7) {
      if ( cur_height == 0 ) {
        break;
      }
      if ( new_key < keys[cur_idx] ) {
        cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
      } else {
        cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
      }
      cur_height--;
    }

    // Make space for the new key
    push_keys_and_sepd_sp( keys, cur_idx, orig_nr_fields - cur_idx, 1,
      indices, 31 - _lzcnt_u32(sizeof(new_key)), idx_len_in_bits );

    // Store new_key at cur_idx
    goto a_slot_for_the_new_key_is_ready;
  }

  while(-7) {
    if ( keys[cur_idx] == 0 ) {
a_slot_for_the_new_key_is_ready:
      // Store values.
      keys[cur_idx] = new_key;
      set_bits_sp( indices, cur_idx, idx_len_in_bits, sepd );
      tree->nr_active_nodes++;

      return cur_idx;
    }

    //if ( keys[cur_idx] == new_key ) {
    //  // Check the neighbor slots
    //  if ( keys[cur_idx - 1] == 0 ) {
    //    cur_idx = cur_idx - 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else if ( keys[cur_idx + 1] == 0 ) {
    //    cur_idx = cur_idx + 1;
    //    goto a_slot_for_the_new_key_is_ready;
    //  } else {
    //    break;
    //  }
    //}

    if ( cur_height == 0 ) {
      // Find a field to populate the new key
      const bst_node_idx_t dist_head = cur_idx;
      const bst_node_idx_t dist_tail = nr_fields - cur_idx - 1;
      const bst_node_idx_t end = min(dist_head, dist_tail);
      bst_node_idx_t i = 1;
      for ( ; i <= end; i++ ) {
        if ( keys[cur_idx - i] == 0 ) {
          keys[cur_idx - i] = new_key;
          set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx - i);
        }

        if ( keys[cur_idx + i] == 0 ) {
          keys[cur_idx + i] = new_key;
          set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
          tree->nr_active_nodes++;
          return insert_sort_tree_while_tracking(tree, cur_idx + i);
        }
      }

      if ( dist_tail < dist_head ) {
        for ( ; i <= dist_head; i++ ) {
          if ( keys[cur_idx - i] == 0 ) {
            keys[cur_idx - i] = new_key;
            set_bits_sp(indices, cur_idx - i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx - i);
          }
        }
      } else {
        for ( ; i <= dist_tail; i++ ) {
          if ( keys[cur_idx + i] == 0 ) {
            keys[cur_idx + i] = new_key;
            set_bits_sp(indices, cur_idx + i, idx_len_in_bits, sepd);
            tree->nr_active_nodes++;
            return insert_sort_tree_while_tracking(tree, cur_idx + i);
          }
        }
      }
    }

    if ( new_key < keys[cur_idx] ) {
      cur_idx = infos_bst_tr_down_left(cur_idx, cur_height);
    } else {
      cur_idx = infos_bst_tr_down_right(cur_idx, cur_height);
    }
    cur_height--;
  }

}


//-------------------------------------------------------------------------------------------------
//__attribute__ ((target ("lzcnt, sse4.2")))
__attribute__ ((target ("lzcnt")))
void infos_insert_sort_bst( struct infos_bst * tree )
{
  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;
  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;  // The number of bits needed to
                                                              // represent each index

  // Sort them, the node with the smallest key goes to the front
  switch ( tree->key_len ) {
    case 8/8:
      insert_sort_u8_sepd_bits(nr_fields, tree->keys, indices, idx_len_in_bits);
      break;
    case 16/8:
      insert_sort_u16_sepd_bits(nr_fields, tree->keys, indices, idx_len_in_bits);
      break;
    case 32/8:
      insert_sort_u32_sepd_bits(nr_fields, tree->keys, indices, idx_len_in_bits);
      break;
    case 64/8:
      insert_sort_u64_sepd_bits(nr_fields, tree->keys, indices, idx_len_in_bits);
      break;
    default:
      unreachable();
  }


  // Move each node to their place in tree
  const bst_node_idx_t nr_blanks = nr_fields - tree->nr_active_nodes;
  // Unless there's no need
  if ( nr_blanks == 0 || tree->nr_active_nodes == 0 )
    return;

  // Each slot of this buffer can hold a key or sepd(separate data connected to the key)
  // You can assign 1 more slot here to prevent .et_bits_sp() from touching out of bounds
  bst_node_idx_t arr_blank_pos[ nr_blanks * 2 ];
  for ( uint_fast32_t i = 0, j = nr_fields / 2; i < nr_blanks; i++, j-- ) {
    assume( j <= nr_fields / 2 );  // There should be no possibility of underflow here.

    // Designate the indices of tree->keys for blank keys
    assume( sizeof(nr_fields) == 32/8 || sizeof(nr_fields) == 64/8 );
    switch ( sizeof(nr_fields) ) {
      case 32/8:
        arr_blank_pos[i] = _reverse_field(j) >> ( sizeof(j)*8 - (32 - _lzcnt_u32(nr_fields)) );
        break;
      case 64/8:
        arr_blank_pos[i] = _reverse_field(j) >> ( sizeof(j)*8 - (64 - _lzcnt_u64(nr_fields)) );
        break;
      default:
        unreachable();
    }
  }
  // blank keys get ~0 as their linked slot in the separate array.
  memset( arr_blank_pos + nr_blanks, ~0, (nr_blanks) * sizeof(*arr_blank_pos) );

  // Sort
  shell_sort_auto( nr_blanks, arr_blank_pos );
  switch ( tree->key_len ) {
    case 8/8:
      {
        uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );

        typeof(*keys) tmp;
        for ( bst_node_idx_t key_idx = nr_fields - 1,
            idx_blank = nr_blanks - 1, idx_tmp = nr_blanks - 1, idx_lowest_tmp = nr_blanks;
            key_idx <= nr_fields - 1;
            key_idx-- ) {
          if ( idx_blank > nr_blanks - 1 ) {
label_8:
            tmp = keys[key_idx];
            const bst_node_idx_t tmp_sepd = get_bits_sp(indices, key_idx, idx_len_in_bits);
            static_assert(sizeof(nr_fields) >= sizeof(uint32_t),
                "There is a chance that it overflows!");
            keys[key_idx] = arr_blank_pos[idx_tmp];
            set_bits_sp(indices, key_idx, idx_len_in_bits, arr_blank_pos[nr_blanks + idx_tmp]);
            arr_blank_pos[idx_tmp] = tmp;
            arr_blank_pos[nr_blanks + idx_tmp] = tmp_sepd;
            idx_tmp = idx_tmp == idx_lowest_tmp ? nr_blanks - 1 : idx_tmp - 1;
          } else {  // idx_blank <= nr_blanks - 1
            if ( arr_blank_pos[idx_blank] != key_idx )
              goto label_8;

            if ( keys[key_idx] != 0 ) {
              idx_lowest_tmp--;
              arr_blank_pos[idx_lowest_tmp] = keys[key_idx];
              arr_blank_pos[nr_blanks + idx_lowest_tmp] = get_bits_sp(
                  indices, key_idx, idx_len_in_bits );
              shell_sort_u32_sepd_bits(
                  nr_blanks - idx_lowest_tmp,
                  arr_blank_pos + idx_lowest_tmp,
                  arr_blank_pos + idx_lowest_tmp + nr_blanks,
                  sizeof(*arr_blank_pos) * 8 );
              idx_tmp = nr_blanks - 1;
            }
            idx_blank--;
            keys[key_idx] = 0;
            set_bits_sp(indices, key_idx, idx_len_in_bits, ~0);
          }
        }
      }
      break;
    case 16/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
#define _template_(nr_bits)                                                                     \
        typeof(*keys) tmp;                                                                      \
        for ( bst_node_idx_t key_idx = nr_fields - 1,                                           \
            idx_blank = nr_blanks - 1, idx_tmp = nr_blanks - 1, idx_lowest_tmp = nr_blanks;     \
            key_idx <= nr_fields - 1;                                                           \
            key_idx-- ) {                                                                       \
          if ( idx_blank > nr_blanks - 1 ) {                                                    \
label_ ## nr_bits :                                                                             \
            tmp = keys[key_idx];                                                                \
            const bst_node_idx_t tmp_sepd = get_bits_sp(indices, key_idx, idx_len_in_bits);     \
            static_assert(sizeof(nr_fields) >= sizeof(uint32_t),                                \
                "There is a chance that it overflows!");                                        \
            keys[key_idx] = arr_blank_pos[idx_tmp];                                             \
            set_bits_sp(indices, key_idx, idx_len_in_bits, arr_blank_pos[nr_blanks + idx_tmp]); \
            arr_blank_pos[idx_tmp] = tmp;                                                       \
            arr_blank_pos[nr_blanks + idx_tmp] = tmp_sepd;                                      \
            idx_tmp = idx_tmp == idx_lowest_tmp ? nr_blanks - 1 : idx_tmp - 1;                  \
          } else {                                                                              \
            if ( arr_blank_pos[idx_blank] != key_idx ) {                                        \
              goto label_ ## nr_bits ;                                                          \
            } else {                                                                            \
              if ( keys[key_idx] != 0 ) {                                                       \
                idx_lowest_tmp--;                                                               \
                arr_blank_pos[idx_lowest_tmp] = keys[key_idx];                                  \
                arr_blank_pos[nr_blanks + idx_lowest_tmp] =                                     \
                  get_bits_sp(indices, key_idx, idx_len_in_bits);                               \
                shell_sort_u32_sepd_bits(                                                       \
                    nr_blanks - idx_lowest_tmp, arr_blank_pos + idx_lowest_tmp,                 \
                    arr_blank_pos + idx_lowest_tmp + nr_blanks, sizeof(*arr_blank_pos) * 8 );   \
                idx_tmp = nr_blanks - 1;                                                        \
              }                                                                                 \
              idx_blank--;                                                                      \
              keys[key_idx] = 0;                                                                \
              set_bits_sp(indices, key_idx, idx_len_in_bits, ~0);                               \
            }                                                                                   \
          }                                                                                     \
        }
        _template_(16)
      }
      break;
    case 32/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(32)
      }
      break;
    case 64/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(64)
#undef _template_
      }
      break;
    default:
      unreachable();
  }
}


//-------------------------------------------------------------------------------------------------
//__attribute__ ((target ("lzcnt, sse4.2")))
__attribute__ ((target ("lzcnt"))) [[maybe_unused]]
static bst_node_idx_t insert_sort_tree_while_tracking(
    struct infos_bst * tree,
    bst_node_idx_t node_idx
    )
{
  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;
  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;  // The number of bits needed to
                                                              // represent each index

  // Sort them, the node with the smallest key goes first
  switch ( tree->key_len ) {
    case sizeof(uint8_t):
      node_idx = insert_sort_u8_sepd_bits_track(
          nr_fields, tree->keys, indices, idx_len_in_bits, node_idx);
      break;
    case sizeof(uint16_t):
      node_idx = insert_sort_u16_sepd_bits_track(
          nr_fields, tree->keys, indices, idx_len_in_bits, node_idx);
      break;
    case sizeof(uint32_t):
      node_idx = insert_sort_u32_sepd_bits_track(
          nr_fields, tree->keys, indices, idx_len_in_bits, node_idx);
      break;
    case sizeof(uint64_t):
      node_idx = insert_sort_u64_sepd_bits_track(
          nr_fields, tree->keys, indices, idx_len_in_bits, node_idx);
      break;
    default:
      unreachable();
  }


  // Move each node to their place in tree
  const bst_node_idx_t nr_blanks = nr_fields - tree->nr_active_nodes;
  // Unless there's no need
  if ( nr_blanks == 0 || tree->nr_active_nodes == 0 )
    return node_idx;

  // Each slot of this buffer can hold a key or sepd(separate data connected to the key)
  // You can assign 1 more slot here to prevent .et_bits_sp() from touching out of bounds
  bst_node_idx_t arr_blank_pos[ nr_blanks * 2 ];
  for ( uint_fast32_t i = 0, j = nr_fields / 2; i < nr_blanks; i++, j-- ) {
    assume( j <= nr_fields / 2 );  // There should be no possibility of underflow here.

    // Designate the indices of tree->keys for blank keys
    assume( sizeof(nr_fields) == 32/8 || sizeof(nr_fields) == 64/8 );
    switch ( sizeof(nr_fields) ) {
      case 32/8:
        arr_blank_pos[i] = _reverse_field(j) >> ( sizeof(j)*8 - (32 - _lzcnt_u32(nr_fields)) );
        break;
      case 64/8:
        arr_blank_pos[i] = _reverse_field(j) >> ( sizeof(j)*8 - (64 - _lzcnt_u64(nr_fields)) );
        break;
      default:
        unreachable();
    }
  }
  // blank keys get ~0 as their linked slot in the separate array.
  memset( arr_blank_pos + nr_blanks, ~0, (nr_blanks) * sizeof(*arr_blank_pos) );

  // Sort
  shell_sort_auto( nr_blanks, arr_blank_pos );
  switch ( tree->key_len ) {
    case 8/8:
      {
        uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        typeof(*keys) tmp;
        for ( bst_node_idx_t key_idx = nr_fields - 1,
            idx_blank = nr_blanks - 1, idx_tmp = nr_blanks - 1, idx_lowest_tmp = nr_blanks;
            key_idx <= nr_fields - 1;
            key_idx-- ) {
          if ( idx_blank > nr_blanks - 1 ) {
label_8 :
            tmp = keys[key_idx];
            const bst_node_idx_t tmp_sepd = get_bits_sp(indices, key_idx, idx_len_in_bits);
            static_assert(sizeof(nr_fields) >= sizeof(uint32_t),
                "There is a chance that it overflows!");
            keys[key_idx] = arr_blank_pos[idx_tmp];
            set_bits_sp(indices, key_idx, idx_len_in_bits, arr_blank_pos[nr_blanks + idx_tmp]);
            arr_blank_pos[idx_tmp] = tmp;
            arr_blank_pos[nr_blanks + idx_tmp] = tmp_sepd;
            idx_tmp = idx_tmp == idx_lowest_tmp ? nr_blanks - 1 : idx_tmp - 1;
          } else {  // idx_blank <= nr_blanks - 1
            if ( arr_blank_pos[idx_blank] != key_idx )
              goto label_8 ;

            if ( keys[key_idx] != 0 ) {
              idx_lowest_tmp--;
              arr_blank_pos[idx_lowest_tmp] = keys[key_idx];
              arr_blank_pos[nr_blanks + idx_lowest_tmp] = get_bits_sp(
                  indices, key_idx, idx_len_in_bits );
              shell_sort_u32_sepd_bits(
                  nr_blanks - idx_lowest_tmp,
                  arr_blank_pos + idx_lowest_tmp,
                  arr_blank_pos + idx_lowest_tmp + nr_blanks,
                  sizeof(*arr_blank_pos) * 8 );
              idx_tmp = nr_blanks - 1;
            }
            idx_blank--;
            keys[key_idx] = 0;
            set_bits_sp(indices, key_idx, idx_len_in_bits, ~0);
          }
        }
      }
      break;
    case 16/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
#define _template_(nr_bits)                                                                     \
        typeof(*keys) tmp;                                                                      \
        for ( bst_node_idx_t key_idx = nr_fields - 1,                                           \
            idx_blank = nr_blanks - 1, idx_tmp = nr_blanks - 1, idx_lowest_tmp = nr_blanks;     \
            key_idx <= nr_fields - 1;                                                           \
            key_idx-- ) {                                                                       \
          if ( idx_blank > nr_blanks - 1 ) {                                                    \
label_ ## nr_bits :                                                                             \
            tmp = keys[key_idx];                                                                \
            const bst_node_idx_t tmp_sepd = get_bits_sp(indices, key_idx, idx_len_in_bits);     \
            static_assert(sizeof(nr_fields) >= sizeof(uint32_t),                                \
                "There is a chance that it overflows!");                                        \
            keys[key_idx] = arr_blank_pos[idx_tmp];                                             \
            set_bits_sp(indices, key_idx, idx_len_in_bits, arr_blank_pos[nr_blanks + idx_tmp]); \
            arr_blank_pos[idx_tmp] = tmp;                                                       \
            arr_blank_pos[nr_blanks + idx_tmp] = tmp_sepd;                                      \
            idx_tmp = idx_tmp == idx_lowest_tmp ? nr_blanks - 1 : idx_tmp - 1;                  \
          } else {                                                                              \
            if ( arr_blank_pos[idx_blank] != key_idx )                                          \
              goto label_ ## nr_bits ;                                                          \
            if ( keys[key_idx] != 0 ) {                                                         \
              idx_lowest_tmp--;                                                                 \
              arr_blank_pos[idx_lowest_tmp] = keys[key_idx];                                    \
              arr_blank_pos[nr_blanks + idx_lowest_tmp] =                                       \
                get_bits_sp(indices, key_idx, idx_len_in_bits);                                 \
              shell_sort_u32_sepd_bits(                                                         \
                  nr_blanks - idx_lowest_tmp, arr_blank_pos + idx_lowest_tmp,                   \
                  arr_blank_pos + idx_lowest_tmp + nr_blanks, sizeof(*arr_blank_pos) * 8 );     \
              idx_tmp = nr_blanks - 1;                                                          \
            }                                                                                   \
            idx_blank--;                                                                        \
            keys[key_idx] = 0;                                                                  \
            set_bits_sp(indices, key_idx, idx_len_in_bits, ~0);                                 \
          }                                                                                     \
        }
        _template_(16)
      }
      break;
    case 32/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(32)
      }
      break;
    case 64/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(64)
#undef _template_
      }
      break;
    default:
      unreachable();
  }

  return node_idx;
}


//-------------------------------------------------------------------------------------------------
//__attribute__ ((target ("lzcnt, sse4.2")))
__attribute__ ((target ("lzcnt")))
static void shell_sort_tree( struct infos_bst * tree )
{
  bst_node_idx_t nr_fields = (2 << tree->max_active_depth) - 1;
  nr_fields |= (2 << 1) - 1;  // The minimum array length is 3
  void * restrict const indices = (void*)(tree + 1) + tree->ofs_idx_buf;
  const uint_fast8_t idx_len_in_bits = tree->height_cap + 1;  // The number of bits needed to
                                                              // represent each index

  // Sort them, the node with the smallest key goes first
  shell_sort_sepd_bits_halfauto( tree->key_len, nr_fields, tree->keys, indices, idx_len_in_bits );
  // Move each node to their place in tree
  const bst_node_idx_t nr_blanks = nr_fields - tree->nr_active_nodes;
  // Unless there's no need
  if ( nr_blanks == 0 || tree->nr_active_nodes == 0 )
    return;

  // Each slot of this buffer can hold a key or sepd(separate data connected to the key)
  // You can assign 1 more slot here to prevent .et_bits_sp() from touching out of bounds
  bst_node_idx_t arr_blank_pos[ nr_blanks * 2 ];
  for ( uint_fast32_t i = 0, j = nr_fields / 2; i < nr_blanks; i++, j-- ) {
    assume( j <= nr_fields / 2 );  // There should be no possibility of underflow here.

    // Designate the indices of tree->keys for blank keys
    assume( sizeof(nr_fields) == 32/8 || sizeof(nr_fields) == 64/8 );
    switch ( sizeof(nr_fields) ) {
      case 32/8:
        arr_blank_pos[i] = _reverse_field(j) >> ( sizeof(j)*8 - (32 - _lzcnt_u32(nr_fields)) );
        break;
      case 64/8:
        arr_blank_pos[i] = _reverse_field(j) >> ( sizeof(j)*8 - (64 - _lzcnt_u64(nr_fields)) );
        break;
      default:
        unreachable();
    }
  }
  // blank keys get ~0 as their linked slot in the separate array.
  memset( arr_blank_pos + nr_blanks, ~0, (nr_blanks) * sizeof(*arr_blank_pos) );

  // Sort
  shell_sort_auto( nr_blanks, arr_blank_pos );
  switch ( tree->key_len ) {
    case 8/8:
      {
        uint8_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        typeof(*keys) tmp;
        for ( bst_node_idx_t key_idx = nr_fields - 1,
            idx_blank = nr_blanks - 1, idx_tmp = nr_blanks - 1, idx_lowest_tmp = nr_blanks;
            key_idx <= nr_fields - 1;
            key_idx-- ) {
          if ( idx_blank > nr_blanks - 1 ) {
label_8 :
            tmp = keys[key_idx];
            const bst_node_idx_t tmp_sepd = get_bits_sp(indices, key_idx, idx_len_in_bits);
            static_assert( sizeof(nr_fields) >= sizeof(uint32_t),
                "There is a chance that it overflows!" );
            keys[key_idx] = arr_blank_pos[idx_tmp];
            set_bits_sp(indices, key_idx, idx_len_in_bits, arr_blank_pos[nr_blanks + idx_tmp]);
            arr_blank_pos[idx_tmp] = tmp;
            arr_blank_pos[nr_blanks + idx_tmp] = tmp_sepd;
            idx_tmp = idx_tmp == idx_lowest_tmp ? nr_blanks - 1 : idx_tmp - 1;
          } else {  // idx_blank <= nr_blanks - 1
            if ( arr_blank_pos[idx_blank] != key_idx )
              goto label_8;

            if ( keys[key_idx] != 0 ) {
              idx_lowest_tmp--;
              arr_blank_pos[idx_lowest_tmp] = keys[key_idx];
              arr_blank_pos[nr_blanks + idx_lowest_tmp] = get_bits_sp(
                  indices, key_idx, idx_len_in_bits );
              assume( sizeof(*arr_blank_pos) == 32/8 );
              shell_sort_u32_sepd_bits(
                  nr_blanks - idx_lowest_tmp,
                  arr_blank_pos + idx_lowest_tmp,
                  arr_blank_pos + idx_lowest_tmp + nr_blanks,
                  sizeof(*arr_blank_pos) * 8 );
              idx_tmp = nr_blanks - 1;
            }
            idx_blank--;
            keys[key_idx] = 0;
            set_bits_sp(indices, key_idx, idx_len_in_bits, ~0);
          }
        }
      }
      break;
    case 16/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint16_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
// to_another_byte_order(util_endian_BIG_ENDIAN, 0, &debug, sizeof(uint32_t));\
//
#define _template_(nr_bits)                                                                     \
        typeof(*keys) tmp;                                                                      \
        for ( bst_node_idx_t key_idx = nr_fields - 1,                                           \
            idx_blank = nr_blanks - 1, idx_tmp = nr_blanks - 1, idx_lowest_tmp = nr_blanks;     \
            key_idx <= nr_fields - 1;                                                           \
            key_idx-- ) {                                                                       \
          if ( idx_blank > nr_blanks - 1 ) {                                                    \
label_ ## nr_bits :                                                                             \
            tmp = keys[key_idx];                                                                \
            bst_node_idx_t tmp_sepd = get_bits_sp(indices, key_idx, idx_len_in_bits);           \
            static_assert(sizeof(nr_fields) >= sizeof(uint32_t),                                \
                "There is a chance that it overflows!");                                        \
            keys[key_idx] = arr_blank_pos[idx_tmp];                                             \
            set_bits_sp(indices, key_idx, idx_len_in_bits, arr_blank_pos[nr_blanks + idx_tmp]); \
            arr_blank_pos[idx_tmp] = tmp;                                                       \
            arr_blank_pos[nr_blanks + idx_tmp] = tmp_sepd;                                      \
            idx_tmp = idx_tmp == idx_lowest_tmp ? nr_blanks - 1 : idx_tmp - 1;                  \
          } else {                                                                              \
            if ( arr_blank_pos[idx_blank] != key_idx )                                          \
              goto label_ ## nr_bits ;                                                          \
            if ( keys[key_idx] != 0 ) {                                                         \
              idx_lowest_tmp--;                                                                 \
              arr_blank_pos[idx_lowest_tmp] = keys[key_idx];                                    \
              arr_blank_pos[nr_blanks + idx_lowest_tmp] =                                       \
                get_bits_sp(indices, key_idx, idx_len_in_bits);                                 \
              assume( sizeof(*arr_blank_pos) == 32/8 );                                         \
              shell_sort_u32_sepd_bits(                                                         \
                  nr_blanks - idx_lowest_tmp,                                                   \
                  arr_blank_pos + idx_lowest_tmp,                                               \
                  arr_blank_pos + idx_lowest_tmp + nr_blanks,                                   \
                  sizeof(*arr_blank_pos) * 8 );                                                 \
              idx_tmp = nr_blanks - 1;                                                          \
            }                                                                                   \
            idx_blank--;                                                                        \
            keys[key_idx] = 0;                                                                  \
            set_bits_sp(indices, key_idx, idx_len_in_bits, ~0);                                 \
          }                                                                                     \
        }
        _template_(16)
      }
      break;
    case 32/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint32_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(32)
      }
      break;
    case 64/8:
      {
        // The only one line that is different across the switch block except the goto labels.
        uint64_t * restrict const keys = __builtin_assume_aligned( tree->keys, ALIGN_FOR_AVX );
        _template_(64)
#undef _template_
      }
      break;
    default:
      unreachable();
  }
}


//-------------------------------------------------------------------------------------------------
#if defined(__clang__)
__attribute__ ((minsize))
#elif defined(__GNUC__) || defined(__GNUG__)
__attribute__ ((optimize("s")))
#endif
void init_infos_tree(uint64_t proc_info) {
  union ucl_x86_proc_info x86_proc_info;
  x86_proc_info.whole = proc_info;

  [[maybe_unused]] uint8_t alignment;
#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64)\
  || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
  alignment = x86_proc_info.avx ? ALIGN_FOR_AVX : ALIGN_FOR_SSE;
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM)
  alignment = ALIGN_FOR_NEON;
#endif

  // infos_bst_create_impl = alignment == 32 ? infos_bst_create_256 : infos_bst_create_128;
  infos_bst_create_impl = infos_bst_create_default;

}


