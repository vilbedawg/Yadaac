# Yadaac - Yet another double-array Aho-Corasick

A high-performance, header-only C++20 implementation of the Aho-Corasick multi-pattern string matching algorithm using the compact double-array data structure.

## Requirements

- C++20 compiler (GCC 10+, Clang 12+).
- CMake 3.20+ (for building tests)

## Installation

### Option 1: Copy headers

Copy the `include/yadaac/` directory into your project and add the include path.

### Option 2: CMake subdirectory

```cmake
add_subdirectory(vendor/yadaac)
target_link_libraries(my_target PRIVATE yadaac::yadaac)
```

### Option 3: CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    yadaac
    GIT_REPOSITORY https://github.com/vilbedawg/yadaac.git
    GIT_TAG main
)
FetchContent_MakeAvailable(yadaac)
target_link_libraries(my_target PRIVATE yadaac::yadaac)
```

### Option 4: System install + find_package

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --install build
```

```cmake
find_package(yadaac REQUIRED)
target_link_libraries(my_target PRIVATE yadaac::yadaac)
```

## Quick Start

```cpp
#include <iostream>
#include <vector>
#include <string>

#include "yadaac/yadaac.hpp"
#include "yadaac/stream_utils.hpp"

int main() {
    std::vector<std::string> patterns = {"he", "she", "his", "hers"};
    yadaac::daac automaton(patterns);

    // Stream all matches
    auto stream = automaton.make_stream("ushers");
    yadaac::match m{};
    while (stream.consume(m)) {
        std::cout << "Pattern " << m.value
                  << " matched at [" << m.start << ", " << m.end << ")\n";
    }
    // Output:
    //   Pattern 1 matched at [1, 4)    → "she"
    //   Pattern 0 matched at [2, 4)    → "he"
    //   Pattern 3 matched at [2, 6)    → "hers"

    // With stream adapters
    namespace su = yadaac::stream;
    auto longest = automaton.make_stream("ushers") | su::longest_non_overlapping();
    while (longest.consume(m)) {
        std::cout << "Longest: pattern " << m.value
                  << " at [" << m.start << ", " << m.end << ")\n";
    }
}
```

## Design and performance

Yadaac implements three optimizations on top of the standard double-array Aho-Corasick construction:

- **Direct construction**: Patterns are inserted directly into the double array without building an intermediate trie, which reduces peak memory and eliminates the two-phase construction bottleneck. Experiemental results show up to 5.8x faster construction times, and 3x times less memory usage compared to trie-based construction.
- **Bitwise vacant-slot search**: During construction, 64 candidate positions are tested in parallel using bitmask operations, giving a 2.5–3.5x speedup in vacant-slot search over a scalar scan.
- **Transition prefilter**: Each automaton state carries a lightweight Bloom-type filter that short-circuits transition lookups for characters with no valid outgoing transition, reducing unnecessary cache misses during matching. Experiemental results show promising results especially for longer patterns.

Benchmarks against a popular Rust crate [daachorse](https://github.com/daac-tools/daachorse) using three natural-language datasets from the [accompanying master's thesis](#Citation):

| Metric | Result |
|---|---|
| Construction time | 3.7–4.4× faster |
| Matching — word-level datasets | Comparable; typically within a few percent |
| Matching — phrase-level dataset | Up to 10–11% faster (K = 10³–10⁵) |
| Peak memory during construction | 50–63% lower |

## API Reference

See [docs/API.md](docs/API.md) for the full API reference.

## Building Tests

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Citation

If you use Yadaac in academic work, please cite the accompanying [paper](https://trepo.tuni.fi/handle/10024/237151):

```bibtex
@mastersthesis{Luoma2026,
    author  = {Luoma, Vilho},
    title   = {Kaksoistaulukkoisen {Aho-Corasick}-automaatin optimoinnit},
    year    = {2026},
    month   = {May},
    school  = {Tampereen yliopisto},
    type    = {Master's Thesis}
}
```

## License

[MIT](LICENSE)
