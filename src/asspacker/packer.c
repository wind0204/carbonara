/*
 * This code works on little-endian POSIX systems only.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <regex.h>
#include <unistd.h>

#include "util-c_lang-basic.h"
#include "sorter.h"
#include "appdev_support-file.h"
#include "info_struct.h"

#include "asspacker.h"
#include "carbonara-asset.h"

enum options : unsigned char {
  option_output = 'o',
  option_csv = 'c',
  option_texture_format = 't',
};

struct salumi_pack_ass_index {
  uint32_t asset_id;
  uint32_t offset;
};

struct offs_n_len {
  uint8_t offset[3];
  uint8_t length[3];
};

struct salumi_pack_ass_header {
  uint8_t compression_algorithm;
  uint8_t nr_subassets;
};
//struct offs_n_len[];

struct salumi_pack_subass_tex2d_header {
  uint8_t nr_mip_lvls;
  uint8_t highest_mip_level;
};
//struct offs_n_len[];

struct astc_info {
  uint8_t blockdim_x;
  uint8_t blockdim_y;
};
struct tex2d_info {
  uint8_t xsize[3];
  uint8_t ysize[3];
};

static regex_t arr_regex_structs[3];
static regex_t * regex_three_chan_comp_filename;
static regex_t * regex_two_chan_comp_filename;
static regex_t * regex_lod_number_in_filename;
static uint8_t texture_format = 0;
struct astc_header {
  uint8_t magic[4];
  uint8_t blockdim_x;
  uint8_t blockdim_y;
  uint8_t blockdim_z;
  uint8_t xsize[3];
  uint8_t ysize[3];
  uint8_t zsize[3];
};

static int ret;

unsigned int get_nr_lines_in_csv(FILE * csv_file);







int main(int argc, char * argv[]) {
  char * path_csv = 0;
  char * path_output = 0;

  for ( uint_fast8_t idx_arg = 1, current_option = 0; idx_arg < argc; idx_arg++ ) {
    if ( current_option ) {
      switch ( current_option ) {
        case option_output:
          path_output = argv[idx_arg];
          break;
        case option_csv:
          path_csv = argv[idx_arg];
          break;
        case option_texture_format:
          switch ( argv[idx_arg][0] ) {
            case 'a':
            case 'A':
              texture_format = tf_astc;
              break;
            case 's':
            case 'S':
              texture_format = tf_s3tc;
              break;
            default:
              fprintf(stderr, "Unknown texture format: %s\n", argv[idx_arg]);
              return 1;
          }
          break;
      }

      current_option = 0;

    } else {
      if ( argv[idx_arg][0] != '-' ) {
print_usage_and_quit:;
        fprintf(stderr, "Usage: %s [options]\n\nOPTIONS\n  -o {path to the output file}\n"
            "  -c {path to the csv file}\n  -t {astc|s3tc}\n", argv[0]);
        return 1;
      }

      switch ( argv[idx_arg][1] ) {
        case 'o':
        case 'c':
        case 't':
          current_option = argv[idx_arg][1];
          break;
        default:
          fprintf(stderr, "Unknown option: %s\n", argv[idx_arg]);
          return 1;
      }
    }
  }

  if ( !path_output || !path_csv || !texture_format ) {
    goto print_usage_and_quit;
  }
  char base_dir[256];
  char pathbuf[256];
  uint32_t idx_last_slash = get_idx_of_nth_char(path_csv, '/', -1);
  uint32_t len_base_dir = idx_last_slash;
  if ( idx_last_slash == UINT32_MAX ) {
    if ( !getcwd(base_dir, 256) ) {
      fprintf(stderr, "an error occurred in getcwd\n");
      return 1;
    }
  } else if ( idx_last_slash == 0 ) {
    fprintf(stderr, "path_csv: %s\n", path_csv);
    return 1;
  } else {
    memcpy(base_dir, path_csv, idx_last_slash);
    base_dir[idx_last_slash] = 0;

    // I guess we'll use pathbuf only for base_directory
    memcpy(pathbuf, base_dir, idx_last_slash);
    pathbuf[idx_last_slash] = '/';
  }

  FILE * csv_file, * output_file;
  csv_file = fopen(path_csv, "r");
  if ( !csv_file ) {
    fprintf(stderr, "Failed to open file %s\n", path_csv);
    return 1;
  }
  output_file = fopen(path_output, "w");
  if ( !output_file ) {
    fprintf(stderr, "Failed to open file %s\n", path_output);
    fclose(csv_file);
    return 1;
  }

  //uint32_t tmp_buf_len = 4096;
  char * tmp_buffer = malloc(4096);
  if ( !tmp_buffer ) {
out_of_memory:;
    fprintf(stderr, "Out of memory\n");
    fclose(csv_file);
    fclose(output_file);
    return 1;
  }
  char * buffer = malloc(8192);
  if ( !buffer ) {
    goto out_of_memory;
  }
  char * end_cap = buffer + 8192 - 1;

  // Ignore the first line
  fgets(buffer, 8192, csv_file);
  long int file_position = ftell(csv_file);

  uint32_t nr_assets = get_nr_lines_in_csv(csv_file);
  if ( nr_assets < 2 ) {
    fprintf(stderr, "get_nr_lines_in_csv returned %u, which should be greater than 1.\n", nr_assets);
    return 1;
  }
  nr_assets--;

  struct salumi_pack_header header;
  header.signature = salumi_pack_signature;
  header.nr_assets = nr_assets;
  header.version_code = 1;
  header.flags = texture_format;
  if ( fwrite(&header, sizeof(header), 1, output_file) != 1 ) {
failed_to_write:;
    fprintf(stderr, "Failed to write on file %s\n", path_output);
    fclose(csv_file);
    fclose(output_file);
    return 1;
  }

  uint32_t next_ass_offset = sizeof(header) +
    nr_assets * sizeof(struct salumi_pack_ass_index);
  if ( fseek(output_file, next_ass_offset, SEEK_SET) != 0 ) {
failed_to_fseek:;
    //fprintf(stderr, "fseek() failed in file %s at line # %u\n", __FILE__, __LINE__);
    fprintf(stderr, "fseek() failed\n");
    fclose(csv_file);
    fclose(output_file);
    return 1;
  }


  regex_lod_number_in_filename = arr_regex_structs + 2;
  ret = regcomp(regex_lod_number_in_filename, ".+_mip_([0-9]+)\\.[^.]+$", REG_EXTENDED );
  if ( ret ) {
failed_to_compile_regex:;
    fprintf(stderr, "Failed to compile a regular expression\n");
    fclose(csv_file);
    fclose(output_file);
    return 1;
  }
  switch ( texture_format ) {
    case tf_astc:
      regex_three_chan_comp_filename = arr_regex_structs + 0;
      regex_two_chan_comp_filename = arr_regex_structs + 0;
      ret = regcomp(arr_regex_structs, ".+\\.astc$", REG_NOSUB | REG_EXTENDED );
      if ( ret ) {
        goto failed_to_compile_regex;
      }
      break;
    case tf_s3tc:
      regex_three_chan_comp_filename = arr_regex_structs + 0;
      regex_two_chan_comp_filename = arr_regex_structs + 1;
      ret = regcomp(regex_three_chan_comp_filename, ".+\\.bc[137]$", REG_NOSUB | REG_EXTENDED );
      if ( ret ) goto failed_to_compile_regex;
      ret = regcomp(regex_two_chan_comp_filename, ".+\\.bc5$", REG_NOSUB | REG_EXTENDED );
      if ( ret ) goto failed_to_compile_regex;
      break;
    default:
      unreachable();
  }


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
  struct infos_arr_list * al_ass_indices = infos_al_create(
      NULL, NULL, 4096, sizeof(struct salumi_pack_ass_index), 0
      );
#pragma GCC diagnostic pop

  uint_fast32_t nr_ass_indices = 0;

  //struct infos_growing_mem_manager * gmm = infos_gmm_init(0, 0, 1024*128);
  struct infos_singly_linked_list buffer_list;
  buffer_list.head = malloc(1024*128 + sizeof(struct infos_singly_linked_list_node));
  if ( buffer_list.head == 0 ) {
    goto out_of_memory;
  }
  buffer_list.nr_nodes = 1;
  buffer_list.head->next = 0;
  buffer_list.head->data = buffer_list.head + 1;
  static_assert( alignof(typeof(buffer_list)) >= alignof(struct infos_singly_linked_list_node),
      "should be aligned." );


  long last_file_position = 0;
  uint8_t is_at_eof = 0;
  char * cursor = end_cap;
  while(-7) {
stream_more:;
    if ( is_at_eof ) {
      break;
    }
    file_position = file_position - (end_cap - cursor);
    if ( fseek(csv_file, file_position, SEEK_SET) != 0 ) {
      goto failed_to_fseek;
    }

    if ( last_file_position == file_position ) {
      fprintf(stderr, "infinite looping was detected.\n");
      fclose(csv_file);
      fclose(output_file);
      return 1;
    }
    last_file_position = file_position;

    uint_fast32_t chunk_len, len_new_data;
    memcpy(buffer, cursor, end_cap - cursor);
    len_new_data = end_cap - buffer - (end_cap - cursor);
    chunk_len = fread(buffer + (end_cap - cursor), 1, len_new_data, csv_file);
    is_at_eof = feof(csv_file);
    if ( chunk_len != len_new_data ) {
      if ( !is_at_eof ) {
        fprintf(stderr, "Failed to read file %s\n", path_csv);
        fclose(csv_file);
        fclose(output_file);
        return 1;
      }
    }
    file_position += chunk_len;
    chunk_len += (end_cap - cursor);
    cursor = buffer;

    buffer[chunk_len] = 0;
    while(-7) {
      char * local_cursor = cursor;
      uint_fast32_t loc_separator;

      loc_separator = get_idx_of_char(local_cursor, ',');
      if ( loc_separator == UINT32_MAX ) break;
      local_cursor[loc_separator] = 0;
      uint32_t asset_id = atoi(local_cursor);
      if ( asset_id == 0 ) {
        fprintf(stderr, "Wrong asset id (%s)\n", local_cursor);
        fclose(csv_file);
        fclose(output_file);
        return 1;
      }
      local_cursor = local_cursor + loc_separator + 1;

      loc_separator = get_idx_of_char(local_cursor, ',');
      if ( loc_separator == UINT32_MAX ) break;
      local_cursor[loc_separator] = 0;
      uint8_t type;
      if ( !strcmp(local_cursor, "diffuse_map") ) {
        type = asset_type_diffuse_map;
      //} else if ( !strcmp(local_cursor, "audio_mono") ) {
      //  type = asset_type_audio_mono;
      } else {
        fprintf(stderr, "Unknown asset type (%s)\n", local_cursor);
        fclose(csv_file);
        fclose(output_file);
        return 1;
      }
      local_cursor = local_cursor + loc_separator + 1;

      loc_separator = get_idx_of_char(local_cursor, ',');
      if ( loc_separator == UINT32_MAX ) break;
      local_cursor[loc_separator] = 0;
      uint8_t comp_alg;
      if ( !strcmp(local_cursor, "snappy") ) {
        comp_alg = compression_algorithm_snappy;
      } else if ( !strcmp(local_cursor, "lzma") ) {
        comp_alg = compression_algorithm_lzma;
      } else if ( !strcmp(local_cursor, "none") || !strcmp(local_cursor, "0") ) {
        comp_alg = compression_algorithm_none;
      } else {
        fprintf(stderr, "Wrong compression algorithm (%s)\n", local_cursor);
        fclose(csv_file);
        fclose(output_file);
        return 1;
      }
      local_cursor = local_cursor + loc_separator + 1;

      switch ( type ) {
        case asset_type_diffuse_map:
          {
            char * cache_dir_path;
            uint8_t cache_dir_path_len;
            DIR * cache_dir;
            struct dirent * entry;
            switch ( texture_format ) {
              case tf_astc:
                loc_separator = get_idx_of_char(local_cursor, ',');
                if ( loc_separator == UINT32_MAX ) {
                  goto stream_more;
                }
                local_cursor[loc_separator] = 0;
                cache_dir_path = local_cursor;
                cache_dir_path_len = loc_separator;
                local_cursor = local_cursor + loc_separator + 1;
                memcpy(pathbuf + len_base_dir + 1, cache_dir_path, cache_dir_path_len + 1);

                // Make a list of astc levels
                cache_dir = opendir(pathbuf);
                if ( !cache_dir ) {
                  fprintf(stderr, "Could not open the directory %s\n", pathbuf);
                  fclose(csv_file);
                  fclose(output_file);
                  return 1;
                }
                uint_fast8_t nr_astc_files = 0;
                char basename[_sizeof_member(struct dirent, d_name)];
                while ( (entry = readdir(cache_dir)) ) {
                  regmatch_t arr_regmatches[2];
                  ret = regexec(regex_three_chan_comp_filename, entry->d_name, 0, 0, 0);
                  if ( ret == 0 ) {
                    uint_fast8_t lod;
                    ret = regexec(regex_lod_number_in_filename, entry->d_name, 2, arr_regmatches, 0);
                    if ( ret ) {
                      lod = 0;

                      // Capture the base name
                      uint_fast8_t string_len = get_idx_of_null(entry->d_name, 1);
                      uint_fast8_t idx_char;
                      for ( idx_char = string_len-1; ; idx_char--) {
                        if ( entry->d_name[idx_char] == '.' ) {
                          break;
                        }
                      }
                      memcpy(basename, entry->d_name, idx_char);
                      basename[idx_char] = 0;
                    } else {
                      char number[sizeof(int)];
                      uint_fast8_t string_len = arr_regmatches[1].rm_eo - arr_regmatches[1].rm_so;
                      memcpy(number, &entry->d_name[arr_regmatches[1].rm_so], string_len);
                      number[string_len] = 0;
                      lod = atoi(number);
                    }

                    // No one will see that much number of levels
                    //if ( len_tmp_buffer < sizeof(lod) * nr_astc_files ) {
                    //  len_tmp_buffer += 4096;
                    //  tmp_buffer = realloc(tmp_buffer, len_tmp_buffer);
                    //  if ( tmp_buffer == 0 ) {
                    //    goto out_of_memory;
                    //  }
                    //}
                    uint8_t * arr_lod = (void*) tmp_buffer;
                    arr_lod[nr_astc_files] = lod;
                    nr_astc_files++;
                  }
                }
                closedir(cache_dir);

                if ( nr_astc_files == 0 ) {
                  fprintf(stderr, "Couldn't find any astc file in %s\n", pathbuf);
                  fclose(csv_file);
                  fclose(output_file);
                  return 1;
                }

                uint8_t * arr_lod = (void*) tmp_buffer;
                char * tmp_buffer2 = tmp_buffer + nr_astc_files;
                shell_sort_u8(nr_astc_files, arr_lod);
                uint_fast8_t is_in_order = 1;
                for ( uint_fast8_t idx_astc_file = 1;
                    idx_astc_file < nr_astc_files; idx_astc_file++ ) {
                  if ( arr_lod[idx_astc_file] <= arr_lod[idx_astc_file-1] ) {
                    is_in_order = 0;
                  }
                }
                if ( !is_in_order ) {
                  //memcpy(pathbuf + len_base_dir + 1, path_cache_dir, len_path_cache_dir + 1);
                  fprintf(stdout, "The mip levels in %s are not in order!  LoD's:", pathbuf);
                  for ( uint_fast8_t i = 0; i < nr_astc_files; i++ ) {
                    fprintf(stdout, " %u", arr_lod[i]);
                  }
                  fprintf(stdout, "\n");
                  fclose(csv_file);
                  fclose(output_file);
                  return 1;
                }


                // Obtain the dimensions from the file of lod level 0.
                //char * tmp_buffer2 = tmp_buffer + nr_astc_files;
                uint_fast16_t basename_len = get_idx_of_null(basename, 1);
                //memcpy(tmp_buffer2, basename, len_basename);
                //memcpy(tmp_buffer2 + len_basename, ".astc", sizeof(".astc"));
                pathbuf[len_base_dir + 1 + cache_dir_path_len] = '/';
                memcpy(pathbuf + len_base_dir + 1 + cache_dir_path_len + 1,
                    basename, basename_len);
                memcpy(pathbuf + len_base_dir + 1 + cache_dir_path_len + 1 + basename_len,
                    ".astc", sizeof(".astc"));
                FILE * cache_file;
                cache_file = fopen(pathbuf, "r");
                if ( !cache_file ) {
                  fprintf(stderr, "Failed to open file %s\n", tmp_buffer2);
                  fclose(csv_file);
                  fclose(output_file);
                  return 1;
                }
                uint_fast32_t il_chunk_len;
                if ( (il_chunk_len = fread(tmp_buffer2, sizeof(struct astc_header), 1, cache_file)) != 1 ) {
                  if ( feof(cache_file) ) {
                    fprintf(stderr, "Failed to read the header data from file %s\n", tmp_buffer2);
                  } else {
                    fprintf(stderr, "Failed to read file %s\n", tmp_buffer2);
                  }
                  fclose(csv_file);
                  fclose(output_file);
                  fclose(cache_file);
                  return 1;
                }

                struct astc_header * il_header = (void*) tmp_buffer2;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
                if ( *(uint32_t*)il_header->magic != 0x5ca1ab13u ) {
                  fprintf(stderr, "incorrect magic number 0x%8x, it should be 0x%8x\n",
                      *(uint32_t*)il_header->magic, 0x5ca1ab13u);
#pragma GCC diagnostic pop
                  fclose(csv_file);
                  fclose(output_file);
                  fclose(cache_file);
                  return 1;
                }
                fclose(cache_file);

                uint8_t blockdim_x = il_header->blockdim_x;
                uint8_t blockdim_y = il_header->blockdim_y;
                uint32_t size_x = 0, size_y = 0;
                memcpy( (void*)&size_x, il_header->xsize, sizeof(il_header->xsize) );
                memcpy( (void*)&size_y, il_header->ysize, sizeof(il_header->ysize) );

                uint32_t * arr_len_texture_data = (void*) tmp_buffer2;
                _align_ptr(tmp_buffer2);
                // Obtain the sizes of the files in path_to_cache
                for ( uint_fast8_t idx_lod = 0; idx_lod < nr_astc_files; idx_lod++ ) {
                  //char * tmp_buffer3 = tmp_buffer2 + sizeof(*arr_len_texture_data) * nr_astc_files;

                  //memcpy(tmp_buffer3, basename, len_basename);
                  //memcpy(tmp_buffer3 + len_basename, "_mip_", sizeof("_mip_")-1);
                  //itoa(arr_lod[idx_lod], tmp_buffer3 + len_basename + sizeof("_mip_") - 1);
                  pathbuf[len_base_dir + 1 + cache_dir_path_len + 1 + basename_len] = 0;
                  if ( arr_lod[idx_lod] ) {
                    memcpy(pathbuf + len_base_dir + 1 + cache_dir_path_len + 1 + basename_len,
                        "_mip_", sizeof("_mip_") - 1);
                    itoa(arr_lod[idx_lod],
                        pathbuf + len_base_dir + 1 + cache_dir_path_len + 1 + basename_len + sizeof("_mip_") - 1);
                  }
                  uint16_t length = get_idx_of_null(pathbuf, 1);
                  memcpy(pathbuf + length, ".astc", sizeof(".astc"));

                  if ( (arr_len_texture_data[idx_lod] = get_file_size(pathbuf)) == 0 ) {
                    fprintf(stderr, "Failed to obtain the size of a file(%s)\n", pathbuf);
                    fclose(csv_file);
                    fclose(output_file);
                    return 1;
                  }
                  arr_len_texture_data[idx_lod] -= sizeof(struct astc_header);
                }

                // Group files into groups of size equal to or bigger than 32KiB
                uint32_t * arr_group_size = (void*)( arr_len_texture_data + nr_astc_files );
                _align_ptr(arr_group_size);
                uint8_t * arr_nr_grp_membs = (void*)( arr_group_size + nr_astc_files );
                uint_fast16_t bucket_size;
                switch ( comp_alg ) {
                  case compression_algorithm_none:
                    bucket_size = 1024 * 32;
                    break;
                  default:
                    bucket_size = 1024 * 64;
                    break;
                }
                uint_fast8_t nr_grps = 0;
                for ( uint_fast8_t idx_group = 0, idx_lod = nr_astc_files - 1;
                    idx_lod != UINT_FAST8_MAX; idx_group++ ) {
                  nr_grps++;
                  arr_nr_grp_membs[idx_group] = 0;
                  for ( typeof(*arr_len_texture_data) group_size = 0; idx_lod != UINT_FAST8_MAX; ) {
                    group_size += arr_len_texture_data[idx_lod];
                    arr_nr_grp_membs[idx_group]++;
                    idx_lod--;
                    if ( group_size >= bucket_size ) {
                      arr_group_size[idx_group] = group_size;
                      break;
                    }
                  }
                }


                if ( nr_ass_indices >=
                    ((1 << al_ass_indices->log2_block_size) / sizeof(struct salumi_pack_ass_index)) *
                    al_ass_indices->active_block_cnt ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
                  infos_al_grow( al_ass_indices, nullptr, nullptr, nullptr, nullptr, 1, false, 0 );
#pragma GCC diagnostic pop
                }

                struct salumi_pack_ass_index * ass_index = infos_al_get_elem_unsafe(
                    al_ass_indices, nr_ass_indices );
                ass_index->asset_id = asset_id;
                ass_index->offset = next_ass_offset;

                nr_ass_indices++;


                struct salumi_pack_ass_header ass_header;
                ass_header.compression_algorithm = comp_alg;
                ass_header.nr_subassets = nr_grps;
                if ( fwrite(&ass_header, sizeof(ass_header), 1, output_file) != 1 ) {
                  goto failed_to_write;
                }
                struct offs_n_len * restrict arr_offs_n_len = (void*)( arr_nr_grp_membs + nr_grps );
                uint32_t * restrict arr_offs_in_uncompressed_data = (void*)( arr_offs_n_len + nr_astc_files );
                _align_ptr(arr_offs_in_uncompressed_data);
                uint_fast32_t tally = sizeof(struct salumi_pack_ass_header) +
                  sizeof(struct offs_n_len) * nr_grps +
                  sizeof(struct tex2d_info) + sizeof(struct astc_info) +
                  sizeof(struct salumi_pack_subass_tex2d_header) * nr_grps +
                  sizeof(uint32_t) * nr_astc_files;
                for ( uint_fast8_t idx_group = 0; idx_group < nr_grps; idx_group++ ) {
                  // TODO implement Snappy and LZMA support; utilize buffer_list
                  memcpy(arr_offs_n_len[idx_group].length,
                      &arr_group_size[idx_group], sizeof(arr_offs_n_len[0].length));
                  memcpy(arr_offs_n_len[idx_group].offset,
                      &tally, sizeof(arr_offs_n_len[0].offset));
                  tally += arr_group_size[idx_group];
                }
                next_ass_offset += tally;

                if ( fwrite(arr_offs_n_len, sizeof(*arr_offs_n_len), nr_grps, output_file)
                    != nr_grps ) {
                  goto failed_to_write;
                }
                struct tex2d_info tex2d_specific;
                memcpy(tex2d_specific.xsize, &size_x, sizeof(tex2d_specific.xsize));
                memcpy(tex2d_specific.ysize, &size_y, sizeof(tex2d_specific.ysize));
                if ( fwrite(&tex2d_specific, sizeof(tex2d_specific), 1, output_file) != 1 ) {
                  goto failed_to_write;
                }
                struct astc_info astc_specific;
                astc_specific.blockdim_x = blockdim_x;
                astc_specific.blockdim_y = blockdim_y;
                if ( fwrite(&astc_specific, sizeof(astc_specific), 1, output_file) != 1 ) {
                  goto failed_to_write;
                }

                for ( uint_fast8_t idx_group = 0, mip_level = nr_astc_files - 1;
                    idx_group < nr_grps; idx_group++ ) {
                  struct salumi_pack_subass_tex2d_header tex2d_header;
                  tex2d_header.highest_mip_level = mip_level;
                  tex2d_header.nr_mip_lvls = arr_nr_grp_membs[idx_group];
                  if ( fwrite(&tex2d_header, sizeof(tex2d_header), 1, output_file) != 1 ) {
                    goto failed_to_write;
                  }
                  tally = 0;
                  for ( uint_fast8_t idx_lod = mip_level;
                      idx_lod != (uint_fast8_t)(mip_level - arr_nr_grp_membs[idx_group]); idx_lod-- ) {
                    // TODO implement Snappy and LZMA support
                    arr_offs_in_uncompressed_data[idx_lod] = tally;
                    tally += arr_len_texture_data[idx_lod];
                  }
                  mip_level -= arr_nr_grp_membs[idx_group];
                }

                if ( fwrite(arr_offs_in_uncompressed_data,
                      sizeof(*arr_offs_in_uncompressed_data),
                      nr_astc_files,
                      output_file) != nr_astc_files ) {
                  goto failed_to_write;
                }

                switch( comp_alg ) {
                  case compression_algorithm_none:
                    // Read every texture data, and write the content except headers
                    for ( uint_fast8_t idx_lod =  nr_astc_files-1; idx_lod != UINT_FAST8_MAX; idx_lod-- ) {
                      //char * tmp_buffer3 = tmp_buffer2 + sizeof(*arr_len_texture_data) * nr_astc_files;

                      //memcpy(tmp_buffer3, basename, len_basename);
                      //memcpy(tmp_buffer3 + len_basename, "_mip_", sizeof("_mip_")-1);
                      //itoa(arr_lod[idx_lod], tmp_buffer3 + len_basename + sizeof("_mip_") - 1);

                      pathbuf[len_base_dir + 1 + cache_dir_path_len + 1 + basename_len] = 0;
                      if ( arr_lod[idx_lod] ) {
                        memcpy(pathbuf + len_base_dir + 1 + cache_dir_path_len + 1 + basename_len,
                            "_mip_", sizeof("_mip_") - 1);
                        itoa(arr_lod[idx_lod],
                            pathbuf + len_base_dir + 1 + cache_dir_path_len + 1 + basename_len + sizeof("_mip_") - 1);
                      }
                      uint16_t length = get_idx_of_null(pathbuf, 1);
                      memcpy(pathbuf + length, ".astc", sizeof(".astc"));

                      FILE * input_file;
                      input_file = fopen(pathbuf, "r");
                      if ( !input_file ) {
                        fprintf(stderr, "Failed to open file %s\n", pathbuf);
                        fclose(csv_file);
                        fclose(output_file);
                        return 1;
                      }
                      if ( fseek(input_file, sizeof(struct astc_header), SEEK_SET) != 0 ) {
                        goto failed_to_fseek;
                      }

                      while(-7) {
                        uint_fast32_t il_il_chunk_len = fread(buffer_list.head->data, 1, 1024*128, input_file);
                        uint_fast8_t il_is_at_eof = feof(input_file);
                        if ( il_il_chunk_len != 1024*128 ) {
                          if ( !il_is_at_eof ) {
                            fprintf(stderr, "Failed to read file %s\n", pathbuf);
                            fclose(input_file);
                            fclose(csv_file);
                            fclose(output_file);
                            return 1;
                          }
                        }
                        fwrite(buffer_list.head->data, il_il_chunk_len, 1, output_file);
                        //fprintf(stdout, "wrote %ld bytes.\n", len_chunk);
                        if ( il_is_at_eof ) {
                          break;
                        }
                      }
                    }
                    break;

                  default:
                    // TODO implement Snappy and LZMA support; utilize buffer_list
                    break;
                }

                break;

              case tf_s3tc:
                // TODO support s3tc texture format
                break;
            }
            break;
          }

        //case asset_type_audio_mono:
        //  {
        //  }
        //  break;
      }

      loc_separator = get_idx_of_char(local_cursor, '\n');
      if ( loc_separator == UINT32_MAX ) break;
      cursor = local_cursor + loc_separator + 1;
    }
  }


  // Write asset indices
  if ( fseek(output_file, sizeof(header), SEEK_SET) != 0 ) {
    goto failed_to_fseek;
  }
  struct linked_list_of_lists_node * node = &al_ass_indices->list.head;
  while (-7) {
    for ( unsigned int data_idx = 0; data_idx < _arr_elem_cnt(node->arr_blocks); data_idx++ ) {
      if ( !node->arr_blocks[data_idx] ) {
        goto out_of_nested_loop;
      }
      uint32_t nr_elems;
      if ( nr_ass_indices >=
          (1 << al_ass_indices->log2_block_size) / sizeof(struct salumi_pack_ass_index) ) {
        nr_elems = (1 << al_ass_indices->log2_block_size) / sizeof(struct salumi_pack_ass_index);
      } else {
        nr_elems = nr_ass_indices;
      }

      if ( fwrite(node->arr_blocks, sizeof(struct salumi_pack_ass_index), nr_elems, output_file) !=
          nr_elems ) {
        goto failed_to_write;
      }
      nr_ass_indices -= nr_elems;
    }
  }
out_of_nested_loop:

  fclose(csv_file);
  fclose(output_file);
  return 0;
}







unsigned int get_nr_lines_in_csv(FILE * csv_file) {
  uint_fast32_t nr_lines = 0;
  char * buffer = malloc(8192);
  if ( !buffer ) {
//out_of_memory:;
    fprintf(stderr, "Out of memory\n");
    return 0;
  }
  char * end_cap = buffer + 8192 - 1;
  char * cursor = buffer;

  long int orig_file_position = ftell(csv_file);
  long int file_position = orig_file_position;
  if ( fseek(csv_file, 0, SEEK_SET) != 0 ) {
    fprintf(stderr, "fseek() failed in file %s at line # %d\n", __FILE__, __LINE__);
    return 0;
  }
  if ( file_position != orig_file_position ) {
    file_position = file_position - (end_cap - cursor);
    if ( fseek(csv_file, file_position, SEEK_SET) != 0 ) {
      fprintf(stderr, "fseek() failed in file %s at line # %d\n", __FILE__, __LINE__);
      return 0;
    }
  }

  while(-7) {
    if ( file_position != orig_file_position ) {
stream_more:;
      file_position = file_position - (end_cap - cursor);
      if ( fseek(csv_file, file_position, SEEK_SET) != 0 ) {
        fprintf(stderr, "fseek() failed in file %s at line # %d\n", __FILE__, __LINE__);
        return 0;
      }
    }

    uint_fast32_t chunk_len = fread(buffer, 1, end_cap - cursor, csv_file);
    uint_fast8_t is_at_eof = feof(csv_file);
    if ( chunk_len != (uintptr_t)end_cap - (uintptr_t)cursor ) {
      if ( !is_at_eof ) {
        fprintf(stderr, "Failed to read the csv file\n");
        if ( fseek(csv_file, orig_file_position, SEEK_SET) != 0 ) {
          fprintf(stderr, "fseek() failed in file %s at line # %d\n", __FILE__, __LINE__);
          return 0;
        }
        return 0;
      }
    }
    file_position += chunk_len;
    cursor = buffer;

    buffer[chunk_len] = 0;

    while(-7) {
      uint_fast32_t loc_separator, loc_newline;
      //uint_fast32_t string_len;

      loc_separator = get_idx_of_char(cursor, ',');
      loc_newline = get_idx_of_char(cursor, '\n');

      if ( loc_newline > loc_separator ) {
        if ( loc_newline == UINT32_MAX ) {
          if ( is_at_eof ) {
            if ( fseek(csv_file, orig_file_position, SEEK_SET) != 0 ) {
              fprintf(stderr, "fseek() failed in file %s at line # %d\n", __FILE__, __LINE__);
              return 0;
            }
            return nr_lines + 1;
          }
          goto stream_more;
        }
        nr_lines++;
        cursor = cursor + loc_newline + 1;
      } else if ( loc_newline < loc_separator ) {
        if ( fseek(csv_file, orig_file_position, SEEK_SET) != 0 ) {
          fprintf(stderr, "fseek() failed in file %s at line # %d\n", __FILE__, __LINE__);
          return 0;
        }
        return nr_lines;
      } else {
        if ( is_at_eof ) {
          if ( fseek(csv_file, orig_file_position, SEEK_SET) != 0 ) {
            fprintf(stderr, "fseek() failed in file %s at line # %d\n", __FILE__, __LINE__);
            return 0;
          }
          return nr_lines;
        }

        goto stream_more;
      }

    }
  }
}
