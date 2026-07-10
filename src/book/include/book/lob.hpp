#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <list>
#include <map>
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
    std::uint64_t quantity{};
};

struct Order : LimitOrder {
    Timestamp timestamp;
    std::array<char, 5> trader_id{}; // NOLINT(readability-magic-numbers)
};

struct BuyOrder : Order {
    Price price{};
};
struct SellOrder : Order {
    Price price{};
};

template <typename T> using expected = std::expected<T, std::string_view>;

class Book {
  public:
    /// Called once per fill with the id of the resting order, the resting order itself,
    /// the execution price and the executed quantity.
    using OrderCallback = std::function_ref<void(OrderId, Order const &, Price, std::uint64_t)>;
    static constexpr auto default_callback = [](OrderId, Order const &, Price,
                                                std::uint64_t) -> void {};

    static constexpr std::size_t default_max_orders = 1'000'000;

    /// \param max_orders bounds the total number of orders added over the book's lifetime,
    /// not just concurrently live ones: ids are assigned sequentially, never reused, and
    /// index directly into a preallocated arena.
    explicit Book(std::size_t max_orders = default_max_orders);

    /// Adds buy order to book.
    ///
    /// time: O(bids.size()) for first order at limit, O(1) for all others
    ///
    /// \return id assigned to the order (sequential, starting at 0)
    auto add(BuyOrder, OrderCallback on_fill = default_callback) -> expected<OrderId>;

    /// Adds sell order to book.
    ///
    /// time: O(asks.size()) for first order at limit, O(1) for all others
    ///
    /// \return id assigned to the order (sequential, starting at 0)
    auto add(SellOrder, OrderCallback on_fill = default_callback) -> expected<OrderId>;

    /// Cancels order by id. Returns true if order was found and cancelled.
    /// time: amortized O(1)
    /// @warning weak_ptr to the order will remain in the order book until it is cleaned up
    auto cancel(OrderId) -> expected<void>;

    /// Cancels shares of order by id.
    ///
    /// \return true if order was found and cancelled some or all of the shares.
    /// \return false if order was not found.
    auto cancel(OrderId, std::uint64_t) -> expected<void>;

    /// Returns total volume at given price level.
    /// time: O(1)
    auto volume(Price) const -> expected<std::uint64_t>;

    /// Returns best bid and ask prices.
    /// time: O(1)
    auto spread() const -> expected<std::pair<Price, Price>>;

  private:
    void execute(BuyOrder &order, OrderCallback on_fill);
    void execute(SellOrder &order, OrderCallback on_fill);

    /// Order resting on the book, tagged with its book-assigned id.
    struct RestingOrder : Order {
        OrderId order_id{};
    };

    struct OrdersAtLimit {
        // Limit limit;
        std::list<RestingOrder> orders;
        std::uint64_t total_quantity{0};
    };

    std::map<Price, OrdersAtLimit, std::greater<>> bids;
    std::map<Price, OrdersAtLimit, std::less<>> asks;

    struct OrderAtLimit {
        Price price{};
        std::list<RestingOrder>::iterator order_it;
    };

    std::vector<OrderAtLimit> orders; // arena, index is order_id

    OrderId next_id{0};
};

} // namespace ome::book

template <> struct std::formatter<ome::book::LimitOrder> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::LimitOrder const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "qty={}", order.quantity);
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
        return std::format_to(ctx.out(), "BUY {} price={}",
                              std::format("{}", static_cast<const ome::book::LimitOrder &>(order)),
                              order.price);
    }
};

template <> struct std::formatter<ome::book::SellOrder> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::SellOrder const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "SELL {} price={}",
                              std::format("{}", static_cast<const ome::book::LimitOrder &>(order)),
                              order.price);
    }
};
