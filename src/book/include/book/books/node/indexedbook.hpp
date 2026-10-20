#pragma once

#include <book/books/node/book.hpp>
#include <book/concepts.hpp>
#include <book/order.hpp>

#include <cstddef>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>

namespace ome::book::books::node {

class IndexedBook;

class IndexedBookIterator {
  public:
    using iterator_concept = std::forward_iterator_tag;

    using difference_type = std::ptrdiff_t;
    using value_type = RestingOrder;

    IndexedBookIterator() = default;

    auto operator*() const -> value_type &;
    auto operator->() const -> value_type *;

    auto operator++() -> IndexedBookIterator &;
    auto operator++(int) -> IndexedBookIterator;

    auto operator==(IndexedBookIterator const &) const -> bool;

  private:
    friend class IndexedBook;

    // TODO: map_iterator is only technically correct for the the std::less map, however map
    // implementations under libc++ and libstdc++ use the same iterator type.
    // This can be fixed by having a custom iterator that selects the side from its construtor, but
    // it potentially adds an extra branch for every comparison.
    // Test if this has an impact, or if the compiler can always chose the correct branch.
    using map_iterator = std::map<Price, Level>::iterator;
    using list_iterator = decltype(Level::orders)::iterator;

    IndexedBookIterator(map_iterator, map_iterator, list_iterator);
    IndexedBookIterator(map_iterator, map_iterator);
    IndexedBookIterator(map_iterator);

    void satisfy(std::optional<map_iterator> next_outer_it = std::nullopt);

    map_iterator outer_it_;
    map_iterator outer_end_;
    list_iterator inner_it_;
};

static_assert(concepts::OrderBookIterator<IndexedBookIterator>);

class IndexedBook {
  public:
    using iterator = IndexedBookIterator;

    template <Side> [[nodiscard]] auto begin() const -> IndexedBookIterator;
    template <Side> auto begin() -> IndexedBookIterator;
    template <Side> [[nodiscard]] auto end() const -> IndexedBookIterator;
    template <Side> auto end() -> IndexedBookIterator;

    template <Side> auto emplace(RestingOrder) -> IndexedBookIterator;
    auto erase(IndexedBookIterator) -> IndexedBookIterator;

    auto find(OrderId) -> IndexedBookIterator;

  private:
    template <Side> constexpr auto map() const -> auto const &;
    template <Side> constexpr auto map() -> auto &;

    template <Side> constexpr auto by_price_map() const -> auto const &;
    template <Side> constexpr auto by_price_map() -> auto &;

    std::map<Price, Level, std::greater<>> bids_;
    std::map<Price, Level, std::less<>> asks_;

    std::unordered_map<OrderId, std::list<RestingOrder>::iterator> by_order_id_;
    std::unordered_map<Price, decltype(bids_)::iterator> by_bid_price_;
    std::unordered_map<Price, decltype(asks_)::iterator> by_ask_price_;
};

static_assert(concepts::OrderBook<IndexedBook>);

} // namespace ome::book::books::node
