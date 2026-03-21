#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <list>
#include <map>
#include <string_view>
#include <unordered_map>

// TODO: research alignment and packing
namespace ome::book {

using Price = std::uint64_t;

struct Limit {
    Price price{};
    std::uint64_t quantity{};
};

using OrderId = std::uint64_t;
#if __cpp_lib_chrono >= 201907L
using Timestamp = std::chrono::gps_time<std::chrono::nanoseconds>;
#else
using Timestamp = std::chrono::sys_time<std::chrono::nanoseconds>;
#endif

struct Order {
    OrderId order_id{};
    Limit limit;
    Timestamp timestamp;
    // TODO: timestamp when the order was created
};

struct BuyOrder : Order {};
struct SellOrder : Order {};

class Book {
  public:
    /// Adds buy order to book. If it can be matched, it will be executed immediately.
    /// time: O(bids.size()) for first order at limit, O(1) for all others
    ///
    /// \return true if order was added to the book
    auto add(BuyOrder) -> std::expected<void, std::string_view>;

    /// Adds sell order to book. If it can be matched, it will be executed immediately.
    /// time: O(asks.size()) for first order at limit, O(1) for all others
    ///
    /// \return true if order was added to the book
    auto add(SellOrder) -> std::expected<void, std::string_view>;

    /// Cancels order by id. Returns true if order was found and cancelled.
    /// time: amortized O(1)
    /// @warning weak_ptr to the order will remain in the order book until it is cleaned up
    auto cancel(OrderId) -> std::expected<void, std::string_view>;

    /// Returns total volume at given price level.
    /// time: O(1)
    auto volume(Price) const -> std::expected<std::uint64_t, std::string_view>;

    /// Returns best bid and ask prices.
    /// time: O(1)
    auto spread() const -> std::expected<std::pair<Price, Price>, std::string_view>;

    /// Reserves space for n orders to avoid rehashing.
    void reserve(std::size_t n);

  private:
    auto execute(BuyOrder order) -> BuyOrder;
    auto execute(SellOrder order) -> SellOrder;

    struct OrdersAtLimit {
        Limit limit;
        std::uint64_t total_quantity{0};
        std::list<std::weak_ptr<Order>> orders;
    };

    std::map<Price, OrdersAtLimit, std::greater<>> bids;
    std::map<Price, OrdersAtLimit, std::less<>> asks;

    std::unordered_map<std::uint64_t, std::shared_ptr<Order>> orders;
};

} // namespace ome::book
