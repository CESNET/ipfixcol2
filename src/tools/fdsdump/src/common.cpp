
#include <algorithm>

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

void
string_ltrim(std::string &str)
{
    auto is_space = [](char ch) {
        return std::isspace(int(ch));
    };

    auto first_char = std::find_if_not(str.begin(), str.end(), is_space);
    str.erase(str.begin(), first_char);
}

void
string_rtrim(std::string &str)
{
    auto is_space = [](char ch) {
        return std::isspace(int(ch));
    };

    auto last_char = std::find_if_not(str.rbegin(), str.rend(), is_space);
    str.erase(last_char.base(), str.end());
}

void
string_trim(std::string &str)
{
    string_rtrim(str);
    string_ltrim(str);
}

std::string
string_trim_copy(std::string str)
{
    string_trim(str);
    return str;
}
