#include "book/engine.hpp"

#include <book/books/array/indexedbook.hpp>
#include <book/books/node/book.hpp>
#include <book/books/node/indexedbook.hpp>
#include <book/concepts.hpp>
#include <book/order.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <utility>

namespace ome::book {

template <concepts::OrderBook Book>
BasicEngine<Book>::BasicEngine(std::size_t max_orders) : book{max_orders} {}

template <concepts::OrderBook Book>
auto BasicEngine<Book>::add(Order order, OrderCallback on_fill) -> expected<OrderId> {
    auto const id = ++cur_order_id_;
    add({order, id}, on_fill);
    return id;
}

template <concepts::OrderBook Book> auto BasicEngine<Book>::cancel(OrderId id) -> expected<void> {
    auto const it = book.find(id);

    if (it == book.template end<Side::Buy>()) {
        return std::unexpected{"order not found"};
    }

    book.erase(it);

    return {};
}

template <concepts::OrderBook Book>
auto BasicEngine<Book>::modify(OrderId id, ModifyOps ops, OrderCallback on_fill) -> expected<void> {
    auto const it = book.find(id);
    if (it == book.template end<Side::Buy>()) [[unlikely]] {
        return std::unexpected{"order not found"};
    }

    if (ops.qty == 0) [[unlikely]] {
        return std::unexpected{"quantity must be positive"};
    }

    if (ops.price == it->price) {
        ops.price.reset();
    }
    if (ops.qty == it->quantity) {
        ops.qty.reset();
    }

    if (!ops.price && !ops.qty) [[unlikely]] {
        return std::unexpected{"nothing to modify"};
    }

    if (!ops.price && ops.qty && *ops.qty < it->quantity) {
        level_iterator{it}->qty -= it->quantity - *ops.qty;
        it->quantity = *ops.qty;
    } else {
        auto order = *it;
        book.erase(it);
        if (ops.price) {
            order.price = *ops.price;
        }
        if (ops.qty) {
            order.quantity = *ops.qty;
        }
        add(order, on_fill);
    }
    return {};
}

template <concepts::OrderBook Book>
auto BasicEngine<Book>::volume(Price price) const -> expected<std::uint64_t> {
    std::uint64_t vol{};

    if (auto const itf = book.template find<Side::Buy>(price);
        itf != book.template levels<Side::Buy>().end()) {
        vol += itf->qty;
    }

    if (auto const itf = book.template find<Side::Sell>(price);
        itf != book.template levels<Side::Sell>().end()) {
        vol += itf->qty;
    }

    return vol;
}

template <concepts::OrderBook Book>
auto BasicEngine<Book>::spread() const -> expected<std::pair<Price, Price>> {
    auto const it_bid = book.template begin<Side::Buy>();
    auto const best_bid = it_bid != book.template end<Side::Buy>() ? it_bid->price : 0;

    auto const it_ask = book.template begin<Side::Sell>();
    auto const best_ask = it_ask != book.template end<Side::Sell>() ? it_ask->price : 0;

    return std::pair{best_bid, best_ask};
}

template <concepts::OrderBook Book>
void BasicEngine<Book>::add(RestingOrder order, OrderCallback on_fill) {
    switch (order.side) {
    case Side::Buy:
        execute<Side::Sell>(order, on_fill);
        if (order.quantity > 0) {
            book.template emplace<Side::Buy>({order, order.order_id});
        }
        break;
    case Side::Sell:
        execute<Side::Buy>(order, on_fill);
        if (order.quantity > 0) {
            book.template emplace<Side::Sell>({order, order.order_id});
        }
        break;
    }
}

template <concepts::OrderBook Book>
template <Side S>
void BasicEngine<Book>::execute(Order &order, OrderCallback on_fill) {

    auto const comp = [] {
        if constexpr (S == Side::Buy) {
            return std::greater<>{};
        } else {
            return std::less<>{};
        }
    };

    for (auto it = book.template begin<S>();
         it != book.template end<S>() && !comp()(order.price, (*it).price) && order.quantity > 0;) {

        on_fill(it->order_id, *it, it->price, std::min(order.quantity, it->quantity));

        if (it->quantity > order.quantity) {
            it->quantity -= order.quantity;
            level_iterator{it}->qty -= order.quantity;
            order.quantity = 0;
            break;
        }

        order.quantity -= it->quantity;

        it = book.erase(it);
    }
}

template class BasicEngine<books::node::Book>;
template class BasicEngine<books::node::IndexedBook>;
template class BasicEngine<books::array::IndexedBook>;

} // namespace ome::book
