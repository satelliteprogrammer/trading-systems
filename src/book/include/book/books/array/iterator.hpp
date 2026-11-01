#pragma once

#include <book/books/array/pricelevels.hpp>
#include <book/order.hpp>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <map>
#include <optional>

namespace ome::book::books::array {

template <typename Arena> class LevelIterator;

template <typename Arena> class Iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;

    using difference_type = std::ptrdiff_t;
    using value_type = RestingOrder;

    constexpr Iterator() = default;

    constexpr auto operator*() const -> value_type & { return *order_it_; }
    constexpr auto operator->() const -> value_type * { return &*order_it_; }

    constexpr auto operator++() -> Iterator & {
        ++order_it_;
        satisfy();
        return *this;
    }
    constexpr auto operator++(int) -> Iterator {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr auto operator==(Iterator const &it) const -> bool {
        return this->order_it_ == it.order_it_;
    }

  private:
    friend Arena;
    friend LevelIterator<Arena>;

    // \warning relies on SCARY iterators (N2982)
    using map_iterator = std::map<Price, detail::Level>::iterator;
    using list_iterator = decltype(detail::Level::orders)::iterator;

    constexpr Iterator(map_iterator level_it, map_iterator end, list_iterator const &order_it)
        : level_it_{level_it}, level_end_{end}, order_it_{order_it} {}
    constexpr Iterator(map_iterator level_it, map_iterator end)
        : level_it_{level_it}, level_end_{end} {
        if (level_it != end) {
            assert(!level_it->second.orders.empty());
            order_it_ = level_it->second.orders.begin();
        }
    }
    // NOLINTNEXTLINE(misc-explicit-constructor)
    constexpr Iterator(map_iterator end) : level_it_{end}, level_end_{end} {}

    constexpr void satisfy(std::optional<map_iterator> next_level_it = std::nullopt) {
        if (next_level_it) {
            level_it_ = *next_level_it;
        } else {
            if (order_it_ != level_it_->second.orders.end()) {
                return;
            }
            ++level_it_;
        }

        if (level_it_ != level_end_) {
            assert(!level_it_->second.orders.empty());
            order_it_ = level_it_->second.orders.begin();
        } else {
            order_it_ = {};
        }
    }

    map_iterator level_it_;
    map_iterator level_end_;
    list_iterator order_it_;
};

template <typename Arena> class LevelIterator {
  public:
    using iterator_concept = std::forward_iterator_tag;

    using difference_type = std::ptrdiff_t;
    using value_type = detail::Level;

    constexpr LevelIterator() = default;
    constexpr explicit LevelIterator(Iterator<Arena> it) : level_it_{it.level_it_} {}

    constexpr auto operator*() const -> value_type & { return level_it_->second; }
    constexpr auto operator->() const -> value_type * { return &level_it_->second; }

    constexpr auto operator++() -> LevelIterator & {
        ++level_it_;
        return *this;
    }
    constexpr auto operator++(int) -> LevelIterator {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr auto operator==(LevelIterator const &it) const -> bool = default;

  private:
    friend Arena;

    // \warning relies on SCARY iterators (N2982)
    using map_iterator = std::map<Price, detail::Level>::iterator;

    // NOLINTNEXTLINE(misc-explicit-constructor)
    constexpr LevelIterator(map_iterator level_it) : level_it_{level_it} {}

    map_iterator level_it_;
};

} // namespace ome::book::books::array
