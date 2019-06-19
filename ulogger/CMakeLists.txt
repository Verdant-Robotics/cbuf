cmake_minimum_required( VERSION 3.2.0 )

include(BuildCbuf)

build_cbuf( cbufmsg/metadata.cbuf )

add_custom_target( generate_cbuflogger DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/cbufmsg/metadata.h
  )

add_library( ulogger SHARED ulogger.cpp cbuf_stream.cpp )
target_include_directories( ulogger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )
target_include_directories( ulogger PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cbufmsg )
add_dependencies( ulogger generate_cbuflogger )
target_link_libraries( ulogger PUBLIC cbuf_lib vlog pthread )

