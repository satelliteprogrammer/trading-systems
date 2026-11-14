# Every book the engine is instantiated for (src/book/src/engine.cpp): TS_BOOKS holds their names,
# TS_BOOK_<name> their types.
set(TS_BOOKS node-book node-indexedbook array-indexedbook)
set(TS_BOOK_node-book ome::book::books::node::Book)
set(TS_BOOK_node-indexedbook ome::book::books::node::IndexedBook)
set(TS_BOOK_array-indexedbook ome::book::books::array::IndexedBook)

# ts_add_book_benchmark(<name> SOURCES <source>... [LIBRARIES <library>...])
#
# Adds an executable <name>_<book> for every book in TS_BOOKS, built with BENCHMARK_BOOK defined to
# the book's type and BENCHMARK_BOOK_NAME to its name.
function(ts_add_book_benchmark name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
  foreach(book IN LISTS TS_BOOKS)
    set(target ${name}_${book})
    add_executable(${target} ${arg_SOURCES})
    target_compile_definitions(${target} PRIVATE BENCHMARK_BOOK=${TS_BOOK_${book}}
                                                 BENCHMARK_BOOK_NAME="${book}")
    target_link_libraries(${target} PRIVATE meta tools_testing ${arg_LIBRARIES})
    ts_enable_warnings(${target})
  endforeach()
endfunction()
