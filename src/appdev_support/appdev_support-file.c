#include <stdint.h>
#include <sys/stat.h>

#include "appdev_support-file.h"


//-------------------------------------------------------------------------------------------------
uint64_t get_file_size( const char *path )
{
  struct stat filestat;
  if ( stat(path, &filestat) == -1 ) {
    return 0;
  }
  return filestat.st_size;
}


