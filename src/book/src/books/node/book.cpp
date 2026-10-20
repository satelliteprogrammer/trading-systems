#include "book/books/node/book.hpp"
#include "book/order.hpp"

#include <algorithm>
#include <cassert>

namespace ome::book::books::node {

BookIterator::BookIterator(map_iterator outer_it, map_iterator end, list_iterator inner_it)
    : outer_it_{outer_it}, outer_end_{end}, inner_it_{inner_it} {}

BookIterator::BookIterator(map_iterator outer_it, map_iterator end)
    : outer_it_{outer_it}, outer_end_{end} {
    if (outer_it != end) {
        assert(!outer_it->second.orders.empty());
        inner_it_ = outer_it->second.orders.begin();
    }
}

BookIterator::BookIterator(map_iterator end) : outer_it_{end}, outer_end_{end} {}

auto BookIterator::operator*() const -> value_type & { return *inner_it_; }

auto BookIterator::operator->() const -> value_type * { return &*inner_it_; }

auto BookIterator::operator++() -> BookIterator & {
    ++inner_it_;
    satisfy();
    return *this;
}

auto BookIterator::operator++(int) -> BookIterator {
    auto tmp = *this;
    ++(*this);
    return tmp;
}

auto BookIterator::operator==(BookIterator const &it) const -> bool {
    return this->inner_it_ == it.inner_it_;
}

auto BookIterator::satisfy(std::optional<map_iterator> next_outer_it) -> void {
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

template <Side S> auto Book::begin() const -> BookIterator {
    return const_cast<Book *>(this)->begin<S>();
}

template <Side S> auto Book::begin() -> BookIterator { return {map<S>().begin(), map<S>().end()}; }

template <Side S> auto Book::end() const -> BookIterator {
    return const_cast<Book *>(this)->end<S>();
}

template <Side S> auto Book::end() -> BookIterator { return {map<S>().end()}; }

template <Side S> auto Book::emplace(RestingOrder order) -> BookIterator {
    auto [it, _] = map<S>().try_emplace(order.price);
    it->second.orders.emplace_back(order);
    return {it, map<S>().end(), std::prev(it->second.orders.end())};
}

auto Book::erase(BookIterator it) -> BookIterator {
    auto side = it->side;

    it.inner_it_ = it.outer_it_->second.orders.erase(it.inner_it_);

    if (it.outer_it_->second.orders.empty()) {
        switch (side) {
        case Side::Buy:
            it.satisfy(bids_.erase(it.outer_it_));
            break;
        case Side::Sell:
            it.satisfy(asks_.erase(it.outer_it_));
        }
    } else {
        it.satisfy();
    }

    return it;
}

auto Book::find(OrderId id) -> BookIterator {
    auto is_order = [id](auto const &order) { return order.order_id == id; };

    if (auto itf = std::ranges::find_if(begin<Side::Buy>(), end<Side::Buy>(), is_order);
        itf != end<Side::Buy>()) {
        return itf;
    }

    if (auto itf = std::ranges::find_if(begin<Side::Sell>(), end<Side::Sell>(), is_order);
        itf != end<Side::Sell>()) {
        return itf;
    }

    return end<Side::Buy>();
}

template <Side S> constexpr auto Book::map() const -> auto const & {
    return const_cast<Book *>(this)->map<S>();
}

template <Side S> constexpr auto Book::map() -> auto & {
    if constexpr (S == Side::Buy) {
        return bids_;
    } else if constexpr (S == Side::Sell) {
        return asks_;
    } else {
        static_assert(false, "invalid side");
    }
}

template auto Book::begin<Side::Buy>() const -> BookIterator;
template auto Book::begin<Side::Buy>() -> BookIterator;
template auto Book::begin<Side::Sell>() const -> BookIterator;
template auto Book::begin<Side::Sell>() -> BookIterator;
template auto Book::end<Side::Buy>() const -> BookIterator;
template auto Book::end<Side::Buy>() -> BookIterator;
template auto Book::end<Side::Sell>() const -> BookIterator;
template auto Book::end<Side::Sell>() -> BookIterator;

template auto Book::emplace<Side::Buy>(RestingOrder) -> BookIterator;
template auto Book::emplace<Side::Sell>(RestingOrder) -> BookIterator;

} // namespace ome::book::books::node
