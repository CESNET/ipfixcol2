add_executable(ipfixsend2
	ipfixsend.c
	ipfixsend.h
	reader.c
	reader.h
	sender.c
	sender.h
	siso.c
	siso.h
)

# Installation targets
install(
	TARGETS ipfixsend2
	DESTINATION bin
)