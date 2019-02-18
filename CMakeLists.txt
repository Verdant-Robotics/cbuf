cmake_minimum_required( VERSION 3.2.0 )

project( vrm )

option(ENABLE_TESTS "Enable Tests" OFF)
if (${ENABLE_TESTS})
    enable_testing()
endif()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

set( VRM_SRCS 
    src/AstPrinter.cpp src/CPrinter.cpp src/FileData.cpp src/Lexer.cpp
    src/Parser.cpp src/PoolAllocator.cpp src/StringBuffer.cpp src/SymbolTable.cpp
    src/TextType.cpp src/Token.cpp src/vrm.cpp
)

set( VRM_HDRS
    include/vrm_preamble.h
)

add_executable( vrm ${VRM_SRCS} )

add_subdirectory( test )