#include "book/lob.hpp"

// types.h uses STRINGLEN so limits.h must come first.
#include "limits.h"
#include "types.h"

// Declare the quantcup API with C linkage so the C test/score binaries can link against it.
extern "C" {
void init();
void destroy();
t_orderid limit(t_order order);
void cancel(t_orderid orderid);
void execution(t_execution exec); // provided by test.c / score.c
}

#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>

using namespace ome::book;

namespace {

struct Meta {
    char symbol[STRINGLEN];
    char trader[STRINGLEN];
    t_side side;
    t_price price;
    t_size qty;
    std::list<t_orderid>::iterator it; // stable iterator for O(1) cancel
};

using BidQueue = std::map<t_price, std::list<t_orderid>, std::greater<t_price>>;
using AskQueue = std::map<t_price, std::list<t_orderid>, std::less<t_price>>;

Book book;
t_orderid next_id;
std::unordered_map<t_orderid, Meta> meta;
BidQueue bid_queue;
AskQueue ask_queue;

void fire(const Meta &m, t_price price, t_size qty) {
    t_execution exec{};
    std::memcpy(exec.symbol, m.symbol, STRINGLEN);
    std::memcpy(exec.trader, m.trader, STRINGLEN);
    exec.side = m.side;
    exec.price = price;
    exec.size  = qty;
    execution(exec);
}

void fire(const t_order &o, t_price price, t_size qty) {
    t_execution exec{};
    std::memcpy(exec.symbol, o.symbol, STRINGLEN);
    std::memcpy(exec.trader, o.trader, STRINGLEN);
    exec.side  = o.side;
    exec.price = price;
    exec.size  = qty;
    execution(exec);
}

} // namespace

void init() {
    book = Book{};
    next_id = 1;
    meta.clear();
    bid_queue.clear();
    ask_queue.clear();
}

void destroy() {
    book = Book{};
    meta.clear();
    bid_queue.clear();
    ask_queue.clear();
}

t_orderid limit(t_order order) {
    t_orderid id = next_id++;
    t_size remaining = order.size;

    if (order.side == 1) { // ASK: match against bids (best bid = highest price first)
        Price best_bid = book.spread().value().first;
        while (remaining > 0 && best_bid >= static_cast<Price>(order.price)) {
            auto &front_list = bid_queue.begin()->second;
            t_orderid maker_id = front_list.front();
            Meta &maker = meta.at(maker_id);

            t_size qty = remaining < maker.qty ? remaining : maker.qty;

            fire(maker, maker.price, qty);
            fire(order, maker.price, qty);

            maker.qty -= qty;
            remaining -= qty;

            if (maker.qty == 0) {
                front_list.pop_front();
                if (front_list.empty())
                    bid_queue.erase(bid_queue.begin());
                book.cancel(maker_id);
                meta.erase(maker_id);
            } else {
                book.cancel(maker_id, qty);
            }

            best_bid = book.spread().value().first;
        }
    } else { // BID: match against asks (best ask = lowest price first)
        Price best_ask = book.spread().value().second;
        while (remaining > 0 && best_ask > 0 && best_ask <= static_cast<Price>(order.price)) {
            auto &front_list = ask_queue.begin()->second;
            t_orderid maker_id = front_list.front();
            Meta &maker = meta.at(maker_id);

            t_size qty = remaining < maker.qty ? remaining : maker.qty;

            fire(maker, maker.price, qty);
            fire(order, maker.price, qty);

            maker.qty -= qty;
            remaining -= qty;

            if (maker.qty == 0) {
                front_list.pop_front();
                if (front_list.empty())
                    ask_queue.erase(ask_queue.begin());
                book.cancel(maker_id);
                meta.erase(maker_id);
            } else {
                book.cancel(maker_id, qty);
            }

            best_ask = book.spread().value().second;
        }
    }

    if (remaining > 0) {
        Meta m{};
        std::memcpy(m.symbol, order.symbol, STRINGLEN);
        std::memcpy(m.trader, order.trader, STRINGLEN);
        m.side  = order.side;
        m.price = order.price;
        m.qty   = remaining;

        if (order.side == 1) { // ASK
            auto &lst = ask_queue[order.price];
            lst.push_back(id);
            m.it = std::prev(lst.end());
            book.add(SellOrder{{id, {static_cast<Price>(order.price), remaining}, {}}});
        } else { // BID
            auto &lst = bid_queue[order.price];
            lst.push_back(id);
            m.it = std::prev(lst.end());
            book.add(BuyOrder{{id, {static_cast<Price>(order.price), remaining}, {}}});
        }
        meta[id] = m;
    }

    return id;
}

void cancel(t_orderid orderid) {
    auto it = meta.find(orderid);
    if (it == meta.end())
        return; // not in book, ignore per spec

    Meta &m = it->second;
    if (m.side == 1) { // ASK
        auto qit = ask_queue.find(m.price);
        if (qit != ask_queue.end()) {
            qit->second.erase(m.it);
            if (qit->second.empty())
                ask_queue.erase(qit);
        }
    } else { // BID
        auto qit = bid_queue.find(m.price);
        if (qit != bid_queue.end()) {
            qit->second.erase(m.it);
            if (qit->second.empty())
                bid_queue.erase(qit);
        }
    }
    book.cancel(orderid);
    meta.erase(it);
}
