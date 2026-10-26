#pragma once

#include <book/order.hpp>

#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <type_traits>

namespace ome::book::concepts {

template <typename T> using iterator_t = std::remove_cvref_t<T>::iterator;
template <typename T> using level_iterator_t = std::remove_cvref_t<T>::level_iterator;

template <typename T>
concept OrderBookIterator =
    std::forward_iterator<T> && std::same_as<std::iter_value_t<T>, RestingOrder> &&
    std::indirectly_writable<T, RestingOrder>;

template <typename T>
concept OrderBookLevelIterator =
    std::forward_iterator<T> && requires(std::iter_reference_t<T> level) {
        { level.qty } -> std::same_as<std::uint64_t &>;
    };

template <typename R, typename I>
concept ForwardRangeOf =
    std::ranges::forward_range<R> && std::same_as<std::ranges::iterator_t<R>, I>;

template <typename T, Side S>
concept IterableSide = requires(T &book) {
    { book.template begin<S>() } -> std::same_as<iterator_t<T>>;
    { book.template end<S>() } -> std::same_as<iterator_t<T>>;
    { book.template levels<S>() } -> ForwardRangeOf<level_iterator_t<T>>;
};

template <typename T, Side S>
concept OrderBookSide = IterableSide<T, S> && IterableSide<T const, S> &&
                        requires(T &book, T const &cbook, RestingOrder order, Price price) {
                            { book.template emplace<S>(order) } -> std::same_as<iterator_t<T>>;
                            { cbook.template find<S>(price) } -> std::same_as<level_iterator_t<T>>;
                        };

template <typename T>
concept OrderBook = std::default_initializable<T> && OrderBookIterator<iterator_t<T>> &&
                    OrderBookLevelIterator<level_iterator_t<T>> &&
                    std::constructible_from<level_iterator_t<T>, iterator_t<T>> &&
                    !std::convertible_to<iterator_t<T>, level_iterator_t<T>> &&
                    OrderBookSide<T, Side::Buy> && OrderBookSide<T, Side::Sell> &&
                    requires(T &book, T const &cbook, iterator_t<T> it, OrderId id) {
                        { book.erase(it) } -> std::same_as<iterator_t<T>>;
                        { cbook.find(id) } -> std::same_as<iterator_t<T>>;
                    };

} // namespace ome::book::concepts
