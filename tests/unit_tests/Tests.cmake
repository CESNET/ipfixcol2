# ------------------------------------------------------------------------------
# Download GTest

# GTest required threads support
find_package(Threads REQUIRED)

include(ExternalProject)

# Set default ExternalProject root directory
set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/third_party)

# Add GTest
ExternalProject_Add(
	gtest
	GIT_REPOSITORY  "https://github.com/google/googletest.git"
	GIT_TAG         "release-1.7.0"
	# Disable install step
	INSTALL_COMMAND ""
	LOG_DOWNLOAD    ON
	LOG_CONFIGURE   ON
	LOG_BUILD       ON
)

ExternalProject_Get_Property(gtest source_dir binary_dir)

# Create a libtest target to be used as a dependency by test programs
add_library(libgtest INTERFACE)
add_dependencies(libgtest gtest)
target_link_libraries(libgtest INTERFACE
	Threads::Threads
	"${binary_dir}/libgtest.a"
	"${binary_dir}/libgtest_main.a"
	)
target_include_directories(libgtest INTERFACE
	"${source_dir}/include")

# ------------------------------------------------------------------------------
# Valgrind
find_program(VALGRIND_PATH NAMES valgrind)
if (ENABLE_VALGRIND_TESTS)
	if (NOT VALGRIND_PATH)
		message(FATAL_ERROR "Valgrind executable not found!")
	endif()

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
# Register test cases
# Param: _file   File with a test case
function(ipx_add_test_file _file)
	# Get a test name
	get_filename_component(CASE_NAME "${_file}" NAME_WE)
	set(CASE_NAME test_${CASE_NAME})

	# Get source files
	ipx_files_get(HEADERS PUBLIC_HEADERS)
	ipx_files_get(SRCS SOURCES)

	# Add executable
	add_executable(${CASE_NAME} ${_file} ${SRCS} ${HEADERS})
	target_link_libraries(${CASE_NAME} PUBLIC libgtest)

	if (ENABLE_TESTS_COVERAGE)
		set_target_properties(${CASE_NAME}
			PROPERTIES COMPILE_FLAGS "-g -O0 --coverage -fprofile-arcs -ftest-coverage")
		set_target_properties(${CASE_NAME}
			PROPERTIES LINK_FLAGS "-g -O0 -fprofile-arcs")
	endif()

	# Add the test
	add_test(NAME ${CASE_NAME} COMMAND "$<TARGET_FILE:${CASE_NAME}>")

	if (ENABLE_TESTS_VALGRIND)
		add_test(
			NAME ${CASE_NAME}_valgrind
			COMMAND ${VALGRIND_PATH} ${VALGRIND_FLAGS} "$<TARGET_FILE:${CASE_NAME}>"
			)
	endif()

	# Remember all test cases for code coverage dependencies
	ipx_files_add(UNIT_TESTS $<TARGET_FILE:${CASE_NAME}>)
endfunction()


# ------------------------------------------------------------------------------
# Code coverage
function(ipx_register_coverage)
	# Find requried programs
	find_program(GCOV_PATH    NAMES gcov)
	find_program(LCOV_PATH    NAMES lcov)
	find_program(GENHTML_PATH NAMES genhtml)

	if (NOT GCOV_PATH)
		message(FATAL_ERROR "gcov executable not found!")
	endif()

	if (NOT LCOV_PATH)
		message(FATAL_ERROR "lcov executable not found!")
	endif()

	if (NOT GENHTML_PATH)
		message(FATAL_ERROR "genhtml executable not found!")
	endif()

	if (NOT CMAKE_COMPILER_IS_GNUCXX)
		message(WARNING "Compiler is not gcc/g++! Coverage may not work!")
	endif()

	# Destination
	set(COVERAGE_DIR        "${CMAKE_BINARY_DIR}/code_coverage/")
	set(COVERAGE_FILE_RAW   "${CMAKE_BINARY_DIR}/coverage_raw.info")
	set(COVERAGE_FILE_CLEAN "${CMAKE_BINARY_DIR}/coverage_clean.info")
	# Get the list of all binary files of test cases
	ipx_files_get(UNIT_TESTS_TARGETS UNIT_TESTS)

	# Add coverage target
	add_custom_target(coverage
		COMMENT "Generating code coverage..."
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
		# Cleanup code counters
		COMMAND "${LCOV_PATH}" --directory . --zerocounters --quiet

		# Run tests
		DEPENDS ${UNIT_TESTS_TARGETS}
		COMMAND ctest --quiet

		# Capture the counters
		COMMAND "${LCOV_PATH}"
			--directory .
			--rc lcov_branch_coverage=1
			--capture --quiet
			--output-file "${COVERAGE_FILE_RAW}"
		# Remove coverage of Google Test files, system headers, etc.
		COMMAND "${LCOV_PATH}"
			--remove "${COVERAGE_FILE_RAW}" 'gtest/*' 'tests/*' '/usr/*'
			--rc lcov_branch_coverage=1
			--quiet --output-file "${COVERAGE_FILE_CLEAN}"
		# Generate HTML report
		COMMAND "${GENHTML_PATH}"
			--branch-coverage --function-coverage --quiet --title "IPFIXcol2"
			--legend --show-details --output-directory "${COVERAGE_DIR}"
			"${COVERAGE_FILE_CLEAN}"
		# Delete the counters
		COMMAND "${CMAKE_COMMAND}" -E remove
			${COVERAGE_FILE_RAW} ${COVERAGE_FILE_CLEAN}
		)
endfunction()

function(ipx_code_coverage)
	if (ENABLE_TESTS_COVERAGE)
		ipx_register_coverage()
	endif()
endfunction()

