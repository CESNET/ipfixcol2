cmake_minimum_required(VERSION 2.8.8)
include(${CMAKE_SOURCE_DIR}/CMakeModules/unit_tests.cmake)

# List of tests
ipx_add_test_file("src/convertors.cpp")
ipx_add_test_file("src/verbose.cpp")
# >> Add your new tests HERE <<

# Enable code coverage target (i.e. make coverage) when appropriate build
# type is chosen.
if (${BUILD_TYPE_UPPER} STREQUAL "COVERAGE")
	coverage_add_target()
endif()

