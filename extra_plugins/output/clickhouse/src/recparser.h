/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Record parser
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "column.h"
#include <ipfixcol2.h>
#include <libfds.h>
#include <vector>

/**
 * @class RecParser
 * @brief Parses data records based on a given template and column configuration.
 *
 * The RecParser class is responsible for mapping fields from a data record
 * to the corresponding columns, handling biflow templates, and determining
 * whether certain records should be skipped.
 */
class RecParser {
public:
    /**
     * @brief Constructs a RecParser instance.
     *
     * @param columns A vector of Column objects.
     * @param tmplt A pointer to the template used for parsing.
     * @param biflow_autoignore Whether to automatically ignore empty biflow records.
     */
    RecParser(const std::vector<Column>& columns, const fds_template *tmplt, bool biflow_autoignore);

    /**
     * @brief Parses a data record and maps its fields to columns.
     *
     * @param rec The data record to parse.
     */
    void parse_record(fds_drec &rec);

    /**
     * @brief Retrieves the field corresponding to a specific column.
     *
     * If the field was not found in the record, the returned field points to
     * NULL and has size of 0.
     *
     * If the provided index is outside of bounds, the behavior is undefined.
     *
     * @param idx The index of the column.
     * @param rev Whether to retrieve the reverse field (for biflow templates).
     * @return A reference to the corresponding field.
     */
    fds_drec_field& get_column(std::size_t idx, bool rev = false);

    /**
     * @brief Checks whether the forward direction of a biflow should be skipped.
     *
     * @return True if the forward direction should be skipped, otherwise false.
     */
    bool skip_fwd() const;

    /**
     * @brief Checks whether the reverse direction of a biflow should be skipped.
     *
     * @return True if the reverse direction should be skipped, otherwise false.
     */
    bool skip_rev() const;

    const fds_template *tmplt() const;

private:
    struct tmplt_destroy { void operator()(fds_template *t) const { fds_template_destroy(t); } };

    std::unique_ptr<fds_template, tmplt_destroy> m_tmplt;
    bool m_biflow; // Indicates whether the template is biflow.
    bool m_biflow_autoignore; // Perform the skip checks.
    bool m_skip_flag_fwd; // Skip the fwd direction of this biflow.
    bool m_skip_flag_rev; // Skip the reverse direction of this biflow.
    std::vector<int> m_mapping; // Index of field in drec -> index of field in the field vec.
    std::vector<int> m_mapping_rev; // Index of field in drec -> index of field in the reverse field vec.
    std::vector<fds_drec_field> m_fields; // Field for the nth column.
    std::vector<fds_drec_field> m_fields_rev; // Reverse field for the nth column.
};

/**
 * @class RecParserManager
 * @brief Manages RecParser instances for different sessions, ODIDs, and templates.
 *
 * The RecParserManager class provides functionality to manage multiple RecParser
 * instances, allowing selection of active sessions and ODIDs, and retrieval of
 * parsers for specific templates.
 */
class RecParserManager {
public:
    /**
     * @brief Constructs a RecParserManager instance.
     *
     * @param columns A vector of Column objects defining the mapping.
     * @param biflow_autoignore Whether to automatically ignore biflow records.
     */
    RecParserManager(const std::vector<Column> &columns, bool biflow_autoignore);

    /**
     * @brief Selects the active session.
     *
     * @param sess A pointer to the session to select.
     */
    void select_session(const ipx_session *sess);

    /**
     * @brief Selects the active ODID (Observation Domain ID).
     *
     * @param odid The ODID to select.
     */
    void select_odid(uint32_t odid);

    /**
     * @brief Deletes a session and its associated data.
     *
     * @param sess A pointer to the session to delete.
     */
    void delete_session(const ipx_session *sess);

    /**
     * @brief Retrieves the RecParser for a specific template.
     *
     * @param tmplt A pointer to the template.
     * @return A reference to the corresponding RecParser.
     */
    RecParser& get_parser(const fds_template *tmplt);

private:
    using TemplateMap = std::unordered_map<uint16_t, RecParser>; ///< Map of template IDs to RecParser instances.
    using OdidMap = std::unordered_map<uint32_t, TemplateMap>; ///< Map of ODIDs to TemplateMap instances.
    using SessionMap = std::unordered_map<const ipx_session *, OdidMap>; ///< Map of sessions to OdidMap instances.

    const std::vector<Column> &m_columns; ///< Reference to the column configuration.
    bool m_biflow_autoignore; ///< Whether to automatically ignore biflow records.

    TemplateMap *m_active_odid = nullptr; ///< Pointer to the active TemplateMap.
    OdidMap *m_active_session = nullptr; ///< Pointer to the active OdidMap.
    SessionMap m_sessions; ///< Map of sessions to their associated ODID maps.
};
