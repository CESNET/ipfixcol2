# Create an aggregator "object" library
set(AGGREGATOR_SRC
    mode.cpp
    aggregator.cpp
    arenaAllocator.cpp
    hashTable.cpp
    view.cpp
    sort.cpp
    print.cpp
    printer.cpp
    jsonPrinter.cpp
    tablePrinter.cpp
)

add_library(aggregator_obj OBJECT ${AGGREGATOR_SRC})
