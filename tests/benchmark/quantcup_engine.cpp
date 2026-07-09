#include "book/lob.hpp"

extern "C" {
#include "engine.h"
}

#include <cstring>

using namespace ome::book;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

namespace {

Book book;
t_orderid next_id;

auto execute(t_order const &order, Order const &order_, Price price, std::uint64_t quantity) {
    t_execution exec;
    std::strncpy(exec.symbol, order.symbol, STRINGLEN);
    exec.price = price;
    exec.size = quantity;

    exec.side = order.side;
    std::strncpy(exec.trader, order.trader, STRINGLEN);
    execution(exec);

    exec.side = (order.side == 1) ? 0 : 1;
    std::strncpy(exec.trader, order_.trader_id.data(), STRINGLEN);
    execution(exec);
}

} // namespace

void init() {
    book = Book{};
    book.reserve(MAX_LIVE_ORDERS);
    next_id = 1;
}

void destroy() {
    book = Book{};
    // order_info.clear();
}

auto limit(t_order order) -> t_orderid {
    t_orderid id = next_id++;

    if (is_ask(order.side) != 0) {
        SellOrder sell{id, order.size, {}, std::to_array(order.trader), order.price};
        [[maybe_unused]] auto res = book.add(sell, std::bind_front(&execute, order));
    } else {
        BuyOrder buy{id, order.size, {}, std::to_array(order.trader), order.price};
        [[maybe_unused]] auto res = book.add(buy, std::bind_front(&execute, order));
    }

    return id;
}

void cancel(t_orderid orderid) { [[maybe_unused]] auto res = book.cancel(orderid); }

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
