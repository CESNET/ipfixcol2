# Create a linkable module
add_library(dummy-output MODULE
	dummy.c
	config.c
	config.h
)

install(
	TARGETS dummy-output
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)