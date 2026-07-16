add_library(lobster_replay
  replay.cpp
)
target_compile_features(lobster_replay PUBLIC cxx_std_26)
target_include_directories(lobster_replay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lobster_replay PUBLIC book tools_lobster_parser)

add_executable(bench_big_o_add
  bench_big_o_add.cpp
)
target_link_libraries(bench_big_o_add PRIVATE lobster_replay)

add_executable(lobster_validate
  lobster_validate.cpp
)
target_link_libraries(lobster_validate PRIVATE lobster_replay)

# Replays the LOBSTER sample datasets through the book and checks every
# message against the recorded orderbook. Off by default: the datasets live
# in Git LFS and each run takes minutes, so only the LOBSTER Validation
# workflow (and anyone opting in locally) registers them.
option(LOBSTER_VALIDATION "Register LOBSTER dataset validation tests" OFF)
if(LOBSTER_VALIDATION)
  foreach(symbol IN ITEMS AAPL MSFT)
    add_test(
      NAME lobster_validate_${symbol}
      COMMAND
        lobster_validate
        ${CMAKE_SOURCE_DIR}/data/lobster/${symbol}_2012-06-21_34200000_57600000_message_10.csv
        ${CMAKE_SOURCE_DIR}/data/lobster/${symbol}_2012-06-21_34200000_57600000_orderbook_10.csv)
    set_tests_properties(lobster_validate_${symbol} PROPERTIES LABELS lobster
                                                               TIMEOUT 1800)
  endforeach()
endif()

set(QUANTCUP ${CMAKE_SOURCE_DIR}/external/quantcup)

add_executable(quantcup_test
  ${QUANTCUP}/test.c
  quantcup_engine.cpp
)
target_compile_features(quantcup_test PRIVATE cxx_std_26)
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
target_compile_features(quantcup_score PRIVATE cxx_std_26)
target_include_directories(quantcup_score PRIVATE ${QUANTCUP})
target_link_libraries(quantcup_score PRIVATE book m rt)
# score.c uses sched_getaffinity without including <sched.h>; force-include it.
target_compile_options(quantcup_score PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-include sched.h -D_GNU_SOURCE -Wno-incompatible-pointer-types>)
