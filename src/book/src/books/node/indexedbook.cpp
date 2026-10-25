#include "book/books/node/indexedbook.hpp"

#include <book/order.hpp>

#include <cassert>
#include <iterator>
#include <tuple>
#include <utility>

namespace ome::book::books::node {

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

template <Side S> auto IndexedBook::begin() const -> iterator {
    return const_cast<IndexedBook *>(this)->begin<S>();
}

template <Side S> auto IndexedBook::begin() -> iterator {
    return {maps_.get<S>().begin(), maps_.get<S>().end()};
}

template <Side S> auto IndexedBook::end() const -> iterator {
    return const_cast<IndexedBook *>(this)->end<S>();
}

template <Side S> auto IndexedBook::end() -> iterator { return {maps_.get<S>().end()}; }

template <Side S> auto IndexedBook::emplace(RestingOrder order) -> iterator {
    iterator::map_iterator it;
    if (auto const itf = by_price_map<S>().find(order.price); itf != by_price_map<S>().end()) {
        it = itf->second;
    } else {
        bool inserted{};
        std::tie(it, inserted) = maps_.get<S>().try_emplace(order.price);
        assert(inserted);
        by_price_map<S>().try_emplace(order.price, it);
    }

    it->second.orders.emplace_back(order);

    auto order_it = std::prev(it->second.orders.end());
    by_order_id_.try_emplace(order.order_id, order_it);

    return {it, maps_.get<S>().end(), order_it};
}

auto IndexedBook::erase(iterator it) -> iterator {
    auto const id = it->order_id;
    auto const price = it->price;
    auto const side = it->side;

    by_order_id_.erase(id);
    it.order_it_ = it.level_it_->second.orders.erase(it.order_it_);

    if (it.level_it_->second.orders.empty()) {
        switch (side) {
        case Side::Buy:
            by_price_map<Side::Buy>().erase(price);
            it.satisfy(maps_.get<Side::Buy>().erase(it.level_it_));
            break;
        case Side::Sell:
            by_price_map<Side::Sell>().erase(price);
            it.satisfy(maps_.get<Side::Sell>().erase(it.level_it_));
            break;
        }
    } else {
        it.satisfy();
    }

    return it;
}

auto IndexedBook::find(OrderId id) -> iterator {
    auto const it = by_order_id_.find(id);
    if (it == by_order_id_.end()) {
        return end<Side::Buy>();
    }

    switch (it->second->side) {
    case Side::Buy:
        assert(by_price_map<Side::Buy>().contains(it->second->price));
        return {by_price_map<Side::Buy>()[it->second->price], maps_.get<Side::Buy>().end(),
                it->second};
    case Side::Sell:
        assert(by_price_map<Side::Sell>().contains(it->second->price));
        return {by_price_map<Side::Sell>()[it->second->price], maps_.get<Side::Sell>().end(),
                it->second};
    }
    std::unreachable();
}

template auto IndexedBook::begin<Side::Buy>() const -> iterator;
template auto IndexedBook::begin<Side::Buy>() -> iterator;
template auto IndexedBook::begin<Side::Sell>() const -> iterator;
template auto IndexedBook::begin<Side::Sell>() -> iterator;
template auto IndexedBook::end<Side::Buy>() const -> iterator;
template auto IndexedBook::end<Side::Buy>() -> iterator;
template auto IndexedBook::end<Side::Sell>() const -> iterator;
template auto IndexedBook::end<Side::Sell>() -> iterator;

template auto IndexedBook::emplace<Side::Buy>(RestingOrder) -> iterator;
template auto IndexedBook::emplace<Side::Sell>(RestingOrder) -> iterator;

} // namespace ome::book::books::node
