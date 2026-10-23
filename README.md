# Order Matching Engine

[![Build and Test](https://github.com/satelliteprogrammer/trading-systems/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/satelliteprogrammer/trading-systems/actions/workflows/build-and-test.yml)

Find the benchmarks [here](docs/benchmarks.md).

## Dependencies

 - Compiler supporting C++26 (std::views::concat to be specific, or range-v3 as a fallback)
 - CMake >= 3.21
 - Catch2 >= 3.8, installable with [Conan](https://conan.io):

```sh
conan profile detect  # once, if you have no default profile yet
conan install . -pr:a conan/profiles/cpp26 --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
```

### Documentation flamegraphs

 - perf
