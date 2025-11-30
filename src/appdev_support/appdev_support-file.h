#if !defined APPDEV_SUPPORT_FILE_H_
#define APPDEV_SUPPORT_FILE_H_


#include <stdint.h>


//-------------------------------------------------------------------------------------------------
/**
 * \brief Get the size of the file referred to by \a path.
 * TODO MS Windows is not supported yet.
 * \return 0 when it failed
 *
 * errno errno is set by `stat` in sys/stat.h on POSIX systems
 */
uint64_t get_file_size( const char *path );


#endif  // APPDEV_SUPPORT_FILE_H_
