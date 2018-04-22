cmake_policy(SET CMP0063 NEW)

# Configure a header file to pass some CMake variables
configure_file(
	"${PROJECT_SOURCE_DIR}/src/build_config.h.in"
	"${PROJECT_BINARY_DIR}/src/build_config.h"
)

# Find libfds
find_package(LibFds 0.1.0 REQUIRED)
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