#include "book/engine.hpp"

#include <book/order.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <utility>

namespace ome::book {

Engine::Engine(std::size_t /*max_orders*/) {}

auto Engine::add(Order order, OrderCallback on_fill) -> expected<OrderId> {
    auto const id = ++cur_order_id_;
    add({order, id}, on_fill);
    return id;
}

auto Engine::cancel(OrderId id) -> expected<void> {
    auto const it = book.find(id);

    if (it == book.end<Side::Buy>()) {
        return std::unexpected{"order not found"};
    }

    book.erase(it);

    return {};
}

auto Engine::modify(OrderId id, ModifyOps ops, OrderCallback on_fill) -> expected<void> {
    auto const it = book.find(id);
    if (it == book.end<Side::Buy>()) [[unlikely]] {
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
        decltype(book)::level_iterator{it}->qty -= it->quantity - *ops.qty;
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

auto Engine::volume(Price price) const -> expected<std::uint64_t> {
    std::uint64_t vol{};

    if (auto const itf = book.find<Side::Buy>(price); itf != book.levels<Side::Buy>().end()) {
        vol += itf->qty;
    }

    if (auto const itf = book.find<Side::Sell>(price); itf != book.levels<Side::Sell>().end()) {
        vol += itf->qty;
    }

    return vol;
}

auto Engine::spread() const -> expected<std::pair<Price, Price>> {
    auto const it_bid = book.begin<Side::Buy>();
    auto const best_bid = it_bid != book.end<Side::Buy>() ? it_bid->price : 0;

    auto const it_ask = book.begin<Side::Sell>();
    auto const best_ask = it_ask != book.end<Side::Sell>() ? it_ask->price : 0;

    return std::pair{best_bid, best_ask};
}

void Engine::add(RestingOrder order, OrderCallback on_fill) {
    switch (order.side) {
    case Side::Buy:
        execute<Side::Sell>(order, on_fill);
        if (order.quantity > 0) {
            book.emplace<Side::Buy>({order, order.order_id});
        }
        break;
    case Side::Sell:
        execute<Side::Buy>(order, on_fill);
        if (order.quantity > 0) {
            book.emplace<Side::Sell>({order, order.order_id});
        }
        break;
    }
}

template <Side S> void Engine::execute(Order &order, OrderCallback on_fill) {

    auto const comp = [] {
        if constexpr (S == Side::Buy) {
            return std::greater<>{};
        } else {
            return std::less<>{};
        }
    };

    for (auto it = book.begin<S>();
         it != book.end<S>() && !comp()(order.price, (*it).price) && order.quantity > 0;) {

        on_fill(it->order_id, *it, it->price, std::min(order.quantity, it->quantity));

        if (it->quantity > order.quantity) {
            it->quantity -= order.quantity;
            decltype(book)::level_iterator{it}->qty -= order.quantity;
            order.quantity = 0;
            break;
        }

        order.quantity -= it->quantity;

        it = book.erase(it);
    }
}

} // namespace ome::book
