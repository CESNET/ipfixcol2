# Create a linkable module
add_library(json-output MODULE
    src/json.cpp
    src/Config.cpp
    src/Config.hpp
    src/Storage.cpp
    src/Storage.hpp
    src/Printer.cpp
    src/Printer.hpp
    src/File.cpp
    src/File.hpp
    src/Kafka.cpp
    src/Kafka.hpp
    src/Server.cpp
    src/Server.hpp
    src/Sender.cpp
    src/Server.hpp
)

find_package(LibRDKafka 0.9.3 REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(
    ${ZLIB_INCLUDE_DIRS}         # zlib
    ${LIBRDKAFKA_INCLUDE_DIRS}   # librdkafka
)
target_link_libraries(json-output
    ${ZLIB_LIBRARIES}
    ${LIBRDKAFKA_LIBRARIES}
)

install(
    TARGETS json-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-json-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-json-output.7")

    add_custom_command(TARGET json-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
