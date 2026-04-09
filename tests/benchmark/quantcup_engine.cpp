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
    t_size remaining = order.size;

    auto execute = [&order](Order const &order_, std::uint64_t quantity) -> void {
        t_execution exec;
        // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
        std::strncpy(exec.symbol, order.symbol, STRINGLEN);
        exec.price = order_.price;
        exec.size = quantity;

        exec.side = order.side;
        std::strncpy(exec.trader, order.trader, STRINGLEN);
        execution(exec);

        exec.side = (order.side == 1) ? 0 : 1;
        std::strncpy(exec.trader, order_.trader_id.data(), STRINGLEN);
        execution(exec);
    };

    if (is_ask(order.side) != 0) {
        SellOrder sell{id, order.price, order.size, {}, std::to_array(order.trader)};
        [[maybe_unused]] auto res = book.add(sell, execute);
    } else {
        BuyOrder buy{id, order.price, order.size, {}, std::to_array(order.trader)};
        [[maybe_unused]] auto res = book.add(buy, execute);
    }

    return id;
}

void cancel(t_orderid orderid) {
    book.cancel(orderid); // NOLINT(bugprone-unused-return-value)
}

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
