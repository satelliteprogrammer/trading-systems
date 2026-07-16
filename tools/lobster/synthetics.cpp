#include "synthetics.hpp"

#include "algorithm.hpp"
#include "lobster/types.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <functional>
#include <ranges>
#include <stdexcept>

namespace ome::tools::lobster {

namespace ranges = std::ranges;
using algorithm::vector_to_map;

Synthetics::Synthetics(Orphans const &orphans, Levels const &levels,
                       std::function_ref<OrderId(MessageLine)> next_order_id_)
    : only_referenced_by_price{vector_to_map(orphans.only_referenced, &Message::price)},
      only_created_by_price{vector_to_map(orphans.only_created, &Message::price)},
      partially_deleted_by_price{vector_to_map(orphans.partially_deleted, &Message::price)},
      next_order_id(next_order_id_) {

    for (auto [price, views] : levels) {
        assert(!views.empty());

        PriceView start_view{.entry = {.line = 0,
                                       .timestamp = std::chrono::clock_cast<Clock>(MarketOpen),
                                       .size = 0},
                             .exit = {.line = 0,
                                      .timestamp = std::chrono::clock_cast<Clock>(MarketOpen),
                                      .size = 0}};
        views.insert(views.begin(), start_view);

        auto &&orphans = extract(price);
        Messages orphans_consumed; // debug only

        synthetics_per_price(price, views, orphans, orphans_consumed);

        if (!orphans.only_referenced.empty()) {
            throw SyntheticsError{
                "no more fakes to cancel @{}",
                price,
                views,
                synthetics,
                orphans.only_referenced,
                orphans.only_created,
                orphans_consumed,
                next_order_id,

            };
        }
        if (!orphans.partially_deleted.empty()) {
            throw SyntheticsError{
                "unconsumed hidden partial deletions @{}",
                price,
                views,
                synthetics,
                orphans.only_referenced,
                orphans.only_created,
                orphans_consumed,
                next_order_id,
            };
        }
    }
}

auto Synthetics::operator()() -> Messages { return std::move(synthetics); }

auto Synthetics::extract(Price price) -> OrphansInPrice {
    auto node = only_referenced_by_price.extract(price);
    auto &&only_referenced = node ? std::move(node).mapped() : Messages{};
    node = only_created_by_price.extract(price);
    auto &&only_created = node ? std::move(node).mapped() : Messages{};
    node = partially_deleted_by_price.extract(price);
    auto &&partially_deleted = node ? std::move(node).mapped() : Messages{};

    return {
        .only_referenced = only_referenced,
        .only_created = only_created,
        .partially_deleted = partially_deleted,
    };
}

auto Synthetics::synthetics_per_price(Price price, PriceViews const &views, OrphansInPrice &orphans,
                                      Messages &consumed) -> void {

    for (auto const &[first, second] : views | std::views::adjacent<2>) {
        // generate all orphans that belong in this view interval

        PriceView hidden{.entry = first.exit, .exit = second.entry};
        auto volume = static_cast<std::int64_t>(hidden.exit.size - hidden.entry.size);

        for (auto &&orphan :
             referenced_in_view(orphans.only_referenced, hidden, second, consumed)) {
            synthetics.push_back(make_order(orphan, hidden.entry.timestamp));
            volume -= static_cast<std::int64_t>(orphan.size);
        }

        for (auto &&deleted : partially_deleted_in_view(orphans.partially_deleted, hidden)) {
            synthetics.push_back(make_cancel(deleted, hidden.entry.timestamp, deleted.size));
            volume += static_cast<std::int64_t>(deleted.size);
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
            orphans.only_created.insert(orphans.only_created.begin(), fake_order);
            synthetics.push_back(fake_order);
            volume = 0;
        } else if (volume < 0) {
            volume = std::abs(volume);

            auto it = orphans.only_created.begin();
            while (volume != 0 && it != orphans.only_created.end()) {

                if (in(OrderIdView{.entry = order_ids(hidden).exit, .exit = first_synthetic_id},
                       it->order_id)) {
                    ++it;
                    continue;
                }

                if (it->size > volume) {
                    synthetics.push_back(
                        make_cancel(*it, hidden.entry.timestamp, static_cast<Size>(volume)));
                    it->size -= volume;
                    volume = 0;
                } else {
                    // even if we're entirely consuming an order that was created and never
                    // deleted, there could still exist other EXECUTE or CANCEL messages
                    // referencing it, from which we have subtracted the size used here.
                    // given a cancel is always safe to use and if cancelling the entire
                    // order size the result is the same as a delete, we can use it here for
                    // any scenario
                    synthetics.push_back(make_cancel(*it, hidden.entry.timestamp, it->size));
                    volume -= static_cast<std::int64_t>(it->size);
                    ++it;
                }
            }
            orphans.only_created.erase(orphans.only_created.begin(), it);
        }

        if (volume != 0) {
            throw SyntheticsError{
                std::format("failed to fulfill volume ({})", volume),
                price,
                views,
                synthetics,
                orphans.only_referenced,
                orphans.only_created,
                consumed,
                next_order_id,
            };
        }
    }
}

auto Synthetics::referenced_in_view(Messages &only_referenced, PriceView const &hidden,
                                    PriceView const &second, Messages &consumed) -> Messages {
    Messages referenced_in_view;
    for (auto it = only_referenced.begin(); it != only_referenced.end();) {
        bool in_view{false};

        // while order IDs are monotonically increasing, there are instances where orders
        // with IDs pre-market open are referenced out-of-order early in the replay sequence
        // capture these when they are in the second view
        if (it->order_id < first_order_id) {
            if (*it->index + 1 >= second.entry.line && *it->index + 1 <= second.exit.line) {
                in_view = true;
            }
        } else if (in(order_ids(hidden), it->order_id)) {
            in_view = true;
        }

        if (in_view) {
            referenced_in_view.emplace_back(*it);
            consumed.emplace_back(*it);
            it = only_referenced.erase(it);
        } else {
            ++it;
        }
    }
    ranges::sort(referenced_in_view, {}, &Message::order_id);

    return referenced_in_view;
}

auto Synthetics::partially_deleted_in_view(Messages &partially_deleted, PriceView const &hidden)
    -> Messages {
    Messages partially_deleted_in_view;
    for (auto it = partially_deleted.begin(); it != partially_deleted.end();) {
        if (it->order_id < order_ids(hidden).exit) {
            partially_deleted_in_view.emplace_back(*it);
            it = partially_deleted.erase(it);
        } else {
            ++it;
        }
    }
    return partially_deleted_in_view;
}

auto Synthetics::order_ids(PriceView const &view) -> OrderIdView {
    return {
        .entry = next_order_id(view.entry.line),
        .exit = next_order_id(view.exit.line),
    };
}

auto Synthetics::in(OrderIdView const &order_ids, OrderId id) -> bool {
    return order_ids.entry <= id && id < order_ids.exit;
}

auto Synthetics::make_order(Message const &original, Timestamp dt) -> Message {
    return Message{
        .timestamp = dt,
        .type = Type::ORDER,
        .order_id = original.order_id,
        .size = original.size,
        .price = original.price,
        .direction = original.direction,
    };
}

auto Synthetics::make_cancel(Message const &orphan, Timestamp dt, Size size) -> Message {
    return Message{
        .timestamp = dt,
        .type = Type::CANCEL,
        .order_id = orphan.order_id,
        .size = size,
        .price = orphan.price,
        .direction = orphan.direction,
    };
}

namespace {
auto error_msg(auto const &error, auto const &price, auto const &views, auto const &synthetics,
               auto const &only_referenced, auto const &only_created, auto const &orphans_consumed,
               std::function_ref<OrderId(MessageLine)> next_order_id) {

    std::ostringstream oss;
    oss << error << '\n';
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
}
} // namespace

SyntheticsError::SyntheticsError(auto error, auto const &price, auto const &views,
                                 auto const &synthetics, auto const &only_referenced,
                                 auto const &only_created, auto const &orphans_consumed,
                                 std::function_ref<OrderId(MessageLine)> next_order_id)
    : std::runtime_error{error_msg(error, price, views, synthetics, only_referenced, only_created,
                                   orphans_consumed, next_order_id)} {}

} // namespace ome::tools::lobster
