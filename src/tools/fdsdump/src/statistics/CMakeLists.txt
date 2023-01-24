# Create a common "statistics" library
set(STATISTICS_SRC
    mode.cpp
    printer.cpp
    jsonPrinter.cpp
    tablePrinter.cpp
)

add_library(statistics_obj OBJECT ${STATISTICS_SRC})
