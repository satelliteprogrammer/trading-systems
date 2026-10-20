#include "book/books/node/indexedbook.hpp"

#include <cassert>

namespace ome::book::books::node {

IndexedBookIterator::IndexedBookIterator(map_iterator outer_it, map_iterator end,
                                         list_iterator inner_it)
    : outer_it_{outer_it}, outer_end_{end}, inner_it_{inner_it} {}

IndexedBookIterator::IndexedBookIterator(map_iterator outer_it, map_iterator end)
    : outer_it_{outer_it}, outer_end_{end} {
    if (outer_it != end) {
        assert(!outer_it->second.orders.empty());
        inner_it_ = outer_it->second.orders.begin();
    }
}

IndexedBookIterator::IndexedBookIterator(map_iterator end) : outer_it_{end}, outer_end_{end} {}

auto IndexedBookIterator::operator*() const -> value_type & { return *inner_it_; }

auto IndexedBookIterator::operator->() const -> value_type * { return &*inner_it_; }

auto IndexedBookIterator::operator++() -> IndexedBookIterator & {
    ++inner_it_;
    satisfy();
    return *this;
}

auto IndexedBookIterator::operator++(int) -> IndexedBookIterator {
    auto tmp = *this;
    ++(*this);
    return tmp;
}

auto IndexedBookIterator::operator==(IndexedBookIterator const &it) const -> bool {
    return this->inner_it_ == it.inner_it_;
}

auto IndexedBookIterator::satisfy(std::optional<map_iterator> next_outer_it) -> void {
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

template <Side S> constexpr auto IndexedBook::map() const -> auto const & {
    return const_cast<IndexedBook *>(this)->map<S>();
}

template <Side S> constexpr auto IndexedBook::map() -> auto & {
    if constexpr (S == Side::Buy) {
        return bids_;
    } else if constexpr (S == Side::Sell) {
        return asks_;
    } else {
        static_assert(false, "invalid side");
    }
}

template <Side S> constexpr auto IndexedBook::by_price_map() const -> auto const & {
    return const_cast<IndexedBook *>(this)->by_price_map<S>();
}

template <Side S> constexpr auto IndexedBook::by_price_map() -> auto & {
    if constexpr (S == Side::Buy) {
        return by_bid_price_;
    } else if constexpr (S == Side::Sell) {
        return by_ask_price_;
    } else {
        static_assert(false, "invalid side");
    }
}

template <Side S> auto IndexedBook::begin() const -> IndexedBookIterator {
    return const_cast<IndexedBook *>(this)->begin<S>();
}

template <Side S> auto IndexedBook::begin() -> IndexedBookIterator {
    return {map<S>().begin(), map<S>().end()};
}

template <Side S> auto IndexedBook::end() const -> IndexedBookIterator {
    return const_cast<IndexedBook *>(this)->end<S>();
}

template <Side S> auto IndexedBook::end() -> IndexedBookIterator { return {map<S>().end()}; }

template <Side S> auto IndexedBook::emplace(RestingOrder order) -> IndexedBookIterator {
    IndexedBookIterator::map_iterator it;
    if (auto itf = by_price_map<S>().find(order.price); itf != by_price_map<S>().end()) {
        it = itf->second;
    } else {
        bool inserted{};
        std::tie(it, inserted) = map<S>().try_emplace(order.price);
        assert(inserted);
        by_price_map<S>().try_emplace(order.price, it);
    }

    it->second.orders.emplace_back(order);

    auto order_it = std::prev(it->second.orders.end());
    by_order_id_.try_emplace(order.order_id, order_it);

    return {it, map<S>().end(), order_it};
}

auto IndexedBook::erase(IndexedBookIterator it) -> IndexedBookIterator {
    auto id = it->order_id;
    auto price = it->price;
    auto side = it->side;

    by_order_id_.erase(id);
    it.inner_it_ = it.outer_it_->second.orders.erase(it.inner_it_);

    if (it.outer_it_->second.orders.empty()) {
        switch (side) {
        case Side::Buy:
            by_price_map<Side::Buy>().erase(price);
            it.satisfy(map<Side::Buy>().erase(it.outer_it_));
            break;
        case Side::Sell:
            by_price_map<Side::Sell>().erase(price);
            it.satisfy(map<Side::Sell>().erase(it.outer_it_));
            break;
        }
    } else {
        it.satisfy();
    }

    return it;
}

auto IndexedBook::find(OrderId id) -> IndexedBookIterator {
    auto it = by_order_id_.find(id);
    if (it == by_order_id_.end()) {
        return end<Side::Buy>();
    }

    switch (it->second->side) {
    case Side::Buy:
        assert(by_price_map<Side::Buy>().contains(it->second->price));
        return {by_price_map<Side::Buy>()[it->second->price], map<Side::Buy>().end(), it->second};
    case Side::Sell:
        assert(by_price_map<Side::Sell>().contains(it->second->price));
        return {by_price_map<Side::Sell>()[it->second->price], map<Side::Sell>().end(), it->second};
    }
}

template auto IndexedBook::begin<Side::Buy>() const -> IndexedBookIterator;
template auto IndexedBook::begin<Side::Buy>() -> IndexedBookIterator;
template auto IndexedBook::begin<Side::Sell>() const -> IndexedBookIterator;
template auto IndexedBook::begin<Side::Sell>() -> IndexedBookIterator;
template auto IndexedBook::end<Side::Buy>() const -> IndexedBookIterator;
template auto IndexedBook::end<Side::Buy>() -> IndexedBookIterator;
template auto IndexedBook::end<Side::Sell>() const -> IndexedBookIterator;
template auto IndexedBook::end<Side::Sell>() -> IndexedBookIterator;

template auto IndexedBook::emplace<Side::Buy>(RestingOrder) -> IndexedBookIterator;
template auto IndexedBook::emplace<Side::Sell>(RestingOrder) -> IndexedBookIterator;

} // namespace ome::book::books::node
