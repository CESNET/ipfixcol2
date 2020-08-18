#include <libtrap/trap.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
    printf("%s", trap_version);
    return EXIT_SUCCESS;
}