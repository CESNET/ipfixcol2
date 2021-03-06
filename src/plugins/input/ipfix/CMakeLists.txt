# Create a linkable module
add_library(ipfix-input MODULE
    ipfix.c
    config.c
    config.h
)

install(
    TARGETS ipfix-input
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-ipfix-input.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-ipfix-input.7")

    add_custom_command(TARGET ipfix-input PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
