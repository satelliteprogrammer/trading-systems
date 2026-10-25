// Contract tests for concepts::OrderBook. Every book the Engine can run on goes in Books.

#include <book/books/node/book.hpp>
#include <book/books/node/indexedbook.hpp>
#include <book/concepts.hpp>
#include <book/order.hpp>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_container_properties.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <ranges>
#include <tuple>
#include <vector>

namespace ome::book::books {

// NOLINTBEGIN(readability-magic-numbers)

namespace {

using Books = std::tuple<node::Book, node::IndexedBook>;

using Catch::Matchers::IsEmpty;
using Catch::Matchers::RangeEquals;

template <Side S>
auto make_order(OrderId id, Price price, std::uint64_t quantity = 1) -> RestingOrder {
    return RestingOrder{Order{{quantity}, S, price}, id};
}

// Free helpers so tests don't need `book.template begin<S>()` on a dependent type.

template <Side S, typename B> auto begin(B &book) { return book.template begin<S>(); }
template <Side S, typename B> auto end(B &book) { return book.template end<S>(); }
template <Side S, typename B> auto levels(B &book) { return book.template levels<S>(); }
template <Side S, typename B> auto find_level(B &book, Price price) {
    return book.template find<S>(price);
}

template <Side S, concepts::OrderBook B>
auto emplace(B &book, OrderId id, Price price, std::uint64_t quantity = 1) {
    return book.template emplace<S>(make_order<S>(id, price, quantity));
}

/// Order ids on side S, in iteration order.
template <Side S, typename B> auto ids(B const &book) {
    return std::ranges::subrange(begin<S>(book), end<S>(book)) |
           std::views::transform(&RestingOrder::order_id);
}

/// Level quantities on side S, in iteration order.
template <Side S, typename B> auto level_qtys(B const &book) {
    return levels<S>(book) | std::views::transform([](auto const &level) { return level.qty; });
}

} // namespace

TEMPLATE_LIST_TEST_CASE("Emplace returns an iterator to the emplaced order", "[book]", Books) {
    TestType book;

    SECTION("buy") {
        auto it = emplace<Side::Buy>(book, 42, 100, 7);
        REQUIRE(it->order_id == 42);
        REQUIRE(it->price == 100);
        REQUIRE(it->quantity == 7);
        REQUIRE(it == book.find(42));
        REQUIRE(it == begin<Side::Buy>(book));
        REQUIRE(++it == end<Side::Buy>(book));
    }

    SECTION("sell") {
        auto it = emplace<Side::Sell>(book, 42, 110, 7);
        REQUIRE(it->order_id == 42);
        REQUIRE(it->price == 110);
        REQUIRE(it->quantity == 7);
        REQUIRE(it == book.find(42));
        REQUIRE(it == begin<Side::Sell>(book));
        REQUIRE(++it == end<Side::Sell>(book));
    }
}

TEMPLATE_LIST_TEST_CASE("Each side iterates in price-time priority", "[book]", Books) {
    TestType book;
    emplace<Side::Buy>(book, 1, 100);
    emplace<Side::Sell>(book, 2, 110);
    emplace<Side::Buy>(book, 3, 102);
    emplace<Side::Sell>(book, 4, 108);
    emplace<Side::Buy>(book, 5, 100);
    emplace<Side::Sell>(book, 6, 110);
    emplace<Side::Buy>(book, 7, 101);
    emplace<Side::Sell>(book, 8, 109);

    // bids highest price first, asks lowest price first, FIFO within a price
    REQUIRE_THAT(ids<Side::Buy>(book), RangeEquals<OrderId>({3, 7, 1, 5}));
    REQUIRE_THAT(ids<Side::Sell>(book), RangeEquals<OrderId>({4, 8, 2, 6}));
}

TEMPLATE_LIST_TEST_CASE("Post-increment returns the previous position", "[book]", Books) {
    TestType book;
    emplace<Side::Buy>(book, 1, 100);
    emplace<Side::Buy>(book, 2, 100);

    auto it = begin<Side::Buy>(book);
    auto const old = it++;
    REQUIRE(old->order_id == 1);
    REQUIRE(it->order_id == 2);
}

TEMPLATE_LIST_TEST_CASE("Orders can be modified through an iterator", "[book]", Books) {
    // the Engine shrinks resting orders in place on partial fills and partial cancels
    TestType book;
    emplace<Side::Sell>(book, 1, 110, 10);

    book.find(1)->quantity = 4;
    REQUIRE(begin<Side::Sell>(book)->quantity == 4);
}

TEMPLATE_LIST_TEST_CASE("Erase returns the next order on the same side", "[book]", Books) {
    // bids: 102 [1], 101 [2 3 4], 100 [5]
    // asks: 110 [6], 111 [7]
    TestType book;
    emplace<Side::Buy>(book, 1, 102);
    emplace<Side::Buy>(book, 2, 101);
    emplace<Side::Buy>(book, 3, 101);
    emplace<Side::Buy>(book, 4, 101);
    emplace<Side::Buy>(book, 5, 100);
    emplace<Side::Sell>(book, 6, 110);
    emplace<Side::Sell>(book, 7, 111);

    SECTION("order in the middle of a level") {
        REQUIRE(book.erase(book.find(3))->order_id == 4);
        REQUIRE_THAT(ids<Side::Buy>(book), RangeEquals<OrderId>({1, 2, 4, 5}));
    }

    SECTION("last order of a level that keeps others") {
        REQUIRE(book.erase(book.find(4))->order_id == 5);
        REQUIRE_THAT(ids<Side::Buy>(book), RangeEquals<OrderId>({1, 2, 3, 5}));
    }

    SECTION("only order at a level") {
        REQUIRE(book.erase(book.find(1))->order_id == 2);
        REQUIRE_THAT(ids<Side::Buy>(book), RangeEquals<OrderId>({2, 3, 4, 5}));
    }

    SECTION("last order on the side") {
        REQUIRE(book.erase(book.find(5)) == end<Side::Buy>(book));
        REQUIRE_THAT(ids<Side::Buy>(book), RangeEquals<OrderId>({1, 2, 3, 4}));
    }

    SECTION("only order at an ask level") {
        REQUIRE(book.erase(book.find(6))->order_id == 7);
        REQUIRE_THAT(ids<Side::Sell>(book), RangeEquals<OrderId>({7}));
        REQUIRE_THAT(ids<Side::Buy>(book), RangeEquals<OrderId>({1, 2, 3, 4, 5}));
    }

    SECTION("last order on the ask side") {
        book.erase(book.find(6));
        REQUIRE(book.erase(book.find(7)) == end<Side::Sell>(book));
        REQUIRE_THAT(ids<Side::Sell>(book), IsEmpty());
    }
}

TEMPLATE_LIST_TEST_CASE("Erased orders can no longer be found", "[book]", Books) {
    TestType book;
    emplace<Side::Buy>(book, 1, 100);
    emplace<Side::Buy>(book, 2, 100);

    book.erase(book.find(1));
    REQUIRE(book.find(1) == end<Side::Buy>(book));
    REQUIRE(book.find(2)->order_id == 2);
}

TEMPLATE_LIST_TEST_CASE("Find of an unknown id returns end", "[book]", Books) {
    // the Engine detects a miss by comparing against end<Side::Buy>(), whatever the side
    TestType book;

    SECTION("empty book") {
        REQUIRE(book.find(1) == end<Side::Buy>(book));
        REQUIRE(book.find(1) == end<Side::Sell>(book));
    }

    SECTION("non-empty book") {
        emplace<Side::Buy>(book, 1, 100);
        emplace<Side::Sell>(book, 2, 110);
        REQUIRE(book.find(999) == end<Side::Buy>(book));
        REQUIRE(book.find(999) == end<Side::Sell>(book));
    }
}

TEMPLATE_LIST_TEST_CASE("Levels iterate in price priority", "[book][level]", Books) {
    TestType book;
    emplace<Side::Buy>(book, 1, 100, 1);
    emplace<Side::Buy>(book, 2, 102, 2);
    emplace<Side::Buy>(book, 3, 101, 3);
    emplace<Side::Sell>(book, 4, 111, 4);
    emplace<Side::Sell>(book, 5, 110, 5);

    REQUIRE_THAT(level_qtys<Side::Buy>(book), RangeEquals<std::uint64_t>({2, 3, 1}));
    REQUIRE_THAT(level_qtys<Side::Sell>(book), RangeEquals<std::uint64_t>({5, 4}));
}

TEMPLATE_LIST_TEST_CASE("Levels track the total quantity of their orders", "[book][level]", Books) {
    TestType book;
    emplace<Side::Buy>(book, 1, 100, 10);
    emplace<Side::Buy>(book, 2, 100, 20);
    emplace<Side::Buy>(book, 3, 99, 5);

    REQUIRE(find_level<Side::Buy>(book, 100)->qty == 30);
    REQUIRE(find_level<Side::Buy>(book, 99)->qty == 5);

    book.erase(book.find(1));
    REQUIRE(find_level<Side::Buy>(book, 100)->qty == 20);

    // erasing the last order removes the level
    book.erase(book.find(2));
    REQUIRE(find_level<Side::Buy>(book, 100) == levels<Side::Buy>(book).end());
    REQUIRE_THAT(level_qtys<Side::Buy>(book), RangeEquals<std::uint64_t>({5}));
}

TEMPLATE_LIST_TEST_CASE("Find of an unknown price returns the end of levels", "[book][level]",
                        Books) {
    TestType book;

    SECTION("empty book") {
        REQUIRE(find_level<Side::Buy>(book, 100) == levels<Side::Buy>(book).end());
        REQUIRE(find_level<Side::Sell>(book, 100) == levels<Side::Sell>(book).end());
    }

    SECTION("price only on the other side") {
        emplace<Side::Sell>(book, 1, 100);
        REQUIRE(find_level<Side::Buy>(book, 100) == levels<Side::Buy>(book).end());
        REQUIRE(find_level<Side::Sell>(book, 101) == levels<Side::Sell>(book).end());
    }
}

TEMPLATE_LIST_TEST_CASE("A level iterator can be made from an order iterator", "[book][level]",
                        Books) {
    // the Engine updates the level's qty when it shrinks an order in place
    TestType book;
    emplace<Side::Sell>(book, 1, 110, 10);
    emplace<Side::Sell>(book, 2, 111, 7);

    auto const level = typename TestType::level_iterator{book.find(2)};
    REQUIRE(level == find_level<Side::Sell>(book, 111));

    level->qty -= 3;
    REQUIRE(find_level<Side::Sell>(book, 111)->qty == 4);
}

// Edge cases

TEMPLATE_LIST_TEST_CASE("An empty side has begin == end", "[book][edge]", Books) {
    TestType book;
    REQUIRE(begin<Side::Buy>(book) == end<Side::Buy>(book));
    REQUIRE(begin<Side::Sell>(book) == end<Side::Sell>(book));

    // the other side being populated changes nothing
    emplace<Side::Sell>(book, 1, 110);
    REQUIRE(begin<Side::Buy>(book) == end<Side::Buy>(book));
}

TEMPLATE_LIST_TEST_CASE("The iterator returned by emplace walks into worse levels", "[book][edge]",
                        Books) {
    TestType book;
    emplace<Side::Buy>(book, 1, 99);
    auto it = emplace<Side::Buy>(book, 2, 100);

    REQUIRE((++it)->order_id == 1);
    REQUIRE(++it == end<Side::Buy>(book));
}

TEMPLATE_LIST_TEST_CASE("Iterators stay valid across emplace", "[book][edge]", Books) {
    TestType book;
    auto it = emplace<Side::Buy>(book, 1, 100);

    emplace<Side::Buy>(book, 2, 101); // better level, before it
    emplace<Side::Buy>(book, 3, 100); // same level, after it
    emplace<Side::Buy>(book, 4, 99);  // worse level, after it
    emplace<Side::Sell>(book, 5, 110);

    REQUIRE(it->order_id == 1);
    REQUIRE((++it)->order_id == 3);
    REQUIRE((++it)->order_id == 4);
    REQUIRE(++it == end<Side::Buy>(book));
}

TEMPLATE_LIST_TEST_CASE("Erasing from begin until end empties only that side", "[book][edge]",
                        Books) {
    TestType book;
    emplace<Side::Buy>(book, 1, 100);
    emplace<Side::Buy>(book, 2, 101);
    emplace<Side::Buy>(book, 3, 100);
    emplace<Side::Sell>(book, 4, 110);

    std::vector<OrderId> erased;
    for (auto it = begin<Side::Buy>(book); it != end<Side::Buy>(book);) {
        erased.push_back(it->order_id);
        it = book.erase(it);
    }

    REQUIRE_THAT(erased, RangeEquals<OrderId>({2, 1, 3}));
    REQUIRE_THAT(ids<Side::Buy>(book), IsEmpty());
    REQUIRE_THAT(ids<Side::Sell>(book), RangeEquals<OrderId>({4}));
}

TEMPLATE_LIST_TEST_CASE("A removed price level can be emplaced into again", "[book][edge]", Books) {
    TestType book;
    emplace<Side::Sell>(book, 1, 110);
    emplace<Side::Sell>(book, 2, 111);
    book.erase(book.find(1));

    emplace<Side::Sell>(book, 3, 110);
    REQUIRE_THAT(ids<Side::Sell>(book), RangeEquals<OrderId>({3, 2}));
}

TEMPLATE_LIST_TEST_CASE("Extreme prices keep their order", "[book][edge]", Books) {
    constexpr auto max = std::numeric_limits<Price>::max();

    TestType book;
    emplace<Side::Buy>(book, 1, 0);
    emplace<Side::Buy>(book, 2, max);
    emplace<Side::Sell>(book, 3, max);
    emplace<Side::Sell>(book, 4, 0);

    REQUIRE_THAT(ids<Side::Buy>(book), RangeEquals<OrderId>({2, 1}));
    REQUIRE_THAT(ids<Side::Sell>(book), RangeEquals<OrderId>({4, 3}));
}

TEMPLATE_LIST_TEST_CASE("A const book can be iterated", "[book][edge]", Books) {
    // the Engine's spread() iterates a const book
    TestType book;
    emplace<Side::Buy>(book, 1, 100);
    emplace<Side::Buy>(book, 2, 99);

    auto const &cbook = book;
    auto const it = begin<Side::Buy>(cbook);
    REQUIRE(it->order_id == 1);
    REQUIRE(std::next(it, 2) == end<Side::Buy>(cbook));
}

TEMPLATE_LIST_TEST_CASE("A const book's levels can be walked", "[book][edge]", Books) {
    // the Engine's volume() looks up levels of a const book
    TestType book;
    emplace<Side::Buy>(book, 1, 100, 3);
    emplace<Side::Buy>(book, 2, 99, 4);

    auto const &cbook = book;
    REQUIRE(find_level<Side::Buy>(cbook, 99)->qty == 4);
    REQUIRE_THAT(level_qtys<Side::Buy>(cbook), RangeEquals<std::uint64_t>({3, 4}));
}

TEST_CASE("RestingOrder formats with its id", "[order]") {
    auto order = make_order<Side::Buy>(42, 100, 7);
    REQUIRE(std::format("{}", order) == std::format("id=42 {}", static_cast<Order const &>(order)));
}

// NOLINTEND(readability-magic-numbers)

} // namespace ome::book::books
