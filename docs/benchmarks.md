# Benchmarks

## QuantCup 1

"A quant trading themed programming contest" to develop a "Price-Time Matching Engine".
Retrived from https://web.archive.org/web/20120110103600/http://www.quantcup.org/.

The engines were compiled with GCC 15.2.1, and require -fpermissive.
The test and score were changed to include the engine header instead of the source code directly, to simplify testing different implementations.

All results were run 10+ times on an Intel(R) Core(TM) i5-6200U CPU @ 2.30GHz, and the lowest (read best) results were added to the table below.

### Results

| Engine   | mean latency (ns) | sd latency (ns) | score   |
| -------- | ----------------- | --------------- | ------- |
| original | 5504.25           | 5818.09         | 5661.17 |
| voyager  | 264.68            | 348.10          | 306.39  |
| lob      | 1369.41           | 1659.60         | 1514.51 |
