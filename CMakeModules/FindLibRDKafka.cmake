#  LIBRDKAFKA_FOUND - System has librdkafka
#  LIBRDKAFKA_INCLUDE_DIRS - The librdkafka include directories
#  LIBRDKAFKA_LIBRARIES - The libraries needed to use librdkafka
#  LIBRDKAFKA_DEFINITIONS - Compiler switches required for using librdkafka

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_RDKAFKA QUIET rdkafka)
    set(LIBRDKAFKA_DEFINITIONS ${PC_RDKAFKA_CFLAGS_OTHER})
endif()

find_path(
    KAFKA_INCLUDE_DIR librdkafka/rdkafka.h
    HINTS ${PC_RDKAFKA_INCLUDEDIR} ${PC_RDKAFKA_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library(
    KAFKA_LIBRARY NAMES rdkafka librdkafka
    HINTS ${PC_RDKAFKA_LIBDIR} ${PC_RDKAFKA_LIBRARY_DIRS}
    PATH_SUFFIXES lib lib64
)

if (PC_RDKAFKA_VERSION)
    # Version extracted from pkg-config
    set(KAFKA_VERSION_STRING ${PC_RDKAFKA_VERSION})
elseif(KAFKA_INCLUDE_DIR AND KAFKA_LIBRARY)
    # Try to get the version of the installed library
    try_run(
        KAFKA_RES_RUN KAFKA_RES_COMP
        ${CMAKE_CURRENT_BINARY_DIR}/try_run/kafka_version_test/
        ${PROJECT_SOURCE_DIR}/CMakeModules/try_run/kafka_version.c
        CMAKE_FLAGS
            -DLINK_LIBRARIES=${KAFKA_LIBRARY}
            -DINCLUDE_DIRECTORIES=${KAFKA_INCLUDE_DIR}
        RUN_OUTPUT_VARIABLE KAFKA_VERSION_VAR
    )

    if (KAFKA_RES_COMP AND KAFKA_RES_RUN EQUAL 0)
        # Successfully compiled and executed with return code 0
        set(KAFKA_VERSION_STRING ${KAFKA_VERSION_VAR})
    endif()
endif()

# handle the QUIETLY and REQUIRED arguments and set LIBRDKAFKA_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibRDKafka
    REQUIRED_VARS KAFKA_LIBRARY KAFKA_INCLUDE_DIR
    VERSION_VAR KAFKA_VERSION_STRING
)

set(LIBRDKAFKA_LIBRARIES ${KAFKA_LIBRARY})
set(LIBRDKAFKA_INCLUDE_DIRS ${KAFKA_INCLUDE_DIR})
mark_as_advanced(KAFKA_INCLUDE_DIR KAFKA_LIBRARY)
