/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Argument parser component
 */

#include <common/argParser.hpp>

#include <algorithm>
#include <map>

#include <getopt.h>

namespace fdsdump {

bool Args::has(char short_opt) const {
    return std::any_of(m_named_args.begin(), m_named_args.end(), [=](const NamedArg &arg) { return arg.short_opt == short_opt; });
}

bool Args::has(const std::string &long_opt) const {
    return std::any_of(m_named_args.begin(), m_named_args.end(), [&](const NamedArg &arg) { return arg.long_opt == long_opt; });
}

bool Args::has(int pos) const {
    return pos < int(m_pos_args.size());
}

bool Args::count(char short_opt) const {
    return std::count_if(m_named_args.begin(), m_named_args.end(), [=](const NamedArg &arg) { return arg.short_opt == short_opt; });
}

bool Args::count(const std::string &long_opt) const {
    return std::count_if(m_named_args.begin(), m_named_args.end(), [&](const NamedArg &arg) { return arg.long_opt == long_opt; });
}

bool Args::pos_count() const {
    return m_pos_args.size();
}

std::string Args::get(char short_opt) const {
    auto it = std::find_if(m_named_args.rbegin(), m_named_args.rend(), [&](const NamedArg &arg) { return arg.short_opt == short_opt; });
    if (it == m_named_args.rend()) {
        return "";
    }
    return it->value;
}

std::string Args::get(const std::string &long_opt) const {
    auto it = std::find_if(m_named_args.rbegin(), m_named_args.rend(), [&](const NamedArg &arg) { return arg.long_opt == long_opt; });
    if (it == m_named_args.rend()) {
        return "";
    }
    return it->value;
}

std::string Args::get(int pos) const {
    if (!has(pos)) {
        return "";
    }
    return m_pos_args[pos];
}

std::vector<std::string> Args::get_all(char short_opt) const {
    std::vector<std::string> values;
    for (const auto &arg : m_named_args) {
        if (arg.short_opt == short_opt) {
            values.push_back(arg.value);
        }
    }
    return values;
}

std::vector<std::string> Args::get_all(const std::string &long_opt) const {
    std::vector<std::string> values;
    for (const auto &arg : m_named_args) {
        if (arg.long_opt == long_opt) {
            values.push_back(arg.value);
        }
    }
    return values;
}

void ArgParser::add(char short_opt, const std::string &long_opt, bool requires_value) {
    m_defs.push_back({short_opt, long_opt, requires_value});
}

void ArgParser::add(char short_opt, bool requires_value) {
    m_defs.push_back({short_opt, "", requires_value});
}

void ArgParser::add(const std::string &long_opt, bool requires_value) {
    m_defs.push_back({0, long_opt, requires_value});
}

Args ArgParser::parse(int argc, char **argv) const {
    std::string short_opts = ":";
    std::vector<option> long_opts;
    int next_nonchar_val = 256;

    std::map<int, const ArgDef *> val_to_arg_map;

    for (const auto &def : m_defs) {
        if (def.short_opt != 0) {
            short_opts.push_back(def.short_opt);
            if (def.requires_value) {
                short_opts.append(":");
            }
        }

        option getopt_option = {};

        if (!def.long_opt.empty()) {
            getopt_option.name = def.long_opt.c_str();
        }

        if (def.short_opt == 0) {
            getopt_option.val = next_nonchar_val;
            next_nonchar_val++;
        } else {
            getopt_option.val = def.short_opt;
        }

        if (def.requires_value) {
            getopt_option.has_arg = required_argument;
        } else {
            getopt_option.has_arg = no_argument;
        }

        long_opts.push_back(getopt_option);
        val_to_arg_map.emplace(getopt_option.val, &def);
    }

    long_opts.push_back({0, 0, 0, 0});

    // Parse args
    Args parsed;
    opterr = 0;
    int opt;

    while ((opt = getopt_long(argc, argv, short_opts.c_str(), long_opts.data(), nullptr)) != -1) {
        if (opt == '?') {
            throw ArgParser::UnknownArgument {argv[optind - 1]};
        }
        if (opt == ':') {
            throw ArgParser::MissingArgument {argv[optind - 1]};
        }

        const ArgDef *def = val_to_arg_map[opt];

        Args::NamedArg arg;
        arg.short_opt = def->short_opt;
        arg.long_opt = def->long_opt;
        if (optarg) {
            arg.value = optarg;
        }
        parsed.m_named_args.push_back(arg);
    }

    // Collect remaining positional arguments
    for (int i = optind; i < argc; i++) {
        parsed.m_pos_args.push_back(argv[i]);
    }

    return parsed;
}

} // fdsdump
