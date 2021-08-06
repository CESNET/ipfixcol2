#pragma once

#include <string>
#include <mutex>
#include <queue>

class FileList {
public:
	void
	add_files(const std::string &pattern);

    bool
    has_next_file();

    std::string
    pop_next_file();

    size_t
    length();

private:
    std::mutex m_mutex;
	std::queue<std::string> m_files;
};