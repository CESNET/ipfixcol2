#include "common.hpp"
#include <glob.h>

unique_fds_iemgr
make_iemgr()
{
    int rc;

    unique_fds_iemgr iemgr;
    iemgr.reset(fds_iemgr_create());
    if (!iemgr) {
        throw std::bad_alloc();
    }

    rc = fds_iemgr_read_dir(iemgr.get(), fds_api_cfg_dir());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot read iemgr definitions");
    }

    return iemgr;
}

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

std::vector<std::string>
match_files(const std::string &pattern)
{
    std::vector<std::string> files;

    glob_t globbuf = {};

    int ret = glob(pattern.c_str(), GLOB_MARK, NULL, &globbuf);

    if (ret == GLOB_NOSPACE) {
        throw std::bad_alloc();
    }

    if (ret == GLOB_ABORTED) {
        throw std::runtime_error("glob failed");
    }

    if (ret == GLOB_NOMATCH) {
        return {};
    }

    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        std::string filename = globbuf.gl_pathv[i];

        if (filename[filename.size() - 1] == '/') {
            // Skip directories
            continue;
        }

        files.push_back(std::move(filename));
    }

    return files;
}