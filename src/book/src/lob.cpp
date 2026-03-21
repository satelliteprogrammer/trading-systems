#include "book/lob.hpp"

#include <expected>
#include <ranges>
#include <utility>

namespace ome::book {

auto Book::add(BuyOrder order) -> std::expected<void, std::string_view> {
    order = execute(order);

    if (order.limit.quantity == 0) {
        return {};
    }

    orders[order.order_id] = std::make_shared<Order>(order);

    if (!bids.contains(order.limit.price)) {
        bids.emplace(order.limit.price, order.limit);
    }
    bids[order.limit.price].orders.emplace_back(orders[order.order_id]);
    bids[order.limit.price].total_quantity += order.limit.quantity;

    return {};
}

auto Book::add(SellOrder order) -> std::expected<void, std::string_view> {
    if (order.limit.quantity == 0) {
        return {};
    }

    orders[order.order_id] = std::make_shared<Order>(order);

    if (!asks.contains(order.limit.price)) {
        asks.emplace(order.limit.price, order.limit);
    }
    asks[order.limit.price].orders.emplace_back(orders[order.order_id]);
    asks[order.limit.price].total_quantity += order.limit.quantity;

    return {};
}

auto Book::cancel(OrderId id) -> std::expected<void, std::string_view> {
    if (!orders.contains(id)) {
        return std::unexpected{"order not found"};
    }

    auto const &order = orders[id];
    if (auto it = bids.find(order->limit.price); it != bids.end()) {
        it->second.total_quantity -= order->limit.quantity;
    } else if (auto it = asks.find(order->limit.price); it != asks.end()) {
        it->second.total_quantity -= order->limit.quantity;
    } else {
        std::unreachable();
    }

    if (orders.erase(id) == 0U) {
        std::unreachable();
    }

    return {};
}

auto Book::volume(Price price) const -> std::expected<std::uint64_t, std::string_view> {
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

auto Book::execute(BuyOrder order) -> BuyOrder {
    while (order.limit.quantity > 0 && !asks.empty() &&
           order.limit.price >= asks.begin()->second.limit.price) {

        auto &ask = std::views::values(asks).front();
        if (ask.orders.empty()) {
            asks.erase(asks.begin());
            continue;
        }

        if (auto sp_order = ask.orders.front().lock()) {
            if (sp_order->limit.quantity > order.limit.quantity) {
                sp_order->limit.quantity -= order.limit.quantity;
                ask.total_quantity -= order.limit.quantity;
                order.limit.quantity = 0;
            } else {
                order.limit.quantity -= sp_order->limit.quantity;
                ask.total_quantity -= sp_order->limit.quantity;
                ask.orders.pop_front();
            }
        } else {
            // expired order, remove it
            ask.orders.pop_front();
        }
    }

    return order;
}

auto Book::execute(SellOrder order) -> SellOrder {
    while (order.limit.quantity > 0 && !bids.empty() &&
           order.limit.price <= bids.begin()->second.limit.price) {

        auto &bid = std::views::values(bids).front();
        if (bid.orders.empty()) {
            bids.erase(bids.begin());
            continue;
        }

        if (auto sp_order = bid.orders.front().lock()) {
            if (sp_order->limit.quantity > order.limit.quantity) {
                sp_order->limit.quantity -= order.limit.quantity;
                bid.total_quantity -= order.limit.quantity;
                order.limit.quantity = 0;
            } else {
                order.limit.quantity -= sp_order->limit.quantity;
                bid.total_quantity -= sp_order->limit.quantity;
                bid.orders.pop_front();
            }
        } else {
            // expired order, remove it
            bid.orders.pop_front();
        }
    }

    return order;
}

auto Book::spread() const -> std::expected<std::pair<Price, Price>, std::string_view> {
    auto best_bid = bids.empty() ? 0 : bids.begin()->first;
    auto best_ask = asks.empty() ? 0 : asks.begin()->first;
    return std::pair{best_bid, best_ask};
}

void Book::reserve(std::size_t n) { orders.reserve(n); }

} // namespace ome::book
