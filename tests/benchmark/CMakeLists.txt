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

# Names must match books::name<Book> (book/books/all.hpp).
set(QUANTCUP_BOOKS
    "node-book=ome::book::books::node::Book"
    "node-indexedbook=ome::book::books::node::IndexedBook"
    "array-indexedbook=ome::book::books::array::IndexedBook")

foreach(entry IN LISTS QUANTCUP_BOOKS)
  string(REPLACE "=" ";" entry "${entry}")
  list(GET entry 0 name)
  list(GET entry 1 type)

  add_executable(quantcup_test_${name} quantcup_engine.cpp)
  target_compile_features(quantcup_test_${name} PRIVATE cxx_std_26)
  target_compile_definitions(quantcup_test_${name} PRIVATE QUANTCUP_BOOK=${type})
  target_link_libraries(quantcup_test_${name} PRIVATE book quantcup quantcup_test_driver)
  ts_enable_warnings(quantcup_test_${name})

  # The autotester always exits 0, so pass only when it reports every test correct.
  add_test(NAME quantcup_test_${name} COMMAND quantcup_test_${name})
  set_tests_properties(quantcup_test_${name} PROPERTIES PASS_REGULAR_EXPRESSION
                                                        "You got 14/14 tests correct")

  if(TARGET quantcup_score_driver)
    add_executable(quantcup_score_${name} quantcup_engine.cpp)
    target_compile_features(quantcup_score_${name} PRIVATE cxx_std_26)
    target_compile_definitions(quantcup_score_${name} PRIVATE QUANTCUP_BOOK=${type})
    target_link_libraries(quantcup_score_${name} PRIVATE book quantcup quantcup_score_driver m rt)
    ts_enable_warnings(quantcup_score_${name})
  endif()
endforeach()
