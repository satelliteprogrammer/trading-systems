#pragma once

#include <lobster/types.hpp>

#include <functional>
#include <unordered_map>

namespace ome::tools::lobster {

class SyntheticsError : public std::runtime_error {
  public:
    explicit SyntheticsError(auto error, auto const &price, auto const &views,
                             auto const &synthetics, auto const &only_referenced,
                             auto const &only_created, auto const &orphans_consumed,
                             std::function_ref<OrderId(MessageLine)> next_order_id);
};

class Synthetics {
  public:
    Synthetics(Orphans const &, Levels const &, std::function_ref<OrderId(MessageLine)>);

    /// Return missing orders (synthetics) for the given orphans/levels.
    ///
    /// Can only be called once.
    auto operator()() -> Messages;

  private:
    struct OrphansInPrice {
        Messages only_referenced;
        Messages only_created;
        Messages partially_deleted;
    };
    auto extract(Price) -> OrphansInPrice;
    auto synthetics_per_price(Price price, PriceViews const &, OrphansInPrice &, Messages &consumed)
        -> void;

    auto referenced_in_view(Messages &, PriceView const &hidden, PriceView const &second,
                            Messages &consumed) -> Messages;
    auto partially_deleted_in_view(Messages &, PriceView const &hidden) -> Messages;

    struct OrderIdView {
        OrderId entry;
        OrderId exit;
    };
    auto order_ids(PriceView const &) -> OrderIdView;
    static auto in(OrderIdView const &, OrderId) -> bool;

    static auto make_order(Message const &, Timestamp) -> Message;
    static auto make_cancel(Message const &, Timestamp, Size) -> Message;

    std::unordered_map<Price, Messages> only_referenced_by_price;
    std::unordered_map<Price, Messages> only_created_by_price;
    std::unordered_map<Price, Messages> partially_deleted_by_price;

    std::function_ref<OrderId(MessageLine)> next_order_id;
    OrderId first_order_id{next_order_id(1)};
    OrderId first_synthetic_id{next_order_id(-1)};

    OrderId synthetic_id{first_synthetic_id};
    Messages synthetics;
};

} // namespace ome::tools::lobster
