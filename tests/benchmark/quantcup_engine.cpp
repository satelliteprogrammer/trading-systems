#include "book/lob.hpp"

extern "C" {
#include "engine.h"
}

#include <cstring>
#include <string>
#include <unordered_map>

using namespace ome::book;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

namespace {

Book book;
t_orderid next_id;
struct Ids {
    std::string symbol;
    std::string trader;
};
std::unordered_map<t_orderid, Ids> order_info;

} // namespace

void init() {
    book = Book{};
    book.reserve(MAX_LIVE_ORDERS);
    next_id = 1;
    order_info.clear();
}

void destroy() {
    book = Book{};
    order_info.clear();
}

auto limit(t_order order) -> t_orderid {
    t_orderid id = next_id++;
    t_size remaining = order.size;
    t_side maker_side = (order.side == 1) ? 0 : 1;

    order_info[id] = {.symbol = order.symbol, .trader = order.trader};

    OrdersFilled fills;
    if (is_ask(order.side) != 0) {
        fills = *book.add(SellOrder{{id, order.price, order.size}});
    } else {
        fills = *book.add(BuyOrder{{id, order.price, order.size}});
    }

    for (auto const &order_ : fills) {
        auto const &[symbol, trader] = order_info[order_.order_id];

        t_execution exec;
        std::strncpy(exec.symbol, symbol.data(), STRINGLEN);
        exec.price = order_.price;
        exec.size = order_.quantity;

        exec.side = order.side;
        std::strncpy(exec.trader, order.trader, STRINGLEN);
        execution(exec);

        exec.side = maker_side;
        std::strncpy(exec.trader, trader.data(), STRINGLEN);
        execution(exec);
    }

    return id;
}

void cancel(t_orderid orderid) {
    book.cancel(orderid); // NOLINT(bugprone-unused-return-value)
}

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
