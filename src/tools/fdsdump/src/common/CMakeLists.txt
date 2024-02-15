# Create a common "object" library
set(COMMON_SRC
    argParser.cpp
    common.cpp
    field.cpp
    fieldView.cpp
    flowProvider.cpp
    ipaddr.cpp
    ieMgr.cpp
    logger.cpp
)

add_library(common_obj OBJECT ${COMMON_SRC})
