cmake_minimum_required( VERSION 3.2.0 )

project( vrm )

option(ENABLE_TESTS "Enable Tests" OFF)
if (${ENABLE_TESTS})
    enable_testing()
endif()

set( VRM_SRCS 
    src/AstPrinter.cpp src/CPrinter.cpp src/FileData.cpp src/Lexer.cpp
    src/Parser.cpp src/PoolAllocator.cpp src/StringBuffer.cpp src/SymbolTable.cpp
    src/TextType.cpp src/Token.cpp src/vrm.cpp
)

add_executable( vrm ${VRM_SRCS} )
target_include_directories(vrm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )

add_library( vrm_lib INTERFACE )
target_include_directories(vrm_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include )

add_subdirectory( test )