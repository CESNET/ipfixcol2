add_executable(fdsdump
    fdsdump.cpp
    common.hpp
    aggregator.cpp
    aggregator.hpp
    printer.cpp
    printer.hpp
    table_printer.cpp
    table_printer.hpp
    reader.cpp
    reader.hpp
)

target_link_libraries(fdsdump
    ${FDS_LIBRARIES}           # libfds
)

install(
    TARGETS fdsdump
    DESTINATION bin
)