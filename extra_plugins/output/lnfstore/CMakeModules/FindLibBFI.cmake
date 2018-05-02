#  BFI_FOUND        - System has bloom filter index
#  BFI_INCLUDE_DIRS - Include directories
#  BFI_LIBRARIES    - The libraries needed to use it

find_path(
	BFI_INCLUDE_DIR bf_index.h
	PATH_SUFFIXES include
)

find_library(
	BFI_LIBRARY NAMES bfindex libbfindex
	PATH_SUFFIXES lib lib64
)

# handle the QUIETLY and REQUIRED arguments and set BFI_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibBFI
	REQUIRED_VARS BFI_LIBRARY BFI_INCLUDE_DIR
)

set(BFI_LIBRARIES ${BFI_LIBRARY})
set(BFI_INCLUDE_DIRS ${BFI_INCLUDE_DIR})
mark_as_advanced(BFI_INCLUDE_DIR BFI_LIBRARY)
