#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <flat_set>
#include <format>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

namespace ome::tools::lobster {

enum class Type : std::uint8_t {
    ORDER = 1,
    CANCEL = 2,
    DELETE = 3,
    EXECUTE = 4,
    EXECUTE_HIDDEN = 5,
    HALT = 7,
};

#if __cpp_lib_chrono >= 201907L
using Clock = std::chrono::gps_clock;
#else
using Clock = std::chrono::system_clock;
#endif

using MessageLine = std::size_t;
using Timestamp = std::chrono::time_point<Clock, std::chrono::nanoseconds>;
using OrderId = std::uint64_t;
using Size = std::uint64_t;
using Price = std::uint64_t;

enum class Direction : std::int8_t {
    BUY = 1,
    SELL = -1,
};

struct Message {
    std::optional<MessageLine> index;
    Timestamp timestamp;
    Type type{Type::HALT};
    OrderId order_id{};
    Size size{};
    Price price{};
    Direction direction{Direction::SELL};
};

struct Limit {
    Price price{};
    std::uint64_t quantity{};

    friend auto operator<=>(Limit const &lhs, Limit const &rhs) { return lhs.price <=> rhs.price; }
};

struct ExpectedOrderbook {
    std::flat_set<Limit, std::greater<>> bids;
    std::flat_set<Limit, std::less<>> asks;
};

class ReconstructorError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
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

struct Orphans {
    std::vector<Message> only_referenced;
    std::vector<Message> only_created;
    // created and later deleted, but the recorded reductions don't add up to the
    // created size: the difference was cancelled while the level was hidden
    std::vector<Message> partially_deleted;
};

constexpr auto LobsterDay = std::chrono::year{2012} / 6 / 21;
constexpr auto MarketOpen = std::chrono::sys_days{LobsterDay} + std::chrono::hours{4};
constexpr auto MarketClose = std::chrono::sys_days{LobsterDay} + std::chrono::hours{20};

struct PriceSnapshot {
    MessageLine line{0};
    Timestamp timestamp;
    Size size{0};
    Direction direction{Direction::BUY};
};

struct PriceView {
    PriceSnapshot entry{.timestamp = std::chrono::clock_cast<Clock>(MarketOpen)};
    PriceSnapshot exit{.timestamp = std::chrono::clock_cast<Clock>(MarketClose)};
};

class Reconstructor {
  public:
    Reconstructor(LobsterData const &data);

    [[nodiscard]] auto messages() && -> std::vector<Message>;
    [[nodiscard]] auto expected_orderbooks() && -> std::vector<ExpectedOrderbook>;

  private:
    // ordered by OrderId
    [[nodiscard]] auto orphans() const -> Orphans;
    [[nodiscard]] auto levels() const -> std::map<std::uint64_t, std::vector<PriceView>>;
    [[nodiscard]] auto initial_orderbook() const -> ExpectedOrderbook;
    [[nodiscard]] auto synthetics() const -> std::vector<Message>;
    [[nodiscard]] auto next_order_id(MessageLine) const -> OrderId;

    static auto make_order(Message const &, Timestamp) -> Message;
    static auto make_cancel(Message const &, Timestamp, Size) -> Message;

    std::filesystem::path p_messages_;
    std::filesystem::path p_orderbook_;

    std::vector<Message> messages_;
    std::vector<ExpectedOrderbook> expected_orderbooks_;
    // friend struct std::formatter<std::vector<PriceView>>;
};

} // namespace ome::tools::lobster

template <> struct std::formatter<ome::tools::lobster::Type> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::tools::lobster::Type type, FormatContext &ctx) const {
        using ome::tools::lobster::Type;
        std::string_view name = [type]() -> std::string_view {
            switch (type) {
            case Type::ORDER:
                return "ORDER";
            case Type::CANCEL:
                return "CANCEL";
            case Type::DELETE:
                return "DELETE";
            case Type::EXECUTE:
                return "EXECUTE";
            case Type::EXECUTE_HIDDEN:
                return "EXECUTE_HIDDEN";
            case Type::HALT:
                return "HALT";
            }
            std::unreachable();
        }();
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct std::formatter<ome::tools::lobster::Direction> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::tools::lobster::Direction dir, FormatContext &ctx) const {
        using ome::tools::lobster::Direction;
        std::string_view name = (dir == Direction::BUY) ? "BUY" : "SELL";
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

template <> struct std::formatter<ome::tools::lobster::Message> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::tools::lobster::Message const &msg, FormatContext &ctx) const {
        if (msg.index) {
            return std::format_to(
                ctx.out(),
                "Original{{line={:>6}, timestamp={}, type={:>7}, id={:>9}, size={:>5}, "
                "price={}, dir={:>4}}}",
                *msg.index + 1, msg.timestamp, msg.type, msg.order_id, msg.size, msg.price,
                msg.direction);
        }
        return std::format_to(
            ctx.out(),
            "Synthetic{{timestamp={}, type={:>7}, id={:>9}, size={:>5}, price={}, dir={:>4}}}",
            msg.timestamp, msg.type, msg.order_id, msg.size, msg.price, msg.direction);
    }
};

template <> struct std::formatter<ome::tools::lobster::Limit> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::tools::lobster::Limit const &limit, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{}@{}", limit.quantity, limit.price);
    }
};

template <>
struct std::formatter<ome::tools::lobster::PriceSnapshot> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::tools::lobster::PriceSnapshot const &snapshot, FormatContext &ctx) const {
        return std::format_to(ctx.out(),
                              "Snapshot{{line={:>6}, timestamp={}, size={:>5}, direction={:>4}}}",
                              snapshot.line, snapshot.timestamp, snapshot.size, snapshot.direction);
    }
};

template <>
struct std::formatter<ome::tools::lobster::PriceView> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(ome::tools::lobster::PriceView const &pv, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{} -> {}", pv.entry, pv.exit);
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
