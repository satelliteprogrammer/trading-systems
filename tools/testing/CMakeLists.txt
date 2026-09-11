# test-only helpers shared by the unit tests, see include/testing/
add_library(tools_testing INTERFACE)

target_compile_features(tools_testing INTERFACE cxx_std_23)
target_include_directories(
  tools_testing
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(tools_testing INTERFACE Catch2::Catch2)
