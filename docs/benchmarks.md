# Benchmarks

## QuantCup 1

"A quant trading themed programming contest" to develop a "Price-Time Matching Engine".
Retrived from https://web.archive.org/web/20120110103600/http://www.quantcup.org/.

The engines were compiled with GCC 15.2.1, and require -fpermissive.
The test and score were changed to include the engine header instead of the source code directly, to simplify testing different implementations.

All results were run 10+ times on an AMD Ryzen 7 1700 @ 3.20GHz, and the lowest (read best) results were added to the table below.

### Results

| Engine                                        | mean latency (ns) | sd latency (ns) | score   |
| --------------------------------------------- | ----------------- | --------------- | ------- |
| original                                      | 4496.15           | 4614.44         | 4555.29 |
| original (split TU)                           | 4576.89           | 4620.20         | 4598.54 |
| voyager                                       | 150.33            | 294.01          | 222.17  |
| voyager (split TU)                            | 242.49            | 400.88          | 321.69  |
| lob                                           | 1233.51           | 1169.34         | 1201.43 |
| lob (order it)                                | 1074.79           | 1020.15         | 1047.47 |
| lob ([execute inplace](#execute-inplace))     | 708.34            | 703.31          | 705.82  |
| lob (1x unordered_map.find)                   | 701.19            | 708.26          | 704.73  |
| lob ([refactored execute](#execute-refactor)) | 569.41            | 597.25          | 583.33  |

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
