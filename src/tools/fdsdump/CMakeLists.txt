set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")

add_executable(fdsdump
    fdsdump.cpp
    common.cpp
    common.hpp
    aggregator.cpp
    aggregator.hpp
    printer.cpp
    printer.hpp
    table_printer.cpp
    table_printer.hpp
    reader.cpp
    reader.hpp
    ipfix_filter.cpp
    ipfix_filter.hpp
    config.cpp
    config.hpp
    aggregate_filter.cpp
    aggregate_filter.hpp
    view.cpp
    view.hpp
)


target_link_libraries(fdsdump
    ${FDS_LIBRARIES}           # libfds
)


install(
    TARGETS fdsdump
    DESTINATION bin
)