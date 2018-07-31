#  UNIREC_FOUND - System has libfds
#  UNIREC_INCLUDE_DIRS - The libfds include directories
#  UNIREC_LIBRARIES - The libraries needed to use libfds
#  UNIREC_DEFINITIONS - Compiler switches required for using libfds

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig)
pkg_check_modules(PC_UNIREC QUIET LibTrap)
set(UNIREC_DEFINITIONS ${PC_UNIREC_CFLAGS_OTHER})

find_path(
	UNIREC_INCLUDE_DIR unirec.h
	HINTS ${PC_UNIREC_INCLUDEDIR} ${PC_UNIREC_INCLUDE_DIRS}
	PATH_SUFFIXES include/unirec
)

find_library(
	UNIREC_LIBRARY NAMES unirec
	HINTS ${PC_UNIREC_LIBDIR} ${PC_UNIREC_LIBRARY_DIRS}
	PATH_SUFFIXES lib lib64
)

# handle the QUIETLY and REQUIRED arguments and set LIBUNIREC_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libtrap
	REQUIRED_VARS UNIREC_LIBRARY UNIREC_INCLUDE_DIR
	VERSION_VAR UNIREC_VERSION_STRING
)

set(UNIREC_LIBRARIES ${UNIREC_LIBRARY})
set(UNIREC_INCLUDE_DIRS ${UNIREC_INCLUDE_DIR})
mark_as_advanced(UNIREC_INCLUDE_DIR UNIREC_LIBRARIES)
