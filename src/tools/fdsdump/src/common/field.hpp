/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Abstraction for IPFIX/alias field
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <functional>
#include <string>

#include <libfds.h>

#include <common/common.hpp>

namespace fdsdump {

/**
 * @brief Data type of a record field.
 */
enum class FieldType {
    none,            ///< Unknown or unsupported type
    bytes,           ///< IPFIX octetArray
    num_unsigned,    ///< IPFIX unsigned{8,16,32,64}
    num_signed,      ///< IPFIX signed{8,16,32,64}
    num_float,       ///< IPFIX float{32,64}
    boolean,         ///< IPFIX boolean
    macaddr,         ///< IPFIX macaddr
    string,          ///< IPFIX string
    datetime,        ///< IPFIX dateTime{Seconds,Milliseconds,Microseconds,Nanoseconds}
    ipaddr,          ///< IPFIX ipv4Address,ipv6Address
    list,            ///< IPFIX basicList,subTemplateList,subTemplateMultiList
};

/**
 * @brief Description of a field of an IPFIX Data Record.
 *
 * The class contains reference to either an alias for one or more IPFIX
 * elements or an IPFIX element, but not both at the same time.
 */
struct Field {
public:
    /**
     * @brief Create a new getter for an alias or an IPFIX Field.
     *
     * @param name  Name of alias or an IPFIX Element to lookup
     * @throw std::invalid_argument If the alias or element cannot be found.
     */
    Field(std::string name);
    ~Field() = default;

    /**
     * @brief Get the name of the field.
     */
    const std::string &get_name() const { return m_name; };
    /**
     * @brief Get a reference to alias definition (might be nullptr).
     */
    const struct fds_iemgr_alias *get_alias() const { return m_alias; };
    /**
     * @brief Get a reference to IPFIX element definition (might be nullptr).
     */
    const struct fds_iemgr_elem *get_element() const { return m_elem; };

    /**
     * @brief Get data type of the field.
     *
     * In case of an alias, it represents the common type of all elements.
     * However, if the common type cannot be determined, FieldType::none is
     * returned.
     */
    FieldType get_type() const { return m_type; };

    /**
     * @brief Test if the field is an alias for one or more IPFIX Elements.
     */
    bool is_alias() const { return m_alias != nullptr; };
    /**
     * @brief Test if the field is an IPFIX Element.
     */
    bool is_element() const { return m_elem != nullptr; };

    /**
     * @brief For each occurence of the field in a Data Record call
     * a user-defined callback.
     *
     * @param rec     IPFIX Data record where to look for occurences.
     * @param cb      Callback to call on each field occurence.
     * @param reverse Look for the field from the reverse direction of biflow (if available)
     * @return Number of occurrences.
     */
    unsigned int for_each(
        struct fds_drec *rec,
        std::function<void(fds_drec_field &)> cb,
        bool reverse);

private:
    // Name of the field (trimmed)
    std::string m_name;
    // Data type of the the field
    FieldType m_type;
    // Alias definition (might be nullptr)
    const struct fds_iemgr_alias *m_alias = nullptr;
    // Element definition (might be nullptr)
    const struct fds_iemgr_elem *m_elem = nullptr;

    unsigned int for_each_alias(
        struct fds_drec *rec,
        std::function<void(struct fds_drec_field &)> cb,
        bool reverse);
    unsigned int for_each_element(
        struct fds_drec *rec,
        std::function<void(struct fds_drec_field &)> cb,
        bool reverse);

    FieldType get_type_of_element(const struct fds_iemgr_elem *elem) const;
    FieldType get_type_of_alias(const struct fds_iemgr_alias *alias) const;
};

} // fdsdump
