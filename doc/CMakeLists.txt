# Build code documentation
if (ENABLE_DOC_DOXYGEN)
    find_package(Doxygen)
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is not available.")
    endif()

    # Hide doxygen warnings in "Release" builds
    set(DOXYGEN_SHOW_WARNINGS "YES")
    if (BUILD_TYPE_UPPER STREQUAL "RELEASE")
        set(DOXYGEN_SHOW_WARNINGS "NO")
    endif()

    configure_file(
        "${PROJECT_SOURCE_DIR}/doc/doxygen/Doxyfile.in"
        "${PROJECT_BINARY_DIR}/doc/doxygen/Doxyfile"
        @ONLY
    )

    set(DOXYGEN_DIRECTORY
        "${PROJECT_BINARY_DIR}/doc/doxygen/"
    )

    add_custom_target(
        doc ALL
        COMMAND ${DOXYGEN_EXECUTABLE} "${DOXYGEN_DIRECTORY}/Doxyfile"
        WORKING_DIRECTORY "${DOXYGEN_DIRECTORY}"
        COMMENT "Generating API documentation with Doxygen to ${DOXYGEN_DIRECTORY}"
        VERBATIM
    )
endif(ENABLE_DOC_DOXYGEN)
