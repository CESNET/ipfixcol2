# Create a common "object" library
set(COMMON_SRC
    common.cpp
    field.cpp
    fieldView.cpp
    filelist.cpp
    flowProvider.cpp
    ipaddr.cpp
)

add_library(common_obj OBJECT ${COMMON_SRC})
