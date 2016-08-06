cmake_minimum_required(VERSION 2.8.8)
include(GTest.cmake)

# List of tests
set(test_cases
	core/convertors.cpp
	core/verbose.cpp
	# Add your tests HERE !!!
	)

# List of source and header files
ipx_files_get(IPFIXCOL_PUBLIC_HEADERS PUBLIC_HEADERS)
ipx_files_get(IPFIXCOL_SOURCES SOURCES)

# Find Valgrind
if (ENABLE_VALGRIND_TESTS)
	find_program(valgrind_FOUND NAMES valgrind)
	if (NOT valgrind_FOUND)
		 Message("-- valgrind executable not found! Disabling memory leaks tests")
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

# Add tests
foreach(case_file ${test_cases})
	get_filename_component(case_name ${case_file} NAME_WE)
	set (case_name test_${case_name})

	add_executable(
		${case_name}
		${case_file}
		${IPFIXCOL_SOURCES}
		${IPFIXCOL_PUBLIC_HEADERS}
		)
	target_link_libraries(${case_name} PUBLIC libgtest)
	add_test(NAME ${case_name} COMMAND $<TARGET_FILE:${case_name}>)

	if (valgrind_FOUND)
		add_test(
			NAME ${case_name}_valgrind
			COMMAND valgrind ${VALGRIND_FLAGS} $<TARGET_FILE:${case_name}>
			)
	endif()
endforeach()

