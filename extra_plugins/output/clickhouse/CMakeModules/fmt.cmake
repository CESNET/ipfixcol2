# fmt library
#
# The project consists of a library that can be independently
# added as dependency:
#  - fmt::fmt

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(
        fmt
        GIT_REPOSITORY "https://github.com/fmtlib/fmt"
        GIT_TAG "11.0.2"
        GIT_SHALLOW ON
)

FetchContent_MakeAvailable(fmt)
set(BUILD_SHARED_LIBS ON)
