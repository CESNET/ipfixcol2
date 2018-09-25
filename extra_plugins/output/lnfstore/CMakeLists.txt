cmake_minimum_required(VERSION 2.8.8)
project(lnfstore)

# Description of the project
set(LNFSTORE_DESCRIPTION
    "Output plugin for IPFIXcol2 that stores flow records into nfdump compatible files."
)

set(LNFSTORE_VERSION_MAJOR 2)
set(LNFSTORE_VERSION_MINOR 0)
set(LNFSTORE_VERSION_PATCH 0)
set(LNFSTORE_VERSION
    ${LNFSTORE_VERSION_MAJOR}.${LNFSTORE_VERSION_MINOR}.${LNFSTORE_VERSION_PATCH})

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(GNUInstallDirs)
# Include custom FindXXX modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

# Find IPFIXcol and libnf
find_package(IPFIXcol2 2.0.0 REQUIRED)
find_package(LibFds REQUIRED)
find_package(LibNf REQUIRED)
find_package(LibBFI REQUIRED)

# Check capabilities of a compiler
CHECK_C_COMPILER_FLAG(-std=gnu11 COMPILER_SUPPORT_GNU11)
if (NOT COMPILER_SUPPORT_GNU11)
    message(FATAL_ERROR "Compiler does NOT support C11 with GNU extension")
endif()

CHECK_CXX_COMPILER_FLAG(-std=gnu++11 COMPILER_SUPPORT_GNUXX11)
if (NOT COMPILER_SUPPORT_GNUXX11)
    message(FATAL_ERROR "Compiler does NOT support C++11 with GNU extension")
endif()

# Set default build type if not specified by user
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release
        CACHE STRING "Choose type of build (Release/Debug/Coverage)." FORCE)
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
    "${FDS_INCLUDE_DIRS}"        # libfds header files
    "${NF_INCLUDE_DIRS}"         # libnf header files
    "${BFI_INCLUDE_DIRS}"        # libbfindex header files
)

# Create a linkable module
add_library(lnfstore-output MODULE
    src/configuration.c
    src/configuration.h
    src/files_manager.c
    src/files_manager.h
    src/idx_manager.c
    src/idx_manager.h
    src/lnfstore.c
    src/lnfstore.h
    src/storage_basic.c
    src/storage_basic.h
    src/storage_common.c
    src/storage_common.h
    src/translator.c
    src/translator.h
    src/utils.c
    src/utils.h
)

target_link_libraries(lnfstore-output
    ${NF_LIBRARIES}               # libnf
    ${BFI_LIBRARIES}              # libbfindex
    ${FDS_LIBRARIES}              # libfds
)

install(
    TARGETS lnfstore-output
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    find_package(Rst2Man)
    if (NOT RST2MAN_FOUND)
        message(FATAL_ERROR "rst2man is not available. Install python-docutils or disable manual page generation (-DENABLE_DOC_MANPAGE=False)")
    endif()

    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-lnfstore-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-lnfstore-output.7")

    add_custom_command(TARGET lnfstore-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man7"
    )
endif()
