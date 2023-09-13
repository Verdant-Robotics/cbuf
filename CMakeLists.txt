cmake_minimum_required(VERSION 3.13.0)

project(cbuf)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if (NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
endif()

option(ENABLE_HJSON "Enable HJSON support" ON)
option(ENABLE_CBUF_TESTS "Enable Tests" ON)

if (${ENABLE_HJSON})
  include(FetchDependency)
  fetch_dependency(hjson "https://github.com/Verdant-Robotics/hjson.git" "main")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  #  GCC specific flags
  set(CMAKE_CXX_FLAGS
          "${CMAKE_CXX_FLAGS} \
    -pedantic \
    -Werror \
    -Wall \
    -Wextra \
    -Wno-sign-conversion \
    -Wno-sign-compare \
    -Wno-format-zero-length \
    -Wno-stringop-truncation \
    -Wno-unused-parameter \
    -Wno-maybe-uninitialized \
    ")
else()
  set(CMAKE_CXX_FLAGS
          "${CMAKE_CXX_FLAGS} \
    -Werror \
    -Wall \
    -Wextra \
    -Weverything \
    -Werror=return-type \
    -Wno-unknown-warning-option \
    -Wno-c++98-compat \
    -Wno-c++98-compat-pedantic \
    -Wno-ctad-maybe-unsupported \
    -Wno-global-constructors \
    -Wno-exit-time-destructors \
    -Wno-double-promotion \
    -Wno-gnu-zero-variadic-macro-arguments")

  # TODO(jhurliman): Fix these warnings
  set(CMAKE_CXX_FLAGS
          "${CMAKE_CXX_FLAGS} \
    -Wno-unused-parameter \
    -Wno-weak-vtables \
    -Wno-old-style-cast \
    -Wno-padded \
    -Wno-missing-prototypes \
    -Wno-shadow \
    -Wno-sign-conversion \
    -Wno-missing-variable-declarations \
    -Wno-missing-noreturn \
    -Wno-switch-enum \
    -Wno-unreachable-code \
    -Wno-shorten-64-to-32 \
    -Wno-zero-as-null-pointer-constant \
    -Wno-sign-compare \
    -Wno-cast-align \
    -Wno-packed \
    -Wno-implicit-int-conversion \
    -Wno-float-equal \
    -Wno-covered-switch-default \
    -Wno-unused-macros \
    -Wno-ignored-qualifiers \
    -Wno-cast-qual \
    -Wno-disabled-macro-expansion \
    -Wno-unreachable-code-return \
    -Wno-zero-as-null-pointer-constant \
    -Wno-deprecated-declarations")
endif()

set(CBUF_PARSE_SRCS
    src/Allocator.cpp
    src/AstPrinter.cpp
    src/CBufParser.cpp
    src/CPrinter.cpp
    src/FileData.cpp
    src/fileutils.cpp
    src/Interp.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/StdStringBuffer.cpp
    src/StringBuffer.cpp
    src/SymbolTable.cpp
    src/TextType.cpp
    src/Token.cpp
)

set(CBUF_SRCS src/cbuf.cpp)

add_library(cbuf_parse STATIC ${CBUF_PARSE_SRCS})
target_include_directories(cbuf_parse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14.0.0")
  # on new compiler version, we need to disable warning
  target_compile_options(cbuf_parse PRIVATE -Wno-unused-but-set-variable)
endif()

# Add a shared version of cbuf_parse for those programs needing position independent code
add_library(cbuf_parse_dynamic SHARED ${CBUF_PARSE_SRCS} ${CBUF_HDRS})
target_include_directories(cbuf_parse_dynamic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
install(TARGETS cbuf_parse_dynamic DESTINATION lib)
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14.0.0")
  # on new compiler version, we need to disable warning
  target_compile_options(cbuf_parse_dynamic PRIVATE -Wno-unused-but-set-variable)
endif()

if (${ENABLE_HJSON})
  target_compile_definitions(cbuf_parse PUBLIC HJSON_PRESENT)
  target_link_libraries(cbuf_parse PUBLIC hjson)
  target_compile_definitions(cbuf_parse_dynamic PUBLIC HJSON_PRESENT)
  target_link_libraries(cbuf_parse_dynamic PUBLIC hjson)
endif()

add_executable(cbuf ${CBUF_SRCS})
target_include_directories(cbuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cbuf PRIVATE cbuf_parse)

if(NOT TARGET cbuf_lib)
  add_library(cbuf_lib INTERFACE)
  add_library(cbuf_lib::cbuf_lib ALIAS cbuf_lib)
  target_include_directories(cbuf_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# Internal cbuf headers for extensions
add_library(cbuf_internal INTERFACE)
target_include_directories(cbuf_internal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(${ENABLE_CBUF_TESTS})
  include(AddTest)
  enable_testing()
  add_subdirectory(test)
endif()

add_subdirectory(ulog)
add_subdirectory(pycbuf)
