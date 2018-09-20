# Add header files of the IPFIX Message generator
include_directories(tools)

set(AUX_TOOLS
    "tools/MsgGen.cpp"
    "tools/MsgGen.h"
)

# Copy auxiliary files for tests
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/iana_part.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/data/iana_part.xml"
    COPYONLY
)

# Register tests
unit_tests_register_test(parser_common.cpp ${AUX_TOOLS})