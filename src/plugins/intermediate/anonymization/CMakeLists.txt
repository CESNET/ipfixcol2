# Create a linkable module
add_library(anonymization-intermediate MODULE
	anonymization.c
	config.c
	config.h
	Crypto-PAn/panonymizer.c
	Crypto-PAn/panonymizer.h
	Crypto-PAn/rijndael.c
	Crypto-PAn/rijndael.h
)

install(
	TARGETS anonymization-intermediate
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)