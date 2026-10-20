#pragma once

#include "../../concepts.hpp"
#include "../../order.hpp"

#include <cstddef>
#include <list>
#include <map>
#include <optional>

namespace ome::book::books::node {

struct Level {
    std::list<RestingOrder> orders;
};

class Book;

class BookIterator {
  public:
    using iterator_concept = std::forward_iterator_tag;

    using difference_type = std::ptrdiff_t;
    using value_type = RestingOrder;

    BookIterator() = default;

    auto operator*() const -> value_type &;
    auto operator->() const -> value_type *;

    auto operator++() -> BookIterator &;
    auto operator++(int) -> BookIterator;

    auto operator==(BookIterator const &) const -> bool;

  private:
    friend class Book;

    // TODO: map_iterator is only technically correct for the the std::less map, however map
    // implementations under libc++ and libstdc++ use the same iterator type.
    // This can be fixed by having a custom iterator that selects the side from its construtor, but
    // it potentially adds an extra branch for every comparison.
    // Test if this has an impact, or if the compiler can always chose the correct branch.
    using map_iterator = std::map<Price, Level>::iterator;
    using list_iterator = decltype(Level::orders)::iterator;

    BookIterator(map_iterator, map_iterator, list_iterator);
    BookIterator(map_iterator, map_iterator);
    BookIterator(map_iterator);

    void satisfy(std::optional<map_iterator> next_outer_it = std::nullopt);

    map_iterator outer_it_;
    map_iterator outer_end_;
    list_iterator inner_it_;
};

static_assert(concepts::OrderBookIterator<BookIterator>);

class Book {
  public:
    using iterator = BookIterator;

    template <Side> [[nodiscard]] auto begin() const -> BookIterator;
    template <Side> auto begin() -> BookIterator;
    template <Side> [[nodiscard]] auto end() const -> BookIterator;
    template <Side> auto end() -> BookIterator;

    template <Side> auto emplace(RestingOrder) -> BookIterator;
    auto erase(BookIterator) -> BookIterator;

    auto find(OrderId) -> BookIterator;

  private:
    template <Side> constexpr auto map() const -> auto const &;
    template <Side> constexpr auto map() -> auto &;

    std::map<Price, Level, std::greater<>> bids_;
    std::map<Price, Level, std::less<>> asks_;
};

static_assert(concepts::OrderBook<Book>);

} // namespace ome::book::books::node
