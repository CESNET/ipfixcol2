#include "utils/filelist.hpp"
#include <glob.h>
#include <cassert>

void
FileList::add_files(const std::string &pattern)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    glob_t globbuf = {};

    int ret = glob(pattern.c_str(), GLOB_MARK, NULL, &globbuf);

    if (ret == GLOB_NOSPACE) {
        throw std::bad_alloc();
    }

    if (ret == GLOB_ABORTED) {
        throw std::runtime_error("glob failed");
    }

    if (ret == GLOB_NOMATCH) {
        // This is probably fine?
        return;
    }

    assert(ret == 0);

    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        std::string filename = globbuf.gl_pathv[i];

        if (filename[filename.size() - 1] == '/') {
            // Skip directories
            continue;
        }

        m_files.push_back(std::move(filename));
    }
}

bool
FileList::pop(std::string &filename)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (empty()) {
        return false;
    }

    filename = std::move(*m_files.begin());
    m_files.erase(m_files.begin());
    return true;
}
