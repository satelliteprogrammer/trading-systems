#include "synthetics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <ranges>
#include <vector>

namespace ome::tools::lobster {

namespace {

// NOLINTBEGIN(readability-magic-numbers)

constexpr auto open_timestamp() -> Timestamp { return std::chrono::clock_cast<Clock>(MarketOpen); }

auto ts(double seconds) -> Timestamp {
    auto since_midnight = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>{seconds});
    return std::chrono::clock_cast<Clock>(std::chrono::sys_days{LobsterDay} + since_midnight);
}

// an original message surviving as an orphan; line is the 1-based message line
auto orphan(MessageLine line, double time, Type type, OrderId id, Size size, Price price,
            Direction direction) -> Message {
    return {.index = line - 1,
            .timestamp = ts(time),
            .type = type,
            .order_id = id,
            .size = size,
            .price = price,
            .direction = direction};
}

auto snap(MessageLine line, double time, Size size, Direction direction) -> PriceSnapshot {
    return {.line = line, .timestamp = ts(time), .size = size, .direction = direction};
}

auto view(PriceSnapshot entry, PriceSnapshot exit) -> PriceView {
    return {.entry = entry, .exit = exit};
}

// Mirrors Reconstructor::next_order_id: the id of the first ORDER at or after
// the 1-based line, one past the last recorded id when there is none (which is
// also the base for synthetic ids), and 0 at market open.
struct NextOrderId {
    // order id per message line, 0 for lines that are not ORDER messages
    std::vector<OrderId> ids;

    auto operator()(MessageLine line) const -> OrderId {
        if (line == 0) {
            return 0;
        }
        for (auto i = line - 1; i < ids.size(); ++i) {
            if (ids[i] != 0) {
                return ids[i];
            }
        }
        auto last =
            std::ranges::find_if(ids | std::views::reverse, [](OrderId id) { return id != 0; });
        return *last + 1;
    }
};

auto find_synthetic(Messages const &synthetics, Type type, OrderId id) {
    return std::ranges::find_if(
        synthetics, [&](Message const &m) { return m.type == type && m.order_id == id; });
}

// NOLINTEND(readability-magic-numbers)

} // namespace

// NOLINTBEGIN(readability-magic-numbers, readability-function-cognitive-complexity)

TEST_CASE("Fully visible stream needs only a fake for the pre-open book", "[synthetics]") {
    // the ask at 10100 rests in the book from the open with no recorded
    // creation: a fake order fills it. Order 1000 still holds 6 unaccounted
    // shares, but its level never leaves the visible depth, so it is untouched.
    NextOrderId next{.ids = {1000, 1001, 0, 0}};
    Orphans orphans{
        .only_created = {orphan(1, 34201, Type::ORDER, 1000, 6, 10000, Direction::BUY)},
    };
    Levels levels{
        {10000, {view(snap(1, 34201, 0, Direction::BUY), snap(4, 34204, 6, Direction::BUY))}},
        {10100, {view(snap(1, 34201, 5, Direction::SELL), snap(4, 34204, 5, Direction::SELL))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();

    REQUIRE(synthetics.size() == 1);
    CHECK(synthetics.front().type == Type::ORDER);
    CHECK(synthetics.front().price == 10100);
    CHECK(synthetics.front().size == 5);
    CHECK(synthetics.front().direction == Direction::SELL);
    CHECK(synthetics.front().timestamp == open_timestamp());
    // synthetic ids don't clash with recorded ones
    CHECK(synthetics.front().order_id > 1001);
}

TEST_CASE("Hidden growth mints a fake order, hidden shrink cancels it back", "[synthetics]") {
    // 10000 leaves the visible depth holding 10 and returns with 14 (4 grew
    // hidden), then leaves with 14 and returns with 6 (8 shrank hidden): the
    // fake 4 and part of order 1000 must be cancelled
    NextOrderId next{.ids = {1000, 1001, 1002, 0, 1003, 0}};
    Orphans orphans{
        .only_created = {orphan(1, 34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
                         orphan(2, 34202, Type::ORDER, 1001, 20, 10100, Direction::BUY)},
    };
    Levels levels{
        {10000,
         {view(snap(1, 34201, 0, Direction::BUY), snap(3, 34203, 10, Direction::BUY)),
          view(snap(4, 34204, 14, Direction::BUY), snap(5, 34205, 14, Direction::BUY)),
          view(snap(6, 34206, 6, Direction::BUY), snap(6, 34206, 6, Direction::BUY))}},
        {10100, {view(snap(1, 34201, 0, Direction::BUY), snap(6, 34206, 20, Direction::BUY))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();
    REQUIRE(synthetics.size() == 3);

    auto fake = std::ranges::find(synthetics, Type::ORDER, &Message::type);
    REQUIRE(fake != synthetics.end());
    CHECK(fake->price == 10000);
    CHECK(fake->size == 4);
    // minted when the level went hidden with 10 resting
    CHECK(fake->timestamp == ts(34203));

    // the shrink consumes the fake before touching order 1000
    auto fake_cancel = find_synthetic(synthetics, Type::CANCEL, fake->order_id);
    REQUIRE(fake_cancel != synthetics.end());
    CHECK(fake_cancel->size == 4);
    CHECK(fake_cancel->timestamp == ts(34205));

    auto real_cancel = find_synthetic(synthetics, Type::CANCEL, 1000);
    REQUIRE(real_cancel != synthetics.end());
    CHECK(real_cancel->size == 4);
    CHECK(real_cancel->timestamp == ts(34205));
}

TEST_CASE("Orphan reference is fulfilled by a synthetic order in its id window", "[synthetics]") {
    // order 1005 was created while 10000 was outside the visible depth: only
    // its DELETE on line 4 is recorded. Its id falls in the id window of the
    // hidden stretch before 10000's only view, so the creation lands there.
    NextOrderId next{.ids = {1000, 1010, 0, 0}};
    Orphans orphans{
        .only_referenced = {orphan(4, 34204, Type::DELETE, 1005, 15, 10000, Direction::BUY)},
        .only_created = {orphan(1, 34201, Type::ORDER, 1000, 10, 10100, Direction::BUY)},
    };
    Levels levels{
        {10000, {view(snap(3, 34203, 15, Direction::BUY), snap(4, 34204, 15, Direction::BUY))}},
        {10100, {view(snap(1, 34201, 0, Direction::BUY), snap(4, 34204, 10, Direction::BUY))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();

    REQUIRE(synthetics.size() == 1);
    CHECK(synthetics.front().type == Type::ORDER);
    CHECK(synthetics.front().order_id == 1005);
    CHECK(synthetics.front().size == 15);
    CHECK(synthetics.front().price == 10000);
    // created in the hidden window before the view, i.e. before its reference
    CHECK(synthetics.front().timestamp == open_timestamp());
}

TEST_CASE("Hidden partial deletion is cancelled before the real delete", "[synthetics]") {
    // order 1001 was created with 200 but its recorded DELETE only accounts
    // for 100: the residual 100 was cancelled while 10000 was hidden between
    // its two views (lines 4-5). The residual must be a CANCEL inside that
    // window so the real DELETE still finds the order.
    NextOrderId next{.ids = {1000, 1001, 1002, 1003, 0, 0}};
    Orphans orphans{
        .partially_deleted = {orphan(2, 34202, Type::ORDER, 1001, 100, 10000, Direction::BUY)},
    };
    Levels levels{
        {10000,
         {view(snap(1, 34201, 0, Direction::BUY), snap(4, 34204, 200, Direction::BUY)),
          view(snap(5, 34205, 100, Direction::BUY), snap(6, 34206, 0, Direction::BUY))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();

    REQUIRE(synthetics.size() == 1);
    CHECK(synthetics.front().type == Type::CANCEL);
    CHECK(synthetics.front().order_id == 1001);
    CHECK(synthetics.front().size == 100);
    // cancelled when the level went hidden: after the creation on line 2 and
    // before the real DELETE on line 6
    CHECK(synthetics.front().timestamp == ts(34204));
}

TEST_CASE("Referenced orders are consumed by cancel, never by delete", "[synthetics]") {
    // hidden shrink of 100 at 10100 (lines 4-5) must take order 1001's single
    // unaccounted share via CANCEL: a synthetic DELETE would wipe the 199
    // shares the real CANCEL on line 6 still needs
    NextOrderId next{.ids = {1000, 1001, 1003, 1004, 0, 0}};
    Orphans orphans{
        .only_created =
            {// 199 of order 1001's 200 shares are still referenced later
             orphan(2, 34202, Type::ORDER, 1001, 1, 10100, Direction::BUY),
             orphan(3, 34203, Type::ORDER, 1003, 99, 10100, Direction::BUY)},
    };
    Levels levels{
        {10100,
         {view(snap(1, 34201, 0, Direction::BUY), snap(4, 34204, 299, Direction::BUY)),
          view(snap(5, 34205, 199, Direction::BUY), snap(6, 34206, 0, Direction::BUY))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();
    REQUIRE(synthetics.size() == 2);

    for (auto const &synthetic : synthetics) {
        INFO(std::format("{}", synthetic));
        CHECK(synthetic.type == Type::CANCEL);
    }

    auto cancel_1001 = find_synthetic(synthetics, Type::CANCEL, 1001);
    REQUIRE(cancel_1001 != synthetics.end());
    CHECK(cancel_1001->size == 1);

    auto cancel_1003 = find_synthetic(synthetics, Type::CANCEL, 1003);
    REQUIRE(cancel_1003 != synthetics.end());
    CHECK(cancel_1003->size == 99);
}

TEST_CASE("Pre-open order visible at the open is created at market open", "[synthetics]") {
    // id 55 predates the first recorded order (1000) and is referenced during
    // 10200's opening view: its 300 shares are the ask level standing in the
    // opening book
    NextOrderId next{.ids = {1000, 0}};
    Orphans orphans{
        .only_referenced = {orphan(2, 34202, Type::EXECUTE, 55, 300, 10200, Direction::SELL)},
        .only_created = {orphan(1, 34201, Type::ORDER, 1000, 10, 10100, Direction::BUY)},
    };
    Levels levels{
        {10100, {view(snap(1, 34201, 0, Direction::BUY), snap(2, 34202, 10, Direction::BUY))}},
        {10200, {view(snap(1, 34201, 300, Direction::SELL), snap(2, 34202, 0, Direction::SELL))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();

    REQUIRE(synthetics.size() == 1);
    CHECK(synthetics.front().type == Type::ORDER);
    CHECK(synthetics.front().order_id == 55);
    CHECK(synthetics.front().size == 300);
    CHECK(synthetics.front().price == 10200);
    CHECK(synthetics.front().timestamp == open_timestamp());
}

TEST_CASE("Pre-open id appearing mid-day is created before its reference view, not at the open",
          "[synthetics]") {
    // 9900 holds 25 pre-open shares at the open and leaves the visible depth
    // with them still resting (line 2). While hidden, those 25 are cancelled
    // and order 70 — carrying a pre-open id — arrives with 40. Creating order
    // 70 at the open would break the book while 9900 shows 25: it must be
    // created in the hidden window right before its reference view (lines 2-3).
    NextOrderId next{.ids = {1000, 1001, 0, 0}};
    Orphans orphans{
        .only_referenced = {orphan(4, 34204, Type::DELETE, 70, 40, 9900, Direction::BUY)},
        .only_created = {orphan(1, 34201, Type::ORDER, 1000, 10, 10000, Direction::BUY)},
    };
    Levels levels{
        {9900,
         {view(snap(1, 34201, 25, Direction::BUY), snap(2, 34202, 25, Direction::BUY)),
          view(snap(3, 34203, 40, Direction::BUY), snap(4, 34204, 40, Direction::BUY))}},
        {10000, {view(snap(1, 34201, 0, Direction::BUY), snap(4, 34204, 10, Direction::BUY))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();

    auto order_70 = find_synthetic(synthetics, Type::ORDER, 70);
    REQUIRE(order_70 != synthetics.end());
    CHECK(order_70->size == 40);
    // the hidden window between the two views, not market open
    CHECK(order_70->timestamp == ts(34202));

    // the pre-open 25 shares get a fake order at the open...
    auto fake = std::ranges::find_if(
        synthetics, [](Message const &m) { return m.type == Type::ORDER && m.order_id != 70; });
    REQUIRE(fake != synthetics.end());
    CHECK(fake->price == 9900);
    CHECK(fake->size == 25);
    CHECK(fake->timestamp == open_timestamp());

    // ...cancelled in the same hidden window that admits order 70
    auto fake_cancel = find_synthetic(synthetics, Type::CANCEL, fake->order_id);
    REQUIRE(fake_cancel != synthetics.end());
    CHECK(fake_cancel->size == 25);
    CHECK(fake_cancel->timestamp == ts(34202));
}

TEST_CASE("Orphan referenced during a view still open at the end of the data", "[synthetics]") {
    // 10100 never leaves the visible depth, so its only view is closed by the
    // end of the data (exit on the last line); the pre-open id 77 referenced
    // on that final line must still be matched to it
    NextOrderId next{.ids = {1000, 1010, 0}};
    Orphans orphans{
        .only_referenced = {orphan(3, 34203, Type::CANCEL, 77, 4, 10100, Direction::SELL)},
        .only_created = {orphan(1, 34201, Type::ORDER, 1000, 10, 10000, Direction::BUY),
                         orphan(2, 34202, Type::ORDER, 1010, 20, 10000, Direction::BUY)},
    };
    Levels levels{
        {10000, {view(snap(1, 34201, 0, Direction::BUY), snap(3, 34203, 30, Direction::BUY))}},
        {10100, {view(snap(1, 34201, 5, Direction::SELL), snap(3, 34203, 1, Direction::SELL))}},
    };

    auto synthetics = Synthetics{orphans, levels, next}();

    auto order_77 = find_synthetic(synthetics, Type::ORDER, 77);
    REQUIRE(order_77 != synthetics.end());
    CHECK(order_77->size == 4);
    CHECK(order_77->timestamp == open_timestamp());

    // the remaining share of the opening ask level still gets its fake
    auto fake = std::ranges::find_if(
        synthetics, [](Message const &m) { return m.type == Type::ORDER && m.order_id != 77; });
    REQUIRE(fake != synthetics.end());
    CHECK(fake->price == 10100);
    CHECK(fake->size == 1);
}

TEST_CASE("Hidden shrink with nothing left to cancel is an error", "[synthetics]") {
    // 10000 loses 5 hidden shares between its views but no fake or recorded
    // order is alive to absorb the shrink
    NextOrderId next{.ids = {1000, 0, 0}};
    Orphans orphans{};
    Levels levels{
        {10000,
         {view(snap(1, 34201, 0, Direction::BUY), snap(2, 34202, 10, Direction::BUY)),
          view(snap(3, 34203, 5, Direction::BUY), snap(3, 34203, 5, Direction::BUY))}},
    };

    REQUIRE_THROWS_AS((Synthetics{orphans, levels, next}), SyntheticsError);
}

// NOLINTEND(readability-magic-numbers, readability-function-cognitive-complexity)

} // namespace ome::tools::lobster
