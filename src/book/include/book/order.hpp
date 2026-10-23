#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <string_view>

// TODO: research alignment and packing
namespace ome::book {

using Price = std::uint64_t;

using OrderId = std::uint64_t;
using Timestamp = std::chrono::sys_time<std::chrono::nanoseconds>;

struct LimitOrder {
    std::uint64_t quantity{};
};

enum class Side : std::uint8_t { Buy, Sell };

struct Order : LimitOrder {
    Side side{};
    Price price{};
    Timestamp timestamp{}; // NOLINT(readability-redundant-member-init) for designatated initializer
    std::array<char, 5> trader_id{}; // NOLINT(readability-magic-numbers)
};

struct RestingOrder : Order {
    OrderId order_id{};
};

} // namespace ome::book

template <> struct std::formatter<ome::book::LimitOrder> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::LimitOrder const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "qty={}", order.quantity);
    }
};

template <> struct std::formatter<ome::book::Side> : std::formatter<std::string_view> {
    template <typename FormatContext> auto format(ome::book::Side side, FormatContext &ctx) const {
        return std::formatter<std::string_view>::format(
            side == ome::book::Side::Buy ? "BUY" : "SELL", ctx);
    }
};

template <> struct std::formatter<ome::book::Order> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::Order const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{} {} price={} timestamp={}", order.side,
                              std::format("{}", static_cast<const ome::book::LimitOrder &>(order)),
                              order.price, order.timestamp);
    }
};

template <> struct std::formatter<ome::book::RestingOrder> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::book::RestingOrder const &order, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "id={} {}", order.order_id,
                              static_cast<const ome::book::Order &>(order));
    }
};
