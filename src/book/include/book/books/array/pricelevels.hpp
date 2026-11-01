#pragma once

#include <book/order.hpp>

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/options.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace ome::book::books::array {

namespace detail {

namespace bi = boost::intrusive;

// NOLINTNEXTLINE(misc-multiple-inheritance)
struct RestingOrder : ome::book::RestingOrder, bi::list_base_hook<> {};

using orders_container_t = std::vector<RestingOrder>;

struct Level {
    bi::list<RestingOrder> orders;
    std::uint64_t qty{};
};

} // namespace detail

inline namespace v1 {

class PriceLevels {
  public:
    explicit PriceLevels(std::size_t max_orders) { orders_.resize(max_orders); }

    template <Side S> constexpr auto get(this auto &self) -> auto & {
        if constexpr (S == Side::Buy) {
            return self.bids_;
        } else if constexpr (S == Side::Sell) {
            return self.asks_;
        } else {
            static_assert(false, "invalid side");
        }
    }

    constexpr auto operator()(this auto & /*self*/, Side /*side*/) -> auto & = delete;

    auto operator[](OrderId id) const -> detail::RestingOrder const & { return orders_[id]; }
    auto operator[](OrderId id) -> detail::RestingOrder & { return orders_[id]; }

  private:
    detail::orders_container_t orders_;

    std::map<Price, detail::Level, std::greater<>> bids_;
    std::map<Price, detail::Level, std::less<>> asks_;
};

} // namespace v1

} // namespace ome::book::books::array
