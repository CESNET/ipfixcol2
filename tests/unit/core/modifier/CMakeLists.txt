# Add header files of the IPFIX Modifier
include_directories(../../tools)

set(AUX_TOOLS
    "../../tools/MsgGen.cpp"
    "../../tools/MsgGen.h"
)

# Register tests
unit_tests_register_test(modifier_append_and_filter.cpp ${AUX_TOOLS})