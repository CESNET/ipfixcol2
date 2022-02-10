add_executable(fdsdump
    common.cpp
    main.cpp
    printer.cpp
    jsonPrinter.cpp
    jsonRawPrinter.cpp
    tablePrinter.cpp
    flowProvider.cpp
    filelist.cpp
    field.cpp
    fieldView.cpp
    options.cpp
    ipaddr.cpp
    storageRecord.cpp
    storageSorter.cpp
    storageSorted.cpp)

target_include_directories(fdsdump
    PUBLIC
        ${FDS_INCLUDE_DIRS})

target_link_libraries(fdsdump
    PUBLIC
        ${FDS_LIBRARIES})

# Installation targets
install(
    TARGETS fdsdump
    DESTINATION bin)

