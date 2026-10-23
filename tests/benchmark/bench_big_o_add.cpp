#include "replay.hpp"

#include <book/engine.hpp>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <stdexcept>

namespace chrono = std::chrono;
namespace fs = std::filesystem;

// Replays a LOBSTER day through the book, timing every book operation into
// results.csv (op-id,nanoseconds per line). Pure measurement: the book state
// is not validated between operations so the timings stay undisturbed (see
// lobster_validate for correctness checks).
auto main(int argc, char *argv[]) -> int {
    using namespace ome::testing;

    if (argc != 3) {
        std::println(std::cerr, "Usage: {} <messages-file> <orderbook-file>", argv[0]);
        return EXIT_FAILURE;
    }

    try {
        LobsterReplay replay{fs::path{argv[1]}, fs::path{argv[2]}};
        ome::book::Engine book;
        std::ofstream results{"results.csv"};

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
                std::println(std::cerr, "{}: error \"{}\", trade={} failed", trade_id,
                             success.error(), *trade);
                return EXIT_FAILURE;
            }
        }

        std::println("Replayed all {} messages", trade_id);
    } catch (std::runtime_error &e) {
        std::println(std::cerr, "{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
