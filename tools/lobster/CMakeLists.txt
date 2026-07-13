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

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)

  add_executable(tests_lobster_reconstructor
    algorithm.test.cpp
    reconstructor.test.cpp
  )
  target_compile_features(tests_lobster_reconstructor PUBLIC cxx_std_26)
  target_link_libraries(tests_lobster_reconstructor PRIVATE Catch2::Catch2WithMain
                                                            tools_lobster_parser)
  catch_discover_tests(tests_lobster_reconstructor)

endif()
