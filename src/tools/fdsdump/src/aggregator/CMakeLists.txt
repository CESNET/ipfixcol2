# Create an aggregator "object" library
set(AGGREGATOR_SRC
    mode.cpp
    aggregator.cpp
    arenaAllocator.cpp
    fastHashTable.cpp
    viewOld.cpp
    sort.cpp
    print.cpp
    printer.cpp
    jsonPrinter.cpp
    tablePrinter.cpp
    stdHashTable.cpp
    stdAllocator.cpp
)

add_library(aggregator_obj OBJECT ${AGGREGATOR_SRC})
