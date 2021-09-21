# Create a linkable module
add_library(forwarder-output MODULE
    src/main.cpp
    src/Config.h
    src/Config.cpp
    src/Forwarder.h
    src/Forwarder.cpp
    src/Connection.h
    src/Connection.cpp
    src/Host.cpp
    src/Host.h
    src/common.h
    src/common.cpp
    src/Message.h
    src/Message.cpp
    src/Sender.h
    src/Sender.cpp
    src/connector/Connector.h
    src/connector/Connector.cpp
    src/connector/FutureSocket.h
    src/connector/FutureSocket.cpp
    src/connector/Pipe.h
    src/connector/Pipe.cpp
)

target_include_directories(forwarder-output PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(
    TARGETS forwarder-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-forwarder-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-forwarder-output.7")

    add_custom_command(TARGET forwarder-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
