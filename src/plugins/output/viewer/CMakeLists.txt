# Create a linkable module

add_library(viewer-output MODULE
		viewer.c
		config.c
		config.h
		Reader.h
		Reader.c
)

install(
	TARGETS viewer-output
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-viewer-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-viewer-output.7")

    add_custom_command(TARGET viewer-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man7"
    )
endif()