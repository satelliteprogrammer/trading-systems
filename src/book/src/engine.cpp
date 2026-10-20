#include "book/engine.hpp"

#include <cassert>
#include <expected>
#include <utility>

namespace ome::book {

Engine::Engine(std::size_t max_orders) : orders{max_orders} {}

auto Engine::add(Order order, OrderCallback on_fill) -> expected<OrderId> {
    auto const id = next_id++;
    assert(id < orders.size());

    switch (order.side) {
    case Side::Buy:
        execute(order, asks, on_fill);
        rest(order, id, bids);
        return id;
    case Side::Sell:
        execute(order, bids, on_fill);
        rest(order, id, asks);
        return id;
    }
    std::unreachable();
}

auto Engine::cancel(OrderId id) -> expected<void> {
    assert(id < orders.size());

    auto &[price, order_it] = orders[id];
    if (order_it == std::list<RestingOrder>::iterator{}) {
        return std::unexpected{"order not found"};
    }

    auto const &order = *order_it;
    if (auto it = bids.find(price); it != bids.end()) {
        it->second.total_quantity -= order.quantity;
        it->second.orders.erase(order_it);
        if (it->second.total_quantity == 0) {
            assert(it->second.orders.empty());
            bids.erase(it);
        }
    } else if (auto it = asks.find(price); it != asks.end()) {
        it->second.total_quantity -= order.quantity;
        it->second.orders.erase(order_it);
        if (it->second.total_quantity == 0) {
            assert(it->second.orders.empty());
            asks.erase(it);
        }
    } else {
        std::unreachable();
    }

    order_it = std::list<RestingOrder>::iterator{};

    return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto Engine::cancel(OrderId id, std::uint64_t quantity) -> expected<void> {
    assert(id < orders.size());

    auto &[price, order_it] = orders[id];
    if (order_it == std::list<RestingOrder>::iterator{}) {
        return std::unexpected{"order not found"};
    }

    auto &order = *order_it;
    if (order.quantity <= quantity) {
        return cancel(id);
    }

    order.quantity -= quantity;

    if (auto it = bids.find(price); it != bids.end()) {
        it->second.total_quantity -= quantity;
        assert(it->second.total_quantity > 0);
    } else if (auto it = asks.find(price); it != asks.end()) {
        it->second.total_quantity -= quantity;
        assert(it->second.total_quantity > 0);
    } else {
        std::unreachable();
    }

    return {};
}

auto Engine::volume(Price price) const -> expected<std::uint64_t> {
    if (!asks.empty() && price >= asks.begin()->first) {
        if (auto it = asks.find(price); it != asks.end()) {
            return it->second.total_quantity;
        }
    } else {
        if (auto it = bids.find(price); it != bids.end()) {
            return it->second.total_quantity;
        }
    }

    // limit not found
    return 0;
}

auto Engine::spread() const -> expected<std::pair<Price, Price>> {
    auto best_bid = bids.empty() ? 0 : bids.begin()->first;
    auto best_ask = asks.empty() ? 0 : asks.begin()->first;
    return std::pair{best_bid, best_ask};
}

template <typename Levels>
void Engine::execute(Order &order, Levels &opposite, OrderCallback on_fill) {
    // levels are sorted best first, so a level crosses unless the order's price sorts before it
    auto const crosses = [&](Price level) -> bool {
        return !opposite.key_comp()(order.price, level);
    };

    for (auto it = opposite.begin();
         it != opposite.end() && crosses(it->first) && order.quantity > 0;) {
        for (auto it_order = it->second.orders.begin();
             it_order != it->second.orders.end() && order.quantity > 0;) {

            on_fill(it_order->order_id, *it_order, it->first,
                    std::min(order.quantity, it_order->quantity));

            if (it_order->quantity > order.quantity) {
                it_order->quantity -= order.quantity;
                it->second.total_quantity -= order.quantity;

                order.quantity = 0;
                break;
            }

            order.quantity -= it_order->quantity;
            it->second.total_quantity -= it_order->quantity;

            orders[it_order->order_id].order_it = std::list<RestingOrder>::iterator{};
            it_order = it->second.orders.erase(it_order);
        }

        if (it->second.orders.empty()) {
            // all orders consumed at this level, remove it
            it = opposite.erase(it);
        } else {
            ++it;
        }
    }
}

template <typename Levels> void Engine::rest(Order const &order, OrderId id, Levels &same) {
    if (order.quantity == 0) {
        return;
    }
    auto &level = same[order.price];
    level.total_quantity += order.quantity;
    level.orders.emplace_back(order, id);
    orders[id] = OrderAtLimit{.price = order.price, .order_it = std::prev(level.orders.end())};
}

} // namespace ome::book
