# Configure a header file to pass some CMake variables
configure_file(
	"${PROJECT_SOURCE_DIR}/src/build_config.h.in"
	"${PROJECT_BINARY_DIR}/src/build_config.h"
)

# Find libfds
find_package(LibFds 0.1.0 REQUIRED)

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
	core/message_base.c
	core/message_base.h
	core/message_garbage.c
	core/message_ipfix.c
	core/message_ipfix.h
	core/message_session.c
	#core/parser.c
	#core/parser.h
	core/plugin.c
	core/plugin.h
	core/session.c
	core/verbose.c

	"${PROJECT_SOURCE_DIR}/include/ipfixcol2/"
)

# Create a static library from all source code (useful for building
# the main application and unit tests separately)
add_library(
	ipfixcol2base STATIC
	${CORE_SOURCE}
)
target_link_libraries(ipfixcol2base ${FDS_LIBRARIES})

# Build IPFIXCOL2 exacutable with all symbols from the base library
set(BASE_LIB -Wl,--whole-archive ipfixcol2base -Wl,--no-whole-archive)
add_executable(ipfixcol2 main.c)
target_link_libraries(ipfixcol2 ${BASE_LIB})

# Installation targets
install(
	TARGETS ipfixcol2
	DESTINATION bin
)
