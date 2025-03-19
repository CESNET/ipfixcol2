#  LIBLZ4_FOUND - System has LZ4
#  LIBLZ4_INCLUDE_DIRS - The LZ4 include directories
#  LIBLZ4_LIBRARIES - The libraries needed to use LZ4

find_path(
    LZ4_INCLUDE_DIR lz4.h
    PATH_SUFFIXES include
)

find_library(
    LZ4_LIBRARY
    NAMES lz4 liblz4
    PATH_SUFFIXES lib lib64
)

set(LZ4_HEADER_FILE "${LZ4_INCLUDE_DIR}/lz4.h")
if (LZ4_INCLUDE_DIR AND EXISTS ${LZ4_HEADER_FILE})
    # Try to extract library version from the header file
    file(STRINGS ${LZ4_HEADER_FILE} lz4_major_define
        REGEX "^#define[\t ]+LZ4_VERSION_MAJOR[\t ]+[0-9]+"
        LIMIT_COUNT 1
    )
    file(STRINGS ${LZ4_HEADER_FILE} lz4_minor_define
        REGEX "^#define[\t ]+LZ4_VERSION_MINOR[\t ]+[0-9]+"
        LIMIT_COUNT 1
    )
    file(STRINGS ${LZ4_HEADER_FILE} lz4_release_define
        REGEX "^#define[\t ]+LZ4_VERSION_RELEASE[\t ]+[0-9]+"
        LIMIT_COUNT 1
    )

    string(REGEX REPLACE "^#define[\t ]+LZ4_VERSION_MAJOR[\t ]+([0-9]+).*" "\\1"
        lz4_major_num ${lz4_major_define})
    string(REGEX REPLACE "^#define[\t ]+LZ4_VERSION_MINOR[\t ]+([0-9]+).*" "\\1"
        lz4_minor_num ${lz4_minor_define})
    string(REGEX REPLACE "^#define[\t ]+LZ4_VERSION_RELEASE[\t ]+([0-9]+).*" "\\1"
        lz4_release_num ${lz4_release_define})

    set(LZ4_VERSION_STRING "${lz4_major_num}.${lz4_minor_num}.${lz4_release_num}")
endif()
unset(LZ4_HEADER_FILE)

# Handle the REQUIRED arguments and set LIBLZ4_FOUND to TRUE if all listed
# variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibLz4
    REQUIRED_VARS LZ4_LIBRARY LZ4_INCLUDE_DIR
    VERSION_VAR LZ4_VERSION_STRING
)

set(LIBLZ4_LIBRARIES ${LZ4_LIBRARY})
set(LIBLZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR})
mark_as_advanced(LZ4_INCLUDE_DIR LZ4_LIBRARY)
