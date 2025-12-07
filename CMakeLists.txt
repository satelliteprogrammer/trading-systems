cmake_minimum_required(VERSION 3.10)
project(
  OrderMatchingEngine
  VERSION 0.1.0
  LANGUAGES CXX)

add_library(Engine STATIC src/engine/engine.cpp)

find_package(Catch2 3 REQUIRED)
add_executable(tests src/engine/engine.test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain Engine)

include(CTest)
include(Catch)
catch_discover_tests(tests)
