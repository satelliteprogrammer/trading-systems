#pragma once

#include <book/books/node/iterator.hpp>
#include <book/books/node/pricelevels.hpp>
#include <book/concepts.hpp>
#include <book/order.hpp>

namespace ome::book::books::node {

class Book {
  public:
    using iterator = Iterator<Book>;

    template <Side> [[nodiscard]] auto begin() const -> iterator;
    template <Side> auto begin() -> iterator;
    template <Side> [[nodiscard]] auto end() const -> iterator;
    template <Side> auto end() -> iterator;

    template <Side> auto emplace(RestingOrder) -> iterator;
    auto erase(iterator) -> iterator;

    auto find(OrderId) -> iterator;

  private:
    PriceLevels maps_;
};

static_assert(concepts::OrderBookIterator<Book::iterator>);
static_assert(concepts::OrderBook<Book>);

} // namespace ome::book::books::node
