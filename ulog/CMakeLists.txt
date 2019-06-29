cmake_minimum_required( VERSION 3.2.0 )

include(BuildCbuf)

build_cbuf( cbufmsg/metadata.cbuf )

add_custom_target( generate_cbufulog DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/cbufmsg/metadata.h
  )

add_library( uloglib SHARED ulogger.cpp cbuf_stream.cpp )
target_include_directories( uloglib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )
target_include_directories( uloglib PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cbufmsg )
add_dependencies( uloglib generate_cbufulog )
target_link_libraries( uloglib PUBLIC cbuf_lib vlog pthread stdc++fs )

