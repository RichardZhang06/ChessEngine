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

add_library(engine_core STATIC ${ENGINE_SOURCES})
target_include_directories(engine_core PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(engine_core PUBLIC Threads::Threads)

add_executable(engine src/main.cpp)
target_link_libraries(engine engine_core)

enable_testing()
file(GLOB TEST_SOURCES tests/*_test.cpp)
foreach(test_src ${TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} engine_core)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
