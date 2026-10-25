#include "book/engine.hpp"

#include <book/order.hpp>
#include <testing/catch_expected.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace ome::book {

// NOLINTBEGIN(readability-magic-numbers)

namespace {
struct Fill {
    OrderId order_id{};
    Price price{};
    std::uint64_t quantity{};
};

/// on_fill callback appending every fill to fills.
auto recorder(std::vector<Fill> &fills) {
    return
        [&fills](OrderId id, Order const & /*order*/, Price price, std::uint64_t quantity) -> void {
            fills.push_back({.order_id = id, .price = price, .quantity = quantity});
        };
}
} // namespace

TEST_CASE("Add assigns sequential ids starting at 1", "[lob]") {
    Engine book;

    REQUIRE(book.add(Order{{10}, Side::Buy, 100}) == 1);
    REQUIRE(book.add(Order{{5}, Side::Sell, 110}) == 2);

    // fully matched orders still consume an id
    REQUIRE(book.add(Order{{10}, Side::Sell, 100}) == 3);
    REQUIRE(book.add(Order{{10}, Side::Buy, 100}) == 4);
}

TEST_CASE("Add crossing spread behavior", "[lob]") {
    Engine book;

    REQUIRE(book.add(Order{{10}, Side::Buy, 100}));

    // sell at best bid executes against existing bid
    REQUIRE(book.add(Order{{5}, Side::Sell, 100}));
    REQUIRE(book.volume(100) == 5);

    // sell below best bid executes against existing bid
    REQUIRE(book.add(Order{{5}, Side::Sell, 99}));
    REQUIRE(book.volume(100) == 0);

    // ask above bid becomes resting ask
    REQUIRE(book.add(Order{{5}, Side::Sell, 110}));
    REQUIRE(book.volume(110) == 5);

    // buy at ask executes against resting ask
    REQUIRE(book.add(Order{{5}, Side::Buy, 110}));
    REQUIRE(book.volume(110) == 0);

    // buy above empty book enters resting bid
    REQUIRE(book.add(Order{{5}, Side::Buy, 111}));
    REQUIRE(book.volume(111) == 5);
}

TEST_CASE("Cancel and modify of an unknown id fail", "[lob]") {
    Engine book;
    REQUIRE(!book.cancel(999));
    REQUIRE(!book.modify(999, {.qty = 5}));
}

TEST_CASE("Cancel removes orders on either side", "[lob]") {
    Engine book;

    auto first = book.add(Order{{10}, Side::Buy, 100});
    auto second = book.add(Order{{20}, Side::Buy, 100});
    auto ask = book.add(Order{{5}, Side::Sell, 110});
    REQUIRE(book.volume(100) == 30);

    REQUIRE(book.cancel(*first));
    REQUIRE(book.volume(100) == 20);

    // last order at the level
    REQUIRE(book.cancel(*second));
    REQUIRE(book.volume(100) == 0);

    REQUIRE(book.cancel(*ask));
    REQUIRE(book.volume(110) == 0);

    // already cancelled
    REQUIRE(!book.cancel(*first));
}

TEST_CASE("Modify reduces quantity to the new total", "[lob]") {
    Engine book;

    auto bid = book.add(Order{{10}, Side::Buy, 100});
    auto ask = book.add(Order{{20}, Side::Sell, 110});

    REQUIRE(book.modify(*bid, {.qty = 7}));
    REQUIRE(book.volume(100) == 7);

    REQUIRE(book.modify(*bid, {.qty = 3}));
    REQUIRE(book.volume(100) == 3);

    REQUIRE(book.modify(*ask, {.qty = 12}));
    REQUIRE(book.volume(110) == 12);
}

TEST_CASE("Add buy order partial match", "[lob]") {
    Engine book;

    REQUIRE(book.add(Order{{5}, Side::Sell, 100}));
    REQUIRE(book.add(Order{{10}, Side::Buy, 100}));
    REQUIRE(book.volume(100) == 5);
}

TEST_CASE("Add sell order partial match", "[lob]") {
    Engine book;

    REQUIRE(book.add(Order{{5}, Side::Buy, 100}));
    REQUIRE(book.add(Order{{10}, Side::Sell, 100}));
    REQUIRE(book.volume(100) == 5);
}

TEST_CASE("Add sell across multiple price levels", "[lob]") {
    Engine book;

    REQUIRE(book.add(Order{{5}, Side::Buy, 105}));
    REQUIRE(book.add(Order{{5}, Side::Buy, 100}));

    REQUIRE(book.add(Order{{10}, Side::Sell, 100}));
    REQUIRE(book.volume(105) == 0);
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Volume edge cases", "[lob]") {
    Engine book;

    // empty book
    REQUIRE(book.volume(100) == 0);
    REQUIRE(book.volume(0) == 0);

    // price with no orders
    REQUIRE(book.add(Order{{10}, Side::Buy, 100}));
    REQUIRE(book.volume(99) == 0);
    REQUIRE(book.volume(101) == 0);
}

TEST_CASE("Book spread test", "[lob]") {
    Engine book;

    REQUIRE(book.spread() == std::make_pair(0, 0));

    REQUIRE(book.add(Order{{10}, Side::Buy, 100}));
    REQUIRE(book.spread() == std::make_pair(100, 0));

    REQUIRE(book.add(Order{{10}, Side::Sell, 110}));
    REQUIRE(book.spread() == std::make_pair(100, 110));

    REQUIRE(book.add(Order{{10}, Side::Buy, 105}));
    REQUIRE(book.spread() == std::make_pair(105, 110));

    REQUIRE(book.add(Order{{10}, Side::Sell, 108}));
    REQUIRE(book.spread() == std::make_pair(105, 108));
}

TEST_CASE("Exactly matching the first order at a level keeps the rest cancellable", "[lob]") {
    // Regression: the execute loop previously always erased the price level after the inner
    // loop, even when orders remained. An order that exactly consumes the first order at a level
    // would erase the level while the second order was still live. Calling cancel on that
    // surviving order would then fail to find its price level and hit std::unreachable().
    Engine book;

    SECTION("sell against bids") {
        REQUIRE(book.add(Order{{5}, Side::Buy, 100}));
        auto survivor = book.add(Order{{5}, Side::Buy, 100});

        REQUIRE(book.add(Order{{5}, Side::Sell, 100}));
        REQUIRE(book.volume(100) == 5);

        REQUIRE(book.cancel(*survivor));
        REQUIRE(book.volume(100) == 0);
    }

    SECTION("buy against asks") {
        REQUIRE(book.add(Order{{5}, Side::Sell, 100}));
        auto survivor = book.add(Order{{5}, Side::Sell, 100});

        REQUIRE(book.add(Order{{5}, Side::Buy, 100}));
        REQUIRE(book.volume(100) == 5);

        REQUIRE(book.cancel(*survivor));
        REQUIRE(book.volume(100) == 0);
    }
}

TEST_CASE("on_fill callback: full single fill", "[lob][callback]") {
    Engine book;

    auto resting = book.add(Order{{10}, Side::Sell, 100});

    std::vector<Fill> fills;
    REQUIRE(book.add(Order{{10}, Side::Buy, 100}, recorder(fills)));

    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].order_id == *resting);
    REQUIRE(fills[0].price == 100);
    REQUIRE(fills[0].quantity == 10);
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("on_fill callback: partial fill of resting order", "[lob][callback]") {
    Engine book;

    auto resting = book.add(Order{{10}, Side::Sell, 100});

    std::vector<Fill> fills;
    REQUIRE(book.add(Order{{4}, Side::Buy, 100}, recorder(fills)));

    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].order_id == *resting);
    REQUIRE(fills[0].quantity == 4);
    REQUIRE(book.volume(100) == 6);
}

TEST_CASE("on_fill callback: multiple resting orders at same price level", "[lob][callback]") {
    Engine book;

    auto first = book.add(Order{{5}, Side::Sell, 100});
    auto second = book.add(Order{{5}, Side::Sell, 100});

    std::vector<Fill> fills;
    REQUIRE(book.add(Order{{10}, Side::Buy, 100}, recorder(fills)));

    REQUIRE(fills.size() == 2);
    REQUIRE(fills[0].order_id == *first);
    REQUIRE(fills[0].quantity == 5);
    REQUIRE(fills[1].order_id == *second);
    REQUIRE(fills[1].quantity == 5);
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("on_fill callback: fills across multiple price levels", "[lob][callback]") {
    Engine book;

    auto first = book.add(Order{{5}, Side::Sell, 100});
    auto second = book.add(Order{{5}, Side::Sell, 105});

    std::vector<Fill> fills;
    REQUIRE(book.add(Order{{10}, Side::Buy, 105}, recorder(fills)));

    REQUIRE(fills.size() == 2);
    REQUIRE(fills[0].order_id == *first);
    REQUIRE(fills[0].price == 100);
    REQUIRE(fills[1].order_id == *second);
    REQUIRE(fills[1].price == 105);
    REQUIRE(book.volume(100) == 0);
    REQUIRE(book.volume(105) == 0);
}

TEST_CASE("on_fill callback: sell side fills", "[lob][callback]") {
    Engine book;

    auto first = book.add(Order{{5}, Side::Buy, 100});
    auto second = book.add(Order{{5}, Side::Buy, 100});

    std::vector<Fill> fills;
    REQUIRE(book.add(Order{{7}, Side::Sell, 100}, recorder(fills)));

    REQUIRE(fills.size() == 2);
    REQUIRE(fills[0].order_id == *first);
    REQUIRE(fills[0].quantity == 5);
    REQUIRE(fills[1].order_id == *second);
    REQUIRE(fills[1].quantity == 2);
    REQUIRE(book.volume(100) == 3);
}

TEST_CASE("Modify without changes fails", "[lob][modify]") {
    Engine book;

    auto bid = book.add(Order{{10}, Side::Buy, 100});

    REQUIRE(!book.modify(*bid, {}));
    REQUIRE(!book.modify(*bid, {.qty = 10}));
    REQUIRE(!book.modify(*bid, {.price = 100}));
    REQUIRE(!book.modify(*bid, {.price = 100, .qty = 10}));
    REQUIRE(book.volume(100) == 10);
}

TEST_CASE("Modify to zero quantity fails", "[lob][modify]") {
    Engine book;

    auto bid = book.add(Order{{10}, Side::Buy, 100});

    REQUIRE(!book.modify(*bid, {.qty = 0}));
    REQUIRE(!book.modify(*bid, {.price = 101, .qty = 0}));
    REQUIRE(book.volume(100) == 10);
    REQUIRE(book.spread() == std::make_pair(100, 0));
}

TEST_CASE("Modify keeps priority only when reducing quantity", "[lob][modify]") {
    Engine book;

    auto first = book.add(Order{{10}, Side::Buy, 100});
    auto second = book.add(Order{{10}, Side::Buy, 100});

    SECTION("reduce") {
        REQUIRE(book.modify(*first, {.qty = 5}));
        REQUIRE(book.volume(100) == 15);
        std::vector<Fill> fills;
        REQUIRE(book.add(Order{{1}, Side::Sell, 100}, recorder(fills)));
        REQUIRE(fills.at(0).order_id == *first);
    }

    SECTION("increase") {
        REQUIRE(book.modify(*first, {.qty = 15}));
        REQUIRE(book.volume(100) == 25);
        std::vector<Fill> fills;
        REQUIRE(book.add(Order{{1}, Side::Sell, 100}, recorder(fills)));
        REQUIRE(fills.at(0).order_id == *second);
    }

    SECTION("same price, smaller quantity") {
        REQUIRE(book.modify(*first, {.price = 100, .qty = 5}));
        REQUIRE(book.volume(100) == 15);
        std::vector<Fill> fills;
        REQUIRE(book.add(Order{{1}, Side::Sell, 100}, recorder(fills)));
        REQUIRE(fills.at(0).order_id == *first);
    }
}

TEST_CASE("Modify price moves the order to the back of the new level", "[lob][modify]") {
    Engine book;

    auto moved = book.add(Order{{10}, Side::Buy, 100});
    auto resting = book.add(Order{{5}, Side::Buy, 105});

    REQUIRE(book.modify(*moved, {.price = 105, .qty = 7}));
    REQUIRE(book.volume(100) == 0);
    REQUIRE(book.volume(105) == 12);
    REQUIRE(book.spread() == std::make_pair(105, 0));
    std::vector<Fill> fills;
    REQUIRE(book.add(Order{{1}, Side::Sell, 105}, recorder(fills)));
    REQUIRE(fills.at(0).order_id == *resting);

    // the order keeps its id
    REQUIRE(book.cancel(*moved));
    REQUIRE(book.volume(105) == 4);
}

TEST_CASE("Modify price across the spread executes the order", "[lob][modify]") {
    Engine book;

    auto ask = book.add(Order{{5}, Side::Sell, 110});
    auto bid = book.add(Order{{10}, Side::Buy, 100});

    std::vector<Fill> fills;
    REQUIRE(book.modify(*bid, {.price = 110}, recorder(fills)));
    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].order_id == *ask);
    REQUIRE(fills[0].price == 110);
    REQUIRE(fills[0].quantity == 5);
    REQUIRE(book.volume(110) == 5);
    REQUIRE(book.spread() == std::make_pair(110, 0));
    REQUIRE(!book.cancel(*ask));
    REQUIRE(book.cancel(*bid));
}

// NOLINTEND(readability-magic-numbers)

} // namespace ome::book
