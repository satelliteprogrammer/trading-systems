#include "book/books/array/indexedbook.hpp"

#include <book/books/array/pricelevels.hpp>
#include <book/order.hpp>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>

namespace ome::book::books::array {

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

IndexedBook::IndexedBook(std::size_t max_orders) : levels_{max_orders} {}

template <Side S> auto IndexedBook::begin() const -> iterator {
    return const_cast<IndexedBook *>(this)->begin<S>();
}

template <Side S> auto IndexedBook::begin() -> iterator {
    return {levels_.get<S>().begin(), levels_.get<S>().end()};
}

template <Side S> auto IndexedBook::end() const -> iterator {
    return const_cast<IndexedBook *>(this)->end<S>();
}

template <Side S> auto IndexedBook::end() -> iterator { return {levels_.get<S>().end()}; }

template <Side S> auto IndexedBook::levels() const -> std::ranges::subrange<level_iterator> {
    return const_cast<IndexedBook *>(this)->levels<S>();
}

template <Side S> auto IndexedBook::levels() -> std::ranges::subrange<level_iterator> {
    return {level_iterator{levels_.get<S>().begin()}, level_iterator{levels_.get<S>().end()}};
}

template <Side S> auto IndexedBook::emplace(RestingOrder order) -> iterator {
    assert(!levels_[order.order_id].is_linked());

    static_cast<RestingOrder &>(levels_[order.order_id]) = order;

    iterator::map_iterator it;
    if (auto const itf = by_price_map<S>().find(order.price); itf != by_price_map<S>().end()) {
        it = itf->second;
    } else {
        bool inserted{};
        std::tie(it, inserted) = levels_.get<S>().try_emplace(order.price);
        assert(inserted);
        by_price_map<S>().try_emplace(order.price, it);
    }

    auto &level = it->second;
    level.orders.push_back(levels_[order.order_id]);
    level.qty += order.quantity;

    return {it, levels_.get<S>().end(), std::prev(level.orders.end())};
}

auto IndexedBook::erase(iterator it) -> iterator {
    auto const price = it->price;
    auto const side = it->side;
    auto const qty = it->quantity;

    it.level_it_->second.qty -= qty;
    it.order_it_ = it.level_it_->second.orders.erase(it.order_it_);

    if (it.level_it_->second.orders.empty()) {
        assert(it.level_it_->second.qty == 0);

        switch (side) {
        case Side::Buy:
            by_price_map<Side::Buy>().erase(price);
            it.satisfy(levels_.get<Side::Buy>().erase(it.level_it_));
            break;
        case Side::Sell:
            by_price_map<Side::Sell>().erase(price);
            it.satisfy(levels_.get<Side::Sell>().erase(it.level_it_));
            break;
        }
    } else {
        it.satisfy();
    }

    return it;
}

auto IndexedBook::find(OrderId id) const -> iterator {
    auto &order = const_cast<IndexedBook *>(this)->levels_[id];

    if (!order.is_linked()) {
        return end<Side::Buy>();
    }

    iterator::map_iterator it;
    switch (order.side) {
    case Side::Buy:
        assert(by_price_map<Side::Buy>().contains(order.price));
        it = by_price_map<Side::Buy>().find(order.price)->second;
        return {it, const_cast<IndexedBook *>(this)->levels_.get<Side::Buy>().end(),
                it->second.orders.iterator_to(order)};
    case Side::Sell:
        assert(by_price_map<Side::Sell>().contains(order.price));
        it = by_price_map<Side::Sell>().find(order.price)->second;
        return {it, const_cast<IndexedBook *>(this)->levels_.get<Side::Sell>().end(),
                it->second.orders.iterator_to(order)};
    }
    std::unreachable();
}

template <Side S> auto IndexedBook::find(Price price) const -> level_iterator {
    auto const it = by_price_map<S>().find(price);
    if (it == by_price_map<S>().end()) {
        return const_cast<IndexedBook *>(this)->levels_.get<S>().end();
    }
    return it->second;
}

template auto IndexedBook::begin<Side::Buy>() const -> iterator;
template auto IndexedBook::begin<Side::Buy>() -> iterator;
template auto IndexedBook::begin<Side::Sell>() const -> iterator;
template auto IndexedBook::begin<Side::Sell>() -> iterator;
template auto IndexedBook::end<Side::Buy>() const -> iterator;
template auto IndexedBook::end<Side::Buy>() -> iterator;
template auto IndexedBook::end<Side::Sell>() const -> iterator;
template auto IndexedBook::end<Side::Sell>() -> iterator;

template auto IndexedBook::levels<Side::Buy>() const -> std::ranges::subrange<level_iterator>;
template auto IndexedBook::levels<Side::Buy>() -> std::ranges::subrange<level_iterator>;
template auto IndexedBook::levels<Side::Sell>() const -> std::ranges::subrange<level_iterator>;
template auto IndexedBook::levels<Side::Sell>() -> std::ranges::subrange<level_iterator>;

template auto IndexedBook::emplace<Side::Buy>(RestingOrder) -> iterator;
template auto IndexedBook::emplace<Side::Sell>(RestingOrder) -> iterator;

template auto IndexedBook::find<Side::Buy>(Price) const -> level_iterator;
template auto IndexedBook::find<Side::Sell>(Price) const -> level_iterator;

} // namespace ome::book::books::array
