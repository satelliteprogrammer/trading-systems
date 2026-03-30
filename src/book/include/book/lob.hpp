#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

// TODO: research alignment and packing
namespace ome::book {

using Price = std::uint64_t;

using OrderId = std::uint64_t;
#if __cpp_lib_chrono >= 201907L
using Timestamp = std::chrono::gps_time<std::chrono::nanoseconds>;
#else
using Timestamp = std::chrono::sys_time<std::chrono::nanoseconds>;
#endif

struct LimitOrder {
    OrderId order_id{};
    Price price{};
    std::uint64_t quantity{};
};

struct Order : LimitOrder {
    Timestamp timestamp;
    std::string trader_id;
};

struct BuyOrder : Order {};
struct SellOrder : Order {};

struct OrderFilled : Order {
    bool fully_filled{};
};

using OrdersFilled = std::vector<OrderFilled>;

struct OrderFulfilled {
    OrdersFilled orders_filled;
    std::optional<Order> unfulfilled_order;
};

template <typename T> using expected = std::expected<T, std::string_view>;

class Book {
  public:
    /// Adds buy order to book.
    ///
    /// time: O(bids.size()) for first order at limit, O(1) for all others
    ///
    /// \return true if order was added to the book
    auto add(BuyOrder, bool execute = true, std::function<void(Order)> const &on_fill = {})
        -> expected<void>;

    /// Adds sell order to book.
    ///
    /// time: O(asks.size()) for first order at limit, O(1) for all others
    ///
    /// \return true if order was added to the book
    auto add(SellOrder, bool execute = true, std::function<void(Order)> const &on_fill = {})
        -> expected<void>;

    /// Cancels order by id. Returns true if order was found and cancelled.
    /// time: amortized O(1)
    /// @warning weak_ptr to the order will remain in the order book until it is cleaned up
    auto cancel(OrderId) -> expected<void>;

    /// Cancels shares of order by id.
    ///
    /// \return true if order was found and cancelled some or all of the shares.
    /// \return false if order was not found.
    auto cancel(OrderId, std::uint64_t) -> expected<void>;

    /// Executes buy order against the book up to its limit price.
    ///
    /// time: O(asks.orders.size()) in worst case
    ///
    /// \return list of fills produced (may be empty if no crossing).
    auto execute(BuyOrder order, bool execute = true,
                 std::function<void(Order)> const &on_fill = {}) -> expected<std::optional<Order>>;

    /// Executes sell order against the book up to its limit price.
    ///
    /// time: O(bids.orders.size()) in worst case
    ///
    /// \return list of fills produced (may be empty if no crossing).
    auto execute(SellOrder order, bool execute = true,
                 std::function<void(Order)> const &on_fill = {}) -> expected<std::optional<Order>>;

    /// Returns total volume at given price level.
    /// time: O(1)
    auto volume(Price) const -> expected<std::uint64_t>;

    /// Returns best bid and ask prices.
    /// time: O(1)
    auto spread() const -> expected<std::pair<Price, Price>>;

    /// Reserves space for n orders to avoid rehashing.
    void reserve(std::size_t n);

  private:
    struct OrdersAtLimit {
        // Limit limit;
        std::list<Order> orders;
        std::uint64_t total_quantity{0};
    };

    std::map<Price, OrdersAtLimit, std::greater<>> bids;
    std::map<Price, OrdersAtLimit, std::less<>> asks;

    struct OrderAtLimit {
        Price price{};
        std::list<Order>::iterator order_it;
    };

    std::unordered_map<OrderId, OrderAtLimit> orders;
};

} // namespace ome::book

template <> struct std::formatter<ome::book::LimitOrder> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::LimitOrder const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "id={} price={} qty={}", order.order_id, order.price,
                              order.quantity);
    }
};

template <> struct std::formatter<ome::book::Order> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::Order const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{} timestamp={}",
                              std::format("{}", static_cast<const ome::book::LimitOrder &>(order)),
                              order.timestamp);
    }
};

template <> struct std::formatter<ome::book::BuyOrder> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::BuyOrder const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "BUY {}",
                              std::format("{}", static_cast<const ome::book::LimitOrder &>(order)));
    }
};

template <> struct std::formatter<ome::book::SellOrder> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::SellOrder const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "SELL {}",
                              std::format("{}", static_cast<const ome::book::LimitOrder &>(order)));
    }
};

template <> struct std::formatter<ome::book::OrderFilled> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::OrderFilled const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{} fully_filled={}",
                              std::format("{}", static_cast<const ome::book::LimitOrder &>(order)),
                              order.fully_filled);
    }
};
