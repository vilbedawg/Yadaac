# Yadaac

A high-performance, header-only C++20 implementation of the Aho-Corasick multi-pattern string matching algorithm using the compact double-array trie data structure.

## Features

- **Header-only** — drop into any C++20 project with zero build steps
- **Double-array** — cache-friendly, compact representation with fast traversal
- **Streaming API** — pull-based `consume()` interface; no allocation per match
- **Pipe-composable adapters** — `filter`, `transform`, `take`, `skip`, `non_overlapping`, `left_most_non_overlapping`, `longest_non_overlapping`
- **Binary-safe** — patterns and haystacks can contain any byte value (0x00–0xFF)
- **Up to 16.7 million patterns** (2²⁴ − 1)

## Requirements

- C++20 compiler (GCC 10+, Clang 12+, MSVC 19.29+)
- CMake 3.20+ (for building tests)

## Installation

### Option 1: Copy headers

Copy the `include/yadaac/` directory into your project and add the include path.

### Option 2: CMake subdirectory

```cmake
add_subdirectory(vendor/yadaac)
target_link_libraries(my_target PRIVATE yadaac)
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
target_link_libraries(my_target PRIVATE yadaac)
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

## API Reference

See [docs/API.md](docs/API.md) for the full API reference covering `daac`, `daac_stream`, `match`, and stream utilities.

## Building Tests

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Citation

If you use Yadaac in academic work, please cite the accompanying paper:

```bibtex
@mastersthesis{TODO,
    author  = {Luoma, Vilho},
    title   = {Kaksoistaulukkoisen Aho-Corasick-automaatin optimoinnit},
    year    = {2026},
    month   = {May},
    school  = {Tampereen yliopisto},
    type    = {Master's Thesis}
}
```

## License

[MIT](LICENSE)
