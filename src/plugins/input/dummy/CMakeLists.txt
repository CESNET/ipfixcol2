# Create a linkable module
add_library(dummy-input MODULE
    dummy.c
    config.c
    config.h
)

install(
    TARGETS dummy-input
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)
