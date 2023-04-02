include_directories(../../tools)

set(AUX_TOOLS
    "../../tools/MsgGen.cpp"
    "../../tools/MsgGen.h"
)

# Register tests
unit_tests_register_test(template_mapper.cpp ${AUX_TOOLS})