add_library(book STATIC src/lob.cpp)
target_compile_features(book PUBLIC cxx_std_26)
target_include_directories(
  book PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
              $<INSTALL_INTERFACE:include>)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)

  add_executable(tests_api src/lob.test.cpp)
  target_compile_features(tests_api PUBLIC cxx_std_26)
  target_link_libraries(tests_api PRIVATE Catch2::Catch2WithMain book)
  catch_discover_tests(tests_api)

endif()
