#pragma once

#include <book/order.hpp>

#include <array>
#include <functional>
#include <list>
#include <map>

namespace ome::book::books::node {

struct Level {
    std::list<RestingOrder> orders;
};

class PriceCompare {
  public:
    constexpr explicit PriceCompare(Side side) : side_(side) {}

    constexpr auto operator()(Price lhs, Price rhs) const -> bool {
        switch (side_) {
        case Side::Buy:
            return lhs > rhs;
        case Side::Sell:
            return lhs < rhs;
        }
    }

  private:
    // TODO: benchmark w/ deleted ctors and const side_
    Side side_;
};

inline namespace v1 {

class PriceLevels {
  public:
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

  private:
    std::map<Price, Level, std::greater<>> bids_;
    std::map<Price, Level, std::less<>> asks_;
};

} // namespace v1

namespace v2 {

// TODO: benchmark array based side selection
class PriceLevels {
  public:
    template <Side S> constexpr auto get(this auto &self) -> auto & {
        if constexpr (S == Side::Buy) {
            return self.maps_[0];
        } else if constexpr (S == Side::Sell) {
            return self.maps_[1];
        } else {
            static_assert(false, "invalid side");
        }
    }

    constexpr auto operator()(this auto &self, Side side) -> auto & { return self.maps_[side]; }

  private:
    using map_t = std::map<Price, Level, PriceCompare>;
    std::array<map_t, 2> maps_{map_t{PriceCompare{Side::Buy}}, map_t{PriceCompare{Side::Sell}}};
};

} // namespace v2

} // namespace ome::book::books::node
