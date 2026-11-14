#pragma once

#include <book/books/all.hpp>
#include <book/engine.hpp>

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace ome::testing {

inline constexpr std::string_view all_books = "all";

inline constexpr std::string_view production_book = book::books::name<book::Engine::book_type>;

/// Comma-separated names of every book.
inline auto book_names() -> std::string {
    return []<typename... Books>(std::type_identity<std::tuple<Books...>>) {
        std::string names;
        ((names += names.empty() ? "" : ", ", names += book::books::name<Books>), ...);
        return names;
    }(std::type_identity<book::books::All>{});
}

inline auto is_book(std::string_view selected) -> bool {
    return selected == all_books ||
           []<typename... Books>(std::type_identity<std::tuple<Books...>>, std::string_view name) {
               return ((name == book::books::name<Books>) || ...);
           }(std::type_identity<book::books::All>{}, selected);
}

/// Calls run.template operator()<Book>() for each book in book::books::All named selected (or for
/// every book when selected is all_books), in order, stopping at the first run returning false.
///
/// \return whether every run succeeded.
template <typename Run> auto for_each_book(std::string_view selected, Run &&run) -> bool {
    return [&]<typename... Books>(std::type_identity<std::tuple<Books...>>) -> bool {
        auto const visit = [&]<typename Book>() -> bool {
            return (selected != all_books && selected != book::books::name<Book>) ||
                   run.template operator()<Book>();
        };
        return (visit.template operator()<Books>() && ...);
    }(std::type_identity<book::books::All>{});
}

} // namespace ome::testing
