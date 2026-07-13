#include "algorithm.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>
#include <catch2/matchers/catch_matchers_quantifiers.hpp>

#include <functional>
#include <string_view>

using namespace std::string_literals;
using namespace Catch;
using namespace Catch::Matchers;

namespace ome::tools::lobster {

namespace {

class Spy {
  public:
    Spy() : ctor{"default"} {}
    Spy(Spy & /*obj*/) : ctor{"copy ctor"} {}
    auto operator=(Spy const & /*obj*/) -> Spy & {
        ctor = "copy assignment";
        return *this;
    }
    Spy(Spy && /*obj*/) noexcept : ctor{"move ctor"} {}
    auto operator=(Spy && /*obj*/) noexcept -> Spy & {
        ctor = "move assignment";
        return *this;
    }
    ~Spy() = default;

    // [[nodiscard]] auto ctor_type() const -> std::string_view { return ctor; }
    [[nodiscard]] auto is_default_ctor() const -> bool { return ctor == "default"; }
    [[nodiscard]] auto is_copy_ctor() const -> bool { return ctor == "copy ctor"; }
    [[nodiscard]] auto is_copy_assign() const -> bool { return ctor == "copy assignment"; }
    [[nodiscard]] auto is_move_ctor() const -> bool { return ctor == "move ctor"; }
    [[nodiscard]] auto is_move_assign() const -> bool { return ctor == "move assignment"; }

  private:
    std::string_view ctor;
};

} // namespace

TEST_CASE("Welford's algorithm computes mean and variance", "[algorithm]") {
    algorithm::Welford welford;
    std::vector<double> values{1.0, 2.0, 3.0, 4.0, 5.0};
    for (auto const &value : values) {
        welford.update(value);
    }
    CHECK(welford.mean() == Catch::Approx(3.0));
    CHECK(welford.stddev() == Catch::Approx(1.41421356237309515));
}

TEST_CASE("SpyObject", "[algorithm]") {
    Spy obj;
    CHECK(obj.is_default_ctor());
}

TEST_CASE("vector_to_map", "[algorithm]") {
    std::vector<Spy> objects(5);
    CHECK_THAT(objects, AllMatch(Predicate<Spy>(std::mem_fn(&Spy::is_default_ctor))));

    std::size_t idx{0};
    auto proj = [&idx](Spy const & /*obj*/) -> std::size_t { return idx++; };

    SECTION("lvalue reference") {
        auto map = algorithm::vector_to_map(objects, proj);

        REQUIRE(map.size() == 5);
        for (auto &&values : map | std::views::values) {
            REQUIRE(values.size() == 1);
            CHECK_THAT(values, AllMatch(Predicate<Spy>(std::mem_fn(&Spy::is_copy_ctor))));
        }
    }

    SECTION("rvalue reference") {
        auto map = algorithm::vector_to_map(std::move(objects), proj);

        REQUIRE(map.size() == 5);
        for (auto &&values : map | std::views::values) {
            REQUIRE(values.size() == 1);
            CHECK_THAT(values, AllMatch(Predicate<Spy>(std::mem_fn(&Spy::is_move_ctor))));
        }
    }
}

TEST_CASE("map_to_vector", "[algorithm]") {
    std::unordered_map<std::size_t, Spy> map;
    for (std::size_t i = 0; i < 5; ++i) {
        map.emplace(i, Spy{});
    }

    SECTION("lvalue reference") {
        auto vec = algorithm::map_to<std::vector>(map);

        REQUIRE(vec.size() == 5);
        CHECK_THAT(vec, AllMatch(Predicate<Spy>(std::mem_fn(&Spy::is_copy_ctor))));
    }

    SECTION("rvalue reference") {
        auto vec = algorithm::map_to<std::vector>(std::move(map));

        REQUIRE(vec.size() == 5);
        CHECK_THAT(vec, AllMatch(Predicate<Spy>(std::mem_fn(&Spy::is_move_ctor))));
    }
}

} // namespace ome::tools::lobster
