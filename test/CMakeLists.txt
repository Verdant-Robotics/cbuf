cmake_minimum_required(VERSION 3.2.0)

include(BuildCbuf)

build_cbuf(NAME test_cbuf_samples MSG_FILES samples/image.cbuf samples/inctype.cbuf)

add_executable(test_main test_main.cpp test_utils.cpp samples/image.h samples/inctype.h)
target_link_libraries(test_main test_cbuf_samples cbuf_lib uloglib stdc++fs)
target_include_directories(test_main PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples)
add_unit_test(NAME test_main COMMAND test_main)

add_executable(test_ulogger test_ulogger.cpp samples/image.h)
target_link_libraries(test_ulogger gtest test_cbuf_samples cbuf_lib uloglib stdc++fs gtest)
target_include_directories(test_ulogger PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples)
add_unit_test(NAME test_ulogger COMMAND test_ulogger)

add_executable(test_cbuf_parse test_cbuf_parse.cpp)
target_compile_definitions(test_cbuf_parse PUBLIC -DSAMPLES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/samples")
target_link_libraries(test_cbuf_parse PRIVATE gtest cbuf_parse_dynamic cbuf_internal)
add_unit_test(NAME test_cbuf_parse COMMAND test_cbuf_parse)

add_executable(test_parsing test_parsing.cpp test_utils.cpp samples/image.h samples/inctype.h hjson_helper.cpp)
target_link_libraries(test_parsing test_cbuf_samples gtest cbuf_lib cbuf_parse_dynamic )
target_include_directories(test_parsing PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(test_parsing PUBLIC "-Wno-reserved-id-macro")
add_unit_test(NAME test_parsing COMMAND test_parsing)
