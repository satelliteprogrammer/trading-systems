#pragma once

#include <book/books/node/indexedbook.hpp>
#include <book/concepts.hpp>
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

/// Matching engine over an order book. Only the books in books::All are instantiated (in
/// engine.cpp); any other book fails to link.
template <concepts::OrderBook Book> class BasicEngine {
  public:
    using book_type = Book;

    /// Called once per fill with the id of the resting order, the resting order itself,
    /// the execution price and the executed quantity.
#if __cpp_lib_function_ref >= 202306L
    using OrderCallback = std::function_ref<void(OrderId, Order const &, Price, std::uint64_t)>;
#else
    using OrderCallback = std::function<void(OrderId, Order const &, Price, std::uint64_t)> const &;
#endif
    struct IgnoreFills {
        constexpr void operator()(OrderId, Order const &, Price, std::uint64_t) const noexcept {}
    };
    static constexpr IgnoreFills default_callback{};

    static constexpr std::size_t default_max_orders = 1'000'000;

    /// \param max_orders exclusive bound on order ids, which start at 1 and are never reused; sizes
    /// the book.
    explicit BasicEngine(std::size_t max_orders = default_max_orders);

    BasicEngine(BasicEngine const &) = delete;
    auto operator=(BasicEngine const &) -> BasicEngine & = delete;
    BasicEngine(BasicEngine &&) = delete;
    auto operator=(BasicEngine &&) -> BasicEngine & = delete;
    ~BasicEngine() = default;

    /// Adds order to the book, matching it first against the opposite side.
    ///
    /// time: O(log # price levels) for first order at limit, O(1) for all others
    ///
    /// \return id assigned to the order (sequential, starting at 1, never reused)
    auto add(Order, OrderCallback on_fill = default_callback) -> expected<OrderId>;

    /// Cancels order by id.
    /// time: amortized O(1)
    ///
    /// \return error if the order was not found.
    auto cancel(OrderId) -> expected<void>;

    /// Modifies order by id, keeping its id. See ModifyOps for the effect on priority.
    /// time: amortized O(1) when shrinking in place, as add otherwise
    ///
    /// \param on_fill called for the fills of an order moved across the spread
    /// \return error if the order was not found, the quantity is zero or nothing would change.
    auto modify(OrderId, ModifyOps, OrderCallback on_fill = default_callback) -> expected<void>;

    /// Returns total volume at given price level.
    /// time: O(1)
    [[nodiscard]] auto volume(Price) const -> expected<std::uint64_t>;

    /// Returns best bid and ask prices.
    /// time: O(1)
    [[nodiscard]] auto spread() const -> expected<std::pair<Price, Price>>;

  private:
    using level_iterator = Book::level_iterator;

    void add(RestingOrder order, OrderCallback on_fill);

    /// Matches order against the opposite side's levels, best price first.
    template <Side> void execute(Order &order, OrderCallback on_fill);

    Book book;
    OrderId cur_order_id_{0};
};

/// The engine used in production.
using Engine = BasicEngine<books::node::IndexedBook>;

} // namespace ome::book
