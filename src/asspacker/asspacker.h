#if !defined ASSPACKER_H_
#define ASSPACKER_H_


#include <stdint.h>


enum salumi_pack_flag : unsigned char {
  salumi_pack_includes_astc = 0x0001,
  salumi_pack_includes_s3tc = 0x0002,
};

enum todo_replace_it : unsigned char {
  asset_type_diffuse_map = 1,
};

enum compression_algorithm : unsigned char {
  compression_algorithm_none = 0,
  compression_algorithm_snappy,
  compression_algorithm_lzma,
};

//extern uint64_t salumi_pack_signature;
// Every data should be written in little-endian order in a pack file.
constexpr uint64_t salumi_pack_signature = (uint64_t)0x05a1 << (8-2)*8 | (uint64_t)'u' << (8-3)*8 |
  (uint64_t)'m' << (8-4)*8 | (uint64_t)'i' << (8-5)*8 | (uint64_t)'p' << (8-6)*8 |
  (uint64_t)0xac << (8-7)*8 | (uint64_t)'k';

struct salumi_pack_header {
  uint64_t signature;
  uint32_t nr_assets;
  uint16_t version_code;
  uint16_t flags;
};
//struct salumi_pack_pseudo_header {
//  uint8_t signature[sizeof(salumi_pack_magic_number)];
//  uint8_t nr_assets[__sizeof_member(struct salumi_pack_header, nr_assets)];
//  uint8_t version_code[__sizeof_member(struct salumi_pack_header, version_code)];
//  uint8_t flags[__sizeof_member(struct salumi_pack_header, flags)];
//};



#endif  // ASSPACKER_H_
