add_library(filter-intermediate MODULE
    msg_builder.h
    filter.c
    config.c
    config.h
)

install(
    TARGETS filter-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-filter-inter.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-filter-inter.7")

    add_custom_command(TARGET filter-intermediate PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
        )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()