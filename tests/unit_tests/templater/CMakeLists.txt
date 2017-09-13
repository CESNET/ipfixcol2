# List of test files
unit_tests_register_test("templater.cpp")
unit_tests_register_test("templater_basic.cpp")

file(COPY individual.xml DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
