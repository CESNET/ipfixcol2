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

ExternalProject_Get_Property(gtest source_dir binary_dir)

# Create a libtest target to be used as a dependency by test programs
add_library(libgtest INTERFACE)
add_dependencies(libgtest gtest)
target_link_libraries(libgtest INTERFACE
	Threads::Threads
	"${binary_dir}/libgtest.a"
	"${binary_dir}/libgtest_main.a"
	)
target_include_directories(libgtest INTERFACE
	"${source_dir}/include")

