# Create a linkable module
add_library(json-output MODULE
	json.cpp
	Config.cpp
	Config.hpp
	Storage.cpp
	Storage.hpp
	protocols.hpp
	protocols.cpp
	Printer.cpp
	Printer.hpp
)

install(
	TARGETS json-output
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)
