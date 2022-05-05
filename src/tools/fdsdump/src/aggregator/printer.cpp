
#include <algorithm>
#include <functional>
#include <vector>
#include <stdexcept>

#include "printer.hpp"
#include "jsonPrinter.hpp"
#include "tablePrinter.hpp"

namespace fdsdump {
namespace aggregator {

struct PrinterFactory {
    const char *name;
    std::function<Printer *(ViewDefinition view_def)> create_fn;
};

static const std::vector<struct PrinterFactory> g_printers {
    {"json", [](ViewDefinition view_def) {
        return new JSONPrinter(view_def); }
    },
    {"table", [](ViewDefinition view_def){
        return new TABLEPrinter(view_def); }
    },
};

std::unique_ptr<Printer>
printer_factory(ViewDefinition view_def, const std::string &manual)
{
    std::string type = manual;

    // Convert type to lower case
    std::for_each(type.begin(), type.end(), [](char &c) { c = std::tolower(c); });

    for (const auto &it : g_printers) {
        if (type != it.name) {
            continue;
        }

        return std::unique_ptr<Printer>(it.create_fn(view_def));
    }

    throw std::invalid_argument("Unsupported output type '" + type + "'");
}

} // aggregator
} // fdsdump
