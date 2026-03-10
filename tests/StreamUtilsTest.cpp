#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "yadaac/yadaac.hpp"
#include "yadaac/stream_utils.hpp"

namespace {

using yadaac::daac;
using yadaac::match;
namespace su = yadaac::stream;

// Helper to collect matches from any stream-like object
template <typename Stream>
auto collect_stream(Stream&& s) {
  using value_type = typename std::remove_reference_t<Stream>::value_type;
  std::vector<value_type> results;
  value_type out{};
  while (s.consume(out)) {
    results.push_back(out);
  }
  return results;
}

// Helper to build automaton and collect all matches
std::vector<match> collect(daac& automaton, std::string_view haystack) {
  auto stream = automaton.make_stream(haystack);
  return collect_stream(stream);
}

// ===================== Filter Tests =====================

TEST(StreamUtilsTest, FilterByValue) {
  std::vector<std::string> patterns = {"he", "she", "his", "hers"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("ushers") | su::filter([](const match& m) { return m.value == 1; });
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 1U);
  EXPECT_EQ(results[0].value, 1U);  // "she"
}

TEST(StreamUtilsTest, FilterByPosition) {
  std::vector<std::string> patterns = {"a", "ab", "abc"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abc") | su::filter([](const match& m) { return m.start == 0 && m.end >= 2; });
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 2U);
  EXPECT_EQ(results[0].value, 1U);  // "ab"
  EXPECT_EQ(results[1].value, 2U);  // "abc"
}

TEST(StreamUtilsTest, FilterAll) {
  std::vector<std::string> patterns = {"a", "b"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("ab") | su::filter([](const match&) { return false; });
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 0U);
}

TEST(StreamUtilsTest, FilterNone) {
  std::vector<std::string> patterns = {"a", "b"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("ab") | su::filter([](const match&) { return true; });
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 2U);
}

// ===================== Transform Tests =====================

struct custom_result {
  size_t position;
  uint32_t id;
};

TEST(StreamUtilsTest, TransformToCustomType) {
  std::vector<std::string> patterns = {"abc"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abc") | su::transform([](const match& m) {
                  return custom_result{m.start, m.value};
                });
  custom_result out{};
  ASSERT_TRUE(stream.consume(out));
  EXPECT_EQ(out.position, 0U);
  EXPECT_EQ(out.id, 0U);
  EXPECT_FALSE(stream.consume(out));
}

TEST(StreamUtilsTest, TransformToLength) {
  std::vector<std::string> patterns = {"a", "ab", "abc"};
  daac automaton(patterns);
  auto stream =
      automaton.make_stream("abc") | su::transform([](const match& m) -> size_t { return m.end - m.start; });

  std::vector<size_t> lengths;
  size_t out{};
  while (stream.consume(out)) {
    lengths.push_back(out);
  }
  ASSERT_EQ(lengths.size(), 3U);
  EXPECT_EQ(lengths[0], 1U);
  EXPECT_EQ(lengths[1], 2U);
  EXPECT_EQ(lengths[2], 3U);
}

// ===================== Take Tests =====================

TEST(StreamUtilsTest, TakeZero) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaa") | su::take(0);
  match out{};
  EXPECT_FALSE(stream.consume(out));
}

TEST(StreamUtilsTest, TakeOne) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaa") | su::take(1);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 1U);
}

TEST(StreamUtilsTest, TakeMoreThanAvailable) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aa") | su::take(100);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 2U);
}

TEST(StreamUtilsTest, TakeExactCount) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaa") | su::take(3);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 3U);
}

// ===================== Skip Tests =====================

TEST(StreamUtilsTest, SkipZero) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaa") | su::skip(0);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 3U);
}

TEST(StreamUtilsTest, SkipOne) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaa") | su::skip(1);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 2U);
}

TEST(StreamUtilsTest, SkipMoreThanAvailable) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aa") | su::skip(100);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 0U);
}

TEST(StreamUtilsTest, SkipExactCount) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaa") | su::skip(3);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 0U);
}

// ===================== Non-overlapping Tests =====================

TEST(StreamUtilsTest, NonOverlappingClassic) {
  // "she" and "he" overlap; non_overlapping should drop "he"
  std::vector<std::string> patterns = {"he", "she", "his", "hers"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("ushers") | su::non_overlapping();
  auto results = collect_stream(stream);
  // "she" at [1,4), then "hers" at [2,6) overlaps so dropped
  // We get "she" first (start=1,end=4), then "he"(start=2) overlaps, "hers"(start=2) overlaps
  ASSERT_EQ(results.size(), 1U);
  EXPECT_EQ(results[0].value, 1U);  // "she"
}

TEST(StreamUtilsTest, NonOverlappingDense) {
  std::vector<std::string> patterns = {"a", "aa", "aaa"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaa") | su::non_overlapping();
  auto results = collect_stream(stream);
  // First match: "a" at [0,1). Next "a" at [1,2) doesn't overlap. Next "aa" at [0,2) overlaps. etc.
  // Non-overlapping keeps first match, skips overlapping ones
  EXPECT_GE(results.size(), 1U);
  // Verify no overlaps
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_GE(results[i].start, results[i - 1].end);
  }
}

TEST(StreamUtilsTest, NonOverlappingNoOverlapInput) {
  std::vector<std::string> patterns = {"ab", "cd", "ef"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abcdef") | su::non_overlapping();
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 3U);
}

// ===================== Left-most Non-overlapping Tests =====================

TEST(StreamUtilsTest, LeftMostEarlierStartWins) {
  // "abc" and "bc" overlap; "abc" has earlier start
  std::vector<std::string> patterns = {"abc", "bc"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abc") | su::left_most_non_overlapping();
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 1U);
  EXPECT_EQ(results[0].value, 0U);  // "abc" starts at 0
}

TEST(StreamUtilsTest, LeftMostLongerAtSameStartWins) {
  std::vector<std::string> patterns = {"a", "ab", "abc"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abc") | su::left_most_non_overlapping();
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 1U);
  EXPECT_EQ(results[0].value, 2U);  // "abc" (longest at start=0)
}

TEST(StreamUtilsTest, LeftMostMultipleGroups) {
  std::vector<std::string> patterns = {"ab", "cd"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abcd") | su::left_most_non_overlapping();
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 2U);
  EXPECT_EQ(results[0].start, 0U);
  EXPECT_EQ(results[1].start, 2U);
}

// ===================== Longest Non-overlapping Tests =====================

TEST(StreamUtilsTest, LongestWins) {
  std::vector<std::string> patterns = {"a", "ab", "abc"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abc") | su::longest_non_overlapping();
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 1U);
  EXPECT_EQ(results[0].value, 2U);  // "abc" is longest
}

TEST(StreamUtilsTest, LongestMultipleGroups) {
  std::vector<std::string> patterns = {"ab", "abc", "de", "def"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abcdef") | su::longest_non_overlapping();
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 2U);
  EXPECT_EQ(results[0].value, 1U);  // "abc"
  EXPECT_EQ(results[1].value, 3U);  // "def"
}

TEST(StreamUtilsTest, LongestTieBreaksLeftmost) {
  // Two same-length patterns at different starts; the leftmost should win
  std::vector<std::string> patterns = {"ab", "bc"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abc") | su::longest_non_overlapping();
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 1U);
  // Both length 2; "ab" starts at 0, "bc" starts at 1 and overlaps
  EXPECT_EQ(results[0].start, 0U);
}

// ===================== Chaining Tests =====================

TEST(StreamUtilsTest, FilterThenTake) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaaaa") | su::filter([](const match& m) { return m.start >= 2; }) | su::take(2);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 2U);
  EXPECT_EQ(results[0].start, 2U);
  EXPECT_EQ(results[1].start, 3U);
}

TEST(StreamUtilsTest, SkipThenTake) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaaaa") | su::skip(2) | su::take(2);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 2U);
  EXPECT_EQ(results[0].start, 2U);
  EXPECT_EQ(results[1].start, 3U);
}

TEST(StreamUtilsTest, NonOverlappingThenFilter) {
  std::vector<std::string> patterns = {"ab", "cd", "ef"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("abcdef") | su::non_overlapping() |
                su::filter([](const match& m) { return m.value != 1; });
  auto results = collect_stream(stream);
  ASSERT_EQ(results.size(), 2U);
  EXPECT_EQ(results[0].value, 0U);  // "ab"
  EXPECT_EQ(results[1].value, 2U);  // "ef"
}

TEST(StreamUtilsTest, TripleChain) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("aaaaaaaaaa")  // 10 a's
                | su::skip(2) | su::filter([](const match& m) { return m.start % 2 == 0; }) | su::take(3);
  auto results = collect_stream(stream);
  EXPECT_EQ(results.size(), 3U);
  EXPECT_EQ(results[0].start, 2U);
  EXPECT_EQ(results[1].start, 4U);
  EXPECT_EQ(results[2].start, 6U);
}

}  // namespace
