#include "book/lob.hpp"

#include <cassert>
#include <expected>
#include <utility>

namespace ome::book {

auto Book::add(BuyOrder order, bool execute, std::function<void(Order)> const &on_fill)
    -> expected<void> {
    std::optional<Order> unfulfilled_order{order};

    if (!asks.empty() && order.price >= asks.begin()->first) {
        if (!execute) {
            return std::unexpected{"order price crosses spread"};
        }
        auto res = this->execute(order, execute, on_fill);
        if (!res) {
            return res.transform([](auto &&) -> auto { return; });
        }
        unfulfilled_order = std::move(res.value());
    }

    if (auto order_ = unfulfilled_order) {
        auto &bid = bids[order_->price];
        bid.orders.emplace_back(*order_);
        bid.total_quantity += order_->quantity;
        orders[order_->order_id] =
            OrderAtLimit{.price = order_->price, .order_it = std::prev(bid.orders.end())};
    }

    return {};
}

auto Book::add(SellOrder order, bool execute, std::function<void(Order)> const &on_fill)
    -> expected<void> {
    std::optional<Order> unfulfilled_order{order};

    if (!bids.empty() && order.price <= bids.begin()->first) {
        if (!execute) {
            return std::unexpected{"order price crosses spread"};
        }
        auto res = this->execute(order, execute, on_fill);
        if (!res) {
            return res.transform([](auto &&) -> auto { return; });
        }
        unfulfilled_order = std::move(res.value());
    }

    if (auto order_ = unfulfilled_order) {
        auto &ask = asks[order_->price];
        ask.orders.emplace_back(*order_);
        ask.total_quantity += order_->quantity;
        orders[order_->order_id] =
            OrderAtLimit{.price = order_->price, .order_it = std::prev(ask.orders.end())};
    }

    return {};
}

auto Book::cancel(OrderId id) -> expected<void> {
    if (!orders.contains(id)) {
        return std::unexpected{"order not found"};
    }

    auto const &[price, order_it] = orders[id];
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
    if (!orders.contains(id)) {
        return std::unexpected{"order not found"};
    }

    auto const &[price, order_it] = orders[id];
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

auto Book::execute(BuyOrder order, bool execute, std::function<void(Order)> const &on_fill)
    -> expected<std::optional<Order>> {
    if (asks.empty()) {
        if (!execute) {
            return std::unexpected{"no asks to execute against"};
        }
        return order;
    }

    for (auto it = asks.begin();
         it != asks.end() && order.price >= it->first && order.quantity > 0;) {
        for (auto ito = it->second.orders.begin();
             ito != it->second.orders.end() && order.quantity > 0;) {
            auto &order_ = *ito;
            OrderFilled order_filled{order_};

            if (order_.quantity > order.quantity) {
                order_.quantity -= order.quantity;
                it->second.total_quantity -= order.quantity;

                order_filled.quantity = order.quantity;
                order_filled.fully_filled = false;
                if (on_fill) {
                    on_fill(order_filled); // NOLINT(cppcoreguidelines-slicing)
                }

                order.quantity = 0;
                break;
            }

            order.quantity -= order_.quantity;
            it->second.total_quantity -= order_.quantity;

            order_filled.fully_filled = true;
            if (on_fill) {
                on_fill(order_filled); // NOLINT(cppcoreguidelines-slicing)
            }

            if (orders.erase(order_.order_id) == 0U) {
                std::unreachable();
            }
            ito = it->second.orders.erase(ito);
        }

        if (it->second.orders.empty()) {
            // all orders consumed at this level, remove it
            it = asks.erase(it);
        } else {
            ++it;
        }
    }

    if (order.quantity > 0) {
        if (!execute) {
            return std::unexpected{"order not fully executed"};
        }
        return order;
    }
    return {};
}

auto Book::execute(SellOrder order, bool execute, std::function<void(Order)> const &on_fill)
    -> expected<std::optional<Order>> {
    if (bids.empty()) {
        if (!execute) {
            return std::unexpected{"no bids to execute against"};
        }
        return order;
    }

    for (auto it = bids.begin();
         it != bids.end() && order.price <= it->first && order.quantity > 0;) {
        for (auto ito = it->second.orders.begin();
             ito != it->second.orders.end() && order.quantity > 0;) {
            auto &order_ = *ito;
            OrderFilled order_filled{order_};

            if (order_.quantity > order.quantity) {
                order_.quantity -= order.quantity;
                it->second.total_quantity -= order.quantity;

                order_filled.quantity = order.quantity;
                order_filled.fully_filled = false;
                if (on_fill) {
                    on_fill(order_filled); // NOLINT(cppcoreguidelines-slicing)
                }

                order.quantity = 0;
                break;
            }

            order.quantity -= order_.quantity;
            it->second.total_quantity -= order_.quantity;

            order_filled.fully_filled = true;
            if (on_fill) {
                on_fill(order_filled); // NOLINT(cppcoreguidelines-slicing)
            }

            if (orders.erase(order_.order_id) == 0U) {
                std::unreachable();
            }
            ito = it->second.orders.erase(ito);
        }

        if (it->second.orders.empty()) {
            // all orders consumed at this level, remove it
            it = bids.erase(it);
        } else {
            ++it;
        }
    }

    if (order.quantity > 0) {
        if (!execute) {
            return std::unexpected{"order not fully executed"};
        }
        return order;
    }
    return {};
}

auto Book::spread() const -> expected<std::pair<Price, Price>> {
    auto best_bid = bids.empty() ? 0 : bids.begin()->first;
    auto best_ask = asks.empty() ? 0 : asks.begin()->first;
    return std::pair{best_bid, best_ask};
}

void Book::reserve(std::size_t n) { orders.reserve(n); }

} // namespace ome::book
