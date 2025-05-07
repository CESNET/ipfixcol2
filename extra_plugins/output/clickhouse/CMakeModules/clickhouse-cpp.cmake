# clickhouse-cpp library (C++ client for ClickHouse)
#
# The project consists of a library that can be independently
# added as dependency:
#  - clickhouse::client

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(
        clickhouse
        GIT_REPOSITORY "https://github.com/ClickHouse/clickhouse-cpp.git"
        GIT_TAG "v2.5.1"
        GIT_SHALLOW ON
)

FetchContent_MakeAvailable(clickhouse)
add_library(clickhouse::client ALIAS clickhouse-cpp-lib)
set(BUILD_SHARED_LIBS ON)
