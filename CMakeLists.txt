cmake_minimum_required( VERSION 3.2.0 )

project( cbuf )

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

include(FetchDependency)

fetch_dependency( vlog "https://git.martian.ag/open-source/vlog.git" "master" )


option(CBUF_ENABLE_TESTS "Enable Tests" ON)
if (${CBUF_ENABLE_TESTS})
    enable_testing()
endif()

set( CBUF_PARSE_SRCS
  src/AstPrinter.cpp src/CPrinter.cpp src/FileData.cpp src/Lexer.cpp
  src/Parser.cpp src/Allocator.cpp src/StringBuffer.cpp src/SymbolTable.cpp
  src/TextType.cpp src/Token.cpp src/CBufParser.cpp
)

set( CBUF_SRCS src/cbuf.cpp )

add_library( cbuf_parse STATIC ${CBUF_PARSE_SRCS} )
target_include_directories(cbuf_parse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )


add_executable( cbuf ${CBUF_SRCS} )
target_include_directories(cbuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )
target_link_libraries(cbuf PRIVATE cbuf_parse )

if (NOT TARGET cbuf_lib)
    add_library( cbuf_lib INTERFACE )
    add_library( cbuf_lib::cbuf_lib ALIAS cbuf_lib )
    target_include_directories(cbuf_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include )
endif()

add_subdirectory( ulog )

if (${CBUF_ENABLE_TESTS})
    add_subdirectory( test )
endif()

