/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Statistics printer
 */

#include <algorithm>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <vector>

#include <statistics/printer.hpp>
#include <statistics/jsonPrinter.hpp>
#include <statistics/tablePrinter.hpp>

namespace fdsdump {
namespace statistics {

struct PrinterFactory {
    const char *name;
    std::function<Printer *(const std::string &)> create_fn;
};

static const std::vector<struct PrinterFactory> g_printers {
    {"json", [](const std::string &args) {
        return new JsonPrinter(args); }
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

} // statistics
} // fdsdump
