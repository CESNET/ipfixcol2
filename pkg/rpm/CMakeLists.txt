# Find RPM builder
find_program(PATH_RPMBUILD NAMES rpmbuild)
if (NOT PATH_RPMBUILD)
    message(FATAL_ERROR "'rpmbuild' executable not found! Install it or disable "
        "RPM package builder.")
endif()
mark_as_advanced(PATH_RPMBUILD)

# Is a package maintainer specified?
if (NOT CPACK_PACKAGE_CONTACT)
    message(FATAL_ERROR "Package maintainer is not specified. Define it or "
        "disable package builder(s). Hint: \n"
        "-DCPACK_PACKAGE_CONTACT=\"Name Surname <email@example.com>\"")
endif()

# RPM build directory
set(RPM_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/rpmbuild")

# Create auxiliary build directories in the RPM build directory
file(MAKE_DIRECTORY
    "${RPM_BUILD_DIR}/BUILD"
    "${RPM_BUILD_DIR}/RPMS"
    "${RPM_BUILD_DIR}/SOURCES"
    "${RPM_BUILD_DIR}/SPECS"
    "${RPM_BUILD_DIR}/SRPMS"
)

# Create a configuration for CPack
set(FILE_SPEC_IN  "${CMAKE_CURRENT_SOURCE_DIR}/ipfixcol2.spec.in")
set(FILE_SPEC_OUT "${RPM_BUILD_DIR}/SPECS/ipfixcol2.spec")

configure_file(
    "${FILE_SPEC_IN}"
    "${FILE_SPEC_OUT}"
    @ONLY
)

set(SRC_ARCHIVE
    "${CPACK_OUTPUT_FILE_PREFIX}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.tar.gz"
)

# New target that will pack all source codes into a SRPM package and build RPM packages
add_custom_target(rpm
    COMMENT "Generating SRPM and RPM packages..."
    WORKING_DIRECTORY "${RPM_BUILD_DIR}"

    # Copy a source code archive
    COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${SRC_ARCHIVE}" "SOURCES/"

    # Call RPM builder
    COMMAND "${PATH_RPMBUILD}" "-ba" ${FILE_SPEC_OUT} "--define" "_topdir ${RPM_BUILD_DIR}/"
)

# RPM target must call TGZ builder first
add_dependencies(rpm
    tgz
)

# Post build (show a message where RPM files are located)
add_custom_command(TARGET rpm POST_BUILD
    WORKING_DIRECTORY "${RPM_BUILD_DIR}"
    COMMENT "SRPM and RPM packages are located in ${RPM_BUILD_DIR}/"
    COMMAND ;
)

