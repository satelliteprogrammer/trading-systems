cmake_minimum_required(VERSION 3.21...4.1)
project(
  OrderMatchingEngine
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build the testing tree." ON)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  include(CTest)
  find_package(Catch2 3 REQUIRED)
  include(Catch)
endif()

add_subdirectory(src/book)
