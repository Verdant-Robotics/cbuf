cmake_minimum_required( VERSION 3.2.0 )

include(BuildCbuf)
build_cbuf( samples/image.cbuf )

add_custom_target( generate_cbuf_test DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/samples/image.h
  )

add_executable( test_main test_main.cpp samples/image.h )
add_dependencies( test_main generate_cbuf_test )
target_link_libraries( test_main cbuf_lib uloglib stdc++fs )
target_include_directories( test_main PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/samples )

add_test(
    NAME test_main
    COMMAND test_main
)
