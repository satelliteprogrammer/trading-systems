#pragma once

#include <book/order.hpp>

#include <concepts>
#include <iterator>
#include <type_traits>

namespace ome::book::concepts {

template <typename T>
concept OrderBookIterator =
    std::forward_iterator<T> && std::same_as<std::iter_value_t<T>, RestingOrder> &&
    std::indirectly_writable<T, RestingOrder>;

template <typename T> using Iterator = std::remove_cvref_t<T>::iterator;

template <typename T, Side S>
concept IterableSide = requires(T &book) {
    { book.template begin<S>() } -> std::same_as<Iterator<T>>;
    { book.template end<S>() } -> std::same_as<Iterator<T>>;
};

template <typename T>
concept Iterable = IterableSide<T, Side::Buy> && IterableSide<T, Side::Sell>;

template <typename T>
concept OrderBook = requires(T book, Iterator<T> it, RestingOrder order, OrderId id) {
    requires std::default_initializable<T>;
    requires OrderBookIterator<Iterator<T>>;

    // iterators
    requires Iterable<T>;
    requires Iterable<T const>;

    // modifiers
    { book.template emplace<Side::Buy>(order) } -> std::same_as<Iterator<T>>;
    { book.template emplace<Side::Sell>(order) } -> std::same_as<Iterator<T>>;
    { book.erase(it) } -> std::same_as<Iterator<T>>;

    // lookup
    { book.find(id) } -> std::same_as<Iterator<T>>;
};

} // namespace ome::book::concepts
