add_library(tools_lobster_parser
  algorithm.cpp
  parser.cpp
  reconstructor.cpp
)

target_compile_features(tools_lobster_parser PUBLIC cxx_std_26)
target_include_directories(
  tools_lobster_parser
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)
target_link_libraries(tools_lobster_parser PRIVATE book)
