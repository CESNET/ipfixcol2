#pragma once

#include <stdexcept>

struct ArgError : public std::runtime_error {
    ArgError(const std::string &text) : std::runtime_error(text) {}
};
