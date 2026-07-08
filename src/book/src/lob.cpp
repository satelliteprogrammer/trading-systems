#include "book/lob.hpp"

#include <cassert>
#include <expected>
#include <utility>

namespace ome::book {

auto Book::add(BuyOrder order, OrderCallback on_fill) -> expected<void> {

    execute(order, on_fill);
    if (order.quantity > 0) {
        auto price = order.price;
        auto &bid = bids[price];
        bid.total_quantity += order.quantity;
        bid.orders.emplace_back(order);
        orders[order.order_id] =
            OrderAtLimit{.price = price, .order_it = std::prev(bid.orders.end())};
    }

    return {};
}

auto Book::add(SellOrder order, OrderCallback on_fill) -> expected<void> {

    execute(order, on_fill);
    if (order.quantity > 0) {
        auto price = order.price;
        auto &ask = asks[price];
        ask.total_quantity += order.quantity;
        ask.orders.emplace_back(order);
        orders[order.order_id] =
            OrderAtLimit{.price = price, .order_it = std::prev(ask.orders.end())};
    }

    return {};
}

auto Book::cancel(OrderId id) -> expected<void> {
    auto orders_it = orders.find(id);
    if (orders_it == orders.end()) {
        return std::unexpected{"order not found"};
    }

    auto const &[price, order_it] = orders_it->second;
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

    if (orders.erase(id) == 0U) {
        std::unreachable();
    }

    return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto Book::cancel(OrderId id, std::uint64_t quantity) -> expected<void> {
    auto orders_it = orders.find(id);
    if (orders_it == orders.end()) {
        return std::unexpected{"order not found"};
    }

    auto const &[price, order_it] = orders_it->second;
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

auto Book::volume(Price price) const -> expected<std::uint64_t> {
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

auto Book::spread() const -> expected<std::pair<Price, Price>> {
    auto best_bid = bids.empty() ? 0 : bids.begin()->first;
    auto best_ask = asks.empty() ? 0 : asks.begin()->first;
    return std::pair{best_bid, best_ask};
}

void Book::reserve(std::size_t n) { orders.reserve(n); }

void Book::execute(BuyOrder &order, OrderCallback on_fill) {
    for (auto it = asks.begin();
         it != asks.end() && order.price >= it->first && order.quantity > 0;) {
        for (auto it_order = it->second.orders.begin();
             it_order != it->second.orders.end() && order.quantity > 0;) {

            on_fill(*it_order, it->first, std::min(order.quantity, it_order->quantity));

            if (it_order->quantity > order.quantity) {
                it_order->quantity -= order.quantity;
                it->second.total_quantity -= order.quantity;

                order.quantity = 0;
                break;
            }

            order.quantity -= it_order->quantity;
            it->second.total_quantity -= it_order->quantity;

            if (orders.erase(it_order->order_id) == 0U) {
                std::unreachable();
            }
            it_order = it->second.orders.erase(it_order);
        }

        if (it->second.orders.empty()) {
            // all orders consumed at this level, remove it
            it = asks.erase(it);
        } else {
            ++it;
        }
    }
}

void Book::execute(SellOrder &order, OrderCallback on_fill) {
    for (auto it = bids.begin();
         it != bids.end() && order.price <= it->first && order.quantity > 0;) {
        for (auto it_order = it->second.orders.begin();
             it_order != it->second.orders.end() && order.quantity > 0;) {

            on_fill(*it_order, it->first, std::min(order.quantity, it_order->quantity));

            if (it_order->quantity > order.quantity) {
                it_order->quantity -= order.quantity;
                it->second.total_quantity -= order.quantity;

                order.quantity = 0;
                break;
            }

            order.quantity -= it_order->quantity;
            it->second.total_quantity -= it_order->quantity;

            if (orders.erase(it_order->order_id) == 0U) {
                std::unreachable();
            }
            it_order = it->second.orders.erase(it_order);
        }

        if (it->second.orders.empty()) {
            // all orders consumed at this level, remove it
            it = bids.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace ome::book
