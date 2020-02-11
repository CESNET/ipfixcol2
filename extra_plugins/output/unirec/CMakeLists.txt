cmake_minimum_required(VERSION 2.8.8)
project(ipfixcol2-unirec-output)

# Description of the project
set(UNIREC_DESCRIPTION
    "Output plugin for IPFIXcol2 that sends flow records in UniRec format into NEMEA modules."
)

set(UNIREC_VERSION_MAJOR 2)
set(UNIREC_VERSION_MINOR 2)
set(UNIREC_VERSION_PATCH 0)
set(UNIREC_VERSION
    ${UNIREC_VERSION_MAJOR}.${UNIREC_VERSION_MINOR}.${UNIREC_VERSION_PATCH})

include(CMakeModules/install_dirs.cmake)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
# Include custom FindXXX modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

# Find IPFIXcol and libnf
find_package(IPFIXcol2 2.1.0 REQUIRED)  # support for basicList is required
find_package(LibTrap 1.13.1 REQUIRED)
find_package(LibUnirec 2.8.0 REQUIRED)

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

# Prepare SPEC file
configure_file(
    "${PROJECT_SOURCE_DIR}/ipfixcol2-unirec-output.spec.in"
    "${PROJECT_BINARY_DIR}/ipfixcol2-unirec-output.spec"
)

# Header files for source code building
include_directories(
    "${IPFIXCOL2_INCLUDE_DIRS}"  # IPFIXcol2 header files
    "${LIBTRAP_INCLUDE_DIRS}"    # libtrap header files
    "${LIBUNIREC_INCLUDE_DIRS}"  # unirec header files
)

# Create a linkable module
add_library(unirec-output MODULE
    src/configuration.c
    src/configuration.h
    src/unirecplugin.c
    src/translator.c
    src/translator.h
    src/fields.c
    src/fields.h
    src/map.c
    src/map.h
)

target_link_libraries(unirec-output
    ${LIBTRAP_LIBRARIES}               # libtrap
    ${LIBUNIREC_LIBRARIES}             # unirec
    m                                  # standard math library
)

install(
    TARGETS unirec-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)
install(
    FILES config/unirec-elements.txt
    DESTINATION "${INSTALL_DIR_SYSCONF}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    find_package(Rst2Man)
    if (NOT RST2MAN_FOUND)
        message(FATAL_ERROR "rst2man is not available. Install python-docutils or disable manual page generation (-DENABLE_DOC_MANPAGE=False)")
    endif()

    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-unirec-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-unirec-output.7")

    add_custom_command(TARGET unirec-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
