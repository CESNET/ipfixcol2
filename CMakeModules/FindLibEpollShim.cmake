#  LIBEPOLLSHIM_FOUND - System has libepoll-shim
#  LIBEPOLLSHIM_INCLUDE_DIRS - The libepoll-shim include directories
#  LIBEPOLLSHIM_LIBRARIES - The libraries needed to use libepoll-shim
#  LIBEPOLLSHIM_DEFINITIONS - Compiler switches required for using libepoll-shim

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_EPOLLSHIM QUIET epoll-shim)
    set(LIBEPOLLSHIM_DEFINITIONS ${PC_EPOLLSHIM_CFLAGS_OTHER})
endif()

find_path(
    EPOLLSHIM_INCLUDE_DIR libepoll-shim/sys/epoll.h
    HINTS ${PC_EPOLLSHIM_INCLUDEDIR} ${PC_EPOLLSHIM_INCLUDE_DIRS}
    PATH_SUFFIXES include
)
find_library(
    EPOLLSHIM_LIBRARY NAMES epoll-shim libepoll-shim
    HINTS ${PC_EPOLLSHIM_LIBDIR} ${PC_EPOLLSHIM_LIBRARY_DIRS}
    PATH_SUFFIXES lib lib64
)

if (PC_EPOLLSHIM_VERSION)
    # Version extracted from pkg-config
    set(EPOLLSHIM_VERSION_STRING ${PC_EPOLLSHIM_VERSION})
endif()

# handle the QUIETLY and REQUIRED arguments and set LIBEPOLLSHIM_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibEpollShim
    REQUIRED_VARS EPOLLSHIM_LIBRARY EPOLLSHIM_INCLUDE_DIR
    VERSION_VAR EPOLLSHIM_VERSION_STRING
)

set(LIBEPOLLSHIM_LIBRARIES ${EPOLLSHIM_LIBRARY})
set(LIBEPOLLSHIM_INCLUDE_DIRS ${EPOLLSHIM_INCLUDE_DIR}/libepoll-shim)
mark_as_advanced(EPOLLSHIM_INCLUDE_DIR EPOLLSHIM_LIBRARY)
