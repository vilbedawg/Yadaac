#ifndef YADAAC_STREAM_UTILS_HPP
#define YADAAC_STREAM_UTILS_HPP

#include <cstddef>
#include <type_traits>  // for std::invoke_result_t
#include <utility>      // for std::forward, std::move

namespace yadaac::stream {

namespace detail {

/* Filtered stream */
template <typename Stream, typename Pred>
struct filtered_stream {
  Stream stream;
  Pred pred;
  using value_type = typename Stream::value_type;

  bool consume(value_type& out) {
    while (stream.consume(out)) {
      if (pred(out)) {
        return true;
      }
    }
    return false;
  }
};
template <typename Stream, typename Pred>
filtered_stream(Stream, Pred) -> filtered_stream<Stream, Pred>;

template <typename Pred>
struct filter_proxy {
  Pred pred;
};
template <typename Stream, typename Pred>
auto operator|(Stream&& stream, filter_proxy<Pred> proxy) {
  return filtered_stream{std::forward<Stream>(stream), std::move(proxy.pred)};
}

/* Transformed stream */
template <typename Stream, typename Func>
struct transformed_stream {
  Stream stream;
  Func func;
  using input_type = typename Stream::value_type;
  using value_type = std::invoke_result_t<Func, input_type>;

  bool consume(value_type& out) {
    input_type temp;
    if (stream.consume(temp)) {
      out = func(temp);
      return true;
    }
    return false;
  }
};
template <typename Stream, typename Func>
transformed_stream(Stream, Func) -> transformed_stream<Stream, Func>;

template <typename Func>
struct transform_proxy {
  Func func;
};
template <typename Stream, typename Func>
auto operator|(Stream&& stream, transform_proxy<Func> proxy) {
  return transformed_stream{std::forward<Stream>(stream), std::move(proxy.func)};
}

/* Take stream */
template <typename Stream>
struct take_stream {
  Stream stream;
  size_t count;
  using value_type = typename Stream::value_type;

  bool consume(value_type& out) {
    if (count == 0) {
      return false;
    }

    if (stream.consume(out)) {
      count--;
      return true;
    }
    return false;
  }
};
template <typename Stream>
take_stream(Stream, size_t) -> take_stream<Stream>;

struct take_proxy {
  size_t n;
};

template <typename Stream>
auto operator|(Stream&& stream, take_proxy proxy) {
  return take_stream{std::forward<Stream>(stream), proxy.n};
}

/* Skip stream */
template <typename Stream>
struct skip_stream {
  Stream stream;
  size_t count;
  using value_type = typename Stream::value_type;

  bool consume(value_type& out) {
    while (count > 0) {
      if (!stream.consume(out)) {
        return false;
      }
      count--;
    }

    return stream.consume(out);
  }
};
template <typename Stream>
skip_stream(Stream, size_t) -> skip_stream<Stream>;

struct skip_proxy {
  size_t n;
};

template <typename Stream>
auto operator|(Stream&& stream, skip_proxy proxy) {
  return skip_stream{std::forward<Stream>(stream), proxy.n};
}

/* Non-overlapping stream */
template <typename Stream>
struct non_overlapping_stream {
  Stream stream;
  using value_type = typename Stream::value_type;

  size_t last_end = 0;

  bool consume(value_type& out) {
    while (stream.consume(out)) {
      if (out.start >= last_end) {
        last_end = out.end;
        return true;
      }
    }
    return false;
  }
};
template <typename Stream>
non_overlapping_stream(Stream) -> non_overlapping_stream<Stream>;

struct non_overlapping_proxy {};

template <typename Stream>
auto operator|(Stream&& stream, non_overlapping_proxy /*unused*/) {
  return non_overlapping_stream{std::forward<Stream>(stream)};
}

/* Left most stream */
template <typename Stream>
struct left_most_stream {
  Stream stream;
  using value_type = typename Stream::value_type;

  value_type pending{};
  bool has_pending = false;

  bool consume(value_type& out) {
    value_type next_match;

    while (true) {
      if (!has_pending) {
        if (!stream.consume(pending)) {
          return false;  // Stream empty
        }
        has_pending = true;
      }

      bool has_next = stream.consume(next_match);

      if (!has_next) {
        out = pending;
        has_pending = false;
        return true;
      }

      if (next_match.start < pending.end) {
        bool next_is_better = next_match.start < pending.start ||
                              (next_match.start == pending.start && next_match.end > pending.end);
        if (next_is_better) {
          pending = next_match;
        }
      } else {
        out = pending;
        pending = next_match;
        return true;
      }
    }
  }
};
template <typename Stream>
left_most_stream(Stream) -> left_most_stream<Stream>;

struct left_most_proxy {};
template <typename Stream>
auto operator|(Stream&& stream, left_most_proxy /*unused*/) {
  return left_most_stream{std::forward<Stream>(stream)};
}

/* Longest match stream */
template <typename Stream>
struct longest_match_stream {
  Stream stream;
  using value_type = typename Stream::value_type;

  value_type best_candidate{};
  bool has_candidate = false;

  bool consume(value_type& out) {
    value_type next_match;

    while (true) {
      if (!has_candidate) {
        if (!stream.consume(best_candidate)) {
          return false;  // Stream empty
        }
        has_candidate = true;
      }

      if (!stream.consume(next_match)) {
        out = best_candidate;
        has_candidate = false;
        return true;
      }

      if (next_match.start < best_candidate.end) {
        size_t best_len = best_candidate.end - best_candidate.start;
        size_t next_len = next_match.end - next_match.start;

        // Prefer left most first
        if (next_len > best_len ||
            (next_len == best_len && next_match.start < best_candidate.start)) {
          best_candidate = next_match;
        }
      } else {
        out = best_candidate;
        best_candidate = next_match;
        has_candidate = true;

        return true;
      }
    }
  }
};

template <typename Stream>
longest_match_stream(Stream) -> longest_match_stream<Stream>;

struct longest_match_proxy {};

template <typename Stream>
inline auto operator|(Stream&& stream, longest_match_proxy /*unused*/) {
  return longest_match_stream{std::forward<Stream>(stream)};
}
}  // namespace detail

template <typename Func>
inline auto transform(Func&& func) {
  return detail::transform_proxy<Func>{std::forward<Func>(func)};
}

template <typename Pred>
inline auto filter(Pred&& pred) {
  return detail::filter_proxy<Pred>{std::forward<Pred>(pred)};
}

inline auto take(size_t n) { return detail::take_proxy{n}; }

inline auto skip(size_t n) { return detail::skip_proxy{n}; }

inline auto non_overlapping() { return detail::non_overlapping_proxy{}; }

inline auto left_most_non_overlapping() { return detail::left_most_proxy{}; }

inline auto longest_non_overlapping() { return detail::longest_match_proxy{}; }

}  // namespace yadaac::stream

#endif  // !YADAAC_STREAM_UTILS_HPP
