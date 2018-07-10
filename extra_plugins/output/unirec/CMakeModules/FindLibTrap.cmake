#  LIBTRAP_FOUND - System has libfds
#  LIBTRAP_INCLUDE_DIRS - The libfds include directories
#  LIBTRAP_LIBRARIES - The libraries needed to use libfds
#  LIBTRAP_DEFINITIONS - Compiler switches required for using libfds

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig)
pkg_check_modules(PC_LIBTRAP QUIET LibTrap)
set(LIBTRAP_DEFINITIONS ${PC_LIBTRAP_CFLAGS_OTHER})

find_path(
	LIBTRAP_INCLUDE_DIR trap.h
	HINTS ${PC_LIBTRAP_INCLUDEDIR} ${PC_LIBTRAP_INCLUDE_DIRS}
	PATH_SUFFIXES include/libtrap
)

find_library(
	LIBTRAP_LIBRARY NAMES trap
	HINTS ${PC_LIBTRAP_LIBDIR} ${PC_LIBTRAP_LIBRARY_DIRS}
	PATH_SUFFIXES lib lib64
)

# handle the QUIETLY and REQUIRED arguments and set LIBLIBTRAP_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libtrap
	REQUIRED_VARS LIBTRAP_LIBRARY LIBTRAP_INCLUDE_DIR
	VERSION_VAR LIBTRAP_VERSION_STRING
)

set(LIBTRAP_LIBRARIES ${LIBTRAP_LIBRARY})
set(LIBTRAP_INCLUDE_DIRS ${LIBTRAP_INCLUDE_DIR})
mark_as_advanced(LIBTRAP_INCLUDE_DIR LIBTRAP_LIBRARIES)
