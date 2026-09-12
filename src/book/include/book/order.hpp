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
