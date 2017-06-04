cmake_minimum_required(VERSION 2.8.8)
project(ipfixcol)

# Description of the project
set(
	IPFIXCOL_DESCRIPTION
	"IPFIXcol is an implementation of an IPFIX (RFC 7011) collector."
	)

# Check the platform
if (NOT UNIX)
	message(FATAL_ERROR "Only Unix like systems are supported.")
endif()

# Versions and other informations
set(IPFIXCOL_VERSION_MAJOR 2)
set(IPFIXCOL_VERSION_MINOR 0)
set(IPFIXCOL_VERSION_PATCH 0)
set(IPFIXCOL_VERSION
	${IPFIXCOL_VERSION_MAJOR}.${IPFIXCOL_VERSION_MINOR}.${IPFIXCOL_VERSION_PATCH})

# Include modules
include(CMakeModules/git_revision.cmake) # Sets variable GIT_REVISION
include(CMakeModules/ipx_files.cmake)
include(CheckCCompilerFlag)
include(GNUInstallDirs)
#include(CheckCXXCompilerFlag)

CHECK_C_COMPILER_FLAG(-std=gnu11 COMPILER_SUPPORT_GNU11)
if (NOT COMPILER_SUPPORT_GNU11)
	message(FATAL_ERROR "Compiler does NOT support C11 with GNU extension")
endif()

#CHECK_CXX_COMPILER_FLAG(-std=gnu++11 COMPILER_SUPPORT_GNUXX11)
#if (NOT COMPILER_SUPPORT_GNUXX11)
#	message(FATAL_ERROR "Compiler does NOT support C++11 with GNU extension")
#endif()

# ------------------------------------------------------------------------------
# Build options
set(TESTS_DEFAULT    OFF)

# Set default build type if not specified by user
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release
		CACHE STRING "Choose type of build (Release/Debug/Coverage)." FORCE)
endif()

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_UPPER)
# Enable unit tests, by default, for coverage builds
if (${BUILD_TYPE_UPPER} STREQUAL "COVERAGE")
	set(TESTS_DEFAULT ON)
endif()

option(ENABLE_DOC            "Enable documentation building"    ON)
option(ENABLE_TESTS          "Build Unit tests (make test)"     ${TESTS_DEFAULT})
option(ENABLE_TESTS_VALGRIND "Build Unit tests with Valgrind Memcheck"       OFF)

## -----------------------------------------------------------------------------
# Hard coded definitions
set(CMAKE_C_FLAGS            "${CMAKE_C_FLAGS} -fvisibility=hidden -std=gnu11")
set(CMAKE_C_FLAGS_RELEASE    "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG      "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_COVERAGE   "${CMAKE_C_FLAGS_DEBUG} --coverage -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS          "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=gnu++11")
set(CMAKE_CXX_FLAGS_RELEASE  "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG    "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} --coverage -fprofile-arcs -ftest-coverage")

# ------------------------------------------------------------------------------
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

# List of public header
ipx_files_add(PUBLIC_HEADERS
	include/ipfixcol2.h
	include/ipfixcol2/converters.h
	include/ipfixcol2/data_record.h
	include/ipfixcol2/ipfix_element.h
	include/ipfixcol2/ipfix_structures.h
	include/ipfixcol2/message.h
	include/ipfixcol2/message_garbage.h
	include/ipfixcol2/message_ipfix.h
	include/ipfixcol2/message_session.h
	include/ipfixcol2/processor.h
	include/ipfixcol2/source.h
	include/ipfixcol2/template.h
	include/ipfixcol2/verbose.h
	"${PROJECT_BINARY_DIR}/include/ipfixcol2/api.h"
	)

# List of source files
ipx_files_add(SOURCES
	src/verbose.c
	"${PROJECT_BINARY_DIR}/src/build_config.h"
	)

# IPFIXcol main
ipx_files_add(MAIN
	src/ipfixcol.c
	src/converters.c
	src/template.c
	src/message.c
	src/message_garbage.c
	src/message_internal.h
	src/message_ipfix.c
	src/message_session.c
	src/source.c
	)

# Get files
ipx_files_get(IPFIXCOL_PUBLIC_HEADERS PUBLIC_HEADERS)
ipx_files_get(IPFIXCOL_SOURCES SOURCES)
ipx_files_get(IPFIXCOL_MAIN MAIN)

# Add the executable
add_executable(
	ipfixcol2
	${IPFIXCOL_MAIN}
	${IPFIXCOL_SOURCES}
	${IPFIXCOL_PUBLIC_HEADERS}
	)

# Build documentation
if (ENABLE_DOC)
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
endif(ENABLE_DOC)

if (ENABLE_TESTS)
	enable_testing()
	add_subdirectory(tests/unit_tests)
endif()

# ------------------------------------------------------------------------------
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
	DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}/ipfixcol2/"
	)

# ------------------------------------------------------------------------------
# Status messages
MESSAGE(STATUS
	"\n\n"
	"IPFIXcol2 version:   ${IPFIXCOL_VERSION}\n"
	"Install prefix...:   ${CMAKE_INSTALL_PREFIX}\n"
	"Build type.......:   ${BUILD_TYPE_UPPER}\n"
	"C Compiler.......:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\n"
	"C Flags..........:   ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BUILD_TYPE_UPPER}}\n"
	"C++ Compiler.....:   ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
	"C++ Flags........:   ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER}}\n"
	"Doxygen..........:   ${DOXYGEN_FOUND}\n"
	)
