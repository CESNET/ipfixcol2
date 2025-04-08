# Create a linkable module
add_library(dummy-input MODULE
    dummy.c
    config.c
    config.h
)

target_link_libraries(dummy-input
    ipfixcol2base               # Link against the core library
)

install(
    TARGETS dummy-input
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)
