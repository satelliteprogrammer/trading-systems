#include "book/books/node/book.hpp"

#include <book/order.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>

namespace ome::book::books::node {

template <Side S> auto Book::begin() const -> iterator {
    return const_cast<Book *>(this)->begin<S>();
}

template <Side S> auto Book::begin() -> iterator {
    return {maps_.get<S>().begin(), maps_.get<S>().end()};
}

template <Side S> auto Book::end() const -> iterator { return const_cast<Book *>(this)->end<S>(); }

template <Side S> auto Book::end() -> iterator { return {maps_.get<S>().end()}; }

template <Side S> auto Book::emplace(RestingOrder order) -> iterator {
    auto [it, _] = maps_.get<S>().try_emplace(order.price);
    it->second.orders.emplace_back(order);
    return {it, maps_.get<S>().end(), std::prev(it->second.orders.end())};
}

auto Book::erase(iterator it) -> iterator {
    auto const side = it->side;

    it.order_it_ = it.level_it_->second.orders.erase(it.order_it_);

    if (it.level_it_->second.orders.empty()) {
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

auto Book::find(OrderId id) -> iterator {
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

template auto Book::begin<Side::Buy>() const -> iterator;
template auto Book::begin<Side::Buy>() -> iterator;
template auto Book::begin<Side::Sell>() const -> iterator;
template auto Book::begin<Side::Sell>() -> iterator;
template auto Book::end<Side::Buy>() const -> iterator;
template auto Book::end<Side::Buy>() -> iterator;
template auto Book::end<Side::Sell>() const -> iterator;
template auto Book::end<Side::Sell>() -> iterator;

template auto Book::emplace<Side::Buy>(RestingOrder) -> iterator;
template auto Book::emplace<Side::Sell>(RestingOrder) -> iterator;

} // namespace ome::book::books::node
