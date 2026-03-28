add_executable(bench_big_o_add
  bench_big_o_add.cpp
)

target_compile_features(bench_big_o_add PUBLIC cxx_std_23)
target_link_libraries(bench_big_o_add PRIVATE book tools_lobster_parser)

set(QUANTCUP ${CMAKE_SOURCE_DIR}/external/quantcup)

add_executable(quantcup_test
  ${QUANTCUP}/test.c
  quantcup_engine.cpp
)
target_compile_features(quantcup_test PRIVATE cxx_std_23)
target_include_directories(quantcup_test PRIVATE ${QUANTCUP})
target_link_libraries(quantcup_test PRIVATE book)
# test.c calls test()/test_cancel() before they are declared; silence the
# C99 implicit-function-declaration error that Clang 21 raises by default.
target_compile_options(quantcup_test PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>)

add_executable(quantcup_score
  ${QUANTCUP}/score.c
  quantcup_engine.cpp
)
target_compile_features(quantcup_score PRIVATE cxx_std_23)
target_include_directories(quantcup_score PRIVATE ${QUANTCUP})
target_link_libraries(quantcup_score PRIVATE book m rt)
# score.c uses sched_getaffinity without including <sched.h>; force-include it.
target_compile_options(quantcup_score PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-include sched.h -D_GNU_SOURCE>)
