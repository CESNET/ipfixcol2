#include <librdkafka/rdkafka.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
    const char *ver_str = rd_kafka_version_str();
    if (!ver_str) {
        return 1;
    }

    printf("%s", ver_str);
    return 0;
}