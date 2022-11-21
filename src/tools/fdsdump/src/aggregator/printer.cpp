
#include <algorithm>
#include <functional>
#include <vector>
#include <stdexcept>

#include <aggregator/printer.hpp>
#include <aggregator/jsonPrinter.hpp>
#include <aggregator/tablePrinter.hpp>

namespace fdsdump {
namespace aggregator {

struct PrinterFactory {
    const char *name;
    std::function<Printer *(std::shared_ptr<View> view)> create_fn;
};

static const std::vector<struct PrinterFactory> g_printers {
    {"json", [](std::shared_ptr<View> view) {
        return new JSONPrinter(view); }
    },
    {"table", [](std::shared_ptr<View> view){
        return new TABLEPrinter(view); }
    },
};

std::unique_ptr<Printer>
printer_factory(std::shared_ptr<View> view, const std::string &manual)
{
    std::string type = manual;

    // Convert type to lower case
    std::for_each(type.begin(), type.end(), [](char &c) { c = std::tolower(c); });

    for (const auto &it : g_printers) {
        if (type != it.name) {
            continue;
        }

        return std::unique_ptr<Printer>(it.create_fn(view));
    }

    throw std::invalid_argument("Unsupported output type '" + type + "'");
}

} // aggregator
} // fdsdump
