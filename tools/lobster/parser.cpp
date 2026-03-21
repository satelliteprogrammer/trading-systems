#include "parser.hpp"

#include <format>
#include <regex>
#include <sstream>
#include <stdexcept>

using namespace std::literals::chrono_literals;

namespace ome::tools::lobster {

auto parse_message(std::string const &line) -> Message {
    // LOBSTER message format: Time,Type,OrderID,Size,Price,Direction
    // e.g 34200.005929046,1,16114346,1300,1356500,-1
    static std::regex const regex{R"((\d+\.?\d*),(\d),(\d+),(\d+),(\d+),(-?1))"};

    std::smatch match;
    if (!std::regex_match(line, match, regex)) {
        throw std::runtime_error{std::format("malformed message line: {}", line)};
    }

    // NOLINTBEGIN(readability-magic-numbers)
    auto time_seconds = std::chrono::duration<double>{std::stod(match[1].str())};
    auto order_type = static_cast<Type>(std::stoul(match[2].str()));
    auto order_id = static_cast<OrderId>(std::stoull(match[3].str()));
    auto size = std::stoull(match[4].str());
    auto price = static_cast<Price>(std::stoull(match[5].str()));
    auto direction = static_cast<Direction>(std::stoi(match[6].str()));
    // NOLINTEND(readability-magic-numbers)

    auto time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_seconds);
    auto timestamp = std::chrono::sys_days{LobsterDay} + time_ns;

    return Message{.timestamp = std::chrono::clock_cast<Clock>(timestamp),
                   .type = order_type,
                   .order_id = order_id,
                   .size = size,
                   .price = price,
                   .direction = direction};
}

auto parse_orderbook_line(std::string const &line) -> ExpectedOrderbook {
    ExpectedOrderbook result;

    std::istringstream iss{line};
    std::string token;

    while (std::getline(iss, token, ',')) {
        auto ask_price = std::stoull(token);

        if (!std::getline(iss, token, ',')) {
            throw std::runtime_error{"Malformed orderbook line: missing ask size"};
        }
        auto ask_size = std::stoull(token);

        if (!std::getline(iss, token, ',')) {
            throw std::runtime_error{"Malformed orderbook line: missing bid price"};
        }
        auto bid_price = std::stoull(token);

        if (!std::getline(iss, token, ',')) {
            throw std::runtime_error{"Malformed orderbook line: missing bid size"};
        }
        auto bid_size = std::stoull(token);

        if (ask_size > 0) {
            result.asks.emplace(ask_price, ask_size);
        }
        if (bid_size > 0) {
            result.bids.emplace(bid_price, bid_size);
        }
    }

    return result;
}

} // namespace ome::tools::lobster
