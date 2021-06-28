#include "table_printer.hpp"
#include <cstdio>
#include <arpa/inet.h>

TablePrinter::TablePrinter()
{
}

TablePrinter::~TablePrinter()
{
}

void
TablePrinter::print_prologue()
{
    printf("%39s:%5s -> %39s:%5s %5s %10s %10s %10s\n", "srcip", "sport", "dstip", "dport", "proto", "bytes", "packets", "flows");
}

void
TablePrinter::print_record(aggregate_record_s &record)
{
    char buf[64];

    inet_ntop(record.key.src_ip.length == 4 ? AF_INET : AF_INET6, record.key.src_ip.address, buf, 64);
    printf("%39s:%5hu", buf, record.key.src_port);

    printf(" -> ");

    inet_ntop(record.key.dst_ip.length == 4 ? AF_INET : AF_INET6, record.key.dst_ip.address, buf, 64);
    printf("%39s:%5hu", buf, record.key.dst_port);

    switch (record.key.protocol) {
    case 6:
        printf("   TCP ");
        break;
    case 17:
        printf("   UDP ");
        break;
    default:
        printf(" %5d ", record.key.protocol);
    }

    printf("%10lu %10lu %10lu\n", record.bytes, record.packets, record.flows);
}

void
TablePrinter::print_epilogue()
{
}