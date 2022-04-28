# Create an aggregator "object" library
set(AGGREGATOR_SRC
    aggregator.cpp
    arenaAllocator.cpp
    hashTable.cpp
)

add_library(aggregator_obj OBJECT ${AGGREGATOR_SRC})
