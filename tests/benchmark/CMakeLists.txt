ts_add_book_benchmark(bench_big_o_add SOURCES bench_big_o_add.cpp replay.cpp
                      LIBRARIES tools_lobster_parser)
ts_add_book_benchmark(lobster_validate SOURCES lobster_validate.cpp replay.cpp
                      LIBRARIES tools_lobster_parser)
ts_add_book_benchmark(quantcup_test SOURCES quantcup_engine.cpp
                      LIBRARIES quantcup quantcup_test_driver)
if(TARGET quantcup_score_driver)
  ts_add_book_benchmark(quantcup_score SOURCES quantcup_engine.cpp
                        LIBRARIES quantcup quantcup_score_driver m rt)
endif()

foreach(book IN LISTS TS_BOOKS)
  # The autotester always exits 0, so pass only when it reports every test correct.
  add_test(NAME quantcup_test_${book} COMMAND quantcup_test_${book})
  set_tests_properties(quantcup_test_${book} PROPERTIES PASS_REGULAR_EXPRESSION
                                                        "You got 14/14 tests correct")
endforeach()

# Replays the LOBSTER sample datasets through the production book (behind ome::book::Engine) and
# checks every message against the recorded orderbook. Off by default: the datasets live in Git LFS
# and each run takes minutes, so only the LOBSTER Validation workflow (and anyone opting in
# locally) registers them.
option(LOBSTER_VALIDATION "Register LOBSTER dataset validation tests" OFF)
if(LOBSTER_VALIDATION)
  set(validator lobster_validate_node-indexedbook)
  add_custom_target(lobster_validate DEPENDS ${validator})
  foreach(symbol IN ITEMS AAPL MSFT)
    add_test(
      NAME lobster_validate_${symbol}
      COMMAND
        ${validator}
        ${CMAKE_SOURCE_DIR}/data/lobster/${symbol}_2012-06-21_34200000_57600000_message_10.csv
        ${CMAKE_SOURCE_DIR}/data/lobster/${symbol}_2012-06-21_34200000_57600000_orderbook_10.csv)
    set_tests_properties(lobster_validate_${symbol} PROPERTIES LABELS lobster
                                                               TIMEOUT 1800)
  endforeach()
endif()
