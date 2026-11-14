#include "meta/tuple.hpp"

#include <catch2/catch_test_macros.hpp>

#include <tuple>

namespace ome::meta {

TEST_CASE("tuple_contains finds exactly the tuple's types", "[meta]") {
    STATIC_REQUIRE(tuple_contains<int, std::tuple<char, int>>);
    STATIC_REQUIRE_FALSE(tuple_contains<long, std::tuple<char, int>>);
    STATIC_REQUIRE_FALSE(tuple_contains<int const, std::tuple<char, int>>);
    STATIC_REQUIRE_FALSE(tuple_contains<int, std::tuple<>>);
    STATIC_REQUIRE_FALSE(tuple_contains<int, int>);
}

} // namespace ome::meta
