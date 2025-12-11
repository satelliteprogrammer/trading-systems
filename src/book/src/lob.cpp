#include "book/lob.hpp"

#include <ranges>
#include <stdexcept>

namespace ome::book {

void Book::add(BuyOrder order) {
    order = execute(order);

    if (order.limit.quantity == 0) {
        return;
    }

    orders[order.order_id] = std::make_shared<Order>(order);

    if (!bids.contains(order.limit.price)) {
        bids.emplace(order.limit.price, order.limit);
    }
    bids[order.limit.price].orders.emplace_back(orders[order.order_id]);
    bids[order.limit.price].total_quantity += order.limit.quantity;
}

void Book::add(SellOrder order) {
    order = execute(order);

    if (order.limit.quantity == 0) {
        return;
    }

    orders[order.order_id] = std::make_shared<Order>(order);

    if (!asks.contains(order.limit.price)) {
        asks.emplace(order.limit.price, order.limit);
    }
    asks[order.limit.price].orders.emplace_back(orders[order.order_id]);
    asks[order.limit.price].total_quantity += order.limit.quantity;
}

auto Book::cancel(OrderId id) -> bool {
    if (!orders.contains(id)) {
        return false;
    }

    auto const &order = orders[id];
    if (auto it = bids.find(order->limit.price); it != bids.end()) {
        it->second.total_quantity -= order->limit.quantity;
    } else if (auto it = asks.find(order->limit.price); it != asks.end()) {
        it->second.total_quantity -= order->limit.quantity;
    } else {
        throw std::runtime_error{"order not found"};
    }

    return orders.erase(id) != 0U;
}

auto Book::volume(Price price) const -> std::uint64_t {
    if (price > asks.begin()->first) {
        if (auto it = bids.find(price); it != bids.end()) {
            return it->second.total_quantity;
        }
    } else {
        if (auto it = asks.find(price); it != asks.end()) {
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
auto Book::spread() const -> std::pair<Price, Price> {
    auto best_bid = bids.empty() ? 0 : bids.begin()->first;
    auto best_ask = asks.empty() ? 0 : asks.begin()->first;
    return {best_bid, best_ask};
}

} // namespace ome::book
