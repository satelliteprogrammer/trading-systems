cmake_minimum_required(VERSION 3.24...4.1)
project(
  OrderMatchingEngine
  VERSION 0.1.0
  LANGUAGES CXX C)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CompilerWarnings)

option(BUILD_TESTING "Build the testing tree." ON)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  include(CTest)
  find_package(Catch2 3.8 REQUIRED)
  include(Catch)
endif()

add_subdirectory(src/book)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  add_subdirectory(external)
  add_subdirectory(tools/testing)
  add_subdirectory(tools/lobster)
  add_subdirectory(tests/benchmark)
endif()
