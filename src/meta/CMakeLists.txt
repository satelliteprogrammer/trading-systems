# header-only compile-time helpers, see include/meta/
add_library(meta INTERFACE)
target_compile_features(meta INTERFACE cxx_std_20)
target_include_directories(
  meta INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                 $<INSTALL_INTERFACE:include>)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)

  add_executable(
    tests_meta

    src/tuple.test.cpp
  )
  ts_enable_warnings(tests_meta)
  target_link_libraries(tests_meta PRIVATE Catch2::Catch2WithMain meta)
  catch_discover_tests(tests_meta)

endif()
