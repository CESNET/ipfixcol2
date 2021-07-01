#include "common.hpp"

std::vector<std::string>
string_split(const std::string &str, const std::string &delimiter)
{
    std::vector<std::string> pieces;
    std::size_t pos = 0;
    for (;;) {
        std::size_t next_pos = str.find(delimiter, pos);
        if (next_pos == std::string::npos) {
            pieces.emplace_back(str.begin() + pos, str.end());
            break;
        }
        pieces.emplace_back(str.begin() + pos, str.begin() + next_pos);
        pos = next_pos + 1;
    }
    return pieces;
}

bool
is_one_of(const std::string &value, const std::vector<std::string> values)
{
    for (const auto &v : values) {
        if (v == value) {
            return true;
        }
    }
    return false;
}
