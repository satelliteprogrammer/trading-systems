#include "lobster/reconstructor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <random>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace ome::tools::lobster {

namespace {

// NOLINTBEGIN(readability-magic-numbers)

constexpr auto open_timestamp() -> Timestamp {
    return std::chrono::clock_cast<Clock>(MarketOpen);
}

auto msg(double time, Type type, OrderId id, Size size, Price price, Direction dir)
    -> std::string {
    return std::format("{:.9f},{},{},{},{},{}", time, std::to_underlying(type), id, size, price,
                       std::to_underlying(dir));
}

struct Level {
    Price price;
    Size size;
};

// LOBSTER orderbook line: ask1,size,bid1,size,ask2,size,bid2,size,...
// zero-size levels are ignored by the parser and pad the shallower side
auto book(std::vector<Level> const &asks, std::vector<Level> const &bids) -> std::string {
    std::string line;
    for (std::size_t i = 0; i < std::max(asks.size(), bids.size()); ++i) {
        auto ask = i < asks.size() ? asks[i] : Level{};
        auto bid = i < bids.size() ? bids[i] : Level{};
        std::format_to(std::back_inserter(line), "{}{},{},{},{}", i == 0 ? "" : ",", ask.price,
                       ask.size, bid.price, bid.size);
    }
    return line;
}

struct TempLobsterFiles {
    std::filesystem::path dir;
    std::filesystem::path messages;
    std::filesystem::path orderbook;

    TempLobsterFiles(std::vector<std::string> const &message_lines,
                     std::vector<std::string> const &orderbook_lines) {
        dir = std::filesystem::temp_directory_path() /
              std::format("lobster_reconstructor_test_{}", std::random_device{}());
        std::filesystem::create_directories(dir);
        messages = dir / "messages.csv";
        orderbook = dir / "orderbook.csv";

        auto write = [](std::filesystem::path const &path, std::vector<std::string> const &lines) {
            std::ofstream file{path};
            for (auto const &line : lines) {
                file << line << '\n';
            }
        };
        write(messages, message_lines);
        write(orderbook, orderbook_lines);
    }

    TempLobsterFiles(TempLobsterFiles const &) = delete;
    auto operator=(TempLobsterFiles const &) -> TempLobsterFiles & = delete;
    TempLobsterFiles(TempLobsterFiles &&) = delete;
    auto operator=(TempLobsterFiles &&) -> TempLobsterFiles & = delete;

    ~TempLobsterFiles() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
};

struct Reconstruction {
    std::vector<Message> messages;
    std::vector<ExpectedOrderbook> expected;

    [[nodiscard]] auto synthetics() const {
        return messages | std::views::filter([](Message const &m) { return !m.index; });
    }

    // position in the reconstructed stream of the original message at 1-based line
    [[nodiscard]] auto position_of_line(MessageLine line) const -> std::size_t {
        auto it = std::ranges::find(messages, line - 1, &Message::index);
        REQUIRE(it != messages.end());
        return static_cast<std::size_t>(std::ranges::distance(messages.begin(), it));
    }

    [[nodiscard]] auto position_of_synthetic(auto predicate) const -> std::size_t {
        auto it = std::ranges::find_if(messages, [&](Message const &m) {
            return !m.index && predicate(m);
        });
        REQUIRE(it != messages.end());
        return static_cast<std::size_t>(std::ranges::distance(messages.begin(), it));
    }
};

auto reconstruct(std::vector<std::string> const &message_lines,
                 std::vector<std::string> const &orderbook_lines) -> Reconstruction {
    TempLobsterFiles files{message_lines, orderbook_lines};
    Reconstructor reconstructor{LobsterData{files.messages, files.orderbook}};
    auto messages = std::move(reconstructor).messages();
    auto expected = std::move(reconstructor).expected_orderbooks();
    return {.messages = std::move(messages), .expected = std::move(expected)};
}

// Replays the reconstructed stream with the same semantics as the benchmark
// (DELETE fully cancels the order regardless of the message size; CANCEL and
// EXECUTE reduce by the message size) and checks the volume of every price in
// the expected orderbook after each original message.
void replay_and_validate(Reconstruction const &reconstruction) {
    std::map<OrderId, std::pair<Price, Size>> live;

    auto volume = [&](Price price) -> Size {
        Size total = 0;
        for (auto const &[order_price, size] : live | std::views::values) {
            if (order_price == price) {
                total += size;
            }
        }
        return total;
    };

    for (auto const &message : reconstruction.messages) {
        INFO(std::format("replaying {}", message));
        switch (message.type) {
        case Type::ORDER:
            REQUIRE(!live.contains(message.order_id));
            live[message.order_id] = {message.price, message.size};
            break;
        case Type::CANCEL:
        case Type::EXECUTE: {
            REQUIRE(live.contains(message.order_id));
            auto &[price, size] = live[message.order_id];
            REQUIRE(price == message.price);
            REQUIRE(size >= message.size);
            size -= message.size;
            if (size == 0) {
                live.erase(message.order_id);
            }
            break;
        }
        case Type::DELETE:
            REQUIRE(live.contains(message.order_id));
            live.erase(message.order_id);
            break;
        case Type::EXECUTE_HIDDEN:
        case Type::HALT:
            break;
        }

        if (message.index) {
            auto const &expected = reconstruction.expected.at(*message.index);
            for (auto const &limit :
                 std::views::concat(expected.bids, expected.asks)) {
                INFO(std::format("after line {}: expected {} @{}", *message.index + 1,
                                 limit.quantity, limit.price));
                REQUIRE(volume(limit.price) == limit.quantity);
            }
        }
    }
}

// NOLINTEND(readability-magic-numbers)

} // namespace

// NOLINTBEGIN(readability-magic-numbers, readability-function-cognitive-complexity)

TEST_CASE("Fully visible stream needs only a fake for the pre-open book", "[reconstructor]") {
    // the ask resting at the open has no recorded creation: a fake order fills it
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
            msg(34202, Type::ORDER, 1001, 6, 10000, Direction::BUY),
            msg(34203, Type::CANCEL, 1000, 4, 10000, Direction::BUY),
            msg(34204, Type::EXECUTE, 1001, 6, 10000, Direction::BUY),
        },
        {
            book({{10100, 5}}, {{10000, 10}}),
            book({{10100, 5}}, {{10000, 16}}),
            book({{10100, 5}}, {{10000, 12}}),
            book({{10100, 5}}, {{10000, 6}}),
        });

    auto synthetics = reconstruction.synthetics() | std::ranges::to<std::vector>();
    REQUIRE(synthetics.size() == 1);
    CHECK(synthetics.front().type == Type::ORDER);
    CHECK(synthetics.front().price == 10100);
    CHECK(synthetics.front().size == 5);
    CHECK(synthetics.front().timestamp == open_timestamp());
    // synthetic ids don't clash with recorded ones
    CHECK(synthetics.front().order_id > 1001);

    replay_and_validate(reconstruction);
}

TEST_CASE("Hidden growth mints a fake order, hidden shrink cancels it back", "[reconstructor]") {
    // 10000 leaves the visible depth at line 3, returns at line 4 with 4 more
    // shares (hidden growth), leaves again at line 5 and returns 8 lighter at
    // line 6 (hidden shrink): the fake and part of order 1000 must be cancelled
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
            msg(34202, Type::ORDER, 1001, 20, 10100, Direction::BUY),
            msg(34203, Type::ORDER, 1002, 8, 10200, Direction::BUY),
            msg(34204, Type::DELETE, 1002, 8, 10200, Direction::BUY),
            msg(34205, Type::ORDER, 1003, 9, 10200, Direction::BUY),
            msg(34206, Type::DELETE, 1003, 9, 10200, Direction::BUY),
        },
        {
            book({{10300, 5}}, {{10000, 10}}),
            book({{10300, 5}}, {{10100, 20}, {10000, 10}}),
            book({{10300, 5}}, {{10200, 8}, {10100, 20}}),
            book({{10300, 5}}, {{10100, 20}, {10000, 14}}),
            book({{10300, 5}}, {{10200, 9}, {10100, 20}}),
            book({{10300, 5}}, {{10100, 20}, {10000, 6}}),
        });

    auto at_10000 = reconstruction.synthetics() |
                    std::views::filter([](Message const &m) { return m.price == 10000; }) |
                    std::ranges::to<std::vector>();

    auto fake = std::ranges::find(at_10000, Type::ORDER, &Message::type);
    REQUIRE(fake != at_10000.end());
    CHECK(fake->size == 4);

    auto cancelled = std::ranges::fold_left(
        at_10000 | std::views::filter([](Message const &m) { return m.type == Type::CANCEL; }),
        Size{0}, [](Size acc, Message const &m) { return acc + m.size; });
    CHECK(cancelled == 8);

    replay_and_validate(reconstruction);
}

TEST_CASE("Orphan reference is fulfilled by a synthetic order in its id window",
          "[reconstructor]") {
    // order 1005 was created while 10000 was outside the visible depth: only its
    // DELETE is recorded; its id pins the creation between orders 1000 and 1010
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 10100, Direction::BUY),
            msg(34202, Type::ORDER, 1010, 6, 10200, Direction::BUY),
            msg(34203, Type::DELETE, 1010, 6, 10200, Direction::BUY),
            msg(34204, Type::DELETE, 1005, 15, 10000, Direction::BUY),
        },
        {
            book({{10300, 5}}, {{10100, 10}}),
            book({{10300, 5}}, {{10200, 6}, {10100, 10}}),
            book({{10300, 5}}, {{10100, 10}, {10000, 15}}),
            book({{10300, 5}}, {{10100, 10}}),
        });

    auto order = reconstruction.position_of_synthetic(
        [](Message const &m) { return m.type == Type::ORDER && m.order_id == 1005; });
    auto reference = reconstruction.position_of_line(4);
    CHECK(order < reference);

    replay_and_validate(reconstruction);
}

TEST_CASE("Hidden partial deletion is cancelled before the real delete", "[reconstructor]") {
    // order 1001 is created with 200 but its recorded DELETE only accounts for
    // 100: the other 100 were cancelled while 10000 was hidden (lines 4-5). The
    // residual must be a CANCEL so the real DELETE still finds the order.
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 9900, Direction::BUY),
            msg(34202, Type::ORDER, 1001, 200, 10000, Direction::BUY),
            msg(34203, Type::ORDER, 1002, 30, 10100, Direction::BUY),
            msg(34204, Type::ORDER, 1003, 40, 10200, Direction::BUY),
            msg(34205, Type::DELETE, 1003, 40, 10200, Direction::BUY),
            msg(34206, Type::DELETE, 1001, 100, 10000, Direction::BUY),
        },
        {
            book({{10300, 5}}, {{9900, 10}}),
            book({{10300, 5}}, {{10000, 200}, {9900, 10}}),
            book({{10300, 5}}, {{10100, 30}, {10000, 200}}),
            book({{10300, 5}}, {{10200, 40}, {10100, 30}}),
            book({{10300, 5}}, {{10100, 30}, {10000, 100}}),
            book({{10300, 5}}, {{10100, 30}, {9900, 10}}),
        });

    auto cancel = reconstruction.position_of_synthetic([](Message const &m) {
        return m.type == Type::CANCEL && m.order_id == 1001 && m.size == 100;
    });
    auto creation = reconstruction.position_of_line(2);
    auto real_delete = reconstruction.position_of_line(6);
    CHECK(creation < cancel);
    CHECK(cancel < real_delete);

    replay_and_validate(reconstruction);
}

TEST_CASE("Referenced orders are consumed by cancel, never by delete", "[reconstructor]") {
    // hidden shrink of 100 at 10100 (lines 4-5) must take order 1001's single
    // unaccounted share via CANCEL: a synthetic DELETE would wipe the 199 shares
    // the real CANCEL at line 6 still needs
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
            msg(34202, Type::ORDER, 1001, 200, 10100, Direction::BUY),
            msg(34203, Type::ORDER, 1003, 99, 10100, Direction::BUY),
            msg(34204, Type::ORDER, 1004, 30, 10200, Direction::BUY),
            msg(34205, Type::DELETE, 1004, 30, 10200, Direction::BUY),
            msg(34206, Type::CANCEL, 1001, 199, 10100, Direction::BUY),
        },
        {
            book({{10300, 5}}, {{10000, 10}}),
            book({{10300, 5}}, {{10100, 200}, {10000, 10}}),
            book({{10300, 5}}, {{10100, 299}, {10000, 10}}),
            book({{10300, 5}}, {{10200, 30}}),
            book({{10300, 5}}, {{10100, 199}, {10000, 10}}),
            book({{10300, 5}}, {{10000, 10}}),
        });

    for (auto const &synthetic : reconstruction.synthetics()) {
        INFO(std::format("{}", synthetic));
        CHECK(synthetic.type != Type::DELETE);
        if (synthetic.order_id == 1001) {
            CHECK(synthetic.type == Type::CANCEL);
            CHECK(synthetic.size == 1);
        }
    }

    replay_and_validate(reconstruction);
}

TEST_CASE("Pre-open order visible at the open is created at market open", "[reconstructor]") {
    // id 55 predates the first recorded order (1000): its 300 shares are the ask
    // level standing in the opening book
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 10100, Direction::BUY),
            msg(34202, Type::EXECUTE, 55, 300, 10200, Direction::SELL),
        },
        {
            book({{10200, 300}, {10300, 7}}, {{10100, 10}}),
            book({{10300, 7}}, {{10100, 10}}),
        });

    auto order = std::ranges::find_if(reconstruction.messages, [](Message const &m) {
        return !m.index && m.type == Type::ORDER && m.order_id == 55;
    });
    REQUIRE(order != reconstruction.messages.end());
    CHECK(order->size == 300);
    CHECK(order->timestamp == open_timestamp());

    replay_and_validate(reconstruction);
}

TEST_CASE("Pre-open id appearing mid-day is created before its reference view, not at the open",
          "[reconstructor]") {
    // 9900 holds 25 pre-open shares at the open and leaves the visible depth
    // with them still resting (line 2). While hidden, those 25 are cancelled and
    // order 70 — carrying a pre-open id — arrives with 40. Creating order 70 at
    // the open would break the book while 9900 shows 25: it must be created in
    // the hidden window right before its reference view (lines 3-4).
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
            msg(34202, Type::ORDER, 1001, 3, 10100, Direction::BUY),
            msg(34203, Type::DELETE, 1001, 3, 10100, Direction::BUY),
            msg(34204, Type::DELETE, 70, 40, 9900, Direction::BUY),
        },
        {
            book({{10300, 7}}, {{10000, 10}, {9900, 25}}),
            book({{10300, 7}}, {{10100, 3}, {10000, 10}}),
            book({{10300, 7}}, {{10000, 10}, {9900, 40}}),
            book({{10300, 7}}, {{10000, 10}}),
        });

    auto order_70 = reconstruction.position_of_synthetic(
        [](Message const &m) { return m.type == Type::ORDER && m.order_id == 70; });
    CHECK(order_70 > reconstruction.position_of_line(2));
    CHECK(order_70 < reconstruction.position_of_line(3));

    // the pre-open 25 shares get a fake order at the open, cancelled while hidden
    auto fake = std::ranges::find_if(reconstruction.messages, [](Message const &m) {
        return !m.index && m.type == Type::ORDER && m.price == 9900 && m.size == 25;
    });
    REQUIRE(fake != reconstruction.messages.end());
    CHECK(fake->timestamp == open_timestamp());

    replay_and_validate(reconstruction);
}

TEST_CASE("Orphan referenced during a view still open at the end of the data", "[reconstructor]") {
    // 10100 never leaves the visible depth, so its last view is only closed by
    // the end of the data; the orphan reference on the final line must still be
    // matched to it
    auto reconstruction = reconstruct(
        {
            msg(34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
            msg(34202, Type::ORDER, 1010, 20, 10000, Direction::BUY),
            msg(34203, Type::CANCEL, 77, 4, 10100, Direction::SELL),
        },
        {
            book({{10100, 5}}, {{10000, 10}}),
            book({{10100, 5}}, {{10000, 30}}),
            book({{10100, 1}}, {{10000, 30}}),
        });

    replay_and_validate(reconstruction);
}

TEST_CASE("LobsterData rejects mismatched file lengths", "[reconstructor]") {
    TempLobsterFiles files{
        {
            msg(34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
            msg(34202, Type::ORDER, 1001, 5, 10000, Direction::BUY),
        },
        {
            book({{10100, 5}}, {{10000, 10}}),
        }};

    REQUIRE_THROWS(LobsterData{files.messages, files.orderbook});
}

TEST_CASE("LobsterData rejects missing files", "[reconstructor]") {
    TempLobsterFiles files{{msg(34201, Type::ORDER, 1000, 10, 10000, Direction::BUY)},
                           {book({{10100, 5}}, {{10000, 10}})}};

    REQUIRE_THROWS(LobsterData{files.dir / "nonexistent.csv", files.orderbook});
    REQUIRE_THROWS(LobsterData{files.messages, files.dir / "nonexistent.csv"});
}

// NOLINTEND(readability-magic-numbers, readability-function-cognitive-complexity)

} // namespace ome::tools::lobster
