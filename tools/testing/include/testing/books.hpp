#pragma once

#include <book/books/array/indexedbook.hpp>
#include <book/books/node/book.hpp>
#include <book/books/node/indexedbook.hpp>

#include <tuple>

namespace ome::testing {

/// Every book the engine is instantiated for (engine.cpp).
using Books = std::tuple<book::books::node::Book, book::books::node::IndexedBook,
                         book::books::array::IndexedBook>;

} // namespace ome::testing
