#include "book/lob.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ome::book {

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

TEST_CASE("Add crossing spread error", "[lob]") {
    Book book;

    // buy at 100, sell at 100 crosses spread
    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(!book.add(SellOrder{1, {.price = 100, .quantity = 5}, {}}));

    // sell below best bid crosses spread
    REQUIRE(!book.add(SellOrder{2, {.price = 99, .quantity = 5}, {}}));

    // valid sell above best bid
    REQUIRE(book.add(SellOrder{3, {.price = 110, .quantity = 5}, {}}));

    // buy at best ask crosses spread
    REQUIRE(!book.add(BuyOrder{4, {.price = 110, .quantity = 5}, {}}));

    // buy above best ask crosses spread
    REQUIRE(!book.add(BuyOrder{5, {.price = 111, .quantity = 5}, {}}));
}

TEST_CASE("Add multiple orders at same price level", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.add(BuyOrder{1, {.price = 100, .quantity = 15}, {}}));
    REQUIRE(book.volume(100) == 25);

    REQUIRE(book.add(SellOrder{2, {.price = 110, .quantity = 5}, {}}));
    REQUIRE(book.add(SellOrder{3, {.price = 110, .quantity = 7}, {}}));
    REQUIRE(book.volume(110) == 12);
}

TEST_CASE("Cancel non-existent order", "[lob]") {
    Book book;
    REQUIRE(!book.cancel(999));
}

TEST_CASE("Cancel one of multiple orders at same price", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.add(BuyOrder{1, {.price = 100, .quantity = 20}, {}}));
    REQUIRE(book.volume(100) == 30);

    // cancel first order, volume should drop by 10
    REQUIRE(book.cancel(0));
    REQUIRE(book.volume(100) == 20);

    // cancel last order at this level, level should be removed
    REQUIRE(book.cancel(1));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Cancel sell orders", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{0, {.price = 110, .quantity = 10}, {}}));
    REQUIRE(book.add(SellOrder{1, {.price = 110, .quantity = 5}, {}}));
    REQUIRE(book.volume(110) == 15);

    REQUIRE(book.cancel(0));
    REQUIRE(book.volume(110) == 5);

    REQUIRE(book.cancel(1));
    REQUIRE(book.volume(110) == 0);
}

TEST_CASE("Partial cancel", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.volume(100) == 10);

    // cancel 3 of 10
    REQUIRE(book.cancel(0, 3));
    REQUIRE(book.volume(100) == 7);

    // cancel remaining 7
    REQUIRE(book.cancel(0, 7));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Partial cancel non-existent order", "[lob]") {
    Book book;
    REQUIRE(!book.cancel(999, 5));
}

TEST_CASE("Partial cancel sell order", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{0, {.price = 110, .quantity = 20}, {}}));
    REQUIRE(book.volume(110) == 20);

    REQUIRE(book.cancel(0, 8));
    REQUIRE(book.volume(110) == 12);
}

TEST_CASE("Execute buy order full match", "[lob]") {
    Book book;

    // place asks
    REQUIRE(book.add(SellOrder{0, {.price = 100, .quantity = 10}, {}}));

    // execute buy that fully matches
    REQUIRE(book.execute(BuyOrder{1, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Execute buy order partial liquidity", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{0, {.price = 100, .quantity = 5}, {}}));

    // try to buy more than available
    REQUIRE(!book.execute(BuyOrder{1, {.price = 100, .quantity = 10}, {}}));
}

TEST_CASE("Execute buy across multiple orders at same price", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{0, {.price = 100, .quantity = 5}, {}}));
    REQUIRE(book.add(SellOrder{1, {.price = 100, .quantity = 5}, {}}));
    REQUIRE(book.volume(100) == 10);

    REQUIRE(book.execute(BuyOrder{2, {.price = 100, .quantity = 8}, {}}));
    REQUIRE(book.volume(100) == 2);
}

TEST_CASE("Execute buy across multiple price levels", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{0, {.price = 100, .quantity = 5}, {}}));
    REQUIRE(book.add(SellOrder{1, {.price = 105, .quantity = 5}, {}}));

    // buy 10 should consume both price levels
    REQUIRE(book.execute(BuyOrder{2, {.price = 105, .quantity = 10}, {}}));
    REQUIRE(book.volume(100) == 0);
    REQUIRE(book.volume(105) == 0);
}

TEST_CASE("Execute sell order full match", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));

    REQUIRE(book.execute(SellOrder{1, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Execute sell order partial liquidity", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 5}, {}}));

    REQUIRE(!book.execute(SellOrder{1, {.price = 100, .quantity = 10}, {}}));
}

TEST_CASE("Execute sell across multiple bid orders", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 5}, {}}));
    REQUIRE(book.add(BuyOrder{1, {.price = 100, .quantity = 5}, {}}));
    REQUIRE(book.volume(100) == 10);

    REQUIRE(book.execute(SellOrder{2, {.price = 100, .quantity = 7}, {}}));
    REQUIRE(book.volume(100) == 3);
}

TEST_CASE("Execute sell across multiple price levels", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{0, {.price = 105, .quantity = 5}, {}}));
    REQUIRE(book.add(BuyOrder{1, {.price = 100, .quantity = 5}, {}}));

    // sell 10 should consume both price levels (best bid first: 105, then 100)
    REQUIRE(book.execute(SellOrder{2, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.volume(105) == 0);
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Execute on empty book", "[lob]") {
    Book book;

    REQUIRE(!book.execute(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(!book.execute(SellOrder{1, {.price = 100, .quantity = 10}, {}}));
}

TEST_CASE("Volume edge cases", "[lob]") {
    Book book;

    // empty book
    REQUIRE(book.volume(100) == 0);
    REQUIRE(book.volume(0) == 0);

    // price with no orders
    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.volume(99) == 0);
    REQUIRE(book.volume(101) == 0);
}

TEST_CASE("Book spread test", "[lob]") {
    Book book;

    REQUIRE(book.spread() == std::make_pair(0, 0));

    REQUIRE(book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}}));
    REQUIRE(book.spread() == std::make_pair(100, 0));

    REQUIRE(book.add(SellOrder{1, {.price = 110, .quantity = 10}, {}}));
    REQUIRE(book.spread() == std::make_pair(100, 110));

    REQUIRE(book.add(BuyOrder{2, {.price = 105, .quantity = 10}, {}}));
    REQUIRE(book.spread() == std::make_pair(105, 110));

    REQUIRE(book.add(SellOrder{3, {.price = 108, .quantity = 10}, {}}));
    REQUIRE(book.spread() == std::make_pair(105, 108));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

} // namespace ome::book
