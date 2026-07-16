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

constexpr auto open_timestamp() -> Timestamp { return std::chrono::clock_cast<Clock>(MarketOpen); }

auto msg(double time, Type type, OrderId id, Size size, Price price, Direction dir) -> std::string {
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
    Messages messages;
    std::vector<ExpectedOrderbook> expected;

    // position in the reconstructed stream of the original message at 1-based line
    [[nodiscard]] auto position_of_line(MessageLine line) const -> std::size_t {
        auto it = std::ranges::find(messages, line - 1, &Message::index);
        REQUIRE(it != messages.end());
        return static_cast<std::size_t>(std::ranges::distance(messages.begin(), it));
    }

    [[nodiscard]] auto position_of_synthetic(auto predicate) const -> std::size_t {
        auto it = std::ranges::find_if(messages,
                                       [&](Message const &m) { return !m.index && predicate(m); });
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
            for (auto const &limit : std::views::concat(expected.bids, expected.asks)) {
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

TEST_CASE("Reconstructor merges synthetics into the recorded stream in order", "[reconstructor]") {
    // end-to-end: orphans and level views are derived from the raw LOBSTER
    // files and the synthetics land between the right original messages after
    // the merge (synthetic generation itself is covered in synthetics.test.cpp).
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

TEST_CASE("LobsterData rejects mismatched file lengths", "[reconstructor]") {
    TempLobsterFiles files{{
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
