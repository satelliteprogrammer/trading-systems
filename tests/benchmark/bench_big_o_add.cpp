#include "book/lob.hpp"

#include <lobster/reconstructor.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <stdexcept>
#include <variant>

using namespace std::chrono_literals;

namespace chrono = std::chrono;
namespace fs = std::filesystem;

// helper for std::visit with lambdas
template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};

namespace ome::testing {

struct NewOrder {
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

class Bench {
  public:
    Bench(fs::path message_file, fs::path orderbook_file)
        : message_file_(std::move(message_file)), orderbook_file_(std::move(orderbook_file)) {
        if (!fs::is_regular_file(message_file_)) {
            throw std::runtime_error{std::format("{} does not exist", message_file_.string())};
        }
        if (!fs::is_regular_file(orderbook_file_)) {
            throw std::runtime_error{std::format("{} does not exist", orderbook_file_.string())};
        }
    }

    void init() {
        std::println("Initializing reconstructor with LOBSTER data ...");
        reconstructor_.emplace(tools::lobster::LobsterData{message_file_, orderbook_file_});

        std::println("Reconstructor initialized. Loading messages ...");

        messages_ = std::move(*reconstructor_).messages();
        message_index_ = 0;
        expected_orderbooks_ = std::move(*reconstructor_).expected_orderbooks();
        reconstructor_.reset();

        std::println("Messages loaded.");
    }

    auto next() -> std::optional<Trade> {
        if (message_index_ >= messages_.size()) {
            return std::nullopt;
        }
        auto msg = messages_[message_index_++];
        return message_to_trade(msg);
    }

    auto validate(book::Book const &book) -> ValidationResult {
        auto previous_message = messages_[message_index_ - 1];
        auto idx = previous_message.index;
        if (!idx) {
            // last message was synthetic, no expected orderbook line to compare against
            return {.valid = true};
        }

        for (auto const &expected_bid : expected_orderbooks_[*idx].bids) {
            if (auto actual_qty = book.volume(expected_bid.price);
                actual_qty != expected_bid.quantity) {
                tools::lobster::Limit actual{.price = expected_bid.price,
                                             .quantity = actual_qty.value()};
                return {
                    .valid = false,
                    .message = previous_message,
                    .expected_direction = tools::lobster::Direction::BUY,
                    .expected = expected_bid,
                    .actual = actual,
                };
            }
        }

        for (auto const &expected_ask : expected_orderbooks_[*idx].asks) {
            if (auto actual_qty = book.volume(expected_ask.price);
                actual_qty != expected_ask.quantity) {
                tools::lobster::Limit actual{.price = expected_ask.price,
                                             .quantity = actual_qty.value()};
                return {
                    .valid = false,
                    .message = previous_message,
                    .expected_direction = tools::lobster::Direction::SELL,
                    .expected = expected_ask,
                    .actual = actual,
                };
            }
        }

        return {.valid = true};
    }

    auto operator()(std::size_t id, auto const &func, auto const &...args) -> auto {
        auto start = chrono::high_resolution_clock::now();
        auto ans = std::invoke(func, args...);
        auto stop = chrono::high_resolution_clock::now();

        auto delta = stop - start;
        std::println(results_, "{},{}", id, delta.count());

        return ans;
    }

    static auto message_to_trade(tools::lobster::Message const &msg) -> Trade {
        switch (msg.type) {
        case tools::lobster::Type::ORDER:
            switch (msg.direction) {
            case tools::lobster::Direction::BUY:
                return NewOrder{book::BuyOrder{{msg.order_id, msg.price, msg.size, msg.timestamp}}};
            case tools::lobster::Direction::SELL:
                return NewOrder{
                    book::SellOrder{{msg.order_id, msg.price, msg.size, msg.timestamp}}};
            default:
                throw std::runtime_error{
                    std::format("Unknown direction type: {}", static_cast<int>(msg.direction))};
            }
        case tools::lobster::Type::CANCEL:
            return PartialCancel{.order_id = msg.order_id,
                                 .price = msg.price,
                                 .quantity = msg.size,
                                 .timestamp = msg.timestamp};
        case tools::lobster::Type::DELETE:
            return Cancel{.order_id = msg.order_id,
                          .price = msg.price,
                          .quantity = msg.size,
                          .timestamp = msg.timestamp};

        case tools::lobster::Type::EXECUTE:
            switch (msg.direction) {
            case tools::lobster::Direction::BUY:
                return Execute{
                    .order = book::SellOrder{{msg.order_id, msg.price, msg.size, msg.timestamp}},
                    .hidden = false};
            case tools::lobster::Direction::SELL:
                return Execute{
                    .order = book::BuyOrder{{msg.order_id, msg.price, msg.size, msg.timestamp}},
                    .hidden = false};
            default:
                throw std::runtime_error{
                    std::format("Unknown direction type: {}", static_cast<int>(msg.direction))};
            }

        case tools::lobster::Type::EXECUTE_HIDDEN:
            switch (msg.direction) {
            case tools::lobster::Direction::BUY:
                return Execute{
                    .order = book::BuyOrder{{msg.order_id, msg.price, msg.size, msg.timestamp}},
                    .hidden = true};
            case tools::lobster::Direction::SELL:
                return Execute{
                    .order = book::SellOrder{{msg.order_id, msg.price, msg.size, msg.timestamp}},
                    .hidden = true};
            }

        case tools::lobster::Type::HALT:
            return Halt{.timestamp = msg.timestamp};
        }

        std::unreachable();
    }

  private:
    fs::path orderbook_file_;
    fs::path message_file_;
    std::ofstream results_{"results.csv"};

    std::optional<tools::lobster::Reconstructor> reconstructor_;
    std::vector<tools::lobster::Message> messages_;
    std::size_t message_index_{0};
    std::vector<tools::lobster::ExpectedOrderbook> expected_orderbooks_;
};

} // namespace ome::testing

// NOLINTBEGIN(readability-convert-member-functions-to-static)
template <> struct std::formatter<ome::testing::NewOrder> {
    constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

    auto format(ome::testing::NewOrder const &order, std::format_context &ctx) const {
        return std::visit(
            [&ctx](auto const &ord) -> auto {
                using T = std::decay_t<decltype(ord)>;
                constexpr auto side = std::is_same_v<T, ome::book::BuyOrder> ? "BUY" : "SELL";
                return std::format_to(ctx.out(), "NewOrder({} id={} price={} qty={})", side,
                                      ord.order_id, ord.price, ord.quantity);
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
                                      exec.hidden ? "HIDDEN " : "", side, ord.order_id, ord.price,
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

auto main(int argc, char *argv[]) -> int {
    using namespace ome::testing;

    if (argc != 3) {
        std::println(std::cerr, "Usage: {} <messages-file> <orderbook-file>", argv[0]);
        return EXIT_FAILURE;
    }

    try {
        Bench bench(fs::path{argv[1]}, fs::path{argv[2]});
        bench.init();
        ome::book::Book book;

        std::size_t trade_id = 0;
        while (auto trade = bench.next()) {
            ++trade_id;

            auto success = std::visit(
                overloaded{
                    [&](NewOrder const &new_order) -> std::expected<void, std::string_view> {
                        return std::visit(
                            [&](auto const &order) -> auto {
                                return bench(trade->index(),
                                             [&] -> auto { return book.add(order); });
                            },
                            new_order.order);
                    },
                    [&](PartialCancel const &partial_cancel)
                        -> std::expected<void, std::string_view> {
                        return bench(trade->index(), [&] -> auto {
                            return book.cancel(partial_cancel.order_id, partial_cancel.quantity);
                        });
                    },
                    [&](Cancel const &cancel) -> std::expected<void, std::string_view> {
                        return bench(trade->index(),
                                     [&] -> auto { return book.cancel(cancel.order_id); });
                    },
                    [&](Execute const &execute) -> std::expected<void, std::string_view> {
                        if (execute.hidden) {
                            // Hidden executions don't affect the visible order book
                            return {};
                        }
                        // LOBSTER EXECUTE messages tell us which resting order was
                        // executed. Use cancel to reduce that specific order.
                        return std::visit(
                            [&](auto const &order) -> auto {
                                return bench(trade->index(), [&] -> auto {
                                    return book.cancel(order.order_id, order.quantity);
                                });
                            },
                            execute.order);
                    },
                    [&](Halt const &) -> std::expected<void, std::string_view> { return {}; }},
                *trade);

            if (!success) {
                // static std::ofstream errors{"errors.log"};
                // std::println(errors, "{}: {} failed", trade_id, *trade);
                std::println("{}: error \"{}\", trade={} failed", trade_id, success.error(),
                             *trade);
                return EXIT_FAILURE;
            }

            // Validate book state against expected orderbook
            auto validation = bench.validate(book);
            if (!validation.valid) {
                std::println(std::cerr, "Validation failed at message {}: {}\n{}", trade_id, *trade,
                             validation);
                return EXIT_FAILURE;
            }
        }

        std::println("All {} messages validated successfully", trade_id);
    } catch (std::runtime_error &e) {
        std::println(std::cerr, "{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
