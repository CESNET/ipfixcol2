
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <libfds.h>

using unique_file = std::unique_ptr<fds_file_t, decltype(&fds_file_close)>;
using unique_iemgr = std::unique_ptr<fds_iemgr_t, decltype(&fds_iemgr_destroy)>;
using unique_filter = std::unique_ptr<fds_ipfix_filter, decltype(&fds_ipfix_filter_destroy)>;
using shared_iemgr = std::shared_ptr<fds_iemgr_t>;
using shared_tsnapshot = std::shared_ptr<fds_tsnapshot_t>;

/**
 * @brief Split string using a user-defined delimeter.
 *
 * If no delimeter is found, the result contains only the original string.
 * @param str String to be splitted
 * @param delimiter String delimeter
 * @return Vector of piecies.
 */
std::vector<std::string>
string_split(const std::string &str, const std::string &delimiter);

void
string_ltrim(std::string &str);

void
string_rtrim(std::string &str);

void
string_trim(std::string &str);

std::string
string_trim_copy(std::string str);
