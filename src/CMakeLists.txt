if (POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
endif()

# Configure a header file to pass some CMake variables
configure_file(
    "${PROJECT_SOURCE_DIR}/src/build_config.h.in"
    "${PROJECT_BINARY_DIR}/src/build_config.h"
)

# Find libfds
find_package(LibFds 0.1.0 REQUIRED)
# Find rst2man

if (ENABLE_DOC_MANPAGE)
    find_package(Rst2Man)
    if (NOT RST2MAN_FOUND)
        message(FATAL_ERROR "rst2man is not available. Install python-docutils or disable manual page generation (-DENABLE_DOC_MANPAGE=False)")
    endif()
endif()

# Find pthreads
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

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
