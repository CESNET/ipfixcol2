# Find CPack
find_program(PATH_CPACK NAMES cpack)
if (NOT PATH_CPACK
    AND (PACKAGE_BUILDER_RPM OR PACKAGE_BUILDER_DEB))
    message(FATAL_ERROR "'cpack' executable not found! Install it or disable "
        "package builders.")
endif()
mark_as_advanced(PATH_CPACK)

if (PATH_CPACK)
    # Create a configuration for CPack
    set(FILE_CFG_IN  "${CMAKE_CURRENT_SOURCE_DIR}/CPackTGZConfig.cmake.in")
    set(FILE_CFG_OUT "${CMAKE_CURRENT_BINARY_DIR}/CPackTGZConfig.cmake")

    configure_file(
        "${FILE_CFG_IN}"
        "${FILE_CFG_OUT}"
        @ONLY
    )

    # New target that will pack all source codes into a TAR.GZ archive
    add_custom_target(tgz
        COMMENT "Generating tar.gz package..."
        COMMAND "${PATH_CPACK}" "--config" "${FILE_CFG_OUT}"
    )
endif()

