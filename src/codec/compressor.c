#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>  // EEXIST

#include "util-c_lang-basic.h"
#include "zstd.h"
#include "lz4hc.h"
#include "lz4.h"
#include "snappy.h"
#include "err_msg.h"

#define TC_TEXTURE_LOAD_IMPLEMENTATION
#define TC_TEXTURE_LOAD_VULKAN_FORMATS
#include "tc_texture_load.h"

constexpr unsigned int dds_header_size =
  4 + sizeof(TCTex_DDS_HEADER) + sizeof(TCTex_DDS_HEADER_DXT10);

/**
 * \brief Resolve a possibly-relative path against the current working directory.
 * \return a heap-allocated absolute path (caller must free()), or NULL on error.
 */
static char * resolve_path( const char * path )
{
  if ( !path ) return NULL;
  if ( path[0] == '/' ) {
    return strdup(path);
  }

  char * cwd = getcwd(NULL, 0);
  if ( !cwd ) {
    fprintf(stderr, "Error: failed to get current working directory\n");
    return NULL;
  }

  size_t needed = strlen(cwd) + 1 + strlen(path) + 1;
  char * abs_path = malloc(needed);
  if ( !abs_path ) {
    free(cwd);
    fprintf(stderr, "Error: failed to allocate memory for path resolution\n");
    return NULL;
  }

  snprintf(abs_path, needed, "%s/%s", cwd, path);
  free(cwd);
  return abs_path;
}

static enum err_msg integrity_check_bc7_2048( FILE * restrict dds_file )
{
  enum err_msg err_msg = emsg_no_error;
  void * data = 0;
  bool tc_texture_loaded = false;

  long orig_pos = ftell(dds_file);

  char buf[dds_header_size];

  fseek(dds_file, 0, SEEK_SET);
  size_t read_size = fread(buf, 1, dds_header_size, dds_file);
  if ( read_size != dds_header_size ) {
    fprintf(stderr, "Error: failed to read the .dds file\n");
    err_msg = emsg_could_not_read_file;
    goto clean_up_and_return;
  }

  constexpr uint32_t magic_dw = (('D' << 8*0) | ('D' << 8*1) | ('S' << 8*2) | (' ' << 8*3));
  uint32_t * p_dds_head = (void*)buf;
  if ( *p_dds_head != magic_dw ) {
    fprintf( stderr, "Error: DDS header magic value is wrong! "
        "%#010x instead of %#010x\n", *p_dds_head, magic_dw );
    err_msg = emsg_corrupted_data_detected;
    goto clean_up_and_return;
  }

  fseek(dds_file, 0, SEEK_END);
  long src_size = ftell(dds_file);
  fseek(dds_file, 0, SEEK_SET);

  data = malloc(src_size);
  if ( !data ) {
    fprintf(stderr, "Error: malloc failed\n");
    err_msg = emsg_out_of_memory;
    goto clean_up_and_return;
  }

  read_size = fread(data, 1, src_size, dds_file);
  if ( read_size != (size_t)src_size ) {
    fprintf(stderr, "Error: failed to read the .dds file\n");
    err_msg = emsg_could_not_read_file;
    goto clean_up_and_return;
  }

  TCTex_Texture tc_texture;
  if ( !tctex_load_mem(&tc_texture, data, src_size) ) {
    fprintf( stderr, "Error: tctex_load_mem failed on the asset! (%s)\n", tc_texture.errmsg );
    err_msg = emsg_corrupted_data_detected;
    goto clean_up_and_return;
  }
  tc_texture_loaded = true;

  //const TCTex_DDS_HEADER* header = (void*)data+4;
  if ( tc_texture.iformat != TCTEX_IFORMAT_COMPRESSED_BC7_UNORM ) {
    fprintf( stderr, "Error: The format was not BC7_UNORM! (%u)\n", tc_texture.iformat );
    err_msg = emsg_ext_not_supported;
    goto clean_up_and_return;
  }
  if ( tc_texture.size.x != 2048 ) {
    fprintf( stderr, "Error: texture width is not 2048 (%u)\n", tc_texture.size.x );
    err_msg = emsg_data_does_not_meet_std_req;
    goto clean_up_and_return;
  }
  if ( tc_texture.size.y != 2048 ) {
    fprintf( stderr, "Error: texture height is not 2048 (%u)\n", tc_texture.size.y );
    err_msg = emsg_data_does_not_meet_std_req;
    goto clean_up_and_return;
  }
  if ( tc_texture.nmiplevels != 12 ) {
    fprintf( stderr, "Error: mipmap level count is not 12 (%u)\n", tc_texture.nmiplevels );
    err_msg = emsg_data_does_not_meet_std_req;
    goto clean_up_and_return;
  }

clean_up_and_return:
  if (tc_texture_loaded) tctex_close(&tc_texture);
  if (data) free(data);
  fseek(dds_file, orig_pos, SEEK_SET);
  return err_msg;
}


static ZSTD_CCtx* cctx = 0;
static int zstd_compress_file( FILE * restrict src_file, const char *out_path, int level,
    bool verbose, size_t data_size, int nr_threads )
{
  assume( src_file );

  void * src_buf = malloc(data_size);
  if ( !src_buf ) {
    fprintf(stderr, "Error: malloc failed\n");
    return emsg_out_of_memory;
  }

  unsigned long read_size = fread(src_buf, 1, data_size, src_file);
  if ( read_size != data_size ) {
    fprintf(stderr, "Error: failed to read the source file\n");
    free(src_buf);
    return emsg_could_not_read_file;
  }
  //fclose(src_file);

  if ( !cctx ) {
    cctx = ZSTD_createCCtx();
    if (!cctx) {
      fprintf(stderr, "Error: ZSTD_createCCtx() failed\n");
      free(src_buf);
      return emsg_unknown;
    }

    // Set the worker thread count
    size_t zstd_ret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, nr_threads);
    if ( ZSTD_isError(zstd_ret) ) {
      fprintf( stderr, "Error: Failed to set the worker thread count to %d: %s\n",
          nr_threads, ZSTD_getErrorName(zstd_ret) );
      ZSTD_freeCCtx(cctx);
      cctx = 0;
      free(src_buf);
      return emsg_unknown;
    }
  }

  size_t zstd_ret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);
  if ( ZSTD_isError(zstd_ret) ) {
    fprintf( stderr, "Error: Failed to set compression level: %s\n", ZSTD_getErrorName(zstd_ret) );
    free(src_buf);
    return emsg_unknown;
  }

  const size_t max_size = ZSTD_compressBound(data_size);
  if ( max_size == 0 || ZSTD_isError(max_size) ) {
    fprintf(stderr, "Error: compress bound failed\n");
    free(src_buf);
    return emsg_unknown;
  }

  void *dst_buf = malloc(max_size);
  if ( !dst_buf ) {
    fprintf(stderr, "Error: malloc failed\n");
    free(src_buf);
    return emsg_out_of_memory;
  }

  //size_t compressed_size = ZSTD_compress(dst_buf, max_size, src_buf, data_size, level);
  const size_t compressed_size = ZSTD_compress2(cctx, dst_buf, max_size, src_buf, data_size);
  if ( ZSTD_isError(compressed_size) ) {
    fprintf( stderr, "Error: zstd compression error. %s\n", ZSTD_getErrorName(compressed_size) );
    free(src_buf);
    return emsg_unknown;
  }
  free(src_buf);

  if ( ZSTD_isError(compressed_size) ) {
    fprintf(stderr, "Error: zstd compress failed: %s\n", ZSTD_getErrorName(compressed_size));
    free(dst_buf);
    return emsg_unknown;
  }

  if ( verbose ) {
    double size_ratio = (double) compressed_size / (double) data_size;
    fprintf(stdout, "Zstd. Compression Level = %d;  Size Ratio = %f (%zu/%zu)\n",
        level, size_ratio, compressed_size, data_size );
  }

  if ( out_path ) {
    // Do write in a file only when not in benchmark mode
    FILE *dst_file = fopen(out_path, "wb");
    if ( !dst_file ) {
      fprintf(stderr, "Error: cannot open '%s' for writing\n", out_path);
      free(dst_buf);
      return emsg_could_not_write_file;
    }

    fwrite(dst_buf, 1, compressed_size, dst_file);
    fclose(dst_file);
  }
  free(dst_buf);

  return 0;
}


constexpr bool use_lz4_stream_api = false;
static LZ4_streamHC_t * p_lz4_stream = 0;
//// Intentionally larger to test unsynchronized ring buffers
//constexpr size_t Decompress_BUFFER_size = RING_BUFFER_BYTES + MESSAGE_MAX_BYTES;
static int lz4_compress_file( FILE * restrict src_file, const char *out_path, int level,
    bool verbose, size_t data_size )
{
  assume( src_file );

  FILE *dst_file = nullptr;
  if ( out_path ) {
    dst_file = fopen(out_path, "wb");
    if ( !dst_file ) {
      fprintf(stderr, "Error: cannot open '%s' for writing\n", out_path);
      return emsg_could_not_write_file;
    }
  }

  int compressed_size = 0;
  if ( use_lz4_stream_api ) {
    // Faster, but slightly weaker compression (Stream)

    if ( !p_lz4_stream ) {
      p_lz4_stream = LZ4_createStreamHC();
      //p_lz4_stream->internal_donotuse.favorDecSpeed = true;
    }

    constexpr size_t max_msg_len = 1024 * 32;
    constexpr size_t ring_buf_size = 1024 * 32 * 4;
    char ring_buf[ring_buf_size];

    LZ4_resetStreamHC_fast(p_lz4_stream, level);
    size_t ring_buf_ofs = 0;
    while(-7) {
      char * const buf_ptr = &ring_buf[ring_buf_ofs];
      const int nr_bytes_read = (int) fread( buf_ptr, 1, max_msg_len, src_file );
      if ( nr_bytes_read == 0 ) {
        break;
      }

      constexpr size_t output_buf_size = LZ4_COMPRESSBOUND(max_msg_len);
      {
        char output_buf[output_buf_size];
        const int32_t len_compressed_block = LZ4_compress_HC_continue(
            p_lz4_stream, buf_ptr, output_buf, nr_bytes_read, output_buf_size );

        if( len_compressed_block <= 0 ) {
          fprintf( stderr, "Error: LZ4_compress_HC_continue returned %d\n", compressed_size );
          return emsg_unknown;
        }
        compressed_size += len_compressed_block;
        if ( out_path ) {
          // Do write in a file when not in benchmark mode
          fwrite( &len_compressed_block, sizeof(int32_t), 1, dst_file );
          fwrite( output_buf, 1, len_compressed_block, dst_file );
        }

        ring_buf_ofs += nr_bytes_read;

        // Wraparound the ringbuffer offset
        if(ring_buf_ofs > ring_buf_size - max_msg_len) {
          ring_buf_ofs = 0;
        }

      }
    }

    int32_t zero = 0;
    if ( out_path ) {
      // Do write in a file when not in benchmark mode
      fwrite( &zero, sizeof(int32_t), 1, dst_file );
    }

  } else {
    // Slower, but slightly stronger compression

    void * src_buf = malloc(data_size);
    if ( !src_buf ) {
      fprintf(stderr, "Error: malloc failed\n");
      return emsg_out_of_memory;
    }

    unsigned long read_size = fread(src_buf, 1, data_size, src_file);
    if ( read_size != data_size ) {
      fprintf(stderr, "Error: failed to read the source file\n");
      free(src_buf);
      return emsg_could_not_read_file;
    }
    //fclose(src_file);

    size_t max_size = LZ4_COMPRESSBOUND(data_size);
    void *dst_buf = malloc(max_size);
    if ( !dst_buf ) {
      fprintf(stderr, "Error: malloc failed\n");
      free(src_buf);
      return emsg_out_of_memory;
    }

    compressed_size = LZ4_compress_HC(src_buf, dst_buf, data_size, max_size, level);
    if ( out_path ) {
      // Do write in a file when not in benchmark mode
      fwrite(dst_buf, 1, compressed_size, dst_file);
    }
    free(dst_buf);
    free(src_buf);
  }

  if ( verbose ) {
    double size_ratio = (double) compressed_size / (double) data_size;
    fprintf(stdout, "LZ4-HC Compression Level = %d;  Size Ratio = %f (%d/%zu)\n",
        level, size_ratio, compressed_size, data_size );
  }

  if ( out_path ) {
    fclose(dst_file);
  }

  return 0;
}


static struct snappy_env snappy_env = {0};
static bool snappy_env_has_been_init = false;
static int snappy_compress_file( FILE * restrict src_file, const char *out_path, bool verbose,
    size_t data_size )
{
  assume( src_file );

  void * src_buf = malloc(data_size);
  if ( !src_buf ) {
    fprintf(stderr, "Error: malloc failed\n");
    return emsg_out_of_memory;
  }

  unsigned long read_size = fread(src_buf, 1, data_size, src_file);
  if ( read_size != data_size ) {
    fprintf(stderr, "Error: failed to read the source file\n");
    free(src_buf);
    return emsg_could_not_read_file;
  }
  //fclose(src_file);

  if ( !snappy_env_has_been_init ) {
    int ret = snappy_init_env(&snappy_env);
    if ( ret ) {
      fprintf( stderr, "Error: snappy_init_env failed and returned %d\n", ret );
      free(src_buf);
      return emsg_unknown;
    }
    snappy_env_has_been_init = true;
  }

  const size_t max_size = snappy_max_compressed_length(data_size);
  if ( max_size == 0 || ZSTD_isError(max_size) ) {
    fprintf(stderr, "Error: compress bound failed\n");
    free(src_buf);
    return emsg_unknown;
  }

  void *dst_buf = malloc(max_size);
  if ( !dst_buf ) {
    fprintf(stderr, "Error: malloc failed\n");
    free(src_buf);
    return emsg_out_of_memory;
  }

  size_t compressed_size;
  int ret = snappy_compress( &snappy_env, src_buf, data_size, dst_buf, &compressed_size );
  if ( ret ) {
    fprintf( stderr, "Error: snappy compression failed. %d\n", ret );
    free(src_buf);
    return emsg_unknown;
  }
  free(src_buf);

  if ( verbose ) {
    double size_ratio = (double) compressed_size / (double) data_size;
    fprintf(stdout, "Snappy Compression;  Size Ratio = %f (%zu/%zu)\n",
        size_ratio, compressed_size, data_size );
  }

  if ( out_path ) {
    // Do write in a file only when not in benchmark mode
    FILE *dst_file = fopen(out_path, "wb");
    if ( !dst_file ) {
      fprintf(stderr, "Error: cannot open '%s' for writing\n", out_path);
      free(dst_buf);
      return emsg_could_not_write_file;
    }

    fwrite(dst_buf, 1, compressed_size, dst_file);
    fclose(dst_file);
  }
  free(dst_buf);

  return 0;
}


static const char * default_output_dir = "/dev/shm/compressor";
#define REQUIRED_ARGS \
  REQUIRED_STRING_ARG(src, "SRC_FILE", "The source file that contains the original data")
#define OPTIONAL_ARGS \
    OPTIONAL_UINT_ARG(threads, 0, "-t", "zstd worker thread count", \
        "Number of threads to use; The default value 0 means single-threaded work; " \
        "At higher zstd compression levels you need bigger SRC_FILE to actually use extra threads"\
        "; the size gets a tiny bit bigger when jobs are distributed among multiple threads") \
    OPTIONAL_STRING_ARG(output_dir, default_output_dir, "-C", "output directory", \
        "The directory where compressed data will be saved") \
    OPTIONAL_UINT_ARG(zstd, 0, "-z", "zstd compression level", \
        "The default value 0 means no zstd compression will be done; " \
        "The valid values are 1-22") \
    OPTIONAL_UINT_ARG(lz4, 0, "-l", "lz4 compression level", \
        "The default value 0 means no lz4 compression will be done; " \
        "The valid values are 1-12")
#define BOOLEAN_ARGS \
    BOOLEAN_ARG(verbose, "-v", "verbose mode") \
    BOOLEAN_ARG(snappy, "-s", "Snappy") \
    BOOLEAN_ARG(minimalist, "-m", "Minimalist: " \
        "strip DDS headers assuming the end user knows the albedo map format" \
        " (2048x2048 and 12 mip levels)") \
    BOOLEAN_ARG(all, "-a", "all") \
    BOOLEAN_ARG(benchmark, "-b", "benchmark mode")
#include "easyargs.h"

int main(int argc, char * argv[])
{
  args_t args = make_default_args();
  int ret = parse_args(argc, argv, &args);
  if ( !ret ) {
    print_help(argv[0]);
    return emsg_unknown;
  }

  if ( args.benchmark ) {
    args.verbose = true;
  }

  const char * src_path = args.src;
  const char * fname = strrchr(src_path, '/');
  fname = fname ? fname + 1 : src_path;

  FILE *src_file = fopen(src_path, "rb");
  if ( !src_file ) {
    fprintf(stderr, "Error: cannot open '%s'\n", src_path);
    return emsg_could_not_read_file;
  }

  fseek(src_file, 0, SEEK_END);
  const long src_size = ftell(src_file);
  if ( src_size < 0 ) {
    fprintf(stderr, "Error: cannot determine size of '%s'\n", src_path);
    fclose(src_file);
    return emsg_could_not_read_file;
  }
  size_t data_size = (size_t) src_size;

  const size_t fpath_len = strlen(src_path);
  const bool is_dds = (fpath_len >= 4) && (strcmp(src_path + fpath_len - 4, ".dds") == 0);
  if ( args.minimalist ) {
    if ( is_dds ) {
      if ( integrity_check_bc7_2048(src_file) ) {
        fclose(src_file);
        return emsg_data_does_not_meet_std_req;
      }
      data_size -= dds_header_size;
    }
  }

  char * abs_output_dir = resolve_path(args.output_dir);
  if ( !args.benchmark ) {
    if ( !abs_output_dir ) {
      ret = emsg_could_not_read_file;
      goto exit_program;
    }

    ret = mkdir(abs_output_dir, 0755);
    if ( ret && errno != EEXIST) {
      fprintf(stderr, "Error: failed to create directory '%s'. (%d)\n", abs_output_dir, errno);
      ret = emsg_could_not_write_file;
      goto exit_program;
    }
  } else if ( strcmp(args.output_dir, default_output_dir ) != 0 ) {
    fprintf(stdout, "Warning: output directory parameter (-C) is not used in benchmark mode\n");
  }

  char out_path_buf[512];
  if ( args.all ) {
    // 35(12 + 22 + 1) compression jobs
    for ( int c_lv = 1; c_lv <= 22; c_lv++ ) {
      fseek(src_file, src_size - data_size, SEEK_SET);
      char * out_path = nullptr;
      if ( !args.benchmark ) {
        snprintf(out_path_buf, sizeof(out_path_buf), "%s/%s.zstd%d", abs_output_dir, fname, c_lv);
        out_path = out_path_buf;
      }
      ret = zstd_compress_file(src_file, out_path, c_lv, args.verbose, data_size, args.threads);
      if ( ret ) {
        goto exit_program;
      }
    }
    for ( int c_lv = 1; c_lv <= 12; c_lv++ ) {
      fseek(src_file, src_size - data_size, SEEK_SET);
      char * out_path = nullptr;
      if ( !args.benchmark ) {
        snprintf(out_path_buf, sizeof(out_path_buf), "%s/%s.lz4%d", abs_output_dir, fname, c_lv);
        out_path = out_path_buf;
      }
      ret = lz4_compress_file(src_file, out_path, c_lv, args.verbose, data_size);
      if ( ret ) {
        goto exit_program;
      }
    }
    fseek(src_file, src_size - data_size, SEEK_SET);
    char * out_path = nullptr;
    if ( !args.benchmark ) {
      snprintf(out_path_buf, sizeof(out_path_buf), "%s/%s.snp", abs_output_dir, fname);
      out_path = out_path_buf;
    }
    ret = snappy_compress_file(src_file, out_path, args.verbose, data_size);
    if ( ret ) {
      goto exit_program;
    }

  } else {
    // 0-3 compression jobs

    if ( args.zstd ) {
      assert( args.zstd <= 22 );
      fseek(src_file, src_size - data_size, SEEK_SET);
      int c_lv = args.zstd;
      char * out_path = nullptr;
      if ( !args.benchmark ) {
        snprintf(out_path_buf, sizeof(out_path_buf), "%s/%s.zstd%d", abs_output_dir, fname, c_lv);
        out_path = out_path_buf;
      }
      ret = zstd_compress_file(src_file, out_path, c_lv, args.verbose, data_size, args.threads);
      if ( ret ) {
        goto exit_program;
      }
    }
    if ( args.lz4 ) {
      assert( args.lz4 <= 12 );
      fseek(src_file, src_size - data_size, SEEK_SET);
      int c_lv = args.lz4;
      char * out_path = nullptr;
      if ( !args.benchmark ) {
        snprintf(out_path_buf, sizeof(out_path_buf), "%s/%s.lz4%d", abs_output_dir, fname, c_lv);
        out_path = out_path_buf;
      }
      ret = lz4_compress_file(src_file, out_path, c_lv, args.verbose, data_size);
      if ( ret ) {
        goto exit_program;
      }
    }
    if ( args.snappy ) {
      fseek(src_file, src_size - data_size, SEEK_SET);
      char * out_path = nullptr;
      if ( !args.benchmark ) {
        snprintf(out_path_buf, sizeof(out_path_buf), "%s/%s.snp", abs_output_dir, fname);
        out_path = out_path_buf;
      }
      ret = snappy_compress_file(src_file, out_path, args.verbose, data_size);
      if ( ret ) {
        goto exit_program;
      }
    }
  }

  ret = 0;
exit_program:
  if ( cctx ) {
    ZSTD_freeCCtx(cctx);
  }
  if ( use_lz4_stream_api && p_lz4_stream ) {
    LZ4_freeStreamHC(p_lz4_stream);
  }
  if ( snappy_env_has_been_init ) {
    snappy_free_env(&snappy_env);
  }
  free(abs_output_dir);
  fclose(src_file);
  return ret;
}
