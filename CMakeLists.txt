cmake_minimum_required( VERSION 3.2.0 )

project( cbuf )

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )


option(CBUF_ENABLE_TESTS "Enable Tests" ON)
if (${CBUF_ENABLE_TESTS})
    enable_testing()
endif()

set( CBUF_SRCS 
    src/AstPrinter.cpp src/CPrinter.cpp src/FileData.cpp src/Lexer.cpp
    src/Parser.cpp src/PoolAllocator.cpp src/StringBuffer.cpp src/SymbolTable.cpp
    src/TextType.cpp src/Token.cpp src/cbuf.cpp
)

add_executable( cbuf ${CBUF_SRCS} )
target_include_directories(cbuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )

include(BuildCbuf)
build_cbuf( stream/metadata.cbuf )

add_custom_target( generate_cbuf_meta DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/stream/metadata.h
  )

if (NOT TARGET cbuf_lib)
    add_library( cbuf_lib INTERFACE )
    add_library( cbuf_lib::cbuf_lib ALIAS cbuf_lib )
    add_dependencies( cbuf_lib generate_cbuf_meta )
    target_include_directories(cbuf_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include )
    target_include_directories(cbuf_lib INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/stream )
endif()

if (${CBUF_ENABLE_TESTS})
    add_subdirectory( test )
endif()

