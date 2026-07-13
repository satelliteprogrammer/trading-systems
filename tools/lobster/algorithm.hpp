#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ome::tools::lobster::algorithm {

template <typename C>
concept HasValuesView = requires(C container) { container | std::views::values; };

/// Converts a map to a range of its values.
template <template <typename...> typename R, typename Map>
    requires HasValuesView<Map>
[[nodiscard]] auto map_to(Map &&map);

/// Groups the elements of a range into vectors indexed by the key given by the projection..
template <std::ranges::input_range R, std::invocable<std::ranges::range_reference_t<R>> Proj>
[[nodiscard]] auto vector_to_map(R &&range, Proj proj);

/// Welford's online algorithm for computing mean and variance
///
/// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
class Welford {
  public:
    void update(double value);

    [[nodiscard]] auto mean() const -> double;
    [[nodiscard]] auto stddev() const -> double;

  private:
    double mean_ = 0.0;
    std::size_t count_ = 0;
    double m2_ = 0.0;
};

} // namespace ome::tools::lobster::algorithm

// === Implementation ===

template <template <typename...> typename R, typename Map>
    requires ome::tools::lobster::algorithm::HasValuesView<Map>
auto ome::tools::lobster::algorithm::map_to(Map &&map) {
    if constexpr (std::is_lvalue_reference_v<Map>) {
        return map | std::views::values | std::ranges::to<R>();
    } else {
        return std::forward<Map>(map) | std::views::values | std::views::as_rvalue |
               std::ranges::to<R>();
    }
}

template <std::ranges::input_range R, std::invocable<std::ranges::range_reference_t<R>> Proj>
[[nodiscard]] auto ome::tools::lobster::algorithm::vector_to_map(R &&range, Proj proj) {
    using Value = std::ranges::range_value_t<R>;
    using Key = std::remove_cvref_t<std::invoke_result_t<Proj, std::ranges::range_reference_t<R>>>;

    std::unordered_map<Key, std::vector<Value>> map;
    for (auto &&value : std::forward<R>(range)) {
        map[std::invoke(proj, value)].emplace_back(std::forward_like<R>(value));
    }
    return map;
}
