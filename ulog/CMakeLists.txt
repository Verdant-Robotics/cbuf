cmake_minimum_required(VERSION 3.13.0)

include(BuildCbuf)

build_cbuf(NAME meta_cbuf MSG_FILES cbufmsg/metadata.cbuf)

set(ULOGLIB_SRCS src/ulogger.cpp src/cbuf_readerbase.cpp src/cbuf_reader.cpp)

# Core cbuf library to handle a stream of cbufs
add_library(cbuf_stream STATIC src/cbuf_stream.cpp)
target_include_directories(cbuf_stream PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(cbuf_stream PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cbufmsg)
target_link_libraries(cbuf_stream PUBLIC meta_cbuf cbuf_lib cbuf_parse pthread)

# uloglib is a library for handling ulogger, how we create logs, and cbufreader, the C++
# Class using templates to have callbacks as we read messages
add_library(uloglib SHARED ${ULOGLIB_SRCS})
target_include_directories(uloglib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uloglib PUBLIC cbuf_stream)

# Build a static version of uloglib
add_library(uloglib_static STATIC ${ULOGLIB_SRCS})
target_include_directories(uloglib_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(uloglib_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cbufmsg)
target_link_libraries(uloglib_static PUBLIC cbuf_stream pthread)

add_library(ringbufferlib INTERFACE)
target_include_directories(ringbufferlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(ringbufferlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/ringbuffer.h)

install(TARGETS uloglib cbuf_stream DESTINATION lib)
