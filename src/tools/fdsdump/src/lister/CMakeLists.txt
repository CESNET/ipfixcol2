# Create a lister "object" library
set(LISTER_SRC
    lister.cpp
    printer.cpp
    csvPrinter.cpp
    jsonPrinter.cpp
    jsonRawPrinter.cpp
    tablePrinter.cpp
    storageRecord.cpp
    storageSorter.cpp
    storageSorted.cpp
)

add_library(lister_obj OBJECT ${LISTER_SRC})
