cmake_minimum_required(VERSION 3.16)
project(samp_native VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(samp_portable INTERFACE)
target_include_directories(samp_portable INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_executable(test_encoding tests/test_encoding.cpp)
target_link_libraries(test_encoding PRIVATE samp_portable)
add_test(NAME encoding
  COMMAND test_encoding ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/cp1251.txt)
