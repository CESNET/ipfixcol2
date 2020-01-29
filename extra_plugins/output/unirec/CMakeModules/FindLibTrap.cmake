#  LIBTRAP_FOUND - System has libfds
#  LIBTRAP_INCLUDE_DIRS - The libfds include directories
#  LIBTRAP_LIBRARIES - The libraries needed to use libfds
#  LIBTRAP_DEFINITIONS - Compiler switches required for using libfds

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(PC_LIBTRAP QUIET "libtrap")
endif()
set(LIBTRAP_DEFINITIONS ${PC_LIBTRAP_CFLAGS_OTHER})

find_path(
	LIBTRAP_INCLUDE_DIR libtrap/trap.h
	HINTS ${PC_LIBTRAP_INCLUDEDIR} ${PC_LIBTRAP_INCLUDE_DIRS}
	PATH_SUFFIX include
)

find_library(
	LIBTRAP_LIBRARY NAMES trap libtrap
	HINTS ${PC_LIBTRAP_LIBDIR} ${PC_LIBTRAP_LIBRARY_DIRS}
	PATH_SUFFIXES lib lib64
)

if (PC_LIBTRAP_VERSION)
	# Version extracted from pkg-config
	set(LIBTRAP_VERSION_STRING ${PC_LIBTRAP_VERSION})
elseif (LIBTRAP_INCLUDE_DIR AND LIBTRAP_LIBRARY)
	# Try to get the version of the installed library
	try_run(
		TRAP_RES_RUN TRAP_RES_COMP
		${CMAKE_CURRENT_BINARY_DIR}/try_run/trap_version_test/
		${PROJECT_SOURCE_DIR}/CMakeModules/try_run/trap_version.c
		CMAKE_FLAGS
		  -DLINK_LIBRARIES=${LIBTRAP_LIBRARY}
		  -DINCLUDE_DIRECTORIES=${LIBTRAP_INCLUDE_DIR}
		RUN_OUTPUT_VARIABLE LIBTRAP_VERSION_VAR
    )

	if (TRAP_RES_COMP AND TRAP_RES_RUN EQUAL 0)
		# Successfully compiled and executed with return code 0
		set(LIBTRAP_VERSION_STRING ${LIBTRAP_VERSION_VAR})
	endif()
endif()

# handle the QUIETLY and REQUIRED arguments and set LIBTRAP_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibTrap
	REQUIRED_VARS LIBTRAP_LIBRARY LIBTRAP_INCLUDE_DIR
	VERSION_VAR LIBTRAP_VERSION_STRING
)

set(LIBTRAP_LIBRARIES ${LIBTRAP_LIBRARY})
set(LIBTRAP_INCLUDE_DIRS ${LIBTRAP_INCLUDE_DIR})
mark_as_advanced(LIBTRAP_INCLUDE_DIR LIBTRAP_LIBRARY)
