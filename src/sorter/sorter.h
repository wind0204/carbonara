#if !defined SORTER_H_
#define SORTER_H_

#include <stdint.h>


//-------------------------------------------------------------------------------------------------
//void sort_sorted_array_inserted
//void sort_sorted_array_inserted_arr
//void sort_sorted_array_inserted_sepd_bits
//void sort_sorted_array_inserted_arr_sepd_bits
//void sort_sorted_array_deleted
//void sort_sorted_array_deleted_arr
//void sort_sorted_array_deleted_sepd_bits
//void sort_sorted_array_deleted_arr_sepd_bits
//void sort_sorted_array_mod
//void sort_sorted_array_mod_arr
//void sort_sorted_array_mod_sepd_bits
//void sort_sorted_array_mod_arr_sepd_bits


//-------------------------------------------------------------------------------------------------
void insert_sort_u64(uint32_t nr_items, void * data);
void insert_sort_u32(uint32_t nr_items, void * data);
void insert_sort_u16(uint32_t nr_items, void * data);
void insert_sort_u8(uint32_t nr_items, void * data);


//-------------------------------------------------------------------------------------------------
/**
 * \brief Do an insert sort on \a data and swap the bitfields in \a sep_data accordingly
 * \a sep_data It is an array of bitfields 
 * \warning The function sometimes accesses beyond \a sep_data due to the nature of get_bits_sp.*
 * \warning The function sometimes issues OR with 0 or AND with ~0 beyond \a sep_data due to the
 *  nature of set_bits_sp.*
 */
void insert_sort_u64_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
void insert_sort_u32_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
void insert_sort_u16_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
void insert_sort_u8_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
uint32_t insert_sort_u64_sepd_bits_track(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx);
uint32_t insert_sort_u32_sepd_bits_track(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx);
uint32_t insert_sort_u16_sepd_bits_track(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx);
uint32_t insert_sort_u8_sepd_bits_track(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits, uint32_t node_idx);


//-------------------------------------------------------------------------------------------------
//void init_sorter(void);
/**
 * \brief shell sort using Tokuda's sequence
 * \a data An array of keys in which each key is uint8_t
 */
void shell_sort_u8(uint32_t nr_items, void * data);
/**
 * \brief shell sort using Tokuda's sequence
 * \a data An array of keys in which each key is uint16_t
 */
void shell_sort_u16(uint32_t nr_items, void * data);
/**
 * \brief shell sort using Tokuda's sequence
 * \a data An array of keys in which each key is uint32_t
 */
void shell_sort_u32(uint32_t nr_items, void * data);
/**
 * \brief shell sort using Tokuda's sequence
 * \a data An array of keys in which each key is uint64_t
 */
void shell_sort_u64(uint32_t nr_items, void * data);
void shell_sort_reverse_u32_u32(uint32_t nr_items, void * data);
#define shell_sort_auto(nr_items, data) \
  switch ( sizeof(*(data)) ) {\
    case 8/8:\
      shell_sort_u8((nr_items), (data));\
      break;\
    case 16/8:\
      shell_sort_u16((nr_items), (data));\
      break;\
    case 32/8:\
      shell_sort_u32((nr_items), (data));\
      break;\
    case 64/8:\
      shell_sort_u64((nr_items), (data));\
      break;\
    default:\
      unreachable();\
  }


//-------------------------------------------------------------------------------------------------
/**
 * \brief Do a Shell sort on \a data and swap the bitfields in \a sep_data accordingly
 * \a sep_data It is an array of bitfields 
 * \warning The function sometimes accesses beyond \a sep_data due to the nature of get_bits_sp.*
 * \warning The function sometimes issues OR with 0 or AND with ~0 beyond \a sep_data due to the
 *  nature of set_bits_sp.*
 */
void shell_sort_u64_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
void shell_sort_u32_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
void shell_sort_u16_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
void shell_sort_u8_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);
#define shell_sort_sepd_bits_auto(nr_items, data, sep_data, len_sepd_in_bits) \
  switch ( sizeof(*(data)) ) {\
    case 8/8:\
      shell_sort_u8_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    case 16/8:\
      shell_sort_u16_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    case 32/8:\
      shell_sort_u32_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    case 64/8:\
      shell_sort_u64_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    default:\
      unreachable();\
  }
#define shell_sort_sepd_bits_halfauto(key_len, nr_items, data, sep_data, len_sepd_in_bits) \
  switch ( key_len ) {\
    case 8/8:\
      shell_sort_u8_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    case 16/8:\
      shell_sort_u16_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    case 32/8:\
      shell_sort_u32_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    case 64/8:\
      shell_sort_u64_sepd_bits(nr_items, data, sep_data, len_sepd_in_bits);\
      break;\
    default:\
      unreachable();\
  }

//-------------------------------------------------------------------------------------------------
void shell_shell_p256_u64_u64_sepd_bits(uint32_t nr_items, void * restrict data,
    void * restrict sep_data, uint8_t len_sepd_in_bits);


/**
 * \a sepd_checker A function that checks the separate data associated with a matching key.
 *    It shall return 0 when the data is not what the user is looking for.
 * \warning Unimplemented yet, why did I want to have such a function??
 */
//uint8_t shell_sort_p128_u32_u32_sepd_bits_with_checker(uint32_t nr_items, void * data, void * sep_data, uint8_t len_sepd_in_bits, uint8_t (*sepd_checker)(void* data));


#endif
