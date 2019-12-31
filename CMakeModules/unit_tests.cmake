# ------------------------------------------------------------------------------
# Download GTest

# Source: https://gist.github.com/ClintLiddick/51deffb768a7319e715071aa7bd3a3ab

find_package(Threads REQUIRED)

include(ExternalProject)
ExternalProject_Add(
  googletest
  GIT_REPOSITORY   https://github.com/google/googletest.git
  GIT_TAG          "release-1.8.1"
  UPDATE_COMMAND   ""
  INSTALL_COMMAND  ""
  LOG_DOWNLOAD     ON
  LOG_CONFIGURE    ON
  LOG_BUILD        ON
)

ExternalProject_Get_Property(googletest source_dir)
set(GTEST_INCLUDE_DIRS ${source_dir}/googletest/include)
set(GMOCK_INCLUDE_DIRS ${source_dir}/googlemock/include)

ExternalProject_Get_Property(googletest binary_dir)
set(GTEST_LIBRARY_PATH ${binary_dir}/googlemock/gtest/${CMAKE_FIND_LIBRARY_PREFIXES}gtest.a)
set(GTEST_LIBRARY gtest)
add_library(${GTEST_LIBRARY} UNKNOWN IMPORTED)
set_target_properties(${GTEST_LIBRARY} PROPERTIES
  IMPORTED_LOCATION ${GTEST_LIBRARY_PATH}
  IMPORTED_LINK_INTERFACE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(${GTEST_LIBRARY} googletest)

set(GTEST_MAIN_LIBRARY_PATH ${binary_dir}/googlemock/gtest/${CMAKE_FIND_LIBRARY_PREFIXES}gtest_main.a)
set(GTEST_MAIN_LIBRARY gtest_main)
add_library(${GTEST_MAIN_LIBRARY} UNKNOWN IMPORTED)
set_target_properties(${GTEST_MAIN_LIBRARY} PROPERTIES
  IMPORTED_LOCATION ${GTEST_MAIN_LIBRARY_PATH}
  IMPORTED_LINK_INTERFACE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(${GTEST_MAIN_LIBRARY} googletest)

set(GMOCK_LIBRARY_PATH ${binary_dir}/googlemock/${CMAKE_FIND_LIBRARY_PREFIXES}gmock.a)
set(GMOCK_LIBRARY gmock)
add_library(${GMOCK_LIBRARY} UNKNOWN IMPORTED)
set_target_properties(${GMOCK_LIBRARY} PROPERTIES
  IMPORTED_LOCATION ${GMOCK_LIBRARY_PATH}
  IMPORTED_LINK_INTERFACE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(${GMOCK_LIBRARY} googletest)

set(GMOCK_MAIN_LIBRARY_PATH ${binary_dir}/googlemock/${CMAKE_FIND_LIBRARY_PREFIXES}gmock_main.a)
set(GMOCK_MAIN_LIBRARY gmock_main)
add_library(${GMOCK_MAIN_LIBRARY} UNKNOWN IMPORTED)
set_target_properties(${GMOCK_MAIN_LIBRARY} PROPERTIES
  IMPORTED_LOCATION ${GMOCK_MAIN_LIBRARY_PATH}
  IMPORTED_LINK_INTERFACE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(${GMOCK_MAIN_LIBRARY} ${GTEST_LIBRARY})

include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

message (STATUS "Include dirs: " ${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS})

# ------------------------------------------------------------------------------
# Valgrind
if (ENABLE_TESTS_VALGRIND)
    find_program(PATH_VALGRIND NAMES valgrind)
    if (NOT PATH_VALGRIND)
        message(FATAL_ERROR "Valgrind executable not found!")
    endif()
    mark_as_advanced(PATH_VALGRIND)

    set(VALGRIND_FLAGS
        "--trace-children=yes"
        "--leak-check=full"
        "--show-leak-kinds=all"
        "--show-reachable=yes"
        "--quiet"
        "--error-exitcode=1"
        "--num-callers=32"
        )
endif()

# ------------------------------------------------------------------------------
# Register standalone unit test (no extra libraries required)
# Note: The fuction will add linkage to "libgtest and "ipfixcol2base".
#       If extra libraries are required, create your own custom target and
#       register it using unit_tests_register_target() function.
# Param: _file   File with a test case
# Param: ...     Extra files can be added as dependences
function(unit_tests_register_test _file)
    # Get a test name
    get_filename_component(CASE_NAME "${_file}" NAME_WE)
    set(CASE_NAME "test_${CASE_NAME}")

    # Add executable
    add_executable(${CASE_NAME} ${ARGV})
    target_link_libraries(${CASE_NAME} PUBLIC ${GTEST_LIBRARY} ${GMOCK_LIBRARY} ipfixcol2base)

    # Add the test
    add_test(NAME ${CASE_NAME} COMMAND "$<TARGET_FILE:${CASE_NAME}>")

    if (ENABLE_TESTS_VALGRIND)
        add_test(
            NAME ${CASE_NAME}_valgrind
            COMMAND ${PATH_VALGRIND} ${VALGRIND_FLAGS} "$<TARGET_FILE:${CASE_NAME}>"
        )
    endif()
endfunction()

# Register unit test target (when extra library is required)
# Note: This function will add linkage to "libgtest", but linkage to
#       "ipfixcol2base" must be done manually, if required.
# Param: _target   CMake target (i.e. executable name)
function(unit_tests_register_target _target)
    # Get a test name
    set(CASE_NAME "test_${_target}")
    target_link_libraries(${_target} PUBLIC ${GTEST_LIBRARY} ${GMOCK_LIBRARY})

    # Add test(s)
    add_test(NAME ${CASE_NAME} COMMAND "$<TARGET_FILE:${_target}>")

    if (ENABLE_TESTS_VALGRIND)
        add_test(
            NAME ${CASE_NAME}_valgrind
            COMMAND ${PATH_VALGRIND} ${VALGRIND_FLAGS} "$<TARGET_FILE:${_target}>"
        )
    endif()
endfunction()

# ------------------------------------------------------------------------------
# Code coverage
function(coverage_add_target)
    # Find requried programs
    find_program(PATH_GCOV    NAMES gcov)
    find_program(PATH_LCOV    NAMES lcov)
    find_program(PATH_GENHTML NAMES genhtml)
    mark_as_advanced(PATH_GCOV PATH_LCOV PATH_GENHTML) # hide variables

    if (NOT PATH_GCOV)
        message(FATAL_ERROR "'gcov' executable not found! Install it or disable code coverage.")
    endif()

    if (NOT PATH_LCOV)
        message(FATAL_ERROR "'lcov' executable not found! Install it or disable code coverage.")
    endif()

    if (NOT PATH_GENHTML)
        message(FATAL_ERROR "'genhtml' executable not found! Install it or disable code coverage.")
    endif()

    if (NOT CMAKE_COMPILER_IS_GNUCXX)
        message(WARNING "Compiler is not gcc/g++! Coverage may not work!")
    endif()

    # Destination
    set(COVERAGE_DIR        "${CMAKE_BINARY_DIR}/code_coverage/")
    set(COVERAGE_FILE_RAW   "${CMAKE_BINARY_DIR}/coverage_raw.info")
    set(COVERAGE_FILE_CLEAN "${CMAKE_BINARY_DIR}/coverage_clean.info")

    # Add coverage target
    add_custom_target(coverage
        COMMENT "Generating code coverage..."
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        # Cleanup code counters
        COMMAND "${PATH_LCOV}" --directory . --zerocounters --quiet

        # Run tests
        COMMAND "${CMAKE_CTEST_COMMAND}"

        # Capture the counters
        COMMAND "${PATH_LCOV}"
            --directory .
            --rc lcov_branch_coverage=1
            --rc 'lcov_excl_line=assert'
            --capture --quiet
            --output-file "${COVERAGE_FILE_RAW}"
        # Remove coverage of Google Test files, system headers, etc.
        COMMAND "${PATH_LCOV}"
            --remove "${COVERAGE_FILE_RAW}" '${CMAKE_BINARY_DIR}/*' '${CMAKE_SOURCE_DIR}/tests/*' '/usr/*'
            --rc lcov_branch_coverage=1
            --quiet --output-file "${COVERAGE_FILE_CLEAN}"
        # Generate HTML report
        COMMAND "${PATH_GENHTML}"
            --branch-coverage --function-coverage --quiet --title "IPFIXcol2"
            --legend --show-details --output-directory "${COVERAGE_DIR}"
            "${COVERAGE_FILE_CLEAN}"
        # Delete the counters
        COMMAND "${CMAKE_COMMAND}" -E remove
            ${COVERAGE_FILE_RAW} ${COVERAGE_FILE_CLEAN}
        )

    add_custom_command(TARGET coverage POST_BUILD
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "To see the coverage report, open ${COVERAGE_DIR}index.html"
        COMMAND ;
    )
endfunction()