cmake_minimum_required(VERSION 3.2.0)

include(BuildCbuf)

build_cbuf(NAME meta_cbuf MSG_FILES cbufmsg/metadata.cbuf)

add_library(uloglib SHARED src/ulogger.cpp src/cbuf_stream.cpp src/cbuf_reader.cpp)
target_include_directories(uloglib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(uloglib PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cbufmsg)
target_link_libraries(uloglib PUBLIC meta_cbuf cbuf_lib cbuf_parse_dynamic vlog pthread stdc++fs)

install(TARGETS uloglib DESTINATION lib)
