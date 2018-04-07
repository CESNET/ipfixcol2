# Core source files
set(CORE_SOURCE
	config_file.cpp
	config_file.hpp
	configurator.cpp
	configurator.hpp
	context.c
	context.h
	fpipe.c
	fpipe.h
	message_base.c
	message_base.h
	message_garbage.c
	message_ipfix.c
	message_ipfix.h
	message_session.c
	parser.c
	parser.h
	parser_plugin.c
	parser_plugin.h
	ring.c
	ring.h
	session.c
	verbose.c
	verbose.h
	utils.h

	"${PROJECT_BINARY_DIR}/src/build_config.h"
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
	${CMAKE_DL_LIBS}           # libdl (dlopen,...)
)

# Build IPFIXCOL2 exacutable with all symbols from the base library
set(BASE_LIB -Wl,--whole-archive ipfixcol2base -Wl,--no-whole-archive)
add_executable(ipfixcol2 main.cpp)
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