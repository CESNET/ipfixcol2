#  NF_FOUND     - System has libnf
#  NF_INCLUDE_DIRS - The libnf include directories
#  NF_LIBRARIES    - The libraries needed to use libnf

find_path(
	NF_INCLUDE_DIR libnf.h
	PATH_SUFFIXES include
)

find_library(
	NF_LIBRARY NAMES nf libnf
	PATH_SUFFIXES lib lib64
)

# handle the QUIETLY and REQUIRED arguments and set NF_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibNf
	REQUIRED_VARS NF_LIBRARY NF_INCLUDE_DIR
)

set(NF_LIBRARIES ${NF_LIBRARY})
set(NF_INCLUDE_DIRS ${NF_INCLUDE_DIR})
mark_as_advanced(NF_INCLUDE_DIR NF_LIBRARY)
