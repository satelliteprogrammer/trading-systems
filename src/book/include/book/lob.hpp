#pragma once

#include "order.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <list>
#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ome::book {

template <typename T> using expected = std::expected<T, std::string_view>;

class Engine {
  public:
    /// Called once per fill with the id of the resting order, the resting order itself,
    /// the execution price and the executed quantity.
#if __cpp_lib_function_ref >= 202306L
    using OrderCallback = std::function_ref<void(OrderId, Order const &, Price, std::uint64_t)>;
#else
    using OrderCallback = std::function<void(OrderId, Order const &, Price, std::uint64_t)> const &;
#endif
    static constexpr auto default_callback = [](OrderId, Order const &, Price,
                                                std::uint64_t) -> void {};

    static constexpr std::size_t default_max_orders = 1'000'000;

    /// \param max_orders bounds the total number of orders added over the book's lifetime,
    /// not just concurrently live ones: ids are assigned sequentially, never reused, and
    /// index directly into a preallocated arena.
    explicit Engine(std::size_t max_orders = default_max_orders);

    /// Adds order to the book.
    ///
    /// time: O(# price levels) for first order at limit, O(1) for all others
    ///
    /// \return id assigned to the order (sequential, starting at 0)
    auto add(Order, OrderCallback on_fill = default_callback) -> expected<OrderId>;

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
    /// Matches order against the opposite side's levels, best price first.
    template <typename Levels> void execute(Order &order, Levels &opposite, OrderCallback on_fill);

    /// Rests what remains of order on its own side's levels.
    template <typename Levels> void rest(Order const &order, OrderId id, Levels &same);

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
