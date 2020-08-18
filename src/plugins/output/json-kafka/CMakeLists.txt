# Create a linkable module
add_library(json-kafka-output MODULE
    src/json.cpp
    src/Config.cpp
    src/Config.hpp
    src/Storage.cpp
    src/Storage.hpp
    src/Kafka.cpp
    src/Kafka.hpp
)

find_package(LibRDKafka 0.9.3 REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(
    ${LIBRDKAFKA_INCLUDE_DIRS}   # librdkafka
)
target_link_libraries(json-kafka-output
    ${LIBRDKAFKA_LIBRARIES}
)

install(
    TARGETS json-kafka-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-json-kafka-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-json-kafka-output.7")

    add_custom_command(TARGET json-kafka-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
