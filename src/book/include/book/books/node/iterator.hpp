#pragma once

#include <book/books/node/pricelevels.hpp>
#include <book/order.hpp>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <list>
#include <map>
#include <optional>

namespace ome::book::books::node {

template <typename NodeTree> class Iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;

    using difference_type = std::ptrdiff_t;
    using value_type = RestingOrder;

    constexpr Iterator() = default;

    constexpr auto operator*() const -> value_type & { return *inner_it_; }
    constexpr auto operator->() const -> value_type * { return &*inner_it_; }

    constexpr auto operator++() -> Iterator & {
        ++inner_it_;
        satisfy();
        return *this;
    }
    constexpr auto operator++(int) -> Iterator {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr auto operator==(Iterator const &it) const -> bool {
        return this->inner_it_ == it.inner_it_;
    }

  private:
    friend NodeTree;

    // \warning relies on SCARY iterators (N2982)
    using map_iterator = std::map<Price, Level>::iterator;
    using list_iterator = std::list<RestingOrder>::iterator;

    constexpr Iterator(map_iterator outer_it, map_iterator end, list_iterator inner_it)
        : outer_it_{outer_it}, outer_end_{end}, inner_it_{inner_it} {}
    constexpr Iterator(map_iterator outer_it, map_iterator end)
        : outer_it_{outer_it}, outer_end_{end} {
        if (outer_it != end) {
            assert(!outer_it->second.orders.empty());
            inner_it_ = outer_it->second.orders.begin();
        }
    }
    // NOLINTNEXTLINE(misc-explicit-constructor)
    constexpr Iterator(map_iterator end) : outer_it_{end}, outer_end_{end} {}

    constexpr void satisfy(std::optional<map_iterator> next_outer_it = std::nullopt) {
        if (next_outer_it) {
            outer_it_ = *next_outer_it;
        } else {
            if (inner_it_ != outer_it_->second.orders.end()) {
                return;
            }
            ++outer_it_;
        }

        if (outer_it_ != outer_end_) {
            assert(!outer_it_->second.orders.empty());
            inner_it_ = outer_it_->second.orders.begin();
        } else {
            inner_it_ = {};
        }
    }

    map_iterator outer_it_;
    map_iterator outer_end_;
    list_iterator inner_it_;
};

} // namespace ome::book::books::node
