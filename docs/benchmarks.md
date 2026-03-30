# Benchmarks

## QuantCup 1

"A quant trading themed programming contest" to develop a "Price-Time Matching Engine".
Retrived from https://web.archive.org/web/20120110103600/http://www.quantcup.org/.

The engines were compiled with GCC 15.2.1, and require -fpermissive.
The test and score were changed to include the engine header instead of the source code directly, to simplify testing different implementations.

All results were run 10+ times on an AMD Ryzen 7 1700 @ 3.20GHz, and the lowest (read best) results were added to the table below.

### Results

| Engine              | mean latency (ns) | sd latency (ns) | score   |
| ------------------- | ----------------- | --------------- | ------- |
| original            | 4496.15           | 4614.44         | 4555.29 |
| original (split TU) | 4576.89           | 4620.20         | 4598.54 |
| voyager             | 150.33            | 294.01          | 222.17  |
| voyager (split TU)  | 242.49            | 400.88          | 321.69  |
| lob                 | 1302.51           | 1167.55         | 1235.03 |
