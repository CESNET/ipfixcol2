# Add GIT revision
#
# If the current Git revision is available (requires a git repository and
# installed Git package), sets "GIT_REVISION" variable to the appropriate value.
# Otherwise the variable will have the "unknown" value.


set(GIT_REVISION "unknown")
find_package(Git QUIET)
if (GIT_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} "log" "-1" "--format=%h"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		RESULT_VARIABLE TMP_GIT_RET_CODE
		OUTPUT_VARIABLE TMP_GIT_REVISION
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	if (TMP_GIT_RET_CODE EQUAL 0)
		set(GIT_REVISION ${TMP_GIT_REVISION})
	endif()
endif(GIT_FOUND)
