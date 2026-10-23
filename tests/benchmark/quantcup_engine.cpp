#include <book/engine.hpp>
#include <book/order.hpp>

extern "C" {
#include <quantcup/engine.h>
#include <quantcup/types.h>
}

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>

using namespace ome::book;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

namespace {

Engine book;

auto execute(t_order const &order, OrderId /*id*/, Order const &order_, Price price,
             std::uint64_t quantity) {
    t_execution exec;
    std::strncpy(exec.symbol, order.symbol, STRINGLEN);
    exec.price = static_cast<t_price>(price);
    exec.size = quantity;

    exec.side = order.side;
    std::strncpy(exec.trader, order.trader, STRINGLEN);
    execution(exec);

    exec.side = (order.side == 1) ? 0 : 1;
    std::strncpy(exec.trader, order_.trader_id.data(), STRINGLEN);
    execution(exec);
}

} // namespace

void init() { book = Engine{MAX_LIVE_ORDERS}; }

void destroy() { book = Engine{}; }

// quantcup requires ids to start at 1; the book's ids start at 0, so shift by one.

auto limit(t_order order) -> t_orderid {
    auto const side = is_ask(order.side) != 0 ? Side::Sell : Side::Buy;
    Order const book_order{{order.size}, side, order.price, {}, std::to_array(order.trader)};
    return *book.add(book_order, std::bind_front(&execute, order)) + 1;
}

void cancel(t_orderid orderid) { [[maybe_unused]] auto const res = book.cancel(orderid - 1); }

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
