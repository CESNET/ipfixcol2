# Create an aggregator "object" library
set(AGGREGATOR_SRC
    aggregatedField.cpp
    aggregator.cpp
    aliasField.cpp
    arenaAllocator.cpp
    extraFields.cpp
    fastHashTable.cpp
    field.cpp
    flowContext.cpp
    inOutField.cpp
    ipfixField.cpp
    jsonPrinter.cpp
    mode.cpp
    print.cpp
    printer.cpp
    stdAllocator.cpp
    stdHashTable.cpp
    tablePrinter.cpp
    threadedAggregator.cpp
    thresholdAlgorithm.cpp
    value.cpp
    view.cpp
    viewFactory.cpp
)

add_library(aggregator_obj OBJECT ${AGGREGATOR_SRC})
