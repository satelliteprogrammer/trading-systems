#include "books.hpp"
#include "replay.hpp"

#include <book/engine.hpp>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <stdexcept>
#include <string_view>

namespace chrono = std::chrono;
namespace fs = std::filesystem;

// Replays a LOBSTER day through each selected book, timing every book operation into
// results-<book>.csv (op-id,nanoseconds per line). Pure measurement: the book state is not
// validated between operations so the timings stay undisturbed (see lobster_validate for
// correctness checks).
auto main(int argc, char *argv[]) -> int {
    using namespace ome::testing;

    if (argc < 3 || argc > 4) {
        std::println(std::cerr, "Usage: {} <messages-file> <orderbook-file> [book]", argv[0]);
        std::println(std::cerr, "  book: one of {} or {} (default)", book_names(), all_books);
        return EXIT_FAILURE;
    }
    std::string_view const selected = argc == 4 ? argv[3] : all_books;

    if (!is_book(selected)) {
        std::println(std::cerr, "Unknown book \"{}\", expected one of {} or {}", selected,
                     book_names(), all_books);
        return EXIT_FAILURE;
    }

    try {
        LobsterReplay replay{fs::path{argv[1]}, fs::path{argv[2]}};

        auto const ok = for_each_book(selected, [&]<typename Book>() -> bool {
            auto const name = ome::book::books::name<Book>;
            auto const out = std::format("results-{}.csv", name);
            std::println("Replaying into {} ...", name);

            replay.rewind();
            ome::book::BasicEngine<Book> book;
            std::ofstream results{out};

            auto timed = [&](std::size_t op, auto const &call) -> auto {
                auto const start = chrono::high_resolution_clock::now();
                auto ans = call();
                auto const stop = chrono::high_resolution_clock::now();

                auto const delta = stop - start;
                std::println(results, "{},{}", op, delta.count());

                return ans;
            };

            std::size_t trade_id = 0;
            while (auto trade = replay.next()) {
                ++trade_id;

                if (auto success = replay.apply(*trade, book, timed); !success) {
                    std::println(std::cerr, "{}: {}: error \"{}\", trade={} failed", name, trade_id,
                                 success.error(), *trade);
                    return false;
                }
            }

            std::println("Replayed all {} messages into {}, timings in {}", trade_id, name, out);
            return true;
        });

        if (!ok) {
            return EXIT_FAILURE;
        }
    } catch (std::runtime_error &e) {
        std::println(std::cerr, "{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
