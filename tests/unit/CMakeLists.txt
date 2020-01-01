include(${CMAKE_SOURCE_DIR}/CMakeModules/unit_tests.cmake)

# Include public headers
include_directories(
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_BINARY_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/src/"     # make internal function available for testing
)

# List of tests
unit_tests_register_test(session.cpp)
unit_tests_register_test("core/verbose.cpp")

add_subdirectory(core/parser)
add_subdirectory(core/netflow)
# >> Add your new tests or test subdirectories HERE <<

# Enable code coverage target (i.e. make coverage) when appropriate build
# type is chosen.
if (ENABLE_TESTS AND ENABLE_TESTS_COVERAGE)
    coverage_add_target()
endif()
