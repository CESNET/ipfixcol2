cmake_minimum_required(VERSION 3.12)
project(ipfixcol2-clichouse-output)

# Description of the project
set(CH_DESCRIPTION
    "Output plugin for IPFIXcol2 that store flow records to ClickHouse database."
)

set(CH_VERSION_MAJOR 1)
set(CH_VERSION_MINOR 0)
set(CH_VERSION_PATCH 0)
set(CH_VERSION
    ${CH_VERSION_MAJOR}.${CH_VERSION_MINOR}.${CH_VERSION_PATCH})

include(CMakeModules/install_dirs.cmake)
include(CMakeModules/clickhouse-cpp.cmake)
include(CMakeModules/fmt.cmake)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
# Include custom FindXXX modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

# Find IPFIXcol
find_package(IPFIXcol2 2.3.0 REQUIRED)

# Set default build type if not specified by user
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug
        CACHE STRING "Choose type of build (Release/Debug)." FORCE)
endif()

option(ENABLE_DOC_MANPAGE    "Enable manual page building"              ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Hard coded definitions
set(CMAKE_C_FLAGS            "${CMAKE_C_FLAGS} -fvisibility=hidden")
set(CMAKE_C_FLAGS_RELEASE    "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG      "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS          "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_RELEASE  "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG    "-g -O0 -Wall -Wextra -pedantic")

# Header files for source code building
include_directories(
    "${IPFIXCOL2_INCLUDE_DIRS}"  # IPFIXcol2 header files
)

# Create a linkable module
add_library(clickhouse-output MODULE
    src/common.cpp
    src/config.cpp
    src/datatype.cpp
    src/main.cpp
    src/plugin.cpp
    src/inserter.cpp
    src/recparser.cpp
    src/stats.cpp
)

target_link_libraries(clickhouse-output PRIVATE clickhouse::client fmt::fmt)

install(
    TARGETS clickhouse-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    find_package(Rst2Man)
    if (NOT RST2MAN_FOUND)
        message(FATAL_ERROR "rst2man is not available. Install python-docutils or disable manual page generation (-DENABLE_DOC_MANPAGE=False)")
    endif()

    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-clickhouse-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-clickhouse-output.7")

    add_custom_command(TARGET clickhouse-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
