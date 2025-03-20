/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Lister printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <algorithm>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <vector>

#include <common/common.hpp>

#include <lister/printer.hpp>
#include <lister/csvPrinter.hpp>
#include <lister/jsonPrinter.hpp>
#include <lister/jsonRawPrinter.hpp>
#include <lister/tablePrinter.hpp>

namespace fdsdump {
namespace lister {

struct PrinterFactory {
    const char *name;
    std::function<Printer *(const std::string &)> create_fn;
};

static const std::vector<struct PrinterFactory> g_printers {
    {"csv", [](const std::string &args) {
        return new CsvPrinter(args); }
    },
    {"json", [](const std::string &args) {
        return new JsonPrinter(args); }
    },
    {"json-raw",  [](const std::string &args) {
        return new JsonRawPrinter(args); }
    },
    {"table", [](const std::string &args){
        return new TablePrinter(args); }
    },
};

std::unique_ptr<Printer>
printer_factory(const std::string &manual)
{
    std::string type;
    std::string args;
    size_t delim_pos;

    delim_pos = manual.find(':');
    type = manual.substr(0, delim_pos);
    args = (delim_pos != std::string::npos)
        ? manual.substr(delim_pos + 1, std::string::npos)
        : "";

    // Convert type to lower case
    std::for_each(type.begin(), type.end(), [](char &c) { c = std::tolower(c); });

    for (const auto &it : g_printers) {
        if (type != it.name) {
            continue;
        }

        return std::unique_ptr<Printer>(it.create_fn(args));
    }

    throw std::invalid_argument("Unsupported output type '" + type + "'");
}

} // lister
} // fdsdump
