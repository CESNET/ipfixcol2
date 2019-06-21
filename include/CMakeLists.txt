# Configure a header file to pass API version to the source code
configure_file(
    "${PROJECT_SOURCE_DIR}/include/ipfixcol2/api.h.in"
    "${PROJECT_BINARY_DIR}/include/ipfixcol2/api.h"
    )

# List of header files
set(TOP_HEADERS
    ipfixcol2.h
)

set(SUB_HEADERS
    ipfixcol2/message.h
    ipfixcol2/message_garbage.h
    ipfixcol2/message_ipfix.h
    ipfixcol2/message_session.h
    ipfixcol2/plugins.h
    ipfixcol2/session.h
    ipfixcol2/utils.h
    ipfixcol2/verbose.h
    "${PROJECT_BINARY_DIR}/include/ipfixcol2/api.h"
)

# Installation targets
install(
    FILES ${TOP_HEADERS}
    DESTINATION include
)

install(
    FILES ${SUB_HEADERS}
    DESTINATION include/ipfixcol2/
)
