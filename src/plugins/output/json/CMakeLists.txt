# Create a linkable module
add_library(json-output MODULE
	src/json.cpp
	src/Config.cpp
	src/Config.hpp
	src/Storage.cpp
	src/Storage.hpp
	src/protocols.hpp
	src/protocols.cpp
	src/Printer.cpp
	src/Printer.hpp
	src/File.cpp
	src/File.hpp
	src/Server.cpp
	src/Server.hpp
	src/Sender.cpp
	src/Server.hpp
)

install(
	TARGETS json-output
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)
