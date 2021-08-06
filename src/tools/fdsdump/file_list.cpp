#include "file_list.hpp"
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

        m_files.push(std::move(filename));
    }
}

bool
FileList::has_next_file()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return !m_files.empty();
}

std::string
FileList::pop_next_file()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    std::string result = m_files.front();
    m_files.pop();
    return result;
}

size_t
FileList::length()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_files.size();
}


