cmake_minimum_required(VERSION 3.18)

find_package(Python3 COMPONENTS Development.Module REQUIRED)

add_library(pycbuf MODULE pycbuf.cpp CBufParserPy.cpp cbuf_reader_python.cpp)
target_include_directories(pycbuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pycbuf)
target_include_directories(pycbuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(pycbuf Python3::Module cbuf_parse uloglib_static)

# Remove the "lib" prefix from the output file name
set_target_properties(pycbuf PROPERTIES PREFIX "")

target_compile_options(pycbuf PRIVATE -Wno-unused-function)

install(TARGETS pycbuf DESTINATION pycbuf)
