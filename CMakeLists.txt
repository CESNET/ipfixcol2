cmake_minimum_required(VERSION 3.0)

# The project name will be the name of the binary executable created. The test
# code will also be named ${PROJECT_NAME}_test.
project(ipfixcol)

# Description of the project
set(
	IPFIXCOL_DESCRIPTION
	"IPFIXcol is an implementation of an IPFIX (RFC 7011) collector."
	)

# Versions
set(IPFIXCOL_VERSION_MAJOR 2)
set(IPFIXCOL_VERSION_MINOR 0)
set(IPFIXCOL_VERSION_PATCH 0)
set(IPFIXCOL_VERSION
	${IPFIXCOL_VERSION_MAJOR}.${IPFIXCOL_VERSION_MINOR}.${IPFIXCOL_VERSION_PATCH})

# Check the platform
if (NOT UNIX)
	message(FATAL_ERROR "Only Unix like systems are supported.")
endif()

# Set default build type if not specified by user
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} -Wall -fvisibility=hidden")
set(CMAKE_C_FLAGS_RELEASE   "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG     "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Wall -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wall -Wextra -pedantic")

# Configure a header file to pass some of the CMake setting to the source code
configure_file(
	"${PROJECT_SOURCE_DIR}/include/ipfixcol2/api.h.in"
	"${PROJECT_BINARY_DIR}/include/ipfixcol2/api.h"
	)

# Local header files are placed in the include directory.
include_directories(
	"${PROJECT_SOURCE_DIR}/include/"
	"${PROJECT_BINARY_DIR}/include/"  # for api.h
	)


# List of source code files
set(SOURCES
	src/verbose.c
	src/ipfixcol.c
	)

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES})

