# Add header files of the IPFIX Message generator
include_directories(tools)

set(AUX_TOOLS
    "tools/MsgGen.cpp"
    "tools/MsgGen.h"
)

# Register tests
unit_tests_register_test(nf_v5.cpp)
unit_tests_register_test(nf_v9.cpp ${AUX_TOOLS})