# Common parameters (based on CPack variables)
set(CPACK_PACKAGE_NAME     ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_VERSION  ${IPFIXCOL_VERSION})
set(CPACK_PACKAGE_RELEASE  1)
set(CPACK_PACKAGE_VENDOR   "CESNET z.s.p.o.")
set(CPACK_PACKAGE_CONTACT  "")

# Short package description
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY  "IPFIX Collector Toolset")
# Long package description
set(CPACK_PACKAGE_DESCRIPTION
	"IPFIXcol is a flexible IPFIX (RFC 7011) flow data collector designed to "
	"be extensible by plugins."
)

# Create a top level directory in an archive with the same name as the archive
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 1)

# Add following directories and files into a source code archive
# A list of "pairs" first element being the name of the directory to be
# copied and the second being the relative path in th toplevel install
# directory.
# Note: This can be also utilized to change the top directory of an archive.
#       Just change "/" to "/<name>/" and disable 
set(CPACK_INSTALLED_DIRECTORIES
	"${CMAKE_SOURCE_DIR}" "/" # By default, add all source codes
	# add here path to extra + relative dir inside archive e.g. "<path>;/extra/"
)

# Ignore following files and directories into a source code archive
set(CPACK_IGNORE_FILES
	".*~$"                        # Ignore temporary files
	"${CMAKE_BINARY_DIR}/"
	"${CMAKE_SOURCE_DIR}/.gitignore"
	"${CMAKE_SOURCE_DIR}/.idea/"  # Ignore JetBrains IDE files
)

# Output directory for archives
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/")

# If a contact (i.e. name and email) is not defined try to get it from the Git
# configuration
if (NOT CPACK_PACKAGE_CONTACT OR CPACK_PACKAGE_CONTACT STREQUAL "")
	if (GIT_USER_NAME AND GIT_USER_EMAIL)
		message(STATUS "Maintainer contact for packages is not specified "
			"- using a name and email from the git configuration")
		string(CONCAT CPACK_PACKAGE_CONTACT
			${GIT_USER_NAME} " <" ${GIT_USER_EMAIL} ">")
	elseif()
		message(WARNING "Maintainer contact for packages is not specified.")
	endif()
endif()

message(STATUS "Maintainer contact for packages: " ${CPACK_PACKAGE_CONTACT})

add_subdirectory(tgz)

# Package builders
#if (PACKAGE_BUILDER_RPM)
#	add_subdirectory(RPM)
#endif()

#if (PACKAGE_BUILDER_DEB)
#	add_subdirectory(DEB)
#endif

