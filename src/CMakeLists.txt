# Configure a header file to pass some CMake variables
configure_file(
	"${PROJECT_SOURCE_DIR}/src/build_config.h.in"
	"${PROJECT_BINARY_DIR}/src/build_config.h"
)

# Local header files are placed in the include directory.
include_directories(
	"${PROJECT_SOURCE_DIR}/include/"
	"${PROJECT_BINARY_DIR}/include/"  # for api.h
	"${PROJECT_BINARY_DIR}/src/"      # for build_config.h
)

# Core source files
set(CORE_SOURCE
	"${PROJECT_BINARY_DIR}/src/build_config.h"
	core/verbose.c
	core/converters.c
	core/message.c
	core/message_garbage.c
	core/message_internal.h
	core/message_ipfix.c
	core/message_session.c
	core/source.c
	core/template.c
)

# Create a static library from all source code (useful for building
# the main application and unit tests separately)
add_library(
	ipfixcol2base STATIC
	${CORE_SOURCE}
)

add_executable(ipfixcol2 main.c)
target_link_libraries(ipfixcol2 PUBLIC ipfixcol2base)

# Installation targets
install(
	TARGETS ipfixcol2
	DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
)
