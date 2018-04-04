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

# Core source files
set(CORE_SOURCE
	"${PROJECT_BINARY_DIR}/src/build_config.h"
	core/config_file.cpp
	core/configurator.cpp
	core/configurator.h
	core/context.c
	core/context.h
	core/fpipe.c
	core/fpipe.h
	core/message_base.c
	core/message_base.h
	core/message_garbage.c
	core/message_ipfix.c
	core/message_ipfix.h
	core/message_session.c
	core/parser.c
	core/parser.h
	core/parser_plugin.c
	core/parser_plugin.h
	core/ring.c
	core/ring.h
	core/context.c
	core/context.h
	core/session.c
	core/verbose.c
	core/verbose.h
	core/utils.h

	"${PROJECT_SOURCE_DIR}/include/ipfixcol2/"
)

# Create a static library from all source code (useful for building
# the main application and unit tests separately)
add_library(
	ipfixcol2base STATIC
	${CORE_SOURCE}
)
target_link_libraries(ipfixcol2base
	${FDS_LIBRARIES}           # libfds
	${CMAKE_THREAD_LIBS_INIT}  # libpthread
)

# Build IPFIXCOL2 exacutable with all symbols from the base library
set(BASE_LIB -Wl,--whole-archive ipfixcol2base -Wl,--no-whole-archive)
add_executable(ipfixcol2 main.c)
target_link_libraries(ipfixcol2 ${BASE_LIB})
set_target_properties(ipfixcol2 PROPERTIES  # by default, hide all symbols
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
)

# Installation targets
install(
	TARGETS ipfixcol2
	DESTINATION bin
)
