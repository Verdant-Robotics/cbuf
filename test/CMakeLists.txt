cmake_minimum_required(VERSION 3.2.0)

include(BuildCbuf)

find_package(GTest REQUIRED)

build_cbuf(NAME test_cbuf_samples MSG_FILES samples/image.cbuf samples/inctype.cbuf)

add_executable(test_main test_main.cpp samples/image.h samples/inctype.h)
target_link_libraries(test_main test_cbuf_samples cbuf_lib uloglib stdc++fs)
target_include_directories(test_main PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples)
add_test(NAME test_main COMMAND test_main)

add_executable(test_ulogger test_ulogger.cpp samples/image.h)
target_link_libraries(test_ulogger gtest test_cbuf_samples cbuf_lib uloglib stdc++fs)
target_include_directories(test_ulogger PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples)
add_test(NAME test_ulogger COMMAND test_ulogger)
