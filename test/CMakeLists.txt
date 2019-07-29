cmake_minimum_required( VERSION 3.2.0 )

include(BuildCbuf)
build_cbuf( CBUF_FILE samples/image.cbuf )
build_cbuf( CBUF_FILE samples/inctype.cbuf )

add_custom_target( generate_cbuf_test DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/samples/image.h
  ${CMAKE_CURRENT_BINARY_DIR}/samples/inctype.h
  )

add_executable( test_main test_main.cpp samples/image.h samples/inctype.h )
add_dependencies( test_main generate_cbuf_test )
target_link_libraries( test_main cbuf_lib uloglib stdc++fs )
target_include_directories( test_main PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples )

add_test(
    NAME test_main
    COMMAND test_main
)

add_executable( test_ulogger test_ulogger_main.cpp test_ulogger.cpp samples/image.h )
add_dependencies( test_ulogger generate_cbuf_test )
target_link_libraries( test_ulogger cbuf_lib uloglib stdc++fs )
target_include_directories( test_ulogger PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples )

set_target_properties( test_ulogger PROPERTIES CXX_STANDARD 17 )
# target_compile_options( test_ulogger PRIVATE -ftime-trace )

add_test(
    NAME test_ulogger
    COMMAND test_ulogger
)

