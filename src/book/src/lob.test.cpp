#include "book/lob.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ome::book {

// NOLINTBEGIN(readability-magic-numbers)

TEST_CASE("Add crossing spread behavior", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));

    // sell at best bid executes against existing bid
    REQUIRE(book.add(SellOrder{{1, 100, 5}}));
    REQUIRE(book.volume(100) == 5);

    // sell below best bid executes against existing bid
    REQUIRE(book.add(SellOrder{{2, 99, 5}}));
    REQUIRE(book.volume(100) == 0);

    // ask above bid becomes resting ask
    REQUIRE(book.add(SellOrder{{3, 110, 5}}));
    REQUIRE(book.volume(110) == 5);

    // buy at ask executes against resting ask
    REQUIRE(book.add(BuyOrder{{4, 110, 5}}));
    REQUIRE(book.volume(110) == 0);

    // buy above empty book enters resting bid
    REQUIRE(book.add(BuyOrder{{5, 111, 5}}));
    REQUIRE(book.volume(111) == 5);
}

TEST_CASE("Add multiple orders at same price level", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));
    REQUIRE(book.add(BuyOrder{{1, 100, 15}}));
    REQUIRE(book.volume(100) == 25);

    REQUIRE(book.add(SellOrder{{2, 110, 5}}));
    REQUIRE(book.add(SellOrder{{3, 110, 7}}));
    REQUIRE(book.volume(110) == 12);
}

TEST_CASE("Cancel non-existent order", "[lob]") {
    Book book;
    REQUIRE(!book.cancel(999));
}

TEST_CASE("Cancel one of multiple orders at same price", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));
    REQUIRE(book.add(BuyOrder{{1, 100, 20}}));
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

    REQUIRE(book.add(SellOrder{{0, 110, 10}}));
    REQUIRE(book.add(SellOrder{{1, 110, 5}}));
    REQUIRE(book.volume(110) == 15);

    REQUIRE(book.cancel(0));
    REQUIRE(book.volume(110) == 5);

    REQUIRE(book.cancel(1));
    REQUIRE(book.volume(110) == 0);
}

TEST_CASE("Partial cancel", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));
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

    REQUIRE(book.add(SellOrder{{0, 110, 20}}));
    REQUIRE(book.volume(110) == 20);

    REQUIRE(book.cancel(0, 8));
    REQUIRE(book.volume(110) == 12);
}

TEST_CASE("Add buy order full match", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{{0, 100, 10}}));
    REQUIRE(book.add(BuyOrder{{1, 100, 10}}));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Add buy order partial match", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{{0, 100, 5}}));
    REQUIRE(book.add(BuyOrder{{1, 100, 10}}));
    REQUIRE(book.volume(100) == 5);
}

TEST_CASE("Add buy across multiple orders at same price", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{{0, 100, 5}}));
    REQUIRE(book.add(SellOrder{{1, 100, 5}}));
    REQUIRE(book.volume(100) == 10);

    REQUIRE(book.add(BuyOrder{{2, 100, 8}}));
    REQUIRE(book.volume(100) == 2);
}

TEST_CASE("Add buy across multiple price levels", "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{{0, 100, 5}}));
    REQUIRE(book.add(SellOrder{{1, 105, 5}}));

    REQUIRE(book.add(BuyOrder{{2, 105, 10}}));
    REQUIRE(book.volume(100) == 0);
    REQUIRE(book.volume(105) == 0);
}

TEST_CASE("Add sell order full match", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));
    REQUIRE(book.add(SellOrder{{1, 100, 10}}));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Add sell order partial match", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 5}}));
    REQUIRE(book.add(SellOrder{{1, 100, 10}}));
    REQUIRE(book.volume(100) == 5);
}

TEST_CASE("Add sell across multiple bid orders", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 5}}));
    REQUIRE(book.add(BuyOrder{{1, 100, 5}}));
    REQUIRE(book.volume(100) == 10);

    REQUIRE(book.add(SellOrder{{2, 100, 7}}));
    REQUIRE(book.volume(100) == 3);
}

TEST_CASE("Add sell across multiple price levels", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 105, 5}}));
    REQUIRE(book.add(BuyOrder{{1, 100, 5}}));

    REQUIRE(book.add(SellOrder{{2, 100, 10}}));
    REQUIRE(book.volume(105) == 0);
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Add on empty book", "[lob]") {
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));
    REQUIRE(book.volume(100) == 10);
    REQUIRE(book.add(SellOrder{{1, 100, 10}}));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Volume edge cases", "[lob]") {
    Book book;

    // empty book
    REQUIRE(book.volume(100) == 0);
    REQUIRE(book.volume(0) == 0);

    // price with no orders
    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));
    REQUIRE(book.volume(99) == 0);
    REQUIRE(book.volume(101) == 0);
}

TEST_CASE("Book spread test", "[lob]") {
    Book book;

    REQUIRE(book.spread() == std::make_pair(0, 0));

    REQUIRE(book.add(BuyOrder{{0, 100, 10}}));
    REQUIRE(book.spread() == std::make_pair(100, 0));

    REQUIRE(book.add(SellOrder{{1, 110, 10}}));
    REQUIRE(book.spread() == std::make_pair(100, 110));

    REQUIRE(book.add(BuyOrder{{2, 105, 10}}));
    REQUIRE(book.spread() == std::make_pair(105, 110));

    REQUIRE(book.add(SellOrder{{3, 108, 10}}));
    REQUIRE(book.spread() == std::make_pair(105, 108));
}

TEST_CASE("Sell exactly matches first bid, second bid at same level survives and is cancellable",
          "[lob]") {
    // Regression: the execute loop previously always erased the price level after the inner
    // loop, even when orders remained. A sell that exactly consumes the first order at a level
    // would erase the level while the second order was still live. Calling cancel on that
    // surviving order would then fail to find its price level and hit std::unreachable().
    Book book;

    REQUIRE(book.add(BuyOrder{{0, 100, 5}}));
    REQUIRE(book.add(BuyOrder{{1, 100, 5}}));
    REQUIRE(book.volume(100) == 10);

    // sell exactly matches the first bid — inner loop exits with order.quantity == 0
    // but order id=1 is still alive at level 100
    REQUIRE(book.add(SellOrder{{2, 100, 5}}));
    REQUIRE(book.volume(100) == 5);

    // level 100 must still exist; cancel must not crash via std::unreachable()
    REQUIRE(book.cancel(1));
    REQUIRE(book.volume(100) == 0);
}

TEST_CASE("Buy exactly matches first ask, second ask at same level survives and is cancellable",
          "[lob]") {
    Book book;

    REQUIRE(book.add(SellOrder{{0, 100, 5}}));
    REQUIRE(book.add(SellOrder{{1, 100, 5}}));
    REQUIRE(book.volume(100) == 10);

    REQUIRE(book.add(BuyOrder{{2, 100, 5}}));
    REQUIRE(book.volume(100) == 5);

    REQUIRE(book.cancel(1));
    REQUIRE(book.volume(100) == 0);
}

// NOLINTEND(readability-magic-numbers)

} // namespace ome::book
