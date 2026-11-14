#pragma once

#include <book/books/node/iterator.hpp>
#include <book/books/node/pricelevels.hpp>
#include <book/concepts.hpp>
#include <book/order.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>

namespace ome::book::books::node {

class Book {
  public:
    using iterator = Iterator<Book>;
    using level_iterator = LevelIterator<Book>;

    /// \param max_orders unused: orders are allocated as they are added.
    explicit Book(std::size_t /*max_orders*/ = std::numeric_limits<std::uint8_t>::max()) {}

    template <Side> [[nodiscard]] auto begin() const -> iterator;
    template <Side> auto begin() -> iterator;
    template <Side> [[nodiscard]] auto end() const -> iterator;
    template <Side> auto end() -> iterator;

    template <Side> [[nodiscard]] auto levels() const -> std::ranges::subrange<level_iterator>;
    template <Side> auto levels() -> std::ranges::subrange<level_iterator>;

    template <Side> auto emplace(RestingOrder) -> iterator;
    auto erase(iterator) -> iterator;

    [[nodiscard]] auto find(OrderId) const -> iterator;
    template <Side> [[nodiscard]] auto find(Price) const -> level_iterator;

  private:
    PriceLevels maps_;
};

static_assert(concepts::OrderBook<Book>);

} // namespace ome::book::books::node
