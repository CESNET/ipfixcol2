include(${CMAKE_SOURCE_DIR}/CMakeModules/unit_tests.cmake)

# Include public headers
include_directories(
	"${PROJECT_SOURCE_DIR}/include/"
	"${PROJECT_BINARY_DIR}/include/"
)

# List of tests
unit_tests_register_test("core/verbose.cpp")
#add_subdirectory(templater)
# >> Add your new tests or test subdirectories HERE <<

# Enable code coverage target (i.e. make coverage) when appropriate build
# type is chosen.
if (${BUILD_TYPE_UPPER} STREQUAL "COVERAGE")
	coverage_add_target()
endif()

