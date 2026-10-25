#pragma once

#include <book/order.hpp>

#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <type_traits>

namespace ome::book::concepts {

template <typename T>
concept OrderBookIterator =
    std::forward_iterator<T> && std::same_as<std::iter_value_t<T>, RestingOrder> &&
    std::indirectly_writable<T, RestingOrder>;

template <typename T>
concept OrderBookLevelIterator =
    std::forward_iterator<T> && requires(std::iter_reference_t<T> level) {
        { level.qty } -> std::same_as<std::uint64_t &>;
    };

template <typename T> using Iterator = std::remove_cvref_t<T>::iterator;
template <typename T> using LevelIterator = std::remove_cvref_t<T>::level_iterator;

template <typename T, Side S>
concept IterableSide = requires(T &book) {
    { book.template begin<S>() } -> std::same_as<Iterator<T>>;
    { book.template end<S>() } -> std::same_as<Iterator<T>>;
    { book.template levels<S>() } -> std::ranges::forward_range;
    requires std::same_as<std::ranges::iterator_t<decltype(book.template levels<S>())>,
                          LevelIterator<T>>;
};

template <typename T>
concept Iterable = IterableSide<T, Side::Buy> && IterableSide<T, Side::Sell>;

template <typename T>
concept OrderBook =
    requires(T book, T const cbook, Iterator<T> it, RestingOrder order, OrderId id, Price price) {
        requires std::default_initializable<T>;
        requires OrderBookIterator<Iterator<T>>;
        requires OrderBookLevelIterator<LevelIterator<T>>;
        requires std::constructible_from<LevelIterator<T>, Iterator<T>>;

        // iterators
        requires Iterable<T>;
        requires Iterable<T const>;

        // modifiers
        { book.template emplace<Side::Buy>(order) } -> std::same_as<Iterator<T>>;
        { book.template emplace<Side::Sell>(order) } -> std::same_as<Iterator<T>>;
        { book.erase(it) } -> std::same_as<Iterator<T>>;

        // lookup
        { cbook.find(id) } -> std::same_as<Iterator<T>>;
        { cbook.template find<Side::Buy>(price) } -> std::same_as<LevelIterator<T>>;
        { cbook.template find<Side::Sell>(price) } -> std::same_as<LevelIterator<T>>;
    };

} // namespace ome::book::concepts
