# Create a linkable module
add_library(tcp-input MODULE
    src/IpAddress.cpp
    src/Config.cpp
    src/ByteVector.cpp
    src/DecodeBuffer.cpp
    src/Connection.cpp
    src/Epoll.cpp
    src/ClientManager.cpp
    src/IpfixDecoder.cpp
    src/Lz4Decoder.cpp
    src/DecoderFactory.cpp
    src/Acceptor.cpp
    src/Plugin.cpp
    src/IpxPlugin.cpp
)

find_package(LibLz4 REQUIRED)
include_directories(${LIBLZ4_INCLUDE_DIRS})
target_link_libraries(tcp-input ${LIBLZ4_LIBRARIES})

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
    find_package(LibEpollShim REQUIRED)
    include_directories(
        ${LIBEPOLLSHIM_INCLUDE_DIRS}
    )
    target_link_libraries(tcp-input
        ${LIBEPOLLSHIM_LIBRARIES}
    )
endif()

install(
    TARGETS tcp-input
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-tcp-input.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-tcp-input.7")

    add_custom_command(TARGET tcp-input PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
        )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
