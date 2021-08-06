set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")

add_executable(fdsdump
    fdsdump.cpp
    informationelements.hpp
    common.cpp
    common.hpp
    aggregator.cpp
    aggregator.hpp
    printer.hpp
    tableprinter.cpp
    tableprinter.hpp
    reader.cpp
    reader.hpp
    ipfixfilter.cpp
    ipfixfilter.hpp
    config.cpp
    config.hpp
    aggregatefilter.cpp
    aggregatefilter.hpp
    view.cpp
    view.hpp
    aggregatetable.cpp
    aggregatetable.hpp
    sorter.cpp
    sorter.hpp
    filelist.cpp
    filelist.hpp
    threadworker.cpp
    threadworker.hpp
    binaryheap.hpp
)


target_link_libraries(fdsdump
    ${FDS_LIBRARIES}           # libfds
)

find_package(Threads REQUIRED)
target_link_libraries(fdsdump PRIVATE Threads::Threads)

install(
    TARGETS fdsdump
    DESTINATION bin
)