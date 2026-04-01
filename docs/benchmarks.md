# Benchmarks

## QuantCup 1

"A quant trading themed programming contest" to develop a "Price-Time Matching Engine".
Retrived from https://web.archive.org/web/20120110103600/http://www.quantcup.org/.

The engines were compiled with GCC 15.2.1, and require -fpermissive.
The test and score were changed to include the engine header instead of the source code directly, to simplify testing different implementations.

All implementations were run multiple times on an AMD Ryzen 7 1700 @ 3.20GHz with core pinning, and the best results were added to the table below.

### Results

| Engine                                        | mean latency (ns) | sd latency (ns) | score   |
| --------------------------------------------- | ----------------- | --------------- | ------- |
| original                                      | 4489.10           | 4557.60         | 4523.35 |
| original (split TU)                           | 4557.60           | 4561.30         | 4562.64 |
| voyager                                       | 150.15            | 301.78          | 225.97  |
| voyager (split TU)                            | 240.56            | 374.68          | 307.62  |
| lob                                           | 1258.71           | 1121.97         | 1190.34 |
| lob ([iterator to list](#iterator-to-list))   | 1070.75           | 962.89          | 1016.82 |
| lob ([execute inplace](#execute-inplace))     | 704.82            | 692.53          | 698.67  |
| lob (1x unordered_map.find)                   | 697.86            | 697.05          | 697.46  |
| lob ([refactored execute](#execute-refactor)) | 568.85            | 617.14          | 592.99  |

#### Iterator to list

TODO

#### Execute inplace

Around 50% (rough estimate) of the time spent on `add(SellOrder)` and `add(BuyOrder)` was around the creation and update of the vector of filled orders.
To improve it, if a caller wants to be notified of filled orders, it must pass a callback function which is called when an order is executed.

![LOB pre execute inplace](benchmarks-lob-pre-exec-inplace.png)
![LOB post execute inplace](benchmarks-lob-post-exec-inplace.png)

The previous flamegraphs show the pre and post call graphs and were generated from a GCC debug build with:

```
sudo perf record -ag -F 99 build/tests/benchmark/quantcup_score
sudo perf script report flamegraph
```

#### Execute refactor

Correcting an earlier missunderstanding that `execute` had to be publicly available (it doesn't) allows for deep refactoring of the method.

The largest improvement is seen in the reduction of copies of Order.

![LOB pre execute refactor](benchmarks-lob-pre-exec-refactor.png)
![LOB post execute refact](benchmarks-lob-post-exec-refactor.png)
