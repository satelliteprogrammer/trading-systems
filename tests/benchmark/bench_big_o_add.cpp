#include "book/lob.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#ifdef __cpp_lib_generator
#include <generator>
#endif
#include <iostream>
#include <limits>
#include <random>
#include <ratio>
#include <vector>

using namespace std::chrono_literals;
using namespace ome::book;

namespace chrono = std::chrono;
namespace ranges = std::ranges;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::uint64_t next_order_id = 1;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto make_buy(Price price, std::uint64_t qty) -> BuyOrder {
    BuyOrder order;
    order.order_id = next_order_id++;
    order.limit.price = price;
    order.limit.quantity = qty;
    return order;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto make_sell(Price price, std::uint64_t qty) -> SellOrder {
    SellOrder order;
    order.order_id = next_order_id++;
    order.limit.price = price;
    order.limit.quantity = qty;
    return order;
}

template <size_t N> auto fibonacci_sequence() {
    std::array<std::uint64_t, N> fib;

    struct Fib {
        auto operator()() {
            auto next = pre + cur;
            pre = cur;
            cur = next;
            return next;
        }

      private:
        std::uint64_t pre = 0;
        std::uint64_t cur = 1;
    };

    ranges::generate(fib, Fib{});
    return fib;
}

#if __cpp_lib_generator
auto fibonacci_generator(std::uint64_t max) -> std::generator<std::uint64_t> {
    std::uint64_t pre = 0;
    std::uint64_t cur = 1;
    while (cur <= max) {
        auto next = pre + cur;
        pre = cur;
        cur = next;
        co_yield next;
    }
}
#endif

template <typename T = std::uint64_t> auto random_value() {
    static std::mt19937_64 eng(std::random_device{}());
    using limits = std::numeric_limits<T>;
    static std::uniform_int_distribution distr(limits::lowest(), limits::max());
    return distr(eng);
}

#if __cpp_lib_generator
template <typename T = std::uint64_t> auto random_generator() -> std::generator<T> {
    while (true) {
        co_yield random_value<T>();
    }
}
#endif

} // namespace

auto main() -> int {
    std::ofstream csv("results.csv");
    csv << "op,size,ns_per_op" << '\n';

    constexpr auto NumberOfOps = 100'000;

    auto bench = [](auto const &func, auto const &gen) -> auto {
        std::vector<chrono::duration<double, std::nano>> deltas;
        deltas.reserve(NumberOfOps);

        // Book book;
        for (std::size_t i = 0; i < NumberOfOps; ++i) {
            auto args = gen();
            auto start = chrono::high_resolution_clock::now();
            std::invoke(func, args);
            auto stop = chrono::high_resolution_clock::now();
            deltas.emplace_back(stop - start);
        }
        return deltas;
    };

    {
        Book book;
        book.reserve(NumberOfOps);

        auto deltas = bench([&](auto price) -> void { book.add(make_buy(price, 1)); },
                            [price = random_value<Price>()] -> Price { return price; });
        for (std::size_t n_ops = 0; n_ops < deltas.size(); ++n_ops) {
            csv << "add_matching," << n_ops + 1 << "," << deltas[n_ops].count() << '\n';
        }
    }

    {
        Book book;
        book.reserve(NumberOfOps);

        auto deltas =
            bench([&](auto price) -> void { book.add(make_sell(price, 1)); }, random_value<Price>);
        for (std::size_t i = 0; i < deltas.size(); ++i) {
            csv << "add_nonmatching," << i + 1 << "," << deltas[i].count() << '\n';
        }
    }

    csv.close();
    std::cout << "Bench results written to results.csv\n";
    return 0;
}
