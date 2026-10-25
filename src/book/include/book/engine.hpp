#pragma once

#include <book/books/node/book.hpp>
#include <book/order.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace ome::book {

template <typename T> using expected = std::expected<T, std::string_view>;

/// Parameters to modify an order currently in the book.
///
/// Only decreasing qty at the same price retains price-time priority. Any other change re-adds
/// the order at the back of its new price level, matching it first if it crosses the spread.
struct ModifyOps {
    /// new price, if any
    std::optional<Price> price = std::nullopt;
    /// new total quantity of the order, if any; must be positive, use cancel to remove the order
    std::optional<std::uint64_t> qty = std::nullopt;
};

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

    /// Modifies order by id, keeping its id. See ModifyOps for the effect on priority.
    /// time: O(# orders)
    ///
    /// \param on_fill called for the fills of an order moved across the spread
    /// \return error if the order was not found, the quantity is zero or nothing would change.
    auto modify(OrderId, ModifyOps, OrderCallback on_fill = default_callback) -> expected<void>;

    /// Returns total volume at given price level.
    /// time: O(log # price levels)
    [[nodiscard]] auto volume(Price) const -> expected<std::uint64_t>;

    /// Returns best bid and ask prices.
    /// time: O(1)
    [[nodiscard]] auto spread() const -> expected<std::pair<Price, Price>>;

  private:
    void add(RestingOrder order, OrderCallback on_fill);

    /// Matches order against the opposite side's levels, best price first.
    template <Side> void execute(Order &order, OrderCallback on_fill);

    books::node::Book book;
    OrderId cur_order_id_{0};
};

} // namespace ome::book
