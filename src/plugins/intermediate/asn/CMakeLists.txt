# Create a linkable module
add_library(asn-intermediate MODULE
    asn.c
    config.c
    config.h
)

# Try to find maxmind library
find_library(MAXMIND maxminddb)
if(NOT MAXMIND)
  message(FATAL_ERROR "MaxMind database not found")
endif()

target_link_libraries(asn-intermediate maxminddb)
target_link_options(asn-intermediate PRIVATE -Wl,--no-as-needed -lprofiler -Wl,--as-needed)

install(
    TARGETS asn-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-asn-inter.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-asn-inter.7")

    add_custom_command(TARGET asn-intermediate PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
        )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
