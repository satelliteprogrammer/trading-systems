#include "replay.hpp"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <print>
#include <stdexcept>

namespace fs = std::filesystem;

// Replays a LOBSTER day through the book and checks the book state against
// the recorded orderbook snapshot after every original message. Pure
// pass/fail: no timing instrumentation (see bench_big_o_add for that).
auto main(int argc, char *argv[]) -> int {
    using namespace ome::testing;

    if (argc != 3) {
        std::println(std::cerr, "Usage: {} <messages-file> <orderbook-file>", argv[0]);
        return EXIT_FAILURE;
    }

    try {
        LobsterReplay replay{fs::path{argv[1]}, fs::path{argv[2]}};
        ome::book::Book book;

        auto uninstrumented = [](std::size_t /*op*/, auto const &call) -> auto { return call(); };

        std::size_t trade_id = 0;
        while (auto trade = replay.next()) {
            ++trade_id;

            if (auto success = replay.apply(*trade, book, uninstrumented); !success) {
                std::println(std::cerr, "{}: error \"{}\", trade={} failed", trade_id,
                             success.error(), *trade);
                return EXIT_FAILURE;
            }

            auto validation = replay.validate(book);
            if (!validation.valid) {
                std::println(std::cerr, "Validation failed at message {}: {}\n{}", trade_id,
                             *trade, validation);
                return EXIT_FAILURE;
            }
        }

        std::println("All {} messages validated successfully", trade_id);
    } catch (std::runtime_error &e) {
        std::println(std::cerr, "{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
