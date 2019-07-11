# Find appropriate packages
find_program(PATH_DEBUILD NAMES debuild)
if (NOT PATH_DEBUILD)
    message(FATAL_ERROR "'debuild' executable not found! Install it or disable "
        "DEB package builder.")
endif()
mark_as_advanced(PATH_DEBUILD)

# Is a package maintainer specified?
if (NOT CPACK_PACKAGE_CONTACT)
    message(FATAL_ERROR "Package maintainer is not specified. Define it or "
        "disable package builder(s). Hint: \n"
        "-DCPACK_PACKAGE_CONTACT=\"Name Surname <email@example.com>\"")
endif()

# DEB build directory
set(DEB_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/debbuild")

# Working directory for package building processor
# Note: the directory MUST have the same name as the unpack directory
#       from SRC_ARCHIVE_NEW (i.e. the one with dash)
set(DEB_UNPACKED_DIR
    "${DEB_BUILD_DIR}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}"
)

# Configuration directory of the source package
set(DEB_CFG_DIR
    "${DEB_UNPACKED_DIR}/debian"
)

# Source code archive (made by TGZ target)
set(SRC_ARCHIVE_OLD
    "${CPACK_OUTPUT_FILE_PREFIX}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.tar.gz"
)

# Debian package builder requires slightly different name of the package:
# i.e. "_" instead of "-" and ".orig.tar.gz" instead of ".tar.gz"
set(SRC_ARCHIVE_NEW
    "${DEB_BUILD_DIR}/${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}.orig.tar.gz"
)

# Create directory hierarchy
file(MAKE_DIRECTORY
    "${DEB_BUILD_DIR}/"
    "${DEB_CFG_DIR}/"
    "${DEB_CFG_DIR}/source"
)

# Copy and create configuration files
file(COPY
    "templates/changelog"
    "templates/rules"
    "templates/compat"
    "templates/ipfixcol2.install"
    "templates/ipfixcol2-dev.install"
    DESTINATION "${DEB_CFG_DIR}"
)

file(COPY
    "templates/source/format"
    DESTINATION "${DEB_CFG_DIR}/source"
)

configure_file(
    "templates/control.in"
    "${DEB_CFG_DIR}/control"
    @ONLY
)

configure_file(
    "templates/copyright.in"
    "${DEB_CFG_DIR}/copyright"
    @ONLY
)

# Target that will prepare build
add_custom_target(deb_hierarchy
    COMMENT "Preparing directory structure for the DEB builder..."
    WORKING_DIRECTORY ${DEB_BUILD_DIR}

    # Copy the source code archive and unpack it
    COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${SRC_ARCHIVE_OLD}" "${SRC_ARCHIVE_NEW}"
    COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xfz" "${SRC_ARCHIVE_NEW}"
)

# DEB hierarchy requires already prepared TGZ archive
add_dependencies(deb_hierarchy
    tgz
)

# New target that will pack all source codes into a source package and build DEB packages
add_custom_target(deb
    COMMENT "Generating source and DEB packages..."
    WORKING_DIRECTORY ${DEB_UNPACKED_DIR}

    # Build the package
    COMMAND ${PATH_DEBUILD} "-us" "-uc"
)

# The directory structure must be prepared before package building
add_dependencies(deb
    deb_hierarchy
)

# Post build (show a message where DEB files are located)
add_custom_command(TARGET deb POST_BUILD
    WORKING_DIRECTORY "${DEB_BUILD_DIR}"
    COMMENT "Source and DEB packages are located in ${DEB_BUILD_DIR}/"
    COMMAND ;
)

