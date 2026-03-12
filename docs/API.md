# API Reference

## Stream protocol

Yadaac uses a pull-based stream model. A stream is any object that satisfies:

```cpp
bool consume(value_type& out);
```

Each call to `consume` attempts to produce the next value. It writes the value into `out` and returns `true`, or returns `false` when the stream is exhausted. This makes streams composable via the `|` operator: adapters wrap an upstream stream and apply a transformation, producing a new stream of the same or a different type.

---

## `yadaac::daac`

The automaton. Construct it from a vector of patterns, then create streams over input text.

```cpp
yadaac::daac automaton(patterns);
```

**Throws** `std::runtime_error` on empty input, duplicate patterns, or exceeding the 16.7M pattern limit.

```cpp
// Create a stream over a haystack string
auto stream = automaton.make_stream("haystack text");

// Inspect the automaton structure
uint32_t states      = automaton.num_of_states();
uint32_t transitions = automaton.num_of_transitions();
```

`make_stream` takes a `std::string_view` and returns a `daac_stream`. The automaton must outlive any stream created from it.

---

## `yadaac::daac_stream`

Returned by `automaton.make_stream()`. Iterates over all pattern occurrences in the haystack in left-to-right order, reporting overlapping matches.

```cpp
yadaac::match m{};
while (stream.consume(m)) {
    // m.start  — byte offset of match start (inclusive)
    // m.end    — byte offset past match end (exclusive)
    // m.value  — index of the matched pattern in the original vector
}
```

Matches are reported in order of their end position. When multiple patterns end at the same position, they are reported shortest-first.

---

## `yadaac::match`

```cpp
struct match {
    size_t   start;  // inclusive start position
    size_t   end;    // exclusive end position
    uint32_t value;  // pattern index (insertion order)
};
```

The byte range `[start, end)` is the match span in the haystack. `value` is the zero-based index of the matched pattern in the vector passed to the `daac` constructor.

---

## Stream utilities (`yadaac/stream_utils.hpp`)

Adapters that wrap any stream and return a new stream. They compose with `|` and chain left-to-right. The element type of the resulting stream depends on the adapter.

```cpp
namespace su = yadaac::stream;
```

### `su::filter(predicate)`

Passes only matches for which `predicate(match)` returns `true`. All other matches are silently skipped.

```cpp
// Only report matches from the first pattern
auto s = stream | su::filter([](const yadaac::match& m) { return m.value == 0; });
```

### `su::transform(function)`

Maps each match to a different type using `function`. The output stream's `value_type` is the return type of `function`.

```cpp
// Produce match lengths instead of match objects
auto s = stream | su::transform([](const yadaac::match& m) { return m.end - m.start; });

size_t len{};
while (s.consume(len)) { /* ... */ }
```

### `su::take(n)`

Limits the stream to at most `n` results. The stream ends after `n` successful `consume` calls, regardless of remaining input.

```cpp
auto s = stream | su::take(5);
```

### `su::skip(n)`

Discards the first `n` matches and then passes through all subsequent ones.

```cpp
auto s = stream | su::skip(3);
```

### `su::non_overlapping()`

Greedily removes overlapping matches. Processes matches left-to-right: a match is kept if it starts at or after the end of the most recently kept match, otherwise it is discarded.

```cpp
auto s = stream | su::non_overlapping();
```

Example: for patterns `{"he", "she", "hers"}` on `"ushers"`, the raw matches are `she`[1,4), `he`[2,4), `hers`[2,6). `non_overlapping` keeps `she` and discards `he` and `hers` because they start before position 4.

### `su::left_most_non_overlapping()`

Within each group of mutually overlapping matches, keeps the one with the earliest start position. If multiple matches share the same start, the longest is kept. Non-overlapping groups are processed independently.

```cpp
auto s = stream | su::left_most_non_overlapping();
```

This is the typical "leftmost-longest" semantics: it prefers matches that begin as early as possible and, among those, are as long as possible.

### `su::longest_non_overlapping()`

Within each group of mutually overlapping matches, keeps the longest match. Ties among matches of equal length are broken by earliest start position.

```cpp
auto s = stream | su::longest_non_overlapping();
```

### Chaining adapters

Adapters can be chained arbitrarily. Each `|` wraps the left-hand stream in the right-hand adapter:

```cpp
// Skip the first 2 matches, then keep only pattern 0, stop after 10 results
auto s = stream | su::skip(2) | su::filter([](const yadaac::match& m) { return m.value == 0; }) | su::take(10);
```

The chain is evaluated lazily: `consume` on the outermost adapter pulls from upstream only as needed.
