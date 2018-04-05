# Create a linkable module
add_library(dummy-input MODULE
	dummy.c)

install(
	TARGETS dummy-input
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)