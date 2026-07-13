#include "lobster/reconstructor.hpp"

#include "parser.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <format>
#include <fstream>
#include <print>
#include <ranges>
#include <unordered_map>

using namespace std::literals::chrono_literals;
namespace ranges = std::ranges;
namespace views = std::views;

namespace ome::tools::lobster {

namespace {

constexpr auto sum_size(Size acc, Message const &msg) -> Size { return acc + msg.size; };

} // namespace

Reconstructor::Reconstructor(LobsterData const &data)
    : p_messages_(data.messages), p_orderbook_(data.orderbook) {

    std::println("Loading LOBSTER messages from {} ...", p_messages_.string());
    messages_.reserve(data.line_count);
    {
        std::ifstream file{p_messages_};
        std::string line;
        std::size_t line_num = 0;
        while (std::getline(file, line)) {
            auto msg = parse_message(line);
            msg.index = line_num++;
            messages_.push_back(msg);
        }
    }

    std::println("Loading LOBSTER expected orderbooks from {} ...", p_orderbook_.string());
    expected_orderbooks_.reserve(data.line_count);
    {
        std::ifstream file{p_orderbook_};
        std::string line;
        while (std::getline(file, line)) {
            auto orderbook = parse_orderbook_line(line);
            expected_orderbooks_.push_back(std::move(orderbook));
        }
    }

    std::println("Recreating missing messages for a valid orderbook...");
    auto synthetics = this->synthetics();

    std::println("Generated {} synthetic messages to fill gaps in orderbook.", synthetics.size());
    messages_.reserve(messages_.size() + synthetics.size());
    messages_.append_range(std::move(synthetics));
    ranges::stable_sort(messages_, {}, &Message::timestamp);
}

auto Reconstructor::messages() && -> std::vector<Message> { return std::move(messages_); }

auto Reconstructor::expected_orderbooks() && -> std::vector<ExpectedOrderbook> {
    return std::move(expected_orderbooks_);
}

auto Reconstructor::orphans() const -> Orphans {
    std::map<OrderId, Message> created_ids;
    std::map<OrderId, std::vector<Message>> deleted;
    std::map<OrderId, std::vector<Message>> referenced;

    for (auto const &msg : messages_) {
        switch (msg.type) {
        case Type::ORDER:
            created_ids.emplace(msg.order_id, msg);
            break;
        case Type::CANCEL:
        case Type::EXECUTE:
            referenced[msg.order_id].push_back(msg);
            break;
        case Type::DELETE: {
            auto node = referenced.extract(msg.order_id);
            deleted.insert(std::move(node));
            deleted[msg.order_id].push_back(msg);
            break;
        }
        case Type::EXECUTE_HIDDEN:
        case Type::HALT:
            break;
        }
    }

    // move from references to delete if all references consume original trade
    for (auto it = referenced.begin(); it != referenced.end();) {
        if (created_ids.contains(it->first)) {
            auto size = ranges::fold_left(it->second, Size{0}, sum_size);
            if (created_ids[it->first].size == size) {
                auto last = it->second.back();
                last.index.reset();
                last.type = Type::DELETE;
                last.size = size;
                deleted[it->first].push_back(last);
                it = referenced.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    std::map<OrderId, Message> orphans;
    for (auto const &[order_id, msgs] : views::concat(deleted, referenced)) {
        if (!created_ids.contains(order_id)) {
            auto orphan = msgs.front();
            // orphan.timestamp = MarketOpen;
            orphan.size = ranges::fold_left(msgs, Size{0}, sum_size);
            orphans.emplace(order_id, orphan);
        }
    }

    std::map<OrderId, Message> alive;
    for (auto const &[order_id, msg] : views::concat(created_ids)) {
        if (!deleted.contains(order_id)) {
            auto msg_ = msg;
            if (referenced.contains(order_id)) {
                msg_.size -= ranges::fold_left(referenced[order_id], Size{0}, sum_size);
            }
            alive.emplace(order_id, msg_);
        }
    }

    std::map<OrderId, Message> partially_deleted;
    for (auto const &[order_id, msgs] : deleted) {
        auto it = created_ids.find(order_id);
        if (it == created_ids.end()) {
            continue;
        }
        auto reduced = ranges::fold_left(msgs, Size{0}, sum_size);
        if (it->second.size > reduced) {
            auto residual = it->second;
            residual.size -= reduced;
            partially_deleted.emplace(order_id, residual);
        }
    }

    auto map_to_vector = [](auto &&map) -> std::vector<Message> {
        std::vector<Message> result;
        result.reserve(map.size());
        for (auto &&orphan : map | std::views::values) {
            result.emplace_back(std::forward<Message>(orphan));
        }
        ranges::sort(result, {}, &Message::timestamp);
        return result;
    };
    return {.only_referenced = map_to_vector(std::move(orphans)),
            .only_created = map_to_vector(std::move(alive)),
            .partially_deleted = map_to_vector(std::move(partially_deleted))};
}

auto Reconstructor::levels() const -> std::map<Price, std::vector<PriceView>> {
    std::map<Price, std::vector<PriceView>> levels;

    // auto initial_orderbook = expected_orderbooks_.front();
    // auto initial_trade = messages_.front();

    for (auto const &[line, orderbook, message] : views::zip(
             views::iota(0U), views::concat(std::vector{initial_orderbook()}, expected_orderbooks_),
             views::concat(std::vector{messages_.front()}, messages_))) {
        // fill missing book prices
        constexpr auto diff = 100;
        std::flat_set<Limit> missing_levels;
        for (auto price = orderbook.bids.rbegin()->price; price < orderbook.asks.rbegin()->price;
             price += diff) {
            if (!orderbook.asks.contains(Limit{.price = price}) &&
                !orderbook.bids.contains(Limit{.price = price})) {
                missing_levels.emplace(price, 0);
            }
        }

        auto direction = [&](Price const &price) -> Direction {
            return (price <= orderbook.bids.begin()->price) ? Direction::BUY : Direction::SELL;
        };

        auto level_views = std::views::concat(orderbook.bids, orderbook.asks, missing_levels);

        // add new levels, update exit volume
        for (auto const &level : level_views) {
            if (!levels.contains(level.price)) {
                // 1st iteration is opening the book (0), but references the initial orderbook (1)
                auto line_ = line != 0 ? line : 1;
                PriceView initial{.entry = {
                                      .line = line_,
                                      .timestamp = message.timestamp,
                                      .size = level.quantity,
                                      .direction = direction(level.price),
                                  }};
                levels.emplace(level.price, std::vector{initial});
            } else if (levels[level.price].back().exit.timestamp !=
                       std::chrono::clock_cast<Clock>(MarketClose)) {
                // level reentered view
                PriceView view{.entry = {
                                   .line = line,
                                   .timestamp = message.timestamp,
                                   .size = level.quantity,
                                   .direction = direction(level.price),
                               }};
                levels[level.price].push_back(view);
            } else {
                // no new view to add
            }

            levels[level.price].back().exit.size = level.quantity;
        }

        // close levels that disappear
        for (auto &[price, views] : levels) {
            auto it = ranges::find(level_views, price, &Limit::price);
            if (it == level_views.end()) {
                if (views.back().exit.timestamp == std::chrono::clock_cast<Clock>(MarketClose)) {
                    views.back().exit.line = line;
                    views.back().exit.timestamp = message.timestamp;
                    views.back().exit.direction = direction(price);
                }
            }
        }
    }

    // close views still open when the data ends
    for (auto &views : levels | views::values) {
        if (views.back().exit.timestamp == std::chrono::clock_cast<Clock>(MarketClose)) {
            views.back().exit.line = messages_.size();
            views.back().exit.timestamp = messages_.back().timestamp;
        }
    }

    return levels;
}

auto Reconstructor::initial_orderbook() const -> ExpectedOrderbook {
    auto initial_orderbook_ = expected_orderbooks_.front();
    auto initial_message = messages_.front();

    Limit limit{.price = initial_message.price};

    switch (initial_message.direction) {
    case Direction::BUY:
        if (initial_orderbook_.bids.contains(limit)) {
            limit.quantity = initial_orderbook_.bids.find(limit)->quantity;
            initial_orderbook_.bids.erase(limit);
        }
        break;
    case Direction::SELL:
        if (initial_orderbook_.asks.contains(limit)) {
            limit.quantity = initial_orderbook_.asks.find(limit)->quantity;
            initial_orderbook_.asks.erase(limit);
        }
        break;
    }

    switch (initial_message.type) {
    case Type::ORDER:
        limit.quantity -= initial_message.size;
        break;
    case Type::CANCEL:
    case Type::DELETE:
    case Type::EXECUTE:
        limit.quantity += initial_message.size;
        break;
    case Type::EXECUTE_HIDDEN:
    case Type::HALT:
        break;
    }

    switch (initial_message.direction) {
    case Direction::BUY:
        initial_orderbook_.bids.insert(limit);
        break;
    case Direction::SELL:
        initial_orderbook_.asks.insert(limit);
        break;
    }

    return initial_orderbook_;
}

auto Reconstructor::synthetics() const -> std::vector<Message> {
    std::println("Retrieving orphan messages...");
    auto orphans = this->orphans();

    std::unordered_map<Price, std::vector<Message>> only_referenced_map;
    std::unordered_map<Price, std::vector<Message>> only_created_map;
    std::unordered_map<Price, std::vector<Message>> partially_deleted_map;
    for (auto &&orphan : std::move(orphans.only_referenced)) {
        only_referenced_map[orphan.price].emplace_back(std::forward<Message>(orphan));
    }
    for (auto &&orphan : std::move(orphans.only_created)) {
        only_created_map[orphan.price].emplace_back(std::forward<Message>(orphan));
    }
    for (auto &&orphan : std::move(orphans.partially_deleted)) {
        partially_deleted_map[orphan.price].emplace_back(std::forward<Message>(orphan));
    }

    std::println("Collecting orderbook levels...");
    auto levels = this->levels();

    std::println("Generating missing messages...");
    std::vector<Message> synthetics;
    auto const synthetic_id_start = this->next_order_id(messages_.size());
    OrderId synthetic_id = synthetic_id_start;
    auto const first_order_id = this->next_order_id(1);

    for (auto [price, views] : levels) {
        assert(!views.empty());

        PriceView start_view{.entry = {.line = 0,
                                       .timestamp = std::chrono::clock_cast<Clock>(MarketOpen),
                                       .size = 0},
                             .exit = {.line = 0,
                                      .timestamp = std::chrono::clock_cast<Clock>(MarketOpen),
                                      .size = 0}};
        views.insert(views.begin(), start_view);

        auto node = only_referenced_map.extract(price);
        auto only_referenced = node ? std::move(node.mapped()) : std::vector<Message>{};
        node = only_created_map.extract(price);
        auto only_created = node ? std::move(node.mapped()) : std::vector<Message>{};
        node = partially_deleted_map.extract(price);
        auto partially_deleted = node ? std::move(node.mapped()) : std::vector<Message>{};

        std::vector<Message> orphans_consumed; // debug only

        auto debug = [this](auto const &price, auto const &views, auto const &synthetics,
                            auto const &only_referenced, auto const &only_created,
                            auto const &orphans_consumed) -> std::string {
            std::ostringstream oss;
            oss << "price: " << price << '\n';

            oss << "views:\n";
            for (auto const &view : views) {
                oss << std::format("{} | OrderId: {} ... {}", view, next_order_id(view.entry.line),
                                   next_order_id(view.exit.line))
                    << '\n';
            }

            oss << "synthetics\n";
            auto fp = [&](auto const &msg) -> bool { return msg.price == price; };
            for (auto const &synthetic : synthetics | std::views::filter(fp)) {
                oss << std::format("{}", synthetic) << '\n';
            }

            if (!only_referenced.empty()) {
                oss << "orphans left\n";
                for (auto const &referenced : only_referenced) {
                    oss << std::format("{}", referenced) << '\n';
                }
            } else {
                oss << "no orphans left\n";
            }

            if (!orphans_consumed.empty()) {
                oss << "orphans consumed\n";
                for (auto const &consumed : orphans_consumed) {
                    oss << std::format("{}", consumed) << '\n';
                }
            } else {
                oss << "no orphans consumed\n";
            }

            if (!only_created.empty()) {
                oss << "created available for use\n";
                oss << "from " << std::format("{}", only_created.front()) << "\n  to "
                    << std::format("{}", only_created.back());
            } else {
                oss << "no alive orders left\n";
            }
            return oss.str();
        };

        for (auto const &[first, second] : views | std::views::adjacent<2>) {
            // generate all orphans that belong in this view interval

            PriceView hidden{.entry = first.exit, .exit = second.entry};
            struct {
                OrderId entry;
                OrderId exit;
            } order_ids{
                .entry = next_order_id(hidden.entry.line),
                .exit = next_order_id(hidden.exit.line),
            };
            auto volume = static_cast<std::int64_t>(hidden.exit.size - hidden.entry.size);

            std::vector<Message> orphans_in_view;
            for (auto it = only_referenced.begin(); it != only_referenced.end();) {
                bool in_view{false};

                // while order IDs are monotonically increasing, there are instances where orders
                // with IDs pre-market open are referenced out-of-order early in the replay sequence
                // capture these when they are in the second view
                if (it->order_id < first_order_id) {
                    if (*it->index + 1 >= second.entry.line && *it->index + 1 <= second.exit.line) {
                        std::println("Found orphan referenced out-of-order: {}", *it);
                        in_view = true;
                    }
                } else if (it->order_id >= order_ids.entry && it->order_id < order_ids.exit) {
                    in_view = true;
                }

                if (in_view) {
                    orphans_in_view.emplace_back(*it);
                    orphans_consumed.emplace_back(*it);
                    it = only_referenced.erase(it);
                } else {
                    ++it;
                }
            }
            ranges::sort(orphans_in_view, {}, &Message::order_id);

            for (auto &&orphan : orphans_in_view) {
                synthetics.push_back(Reconstructor::make_order(orphan, hidden.entry.timestamp));
                volume -= static_cast<std::int64_t>(orphan.size);
            }

            for (auto it = partially_deleted.begin(); it != partially_deleted.end();) {
                if (it->order_id < order_ids.exit) {
                    synthetics.push_back(
                        Reconstructor::make_cancel(*it, hidden.entry.timestamp, it->size));
                    volume += static_cast<std::int64_t>(it->size);
                    it = partially_deleted.erase(it);
                } else {
                    ++it;
                }
            }

            if (volume > 0) {
                Message fake_order{
                    .timestamp = first.exit.timestamp,
                    .type = Type::ORDER,
                    .order_id = synthetic_id++,
                    .size = static_cast<Size>(volume),
                    .price = price,
                    .direction = second.entry.direction,
                };
                only_created.insert(only_created.begin(), fake_order);
                synthetics.push_back(fake_order);
            } else if (volume < 0) {
                volume = std::abs(volume);

                auto it = only_created.begin();
                while (volume != 0 && it != only_created.end()) {
                    if (it->order_id >= order_ids.exit && it->order_id < synthetic_id_start) {
                        ++it;
                        continue;
                    }

                    if (it->size > volume) {
                        synthetics.push_back(Reconstructor::make_cancel(*it, hidden.entry.timestamp,
                                                                        static_cast<Size>(volume)));
                        it->size -= volume;
                        volume = 0;
                    } else {
                        // even if we're entirely consuming an order that was created and never
                        // deleted, there could still exist other EXECUTE or CANCEL messages
                        // referencing it, from which we have subtracted the size used here.
                        // given a cancel is always safe to use and if cancelling the entire
                        // order size the result is the same as a delete, we can use it here for
                        // any scenario
                        synthetics.push_back(
                            Reconstructor::make_cancel(*it, hidden.entry.timestamp, it->size));
                        volume -= static_cast<std::int64_t>(it->size);
                        ++it;
                    }
                }
                only_created.erase(only_created.begin(), it);

                if (volume != 0) {
                    std::println("{}", debug(price, views, synthetics, only_referenced,
                                             only_created, orphans_consumed));
                    throw std::runtime_error{std::format("failed to fulfill volume ({}) @{}\n{}",
                                                         volume, price, hidden)};
                }
            }
        }

        if (!only_referenced.empty()) {
            throw std::runtime_error{std::format(
                "no more fakes to cancel @{}, {}", price,
                debug(price, views, synthetics, only_referenced, only_created, orphans_consumed))};
        }
        if (!partially_deleted.empty()) {
            throw std::runtime_error{std::format(
                "unconsumed hidden partial deletions @{}, {}", price,
                debug(price, views, synthetics, only_referenced, only_created, orphans_consumed))};
        }
    }

    return synthetics;
}

auto Reconstructor::next_order_id(MessageLine idx) const -> OrderId {
    // message lines start at 1. If idx is 0, then we are at market open
    if (idx == 0) {
        return 0;
    }

    auto begin = ranges::next(messages_.begin(), static_cast<std::ptrdiff_t>(idx - 1));
    constexpr auto next_order = [](auto const &msg) -> bool { return msg.type == Type::ORDER; };
    auto it = ranges::find_if(begin, messages_.end(), next_order);
    if (it == messages_.end()) {
        auto it = ranges::find_if(messages_.rbegin(), messages_.rend(), next_order);
        if (it == messages_.rend()) {
            throw std::runtime_error{std::format("No next order found after message line {}", idx)};
        }
        return it->order_id + 1;
    }
    return it->order_id;
}

auto Reconstructor::make_order(Message const &original, Timestamp dt) -> Message {
    return Message{
        .timestamp = dt,
        .type = Type::ORDER,
        .order_id = original.order_id,
        .size = original.size,
        .price = original.price,
        .direction = original.direction,
    };
}

auto Reconstructor::make_cancel(Message const &orphan, Timestamp dt, Size size) -> Message {
    return Message{
        .timestamp = dt,
        .type = Type::CANCEL,
        .order_id = orphan.order_id,
        .size = size,
        .price = orphan.price,
        .direction = orphan.direction,
    };
}

LobsterData::LobsterData(std::filesystem::path const &messages,
                         std::filesystem::path const &orderbook)
    : messages(messages), orderbook(orderbook) {
    if (!std::filesystem::is_regular_file(messages)) {
        throw std::runtime_error{
            std::format("LOBSTER messages file does not exist: {}", messages.string())};
    }
    if (!std::filesystem::is_regular_file(orderbook)) {
        throw std::runtime_error{
            std::format("LOBSTER orderbook file does not exist: {}", orderbook.string())};
    }

    auto count_lines = [](std::filesystem::path const &path) -> std::size_t {
        std::ifstream file{path};
        std::size_t line_count = 0;
        std::string _;
        while (std::getline(file, _)) {
            ++line_count;
        }
        return line_count;
    };

    auto msg_lines = count_lines(messages);
    auto ob_lines = count_lines(orderbook);
    if (msg_lines != ob_lines) {
        throw std::runtime_error{std::format("LOBSTER file line count mismatch: messages ({}) has "
                                             "{} lines, orderbook ({}) has {} lines",
                                             messages.string(), msg_lines, orderbook.string(),
                                             ob_lines)};
    }

    line_count = msg_lines;
}

} // namespace ome::tools::lobster
