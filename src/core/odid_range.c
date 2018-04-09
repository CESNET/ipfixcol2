/**
 * \file src/core/odid_range.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief ODID range filter (internal header file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>

#include "odid_range.h"

/** Default number of pre-allocated filter nodes in a range */
#define NODE_DEF_CNT 4

/** Type of filter nodes */
enum range_node_type {
    /** Interval node (from-to)    */
    RANGE_NODE_INTERVAL,
    /** Value node (single value)  */
    RANGE_NODE_VALUE
};

/** Single range node */
struct range_node {
    /** Type of the node */
    enum range_node_type type;
    union {
        uint32_t val;      /**< Value (in case of #RANGE_NODE_VALUE)       */
        struct {
            uint32_t from; /**< Great or equal                             */
            uint32_t to;   /**< Less or equal                              */
        } interval;        /**< Interval (in case of #RANGE_NODE_INTERVAL) */
    };
};

/** ODID range structure  */
struct ipx_orange {
    /** Array of sorted nodes                      */
    struct range_node *nodes;
    /** Number of valid records in the array       */
    size_t valid;
    /** Number of pre-allocated nodes in the array */
    size_t alloc;
};

/**
 * \brief Add a new range node
 * \param[in] range ODID range filter
 * \param[in] node  Node to add
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
static int
range_add(struct ipx_orange *range, struct range_node node)
{
    if (range->valid == range->alloc) {
        // Add new
        size_t new_alloc = 2 * range->alloc;
        size_t new_size = new_alloc * sizeof(struct range_node);
        struct range_node *new_nodes = realloc(range->nodes, new_size);
        if (!new_nodes) {
            return IPX_ERR_NOMEM;
        }

        range->nodes = new_nodes;
        range->alloc = new_alloc;
    }

    range->nodes[range->valid++] = node;
    return IPX_OK;
}

/**
 * \brief Check if a string is empty or consists only of whitespace characters
 * \param[in] str String to test
 * \return True or false
 */
static bool
range_is_empty(const char *str)
{
    while (*str != '\0') {
        if (isspace(*str) != 0) {
            str++;
            continue;
        }

        // Unexpected character found
        return false;
    }

    return true;
}

/**
 * \brief Convert string to an ODID number
 *
 * String can contain leading and tailing whitespaces that will be ignored.
 * \param[in]  str String to parser
 * \param[out] res ODID value
 * \return #IPX_OK on success and the \p res is filled
 * \return #IPX_ERR_FORMAT if the string is malformed or the number is too big (\p res is undefined)
 */
static int
range_str2uint32(const char *str, uint32_t *res)
{
    static_assert(ULONG_MAX >= UINT32_MAX, "Unsigned long is too small.");

    errno = 0;
    char *end_ptr = NULL;
    unsigned long val = strtoul(str, &end_ptr, 10);
    if (errno != 0) {
        return IPX_ERR_FORMAT; // range
    }

    if (end_ptr == str) {
        // No digits were found
        return IPX_ERR_FORMAT;
    }

    if (!range_is_empty(end_ptr)) {
        // Unexpected (non-whitespace) character(s) after the number
        return IPX_ERR_FORMAT;
    }

    if (val > UINT32_MAX) {
        // Too big value
        return IPX_ERR_FORMAT;
    }

    *res = val;
    return IPX_OK;
}

/**
 * \brief Convert a string into an interval node and add it into the filter
 * \param[in] range ODID range filter
 * \param[in] token Token to process
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the interval token is malformed
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
static int
range_parse_interval(struct ipx_orange *range, char *token)
{
    char *delim_pos = strchr(token, '-');
    assert(delim_pos != NULL);

    // Split the token
    *delim_pos = '\0';
    const char *from_str = token;
    const char *to_str = delim_pos + 1;

    bool from_empty = range_is_empty(from_str);
    bool to_empty = range_is_empty(to_str);
    if (from_empty && to_empty) {
        // At least one side must be always defined
        return IPX_ERR_FORMAT;
    }

    // Parse numbers
    uint32_t from_value;
    uint32_t to_value;
    int rc;

    if (from_empty) {
        from_value = 0;
    } else {
        rc = range_str2uint32(from_str, &from_value);
        if (rc != IPX_OK) {
            return rc;
        }
    }

    if (to_empty) {
        to_value = UINT32_MAX;
    } else {
        rc = range_str2uint32(to_str, &to_value);
        if (rc != IPX_OK) {
            return rc;
        }
    }

    if (from_value > to_value) {
        return IPX_ERR_FORMAT;
    }

    // Add value/interval to the range
    struct range_node node;
    if (from_value == to_value) {
        node.type = RANGE_NODE_VALUE;
        node.val = from_value;
    } else {
        node.type = RANGE_NODE_INTERVAL;
        node.interval.from = from_value;
        node.interval.to = to_value;
    }

    return range_add(range, node);
}

/**
 * \brief Convert a string to a numeric node and add it into the filter
 * \param[in] range ODID range filter
 * \param[in] token Token to process
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the interval token is malformed
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
static int
range_parse_value(struct ipx_orange *range, const char *token)
{
    uint32_t value;
    int rc = range_str2uint32(token, &value);
    if (rc != IPX_OK) {
        return rc;
    }

    // Everything should be ok, add the node
    struct range_node node;
    node.type = RANGE_NODE_VALUE;
    node.val = value;
    return range_add(range, node);
}

/**
 * \brief Comparison function
 * \param[in] p1 First node to compare
 * \param[in] p2 Second node to compare
 * \return The comparison function must return an integer less than, equal to, or greater than zero
 *   if the first argument is considered to be respectively less than, equal to, or greater than
 *   the second.
 */
static int
range_node_cmp(const void *p1, const void *p2)
{
    const struct range_node *node_l = p1;
    const struct range_node *node_r = p2;

    uint32_t min_l = (node_l->type == RANGE_NODE_VALUE)
        ? node_l->val : node_l->interval.from;
    uint32_t min_r = (node_r->type == RANGE_NODE_VALUE)
        ? node_r->val : node_r->interval.from;
    if (min_l < min_r) {
        return -1;
    }
    if (min_l > min_r) {
        return 1;
    }

    uint32_t max_l = (node_l->type == RANGE_NODE_VALUE)
        ? node_l->val : node_l->interval.to;
    uint32_t max_r = (node_r->type == RANGE_NODE_VALUE)
        ? node_r->val : node_r->interval.to;
    if (max_l < max_r) {
        return -1;
    }
    if (max_l > max_r) {
        return 1;
    }

    return 0;
}

/**
 * \brief Sort filter nodes (from the lowest number to the highest)
 * \param range ODID range filter
 */
static void
range_sort(struct ipx_orange *range)
{
    const size_t elem_size = sizeof(struct range_node);
    qsort(range->nodes, range->valid, elem_size, &range_node_cmp);
}


ipx_orange_t *
ipx_orange_create()
{
    struct ipx_orange *res = calloc(1, sizeof(*res));
    if (!res) {
        return NULL;
    }

    const size_t size = NODE_DEF_CNT * sizeof(struct range_node);
    res->nodes = malloc(size);
    if (!res->nodes) {
        free(res);
        return NULL;
    }

    res->valid = 0;
    res->alloc = NODE_DEF_CNT;
    return res;
}

void
ipx_orange_destroy(ipx_orange_t *range)
{
    free(range->nodes);
    free(range);
}

int
ipx_orange_parse(ipx_orange_t *range, const char *expr)
{
    range->valid = 0;
    if (!expr || strlen(expr) == 0) {
        return IPX_ERR_FORMAT;
    }

    // Make a copy of the expression
    char *str2parse = strdup(expr);
    if (!str2parse) {
        return IPX_ERR_NOMEM;
    }

    // Parse all values
    char *token, *save_ptr;
    char *str = str2parse;
    while ((token = strtok_r(str, ",", &save_ptr)) != NULL) {
        str = NULL;

        int rc;
        if (strchr(token, '-') != NULL) {
            rc = range_parse_interval(range, token);
        } else {
            rc = range_parse_value(range, token);
        }

        if (rc != 0) {
            range->valid = 0;
            free(str2parse);
            return rc;
        }
    }

    free(str2parse);

    // Sort and optimize
    range_sort(range);
    return 0;
}

bool
ipx_orange_in(const ipx_orange_t *range, uint32_t odid)
{
    // Try to find value in the sorted array
    for (size_t idx = 0; idx < range->valid; ++idx) {
        const struct range_node *node = &range->nodes[idx];
        if (node->type == RANGE_NODE_VALUE) {
            if (odid == node->val) {
                // Found
                return true;
            }

            if (odid < node->val) {
                break;
            }
        }

        if (node->type == RANGE_NODE_INTERVAL) {
            if (odid >= node->interval.from && odid <= node->interval.to) {
                return true;
            }

            if (odid < node->interval.from) {
                break;
            }
        }
    }

    return false;
}

void
ipx_orange_print(const ipx_orange_t *range)
{
    for (size_t idx = 0; idx < range->valid; ++idx) {
        const struct range_node *node = &range->nodes[idx];
        switch (node->type) {
        case RANGE_NODE_VALUE:
            printf("- value:    %" PRIu32 "\n", node->val);
            break;
        case RANGE_NODE_INTERVAL:
            printf("- interval: %" PRIu32 " - %" PRIu32 "\n",
                node->interval.from, node->interval.to);
            break;
        default:
            printf("- unknown node type!\n");
            break;
        }
    }
}