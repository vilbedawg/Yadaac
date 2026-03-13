#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "yadaac/yadaac.hpp"

namespace {

using yadaac::daac;
using yadaac::match;

// Helper to collect all matches from a stream
std::vector<match> collect(daac& automaton, std::string_view haystack) {
  auto stream = automaton.make_stream(haystack);
  std::vector<match> matches;
  match out{};
  while (stream.consume(out)) {
    matches.push_back(out);
  }
  return matches;
}

TEST(StreamTest, ConsumesClassicAhoCorasickMatches) {
  std::vector<std::string> patterns = {"he", "she", "his", "hers"};
  daac automaton(patterns);
  auto matches = collect(automaton, "ushers");

  ASSERT_EQ(matches.size(), 3U);

  EXPECT_EQ(matches[0].start, 1U);
  EXPECT_EQ(matches[0].end, 4U);
  EXPECT_EQ(matches[0].value, 1U);  // "she"

  EXPECT_EQ(matches[1].start, 2U);
  EXPECT_EQ(matches[1].end, 4U);
  EXPECT_EQ(matches[1].value, 0U);  // "he"

  EXPECT_EQ(matches[2].start, 2U);
  EXPECT_EQ(matches[2].end, 6U);
  EXPECT_EQ(matches[2].value, 3U);  // "hers"
}

TEST(StreamTest, ReturnsFalseWhenNoMatchExists) {
  std::vector<std::string> patterns = {"abc", "def"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("zzzz");

  match out{};
  EXPECT_FALSE(stream.consume(out));
}

TEST(StreamTest, HandlesPrefixPatterns) {
  std::vector<std::string> patterns = {"a", "ab", "abc"};
  daac automaton(patterns);
  auto matches = collect(automaton, "abc");

  ASSERT_EQ(matches.size(), 3U);
  EXPECT_EQ(matches[0].start, 0U);
  EXPECT_EQ(matches[0].end, 1U);
  EXPECT_EQ(matches[0].value, 0U);

  EXPECT_EQ(matches[1].start, 0U);
  EXPECT_EQ(matches[1].end, 2U);
  EXPECT_EQ(matches[1].value, 1U);

  EXPECT_EQ(matches[2].start, 0U);
  EXPECT_EQ(matches[2].end, 3U);
  EXPECT_EQ(matches[2].value, 2U);
}

TEST(StreamTest, SingleCharPattern) {
  std::vector<std::string> patterns = {"a"};
  daac automaton(patterns);
  auto matches = collect(automaton, "abracadabra");
  EXPECT_EQ(matches.size(), 5U);
  for (const auto& m : matches) {
    EXPECT_EQ(m.end - m.start, 1U);
    EXPECT_EQ(m.value, 0U);
  }
}

TEST(StreamTest, SinglePatternAutomaton) {
  std::vector<std::string> patterns = {"needle"};
  daac automaton(patterns);
  auto matches = collect(automaton, "find the needle in the haystack");
  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].value, 0U);
  EXPECT_EQ(matches[0].end - matches[0].start, 6U);
}

TEST(StreamTest, SuffixPatterns) {
  std::vector<std::string> patterns = {"abc", "bc", "c"};
  daac automaton(patterns);
  auto matches = collect(automaton, "abc");
  ASSERT_EQ(matches.size(), 3U);
}

TEST(StreamTest, RepeatedOccurrences) {
  std::vector<std::string> patterns = {"ab"};
  daac automaton(patterns);
  auto matches = collect(automaton, "ababab");
  ASSERT_EQ(matches.size(), 3U);
  EXPECT_EQ(matches[0].start, 0U);
  EXPECT_EQ(matches[1].start, 2U);
  EXPECT_EQ(matches[2].start, 4U);
}

TEST(StreamTest, PatternAtStartAndEnd) {
  std::vector<std::string> patterns = {"start", "end"};
  daac automaton(patterns);
  auto matches = collect(automaton, "startmiddleend");
  ASSERT_EQ(matches.size(), 2U);
  EXPECT_EQ(matches[0].start, 0U);
  EXPECT_EQ(matches[0].end, 5U);
  EXPECT_EQ(matches[1].start, 11U);
  EXPECT_EQ(matches[1].end, 14U);
}

TEST(StreamTest, HaystackExactlyOnePattern) {
  std::vector<std::string> patterns = {"exact"};
  daac automaton(patterns);
  auto matches = collect(automaton, "exact");
  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].start, 0U);
  EXPECT_EQ(matches[0].end, 5U);
}

TEST(StreamTest, EmptyHaystack) {
  std::vector<std::string> patterns = {"anything"};
  daac automaton(patterns);
  auto stream = automaton.make_stream("");
  match out{};
  EXPECT_FALSE(stream.consume(out));
}

TEST(StreamTest, LongHaystackScattered) {
  std::string haystack(10000, 'z');
  haystack[100] = 'x';
  haystack[101] = 'y';
  haystack[102] = 'z';
  haystack[5000] = 'x';
  haystack[5001] = 'y';
  haystack[5002] = 'z';
  haystack[9990] = 'x';
  haystack[9991] = 'y';
  haystack[9992] = 'z';

  std::vector<std::string> patterns = {"xyz"};
  daac automaton(patterns);
  auto matches = collect(automaton, haystack);
  EXPECT_EQ(matches.size(), 3U);
}

TEST(StreamTest, LongPattern) {
  std::string pattern(500, 'a');
  std::string haystack = std::string(100, 'b') + pattern + std::string(100, 'b');

  std::vector<std::string> patterns = {pattern};
  daac automaton(patterns);
  auto matches = collect(automaton, haystack);
  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].start, 100U);
  EXPECT_EQ(matches[0].end, 600U);
}

TEST(StreamTest, CommonPrefixPatterns) {
  std::vector<std::string> patterns = {"abcd", "abce", "abcf"};
  daac automaton(patterns);
  auto matches = collect(automaton, "abcdabceabcf");
  ASSERT_EQ(matches.size(), 3U);
  EXPECT_EQ(matches[0].value, 0U);  // "abcd"
  EXPECT_EQ(matches[1].value, 1U);  // "abce"
  EXPECT_EQ(matches[2].value, 2U);  // "abcf"
}

TEST(StreamTest, DenseOverlapping) {
  std::vector<std::string> patterns = {"a", "aa", "aaa"};
  daac automaton(patterns);
  auto matches = collect(automaton, "aaa");
  // Position 1: "a"
  // Position 2: "a", "aa"
  // Position 3: "a", "aa", "aaa"
  EXPECT_EQ(matches.size(), 6U);
}

TEST(StreamTest, BinaryNonASCIIPatterns) {
  std::string p1 = {(char)0x80, (char)0x81, (char)0x82};
  std::string p2 = {(char)0xFE, (char)0xFF};
  std::string haystack = "abc" + p1 + "def" + p2 + "ghi";

  std::vector<std::string> patterns = {p1, p2};
  daac automaton(patterns);
  auto matches = collect(automaton, haystack);
  ASSERT_EQ(matches.size(), 2U);
  EXPECT_EQ(matches[0].value, 0U);
  EXPECT_EQ(matches[1].value, 1U);
}

TEST(StreamTest, AllByteValues) {
  std::string pattern(256, '\0');
  for (int i = 0; i < 256; ++i) {
    pattern[i] = static_cast<char>(i);
  }

  std::string haystack = "prefix" + pattern + "suffix";
  std::vector<std::string> patterns = {pattern};
  daac automaton(patterns);
  auto matches = collect(automaton, haystack);
  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].start, 6U);
  EXPECT_EQ(matches[0].end, 262U);
}

TEST(StreamTest, PatternValueMatchesInsertionOrder) {
  std::vector<std::string> patterns = {"cat", "dog", "bird", "fish"};
  daac automaton(patterns);

  for (uint32_t i = 0; i < patterns.size(); ++i) {
    auto matches = collect(automaton, patterns[i]);
    ASSERT_EQ(matches.size(), 1U);
    EXPECT_EQ(matches[0].value, i) << "Pattern \"" << patterns[i] << "\" should have value " << i;
  }
}

TEST(StreamTest, ManyPatterns) {
  std::vector<std::string> patterns;
  patterns.reserve(100);
  for (int i = 0; i < 100; ++i) {
    patterns.push_back("pat" + std::to_string(i) + "x");
  }

  std::string haystack;
  for (const auto& p : patterns) {
    haystack += p + " ";
  }

  daac automaton(patterns);
  auto matches = collect(automaton, haystack);
  EXPECT_EQ(matches.size(), 100U);
}

}  // namespace
