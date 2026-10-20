#include "book/engine.hpp"

#include <cassert>
#include <expected>
#include <utility>

namespace ome::book {

Engine::Engine(std::size_t /*max_orders*/) {}

auto Engine::add(Order order, OrderCallback on_fill) -> expected<OrderId> {
    auto id = ++cur_order_id_;

    switch (order.side) {
    case Side::Buy:
        execute<Side::Sell>(order, on_fill);
        if (order.quantity > 0) {
            book.emplace<Side::Buy>({order, id});
        }
        break;
    case Side::Sell:
        execute<Side::Buy>(order, on_fill);
        if (order.quantity > 0) {
            book.emplace<Side::Sell>({order, id});
        }
        break;
    }

    return id;
}

auto Engine::cancel(OrderId id) -> expected<void> {
    auto it = book.find(id);

    if (it == book.end<Side::Buy>()) {
        return std::unexpected{"order not found"};
    }

    book.erase(it);

    return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto Engine::cancel(OrderId id, std::uint64_t quantity) -> expected<void> {
    auto it = book.find(id);

    if (it == book.end<Side::Buy>()) {
        return std::unexpected{"order not found"};
    }

    if (it->quantity > quantity) {
        it->quantity -= quantity;
    } else {
        book.erase(it);
    }
    return {};
}

auto Engine::volume(Price price) const -> expected<std::uint64_t> {
    std::uint64_t vol{};

    for (auto it = book.begin<Side::Buy>(); it != book.end<Side::Buy>(); ++it) {
        if (it->price == price) {
            vol += it->quantity;
        }
    }

    for (auto it = book.begin<Side::Sell>(); it != book.end<Side::Sell>(); ++it) {
        if (it->price == price) {
            vol += it->quantity;
        }
    }

    return vol;
}

auto Engine::spread() const -> expected<std::pair<Price, Price>> {
    auto it_bid = book.begin<Side::Buy>();
    auto best_bid = it_bid != book.end<Side::Buy>() ? it_bid->price : 0;

    auto it_ask = book.begin<Side::Sell>();
    auto best_ask = it_ask != book.end<Side::Sell>() ? it_ask->price : 0;

    return std::pair{best_bid, best_ask};
}

template <Side S> void Engine::execute(Order &order, OrderCallback on_fill) {

    auto comp = [] {
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
            order.quantity = 0;
            break;
        }

        order.quantity -= it->quantity;

        it = book.erase(it);
    }
}

} // namespace ome::book
