# Create a linkable module
add_library(geoip-intermediate MODULE
    geoip.c
    config.c
    config.h
)

# Try to find maxmind library
find_library(MAXMIND maxminddb)
if(NOT MAXMIND)
  message(FATAL_ERROR "MaxMind database not found")
endif()

target_link_libraries(geoip-intermediate maxminddb)

install(
    TARGETS geoip-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-geoip-inter.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-geoip-inter.7")

    add_custom_command(TARGET geoip-intermediate PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
        )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
