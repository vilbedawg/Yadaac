# API Reference

## `yadaac::daac`

Construct the automaton from a vector of patterns. Throws `std::runtime_error` on empty input, duplicate patterns, or exceeding the 16.7M pattern limit.

```cpp
yadaac::daac automaton(patterns);

auto stream = automaton.make_stream("haystack text");

uint32_t states      = automaton.num_of_states();
uint32_t transitions = automaton.num_of_transitions();
```

## `yadaac::daac_stream`

Pull-based stream returned by `make_stream()`. Conforms to the stream protocol (`consume(value_type&) → bool`).

```cpp
yadaac::match m{};
while (stream.consume(m)) {
    // m.start  — byte offset of match start
    // m.end    — byte offset past match end
    // m.value  — index of the pattern in the original vector
}
```

## `yadaac::match`

```cpp
struct match {
    size_t   start;  // inclusive start position
    size_t   end;    // exclusive end position
    uint32_t value;  // pattern index (insertion order)
};
```

## Stream Utilities (`yadaac/stream_utils.hpp`)

All adapters compose with the `|` operator and return new streams.

```cpp
namespace su = yadaac::stream;

// Filter matches by predicate
auto s = stream | su::filter([](const yadaac::match& m) { return m.value == 0; });

// Transform matches to a different type
auto s = stream | su::transform([](const yadaac::match& m) { return m.end - m.start; });

// Limit number of results
auto s = stream | su::take(5);

// Skip first N results
auto s = stream | su::skip(3);

// Remove overlapping matches (greedy left-to-right)
auto s = stream | su::non_overlapping();

// Keep leftmost, longest match per overlapping group
auto s = stream | su::left_most_non_overlapping();

// Keep longest match per overlapping group
auto s = stream | su::longest_non_overlapping();

// Chain adapters
auto s = stream | su::skip(2) | su::filter(pred) | su::take(10);
```
