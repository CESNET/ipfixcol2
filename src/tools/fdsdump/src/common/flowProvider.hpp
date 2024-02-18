
#pragma once

#include <common/common.hpp> // unique_file, unique_iemg
#include <common/flow.hpp>

#include <libfds.h>

#include <memory>
#include <string>
#include <list>

namespace fdsdump {

class FlowProvider {
public:
    FlowProvider();
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

    /**
     * @brief Reset the read and loaded flow counters
     */
    void
    reset_counters();

    /**
     * @brief Get the number of processed flows, i.e. the number of flows in files that we went through
     */
    uint64_t get_processed_flow_count() const { return m_processed_flow_count; }

    /**
     * @brief Get the total number of loaded flows, i.e. the number of flows in files that have been added
     */
    uint64_t get_total_flow_count() const { return m_total_flow_count; }

private:
    bool prepare_next_file();
    bool prepare_next_record();
    enum Direction filter_record(struct fds_drec *rec);
    enum Direction biflow_autoignore(struct fds_drec *rec);

    bool field_has_only_zero_value(struct fds_drec *rec, uint16_t id, bool reverse);
    bool direction_is_empty(struct fds_drec *rec, bool reverse);

    std::list<std::string> m_remains;

    unique_filter m_filter {nullptr, &fds_ipfix_filter_destroy};
    unique_file m_file {nullptr, &fds_file_close};
    bool m_file_ready = false;
    bool m_biflow_autoignore = false;

    uint64_t m_processed_flow_count = 0;
    uint64_t m_total_flow_count = 0;

    Flow m_flow;
};

} // fdsdump
