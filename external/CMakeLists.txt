# Headers are included as "quantcup/<header>", so quantcup's own limits.h can't shadow the system
# <limits.h>. Third-party code: SYSTEM keeps our warnings out of its headers, and its C drivers
# build as their own object libraries so that warnings-as-errors only applies to our adapter.
add_library(quantcup INTERFACE)
target_include_directories(quantcup SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(quantcup_test_driver OBJECT quantcup/test.c)
target_link_libraries(quantcup_test_driver PRIVATE quantcup)
# test.c calls test()/test_cancel() before they are declared; silence the C99
# implicit-function-declaration error that Clang 21 raises by default.
target_compile_options(quantcup_test_driver PRIVATE -Wno-implicit-function-declaration)

# Scores are only comparable on the reference machine (see docs/benchmarks.md), and score.c relies
# on Linux's sched_getaffinity and librt.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_library(quantcup_score_driver OBJECT quantcup/score.c)
  target_link_libraries(quantcup_score_driver PRIVATE quantcup)
  # score.c uses sched_getaffinity without including <sched.h>; force-include it.
  target_compile_options(quantcup_score_driver PRIVATE
    -include sched.h -D_GNU_SOURCE -Wno-incompatible-pointer-types)
endif()
