# Header files for source code building
include_directories(
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_BINARY_DIR}/include/"  # for api.h
    "${PROJECT_BINARY_DIR}/src/"      # for build_config.h
    "${FDS_INCLUDE_DIRS}"             # libfds header files
)

add_subdirectory(input_failure)
add_subdirectory(intermediate_failure)
add_subdirectory(output_failure)