#!/bin/bash
# @file
# @author Michal Sedlak <sedlakm@cesnet.cz>
# @brief Plugin template generator script
# @date 2024
#
# Copyright(c) 2024 CESNET z.s.p.o.
# SPDX-License-Identifier: BSD-3-Clause

set -eo pipefail # Immediately exit on command or pipe failures instead of continuing

cd "$(dirname "$0")" # cd to script dir
cd ../../ # cd to ipfixcol2 root dir

# Gather user input
echo "Enter plugin name"
while true; do
    echo -n "> "
    read -r plugin_name
    if ! [[ "$plugin_name" =~ ^[a-zA-Z0-9_-]+$ ]]; then
        echo "Plugin name may only contain alphanumeric characters, underscores and dashes."
    else
        break
    fi
done
plugin_name="${plugin_name,,}"

echo "Choose plugin type:"
echo " 1. input"
echo " 2. intermediate"
echo " 3. output"
while [ -z "$plugin_type" ]; do
    echo -n "> "
    read -r choice
    case "$choice" in
        1 | input) plugin_type=input ;;
        2 | intermediate) plugin_type=intermediate ;;
        3 | output) plugin_type=output ;;
        *) echo "Invalid choice, type 1/2/3" ;;
    esac
done

plugin_dir="src/plugins/${plugin_type}/${plugin_name}"

if [ -x "$plugin_dir" ]; then
    echo "Directory '$plugin_dir' already exists!"
    exit 1
fi

# Create directory structure
mkdir "$plugin_dir/"
mkdir "$plugin_dir/doc/"
mkdir "$plugin_dir/src/"

# Generate main.c
cat > "$plugin_dir/src/main.c" << EOF
#include <ipfixcol2.h>

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_OUTPUT,
    // Plugin identification name
    .name = "$plugin_name",
    // Brief description of plugin
    .dsc = "${plugin_name^} ${plugin_type} plugin.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "0.1.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

/** Instance */
struct instance_data {

};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    // Create a private data
    struct instance_data *data = calloc(1, sizeof(*data));
    if (!data) {
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, data);

    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    struct instance_data *data = (struct instance_data *) cfg;

    free(data);
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    struct instance_data *data = (struct instance_data *) cfg;

    return IPX_OK;
}
EOF

# Generate CMakeLists.txt
cat > "$plugin_dir/CMakeLists.txt" << EOF
# Create a linkable module
add_library(${plugin_name}-${plugin_type} MODULE
    src/main.c
)

install(
    TARGETS ${plugin_name}-${plugin_type}
    LIBRARY DESTINATION "\${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "\${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-${plugin_name}-${plugin_type}.7.rst")
    set(DST_FILE "\${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-${plugin_name}-${plugin_type}.7")

    add_custom_command(TARGET ${plugin_name}-${plugin_type} PRE_BUILD
        COMMAND \${RST2MAN_EXECUTABLE} --syntax-highlight=none \${SRC_FILE} \${DST_FILE}
        DEPENDS \${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "\${DST_FILE}"
        DESTINATION "\${INSTALL_DIR_MAN}/man7"
    )
endif()
EOF

# Generate README.rst
cat > "$plugin_dir/README.rst" << EOF

${plugin_name^} (${plugin_type} plugin)
=====================

This is an auto-generated template. Enter your plugin documentation here.
EOF

# Generate manpage
author_name=$(git config user.name || echo "Auto-generated template")
author_email=$(git config user.email || echo "author@example.com")
year_today=$(date +%Y)
date_today=$(date +%F)
title=" ipfixcol2-${plugin_name}-${plugin_type}"
subtitle="${plugin_name^} (${plugin_type} plugin)"
cat > "$plugin_dir/README.rst" << EOF
${title//?/=}
$title
${title//?/=}

${subtitle//?/-}
$subtitle
${subtitle//?/-}

:Author: ${author_name} (${author_email})
:Date:   $date_today
:Copyright: Copyright © $year_today CESNET, z.s.p.o.
:Version: 0.1
:Manual section: 7
:Manual group: IPFIXcol collector

Description
-----------

.. include:: ../README.rst
   :start-line: 3
EOF

# Add the new plugin to the upper-level CMakeLists.txt
echo -en "\nadd_subdirectory($plugin_name)" >> "src/plugins/${plugin_type}/CMakeLists.txt"

# Done
echo "Created directory '$plugin_dir'"
