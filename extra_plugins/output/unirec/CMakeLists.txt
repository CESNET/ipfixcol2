cmake_minimum_required(VERSION 2.8.8)
project(unirec)

# Description of the project
set(UNIREC_DESCRIPTION
	"Output plugin for IPFIXcol2 that sends flow records in UniRec format into NEMEA modules."
)

set(UNIREC_VERSION_MAJOR 1)
set(UNIREC_VERSION_MINOR 0)
set(UNIREC_VERSION_PATCH 0)
set(UNIREC_VERSION
	${UNIREC_VERSION_MAJOR}.${UNIREC_VERSION_MINOR}.${UNIREC_VERSION_PATCH})

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(GNUInstallDirs)
# Include custom FindXXX modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

# Find IPFIXcol and libnf
find_package(IPFIXcol 2.0.0 REQUIRED)
find_package(LibTrap REQUIRED)
find_package(Unirec REQUIRED)

# Set default build type if not specified by user
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release
		CACHE STRING "Choose type of build (Release/Debug/Coverage)." FORCE)
endif()

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
	"${LIBTRAP_INCLUDE_DIRS}"        # libtrap header files
	"${UNIREC_INCLUDE_DIRS}"        # unirec header files
)

# Create a linkable module
add_library(unirec-output MODULE
	configuration.c
	configuration.h
	unirecplugin.h
	unirecplugin.c
	translator.c
	translator.h
	fields.c
	fields.h
)

target_link_libraries(unirec-output
    ${LIBTRAP_LIBRARIES}               # libtrap
    ${UNIREC_LIBRARIES}               # unirec
)

install(
	TARGETS unirec-output
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)
