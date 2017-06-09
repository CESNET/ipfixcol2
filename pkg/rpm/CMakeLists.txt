# Find RPM builder
find_program(PATH_RPMBUILD NAMES rpmbuild)
if (NOT PATH_RPMBUILD)
	message(FATAL_ERROR "'rpmbuild' executable not found! Install it or disable "
		"RPM package builder.")
endif()
mark_as_advanced(PATH_RPMBUILD)

# Create auxiliary build directories
file(MAKE_DIRECTORY
	"${CMAKE_CURRENT_BINARY_DIR}/rpmbuild/BUILD"
	"${CMAKE_CURRENT_BINARY_DIR}/rpmbuild/RPMS"
	"${CMAKE_CURRENT_BINARY_DIR}/rpmbuild/SOURCES"
	"${CMAKE_CURRENT_BINARY_DIR}/rpmbuild/SPECS"
	"${CMAKE_CURRENT_BINARY_DIR}/rpmbuild/SRPMS"
)

# Create a configuration for CPack
set(FILE_SPEC_IN  "${CMAKE_CURRENT_SOURCE_DIR}/ipfixcol.spec.in")
set(FILE_SPEC_OUT "${CMAKE_CURRENT_BINARY_DIR}/rpmbuild/SPECS/ipfixcol.spec")

configure_file(
	"${FILE_SPEC_IN}"
	"${FILE_SPEC_OUT}"
	@ONLY
)

set(SRC_ARCHIVE
	"${CPACK_OUTPUT_FILE_PREFIX}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.tar.gz"

)
# New target that will pack all source codes into a RPM source package
add_custom_target(rpm
	COMMENT "Generating RPM packages..."
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rpmbuild"

	# Copy a source code archive
	COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${SRC_ARCHIVE}" "SOURCES/"

	# Call RPM builder
	COMMAND "${PATH_RPMBUILD}" "-ba" ${FILE_SPEC_OUT} "--define" "_topdir ${CMAKE_CURRENT_BINARY_DIR}/rpmbuild/"

)
# TODO: add target rpm
