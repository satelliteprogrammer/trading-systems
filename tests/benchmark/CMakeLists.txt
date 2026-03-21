add_executable(bench_big_o_add
  bench_big_o_add.cpp
)

target_compile_features(bench_big_o_add PUBLIC cxx_std_23)
target_link_libraries(bench_big_o_add PRIVATE book tools_lobster_parser)
