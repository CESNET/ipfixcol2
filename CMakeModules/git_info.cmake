# Get auxiliary information from GIT configuration
#
# If a git repository is available and Git package is installed,
# - sets variable "GIT_HASH" to appropriate value or "unknown" value.
# - sets variable "GIT_USER_NAME",  if available. Otherwise the var is unset.
# - sets variable "GIT_USER_EMAIL", if available. Otherwise the var is unset.

set(GIT_HASH "unknown")
unset(GIT_USER_NAME)
unset(GIT_USER_EMAIL)

find_package(Git QUIET)
if (GIT_FOUND)
    # Get an abbreviated commit hash of the latest commit
    execute_process(
        COMMAND ${GIT_EXECUTABLE} "log" "-1" "--format=%h"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE TMP_GIT_RET_CODE
        OUTPUT_VARIABLE TMP_GIT_HASH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (TMP_GIT_RET_CODE EQUAL 0)
        set(GIT_HASH "${TMP_GIT_HASH}")
    endif()

    # Get a user name
    execute_process(
        COMMAND ${GIT_EXECUTABLE} "config" "--get" "user.name"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE TMP_GIT_RET_CODE
        OUTPUT_VARIABLE TMP_GIT_NAME
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (TMP_GIT_RET_CODE EQUAL 0)
        set(GIT_USER_NAME "${TMP_GIT_NAME}")
    endif()

    # Get a user email
    execute_process(
        COMMAND ${GIT_EXECUTABLE} "config" "--get" "user.email"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE TMP_GIT_RET_CODE
        OUTPUT_VARIABLE TMP_GIT_EMAIL
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (TMP_GIT_RET_CODE EQUAL 0)
        set(GIT_USER_EMAIL "${TMP_GIT_EMAIL}")
    endif()
endif(GIT_FOUND)
