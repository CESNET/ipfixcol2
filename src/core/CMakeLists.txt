# Core source files
set(CORE_SOURCE
    configurator/configurator.cpp
    configurator/configurator.hpp
    configurator/controller_file.cpp
    configurator/controller_file.hpp
    configurator/cpipe.c
    configurator/cpipe.h
    configurator/extensions.cpp
    configurator/extensions.hpp
    configurator/instance.hpp
    configurator/instance_input.cpp
    configurator/instance_input.hpp
    configurator/instance_intermediate.cpp
    configurator/instance_intermediate.hpp
    configurator/instance_outmgr.cpp
    configurator/instance_outmgr.hpp
    configurator/instance_output.cpp
    configurator/instance_output.hpp
    configurator/plugin_mgr.cpp
    configurator/plugin_mgr.hpp
    configurator/model.cpp
    configurator/model.hpp
    netflow2ipfix/netflow2ipfix.h
    netflow2ipfix/netflow5.c
    netflow2ipfix/netflow9.c
    netflow2ipfix/netflow9_templates.c
    netflow2ipfix/netflow9_templates.h
    netflow2ipfix/netflow9_parsers.c
    netflow2ipfix/netflow9_parsers.h
    netflow2ipfix/netflow_structs.h
    api.c
    context.c
    context.h
    extension.c
    extension.h
    fpipe.c
    fpipe.h
    message_base.c
    message_base.h
    message_garbage.c
    message_ipfix.c
    message_ipfix.h
    message_session.c
    message_terminate.c
    message_terminate.h
    odid_range.c
    odid_range.h
    parser.c
    parser.h
    plugin_parser.c
    plugin_parser.h
    plugin_output_mgr.c
    plugin_parser.h
    ring.c
    ring.h
    session.c
    verbose.c
    verbose.h
    utils.c
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
