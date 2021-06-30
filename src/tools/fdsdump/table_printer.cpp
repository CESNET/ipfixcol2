#include "table_printer.hpp"
#include <cstdio>
#include <arpa/inet.h>

TablePrinter::TablePrinter(AggregateConfig aggregate_config)
    : m_aggregate_config(aggregate_config)
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
TablePrinter::print_record(AggregateRecord &record)
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

    Value *value = reinterpret_cast<Value *>(record.values);
    for (const auto &aggregate_field : m_aggregate_config.value_fields) {
        switch (aggregate_field.data_type) {
        case DataType::Unsigned64:
            printf("%10lu", value->u64);
            advance_value_ptr(value, sizeof(value->u64));
            break;
        default: assert(0);
        }
    }
    printf("\n");
}

void
TablePrinter::print_epilogue()
{
}