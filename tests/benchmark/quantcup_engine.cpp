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

auto execute(t_order const &order, OrderId /*id*/, Order const &order_, Price price,
             std::uint64_t quantity) {
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

void init() { book = Book{MAX_LIVE_ORDERS}; }

void destroy() { book = Book{}; }

// quantcup requires ids to start at 1; the book's ids start at 0, so shift by one.

auto limit(t_order order) -> t_orderid {
    if (is_ask(order.side) != 0) {
        SellOrder sell{order.size, {}, std::to_array(order.trader), order.price};
        return *book.add(sell, std::bind_front(&execute, order)) + 1;
    }

    BuyOrder buy{order.size, {}, std::to_array(order.trader), order.price};
    return *book.add(buy, std::bind_front(&execute, order)) + 1;
}

void cancel(t_orderid orderid) { [[maybe_unused]] auto res = book.cancel(orderid - 1); }

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
