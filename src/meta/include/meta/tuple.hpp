#pragma once

#include <concepts>
#include <tuple>

namespace ome::meta {

/// Whether T is one of the types of the std::tuple Tuple.
template <typename T, typename Tuple> inline constexpr bool tuple_contains = false;
template <typename T, typename... Ts>
inline constexpr bool tuple_contains<T, std::tuple<Ts...>> = (std::same_as<T, Ts> || ...);

} // namespace ome::meta
