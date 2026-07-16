#pragma once

#include "book/lob.hpp"

#include <lobster/reconstructor.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ome::testing {

// helper for std::visit with lambdas
template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};

struct NewOrder {
    book::OrderId order_id{}; // external (exchange-assigned) id
    std::variant<book::BuyOrder, book::SellOrder> order;
};

struct PartialCancel {
    book::OrderId order_id{};
    book::Price price{};
    std::uint64_t quantity{};
    book::Timestamp timestamp;
};

struct Cancel {
    book::OrderId order_id{};
    book::Price price{};
    std::uint64_t quantity{};
    book::Timestamp timestamp;
};

struct Execute {
    book::OrderId order_id{}; // external (exchange-assigned) id
    std::variant<book::BuyOrder, book::SellOrder> order;
    bool hidden{false};
};

struct Halt {
    book::Timestamp timestamp;
};

using Trade = std::variant<NewOrder, PartialCancel, Cancel, Execute, Halt>;

struct ValidationResult {
    bool valid{true};
    tools::lobster::Message message;
    tools::lobster::Direction expected_direction;
    tools::lobster::Limit expected;
    tools::lobster::Limit actual;
};

auto message_to_trade(tools::lobster::Message const &msg) -> Trade;

// Replays the reconstructed LOBSTER stream into a Book. LOBSTER ids are
// sparse exchange-assigned reference numbers; the book assigns its own dense
// ids, so they are translated at the boundary. Lookups happen outside the
// instrumented calls so only book operations are measured by callers timing
// them.
class LobsterReplay {
  public:
    LobsterReplay(std::filesystem::path const &message_file,
                  std::filesystem::path const &orderbook_file);

    auto next() -> std::optional<Trade>;

    // Checks the book against the expected orderbook line of the last
    // replayed message; synthetic messages have no line to compare against.
    [[nodiscard]] auto validate(book::Book const &book) const -> ValidationResult;

    // Applies the trade to the book, wrapping every book operation in
    // run(op, call) where op is the Trade variant index, so callers can
    // instrument the calls.
    template <typename Run>
    auto apply(Trade const &trade, book::Book &book, Run &&run)
        -> std::expected<void, std::string_view> {
        auto const op = trade.index();
        return std::visit(
            overloaded{
                [&](NewOrder const &new_order) -> std::expected<void, std::string_view> {
                    return std::visit(
                        [&](auto const &order) -> std::expected<void, std::string_view> {
                            auto id = run(op, [&] -> auto { return book.add(order); });
                            if (!id) {
                                return std::unexpected{id.error()};
                            }
                            id_map_[new_order.order_id] = *id;
                            return {};
                        },
                        new_order.order);
                },
                [&](PartialCancel const &partial_cancel) -> std::expected<void, std::string_view> {
                    return book_id(partial_cancel.order_id).and_then([&](auto id) -> auto {
                        return run(op,
                                   [&] -> auto { return book.cancel(id, partial_cancel.quantity); });
                    });
                },
                [&](Cancel const &cancel) -> std::expected<void, std::string_view> {
                    return book_id(cancel.order_id).and_then([&](auto id) -> auto {
                        return run(op, [&] -> auto { return book.cancel(id); });
                    });
                },
                [&](Execute const &execute) -> std::expected<void, std::string_view> {
                    if (execute.hidden) {
                        // Hidden executions don't affect the visible order book
                        return {};
                    }
                    // LOBSTER EXECUTE messages tell us which resting order was
                    // executed. Use cancel to reduce that specific order.
                    return std::visit(
                        [&](auto const &order) -> std::expected<void, std::string_view> {
                            return book_id(execute.order_id).and_then([&](auto id) -> auto {
                                return run(op, [&] -> auto {
                                    return book.cancel(id, order.quantity);
                                });
                            });
                        },
                        execute.order);
                },
                [&](Halt const &) -> std::expected<void, std::string_view> { return {}; }},
            trade);
    }

  private:
    [[nodiscard]] auto book_id(book::OrderId external_id) const
        -> std::expected<book::OrderId, std::string_view>;

    std::vector<tools::lobster::Message> messages_;
    std::size_t message_index_{0};
    std::vector<tools::lobster::ExpectedOrderbook> expected_orderbooks_;
    std::unordered_map<book::OrderId, book::OrderId> id_map_;
};

} // namespace ome::testing

// NOLINTBEGIN(readability-convert-member-functions-to-static)
template <> struct std::formatter<ome::testing::NewOrder> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::NewOrder const &order, std::format_context &ctx) const {
        return std::visit(
            [&ctx, &order](auto const &ord) -> auto {
                using T = std::decay_t<decltype(ord)>;
                constexpr auto side = std::is_same_v<T, ome::book::BuyOrder> ? "BUY" : "SELL";
                return std::format_to(ctx.out(), "NewOrder({} id={} price={} qty={})", side,
                                      order.order_id, ord.price, ord.quantity);
            },
            order.order);
    }
};

template <> struct std::formatter<ome::testing::PartialCancel> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::PartialCancel const &cancel, std::format_context &ctx) const {
        return std::format_to(ctx.out(), "PartialCancel(id={} price={} qty={})", cancel.order_id,
                              cancel.price, cancel.quantity);
    }
};

template <> struct std::formatter<ome::testing::Cancel> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::Cancel const &cancel, std::format_context &ctx) const {
        return std::format_to(ctx.out(), "Cancel(id={} price={} qty={})", cancel.order_id,
                              cancel.price, cancel.quantity);
    }
};

template <> struct std::formatter<ome::testing::Execute> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::Execute const &exec, std::format_context &ctx) const {
        return std::visit(
            [&ctx, &exec](auto const &ord) -> auto {
                using T = std::decay_t<decltype(ord)>;
                constexpr auto side = std::is_same_v<T, ome::book::BuyOrder> ? "BUY" : "SELL";
                return std::format_to(ctx.out(), "Execute({}{} id={} price={} qty={})",
                                      exec.hidden ? "HIDDEN " : "", side, exec.order_id, ord.price,
                                      ord.quantity);
            },
            exec.order);
    }
};

template <> struct std::formatter<ome::testing::Halt> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::Halt const & /*halt*/, std::format_context &ctx) const {
        return std::format_to(ctx.out(), "Halt");
    }
};

template <> struct std::formatter<ome::testing::Trade> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::Trade const &trade, std::format_context &ctx) const {
        return std::visit([&ctx](auto const &val) { return std::format_to(ctx.out(), "{}", val); },
                          trade);
    }
};

template <> struct std::formatter<ome::testing::ValidationResult> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::ValidationResult const &result, std::format_context &ctx) const {
        if (result.valid) {
            return std::format_to(ctx.out(), "VALID");
        }
        return std::format_to(ctx.out(), "FAILED @{} ({}) expected {}, actual {}\nmessage={}",
                              result.expected.price, result.expected_direction,
                              result.expected.quantity, result.actual.quantity, result.message);
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)
