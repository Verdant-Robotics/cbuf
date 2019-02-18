cmake_minimum_required( VERSION 3.2.0 )

include_directories( ${CMAKE_ROOT}/include )

add_custom_command(
    OUTPUT image.h
    COMMAND ${CMAKE_BINARY_DIR}/vrm ${CMAKE_CURRENT_SOURCE_DIR}/samples/image.vrm > image.h
    DEPENDS vrm ${CMAKE_CURRENT_SOURCE_DIR}/samples/image.vrm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} )
add_executable( test_main test_main.cpp image.h )

add_test(
    NAME test_main
    COMMAND test_main
)