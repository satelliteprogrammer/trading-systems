#include "replay.hpp"

#include <filesystem>
#include <print>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace ome::testing {

LobsterReplay::LobsterReplay(fs::path const &message_file, fs::path const &orderbook_file) {
    if (!fs::is_regular_file(message_file)) {
        throw std::runtime_error{std::format("{} does not exist", message_file.string())};
    }
    if (!fs::is_regular_file(orderbook_file)) {
        throw std::runtime_error{std::format("{} does not exist", orderbook_file.string())};
    }

    std::println("Initializing reconstructor with LOBSTER data ...");
    tools::lobster::Reconstructor reconstructor{
        tools::lobster::LobsterData{message_file, orderbook_file}};

    std::println("Reconstructor initialized. Loading messages ...");
    messages_ = std::move(reconstructor).messages();
    expected_orderbooks_ = std::move(reconstructor).expected_orderbooks();
    std::println("Messages loaded.");

    id_map_.reserve(book::Book::default_max_orders);
}

auto LobsterReplay::next() -> std::optional<Trade> {
    if (message_index_ >= messages_.size()) {
        return std::nullopt;
    }
    auto msg = messages_[message_index_++];
    return message_to_trade(msg);
}

auto LobsterReplay::validate(book::Book const &book) const -> ValidationResult {
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

auto LobsterReplay::book_id(book::OrderId external_id) const
    -> std::expected<book::OrderId, std::string_view> {
    auto it = id_map_.find(external_id);
    if (it == id_map_.end()) {
        return std::unexpected{"unknown order id"};
    }
    return it->second;
}

auto message_to_trade(tools::lobster::Message const &msg) -> Trade {
    switch (msg.type) {
    case tools::lobster::Type::ORDER:
        switch (msg.direction) {
        case tools::lobster::Direction::BUY:
            return NewOrder{.order_id = msg.order_id,
                            .order = book::BuyOrder{{msg.size, msg.timestamp}, msg.price}};
        case tools::lobster::Direction::SELL:
            return NewOrder{.order_id = msg.order_id,
                            .order = book::SellOrder{{msg.size, msg.timestamp}, msg.price}};
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
            return Execute{.order_id = msg.order_id,
                           .order = book::SellOrder{{msg.size, msg.timestamp}, msg.price},
                           .hidden = false};
        case tools::lobster::Direction::SELL:
            return Execute{.order_id = msg.order_id,
                           .order = book::BuyOrder{{msg.size, msg.timestamp}, msg.price},
                           .hidden = false};
        default:
            throw std::runtime_error{
                std::format("Unknown direction type: {}", static_cast<int>(msg.direction))};
        }

    case tools::lobster::Type::EXECUTE_HIDDEN:
        switch (msg.direction) {
        case tools::lobster::Direction::BUY:
            return Execute{.order_id = msg.order_id,
                           .order = book::BuyOrder{{msg.size, msg.timestamp}, msg.price},
                           .hidden = true};
        case tools::lobster::Direction::SELL:
            return Execute{.order_id = msg.order_id,
                           .order = book::SellOrder{{msg.size, msg.timestamp}, msg.price},
                           .hidden = true};
        }

    case tools::lobster::Type::HALT:
        return Halt{.timestamp = msg.timestamp};
    }

    std::unreachable();
}

} // namespace ome::testing
