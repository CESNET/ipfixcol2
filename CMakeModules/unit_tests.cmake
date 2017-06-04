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

# Create a libtest target to be used as a dependency by test programs
add_library(libgtest STATIC IMPORTED GLOBAL)
add_dependencies(libgtest gtest)

ExternalProject_Get_Property(gtest source_dir binary_dir)
set_target_properties(libgtest PROPERTIES
	IMPORTED_LOCATION "${binary_dir}/libgtest.a"
	IMPORTED_LINK_INTERFACE_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}"
	)
include_directories("${source_dir}/include")

# ------------------------------------------------------------------------------
# Valgrind
if (ENABLE_TESTS_VALGRIND)
	find_program(PATH_VALGRIND NAMES valgrind)
	if (NOT PATH_VALGRIND)
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
function(ipx_add_test_file _file)
# Param: _file   File with a test case
	# Get a test name
	get_filename_component(CASE_NAME "${_file}" NAME_WE)
	set(CASE_NAME test_${CASE_NAME})

	# Get source files
	ipx_files_get(HEADERS PUBLIC_HEADERS)
	ipx_files_get(SRCS SOURCES)

	# Add executable
	add_executable(${CASE_NAME} ${_file} ${SRCS} ${HEADERS})
	target_link_libraries(${CASE_NAME} PUBLIC libgtest)

	# Add the test
	add_test(NAME ${CASE_NAME} COMMAND "$<TARGET_FILE:${CASE_NAME}>")

	if (ENABLE_TESTS_VALGRIND)
		add_test(
			NAME ${CASE_NAME}_valgrind
			COMMAND ${PATH_VALGRIND} ${VALGRIND_FLAGS} "$<TARGET_FILE:${CASE_NAME}>"
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
		COMMAND "${CMAKE_CTEST_COMMAND}" --quiet

		# Capture the counters
		COMMAND "${PATH_LCOV}"
			--directory .
			--rc lcov_branch_coverage=1
			--capture --quiet
			--output-file "${COVERAGE_FILE_RAW}"
		# Remove coverage of Google Test files, system headers, etc.
		COMMAND "${PATH_LCOV}"
			--remove "${COVERAGE_FILE_RAW}" 'gtest/*' 'tests/*' '/usr/*'
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


