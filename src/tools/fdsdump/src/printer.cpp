
#include <algorithm>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <vector>

#include "common.hpp"
#include "printer.hpp"
#include "jsonPrinter.hpp"
#include "jsonRawPrinter.hpp"
#include "tablePrinter.hpp"

struct PrinterFactory {
    const char *name;
    std::function<Printer *(const shared_iemgr &iemgr, const std::string &)> create_fn;
};

static const std::vector<struct PrinterFactory> g_printers {
    {"json", [](const shared_iemgr &iemgr, const std::string &args) {
        return new JsonPrinter(iemgr, args); }
    },
    {"json-raw",  [](const shared_iemgr &iemgr, const std::string &args) {
        return new JsonRawPrinter(iemgr, args); }
    },
    {"table", [](const shared_iemgr &iemgr, const std::string &args){
        return new TablePrinter(iemgr, args); }
    },
};

std::unique_ptr<Printer>
printer_factory(const shared_iemgr &iemgr, const std::string &manual)
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

        return std::unique_ptr<Printer>(it.create_fn(iemgr, args));
    }

    throw std::invalid_argument("Unsupported output type '" + type + "'");
}
