#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright © 2021-2024 Inria.  All rights reserved.
# Copyright © 2025 Siemens Corporation and/or its affiliates.  All rights reserved.
# See COPYING in top-level directory.
#
# Linux CMake build for hwloc 2.15

cmake_minimum_required(VERSION 3.15)
project(hwloc
    LANGUAGES C
    VERSION 2.15.1)

enable_testing()

option(HWLOC_ENABLE_TESTING "Enable testing" ON)
option(HWLOC_ENABLE_PLUGINS "Enable plugin support" OFF)
option(HWLOC_SKIP_LSTOPO "don't build/install lstopo")
option(HWLOC_SKIP_TOOLS "don't build/install other hwloc tools")
option(HWLOC_SKIP_INCLUDES "don't install headers")
option(HWLOC_WITH_LIBXML2 "use libxml2 instead of minimal XML")
option(HWLOC_WITH_OPENCL "enable OpenCL support")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.17)
    option(HWLOC_WITH_CUDA "enable CUDA support")
endif()

if(DEFINED HWLOC_BUILD_SHARED_LIBS)
    message(FATAL_ERROR "The option HWLOC_BUILD_SHARED_LIBS has been removed. Please use BUILD_SHARED_LIBS instead.")
endif()

# The hwloc source directory is at .../extern/hwloc relative to this CMakeLists.txt
# PROJECT_SOURCE_DIR = .../extern/hwloc-linux-cmake
# hwloc source = .../extern/hwloc = PROJECT_SOURCE_DIR/../hwloc
set(TOPDIR ${PROJECT_SOURCE_DIR}/../hwloc)

# The hwloc source is at TOPDIR, so include directory is TOPDIR/include
set(HWLOC_INCLUDE_DIR ${TOPDIR}/include CACHE INTERNAL "Path to hwloc source include directory")

# Configure dynamically based on platform capabilities
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckLibraryExists)

check_include_file("dirent.h" HAVE_DIRENT_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("malloc.h" HAVE_MALLOC_H)
check_include_file("memory.h" HAVE_MEMORY_H)
check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("sys/mman.h" HAVE_SYS_MMAN_H)
check_include_file("sys/utsname.h" HAVE_SYS_UTSNAME_H)
check_include_file("numa.h" HAVE_NUMA_H)
check_include_file("numaif.h" HAVE_NUMAIF_H)
check_include_file("pthread.h" HAVE_PTHREAD_H)
check_include_file("pthread_np.h" HAVE_PTHREAD_NP_H)
check_include_file("sched.h" HAVE_SCHED_H)
check_include_file("sys/syscall.h" HAVE_SYS_SYSCALL_H)
check_include_file("linux/version.h" HAVE_LINUX_VERSION_H)
check_include_file("linux/kernel.h" HAVE_LINUX_KERNEL_H)
check_include_file("linux/unistd.h" HAVE_LINUX_UNISTD_H)
check_include_file("sys/param.h" HAVE_SYS_PARAM_H)
check_include_file("sys/cpuset.h" HAVE_SYS_CPUSET_H)
check_include_file("sys/lgrp_user.h" HAVE_SYS_LGRP_USER_H)
check_include_file("sys/sysctl.h" HAVE_SYS_SYSCTL_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("sys/utsname.h" HAVE_SYS_UTSNAME_H)
check_include_file("locale.h" HAVE_LOCALE_H)
check_include_file("langinfo.h" HAVE_LANGINFO_H)
check_include_file("xlocale.h" HAVE_XLOCALE_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("errno.h" HAVE_ERRNO_H)
check_include_file("limits.h" HAVE_LIMITS_H)
check_include_file("assert.h" HAVE_ASSERT_H)
check_include_file("stddef.h" HAVE_STDDEF_H)
check_include_file("endian.h" HAVE_ENDIAN_H)
check_include_file("byteswap.h" HAVE_BYTESWAP_H)
check_include_file("sys/mount.h" HAVE_SYS_MOUNT_H)
check_include_file("mntent.h" HAVE_MNTENT_H)
check_include_file("sched.h" HAVE_SCHED_H)
check_include_file("sys/resource.h" HAVE_SYS_RESOURCE_H)
check_include_file("sys/time.h" HAVE_SYS_TIME_H)
check_include_file("time.h" HAVE_TIME_H)
check_include_file("sys/uio.h" HAVE_SYS_UIO_H)
check_include_file("sys/vfs.h" HAVE_SYS_VFS_H)
check_include_file("sys/statfs.h" HAVE_SYS_STATFS_H)
check_include_file("sys/sysinfo.h" HAVE_SYS_SYSINFO_H)
check_include_file("utime.h" HAVE_UTIME_H)
check_include_file("execinfo.h" HAVE_EXECINFO_H)
check_include_file("link.h" HAVE_LINK_H)
check_include_file("elf.h" HAVE_ELF_H)

check_symbol_exists(mkstemp "stdlib.h" HAVE_MKSTEMP)
check_symbol_exists(memalign "malloc.h" HAVE_MEMALIGN)
check_symbol_exists(posix_memalign "stdlib.h" HAVE_POSIX_MEMALIGN)
check_symbol_exists(strncasecmp "strings.h" HAVE_STRNCASECMP)
check_symbol_exists(strcasecmp "strings.h" HAVE_STRCASECMP)
check_symbol_exists(snprintf "stdio.h" HAVE_SNPRINTF)
check_symbol_exists(strtoull "stdlib.h" HAVE_STRTOULL)
check_symbol_exists(getpagesize "unistd.h" HAVE_GETPAGESIZE)
check_symbol_exists(sysconf "unistd.h" HAVE_SYSCONF)
check_symbol_exists(sysctl "sys/sysctl.h" HAVE_SYSCTL)
check_symbol_exists(sysctlbyname "sys/sysctl.h" HAVE_SYSCTLBYNAME)
check_symbol_exists(uname "sys/utsname.h" HAVE_UNAME)
check_symbol_exists(pthread_getaffinity_np "pthread.h" HAVE_PTHREAD_GETAFFINITY_NP)
check_symbol_exists(pthread_setaffinity_np "pthread.h" HAVE_PTHREAD_SETAFFINITY_NP)
check_symbol_exists(cpuset_setaffinity "sys/cpuset.h" HAVE_CPUSET_SETAFFINITY)
check_symbol_exists(cpuset_setid "sys/cpuset.h" HAVE_CPUSET_SETID)
check_symbol_exists(ffs "strings.h" HAVE_FFS)
check_symbol_exists(ffsl "strings.h" HAVE_FFSL)
check_symbol_exists(fls "strings.h" HAVE_FLS)
check_symbol_exists(flsl "strings.h" HAVE_FLSL)
check_symbol_exists(clz "__builtin_clz" HAVE_CLZ)
check_symbol_exists(clzl "__builtin_clzl" HAVE_CLZL)
check_symbol_exists(setlocale "locale.h" HAVE_SETLOCALE)
check_symbol_exists(nl_langinfo "langinfo.h" HAVE_NL_LANGINFO)
check_symbol_exists(uselocale "xlocale.h" HAVE_USELOCALE)
check_symbol_exists(openat "fcntl.h" HAVE_OPENAT)
check_symbol_exists(host_info "mach/mach_host.h" HAVE_HOST_INFO)

# Check for sysconf constants for fallback_nbprocessors
check_symbol_exists(_SC_NPROCESSORS_CONF "unistd.h" HAVE_DECL__SC_NPROCESSORS_CONF)
check_symbol_exists(_SC_NPROCESSORS_ONLN "unistd.h" HAVE_DECL__SC_NPROCESSORS_ONLN)
check_symbol_exists(_SC_NPROC_CONF "unistd.h" HAVE_DECL__SC_NPROC_CONF)
check_symbol_exists(_SC_NPROC_ONLN "unistd.h" HAVE_DECL__SC_NPROC_ONLN)
check_symbol_exists(_SC_PAGESIZE "unistd.h" HAVE_DECL__SC_PAGESIZE)
check_symbol_exists(_SC_PAGE_SIZE "unistd.h" HAVE_DECL__SC_PAGE_SIZE)
check_symbol_exists(_SC_LARGE_PAGESIZE "unistd.h" HAVE_DECL__SC_LARGE_PAGESIZE)

# Check for syscall numbers on x86_64
check_c_source_compiles("
#include <syscall.h>
#include <unistd.h>
int main(void) { return syscall(__NR_mbind, 0, 0, 0, 0, 0, 0); }" HAVE_SYSCALL_MBIND
)
check_c_source_compiles("
#include <syscall.h>
#include <unistd.h>
int main(void) { return syscall(__NR_set_mempolicy, 0, 0, 0); }" HAVE_SYSCALL_SET_MEMPOLICY
)
check_c_source_compiles("
#include <syscall.h>
#include <unistd.h>
int main(void) { return syscall(__NR_get_mempolicy, 0, 0, 0, 0, 0); }" HAVE_SYSCALL_GET_MEMPOLICY
)
check_c_source_compiles("
#include <syscall.h>
#include <unistd.h>
int main(void) { return syscall(__NR_migrate_pages, 0, 0, 0, 0); }" HAVE_SYSCALL_MIGRATE_PAGES
)
check_c_source_compiles("
#include <syscall.h>
#include <unistd.h>
int main(void) { return syscall(__NR_move_pages, 0, 0, 0, 0, 0, 0); }" HAVE_SYSCALL_MOVE_PAGES
)

set(SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})

# Architecture detection
set(HWLOC_X86_32_ARCH)
set(HWLOC_X86_64_ARCH)
set(HWLOC_HAVE_X86_CPUID 1)
if(CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^(x64|x86_64)$")
    # "x64"    for Windows (MSVC)
    # "x86_64" for Windows (MinGW), Linux, macOS
    set(HWLOC_X86_64_ARCH 1)
elseif(CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^(X86|i[3-6]86)$")
    # "X86"      for Windows (MSVC)
    # "i[3-6]86" for Windows (MinGW), Linux, macOS
    set(HWLOC_X86_32_ARCH 1)
elseif(CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^(aarch64|arm64|ARM64)$")
    set(HWLOC_HAVE_X86_CPUID 0)
else()
    # For other architectures
    set(HWLOC_HAVE_X86_CPUID 0)
endif()
message(STATUS "Target Arch ID = ${CMAKE_C_COMPILER_ARCHITECTURE_ID}")
message(STATUS "HWLOC_HAVE_X86_CPUID = ${HWLOC_HAVE_X86_CPUID}")

# Check for x86 CPUID support
check_c_source_compiles("
#include <cpuid.h>
int main(void) {
    unsigned int eax, ebx, ecx, edx;
    __cpuid(0, eax, ebx, ecx, edx);
    return 0;
}" HWLOC_HAVE_GCC_CPUID
)

check_c_source_compiles("
int main(void) {
    int cpuinfo[4];
    __cpuidex(cpuinfo, 0, 0);
    return 0;
}" HWLOC_HAVE_MSVC_CPUIDEX
)

# Check for libnuma
check_library_exists(numa numa_available "" HWLOC_HAVE_LIBNUMA)
if(HWLOC_HAVE_LIBNUMA)
    check_symbol_exists(numa_available "numa.h" HAVE_NUMA_AVAILABLE)
    check_symbol_exists(numa_num_configured_cpus "numa.h" HAVE_NUMA_NUM_CONFIGURED_CPUS)
    check_symbol_exists(numa_bitmask_alloc "numa.h" HAVE_NUMA_BITMASK_ALLOC)
    check_symbol_exists(numa_alloc_onnode "numa.h" HAVE_NUMA_ALLOC_ONNODE)
    check_symbol_exists(numa_move_pages "numa.h" HAVE_NUMA_MOVE_PAGES)
    check_symbol_exists(numa_distance "numa.h" HAVE_NUMA_DISTANCE)
    check_symbol_exists(numa_get_distance "numa.h" HAVE_NUMA_GET_DISTANCE)
    check_symbol_exists(numa_node_size64 "numa.h" HAVE_NUMA_NODE_SIZE64)
    check_symbol_exists(numa_node_size "numa.h" HAVE_NUMA_NODE_SIZE)
    check_symbol_exists(numa_max_node "numa.h" HAVE_NUMA_MAX_NODE)
    check_symbol_exists(numa_max_possible_node "numa.h" HAVE_NUMA_MAX_POSSIBLE_NODE)
    check_symbol_exists(numa_tonode_memory "numa.h" HAVE_NUMA_TONODE_MEMORY)
    check_symbol_exists(get_mempolicy "sys/mman.h" HAVE_GET_MEMPOLICY)
    check_symbol_exists(mbind "sys/mman.h" HAVE_MBIND)
    check_symbol_exists(set_mempolicy "sys/mman.h" HAVE_SET_MEMPOLICY)
    check_symbol_exists(move_pages "sys/mman.h" HAVE_MOVE_PAGES)
endif()

# Check for libudev
check_library_exists(udev udev_new "" HWLOC_HAVE_LIBUDEV)
if(HWLOC_HAVE_LIBUDEV)
    check_symbol_exists(udev_new "libudev.h" HAVE_UDEV_NEW)
    check_symbol_exists(udev_enumerate_new "libudev.h" HAVE_UDEV_ENUMERATE_NEW)
    check_symbol_exists(udev_device_new_from_syspath "libudev.h" HAVE_UDEV_DEVICE_NEW_FROM_SYSPATH)
    check_symbol_exists(udev_device_get_sysattr_value "libudev.h" HAVE_UDEV_DEVICE_GET_SYSATTR_VALUE)
    check_symbol_exists(udev_device_get_property_value "libudev.h" HAVE_UDEV_DEVICE_GET_PROPERTY_VALUE)
    check_symbol_exists(udev_device_get_parent "libudev.h" HAVE_UDEV_DEVICE_GET_PARENT)
    check_symbol_exists(udev_device_get_devnum "libudev.h" HAVE_UDEV_DEVICE_GET_DEVNUM)
    check_symbol_exists(udev_device_get_devtype "libudev.h" HAVE_UDEV_DEVICE_GET_DEVTYPE)
    check_symbol_exists(udev_device_get_subsystem "libudev.h" HAVE_UDEV_DEVICE_GET_SUBSYSTEM)
    check_symbol_exists(udev_device_get_sysname "libudev.h" HAVE_UDEV_DEVICE_GET_SYSNAME)
    check_symbol_exists(udev_device_get_syspath "libudev.h" HAVE_UDEV_DEVICE_GET_SYSPATH)
    check_symbol_exists(udev_device_get_devnode "libudev.h" HAVE_UDEV_DEVICE_GET_DEVNODE)
    check_symbol_exists(udev_device_unref "libudev.h" HAVE_UDEV_DEVICE_UNREF)
    check_symbol_exists(udev_enumerate_scan_devices "libudev.h" HAVE_UDEV_ENUMERATE_SCAN_DEVICES)
    check_symbol_exists(udev_enumerate_get_list_entry "libudev.h" HAVE_UDEV_ENUMERATE_GET_LIST_ENTRY)
    check_symbol_exists(udev_list_entry_get_name "libudev.h" HAVE_UDEV_LIST_ENTRY_GET_NAME)
    check_symbol_exists(udev_list_entry_get_value "libudev.h" HAVE_UDEV_LIST_ENTRY_GET_VALUE)
    check_symbol_exists(udev_list_entry_get_next "libudev.h" HAVE_UDEV_LIST_ENTRY_GET_NEXT)
    check_symbol_exists(udev_enumerate_unref "libudev.h" HAVE_UDEV_ENUMERATE_UNREF)
    check_symbol_exists(udev_unref "libudev.h" HAVE_UDEV_UNREF)
endif()

# Check for libpciaccess
check_library_exists(pciaccess pci_system_init "" HWLOC_HAVE_LIBPCIACCESS)
if(HWLOC_HAVE_LIBPCIACCESS)
    check_symbol_exists(pci_system_init "pciaccess.h" HAVE_PCI_SYSTEM_INIT)
    check_symbol_exists(pci_device_probe "pciaccess.h" HAVE_PCI_DEVICE_PROBE)
    check_symbol_exists(pci_device_find_by_slot "pciaccess.h" HAVE_PCI_DEVICE_FIND_BY_SLOT)
    check_symbol_exists(pci_device_get_bus "pciaccess.h" HAVE_PCI_DEVICE_GET_BUS)
    check_symbol_exists(pci_device_get_dev "pciaccess.h" HAVE_PCI_DEVICE_GET_DEV)
    check_symbol_exists(pci_device_get_func "pciaccess.h" HAVE_PCI_DEVICE_GET_FUNC)
    check_symbol_exists(pci_device_get_vendor_id "pciaccess.h" HAVE_PCI_DEVICE_GET_VENDOR_ID)
    check_symbol_exists(pci_device_get_device_id "pciaccess.h" HAVE_PCI_DEVICE_GET_DEVICE_ID)
    check_symbol_exists(pci_device_get_subvendor_id "pciaccess.h" HAVE_PCI_DEVICE_GET_SUBVENDOR_ID)
    check_symbol_exists(pci_device_get_subdevice_id "pciaccess.h" HAVE_PCI_DEVICE_GET_SUBDEVICE_ID)
    check_symbol_exists(pci_device_get_revision "pciaccess.h" HAVE_PCI_DEVICE_GET_REVISION)
    check_symbol_exists(pci_device_read_rom "pciaccess.h" HAVE_PCI_DEVICE_READ_ROM)
    check_symbol_exists(pci_device_map_range "pciaccess.h" HAVE_PCI_DEVICE_MAP_RANGE)
    check_symbol_exists(pci_device_unmap_range "pciaccess.h" HAVE_PCI_DEVICE_UNMAP_RANGE)
    check_symbol_exists(pci_device_destroy "pciaccess.h" HAVE_PCI_DEVICE_DESTROY)
    check_symbol_exists(pci_system_cleanup "pciaccess.h" HAVE_PCI_SYSTEM_CLEANUP)
endif()

# Check for hwloc backend support
check_symbol_exists(sched_setaffinity "sched.h" HWLOC_HAVE_SCHED_SETAFFINITY)
check_symbol_exists(sched_getaffinity "sched.h" HWLOC_HAVE_SCHED_GETAFFINITY)
check_symbol_exists(cpu_set_t "sched.h" HWLOC_HAVE_CPU_SET_T)
check_symbol_exists(CPU_ZERO "sched.h" HWLOC_HAVE_CPU_ZERO)
check_symbol_exists(CPU_SET "sched.h" HWLOC_HAVE_CPU_SET)
check_symbol_exists(CPU_ISSET "sched.h" HWLOC_HAVE_CPU_ISSET)
check_symbol_exists(CPU_COUNT "sched.h" HWLOC_HAVE_CPU_COUNT)

# Check for libdl
check_library_exists(dl dlopen "" HWLOC_HAVE_LIBDL)
if(HWLOC_HAVE_LIBDL)
    set(HAVE_DLFCN_H 1)
endif()

# Check for librt (for clock_gettime etc)
check_library_exists(rt clock_gettime "" HWLOC_HAVE_LIBRT)

# Check for libpthread
check_library_exists(pthread pthread_create "" HWLOC_HAVE_LIBPTHREAD)

# Check for libm
check_library_exists(m sin "" HWLOC_HAVE_LIBM)

# --- optional external libraries
set(HWLOC_HAVE_LIBXML2 0)
if(HWLOC_WITH_LIBXML2)
    find_package(LibXml2 REQUIRED)
    set(HWLOC_HAVE_LIBXML2 1)
endif()

set(HWLOC_HAVE_OPENCL 0)
if(HWLOC_WITH_OPENCL)
    find_package(OpenCL REQUIRED)
    set(HWLOC_HAVE_OPENCL 1)
endif()

set(HAVE_CUDA 0)
set(HAVE_CUDA_H)
set(HAVE_CUDA_RUNTIME_API_H)
set(HWLOC_HAVE_CUDART)
if(HWLOC_WITH_CUDA)
    find_package(CUDAToolkit REQUIRED)
    set(HAVE_CUDA 1)
    set(HAVE_CUDA_H 1)
    set(HAVE_CUDA_RUNTIME_API_H 1)
    set(HWLOC_HAVE_CUDART 1)
endif()

set(HWLOC_HAVE_PLUGINS 0)
set(HWLOC_ENABLED_PLUGINS_LIST)
if(HWLOC_ENABLE_PLUGINS)
    # this has to happen before the first add_library call
    if(NOT DEFINED BUILD_SHARED_LIBS)
        message(STATUS "HWLOC plugin support requires BUILD_SHARED_LIBS to be enabled, but it was not set. Setting BUILD_SHARED_LIBS to ON")
        set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
    elseif(NOT BUILD_SHARED_LIBS)
        message(FATAL_ERROR "HWLOC plugin support requires BUILD_SHARED_LIBS to be enabled, but BUILD_SHARED_LIBS is set to ${BUILD_SHARED_LIBS}.")
    endif()

    set(HWLOC_HAVE_PLUGINS 1)
    set(HWLOC_PLUGINS_PATH ${CMAKE_INSTALL_PREFIX}/lib/hwloc)

    if(HWLOC_HAVE_CUDART)
        list(APPEND HWLOC_ENABLED_PLUGINS_LIST "cuda")
    endif()

    if(HWLOC_HAVE_OPENCL)
        list(APPEND HWLOC_ENABLED_PLUGINS_LIST "opencl")
    endif()

    if(HWLOC_HAVE_LIBXML2)
        list(APPEND HWLOC_ENABLED_PLUGINS_LIST "xml_libxml")
    endif()
endif()

# Set variables for the public config.h.in
set(HWLOC_VERSION "2.15.1")
set(HWLOC_VERSION_MAJOR 2)
set(HWLOC_VERSION_MINOR 15)
set(HWLOC_VERSION_RELEASE 1)
set(HWLOC_VERSION_GREEK "rc1")
# HWLOC_C_HAVE_VISIBILITY=0 means no visibility attribute (HWLOC_DECLSPEC will be empty)
set(HWLOC_C_HAVE_VISIBILITY 0)

# Add RUNSTATEDIR for hwloc 2.15
if(NOT DEFINED RUNSTATEDIR)
    set(RUNSTATEDIR "/run")
endif()
add_compile_definitions(RUNSTATEDIR="${RUNSTATEDIR}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/hwloc/autogen/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/private_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/private/autogen/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/static-components.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/static-components.h)

# Library - hwloc 2.15 sources
add_compile_definitions($<$<CONFIG:DEBUG>:HWLOC_DEBUG=1>)

add_library(hwloc
    ${TOPDIR}/hwloc/topology.c
    ${TOPDIR}/hwloc/traversal.c
    ${TOPDIR}/hwloc/distances.c
    ${TOPDIR}/hwloc/memattrs.c
    ${TOPDIR}/hwloc/cpukinds.c
    ${TOPDIR}/hwloc/components.c
    ${TOPDIR}/hwloc/bind.c
    ${TOPDIR}/hwloc/bitmap.c
    ${TOPDIR}/hwloc/pci-common.c
    ${TOPDIR}/hwloc/diff.c
    ${TOPDIR}/hwloc/shmem.c
    ${TOPDIR}/hwloc/misc.c
    ${TOPDIR}/hwloc/base64.c
    ${TOPDIR}/hwloc/topology-noos.c
    ${TOPDIR}/hwloc/topology-synthetic.c
    ${TOPDIR}/hwloc/topology-xml.c
    ${TOPDIR}/hwloc/topology-xml-nolibxml.c
    ${TOPDIR}/hwloc/topology-linux.c
    ${TOPDIR}/hwloc/topology-hardwired.c
    ${TOPDIR}/hwloc/topology-pci.c
    $<$<BOOL:${HWLOC_HAVE_X86_CPUID}>:${TOPDIR}/hwloc/topology-x86.c>
    $<$<AND:$<BOOL:${HWLOC_HAVE_LIBXML2}>,$<NOT:$<BOOL:${HWLOC_HAVE_PLUGINS}>>>:${TOPDIR}/hwloc/topology-xml-libxml.c>
    $<$<AND:$<BOOL:${HWLOC_HAVE_OPENCL}>,$<NOT:$<BOOL:${HWLOC_HAVE_PLUGINS}>>>:${TOPDIR}/hwloc/topology-opencl.c>
    $<$<AND:$<BOOL:${HAVE_CUDA}>,$<NOT:$<BOOL:${HWLOC_HAVE_PLUGINS}>>>:${TOPDIR}/hwloc/topology-cuda.c>
    $<$<BOOL:${HWLOC_HAVE_LIBNUMA}>:${TOPDIR}/hwloc/topology-linux.c>
    $<$<BOOL:${HWLOC_HAVE_LIBPCIACCESS}>:${TOPDIR}/hwloc/topology-pci.c>
    $<$<BOOL:${HWLOC_HAVE_LIBUDEV}>:${TOPDIR}/hwloc/topology-linux.c>
    $<$<BOOL:${WIN32}>:${TOPDIR}/hwloc/topology-windows.c>
    $<$<NOT:$<BOOL:${WIN32}>>:${TOPDIR}/hwloc/topology-hardwired.c>
)

target_link_libraries(hwloc PRIVATE
    $<$<AND:$<BOOL:${HWLOC_HAVE_LIBXML2}>,$<NOT:$<BOOL:${HWLOC_HAVE_PLUGINS}>>>:LibXml2::LibXml2>
    $<$<AND:$<BOOL:${HWLOC_HAVE_OPENCL}>,$<NOT:$<BOOL:${HWLOC_HAVE_PLUGINS}>>>:OpenCL::OpenCL>
    "$<$<AND:$<BOOL:${HAVE_CUDA}>,$<NOT:$<BOOL:${HWLOC_HAVE_PLUGINS}>>>:CUDA::cudart;CUDA::cuda_driver>"
    $<$<BOOL:${HWLOC_HAVE_LIBNUMA}>:numa>
    $<$<BOOL:${HWLOC_HAVE_LIBUDEV}>:udev>
    $<$<BOOL:${HWLOC_HAVE_LIBPCIACCESS}>:pciaccess>
    $<$<BOOL:${HWLOC_HAVE_LIBDL}>:dl>
    $<$<BOOL:${HWLOC_HAVE_LIBRT}>:rt>
    $<$<BOOL:${HWLOC_HAVE_LIBPTHREAD}>:pthread>
    $<$<BOOL:${HWLOC_HAVE_LIBM}>:m>
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(hwloc PRIVATE HWLOC_DLL_EXPORTS)
endif()

target_include_directories(hwloc PUBLIC
    "$<BUILD_INTERFACE:${HWLOC_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/include>"
    $<INSTALL_INTERFACE:include>
)

# plugin support
foreach(plugin IN LISTS HWLOC_ENABLED_PLUGINS_LIST)
    # custom settings per plugin
    if(plugin STREQUAL "cuda")
        add_library(hwloc_${plugin} MODULE ${TOPDIR}/hwloc/topology-${plugin}.c)
        target_link_libraries(hwloc_${plugin} PRIVATE hwloc CUDA::cudart CUDA::cuda_driver)
    elseif(plugin STREQUAL "opencl")
        add_library(hwloc_${plugin} MODULE ${TOPDIR}/hwloc/topology-${plugin}.c)
        target_link_libraries(hwloc_${plugin} PRIVATE hwloc OpenCL::OpenCL)
    elseif(plugin STREQUAL "xml_libxml")
        add_library(hwloc_${plugin} MODULE ${TOPDIR}/hwloc/topology-xml-libxml.c)
        target_link_libraries(hwloc_${plugin} PRIVATE hwloc LibXml2::LibXml2)
    endif()

    # common settings for all plugins
    target_compile_definitions(hwloc_${plugin} PRIVATE "HWLOC_INSIDE_PLUGIN")

    target_include_directories(hwloc_${plugin} PUBLIC
        "$<BUILD_INTERFACE:${TOPDIR}/include;${CMAKE_CURRENT_BINARY_DIR}/include>"
        $<INSTALL_INTERFACE:include>
    )
endforeach()

if(HWLOC_ENABLE_PLUGINS)
    message(STATUS "Enabled plugins:")
    foreach(plugin IN LISTS HWLOC_ENABLED_PLUGINS_LIST)
        message(STATUS "  - ${plugin}")
    endforeach()
endif()

# Tools under utils/hwloc

if(NOT HWLOC_SKIP_TOOLS)

    set(TOOLS
        hwloc-bind
        hwloc-calc
        hwloc-diff
        hwloc-distrib
        hwloc-info
        hwloc-patch
    )

    if(HWLOC_HAVE_X86_CPUID)
        list(APPEND TOOLS hwloc-gather-cpuid)
    endif()

    foreach(tool IN ITEMS ${TOOLS})
        add_executable(${tool}
            ${TOPDIR}/utils/hwloc/${tool}.c)
        target_link_libraries(${tool} PRIVATE hwloc)
    endforeach(tool)

endif()

# lstopo

if(NOT HWLOC_SKIP_LSTOPO)

    set(LSTOPOS
        lstopo-no-graphics
        lstopo
    )

    set(LSTOPO_COMMON_SOURCES
        ${TOPDIR}/utils/lstopo/lstopo.c
        ${TOPDIR}/utils/lstopo/lstopo-draw.c
        ${TOPDIR}/utils/lstopo/lstopo-tikz.c
        ${TOPDIR}/utils/lstopo/lstopo-fig.c
        ${TOPDIR}/utils/lstopo/lstopo-svg.c
        ${TOPDIR}/utils/lstopo/lstopo-ascii.c
        ${TOPDIR}/utils/lstopo/lstopo-text.c
        ${TOPDIR}/utils/lstopo/lstopo-xml.c
        ${TOPDIR}/utils/hwloc/common-ps.c
    )

    add_executable(lstopo-no-graphics
        ${LSTOPO_COMMON_SOURCES}
    )
    target_link_libraries(lstopo-no-graphics PRIVATE hwloc)

    add_executable(lstopo
        ${LSTOPO_COMMON_SOURCES}
    )
    target_compile_definitions(lstopo PRIVATE LSTOPO_HAVE_GRAPHICS)

    foreach(tool IN ITEMS ${LSTOPOS})
        target_include_directories(${tool} PRIVATE ${TOPDIR}/utils/hwloc)
        target_link_libraries(${tool} PRIVATE hwloc)
    endforeach(tool)

endif()

# Misc

foreach(target IN ITEMS hwloc ${TOOLS} ${LSTOPOS})
    target_compile_definitions(${target} PRIVATE $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>)
endforeach(target)

# Install

install(TARGETS hwloc)

# install plugins
foreach(plugin IN LISTS HWLOC_ENABLED_PLUGINS_LIST)
    install(TARGETS hwloc_${plugin}
        LIBRARY DESTINATION lib/hwloc
        ARCHIVE DESTINATION lib/hwloc
        RUNTIME DESTINATION bin/hwloc
    )
endforeach()

if(NOT HWLOC_SKIP_TOOLS)
    install(TARGETS ${TOOLS})
endif()

if(NOT HWLOC_SKIP_LSTOPO)
    install(TARGETS ${LSTOPOS})
endif()

if(NOT HWLOC_SKIP_INCLUDES)
    install(FILES ${TOPDIR}/include/hwloc.h TYPE INCLUDE)
    install(DIRECTORY ${TOPDIR}/include/hwloc TYPE INCLUDE FILES_MATCHING PATTERN "*.h")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/hwloc/autogen/config.h DESTINATION include/hwloc/autogen)
endif()

if(MSVC AND BUILD_SHARED_LIBS)
  install(FILES $<TARGET_PDB_FILE:hwloc>
    TYPE BIN
    OPTIONAL)
endif()

# Testing

if(HWLOC_ENABLE_TESTING)
    add_subdirectory(tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()