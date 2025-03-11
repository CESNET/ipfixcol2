add_executable(ipfixsend2
    ipfixsend.c
    reader.c
    reader.h
    sender.c
    sender.h
    siso.c
    siso.h
)

target_link_libraries(ipfixsend2 crypto ssl)

# Installation targets
install(
    TARGETS ipfixsend2
    DESTINATION bin
)
