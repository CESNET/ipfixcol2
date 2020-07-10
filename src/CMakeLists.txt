if (POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
endif()

include(CheckSymbolExists)
check_symbol_exists("RTLD_DEEPBIND" "dlfcn.h" HAVE_RTLD_DEEPBIND)

# Configure a header file to pass some CMake variables
configure_file(
    "${PROJECT_SOURCE_DIR}/src/build_config.h.in"
    "${PROJECT_BINARY_DIR}/src/build_config.h"
)

# Header files for source code building
include_directories(
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_BINARY_DIR}/include/"  # for api.h
    "${PROJECT_BINARY_DIR}/src/"      # for build_config.h
    "${FDS_INCLUDE_DIRS}"             # libfds header files
)

# Project subdirectories
add_subdirectory(core)
add_subdirectory(plugins)
add_subdirectory(tools)
