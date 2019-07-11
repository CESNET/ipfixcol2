# Create a linkable module
add_library(anonymization-intermediate MODULE
    anonymization.c
    config.c
    config.h
    Crypto-PAn/panonymizer.c
    Crypto-PAn/panonymizer.h
    Crypto-PAn/rijndael.c
    Crypto-PAn/rijndael.h
)

install(
    TARGETS anonymization-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-anonymization-inter.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-anonymization-inter.7")

    add_custom_command(TARGET anonymization-intermediate PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
        )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
