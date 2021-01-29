# Create a linkable module
add_library(forwarder-output MODULE
    src/main.cpp
    src/config.h
    src/Forwarder.h
    src/ConnectionManager.h
    src/ConnectionManager.cpp
    src/ConnectionParams.h
    src/Connection.h
    src/Connection.cpp
    src/ConnectionBuffer.h
    src/SyncPipe.h
    src/IPFIXMessage.h
    src/MessageBuilder.h
)

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
