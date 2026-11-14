#pragma once

#include <book/books/array/iterator.hpp>
#include <book/books/array/pricelevels.hpp>
#include <book/concepts.hpp>
#include <book/order.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <unordered_map>

namespace ome::book::books::array {

class IndexedBook {
  public:
    using iterator = Iterator<IndexedBook>;
    using level_iterator = LevelIterator<IndexedBook>;

    /// \param max_orders number of order slots: order ids must be below it.
    explicit IndexedBook(std::size_t max_orders = std::numeric_limits<std::uint8_t>::max());

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
    template <Side> constexpr auto by_price_map() const -> auto const &;
    template <Side> constexpr auto by_price_map() -> auto &;

    PriceLevels levels_;

    std::unordered_map<Price, iterator::map_iterator> by_bid_price_;
    std::unordered_map<Price, iterator::map_iterator> by_ask_price_;
};

static_assert(concepts::OrderBook<IndexedBook>);

} // namespace ome::book::books::array
