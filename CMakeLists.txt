cmake_minimum_required( VERSION 3.25 )
project( Carbonara )


##-------------------------------------------------------------------------------------------------
# Options
##-------------------------------------------------------------------------------------------------
set( CARA_ASAN_DEBUG_BUILD OFF )  # A top-level user can set this variable

if ( NOT DEFINED CARA_ASAN_DEBUG_BUILD )
  set( CARA_ASAN_DEBUG_BUILD ON )  # The default value of CARA_ASAN_DEBUG_BUILD is defined here
endif()
option( CARA_ASAN_DEBUG_BUILD "Use the address sanitizer in Debug build" CARA_ASAN_DEBUG_BUILD )

# Set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# Prevent installing to system directories.
set( CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/$<CONFIGURATION>" CACHE INTERNAL "" )



##-------------------------------------------------------------------------------------------------
# libhwloc
##-------------------------------------------------------------------------------------------------
# Build hwloc from source using its CMake build system
# The hwloc CMake build contrib/linux-cmake is inspired from contrib/windows-cmake
set(HWLOC_SKIP_LSTOPO ON CACHE INTERNAL "don't build/install lstopo")
set(HWLOC_SKIP_TOOLS ON CACHE INTERNAL "don't build/install other hwloc tools")
set(HWLOC_SKIP_INCLUDES OFF CACHE INTERNAL "install headers")
set(HWLOC_WITH_LIBXML2 OFF CACHE INTERNAL "use libxml2 instead of minimal XML")
set(HWLOC_WITH_OPENCL OFF CACHE INTERNAL "enable OpenCL support")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.17)
  set(HWLOC_WITH_CUDA OFF CACHE INTERNAL "enable CUDA support")
endif()
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "build static libraries" FORCE)

# Use the CMake build from hwloc-linux-cmake
add_subdirectory(extern/hwloc-linux-cmake ${CMAKE_BINARY_DIR}/hwloc EXCLUDE_FROM_ALL)

# Create imported target for hwloc
# Put build directory FIRST so generated hwloc/autogen/config.h is found
add_library(hwloc::hwloc STATIC IMPORTED GLOBAL)
set_target_properties(hwloc::hwloc PROPERTIES
  IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/hwloc/libhwloc.a"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/hwloc/include;${CMAKE_CURRENT_SOURCE_DIR}/extern/hwloc/include"
)
add_dependencies(hwloc::hwloc hwloc)



##-------------------------------------------------------------------------------------------------
# SDL
##-------------------------------------------------------------------------------------------------

option( USE_SDL_IN_EXTERN "Use SDL in extern directory" ON )

set( BUILD_SHARED_LIBS OFF CACHE INTERNAL "" )
set( SDL_SHARED OFF CACHE INTERNAL "" FORCE )
set( SDL_STATIC ON CACHE INTERNAL "" FORCE )
add_definitions( -DSDL_DYNAMIC_API=0 )

#set(SDL_REVISION "My Vendored SDL" CACHE INTERNAL "")  # This prevents re-configures every time a git change occurs: https://github.com/libsdl-org/SDL/issues/9998
#set( SDL_X11_XTEST OFF CACHE INTERNAL "" )  # You might have to deal with unimproved mouse warping by setting it off

set( SDL_VULKAN ON CACHE INTERNAL "" )
set( SDL_AUDIO ON CACHE INTERNAL "" )
set( SDL_HAPTIC ON CACHE INTERNAL "" )
set( SDL_JOYSTICK ON CACHE INTERNAL "" )
set( SDL_LIBC ON CACHE BOOL "" FORCE )  # Use the C libraries that are globally shared.
set( SDL_TESTS OFF CACHE INTERNAL "" )
 set( SDL_TEST_LIBRARY OFF CACHE INTERNAL "" )

# Disable the subsystems we don't use
set( SDL_GPU OFF CACHE INTERNAL "" )
set( SDL_RENDER OFF CACHE INTERNAL "" )
set( SDL_CAMERA OFF CACHE INTERNAL "" )
set( SDL_DIRECTX OFF CACHE INTERNAL "" )
set( SDL_OPENGL OFF CACHE INTERNAL "" )
set( SDL_OPENGLES OFF CACHE INTERNAL "" )
set( SDL_DUMMYAUDIO OFF CACHE INTERNAL "" )
set( SDL_DUMMYVIDEO OFF CACHE INTERNAL "" )
set( SDL_OFFSCREEN OFF CACHE INTERNAL "" )
set( SDL_WASAPI OFF CACHE INTERNAL "Use the Windows WASAPI audio driver" )

if ( USE_SDL_IN_EXTERN )
  # We use EXCLUDE_FROM_ALL so that its install targets and configs
  #   don't pollute upwards into our configuration.
  add_subdirectory( extern/SDL EXCLUDE_FROM_ALL SYSTEM )
else()
  # 1. Look for a SDL3 package,
  # 2. look for the SDL3-shared component, and
  # 3. fail if the shared component cannot be found.
  find_package( SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared SDL3-static )
endif()



##-------------------------------------------------------------------------------------------------
# Carbonara
##-------------------------------------------------------------------------------------------------
set( CMAKE_C_STANDARD_REQUIRED TRUE )

# look for modules in extra cmake modules' directory
if( NOT DEFINED CMAKE_MODULE_PATH )
  set( CMAKE_MODULE_PATH "/usr/share/ECM/find-modules" )
else()
  set( CMAKE_MODULE_PATH "/usr/share/ECM/find-modules;${CMAKE_MODULE_PATH}" )
endif()
set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/find-modules;${CMAKE_MODULE_PATH}" )
message( STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}" )

set( Carbonara_APP_NAME "Carbonara" )
set( Carbonara_APP_VERSION_MAJOR 0 )
set( Carbonara_APP_VERSION_MINOR 1 )
set( Carbonara_VERSION_MAJOR 0 )
set( Carbonara_VERSION_MINOR 1 )

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )  # for clangd


if( CMAKE_BUILD_TYPE MATCHES Debug )
  #set( Carbonara_DEBUG 1 )
  #add_definitions( -DCarbonara_DEBUG=1 )
  add_definitions( -DDEBUG_MODE )
  add_definitions( -DLLVM_ENABLE_RUNTIMES="compiler-rt" )
  add_definitions( -DLLVM_ENABLE_PROJECTS="clang" )
  set(CMAKE_C_FLAGS_DEBUG "-O1 -g")
else()
  #set( Carbonara_DEBUG 0 )
  #add_definitions( -DCarbonara_DEBUG=0 )
endif()

include_directories( SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/extern/spall-web" )
if( CMAKE_BUILD_TYPE MATCHES Release )
  add_definitions( -DARE_WE_SHIPPING_IT )
  message( STATUS "Are we shipping it?" )
else()
  add_definitions( -DENABLE_PROFILING )
endif()



if( NOT DEFINED ENV{VULKAN_SDK} )
  message( FATAL_ERROR "Please provide the environment variable VULKAN_SDK" )
endif()
message( STATUS "\$ENV{VULKAN_SDK}: $ENV{VULKAN_SDK}" )

# Prevent symbole conflicts
add_definitions( -DVK_NO_PROTOTYPES )




if( NOT DEFINED CARBONARA_TARGET_PLATFORM )
  if( WIN32 )
    set( CARBONARA_TARGET_PLATFORM "WIN32" )
  elseif( UNIX )
    set( CARBONARA_TARGET_PLATFORM "WAYLAND_OR_XCB_OR_XLIB" )
    #find_package( Wayland REQUIRED )
    #include( WaylandProtocolGen.cmake )
    #set( WAYLAND_PROTOCOL_C_FILES "" )
    #find_program( WAYLAND_SCANNER NAMES wayland-scanner )
    #file( GLOB WAYLAND_PROTOCOLS_XML RELATIVE "${PROJECT_SOURCE_DIR}/wayland-protocols/" "${PROJECT_SOURCE_DIR}/wayland-protocols/*.xml" )
    #foreach( _XML IN LISTS WAYLAND_PROTOCOLS_XML )
    #  string( REGEX REPLACE "\\.xml$" "" _PROTL "${_XML}" )
    #  WaylandProtocolGen( "${WAYLAND_SCANNER}" "${WAYLAND_SCANNER_CODE_MODE}" "${PROJECT_SOURCE_DIR}/wayland-protocols/${_XML}" "${_PROTL}" )
    #endforeach()
  elseif( ANDROID )
    set( CARBONARA_TARGET_PLATFORM "ANDROID" )
  endif()
endif()
message( STATUS "CARBONARA_TARGET_PLATFORM: ${CARBONARA_TARGET_PLATFORM}" )
add_definitions( -DCARBONARA_TARGET_PLATFORM_${CARBONARA_TARGET_PLATFORM} )



include( "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TargetArch.cmake" )
target_architecture( CMAKE_TARGET_ARCHITECTURES )
list( LENGTH CMAKE_TARGET_ARCHITECTURES cmake_target_arch_len )
message( STATUS "CMAKE_TARGET_ARCHITECTURES: ${CMAKE_TARGET_ARCHITECTURES}" )
if( NOT "${cmake_target_arch_len}" STREQUAL "1" )
  set( CMAKE_TARGET_ARCHITECTURE_UNIVERSAL TRUE )
  set( CMAKE_TARGET_ARCHITECTURE_CODE "universal" )
else()
  set( CMAKE_TARGET_ARCHITECTURE_UNIVERSAL FALSE )
  set( CMAKE_TARGET_ARCHITECTURE_CODE "${CMAKE_TARGET_ARCHITECTURES}" )
endif()



add_definitions( -DTARGET_ENDIANNESS=1 )  # 1: Little Endian, 2: Big Endian, 3: PDP Endian
add_definitions( -DCACHELINE_LEN=64 )
add_definitions( -DLOG2_CACHELINE_LEN=6 )



set( C_FLAGS_IN_HOUSE "" )

# It seems Clang doesn't have anything like -fno-signaling-nans
#set( C_FLAGS_IN_HOUSE "${C_FLAGS_IN_HOUSE};-fno-signaling-nans" )
set( C_FLAGS_IN_HOUSE "${C_FLAGS_IN_HOUSE};-fno-trapping-math" )
set( C_FLAGS_IN_HOUSE "${C_FLAGS_IN_HOUSE};-fno-math-errno" )

set( C_FLAGS_TARG_ARCH "" )
if( CMAKE_TARGET_ARCHITECTURE_CODE MATCHES "x86_64" )
  set( C_FLAGS_TARG_ARCH "${C_FLAGS_TARG_ARCH} -march=haswell" )
  # With MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, POPCNT, CX16, SAHF, FXSR, AVX, XSAVE, PCLMUL, FSGSBASE, RDRND, F16C, AVX2, BMI, BMI2, LZCNT, FMA, MOVBE and HLE instruction set support.
  add_definitions( -DUTILIZE_BMI2 )
  add_definitions( -DUTILIZE_AVX2 )
  #add_definitions( -DUTILIZE_SSE4_2 )

  SET_SOURCE_FILES_PROPERTIES(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info_struct/info_struct-tree.c  # util-c_lang-bits2.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tester.c  # util-c_lang-bits2.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/carbonara.c  # compute-magic.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support/appdev_support-timer.c  # compute-magic.h
    PROPERTIES
      COMPILE_OPTIONS "-masm=intel"
  )

  if ( CMAKE_CC_COMPILER MATCHES gcc )
    #set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_TARG_ARCH} -msse4.2 -mpreferred-stack-boundary=4" )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_TARG_ARCH}" )
  else()
    #set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_TARG_ARCH} -msse4.2 -mstack-alignment=4" )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_TARG_ARCH}" )
  endif()

  # TODO Should I consider distributing another build for the CPUs like The Knights Landing where using
  # 128-bit SIMD instructions can hurt the performance and there is no way to mitigate it?
  #if( DEFINED ENV{ENABLE_AVX2_OPTIMIZATION} )
  #  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2" )
  #endif()
elseif( CMAKE_TARGET_ARCHITECTURE_CODE MATCHES "i386" )
  message( FATAL_ERROR "32-bit processors are not supported." )
endif()

# For SIMDE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp-simd")  # GCC 4.9+ and Clang 6+
add_definitions( -DSIMDE_ENABLE_OPENMP )
include_directories( SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/extern/simde" )

if ( CMAKE_CC_COMPILER MATCHES gcc )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopt-info-vec-optimized")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopt-info-vec-missed")
endif()

# Use pipes rather than temporary files for communication between the various stages of compilation
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")

# We're done with CMAKE_C_FLAGS at this line.
string(REGEX REPLACE "^ +" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})

# Default settings:
#   CMAKE_C_FLAGS_DEBUG is -g
#   CMAKE_C_FLAGS_RELEASE is -O3 -DNDEBUG
#   CMAKE_C_FLAGS_RELWITHDEBINFO is -O2 -g -DNDEBUG
#   CMAKE_C_FLAGS_MINSIZEREL is -Os -DNDEBUG



add_definitions( -DTEXTURE_BILINEAR_FILTERING )

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/carbonara-config.h.in"
  "${PROJECT_BINARY_DIR}/src/carbonara-config.h"
  )



#include( CheckIPOSupported )
#check_ipo_supported()
#if( CMAKE_BUILD_TYPE MATCHES Debug )
#set( CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE )
#endif()



add_executable( packer
  src/asspacker/packer.c
  )
  target_compile_options( packer PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET packer PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property( TARGET packer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE )
endif()
#add_custom_command(TARGET packer POST_BUILD
#  COMMAND cscope -bqUR
#  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#  )

#file( GLOB CARBONARA_SRC CONFIGURE_DEPENDS "src/carbonara*.c" )
#add_library( carbonara ${CARBONARA_SRC} )
add_executable( carbonara
  src/carbonara.c
  src/carbonara-job.c
  src/carbonara-maint.c
  src/carbonara-rainbow.c
  src/carbonara-mm.c
  src/carbonara-vulkan.c
  #src/vulkan/vulkan_functions.c
  src/util/util-spall.c
  )
  target_compile_options( carbonara PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET carbonara PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property( TARGET carbonara PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE )
endif()
if( CMAKE_BUILD_TYPE MATCHES Debug AND CARA_ASAN_DEBUG_BUILD )
  target_compile_options( carbonara PRIVATE -fsanitize=address -fno-omit-frame-pointer )
  target_link_options( carbonara PRIVATE -fsanitize=address )
  add_definitions( -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_RUNTIMES="compiler-rt" )
endif()

add_executable( tester
  src/tester.c
  )
  target_compile_options( tester PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET tester PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property( TARGET tester PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE )
endif()
if( CMAKE_BUILD_TYPE MATCHES Debug AND CARA_ASAN_DEBUG_BUILD )
  target_compile_options( tester PRIVATE -fsanitize=address -fno-omit-frame-pointer )
  target_link_options( tester PRIVATE -fsanitize=address )
endif()

#target_compile_options(carbonara PRIVATE
#  $<$<CONFIG:Release>:-ffast-math>)

#add_custom_command(TARGET carbonara POST_BUILD
#  COMMAND cscope -bqUR
#  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#  )


#file( GLOB APP_DEV_SUPPORT_SRC CONFIGURE_DEPENDS "src/appdev_support/*.c" )
#add_library( App-dev_support ${APP_DEV_SUPPORT_SRC} )
add_library( App-dev_support STATIC
  src/appdev_support/appdev_support-thread.c
  src/appdev_support/appdev_support-timer.c
  src/appdev_support/appdev_support-shared_library.c
  src/appdev_support/appdev_support-processor_map.c
  #src/appdev_support/appdev_support-window.c  # Use SDL3
  src/appdev_support/appdev_support-file.c  # TODO ditch it and just use SDL3
  )
  target_compile_options( App-dev_support PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
  set_property( TARGET App-dev_support PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property( TARGET App-dev_support PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE )
endif()

add_library( Memorable_arena STATIC
  src/memorable_arena/memorable_arena-vulkan.c
  src/memorable_arena/memorable_arena-list.c
  src/memorable_arena/memorable_arena-dynamic.c )
target_compile_options( Memorable_arena PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET Memorable_arena PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property( TARGET Memorable_arena PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE )
endif()
# Link to hwloc to get include directories (for hwloc.h in appdev_support-processor_map.h)
target_link_libraries( Memorable_arena PUBLIC hwloc::hwloc )

add_library( Err_msg STATIC
  src/err_msg/err_msg.c )
target_compile_options( Err_msg PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET Err_msg PROPERTY C_STANDARD 23 )

#file( GLOB INFO_STURUCT_SRC CONFIGURE_DEPENDS "src/info_struct/*.c" )
#add_library( Info_struct ${INFO_STURUCT_SRC} )
add_library( Info_struct STATIC
  src/info_struct/info_struct.c
  src/info_struct/info_struct-stack.c
  src/info_struct/info_struct-gmm.c
  src/info_struct/info_struct-smm.c
  src/info_struct/info_struct-pm.c
  src/info_struct/info_struct-al.c
  src/info_struct/info_struct-hashtable.c
  src/info_struct/info_struct-tree.c
  )
  target_compile_options( Info_struct PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET Info_struct PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property(TARGET Info_struct PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_library( Util-c_lang STATIC
  src/util/util-c_lang.c )
target_compile_options( Util-c_lang PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET Util-c_lang PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property(TARGET Util-c_lang PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_library( Util-hwloc STATIC
  extern/util/util-hwloc.c )
target_compile_options( Util-hwloc PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET Util-hwloc PROPERTY C_STANDARD 23 )
target_link_libraries( Util-hwloc PUBLIC hwloc::hwloc )

add_library( Sorter STATIC
  src/sorter/sorter-array.c )
target_compile_options( Sorter PRIVATE -Weverything -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-enum-cast -Wno-implicit-void-ptr-cast -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-pre-c23-compat -Wno-switch-default -Wno-jump-misses-init -Wno-disabled-macro-expansion $<$<CONFIG:Release>:-Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-parameter> ${C_FLAGS_IN_HOUSE} )
set_property( TARGET Sorter PROPERTY C_STANDARD 23 )
if( NOT CMAKE_BUILD_TYPE MATCHES Debug )
  set_property(TARGET Sorter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# hwloc is now built via add_subdirectory and available as hwloc::hwloc target

#add_library( Netloc STATIC IMPORTED )
#set_target_properties( Netloc PROPERTIES
#  IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/extern/libnetloc.a"
#  INTERFACE_LINK_LIBRARIES "OurHwloc"
#  GENERATED TRUE
#)
#add_dependencies( Netloc OurHwloc )

#add_library( LibSDL STATIC IMPORTED )
#set_target_properties( LibSDL PROPERTIES
#  IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/extern/build-sdl/libSDL3.a"
#  INTERFACE_LINK_LIBRARIES "m"  # You need libm for math.h
#  GENERATED TRUE
#)
#add_dependencies( LibSDL build-sdl )

#find_package(SDL2 REQUIRED)

add_library( Snappy STATIC
  extern/snappy-c/snappy.c )
#target_compile_options( Snappy PRIVATE -Werror -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unused-macros -Wno-gnu-alignof-expression -Wno-error=unused-function -Wno-double-promotion -Wno-logical-op-parentheses -Wno-unsafe-buffer-usage -Wno-extra-semi-stmt -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-vla -Wno-switch-enum -Wno-cast-qual -Wno-implicit-int-conversion -Wno-declaration-after-statement -Wno-gnu-pointer-arith -Wno-keyword-macro -Wno-padded -Wno-reserved-macro-identifier -Wno-language-extension-token -Wno-gnu-statement-expression-from-macro-expansion -Wno-cast-qual -Wno-cast-align -Wno-noshorten-64-to-32 -Wno-reserved-macro-identifier )

# You can find carbonara-config.h here
include_directories( "${PROJECT_BINARY_DIR}/src" )

# clangd was not as smart as I thought it would be; the linting information didn't take into account which header file was loaded by which source.
# So I made some header filles visible globally:
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/src/memorable_arena" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/src/util" )

#include_directories( SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/extern/SOIL2" )
include_directories( SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/extern/tclib" )

include_directories( SYSTEM "$ENV{VULKAN_SDK}/include" )
# hwloc includes are now provided via hwloc::hwloc target

#include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/src/err_msg" )
target_include_directories( Err_msg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/err_msg" )
target_include_directories( Info_struct PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/err_msg" )
target_include_directories( App-dev_support PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/err_msg" )
target_include_directories( Util-hwloc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/err_msg" )

target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" )

target_include_directories( carbonara PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/include" )
# For carbonara-texture.h:
target_include_directories( packer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/include" )
# For vk_page
target_include_directories( Memorable_arena PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/include" )
# For carbonara-math.h
target_include_directories( tester PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/include" )

target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan" )
target_include_directories( App-dev_support PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan" )
target_include_directories( Memorable_arena PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan" )

target_include_directories( App-dev_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support" )
#target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support" )
#target_include_directories( tester PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support" )
#target_include_directories( Info_struct PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support" )
#target_include_directories( Memorable_arena PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support" )
#target_include_directories( packer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/appdev_support" )

target_include_directories( Memorable_arena PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/memorable_arena" )
#target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/memorable_arena" )
#target_include_directories( Info_struct PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/memorable_arena" )

target_include_directories( Info_struct PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/info_struct" )
target_include_directories( Memorable_arena PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/info_struct" )
#target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/info_struct" )
#target_include_directories( tester PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/info_struct" )
#target_include_directories( App-dev_support PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/info_struct" )

target_include_directories( Sorter PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/sorter" )
#target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sorter" )
#target_include_directories( tester PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sorter" )
#target_include_directories( packer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sorter" )
#target_include_directories( Info_struct PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sorter" )

#target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/asspacker" )

target_include_directories( Util-hwloc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/extern/util" )
#target_include_directories( App-dev_support PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/util" )
target_include_directories( carbonara PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/util" )
target_include_directories( Info_struct PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/util" )
target_include_directories( Memorable_arena PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/util" )

# hwloc includes are now provided via hwloc::hwloc target

##include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/extern/sdl/include" )
##target_include_directories( carbonara PRIVATE ${SDL3_INCLUDE_DIRS} )
#target_include_directories( carbonara PRIVATE SDL3::Headers )

target_include_directories( Snappy PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/extern/snappy-c" )

#if( CARBONARA_TARGET_PLATFORM STREQUAL "WIN32" )  # win32 support is not implemented
##elseif( CARBONARA_TARGET_PLATFORM STREQUAL "XCB" )
##  target_sources( App-dev_support PRIVATE ${XCB_INCLUDE_DIRS} )
#elseif( CARBONARA_TARGET_PLATFORM STREQUAL "WAYLAND_OR_XCB_OR_XLIB" )
#  #target_include_directories( App-dev_support PRIVATE ${Wayland_INCLUDE_DIRS} )
#  #target_include_directories( App-dev_support PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/wayland-generated-protocols" )
#  #target_include_directories( App-dev_support PRIVATE ${XCB_INCLUDE_DIRS} )
#  #target_include_directories( App-dev_support PRIVATE ${XLIB_INCLUDE_DIRS} )
##elseif( CARBONARA_TARGET_PLATFORM STREQUAL "XLIB" )  # xlib support is not implemented
##elseif( CARBONARA_TARGET_PLATFORM STREQUAL "ANDROID" )  # Android support is not implemented
#endif()


target_link_libraries( App-dev_support PRIVATE ${CMAKE_DL_LIBS} )
target_link_libraries( App-dev_support PRIVATE Util-hwloc )
#target_link_libraries( App-dev_support PRIVATE Hwloc )
target_link_libraries( App-dev_support PUBLIC hwloc::hwloc )
target_link_libraries( App-dev_support PRIVATE SDL3::SDL3-static )
if ( WIN32 )
else ( UNIX )
  target_link_libraries( App-dev_support PRIVATE pthread)
endif()

#if( CARBONARA_TARGET_PLATFORM STREQUAL "WIN32" )  # win32 support is not implemented
##elseif( CARBONARA_TARGET_PLATFORM STREQUAL "XCB" )
##  target_link_libraries( App-dev_support PRIVATE ${XCB_LIBRARIES} )
#elseif( CARBONARA_TARGET_PLATFORM STREQUAL "WAYLAND_OR_XCB_OR_XLIB" )
#  # appdev_support-window is obsolete.
#  #target_link_libraries( App-dev_support PRIVATE ${Wayland_LIBRARIES} )
#  #target_link_libraries( App-dev_support PRIVATE ${XCB_LIBRARIES} )
##elseif( CARBONARA_TARGET_PLATFORM STREQUAL "XLIB" )  # xlib support is not implemented
##elseif( CARBONARA_TARGET_PLATFORM STREQUAL "ANDROID" )  # Android support is not implemented
#endif()

target_link_libraries( Memorable_arena PRIVATE Err_msg )

target_link_libraries( Sorter PRIVATE m )

target_link_libraries( carbonara PRIVATE App-dev_support )
target_link_libraries( carbonara PRIVATE Memorable_arena )
target_link_libraries( carbonara PRIVATE Err_msg )
target_link_libraries( carbonara PRIVATE Info_struct )
target_link_libraries( carbonara PRIVATE Util-c_lang )
target_link_libraries( carbonara PRIVATE ${CMAKE_DL_LIBS} )  # for volk
#target_link_libraries( carbonara PRIVATE joltc )

#target_link_libraries( carbonara PRIVATE ${SDL2_LIBRARIES} )
target_link_libraries( carbonara PUBLIC SDL3::SDL3-static )  # NOTE Link to SDL3::SDL3 regardless of if it's static or dynamic, it should map it to the right one for you

target_link_libraries( tester PRIVATE App-dev_support )
target_link_libraries( tester PRIVATE Sorter )
target_link_libraries( tester PRIVATE Info_struct )
target_link_libraries( tester PRIVATE Err_msg )
target_link_libraries( tester PRIVATE Util-c_lang )

target_link_libraries( packer PRIVATE Sorter )
target_link_libraries( packer PRIVATE Util-c_lang )
target_link_libraries( packer PRIVATE App-dev_support )
target_link_libraries( packer PRIVATE Info_struct )
target_link_libraries( packer PRIVATE m )

target_link_libraries( Info_struct PRIVATE Sorter )




# NOTE XXX You can use this block to check the assembly code of target_exec
#set( C_FLAGS ${CMAKE_C_FLAGS} )
#if( CMAKE_BUILD_TYPE MATCHES Release)
#  string( APPEND C_FLAGS " ${CMAKE_C_FLAGS_RELEASE}" )
#elseif( CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
#  string( APPEND C_FLAGS " ${CMAKE_C_FLAGS_RELWITHDEBINFO}" )
#endif()
#string( REGEX REPLACE " " ";" C_FLAGS ${C_FLAGS} )
#get_target_property( SRCS carbonara SOURCES )
#get_target_property( INCS carbonara INCLUDE_DIRECTORIES)
#get_directory_property( DEFS COMPILE_DEFINITIONS )
#foreach( SRC ${SRCS} )
#  string( APPEND SRC_FILES "../../${SRC};" )
#endforeach()
#foreach( INC ${INCS} )
#  string( APPEND INC_DIR_OPTIONS "-I${INC};" )
#endforeach()
#foreach( DEF ${DEFS} )
#  string( APPEND DEF_OPTIONS "-D${DEF};" )
#endforeach()
#execute_process( COMMAND mkdir -p "${PROJECT_BINARY_DIR}/asm_output"
#  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
#add_custom_target(gen_asm_files ALL
#  COMMAND ${CMAKE_C_COMPILER} ${C_FLAGS} -S -fverbose-asm ${DEF_OPTIONS} ${INC_DIR_OPTIONS} ${SRC_FILES}
#  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/asm_output"
#  DEPENDS ${INCS} ${SRCS}
#  COMMENT "Generating .s files for the executable 'target_exec'"
#)



install( TARGETS carbonara DESTINATION bin )
