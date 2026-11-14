#include "books.hpp"
#include "replay.hpp"

#include <book/engine.hpp>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <print>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

// Replays a LOBSTER day through the selected books and checks the book state against the recorded
// orderbook snapshot after every original message. Pure pass/fail: no timing instrumentation (see
// bench_big_o_add for that).
auto main(int argc, char *argv[]) -> int {
    using namespace ome::testing;

    if (argc < 3 || argc > 4) {
        std::println(std::cerr, "Usage: {} <messages-file> <orderbook-file> [book]", argv[0]);
        std::println(std::cerr, "  book: one of {} or {}; defaults to {}, the production book",
                     book_names(), all_books, production_book);
        return EXIT_FAILURE;
    }
    std::string_view const selected = argc == 4 ? argv[3] : production_book;

    if (!is_book(selected)) {
        std::println(std::cerr, "Unknown book \"{}\", expected one of {} or {}", selected,
                     book_names(), all_books);
        return EXIT_FAILURE;
    }

    try {
        LobsterReplay replay{fs::path{argv[1]}, fs::path{argv[2]}};

        auto const ok = for_each_book(selected, [&]<typename Book>() -> bool {
            auto const name = ome::book::books::name<Book>;
            std::println("Validating {} ...", name);

            replay.rewind();
            ome::book::BasicEngine<Book> book;

            auto uninstrumented = [](std::size_t /*op*/, auto const &call) -> auto {
                return call();
            };

            std::size_t trade_id = 0;
            while (auto trade = replay.next()) {
                ++trade_id;

                if (auto success = replay.apply(*trade, book, uninstrumented); !success) {
                    std::println(std::cerr, "{}: {}: error \"{}\", trade={} failed", name, trade_id,
                                 success.error(), *trade);
                    return false;
                }

                auto validation = replay.validate(book);
                if (!validation.valid) {
                    std::println(std::cerr, "{}: validation failed at message {}: {}\n{}", name,
                                 trade_id, *trade, validation);
                    return false;
                }
            }

            std::println("All {} messages validated successfully for {}", trade_id, name);
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
