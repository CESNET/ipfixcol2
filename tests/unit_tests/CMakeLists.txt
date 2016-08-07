cmake_minimum_required(VERSION 2.8.8)
include(Tests.cmake)

# List of tests
ipx_add_test_file("src/convertors.cpp")
ipx_add_test_file("src/verbose.cpp")
# >> Add your new tests HERE <<

ipx_code_coverage()

