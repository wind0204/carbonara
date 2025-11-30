#if !defined APPDEV_SUPPORT_SHARED_LIBRARY_H_
#define APPDEV_SUPPORT_SHARED_LIBRARY_H_


#include <stdint.h>

#if defined(__linux)
#include <dlfcn.h>
#elif defined(_WIN32)
//#include <Windows.h>
#include <windows.h>
#endif


//-------------------------------------------------------------------------------------------------
#if defined(__linux)
typedef void * shared_lib;
#elif defined(_WIN32)
typedef HMODULE shared_lib;
#endif


//-------------------------------------------------------------------------------------------------
shared_lib load_shared_lib( const char * lib_name );

//-------------------------------------------------------------------------------------------------
uint8_t unload_shared_lib( shared_lib lib );

//-------------------------------------------------------------------------------------------------
void * get_func_sym_from_shared_lib( shared_lib * library, const char * name );


#endif // APPDEV_SUPPORT_SHARED_LIBRARY_H_
