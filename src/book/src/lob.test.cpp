#include "book/lob.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ome::book {

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

TEST_CASE("LOB structures test", "[lob]") {
    Limit limit{.price = 100, .quantity = 10};
    REQUIRE(limit.price == 100);
    REQUIRE(limit.quantity == 10);

    Order order{.order_id = 1, .limit = limit, .timestamp = {}};
    REQUIRE(order.order_id == 1);
    REQUIRE(order.limit.price == 100);
    REQUIRE(order.limit.quantity == 10);

    BuyOrder buy_order(order);
    REQUIRE(buy_order.order_id == 1);
    REQUIRE(buy_order.limit.price == 100);
    REQUIRE(buy_order.limit.quantity == 10);

    SellOrder sell_order(order);
    REQUIRE(sell_order.order_id == 1);
    REQUIRE(sell_order.limit.price == 100);
    REQUIRE(sell_order.limit.quantity == 10);
}

TEST_CASE("Book add and cancel test", "[lob]") {
    Book book;

    OrderId buy_id = 0;
    Price price = 100;

    book.add(BuyOrder{buy_id, {.price = price, .quantity = 10}, {}});
    REQUIRE(book.volume(price) == 10);

    OrderId sell_id = 1;
    book.add(SellOrder{sell_id, {.price = price, .quantity = 5}, {}});
    REQUIRE(book.volume(price) == 5);
    REQUIRE(book.cancel(sell_id) == false);

    REQUIRE(book.cancel(buy_id) == true);
    REQUIRE(book.volume(price) == 0);
}

TEST_CASE("Book spread test", "[lob]") {
    Book book;

    REQUIRE(book.spread() == std::make_pair(0, 0));

    book.add(BuyOrder{0, {.price = 100, .quantity = 10}, {}});
    REQUIRE(book.spread() == std::make_pair(100, 0));

    book.add(SellOrder{1, {.price = 110, .quantity = 10}, {}});
    REQUIRE(book.spread() == std::make_pair(100, 110));

    book.add(BuyOrder{2, {.price = 105, .quantity = 10}, {}});
    REQUIRE(book.spread() == std::make_pair(105, 110));

    book.add(SellOrder{3, {.price = 108, .quantity = 10}, {}});
    REQUIRE(book.spread() == std::make_pair(105, 108));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

} // namespace ome::book
