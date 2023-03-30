# Add header files of the IPFIX Modifier
include_directories(../../tools)

set(AUX_TOOLS
    "../../tools/MsgGen.cpp"
    "../../tools/MsgGen.h"
)

# Register tests
unit_tests_register_test(modifier_modify_message.cpp ${AUX_TOOLS})
unit_tests_register_test(modifier_context.cpp ${AUX_TOOLS})