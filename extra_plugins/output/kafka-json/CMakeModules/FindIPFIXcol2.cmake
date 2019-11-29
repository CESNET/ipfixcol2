#  IPFIXCOL2_FOUND        - System has IPFIXcol
#  IPFIXCOL2_INCLUDE_DIRS - The IPFIXcol include directories
#  IPFIXCOL2_DEFINITIONS  - Compiler switches required for using IPFIXcol

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig)
pkg_check_modules(PC_IPFIXCOL QUIET ipfixcol2)
set(IPFIXCOL2_DEFINITIONS ${PC_IPFIXCOL_CFLAGS_OTHER})

find_path(
	IPFIXCOL2_INCLUDE_DIR ipfixcol2.h
	HINTS ${PC_IPFIXCOL_INCLUDEDIR} ${PC_IPFIXCOL_INCLUDE_DIRS}
	PATH_SUFFIXES include
)

if (PC_IPFIXCOL_VERSION)
    # Version extracted from pkg-config
    set(IPFIXCOL_VERSION_STRING ${PC_IPFIXCOL_VERSION})
elseif(IPFIXCOL2_INCLUDE_DIR AND EXISTS "${IPFIXCOL2_INCLUDE_DIR}/ipfixcol2/api.h")
    # Try to extract library version from a header file
    file(STRINGS "${IPFIXCOL2_INCLUDE_DIR}/ipfixcol2/api.h" ipfixcol_version_str
         REGEX "^#define[\t ]+IPX_API_VERSION_STR[\t ]+\".*\"")

    string(REGEX REPLACE "^#define[\t ]+IPX_API_VERSION_STR[\t ]+\"([^\"]*)\".*" "\\1"
		IPFIXCOL_VERSION_STRING "${ipfixcol_version_str}")
    unset(ipfixcol_version_str)
endif()

# handle the QUIETLY and REQUIRED arguments and set IPFIXCOL2_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IPFIXcol2
	REQUIRED_VARS IPFIXCOL2_INCLUDE_DIR
	VERSION_VAR IPFIXCOL_VERSION_STRING
)

set(IPFIXCOL2_INCLUDE_DIRS ${IPFIXCOL2_INCLUDE_DIR})
mark_as_advanced(IPFIXCOL2_INCLUDE_DIR)
