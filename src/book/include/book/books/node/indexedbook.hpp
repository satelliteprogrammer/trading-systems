#pragma once

#include <book/books/node/iterator.hpp>
#include <book/books/node/pricelevels.hpp>
#include <book/concepts.hpp>
#include <book/order.hpp>

#include <unordered_map>

namespace ome::book::books::node {

class IndexedBook {
  public:
    using iterator = Iterator<IndexedBook>;

    template <Side> [[nodiscard]] auto begin() const -> iterator;
    template <Side> auto begin() -> iterator;
    template <Side> [[nodiscard]] auto end() const -> iterator;
    template <Side> auto end() -> iterator;

    template <Side> auto emplace(RestingOrder) -> iterator;
    auto erase(iterator) -> iterator;

    auto find(OrderId) -> iterator;

  private:
    template <Side> constexpr auto by_price_map() const -> auto const &;
    template <Side> constexpr auto by_price_map() -> auto &;

    PriceLevels maps_;

    std::unordered_map<OrderId, iterator::list_iterator> by_order_id_;
    std::unordered_map<Price, iterator::map_iterator> by_bid_price_;
    std::unordered_map<Price, iterator::map_iterator> by_ask_price_;
};

static_assert(concepts::OrderBookIterator<IndexedBook::iterator>);
static_assert(concepts::OrderBook<IndexedBook>);

} // namespace ome::book::books::node
