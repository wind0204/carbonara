#include <stdio.h>

#include "appdev_support-shared_library.h"


//-------------------------------------------------------------------------------------------------
shared_lib load_shared_lib( const char * lib_name )
{
  shared_lib lib;

#if defined(__linux)
  lib = dlopen(lib_name, RTLD_LAZY | RTLD_GLOBAL);

  if ( lib == 0 ) {
    const char *err_msg;
    err_msg = dlerror();
    fprintf( stderr, "Failed to load %s: %s\n", lib_name, err_msg );
  }
#elif defined(_WIN32)
  lib = LoadLibrary( lib_name );
#endif

  return lib;
}


//-------------------------------------------------------------------------------------------------
uint8_t unload_shared_lib( shared_lib lib )
{
  uint8_t ret = 0;
#if defined(__linux)
  if ( dlclose(lib) ) {
    const char *err_msg;
    err_msg = dlerror();
    fprintf( stderr, "Failed to unload a library: %s\n", err_msg );
    ret = 1;
  }
#elif defined(_WIN32)
  if ( !FreeLibrary(lib) ) ret = 1;
#endif
  return ret;
}


//-------------------------------------------------------------------------------------------------
void * get_func_sym_from_shared_lib( shared_lib * lib, const char * name )
{
  void * ptr;

#if defined(__linux)
  // TODO You might need to append an underscore to name in some platforms that requrie
  //  different naming convention; I don't know what platforms do though.
  ptr = dlsym( lib, name );
#elif defined(_WIN32)
  ptr = GetProcAddress( lib, name );
#endif

  return ptr;
}


