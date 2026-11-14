#pragma once

#include <book/engine.hpp>
#include <testing/books.hpp>

#include <meta/tuple.hpp>

#include <string_view>

// Each benchmark executable runs a single book, set by ts_add_book_benchmark (cmake/Books.cmake).
#if !defined(BENCHMARK_BOOK) || !defined(BENCHMARK_BOOK_NAME)
#error "BENCHMARK_BOOK and BENCHMARK_BOOK_NAME must name the book to build against"
#endif

namespace ome::testing {

using Book = BENCHMARK_BOOK;
using Engine = book::BasicEngine<Book>;

static_assert(meta::tuple_contains<Book, Books>, "benchmarked books must be in testing::Books");

inline constexpr std::string_view book_name = BENCHMARK_BOOK_NAME;

} // namespace ome::testing
