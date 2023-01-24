# Add header files of the IPFIX Message generator
include_directories(../../tools)

set(AUX_TOOLS
    "../../tools/MsgGen.cpp"
    "../../tools/MsgGen.h"
)

# Register tests
unit_tests_register_test(message_builder.cpp ${AUX_TOOLS})