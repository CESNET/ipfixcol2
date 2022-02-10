
#pragma once

#include <memory>
#include <string>
#include <list>

#include <libfds.h>

#include "common.hpp" // unique_file, unique_iemgr
#include "flow.hpp"

class FlowProvider {
public:
    FlowProvider(const shared_iemgr &iemgr);
    ~FlowProvider() = default;

    FlowProvider(const FlowProvider &) = delete;
    FlowProvider &operator=(const FlowProvider &) = delete;

    /**
     * @brief Add a new file to process
     * @param[in] file Path to the file
     */
    void
    add_file(const std::string &file);

    /**
     * @brief Set a flow filter.
     * @param[in] expr Filtration expression.
     */
    void
    set_filter(const std::string &expr);

    /**
     * @brief Enable heuristic that automatically hides specific direction of
     * biflow record that seems to be empty.
     *
     * Some expoters migth send uniflow records as biflow records but we usually
     * don't want to show these values to a user. If enabled, the <em>match</em>
     * variable of Record is modified to hide direction that seems to be empty
     * even if it matches the filter.
     * @param[in] enable True/false
     */
    void
    set_biflow_autoignore(bool enable);

    /**
     * @brief Get the next flow record
     * @return Pointer or NULL (no more records to process)
     */
    Flow *
    next_record();

private:
    bool prepare_next_file();
    bool prepare_next_record();
    enum Direction filter_record(struct fds_drec *rec);
    enum Direction biflow_autoignore(struct fds_drec *rec);

    bool field_has_only_zero_value(struct fds_drec *rec, uint16_t id, bool reverse);
    bool direction_is_empty(struct fds_drec *rec, bool reverse);

    std::list<std::string> m_remains;

    shared_iemgr m_iemgr;
    unique_filter m_filter {nullptr, &fds_ipfix_filter_destroy};
    unique_file m_file {nullptr, &fds_file_close};
    bool m_file_ready = false;
    bool m_biflow_autoignore = false;

    Flow m_flow;
};
