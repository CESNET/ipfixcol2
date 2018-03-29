# Add header files of the IPFIX Message generator
include_directories(tools)

set(AUX_TOOLS
	"tools/MsgGen.cpp"
	"tools/MsgGen.h"
)

unit_tests_register_test(basic.cpp ${AUX_TOOLS})