# Create a linkable module
add_library(udp-input MODULE
	udp.c
	config.c
	config.h
)

install(
	TARGETS udp-input
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)