#pragma once

#include <book/books/array/indexedbook.hpp>
#include <book/books/node/book.hpp>
#include <book/books/node/indexedbook.hpp>

#include <string_view>
#include <tuple>

namespace ome::book::books {

// A book added here must also be explicitly instantiated in engine.cpp.
using All = std::tuple<node::Book, node::IndexedBook, array::IndexedBook>;

/// Short, filename-safe name of a book.
template <typename Book>
inline constexpr std::string_view name = [] -> std::string_view {
    static_assert(sizeof(Book) == 0, "add a name<Book> specialization for this book");
    return {};
}();
template <> inline constexpr std::string_view name<node::Book> = "node-book";
template <> inline constexpr std::string_view name<node::IndexedBook> = "node-indexedbook";
template <> inline constexpr std::string_view name<array::IndexedBook> = "array-indexedbook";

} // namespace ome::book::books
