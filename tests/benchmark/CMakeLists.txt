add_library(lobster_replay
  replay.cpp
)
target_compile_features(lobster_replay PUBLIC cxx_std_26)
ts_enable_warnings(lobster_replay)
target_include_directories(lobster_replay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lobster_replay PUBLIC book tools_lobster_parser)

add_executable(bench_big_o_add
  bench_big_o_add.cpp
)
target_link_libraries(bench_big_o_add PRIVATE lobster_replay)
ts_enable_warnings(bench_big_o_add)

add_executable(lobster_validate
  lobster_validate.cpp
)
target_link_libraries(lobster_validate PRIVATE lobster_replay)
ts_enable_warnings(lobster_validate)

# Replays the LOBSTER sample datasets through the book and checks every message against the recorded
# orderbook. Off by default: the datasets live in Git LFS and each run takes minutes, so only the
# LOBSTER Validation workflow (and anyone opting in locally) registers them.
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

add_executable(quantcup_test quantcup_engine.cpp)
target_compile_features(quantcup_test PRIVATE cxx_std_26)
target_link_libraries(quantcup_test PRIVATE book quantcup quantcup_test_driver)
ts_enable_warnings(quantcup_test)

# The autotester always exits 0, so pass only when it reports every test correct.
add_test(NAME quantcup_test COMMAND quantcup_test)
set_tests_properties(quantcup_test PROPERTIES PASS_REGULAR_EXPRESSION
                                              "You got 14/14 tests correct")

if(TARGET quantcup_score_driver)
  add_executable(quantcup_score quantcup_engine.cpp)
  target_compile_features(quantcup_score PRIVATE cxx_std_26)
  target_link_libraries(quantcup_score PRIVATE book quantcup quantcup_score_driver m rt)
  ts_enable_warnings(quantcup_score)
endif()
