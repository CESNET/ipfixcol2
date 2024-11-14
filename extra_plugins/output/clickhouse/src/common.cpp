/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Common functionality and general utilities
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

#include <cctype>
#include <iostream>

std::string to_lower(std::string str)
{
    for (auto &c : str) {
        c = std::tolower(c);
    }
    return str;
}

void debug_print_block(const clickhouse::Block& block) {
    std::cout << "================================================================================\n";
    for (size_t i = 0; i < block.GetColumnCount(); i++) {
        std::cout << block.GetColumnName(i) << ":"
                  << block[i]->GetType().GetName()
                  << (i < block.GetColumnCount() - 1 ? '\t' : '\n');
    }
    std::cout << "--------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < block.GetRowCount(); i++) {
        for (size_t j = 0; j < block.GetColumnCount(); j++) {
            switch (block[j]->GetType().GetCode()) {
            case clickhouse::Type::Int8:
                std::cout << +block[j]->As<clickhouse::ColumnInt8>()->At(i);
                break;
            case clickhouse::Type::Int16:
                std::cout << block[j]->As<clickhouse::ColumnInt16>()->At(i);
                break;
            case clickhouse::Type::Int32:
                std::cout << block[j]->As<clickhouse::ColumnInt32>()->At(i);
                break;
            case clickhouse::Type::Int64:
                std::cout << block[j]->As<clickhouse::ColumnInt64>()->At(i);
                break;
            case clickhouse::Type::Int128:
                std::cout << block[j]->As<clickhouse::ColumnInt128>()->At(i);
                break;
            case clickhouse::Type::UInt8:
                std::cout << +block[j]->As<clickhouse::ColumnUInt8>()->At(i);
                break;
            case clickhouse::Type::UInt16:
                std::cout << block[j]->As<clickhouse::ColumnUInt16>()->At(i);
                break;
            case clickhouse::Type::UInt32:
                std::cout << block[j]->As<clickhouse::ColumnUInt32>()->At(i);
                break;
            case clickhouse::Type::UInt64:
                std::cout << block[j]->As<clickhouse::ColumnUInt64>()->At(i);
                break;
            case clickhouse::Type::String:
                std::cout << block[j]->As<clickhouse::ColumnString>()->At(i);
                break;
            default:
                std::cout << "-";
                break;
            }
            std::cout << (j < block.GetColumnCount() - 1 ? '\t' : '\n');
        }
    }
    std::cout << "================================================================================\n\n";
};
