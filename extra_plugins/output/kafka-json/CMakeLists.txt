cmake_minimum_required(VERSION 2.8.8)
project(ipfixcol2-kafka-json-output)

# Description of the project
set(KAFKA_DESCRIPTION
    "Output plugin for IPFIXcol2 that sends flow records in JSON format into Apache Kafka."
)

set(KAFKA_VERSION_MAJOR 2)
set(KAFKA_VERSION_MINOR 0)
set(KAFKA_VERSION_PATCH 0)
set(KAFKA_VERSION
    ${KAFKA_VERSION_MAJOR}.${KAFKA_VERSION_MINOR}.${KAFKA_VERSION_PATCH})

include(CMakeModules/install_dirs.cmake)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
# Include custom FindXXX modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

# Find IPFIXcol and libnf
find_package(IPFIXcol2 2.1.0 REQUIRED)
find_package(LibRDKafka 0.9.4 REQUIRED)   # Note: rd_kafka_producev() was added in this version

# Set default build type if not specified by user
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release
        CACHE STRING "Choose type of build (Release/Debug)." FORCE)
endif()

option(ENABLE_DOC_MANPAGE    "Enable manual page building"              ON)

# Hard coded definitions
set(CMAKE_C_FLAGS            "${CMAKE_C_FLAGS} -fvisibility=hidden -std=gnu11")
set(CMAKE_C_FLAGS_RELEASE    "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG      "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS          "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=gnu++11")
set(CMAKE_CXX_FLAGS_RELEASE  "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG    "-g -O0 -Wall -Wextra -pedantic")

# Header files for source code building
include_directories(
    "${IPFIXCOL2_INCLUDE_DIRS}"  # IPFIXcol2 header files
    "${LIBRDKAFKA_INCLUDE_DIRS}" # librdkafka header files
)

# Create a linkable module
add_library(kafka-json-output MODULE
    src/json.cpp
    src/Config.cpp
    src/Config.hpp
    src/Storage.cpp
    src/Storage.hpp
)

target_link_libraries(kafka-json-output
    ${LIBRDKAFKA_LIBRARIES}            # librdkafka
)

install(
    TARGETS kafka-json-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    find_package(Rst2Man)
    if (NOT RST2MAN_FOUND)
        message(FATAL_ERROR "rst2man is not available. Install python-docutils or disable manual page generation (-DENABLE_DOC_MANPAGE=False)")
    endif()

    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-kafka-json-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-kafka-json-output.7")

    add_custom_command(TARGET kafka-json-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()