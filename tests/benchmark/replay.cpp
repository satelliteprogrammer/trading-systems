#include "replay.hpp"

#include <book/engine.hpp>
#include <book/order.hpp>
#include <lobster/reconstructor.hpp>
#include <lobster/types.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <stdexcept>
#include <string_view>
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

    id_map_.reserve(book::Engine::default_max_orders);
}

auto LobsterReplay::next() -> std::optional<Trade> {
    if (message_index_ >= messages_.size()) {
        return std::nullopt;
    }
    auto const msg = messages_[message_index_++];
    return message_to_trade(msg);
}

void LobsterReplay::rewind() {
    message_index_ = 0;
    id_map_.clear();
}

auto LobsterReplay::book_order(book::OrderId external_id)
    -> std::expected<BookOrder *, std::string_view> {
    auto const it = id_map_.find(external_id);
    if (it == id_map_.end()) {
        return std::unexpected{"unknown order id"};
    }
    return &it->second;
}

auto message_to_trade(tools::lobster::Message const &msg) -> Trade {
    switch (msg.type) {
    case tools::lobster::Type::ORDER:
        switch (msg.direction) {
        case tools::lobster::Direction::BUY:
            return NewOrder{
                .order_id = msg.order_id,
                .order = book::Order{{msg.size}, book::Side::Buy, msg.price, msg.timestamp},
            };
        case tools::lobster::Direction::SELL:
            return NewOrder{
                .order_id = msg.order_id,
                .order = book::Order{{msg.size}, book::Side::Sell, msg.price, msg.timestamp},
            };
        default:
            throw std::runtime_error{
                std::format("Unknown direction type: {}", static_cast<int>(msg.direction))};
        }
    case tools::lobster::Type::CANCEL:
        return PartialCancel{
            .order_id = msg.order_id,
            .price = msg.price,
            .quantity = msg.size,
            .timestamp = msg.timestamp,
        };
    case tools::lobster::Type::DELETE:
        return Cancel{
            .order_id = msg.order_id,
            .price = msg.price,
            .quantity = msg.size,
            .timestamp = msg.timestamp,
        };

    case tools::lobster::Type::EXECUTE:
        switch (msg.direction) {
        case tools::lobster::Direction::BUY:
            return Execute{
                .order_id = msg.order_id,
                .order = book::Order{{msg.size}, book::Side::Sell, msg.price, msg.timestamp},
                .hidden = false,
            };
        case tools::lobster::Direction::SELL:
            return Execute{
                .order_id = msg.order_id,
                .order = book::Order{{msg.size}, book::Side::Buy, msg.price, msg.timestamp},
                .hidden = false,
            };
        default:
            throw std::runtime_error{
                std::format("Unknown direction type: {}", static_cast<int>(msg.direction))};
        }

    case tools::lobster::Type::EXECUTE_HIDDEN:
        switch (msg.direction) {
        case tools::lobster::Direction::BUY:
            return Execute{
                .order_id = msg.order_id,
                .order = book::Order{{msg.size}, book::Side::Buy, msg.price, msg.timestamp},
                .hidden = true,
            };
        case tools::lobster::Direction::SELL:
            return Execute{
                .order_id = msg.order_id,
                .order = book::Order{{msg.size}, book::Side::Sell, msg.price, msg.timestamp},
                .hidden = true,
            };
        default:
            throw std::runtime_error{
                std::format("Unknown direction type: {}", static_cast<int>(msg.direction))};
        }

    case tools::lobster::Type::HALT:
        return Halt{.timestamp = msg.timestamp};
    }

    std::unreachable();
}

} // namespace ome::testing
