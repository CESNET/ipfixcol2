#pragma once

#include <string>
#include <mutex>
#include <vector>

class FileList {
public:
    using Iterator = std::vector<std::string>::const_iterator;

	void
	add_files(const std::string &pattern);

    bool
    pop(std::string &filename);

    size_t length() const { return m_files.size(); }

    size_t empty() const { return m_files.empty(); }

    Iterator begin() const { return m_files.cbegin(); }

    Iterator end() const { return m_files.cend(); }

private:
    std::mutex m_mutex;
	std::vector<std::string> m_files;
};