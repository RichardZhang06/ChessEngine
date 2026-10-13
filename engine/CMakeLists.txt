cmake_minimum_required(VERSION 3.16)
project(chess_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# The backend looks for the binary at engine/build/engine by default.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

file(GLOB ENGINE_SOURCES src/*.cpp)
list(REMOVE_ITEM ENGINE_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)

# Compile the opening book (book/openings.txt, built by tools/build_book.py) into the binary as a byte array.
set(BOOK_TXT ${CMAKE_SOURCE_DIR}/book/openings.txt)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BOOK_TXT})
file(READ ${BOOK_TXT} BOOK_HEX HEX)
string(LENGTH "${BOOK_HEX}" BOOK_HEX_LENGTH)
math(EXPR BOOK_SIZE "${BOOK_HEX_LENGTH} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BOOK_BYTES "${BOOK_HEX}")
file(WRITE ${CMAKE_BINARY_DIR}/book_data.cpp
  "#include <cstddef>\nextern const unsigned char EMBEDDED_BOOK[];\nextern const size_t EMBEDDED_BOOK_SIZE;\n"
  "const unsigned char EMBEDDED_BOOK[] = {${BOOK_BYTES}0};\nconst size_t EMBEDDED_BOOK_SIZE = ${BOOK_SIZE};\n")

add_library(engine_core STATIC ${ENGINE_SOURCES} ${CMAKE_BINARY_DIR}/book_data.cpp)
target_include_directories(engine_core PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(engine_core PUBLIC Threads::Threads)

add_executable(engine src/main.cpp)
target_link_libraries(engine engine_core)

# Simple opponents used by tools/regression.py to rate the engine
add_executable(sample_engine tools/sample_engine.cpp)
target_link_libraries(sample_engine engine_core)

enable_testing()
file(GLOB TEST_SOURCES tests/*_test.cpp)
foreach(test_src ${TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} engine_core)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
