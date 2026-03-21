# LOBSTER sample reconstructor

Trading opens at 9:30 ET, which marks the beginning of our dataset.
However, messages from the previous day have already been been processed and messages outside the N-levels currently in the snapshot don't appear in the dataset.

## How to initialize the book?

Given that there are messages processed outside the dataset, and not only at the beginning, trades must be added/removed from the orderbook to fulfill every new price level discovered.
Further, there are cancels/executions against these "invisble" messages, so the trades that are created must contain matching IDs/sizes.

## Strategy

### Find all orphan orders

An initial scan of the messages can find those which reference non-existing trades.
These can be used to generate trades whenever a price-level is filled.

It is possible for a level to go outside the N-visible levels and return, so they might reference trades that where added in those time intervals.
Issues:
 - the total volume of orphan orders for a specific level could exceed that of when it enters into view for the 1st time
 - executing orders at the margins might be slower than at the center.
   Adding all orders at the start might erroneosly improve performance benchmarks.

### Pre-fill volume whenever a new level appears in the dataset

Whenever a new level comes into view, generate trades that match the orphan orders plus enough "fake" trades to prefill the total volume of that level.

This applies to all N levels for the very first message/snapshot.

Take into consideration that when a level first comes into view, the corresponding trade might be for that new level.

### When to executed hidden trades?

For the initial N-levels, it is known that the orders that fulfill the necessary trading volume have already executed at a previous time.
However, no such guarantees can be done for any other level.
Additionally, trades at levels exceeding N are not on the dataset.

Start by assuming that whenever a new level comes into view, the orders that fulfilled the total volume were also already there at the start.

Whenever a level exits the visible levels and later reappears with a different volume, it is known that the trades that affected it must have been executed in-between.
Generate those matching later orphan orders + whatever necessary "fake" orders, and sparse them throught the time the level was out of view.

### "Fake" orders

Size generated from a normal distribution of all orders in the system.

## Implementation

1. One pass through all messages (m) to retrieve orphan orders (o) and order size distribution.
   Linear with the number of visible messages (O(m)).
   1. Save into bid/ask maps (O(log(o))).
2. One pass through all snapshots to retrieve the intervals when a price-level is in view and the enter/exit volume (O(m)).
3. For each price level (l), if it only comes once into view, generate trades from orphan order at that level + fakes, up to the total volume.
   Linear over the number of price levels (O(l) * O(o), l >> o)
   1. If it comes into view more than once, use only orphan orders up to the trade where the level exits.
   2. "Sprinkle" the generated messages on the interval from when it exited the previous time until the next reentry.


## API

Construct the parser with the path to the message and orderbook files.

```cpp
Parser parser(fs::path);
```

Return the next trade. Returns `std::nullopt` when it finishes the available trades.

```cpp
std::optional<Trade> parser.next();
```

```cpp
std::expected<void, Error> parser.validate();
```
