# Create a linkable module
add_library(tcp-input MODULE
	tcp.c
	config.c
	config.h
)

install(
	TARGETS tcp-input
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)