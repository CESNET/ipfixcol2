set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")

add_executable(fdsdump
    src/ipfix/fdsreader.cpp
    src/ipfix/fdsreader.hpp
    src/ipfix/ipfixfilter.cpp
    src/ipfix/ipfixfilter.hpp
    src/ipfix/informationelements.hpp
    src/ipfix/util.hpp

    src/thirdparty/xxhash.h

    src/utils/binaryheap.hpp
    src/utils/filelist.cpp
    src/utils/filelist.hpp
    src/utils/hashtable.cpp
    src/utils/hashtable.hpp
    src/utils/threadrunner.hpp
    src/utils/util.hpp

    src/view/aggregator.cpp
    src/view/aggregator.hpp
    src/view/aggregatefilter.cpp
    src/view/aggregatefilter.hpp
    src/view/sort.cpp
    src/view/sort.hpp
    src/view/view.cpp
    src/view/view.hpp
    src/view/tableprinter.cpp
    src/view/tableprinter.hpp

    src/fdsdump.cpp
    src/cmdargs.hpp
    src/cmdargs.cpp
    src/printer.hpp
    src/common.hpp
)


target_link_libraries(fdsdump
    ${FDS_LIBRARIES}           # libfds
)

find_package(Threads REQUIRED)
target_include_directories(fdsdump PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fdsdump PRIVATE Threads::Threads)

install(
    TARGETS fdsdump
    DESTINATION bin
)
