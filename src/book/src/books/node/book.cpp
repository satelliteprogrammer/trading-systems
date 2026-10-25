#include "book/books/node/book.hpp"

#include <book/order.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <ranges>

namespace ome::book::books::node {

template <Side S> auto Book::begin() const -> iterator {
    return const_cast<Book *>(this)->begin<S>();
}

template <Side S> auto Book::begin() -> iterator {
    return {maps_.get<S>().begin(), maps_.get<S>().end()};
}

template <Side S> auto Book::end() const -> iterator { return const_cast<Book *>(this)->end<S>(); }

template <Side S> auto Book::end() -> iterator { return {maps_.get<S>().end()}; }

template <Side S> auto Book::levels() const -> std::ranges::subrange<level_iterator> {
    return const_cast<Book *>(this)->levels<S>();
}

template <Side S> auto Book::levels() -> std::ranges::subrange<level_iterator> {
    return {level_iterator{maps_.get<S>().begin()}, level_iterator{maps_.get<S>().end()}};
}

template <Side S> auto Book::emplace(RestingOrder order) -> iterator {
    auto [it, _] = maps_.get<S>().try_emplace(order.price);
    it->second.orders.emplace_back(order);
    it->second.qty += order.quantity;
    return {it, maps_.get<S>().end(), std::prev(it->second.orders.end())};
}

auto Book::erase(iterator it) -> iterator {
    auto const side = it->side;
    auto const qty = it->quantity;

    it.order_it_ = it.level_it_->second.orders.erase(it.order_it_);
    it.level_it_->second.qty -= qty;

    if (it.level_it_->second.orders.empty()) {
        assert(it.level_it_->second.qty == 0);

        switch (side) {
        case Side::Buy:
            it.satisfy(maps_.get<Side::Buy>().erase(it.level_it_));
            break;
        case Side::Sell:
            it.satisfy(maps_.get<Side::Sell>().erase(it.level_it_));
        }
    } else {
        it.satisfy();
    }

    return it;
}

auto Book::find(OrderId id) const -> iterator {
    auto const is_order = [id](auto const &order) { return order.order_id == id; };

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

template <Side S> auto Book::find(Price price) const -> level_iterator {
    return const_cast<Book *>(this)->maps_.get<S>().find(price);
}

template auto Book::begin<Side::Buy>() const -> iterator;
template auto Book::begin<Side::Buy>() -> iterator;
template auto Book::begin<Side::Sell>() const -> iterator;
template auto Book::begin<Side::Sell>() -> iterator;
template auto Book::end<Side::Buy>() const -> iterator;
template auto Book::end<Side::Buy>() -> iterator;
template auto Book::end<Side::Sell>() const -> iterator;
template auto Book::end<Side::Sell>() -> iterator;

template auto Book::levels<Side::Buy>() const -> std::ranges::subrange<level_iterator>;
template auto Book::levels<Side::Buy>() -> std::ranges::subrange<level_iterator>;
template auto Book::levels<Side::Sell>() const -> std::ranges::subrange<level_iterator>;
template auto Book::levels<Side::Sell>() -> std::ranges::subrange<level_iterator>;

template auto Book::emplace<Side::Buy>(RestingOrder) -> iterator;
template auto Book::emplace<Side::Sell>(RestingOrder) -> iterator;

template auto Book::find<Side::Buy>(Price) const -> level_iterator;
template auto Book::find<Side::Sell>(Price) const -> level_iterator;

} // namespace ome::book::books::node
