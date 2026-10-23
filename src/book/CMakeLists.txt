add_library(
  book
  STATIC
    src/books/node/book.cpp
    src/books/node/indexedbook.cpp
    src/engine.cpp
)
target_compile_features(book PUBLIC cxx_std_26)
ts_enable_warnings(book)
target_include_directories(
  book PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
              $<INSTALL_INTERFACE:include>)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)

  add_executable(
    tests_api

    src/books/books.test.cpp
    src/engine.test.cpp
  )
  target_compile_features(tests_api PUBLIC cxx_std_26)
  ts_enable_warnings(tests_api)
  target_link_libraries(tests_api PRIVATE Catch2::Catch2WithMain book tools_testing)
  catch_discover_tests(tests_api)

endif()
