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

# Include modules
include(GNUInstallDirs)

# Set default build type if not specified by user
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} -Wall -fvisibility=hidden -std=gnu11")
set(CMAKE_C_FLAGS_RELEASE   "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG     "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Wall -fvisibility=hidden" -std=gnu++11)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wextra -pedantic")

# TODO: add compile options here
option(documentation_enable "Enable documentation building" ON)


# ----------------------------------------------------------------------

# Configure a header file to pass API version to the source code
configure_file(
	"${PROJECT_SOURCE_DIR}/src/api.h.in"
	"${PROJECT_BINARY_DIR}/include/ipfixcol2/api.h"
	)

# Configure a header file to pass some CMake variables
configure_file(
	"${PROJECT_SOURCE_DIR}/src/build_config.h.in"
	"${PROJECT_BINARY_DIR}/src/build_config.h"
	)

# Local header files are placed in the include directory.
include_directories(
	"${PROJECT_SOURCE_DIR}/include/"
	"${PROJECT_BINARY_DIR}/include/"  # for api.h
	"${PROJECT_BINARY_DIR}/src/"      # for build_config.h
	)

# List of header files
set(PUBLIC_HEADERS
	include/ipfixcol2.h
	include/ipfixcol2/convertors.h
	include/ipfixcol2/ipfix_element.h
	include/ipfixcol2/ipfix_structures.h
	include/ipfixcol2/verbose.h
	"${PROJECT_BINARY_DIR}/include/ipfixcol2/api.h"
	)

#set(PRIVATE_HEADERS
#	)

# List of source files
set(SOURCES
	src/verbose.c
	src/ipfixcol.c
	)

# Add the executable
add_executable(ipfixcol2 ${SOURCES} ${PUBLIC_HEADERS})

# Build documentation
if (documentation_enable)
	find_package(Doxygen)
	if (NOT DOXYGEN_FOUND)
		message(FATAL_ERROR "Doxygen is not available.")
	endif()

	configure_file(
		"${PROJECT_SOURCE_DIR}/doc/doxygen/Doxyfile.in"
		"${PROJECT_BINARY_DIR}/doc/doxygen/Doxyfile"
		@ONLY
	)

	set(DOXYGEN_DIRECTORY
		"${PROJECT_BINARY_DIR}/doc/doxygen/"
	)

	add_custom_target(
		doc ALL
		COMMAND ${DOXYGEN_EXECUTABLE} "${DOXYGEN_DIRECTORY}/Doxyfile"
		WORKING_DIRECTORY "${DOXYGEN_DIRECTORY}"
		COMMENT "Generating API documentation with Doxygen to ${DOXYGEN_DIRECTORY}"
		VERBATIM
    )
endif(documentation_enable)

# Install IPFIXcol and header files
install(
	TARGETS ipfixcol2
	DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
	)
install(
	DIRECTORY include/
	DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
	)
install(
	FILES "${PROJECT_BINARY_DIR}/include/ipfixcol2/api.h"
	DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/ipfixcol2/
	)

