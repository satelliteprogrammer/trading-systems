#pragma once

#include <lobster/types.hpp>

#include <cstddef>
#include <filesystem>
#include <flat_set>
#include <format>
#include <string_view>
#include <vector>

namespace ome::tools::lobster {

struct Limit {
    Price price{};
    std::uint64_t quantity{};

    friend auto operator<=>(Limit const &lhs, Limit const &rhs) { return lhs.price <=> rhs.price; }
};

struct ExpectedOrderbook {
    std::flat_set<Limit, std::greater<>> bids;
    std::flat_set<Limit, std::less<>> asks;
};

class LobsterData {
  public:
    LobsterData(std::filesystem::path const &messages, std::filesystem::path const &orderbook);

  private:
    std::filesystem::path messages;
    std::filesystem::path orderbook;
    std::size_t line_count{0};

    friend class Reconstructor;
};

class Reconstructor {
  public:
    Reconstructor(LobsterData const &data);

    [[nodiscard]] auto messages() && -> Messages;
    [[nodiscard]] auto expected_orderbooks() && -> std::vector<ExpectedOrderbook>;

  private:
    // ordered by OrderId
    [[nodiscard]] auto orphans() const -> Orphans;
    [[nodiscard]] auto levels() const -> Levels;
    [[nodiscard]] auto initial_orderbook() const -> ExpectedOrderbook;
    [[nodiscard]] auto synthetics() const -> Messages;
    [[nodiscard]] auto next_order_id(MessageLine) const -> OrderId;

    std::filesystem::path p_messages_;
    std::filesystem::path p_orderbook_;

    Messages messages_;
    std::vector<ExpectedOrderbook> expected_orderbooks_;
};

} // namespace ome::tools::lobster

template <> struct std::formatter<ome::tools::lobster::Limit> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::tools::lobster::Limit const &limit, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{}@{}", limit.quantity, limit.price);
    }
};

template <>
struct std::formatter<ome::tools::lobster::ExpectedOrderbook> : std::formatter<std::string_view> {
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    template <typename FormatContext>
    auto format(ome::tools::lobster::ExpectedOrderbook const &ob, FormatContext &ctx) const {
        std::format_to(ctx.out(), "Orderbook{{bids=[");
        for (auto it = ob.bids.begin(); it != ob.bids.end(); ++it) {
            if (it != ob.bids.begin()) {
                std::format_to(ctx.out(), ", ");
            }
            std::format_to(ctx.out(), "{}", *it);
        }
        std::format_to(ctx.out(), "], asks=[");
        for (auto it = ob.asks.begin(); it != ob.asks.end(); ++it) {
            if (it != ob.asks.begin()) {
                std::format_to(ctx.out(), ", ");
            }
            std::format_to(ctx.out(), "{}", *it);
        }
        return std::format_to(ctx.out(), "]}}");
    }
};
