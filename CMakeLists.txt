cmake_minimum_required( VERSION 3.2.0 )

project( cbuf )

option(CBUF_ENABLE_TESTS "Enable Tests" ON)
if (${ENABLE_TESTS})
    enable_testing()
endif()

set( CBUF_SRCS 
    src/AstPrinter.cpp src/CPrinter.cpp src/FileData.cpp src/Lexer.cpp
    src/Parser.cpp src/PoolAllocator.cpp src/StringBuffer.cpp src/SymbolTable.cpp
    src/TextType.cpp src/Token.cpp src/cbuf.cpp
)

add_executable( cbuf ${CBUF_SRCS} )
target_include_directories(cbuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )

if (NOT TARGET cbuf_lib)
    add_library( cbuf_lib INTERFACE )
    add_library( cbuf_lib::cbuf_lib ALIAS cbuf_lib )
    target_include_directories(cbuf_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include )
endif()

if (${ENABLE_TESTS})
    add_subdirectory( test )
endif()
